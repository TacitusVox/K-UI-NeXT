/* SPDX-License-Identifier: GPL-3.0-only */
/* Saved Games catalogue: the production cold-load slot choice, disc grouping
 * and display order against a frozen copy of the implementation they
 * replace, on synthetic catalogues and in-memory slot files. This is a host
 * equivalence check, not a console timing. Unused library functions are
 * discarded by the linker. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Every allocation of the library and of the frozen copy can be refused. */
static void *test_malloc(size_t bytes);
static void *test_calloc(size_t count,size_t bytes);
#define malloc(bytes) test_malloc(bytes)
#define calloc(count,bytes) test_calloc(count,bytes)
#include "../src/apps/games_library.c"

/* Frozen copy of games_library.c before the header-first cold load and
 * precomputed display keys: decode, validation, grouping, sort and load.
 * Only the names changed. Its unchanged helpers are shared. */
static const struct library *old_sorting;
static bool old_decode_record(const struct library *lib,unsigned index,struct kui_games_entry *entry) {
    if(index>=lib->count)return false;
    unsigned fields=record_fields(lib),header=record_header(lib),flags=fields*2u;
    size_t at=lib->offsets[index];if(at>lib->bytes || lib->bytes-at<header)return false;
    const uint8_t *p=lib->wire+at;unsigned lengths[5]={0};size_t total=header+(lib->version>=4?GROUP_EVIDENCE_BYTES:0u);
    for(unsigned i=0;i<fields;i++) {lengths[i]=get16(p+2*i);total+=lengths[i];}
    if(total>lib->bytes-at || (p[flags]&~3u))return false;
    for(unsigned i=lib->version>=4?13u:flags+1u;i<header;i++)if(p[i])return false;
    memset(entry,0,sizeof(*entry));char *strings[]={entry->name,entry->path,entry->variant_2048_path,entry->title,entry->region};
    const size_t caps[]={sizeof(entry->name),sizeof(entry->path),sizeof(entry->variant_2048_path),sizeof(entry->title),sizeof(entry->region)};
    p+=header;
    for(unsigned i=0;i<fields;i++) {
        if(lengths[i]>=caps[i] || memchr(p,0,lengths[i]))return false;
        memcpy(strings[i],p,lengths[i]);strings[i][lengths[i]]=0;p+=lengths[i];
    }
    entry->directory=(lib->wire[at+flags]&1u)!=0;entry->disabled=(lib->wire[at+flags]&2u)!=0;
    if(!kui_destination_name_valid(entry->name))return false;
    if(!entry->disabled && !safe_path(lib->root,entry->path,entry->directory))return false;
    if(entry->disabled && entry->path[0] && !safe_path(lib->root,entry->path,entry->directory))return false;
    if(entry->variant_2048_path[0] && (entry->directory || entry->disabled ||
        !safe_path(lib->root,entry->variant_2048_path,false) || !strcmp(entry->path,entry->variant_2048_path)))return false;
    for(const unsigned char *c=(const unsigned char *)entry->title;*c;c++)if(*c<32 || *c==127)return false;
    for(const unsigned char *c=(const unsigned char *)entry->region;*c;c++)if(*c<32 || *c>126)return false;
    struct kui_games_group_evidence evidence;if(!record_evidence(lib,index,&evidence))return false;
    if(evidence.disc) {entry->disc=evidence.disc;if(!entry->region[0])strcpy(entry->region,evidence.region);}
    const struct kui_games_metadata_entry *edit=edits?kui_games_metadata_find(edits,entry->path,""):NULL;
    if(edit) {
        if(edit->edit.alias[0]) {strcpy(entry->title,edit->edit.alias);entry->custom_title=true;}
        strcpy(entry->cover_path,edit->edit.cover_path);
        if(edit->region[0])strcpy(entry->region,edit->region);
        if(edit->disc)entry->disc=edit->disc;
    }
    return true;
}
static bool old_library_validate(struct library *lib,const char *root,const uint8_t identity[32],uint64_t epoch) {
    const uint8_t *p=lib->wire;
    if(lib->bytes<LIBRARY_HEADER)return false;
    unsigned version=get32(p+8);
    if((version!=2 && version!=3 && version!=4) ||
       memcmp(p,version==4?"KUIGLIB4":version==3?"KUIGLIB3":"KUIGLIB2",8) || get32(p+12)!=lib->bytes ||
       !get64(p+16) || get64(p+24)!=epoch || memcmp(p+32,identity,32) ||
       get32(p+68)!=kui_crc32(0,p+LIBRARY_HEADER,lib->bytes-LIBRARY_HEADER) || get32(p+72)!=kui_crc32(0,p,72))return false;
    for(unsigned i=76;i<LIBRARY_HEADER;i++)if(p[i])return false;
    lib->version=version;
    unsigned root_bytes=get16(p+64);lib->count=get16(p+66);
    if(root_bytes!=strlen(root) || root_bytes>=sizeof(lib->root) || lib->count>LIBRARY_ROWS ||
       root_bytes>lib->bytes-LIBRARY_HEADER || memcmp(p+LIBRARY_HEADER,root,root_bytes))return false;
    if(lib->root!=root)strcpy(lib->root,root);
    memmove(lib->identity,identity,32);lib->epoch=epoch;lib->sequence=get64(p+16);
    lib->offsets=calloc(lib->count?lib->count:1,sizeof(*lib->offsets));
    if(!lib->offsets) {lib->resource_error=true;return false;}
    size_t at=LIBRARY_HEADER+root_bytes;
    for(unsigned i=0;i<lib->count;i++) {
        unsigned header=record_header(lib);
        if(lib->bytes-at<header)return false;
        lib->offsets[i]=(uint32_t)at;struct kui_games_entry entry;
        if(!old_decode_record(lib,i,&entry))return false;
        at+=record_bytes(lib,p+lib->offsets[i]);
    }
    return at==lib->bytes;
}
static int old_display_order(const void *left,const void *right) {
    unsigned a=*(const uint32_t *)left,b=*(const uint32_t *)right;
    unsigned ag=old_sorting->members[a].group,bg=old_sorting->members[b].group;
    struct kui_games_entry ae,be;
    if(!old_decode_record(old_sorting,a,&ae) || !old_decode_record(old_sorting,b,&be))return 0;
    const char *an=ag && old_sorting->groups[ag-1].valid?group_title(old_sorting,ag):ae.custom_title?ae.title:ae.name;
    const char *bn=bg && old_sorting->groups[bg-1].valid?group_title(old_sorting,bg):be.custom_title?be.title:be.name;
    const unsigned char *x=(const unsigned char *)an,*y=(const unsigned char *)bn;
    while(*x && *y) {unsigned p=*x++,q=*y++;if(p>='A'&&p<='Z')p+='a'-'A';if(q>='A'&&q<='Z')q+='a'-'A';if(p!=q)return p<q?-1:1;}
    if(*x!=*y)return *x?1:-1;
    int cmp=strcmp(an,bn);return cmp?cmp:a<b?-1:a>b?1:0;
}
static int old_member_order(const void *left,const void *right) {
    unsigned a=*(const uint32_t *)left,b=*(const uint32_t *)right;
    const struct library_member *am=&old_sorting->members[a],*bm=&old_sorting->members[b];
    if(am->group!=bm->group)return am->group<bm->group?-1:1;
    return am->disc<bm->disc?-1:am->disc>bm->disc?1:0;
}
static bool old_library_groups(struct library *lib) {
    lib->groups=calloc(lib->count?lib->count:1,sizeof(*lib->groups));
    lib->members=calloc(lib->count?lib->count:1,sizeof(*lib->members));
    if(!lib->groups || !lib->members)return false;
    for(unsigned i=0;i<lib->count;i++) {
        struct kui_games_entry entry;if(!old_decode_record(lib,i,&entry))return false;
        char base[KUI_DEST_NAME_CAP],key[33];unsigned disc=0;
        if(entry.directory || entry.disabled || !kui_games_group_parse(entry.name,base,&disc) || !kui_games_group_key(base,key))continue;
        unsigned g=0;for(;g<lib->group_count;g++)if(fold_equal(base,lib->groups[g].name))break;
        if(g==lib->group_count) {
            strcpy(lib->groups[g].key,key);strcpy(lib->groups[g].name,base);lib->groups[g].representative=i;lib->groups[g].valid=true;++lib->group_count;
            for(unsigned j=0;j<g;j++)if(!strcmp(lib->groups[j].key,key))lib->groups[g].valid=lib->groups[j].valid=false;
        }
        for(unsigned j=0;j<i;j++)if(lib->members[j].group==g+1 && lib->members[j].disc==disc)lib->groups[g].valid=false;
        lib->members[i]=(struct library_member){g+1,disc};++lib->groups[g].count;
    }
    for(unsigned i=0;i<lib->count;i++) {
        unsigned previous_group=lib->members[i].group;
        if(previous_group && (lib->groups[previous_group-1u].metadata ||
           lib->groups[previous_group-1u].count!=1u))continue;
        struct kui_games_entry entry;struct kui_games_group_evidence evidence;
        if(!old_decode_record(lib,i,&entry) || !record_evidence(lib,i,&evidence))return false;
        char base[KUI_DEST_NAME_CAP],key[33];unsigned named_disc;
        if(entry.directory || entry.disabled || !evidence.disc ||
           !group_candidate_base(entry.name,base,&named_disc) ||
           (named_disc && named_disc!=evidence.disc))continue;
        if(previous_group) {
            lib->members[i].group=0;lib->groups[previous_group-1u].count=0;
            lib->groups[previous_group-1u].valid=false;
        }
        evidence_key(base,&evidence,key);
        unsigned g=0;for(;g<lib->group_count;g++)if(lib->groups[g].metadata && group_family_equal(base,lib->groups[g].name)) {
            struct kui_games_group_evidence previous;
            if(!record_evidence(lib,lib->groups[g].representative,&previous))return false;
            if(kui_games_group_evidence_equal(&previous,&evidence))break;
        }
        if(g==lib->group_count) {
            /* A lone explicit marker reserved one slot above. Reuse that
             * retired slot so conflicting mixed-name releases stay bounded
             * by the number of actual catalogue records. */
            for(g=0;g<lib->group_count;g++)if(!lib->groups[g].count)break;
            if(g==lib->group_count)++lib->group_count;
            lib->groups[g]=(struct library_group){0};
            strcpy(lib->groups[g].key,key);strcpy(lib->groups[g].name,base);lib->groups[g].representative=i;
            lib->groups[g].metadata=lib->groups[g].valid=true;
            for(unsigned j=0;j<lib->group_count;j++)if(j!=g && lib->groups[j].count &&
                !strcmp(lib->groups[j].key,key))lib->groups[g].valid=lib->groups[j].valid=false;
        }
        for(unsigned j=0;j<i;j++)if(lib->members[j].group==g+1 && lib->members[j].disc==evidence.disc)lib->groups[g].valid=false;
        lib->members[i]=(struct library_member){g+1,evidence.disc};++lib->groups[g].count;
    }
    for(unsigned i=0;i<lib->group_count;i++)if(lib->groups[i].count<2)lib->groups[i].valid=false;
    /* Distinct proven releases may share a capture title. Give each actual
     * set a release qualifier; unknown/conflicting singleton rows stay real. */
    for(unsigned i=0;i<lib->group_count;i++)if(lib->groups[i].valid && lib->groups[i].metadata) {
        struct kui_games_entry representative;char base[KUI_DEST_NAME_CAP];bool qualify=false;unsigned named_disc;
        if(!old_decode_record(lib,lib->groups[i].representative,&representative) ||
           !group_candidate_base(representative.name,base,&named_disc))return false;
        for(unsigned j=0;j<lib->group_count && !qualify;j++)if(j!=i && lib->groups[j].valid && lib->groups[j].metadata) {
            struct kui_games_entry other;char other_base[KUI_DEST_NAME_CAP];
            if(!old_decode_record(lib,lib->groups[j].representative,&other))return false;
            qualify=group_candidate_base(other.name,other_base,&named_disc) && group_family_equal(base,other_base);
        }
        if(qualify) {
            struct kui_games_group_evidence e;if(!record_evidence(lib,lib->groups[i].representative,&e))return false;
            char suffix[80];snprintf(suffix,sizeof(suffix)," [%s %s %s %u discs]",e.region,e.version,e.product,e.total);
            group_label(lib->groups[i].name,base,suffix);
        }
    }
    /* Equal visible fields, truncated titles and explicit-name groups can
     * still collide. Rank by stable key before changing any labels. */
    for(unsigned i=0;i<lib->group_count;i++)if(lib->groups[i].valid && lib->groups[i].metadata) {
        bool collision=false;unsigned rank=1;
        for(unsigned j=0;j<lib->group_count;j++)if(j!=i && lib->groups[j].valid && fold_equal(lib->groups[i].name,lib->groups[j].name)) {
            collision=true;if(strcmp(lib->groups[j].key,lib->groups[i].key)<0)++rank;
        }
        if(collision)lib->groups[i].label_set=rank;
    }
    for(unsigned i=0;i<lib->group_count;i++)if(lib->groups[i].label_set) {
            struct kui_games_entry entry;struct kui_games_group_evidence e;char base[KUI_DEST_NAME_CAP],suffix[80];unsigned named_disc;
            if(!old_decode_record(lib,lib->groups[i].representative,&entry) ||
               !record_evidence(lib,lib->groups[i].representative,&e) || !group_candidate_base(entry.name,base,&named_disc))return false;
            snprintf(suffix,sizeof(suffix)," [%s %s %s %u discs set %u]",e.region,e.version,e.product,e.total,lib->groups[i].label_set);
            group_label(lib->groups[i].name,base,suffix);
    }
    lib->display=calloc(lib->count?lib->count:1,sizeof(*lib->display));
    lib->group_display=calloc(lib->count?lib->count:1,sizeof(*lib->group_display));
    if(!lib->display || !lib->group_display)return false;
    unsigned member_count=0;
    for(unsigned i=0;i<lib->count;i++) {
        unsigned g=lib->members[i].group;
        if(g && lib->groups[g-1].valid) {
            lib->group_display[member_count++]=i;
            if(lib->groups[g-1].representative!=i)continue;
        }
        lib->display[lib->display_count++]=i;
    }
    old_sorting=lib;qsort(lib->display,lib->display_count,sizeof(*lib->display),old_display_order);
    qsort(lib->group_display,member_count,sizeof(*lib->group_display),old_member_order);old_sorting=NULL;
    for(unsigned i=0;i<member_count;i++) {
        unsigned g=lib->members[lib->group_display[i]].group;
        if(!i || lib->members[lib->group_display[i-1]].group!=g)lib->groups[g-1].first=i;
    }
    return true;
}
static enum load_result old_library_load(struct library *out,const char *root,const uint8_t identity[32],uint64_t epoch,kui_cancel_fn cancel) {
    enum load_result result=LOAD_MISSING;
    for(unsigned slot=0;slot<2;slot++) {
        if(stopped(cancel)) {library_free(out);return LOAD_ERROR;}
        char path[96];library_path(path,root,slot);FIL file;FRESULT r=f_open(&file,path,FA_READ);
        if(r==FR_NO_FILE || r==FR_NO_PATH)continue;
        if(r!=FR_OK) {library_free(out);return LOAD_ERROR;}
        struct library candidate={0};FSIZE_t bytes=f_size(&file);
        bool ok=bytes>=LIBRARY_HEADER && bytes<=LIBRARY_MAX_BYTES;
        bool memory_error=false;
        if(ok) {candidate.bytes=(size_t)bytes;candidate.wire=malloc(candidate.bytes);ok=candidate.wire!=NULL;memory_error=!ok;}
        bool io_error=false;
        for(size_t at=0;ok && at<candidate.bytes;) {
            if(stopped(cancel)) {ok=false;break;}
            UINT got=0,count=(UINT)(candidate.bytes-at>4096?4096:candidate.bytes-at);
            ok=f_read(&file,candidate.wire+at,count,&got)==FR_OK && got==count;
            io_error=!ok;at+=got;
        }
        if(f_close(&file)!=FR_OK) {ok=false;io_error=true;}
        if(stopped(cancel)) {library_free(&candidate);library_free(out);return LOAD_ERROR;}
        if(io_error || memory_error) {library_free(&candidate);library_free(out);return LOAD_ERROR;}
        if(!ok) {library_free(&candidate);continue;}
        if(old_library_validate(&candidate,root,identity,epoch) &&
           (!out->wire || candidate.sequence>out->sequence)) {
            library_free(out);*out=candidate;memset(&candidate,0,sizeof(candidate));out->slot=slot;result=LOAD_OK;
        }
        bool resource_error=candidate.resource_error;library_free(&candidate);
        if(resource_error) {library_free(out);return LOAD_ERROR;}
    }
    if(result==LOAD_OK && !old_library_groups(out)) {library_free(out);return LOAD_ERROR;}
    return result;
}
#undef malloc
#undef calloc

