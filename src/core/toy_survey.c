/* SPDX-License-Identifier: GPL-3.0-only */
/* The stream survey's report file (include/kui/toy_survey.h): its text, the
 * search for a blank slot and the Toy worker's seed for it. Read-only: the
 * caller writes. Linked into Test 83's cold stage and its host test only. */
#include "kui/toy_survey.h"
#include "kui/retail_image.h"
#include <string.h>

/* A sector under construction: spaces, a line feed in its last byte. */
struct text {uint8_t *out;uint32_t at;};
static void text_begin(struct text *t,uint8_t out[512]) {
    memset(out,' ',KUI_TOY_SURVEY_SECTOR);out[KUI_TOY_SURVEY_SECTOR-1u]='\n';
    t->out=out;t->at=0u;
}
static void put(struct text *t,char c) {
    if(t->at<KUI_TOY_SURVEY_SECTOR-1u) t->out[t->at++]=(uint8_t)c;
}
static void puts_(struct text *t,const char *s) {while(*s) put(t,*s++);}
static void put_hex(struct text *t,uint32_t value,unsigned digits) {
    while(digits--) put(t,"0123456789ABCDEF"[(value>>(4u*digits))&15u]);
}
static void put_decimal(struct text *t,uint32_t value,unsigned least) {
    char digits[10];unsigned n=0u;
    do {digits[n++]=(char)('0'+value%10u);value/=10u;} while(value);
    while(n<least) digits[n++]='0';
    while(n) put(t,digits[--n]);
}
/* A field of the disc's map: printable ASCII, cut at a terminator and at
 * its last character that is no space, and at `most`. */
static void put_field(struct text *t,const char *field,uint32_t bytes,uint32_t most) {
    uint32_t length=0u;
    if(!field) bytes=0u;
    for(uint32_t i=0u;i<bytes && field[i];i++) if(field[i]!=' ') length=i+1u;
    if(length>most) length=most;
    for(uint32_t i=0u;i<length;i++) {
        unsigned char c=(unsigned char)field[i];
        put(t,c>=0x20u && c<0x7fu?(char)c:'?');
    }
}
/* A count of the disc's map, in three digits at most: the launch record
 * fits its sector whatever the map holds. */
static void put_count(struct text *t,uint32_t value) {put_decimal(t,value>999u?999u:value,1u);}
static void put_id(struct text *t,const struct kui_toy_survey_file *file) {
    for(unsigned i=0u;i<KUI_TOY_SURVEY_ID_BYTES;i++) put_hex(t,file->id[i],2u);
}

void kui_toy_survey_file_sector(const struct kui_toy_survey_file *file,uint8_t out[512]) {
    struct text t;text_begin(&t,out);
    puts_(&t,"K-UI STREAM SURVEY FILE 1\nID ");put_id(&t,file);
    puts_(&t,"\nSLOTS ");put_decimal(&t,file->slots,1u);
    puts_(&t,"\nEACH LAUNCH OF A GAME WITH THE SURVEY MODULE FILLS ONE SLOT BELOW.\n"
        "SEND THIS FILE BACK AS IT IS.\n");
}

static int hex_digit(uint8_t c) {
    return c>='0' && c<='9'?c-'0':c>='A' && c<='F'?c-'A'+10:-1;
}
int kui_toy_survey_file_parse(const uint8_t sector[512],struct kui_toy_survey_file *out) {
    static const char head[]="K-UI STREAM SURVEY FILE 1\nID ";
    static const char slots[]="\nSLOTS ";
    const uint32_t id_at=sizeof(head)-1u,slots_at=id_at+2u*KUI_TOY_SURVEY_ID_BYTES;
    uint8_t expect[KUI_TOY_SURVEY_SECTOR];
    memset(out,0,sizeof(*out));
    if(memcmp(sector,head,sizeof(head)-1u) || memcmp(sector+slots_at,slots,sizeof(slots)-1u)) return -1;
    for(unsigned i=0u;i<KUI_TOY_SURVEY_ID_BYTES;i++) {
        int high=hex_digit(sector[id_at+2u*i]),low=hex_digit(sector[id_at+2u*i+1u]);
        if(high<0 || low<0) return -1;
        out->id[i]=(uint8_t)(high<<4|low);
    }
    uint32_t count=0u;
    for(uint32_t at=slots_at+sizeof(slots)-1u,n=0u;sector[at]>='0' && sector[at]<='9';at++,n++) {
        if(n==3u) return -1;
        count=count*10u+(uint32_t)(sector[at]-'0');
    }
    if(!count || count>KUI_TOY_SURVEY_SLOTS_MAX) return -1;
    out->slots=count;
    /* Nothing but the exact sector this ID and this count give. */
    kui_toy_survey_file_sector(out,expect);
    if(memcmp(sector,expect,sizeof(expect))) {memset(out,0,sizeof(*out));return -1;}
    return 0;
}

static void slot_line(struct text *t,const struct kui_toy_survey_file *file,uint32_t slot,uint32_t part,
    const char *state) {
    puts_(t,"==== SLOT ");put_decimal(t,slot,3u);puts_(t," PART ");put_decimal(t,part,1u);
    puts_(t," ==== ID ");put_id(t,file);puts_(t," ==== ");puts_(t,state);puts_(t," ====\n");
}
void kui_toy_survey_blank_sector(const struct kui_toy_survey_file *file,uint32_t slot,uint32_t part,
    uint8_t out[512]) {
    struct text t;text_begin(&t,out);
    slot_line(&t,file,slot,part,"EMPTY");
}

void kui_toy_survey_launch_sector(const struct kui_toy_survey_file *file,uint32_t slot,
    const struct kui_toy_survey_launch *launch,uint8_t out[512]) {
    struct text t;text_begin(&t,out);
    slot_line(&t,file,slot,0u,"LAUNCH");
    puts_(&t,"SURVEY 83 BUILD ");put_field(&t,launch->build,16u,16u);
    puts_(&t,"\nPRODUCT ");put_field(&t,launch->product,launch->product_bytes,16u);
    puts_(&t,"\nTITLE ");put_field(&t,launch->title,launch->title_bytes,56u);
    puts_(&t,"\nBOOT ");put_field(&t,launch->bootfile,launch->bootfile_bytes,16u);
    puts_(&t," REGION ");put_field(&t,launch->region,launch->region_bytes,8u);
    puts_(&t,"\nIPCRC ");put_hex(&t,launch->ip_crc32,8u);
    puts_(&t," GDICRC ");put_hex(&t,launch->gdi_crc32,8u);
    puts_(&t," BOOTCRC ");put_hex(&t,launch->boot_crc32,8u);
    puts_(&t," BOOTBYTES ");put_hex(&t,launch->boot_bytes,8u);
    puts_(&t,"\nEXECCRC ");put_hex(&t,launch->exec_crc32,8u);
    puts_(&t," EXECBYTES ");put_hex(&t,launch->exec_bytes,8u);
    puts_(&t,"\nREADER ");put_hex(&t,launch->reader,1u);
    puts_(&t," TRANSPORT ");put_hex(&t,launch->transport,1u);
    puts_(&t," FLAGS ");put_hex(&t,launch->flags,8u);
    puts_(&t," SOURCE ");put_hex(&t,launch->source,1u);
    puts_(&t,"\nTRACKS ");put_count(&t,launch->tracks);
    puts_(&t," RAW ");put_count(&t,launch->raw);
    puts_(&t," COOKED ");put_count(&t,launch->cooked);
    puts_(&t," AUDIO ");put_count(&t,launch->audio);
    puts_(&t," HDAUDIO ");put_count(&t,launch->audio_high);
    puts_(&t," MAPPED ");put_count(&t,launch->audio_mapped);
    puts_(&t," OTHER ");put_count(&t,launch->other);
    puts_(&t," EXTENTS ");put_count(&t,launch->extents);
    puts_(&t,"\nSESSION ");put_hex(&t,launch->session_lba,8u);
    puts_(&t," BOOTLBA ");put_hex(&t,launch->boot_lba,8u);
    put(&t,'\n');
}