#define require(condition) do {if(!(condition)) {fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#condition);abort();}} while(0)

/* Refuse exactly one allocation: the nth request of one kind and shape. */
static struct {char kind;size_t count,bytes;unsigned nth,seen;bool hit;} fault;
static void refuse(char kind,size_t count,size_t bytes,unsigned nth) {
    fault.kind=kind;fault.count=count;fault.bytes=bytes;fault.nth=nth;fault.seen=0;fault.hit=false;
}
static bool refused(char kind,size_t count,size_t bytes) {
    if(!fault.nth || fault.hit || kind!=fault.kind || count!=fault.count || bytes!=fault.bytes || ++fault.seen!=fault.nth)return false;
    fault.hit=true;return true;
}
static void *test_malloc(size_t bytes) {return refused('m',1,bytes)?NULL:malloc(bytes);}
static void *test_calloc(size_t count,size_t bytes) {return refused('c',count,bytes)?NULL:calloc(count,bytes);}

#define ROOT "/Games"
static const uint8_t volume_id[32]={0x4b,0x55,0x49,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29};
static const uint64_t generation=3;

/* The two slot files of ROOT. A read reaching bad_from fails; one reaching
 * short_from returns fewer bytes; reported, when set, replaces f_size. */
static struct slot_file {uint8_t *data;size_t size,reported,bad_from,short_from;bool present;FRESULT open_result,close_result;} slots[2];
static struct {unsigned opens,reads,open_files;size_t bytes,cancel_after;bool cancelled;} io;
static bool cancel(void) {
    if(io.cancel_after && io.bytes>=io.cancel_after)io.cancelled=true;
    return io.cancelled;
}
static unsigned slot_of(const char *path) {
    char a[96],b[96];library_path(a,ROOT,0);library_path(b,ROOT,1);
    require(!strcmp(path,a) || !strcmp(path,b));return !strcmp(path,b);
}
FRESULT f_open(FIL *file,const TCHAR *path,BYTE mode) {
    require(mode==FA_READ);++io.opens;
    unsigned slot=slot_of(path);const struct slot_file *s=&slots[slot];
    if(!s->present)return FR_NO_FILE;
    if(s->open_result!=FR_OK)return s->open_result;
    memset(file,0,sizeof(*file));file->obj.sclust=slot+1u;file->obj.objsize=s->reported?s->reported:s->size;
    ++io.open_files;return FR_OK;
}
FRESULT f_read(FIL *file,void *out,UINT count,UINT *got) {
    require(file->obj.sclust==1u || file->obj.sclust==2u);
    const struct slot_file *s=&slots[file->obj.sclust-1u];size_t at=(size_t)file->fptr,end=at+count;
    ++io.reads;*got=0;
    if(s->bad_from && end>s->bad_from)return FR_DISK_ERR;
    if(end>s->size)end=s->size;
    if(s->short_from && end>s->short_from)end=s->short_from;
    size_t n=end>at?end-at:0;
    if(n)memcpy(out,s->data+at,n);
    file->fptr+=n;*got=(UINT)n;io.bytes+=n;return FR_OK;
}
FRESULT f_close(FIL *file) {
    require((file->obj.sclust==1u || file->obj.sclust==2u) && io.open_files);
    FRESULT result=slots[file->obj.sclust-1u].close_result;file->obj.sclust=0;--io.open_files;return result;
}
void kui_games_covers_content_clear(void) {}

/* Synthetic catalogues: ordinary rows, explicit disc sets, capture families
 * proved by IP fields (conflicts, duplicates, mixed names, two releases and
 * truncated UTF-8 labels), folders, disabled rows, 2048 pairs, duplicate,
 * case-colliding and prefix names, and ASCII-fold edge characters. */
struct record {
    char name[KUI_DEST_NAME_CAP],path[KUI_GAMES_FILE_CAP],variant[KUI_GAMES_FILE_CAP],title[KUI_COVER_TITLE_CAP],region[9];
    bool directory,disabled;
    struct kui_games_group_evidence evidence;
};
static struct record records[LIBRARY_ROWS];
enum mode {MIXED,SHORT_NAMES,GROUP_HEAVY};
static uint32_t seed_state;
static unsigned roll(unsigned range) {seed_state=seed_state*1664525u+1013904223u;return (seed_state>>8)%range;}
static const char *const words[]={"Sonic","Adventure","Skies","of","Arcadia","Crazy","Taxi","Jet","Set","Radio",
    "Power","Stone","Soul","Calibur","Grandia","Rez","Ikaruga","Chu","Rocket","Space","Channel","Virtua","Tennis",
    "Fighter","Marvel","Street","Dead","Rising","House","Ecco","Toy","Commander","Hydro","Thunder","Racer",
    "Phantasy","Star","Online","Quake","Legacy","Kain","Rayman","Evolution","Time","Stalkers","Under","Defeat",
    "Border","Down","Zero","Gunner","Giga","Wing","Spike","Project","Justice","Plasma","Sword","Metal","Cop",
    "Fur","Fighters","2","3","II","Ultimate","the","and","Zone","a","A","_","[x]","`q","~z"};