static int claim_read(const struct kui_wce_probe_volume *v,const struct kui_wce_probe_layout *layout,
    uint32_t sector,uint8_t work[512],uint32_t *lba) {
    *lba=kui_wce_probe_lba(layout,sector);
    return !*lba || *lba<v->partition_start || *lba>=v->partition_end || v->read(v->context,*lba,work)?-1:0;
}
enum kui_toy_survey_claim_result kui_toy_survey_claim_find(const struct kui_wce_probe_volume *volume,
    const struct kui_wce_probe_layout *layout,uint8_t work[512],uint8_t expect[512],
    struct kui_toy_survey_claim *out) {
    uint32_t lba;
    struct kui_toy_survey_file file;
    memset(out,0,sizeof(*out));
    if(claim_read(volume,layout,0u,work,&lba)) return KUI_TOY_SURVEY_CLAIM_IO;
    if(kui_toy_survey_file_parse(work,&file) || layout->bytes!=kui_toy_survey_bytes(file.slots))
        return KUI_TOY_SURVEY_CLAIM_NOT_FILE;
    for(uint32_t slot=0u;slot<file.slots;slot++) {
        /* Nothing of a slot is known to the caller until all of it is blank. */
        uint32_t at[KUI_TOY_SURVEY_PARTS],crc[KUI_TOY_SURVEY_PARTS],blank=0u;
        for(uint32_t part=0u;part<KUI_TOY_SURVEY_PARTS;part++) {
            if(claim_read(volume,layout,kui_toy_survey_sector(slot,part),work,&lba))
                return KUI_TOY_SURVEY_CLAIM_IO;
            kui_toy_survey_blank_sector(&file,slot,part,expect);
            /* Anything but this part's blank text: a used slot, one cut off
             * or one damaged. It is left alone, whatever it holds. */
            if(memcmp(work,expect,KUI_TOY_SURVEY_SECTOR)) break;
            at[part]=lba;crc[part]=kui_retail_crc32(0u,work,KUI_TOY_SURVEY_SECTOR);
            ++blank;
        }
        if(blank==KUI_TOY_SURVEY_PARTS) {
            /* (Three different sectors: no sector is the blank text of two parts.) */
            out->file=file;out->slot=slot;out->used=slot;
            memcpy(out->lba,at,sizeof(out->lba));memcpy(out->blank_crc,crc,sizeof(out->blank_crc));
            return KUI_TOY_SURVEY_CLAIM_OK;
        }
    }
    out->file=file;out->used=file.slots;
    return KUI_TOY_SURVEY_CLAIM_FULL;
}

void kui_toy_survey_seed_build(const struct kui_toy_survey_claim *claim,const uint8_t cid[16],
    uint32_t high_capacity,struct kui_toy_report_wire *seed) {
    memset(seed,0,sizeof(*seed));
    seed->magic[0]=KUI_TOY_REPORT_MAGIC0;seed->magic[1]=KUI_TOY_REPORT_MAGIC1;
    seed->version=KUI_TOY_REPORT_VERSION;seed->bytes=sizeof(*seed);
    seed->test=KUI_TOY_SURVEY_SEED_TEST;
    for(unsigned i=0u;i<2u;i++) {
        seed->lba[i]=claim->lba[1u+i];seed->placeholder_crc[i]=claim->blank_crc[1u+i];
    }
    memcpy(seed->cid,cid,sizeof(seed->cid));
    seed->high_capacity=high_capacity?1u:0u;
    seed->crc32=kui_retail_crc32(0u,seed,KUI_TOY_REPORT_CRC_OFFSET);
}