static const char *const tails[]={""," (USA)"," (Japan)"," (Europe)"," [!]"," (Rev 1)",""};
static void recase(char *text,unsigned mode) {
    for(bool first=true;*text;text++,first=false) {
        bool upper=mode==1 || (mode==3 && first);
        if(upper && *text>='a' && *text<='z')*text=(char)(*text-'a'+'A');
        else if((mode==2 || (mode==3 && !first)) && *text>='A' && *text<='Z')*text=(char)(*text-'A'+'a');
    }
}
static void words_title(char out[64]) {
    unsigned count=1u+roll(4);out[0]=0;
    for(unsigned i=0;i<count;i++) {
        const char *word=words[roll(sizeof(words)/sizeof(words[0]))];
        if(strlen(out)+strlen(word)+2u>44u)break;
        if(i)strcat(out," ");
        strcat(out,word);
    }
    strcat(out,tails[roll(sizeof(tails)/sizeof(tails[0]))]);
    unsigned mode=roll(10);recase(out,mode<3?mode:0u);
}
static void short_title(char out[24]) {
    static const char *const glyphs[]={"a","A","b","B","_","[","`","~","1","\xc3\xa9","\xc3\x89"," a"};
    unsigned count=1u+roll(5);out[0]=0;
    for(unsigned i=0;i<count;i++)strcat(out,glyphs[roll(i?12u:11u)]);
}
static struct record *add(unsigned *count) {struct record *r=&records[(*count)++];memset(r,0,sizeof(*r));return r;}
static void as_file(struct record *r,const char *name) {
    snprintf(r->name,sizeof(r->name),"%s",name);snprintf(r->path,sizeof(r->path),ROOT "/%s/disc.gdi",name);
}
static void prove(struct record *r,unsigned disc,unsigned total,unsigned product,unsigned version,const char *region,const char *title) {
    r->evidence.disc=disc;r->evidence.total=total;
    snprintf(r->evidence.product,sizeof(r->evidence.product),"MK-5%04u",product%10000u);
    snprintf(r->evidence.version,sizeof(r->evidence.version),"V1.%03u",version%1000u);
    snprintf(r->evidence.region,sizeof(r->evidence.region),"%s",region);
    snprintf(r->evidence.title,sizeof(r->evidence.title),"%s",title);
}
static void single(unsigned *count) {
    char name[64];words_title(name);struct record *r=add(count);as_file(r,name);unsigned pick=roll(20);
    if(pick<2)snprintf(r->variant,sizeof(r->variant),ROOT "/%s-2048/disc.gdi",name);
    if(pick>=2 && pick<5) {snprintf(r->title,sizeof(r->title),"%s",name);recase(r->title,1);strcpy(r->region,"JUE");}
    if(pick==5)strcpy(r->region,"E");
}
static void disc_set(unsigned *count,unsigned limit) {
    static const char *const markers[]={" Disc %u"," (Disc %u)"," CD%u","_Disc_%u"," (Disc %u of 3)"," Disk %u"," D%u"," disc 0%u"};
    char base[64];words_title(base);const char *marker=markers[roll(8)];
    unsigned discs=2u+roll(3),duplicate=roll(6)?0u:1u+roll(discs);
    for(unsigned d=1;d<=discs && *count<limit;d++) {
        char name[KUI_DEST_NAME_CAP],tail[24];snprintf(tail,sizeof(tail),marker,duplicate==d?1u:d);
        snprintf(name,sizeof(name),"%s%s",base,tail);
        if(!roll(8))recase(name,1u+roll(3));
        struct record *r=add(count);
        if(roll(16))as_file(r,name);
        else {snprintf(r->name,sizeof(r->name),"%s",name);snprintf(r->path,sizeof(r->path),ROOT "/%s",name);r->directory=true;}
    }
}
static void capture_family(unsigned *count,unsigned limit) {
    char base[64],ip_title[64];words_title(base);snprintf(ip_title,sizeof(ip_title),"%s",base);recase(ip_title,1);
    unsigned total=2u+roll(4),discs=total-roll(2),product=roll(10000),version=roll(4);
    unsigned conflict=roll(6)?0u:1u+roll(discs),duplicate=roll(8)?0u:1u+roll(discs),marked=roll(5)?0u:1u+roll(discs);
    const char *region=roll(3)?"JUE":"U";
    for(unsigned d=1;d<=discs && *count<limit;d++) {
        char name[KUI_DEST_NAME_CAP];
        if(marked==d)snprintf(name,sizeof(name),"%s Disc %u",base,roll(4)?d:d+1u);
        else if(d==1)snprintf(name,sizeof(name),"%s",base);
        else snprintf(name,sizeof(name),"%s (%u)",base,d);
        struct record *r=add(count);as_file(r,name);
        prove(r,duplicate==d && d>1u?1u:d,total,product,conflict==d?version+1u:version,region,ip_title);
        if(!roll(6))snprintf(r->variant,sizeof(r->variant),ROOT "/%s-2048/disc.gdi",name);
    }
}
/* Two releases under one capture name: different fields get qualifiers;
 * equal visible fields with different IP titles also get set numbers. */
static void two_releases(unsigned *count,unsigned limit,bool long_name) {
    char base[112]={0},ip_title[64]="SYNTHETIC RELEASE";
    if(long_name)for(unsigned i=0;i<36;i++)strcat(base,"\xe6\xb5\xb7");
    else {words_title(ip_title);strcpy(base,ip_title);recase(ip_title,1);}
    unsigned product=roll(10000);bool same_fields=long_name || roll(2);
    for(unsigned release=0;release<2;release++)for(unsigned d=1;d<=2 && *count<limit;d++) {
        char name[KUI_DEST_NAME_CAP],title[72];unsigned ordinal=release*2u+d;
        if(ordinal==1)snprintf(name,sizeof(name),"%s",base);else snprintf(name,sizeof(name),"%s (%u)",base,ordinal);
        snprintf(title,sizeof(title),"%s%s",ip_title,same_fields && release?" B":"");
        struct record *r=add(count);as_file(r,name);
        prove(r,d,2,product,same_fields?3u:3u+release,same_fields?"JUE":release?"E":"U",title);
    }
}
static void collide(unsigned *count) {
    char name[KUI_DEST_NAME_CAP];snprintf(name,sizeof(name),"%s",records[roll(*count)].name);
    recase(name,roll(4));as_file(add(count),name);
}
static void extend(unsigned *count) {
    const char *old=records[roll(*count)].name;
    if(strlen(old)>100) {single(count);return;}
    char name[KUI_DEST_NAME_CAP];snprintf(name,sizeof(name),"%.100s%s",old,roll(2)?" 2":"x");as_file(add(count),name);
}
static unsigned generate(enum mode mode,unsigned limit,uint32_t seed) {
    seed_state=seed;unsigned count=0;
    while(count<limit) {
        unsigned pick=roll(100);char name[64];
        if(mode==SHORT_NAMES) {
            if(pick<70 || !count) {short_title(name);as_file(add(&count),name);}
            else if(pick<85)collide(&count);
            else if(pick<95)extend(&count);
            else single(&count);
            continue;
        }
        unsigned sets=mode==GROUP_HEAVY?35u:5u,families=sets+(mode==GROUP_HEAVY?30u:4u),releases=families+(mode==GROUP_HEAVY?8u:1u);
        if(pick<sets)disc_set(&count,limit);
        else if(pick<families)capture_family(&count,limit);
        else if(pick<releases)two_releases(&count,limit,false);
        else if(pick==releases)two_releases(&count,limit,true);
        else if(!count)single(&count);
        else if(pick<releases+10)collide(&count);
        else if(pick<releases+13)extend(&count);
        else if(pick<releases+18) {short_title(name);as_file(add(&count),name);}
        else if(pick<releases+21) {
            words_title(name);struct record *r=add(&count);
            snprintf(r->name,sizeof(r->name),"%s",name);snprintf(r->path,sizeof(r->path),ROOT "/%s",name);r->directory=true;
        } else if(pick<releases+22) {
            words_title(name);struct record *r=add(&count);
            if(roll(2))as_file(r,name);else snprintf(r->name,sizeof(r->name),"%s",name);
            r->disabled=true;
        } else single(&count);
    }
    return count;
}
/* An independent encoder for the v4 card layout. */
static uint8_t *encode(unsigned count,const char *root,uint64_t sequence,size_t *bytes) {
    size_t root_bytes=strlen(root),total=LIBRARY_HEADER+root_bytes;
    for(unsigned i=0;i<count;i++) {
        const struct record *r=&records[i];
        total+=RECORD_HEADER+GROUP_EVIDENCE_BYTES+strlen(r->name)+strlen(r->path)+strlen(r->variant)+strlen(r->title)+strlen(r->region);
    }
    uint8_t *wire=calloc(1,total);require(wire);
    memcpy(wire,"KUIGLIB4",8);put32(wire+8,4);put32(wire+12,(uint32_t)total);put64(wire+16,sequence);put64(wire+24,generation);
    memcpy(wire+32,volume_id,32);put16(wire+64,(unsigned)root_bytes);put16(wire+66,count);memcpy(wire+LIBRARY_HEADER,root,root_bytes);
    size_t at=LIBRARY_HEADER+root_bytes;
    for(unsigned i=0;i<count;i++) {
        const struct record *r=&records[i];const char *fields[]={r->name,r->path,r->variant,r->title,r->region};
        uint8_t *p=wire+at;
        for(unsigned f=0;f<5;f++)put16(p+2u*f,(unsigned)strlen(fields[f]));
        p[10]=(uint8_t)((r->directory?1u:0u)|(r->disabled?2u:0u));
        if(r->evidence.disc) {p[11]=(uint8_t)r->evidence.disc;p[12]=(uint8_t)r->evidence.total;}
        at+=RECORD_HEADER;
        for(unsigned f=0;f<5;f++) {size_t n=strlen(fields[f]);memcpy(wire+at,fields[f],n);at+=n;}
        if(r->evidence.disc) {
            memcpy(wire+at,r->evidence.product,strlen(r->evidence.product));
            memcpy(wire+at+11,r->evidence.version,strlen(r->evidence.version));
            memcpy(wire+at+18,r->evidence.title,strlen(r->evidence.title));
            memcpy(wire+at+147,r->evidence.region,strlen(r->evidence.region));
        }
        at+=GROUP_EVIDENCE_BYTES;
    }
    require(at==total);
    put32(wire+68,kui_crc32(0,wire+LIBRARY_HEADER,total-LIBRARY_HEADER));put32(wire+72,kui_crc32(0,wire,72));
    *bytes=total;return wire;
}
/* Aliases collide with names, group titles and each other. */
static void alias_text(char out[129],unsigned count) {
    static const char *const fixed[]={"AAA","aaa","Aaa","zzz","_alias","[alias]","~tilde","Sonic","sonic adventure","1st"};
    unsigned pick=roll(14);
    if(pick<10 || !count) {strcpy(out,fixed[pick%10u]);return;}
    const char *name=records[roll(count)].name;size_t n=strlen(name);bool printable=n<129 && name[0]!=' ' && name[n-1]!=' ';
    for(size_t i=0;i<n;i++)if((unsigned char)name[i]<32 || (unsigned char)name[i]>126)printable=false;
    strcpy(out,printable?name:"Fallback alias");
    if(printable && roll(2))recase(out,1u+roll(3));
}
static void alias_records(unsigned count,unsigned every) {
    for(unsigned i=roll(every);i<count && edits->count<90u;i+=every) {
        const struct record *r=&records[i];struct kui_games_metadata_entry e={0};struct kui_games_activity_entry key;
        if(r->directory || r->disabled || !kui_games_activity_key(&key,r->path,""))continue;
        strcpy(e.path,key.path);strcpy(e.anchor,key.path);alias_text(e.edit.alias,count);
        require(kui_games_metadata_upsert(edits,&e));
    }
}
static void alias_groups(const struct library *lib,unsigned count) {
    for(unsigned g=roll(2);g<lib->group_count && edits->count<120u;g+=2)if(lib->groups[g].valid) {
        struct kui_games_metadata_entry e={0};struct kui_games_activity_entry key;
        require(kui_games_activity_key(&key,ROOT,lib->groups[g].key));
        strcpy(e.path,key.path);strcpy(e.group,key.group);strcpy(e.anchor,key.path);alias_text(e.edit.alias,count);
        require(kui_games_metadata_upsert(edits,&e));
    }
}
static void same_library(const struct library *a,const struct library *b) {
    require(a->bytes==b->bytes && !memcmp(a->wire,b->wire,a->bytes) && a->count==b->count && a->version==b->version);
    require(a->sequence==b->sequence && a->epoch==b->epoch && a->slot==b->slot && !memcmp(a->identity,b->identity,32) && !strcmp(a->root,b->root));
    require(a->group_count==b->group_count && a->display_count==b->display_count && a->resource_error==b->resource_error);
    for(unsigned i=0;i<a->count;i++)
        require(a->offsets[i]==b->offsets[i] && a->members[i].group==b->members[i].group &&
            a->members[i].disc==b->members[i].disc && a->group_display[i]==b->group_display[i]);
    for(unsigned i=0;i<a->display_count;i++)require(a->display[i]==b->display[i]);
    for(unsigned g=0;g<a->group_count;g++) {
        const struct library_group *x=&a->groups[g],*y=&b->groups[g];
        require(!strcmp(x->key,y->key) && !strcmp(x->name,y->name) && x->count==y->count && x->first==y->first &&
            x->representative==y->representative && x->label_set==y->label_set && x->valid==y->valid && x->metadata==y->metadata);
    }
}
static void admit(struct library *lib,const uint8_t *wire,size_t bytes,bool previous) {
    memset(lib,0,sizeof(*lib));lib->wire=malloc(bytes);require(lib->wire);memcpy(lib->wire,wire,bytes);lib->bytes=bytes;
    if(previous)require(old_library_validate(lib,ROOT,volume_id,generation) && old_library_groups(lib));
    else require(library_validate(lib,ROOT,volume_id,generation) && library_groups(lib));
}
static void catalogue_check(enum mode mode,unsigned limit,uint32_t seed) {
    unsigned count=generate(mode,limit,seed);size_t bytes=0;uint8_t *wire=encode(count,ROOT,7,&bytes);
    memset(edits,0,sizeof(*edits));alias_records(count,mode==SHORT_NAMES?7u:13u);
    struct library previous,current;
    admit(&previous,wire,bytes,true);admit(&current,wire,bytes,false);same_library(&previous,&current);
    unsigned record_aliases=edits->count;alias_groups(&previous,count);
    library_free(&previous);library_free(&current);
    admit(&previous,wire,bytes,true);admit(&current,wire,bytes,false);same_library(&previous,&current);
    /* Without the key table the sort decodes rows on every comparison. */
    struct library fallback;refuse('m',1,count*sizeof(struct display_key),1);admit(&fallback,wire,bytes,false);
    require(fault.hit || current.display_count<2u);refuse(0,0,0,0);same_library(&previous,&fallback);library_free(&fallback);
    unsigned valid=0,metadata=0,labelled=0;
    for(unsigned g=0;g<current.group_count;g++)if(current.groups[g].valid) {
        ++valid;metadata+=current.groups[g].metadata;labelled+=current.groups[g].label_set!=0;
    }
    /* A saved edit re-sorts the admitted catalogue in place. */
    library_free(&saved);saved=current;memset(&current,0,sizeof(current));
    for(unsigned i=0;i<edits->count;i++) {
        unsigned pick=roll(4);
        if(!pick)edits->entries[i].edit.alias[0]=0;
        else if(pick==1)alias_text(edits->entries[i].edit.alias,count);
    }
    if(seed&1u)refuse('m',1,count*sizeof(struct display_key),1);
    edits_changed();
    require(!(seed&1u) || fault.hit || saved.display_count<2u);refuse(0,0,0,0);
    old_sorting=&previous;qsort(previous.display,previous.display_count,sizeof(*previous.display),old_display_order);old_sorting=NULL;
    same_library(&previous,&saved);
    printf("PASS order %s %u records: %u rows, %u valid groups (%u proven, %u set-labelled), %u record and %u group aliases, re-sort after edits\n",
        mode==MIXED?"mixed":mode==SHORT_NAMES?"short-names":"group-heavy",count,saved.display_count,valid,metadata,labelled,
        record_aliases,edits->count-record_aliases);
    library_free(&previous);library_free(&saved);free(wire);
}

/* Slot files for the cold-load matrix: catalogue sources of unequal sizes. */
static uint8_t *sources[3];
static size_t source_bytes[3];
static unsigned source_count[3];
static void slot_clear(unsigned slot) {free(slots[slot].data);memset(&slots[slot],0,sizeof(slots[slot]));}
static void slot_copy(unsigned slot,unsigned source,uint64_t sequence) {
    slot_clear(slot);struct slot_file *s=&slots[slot];
    s->data=malloc(source_bytes[source]);require(s->data);memcpy(s->data,sources[source],source_bytes[source]);
    s->size=source_bytes[source];s->present=true;
    put64(s->data+16,sequence);put32(s->data+72,kui_crc32(0,s->data,72));
}
static void slot_reseal(unsigned slot) {
    struct slot_file *s=&slots[slot];
    put32(s->data+68,kui_crc32(0,s->data+LIBRARY_HEADER,s->size-LIBRARY_HEADER));put32(s->data+72,kui_crc32(0,s->data,72));
}
static void older_a_newer_b(void) {slot_copy(0,0,4);slot_copy(1,1,5);}
static void none(void) {slot_clear(0);slot_clear(1);}
static void only_a(void) {slot_copy(0,0,1);slot_clear(1);}
static void only_b(void) {slot_clear(0);slot_copy(1,1,1);}
static void a_newer(void) {slot_copy(0,0,5);slot_copy(1,1,4);}
static void equal_sequence(void) {slot_copy(0,0,3);slot_copy(1,1,3);}
static void newer_magic(void) {older_a_newer_b();memcpy(slots[1].data,"BROKEN!!",8);}
static void newer_pending(void) {older_a_newer_b();memcpy(slots[1].data,"KUIGTMP2",8);put32(slots[1].data+72,kui_crc32(0,slots[1].data,72));}
static void newer_body(void) {older_a_newer_b();slots[1].data[slots[1].size-1u]^=0x5a;}
static void newer_record(void) {older_a_newer_b();slots[1].data[LIBRARY_HEADER+strlen(ROOT)+RECORD_HEADER]='?';slot_reseal(1);}
static void newer_truncated(void) {older_a_newer_b();slots[1].size-=100u;}
static void newer_headerless(void) {older_a_newer_b();slots[1].size=50u;}
static void newer_oversized(void) {older_a_newer_b();slots[1].reported=LIBRARY_MAX_BYTES+1u;}
static void newer_other_root(void) {slot_copy(0,0,4);slot_copy(1,2,5);}
static void newer_other_epoch(void) {older_a_newer_b();put64(slots[1].data+24,generation+1u);slot_reseal(1);}
static void older_corrupt(void) {older_a_newer_b();memcpy(slots[0].data,"BROKEN!!",8);}
static void both_damaged(void) {newer_pending();slots[0].data[slots[0].size-1u]^=0x5a;}
static void other_volume(void) {only_b();slots[1].data[40]^=1u;slot_reseal(1);}
static void open_error_a(void) {older_a_newer_b();slots[0].open_result=FR_DISK_ERR;}
static void open_error_b(void) {older_a_newer_b();slots[1].open_result=FR_INT_ERR;}
static void header_read_error(void) {older_a_newer_b();slots[0].bad_from=1;}
static void newer_read_error(void) {older_a_newer_b();slots[1].bad_from=4096;}
static void newer_short_read(void) {older_a_newer_b();slots[1].short_from=4096;}
static void close_error(void) {older_a_newer_b();slots[1].close_result=FR_DISK_ERR;}
static void fallback_read_error(void) {newer_body();slots[0].bad_from=4096;}
static void older_read_error(void) {older_a_newer_b();slots[0].bad_from=4096;}
static void damaged_read_error(void) {older_corrupt();slots[0].bad_from=4096;}
static void cancel_before(void) {older_a_newer_b();io.cancelled=true;}
static void cancel_during(void) {older_a_newer_b();io.cancel_after=2u*LIBRARY_HEADER+1u;}
static void newer_memory(void) {older_a_newer_b();refuse('m',1,source_bytes[1],1);}
static void older_memory(void) {older_a_newer_b();refuse('m',1,source_bytes[0],1);}
static void offsets_memory(void) {only_b();refuse('c',source_count[1],sizeof(uint32_t),1);}
static void groups_memory(void) {older_a_newer_b();refuse('c',source_count[1],sizeof(struct library_group),1);}

struct outcome {enum load_result result;struct library lib;unsigned opens,reads;size_t bytes;};
static void run(void (*setup)(void),bool previous,struct outcome *out) {
    memset(&io,0,sizeof(io));refuse(0,0,0,0);setup();memset(&out->lib,0,sizeof(out->lib));
    out->result=previous?old_library_load(&out->lib,ROOT,volume_id,generation,cancel):
        library_load(&out->lib,ROOT,volume_id,generation,cancel);
    require(!io.open_files && (out->result==LOAD_OK)==(out->lib.wire!=NULL));
    out->opens=io.opens;out->reads=io.reads;out->bytes=io.bytes;
}
static const char *result_name(enum load_result result) {return result==LOAD_OK?"ok":result==LOAD_MISSING?"missing":"error";}
/* before/after: the previous and current results; slot: the current choice. */
static void load_case(const char *name,void (*setup)(void),enum load_result before,enum load_result after,unsigned slot) {
    struct outcome previous,current;run(setup,true,&previous);run(setup,false,&current);
    require(previous.result==before && current.result==after);
    if(after==LOAD_OK)require(current.lib.slot==slot);
    if(before==LOAD_OK && after==LOAD_OK)same_library(&previous.lib,&current.lib);
    printf("PASS load %s: previous %s after %u opens/%u reads/%zu bytes; current %s",name,result_name(before),
        previous.opens,previous.reads,previous.bytes,result_name(after));
    if(after==LOAD_OK)printf(" slot %c",slot?'b':'a');
    printf(" after %u opens/%u reads/%zu bytes\n",current.opens,current.reads,current.bytes);
    library_free(&previous.lib);library_free(&current.lib);
}
static void read_volume(const char *name,void (*setup)(void),size_t expected) {
    struct outcome previous,current;run(setup,true,&previous);run(setup,false,&current);
    require(previous.result==LOAD_OK && current.result==LOAD_OK && current.bytes==expected);
    printf("EVIDENCE %s: previous load read %zu bytes in %u reads/%u opens; current %zu bytes in %u reads/%u opens\n",
        name,previous.bytes,previous.reads,previous.opens,current.bytes,current.reads,current.opens);
    library_free(&previous.lib);library_free(&current.lib);
}
static void load_checks(void) {
    static const uint32_t seeds[]={101,102,103};
    for(unsigned i=0;i<3;i++) {
        source_count[i]=generate(MIXED,40u+i,seeds[i]);
        sources[i]=encode(source_count[i],i==2?"/Other":ROOT,1,&source_bytes[i]);
    }
    require(source_bytes[0]!=source_bytes[1]);
    memset(edits,0,sizeof(*edits));alias_records(source_count[2],5);
    load_case("none",none,LOAD_MISSING,LOAD_MISSING,0);
    load_case("only-a",only_a,LOAD_OK,LOAD_OK,0);
    load_case("only-b",only_b,LOAD_OK,LOAD_OK,1);
    load_case("a-newer",a_newer,LOAD_OK,LOAD_OK,0);
    load_case("b-newer",older_a_newer_b,LOAD_OK,LOAD_OK,1);
    load_case("equal-sequence",equal_sequence,LOAD_OK,LOAD_OK,0);
    load_case("newer-bad-magic",newer_magic,LOAD_OK,LOAD_OK,0);
    load_case("newer-pending",newer_pending,LOAD_OK,LOAD_OK,0);
    load_case("newer-body-crc",newer_body,LOAD_OK,LOAD_OK,0);
    load_case("newer-bad-record",newer_record,LOAD_OK,LOAD_OK,0);
    load_case("newer-truncated",newer_truncated,LOAD_OK,LOAD_OK,0);
    load_case("newer-headerless",newer_headerless,LOAD_OK,LOAD_OK,0);
    load_case("newer-oversized",newer_oversized,LOAD_OK,LOAD_OK,0);
    load_case("newer-other-root",newer_other_root,LOAD_OK,LOAD_OK,0);
    load_case("newer-other-epoch",newer_other_epoch,LOAD_OK,LOAD_OK,0);
    load_case("older-corrupt",older_corrupt,LOAD_OK,LOAD_OK,1);
    load_case("both-damaged",both_damaged,LOAD_MISSING,LOAD_MISSING,0);
    load_case("other-volume",other_volume,LOAD_MISSING,LOAD_MISSING,0);
    load_case("open-error-a",open_error_a,LOAD_ERROR,LOAD_ERROR,0);
    load_case("open-error-b",open_error_b,LOAD_ERROR,LOAD_ERROR,0);
    load_case("header-read-error",header_read_error,LOAD_ERROR,LOAD_ERROR,0);
    load_case("newer-read-error",newer_read_error,LOAD_ERROR,LOAD_ERROR,0);
    load_case("newer-short-read",newer_short_read,LOAD_ERROR,LOAD_ERROR,0);
    load_case("close-error",close_error,LOAD_ERROR,LOAD_ERROR,0);
    load_case("fallback-read-error",fallback_read_error,LOAD_ERROR,LOAD_ERROR,0);
    load_case("cancel-before",cancel_before,LOAD_ERROR,LOAD_ERROR,0);
    load_case("cancel-during",cancel_during,LOAD_ERROR,LOAD_ERROR,0);
    load_case("newer-memory",newer_memory,LOAD_ERROR,LOAD_ERROR,0);
    load_case("offsets-memory",offsets_memory,LOAD_ERROR,LOAD_ERROR,0);
    load_case("groups-memory",groups_memory,LOAD_ERROR,LOAD_ERROR,0);
    /* Intended difference: the header-first load no longer reads an older
     * slot behind a valid newer one, nor anything after a refused header,
     * so faults confined to those bytes no longer fail the load. */
    load_case("older-read-error",older_read_error,LOAD_ERROR,LOAD_OK,1);
    load_case("damaged-read-error",damaged_read_error,LOAD_ERROR,LOAD_OK,1);
    load_case("older-memory",older_memory,LOAD_ERROR,LOAD_OK,1);
    /* Two fixed headers, then complete slots newest first. */
    read_volume("b-newer",older_a_newer_b,2u*LIBRARY_HEADER+source_bytes[1]);
    read_volume("a-newer",a_newer,2u*LIBRARY_HEADER+source_bytes[0]);
    read_volume("newer-bad-magic",newer_magic,2u*LIBRARY_HEADER+source_bytes[0]);
    read_volume("newer-body-crc",newer_body,2u*LIBRARY_HEADER+source_bytes[1]+source_bytes[0]);
    slot_clear(0);slot_clear(1);
    for(unsigned i=0;i<3;i++)free(sources[i]);
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    edits=calloc(1,sizeof(*edits));require(edits);
    static const struct {enum mode mode;unsigned count;uint32_t seed;} catalogues[]={
        {MIXED,0,1},{MIXED,1,2},{MIXED,2,3},{MIXED,3,4},{MIXED,7,5},{MIXED,41,6},{MIXED,300,7},{MIXED,300,8},
        {MIXED,1000,9},{MIXED,LIBRARY_ROWS,10},{SHORT_NAMES,64,21},{SHORT_NAMES,300,22},{SHORT_NAMES,LIBRARY_ROWS,23},
        {GROUP_HEAVY,40,31},{GROUP_HEAVY,300,32},{GROUP_HEAVY,LIBRARY_ROWS,33}};
    for(unsigned i=0;i<sizeof(catalogues)/sizeof(catalogues[0]);i++)catalogue_check(catalogues[i].mode,catalogues[i].count,catalogues[i].seed);
    load_checks();
    free(edits);edits=NULL;
    puts("PASS saved Games catalogue load, grouping and order match the frozen previous implementation");
    return 0;
}
