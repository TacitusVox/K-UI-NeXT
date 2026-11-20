/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_nightly.h"
#include "kui/update.h"
#include "ff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KUI_UPDATE_REVIEW_FEED
#define KUI_UPDATE_REVIEW_FEED 0
#endif
#if KUI_UPDATE_REVIEW_FEED != 0 && KUI_UPDATE_REVIEW_FEED != 1
#error "KUI_UPDATE_REVIEW_FEED must be 0 or 1"
#endif
#define REVIEW_BASE "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/refs/heads/codex/themes-wince-bba-ftp/evidence/themes-bba/"
#define RAW_BASE "https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/"
#define NIGHTLY_BASE RAW_BASE "nightly-channel/"
#define STABLE_BASE RAW_BASE "release-channel/"
#define ASSET_BASE "https://github.com/TacitusVox/K-UI-NeXT/releases/download/"

bool kui_update_test_feed_available(void) {
    return KUI_UPDATE_REVIEW_FEED != 0;
}

#define FILE_LIMIT (KUI_UPDATE_MAX_PAYLOAD + KUI_UPDATE_RECEIPT_MAX)

struct feed_buffer {
    char *data;
    size_t cap, used;
};

struct job {
    struct kui_app_status *status;
    kui_app_progress_fn progress;
    kui_cancel_fn cancel;
    uint32_t offset;
};

static void message(struct job *job, const char *text) {
    snprintf(job->status->message, sizeof(job->status->message), "%s", text);
    if(job->progress) job->progress(job->status);
}

static bool collect(void *context, const void *data, size_t bytes) {
    struct feed_buffer *buffer = context;
    if(bytes > buffer->cap - buffer->used) return false;
    memcpy(buffer->data + buffer->used, data, bytes);
    buffer->used += bytes;
    return true;
}

static bool hex_field(const char *text, unsigned size) {
    for(unsigned i = 0; i < size; ++i) {
        char c = text[i];
        if((c < '0' || c > '9') && (c < 'a' || c > 'f')) return false;
    }
    return true;
}

/* All strings come from the selected channel over HTTPS. Treat malformed or
 * stale data as unavailable and never take a host or local path from the feed. */
struct lines { char *next; };

static char *next_line(struct lines *lines) {
    char *start = lines->next;
    char *end = strchr(start, '\n');
    if(!end) return NULL;
    *end = 0;
    lines->next = end + 1;
    return start;
}

static bool decimal(const char *s, uint32_t limit, uint32_t *out) {
    if(!s || !*s || (s[0] == '0' && s[1])) return false;
    uint32_t n = 0;
    while(*s) {
        unsigned digit = (unsigned char)*s++ - (unsigned)'0';
        if(digit > 9 || digit > limit || n > (limit - digit) / 10) return false;
        n = n * 10 + digit;
    }
    *out = n;
    return true;
}

static bool stable_tag(const char *s) {
    if(!s || *s++ != 'v') return false;
    for(unsigned component = 0; component < 3; component++) {
        const char *start = s;
        uint32_t value = 0;
        while(*s >= '0' && *s <= '9') {
            unsigned d = (unsigned)(*s++ - '0');
            if(value > (UINT32_MAX-d)/10) return false;
            value = value*10+d;
        }
        if(start == s || (*start=='0' && s-start>1)) return false;
        if(component < 2 && *s++ != '.') return false;
    }
    return !*s;
}

static bool asset_name(const char *s) {
    if(!s) return false;
    size_t size = strlen(s);
    if(size < 5 || size > 63 || strstr(s, "..") || strcmp(s + size-4, ".kup")) return false;
    for(size_t i=0; i<size; ++i) {
        char c=s[i];
        if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9')) continue;
        if(!i || (c!='.' && c!='-' && c!='_')) return false;
    }
    return true;
}

static bool begin_lines(struct feed_buffer *buffer, struct lines *lines) {
    if(memchr(buffer->data,0,buffer->used)) return false;
    buffer->data[buffer->used]=0;
    lines->next=buffer->data;
    return true;
}

static bool set_identity(struct kui_update_choice *choice,const char *commit,const char *size,const char *digest) {
    if(!commit || strlen(commit)!=40 || !hex_field(commit,40) || !digest || strlen(digest)!=64 ||
       !hex_field(digest,64) || !decimal(size,FILE_LIMIT,&choice->size) || choice->size<KUI_UPDATE_HEADER_BYTES)
        return false;
    memcpy(choice->commit,commit,sizeof(choice->commit));
    memcpy(choice->digest,digest,sizeof(choice->digest));
    return true;
}

static bool set_release(struct kui_update_choice *choice,const char *tag,const char *name) {
    if(!stable_tag(tag) || !asset_name(name)) return false;
    strcpy(choice->tag,tag);
    strcpy(choice->asset,name);
    return true;
}

static bool parse_manifest(struct feed_buffer *buffer, bool stable, struct kui_update_choice *choice) {
    struct lines lines;
    if(!begin_lines(buffer,&lines)) return false;
    const char *magic=next_line(&lines), *commit=next_line(&lines), *size=next_line(&lines), *digest=next_line(&lines);
    if(!magic || strcmp(magic,stable?"KUI-RELEASE-1":"KUI-NIGHTLY-1") ||
       !set_identity(choice,commit,size,digest)) return false;
    if(stable) {
        const char *tag=next_line(&lines), *name=next_line(&lines);
        if(!set_release(choice,tag,name)) return false;
    }
    choice->scope=stable?KUI_UPDATE_SCOPE_FULL:KUI_UPDATE_SCOPE_PARTIAL;
    choice->components=stable?0:KUI_UPDATE_COMPONENT_GUI;
    return !*lines.next;
}

static unsigned digits2(const char *p) {
    return (unsigned)(p[0]-'0')*10u+(unsigned)(p[1]-'0');
}

/* A checked fixed-width UTC timestamp also sorts with strcmp. */
static bool publication_time(const char *date) {
    static const char shape[]="0000-00-00T00:00:00Z";
    static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(!date || strlen(date)!=sizeof(shape)-1) return false;
    for(unsigned i=0;i<sizeof(shape)-1;i++) {
        if(shape[i]=='0') {
            if(date[i]<'0' || date[i]>'9') return false;
        } else if(date[i]!=shape[i]) return false;
    }
    unsigned year=digits2(date)*100u+digits2(date+2), month=digits2(date+5), day=digits2(date+8);
    bool leap=year%4==0 && (year%100!=0 || year%400==0);
    return year && month && month<=12 && day && day<=days[month-1]+(month==2 && leap) &&
           digits2(date+11)<=23 && digits2(date+14)<=59 && digits2(date+17)<=59;
}

static char *next_field(char **cursor) {
    char *field=*cursor;
    if(!field) return NULL;
    char *delimiter=strchr(field,' ');
    if(delimiter) *delimiter++=0;
    *cursor=delimiter;
    return field;
}

static bool descriptor(struct kui_update_choice *c,bool stable,char **cursor) {
    const char *scope=next_field(cursor);
    if(!scope) return false;
    if(!strcmp(scope,"full")) c->scope=KUI_UPDATE_SCOPE_FULL;
    else if(!stable && !strcmp(scope,"partial")) c->scope=KUI_UPDATE_SCOPE_PARTIAL;
    else return false;
    uint32_t replaced,removed,kept,components;
    if(!decimal(next_field(cursor),KUI_UPDATE_MAX_ENTRIES,&replaced) ||
       !decimal(next_field(cursor),KUI_UPDATE_MAX_ENTRIES,&removed) ||
       !decimal(next_field(cursor),KUI_UPDATE_MAX_ENTRIES,&kept) ||
       !decimal(next_field(cursor),7,&components)) return false;
    unsigned total=replaced+removed+kept;
    if(!total || total>KUI_UPDATE_MAX_ENTRIES || (!!components != !!(replaced+removed)) ||
       (c->scope==KUI_UPDATE_SCOPE_FULL && (kept || !(components & KUI_UPDATE_COMPONENT_GUI) || !replaced))) return false;
    c->described=true;
    c->replace=(uint16_t)replaced;c->remove=(uint16_t)removed;c->keep=(uint16_t)kept;
    c->components=(uint8_t)components;
    return true;
}

static bool parse_choices(struct feed_buffer *buffer, bool stable, bool rich, struct kui_update_choices *out) {
    struct lines lines;
    if(!begin_lines(buffer,&lines)) return false;
    const char *magic=next_line(&lines);
    const char *expected=stable?"KUI-RELEASES-1":rich?"KUI-NIGHTLIES-2":"KUI-NIGHTLIES-1";
    uint32_t count;
    if(!magic || strcmp(magic,expected) || !decimal(next_line(&lines),KUI_UPDATE_NIGHTLY_LIMIT,&count) || !count)
        return false;
    for(unsigned i=0;i<count;i++) {
        char *row=next_line(&lines);
        const char *commit=next_field(&row), *size=next_field(&row), *digest=next_field(&row), *date=next_field(&row);
        struct kui_update_choice *c=&out->items[i];
        if(!set_identity(c,commit,size,digest) || !publication_time(date) ||
           (i && strcmp(date,out->items[i-1].published)>0)) return false;
        memcpy(c->published,date,sizeof(c->published));
        if(stable) {
            const char *tag=next_field(&row), *name=next_field(&row);
            if(!set_release(c,tag,name)) return false;
        }
        for(unsigned j=0;j<i;j++) {
            const struct kui_update_choice *prior=&out->items[j];
            if(stable?!strcmp(c->tag,prior->tag):!strcmp(c->commit,prior->commit)) return false;
        }
        if(rich) {
            if(!descriptor(c,stable,&row)) return false;
        } else {
            c->scope=KUI_UPDATE_SCOPE_PARTIAL;
            c->components=KUI_UPDATE_COMPONENT_GUI;
        }
        if(row) return false;
    }
    if(*lines.next) return false;
    out->count=count;
    return true;
}

const struct kui_update_choice *kui_update_choice_find(const struct kui_update_choices *list,
        const struct kui_update_choice *candidate) {
    if(!list || !candidate || list->count>KUI_UPDATE_NIGHTLY_LIMIT) return NULL;
    for(unsigned i=0;i<list->count;i++) {
        const struct kui_update_choice *p=&list->items[i];
        if(p->size==candidate->size && p->scope==candidate->scope && p->described==candidate->described &&
           p->replace==candidate->replace && p->remove==candidate->remove && p->keep==candidate->keep &&
           p->components==candidate->components && !memcmp(p->tag,candidate->tag,sizeof(p->tag)) &&
           !memcmp(p->asset,candidate->asset,sizeof(p->asset)) && !memcmp(p->commit,candidate->commit,sizeof(p->commit)) &&
           !memcmp(p->digest,candidate->digest,sizeof(p->digest)) && !memcmp(p->published,candidate->published,sizeof(p->published))) return p;
    }
    return NULL;
}

/* Called after the complete KUP and its member bytes have been inspected.
 * A partial can target only modules. Each module retains its own build ID. */
bool kui_update_choice_matches_package(enum kui_update_channel channel,const struct kui_update_choice *choice,
        const struct kui_update_package *package) {
    if(!choice || !package || package->version!=2 || !package->count || package->count>KUI_UPDATE_MAX_ENTRIES ||
       (channel!=KUI_UPDATE_RELEASES && channel!=KUI_UPDATE_NIGHTLIES)) return false;
    bool stable=channel==KUI_UPDATE_RELEASES, full=(package->flags & KUI_UPDATE_FULL)!=0;
    enum kui_update_scope scope=full?KUI_UPDATE_SCOPE_FULL:KUI_UPDATE_SCOPE_PARTIAL;
    if(package->channel!=(stable?0u:1u) || (stable && !full) || scope!=choice->scope) return false;
    unsigned replaced=0, removed=0, kept=0, components=0;
    bool gui=false;
    for(unsigned i=0;i<package->count;i++) {
        const struct kui_update_entry *entry=&package->entries[i];
        if(entry->policy==KUI_UPDATE_INHERIT_ROOT) { ++kept;continue; }
        if(entry->policy==KUI_UPDATE_REMOVE) ++removed;
        else if(entry->policy==KUI_UPDATE_SELF_KUI || entry->policy==KUI_UPDATE_RAW) ++replaced;
        else return false;
        if(!strcmp(entry->path,"runtime.kui")) {
            if(entry->policy!=KUI_UPDATE_SELF_KUI || strncmp(entry->build,choice->commit,12)) return false;
            components|=KUI_UPDATE_COMPONENT_GUI;gui=true;
        } else {
            size_t n=strlen(entry->path);
            bool module=(!strncmp(entry->path,"apps/games/",11) || !strncmp(entry->path,"apps/disc/",10)) &&
                n>4 && !strcmp(entry->path+n-4,".kui");
            components|=module?KUI_UPDATE_COMPONENT_READERS:KUI_UPDATE_COMPONENT_OTHER;
        }
    }
    if(full && !gui) return false;
    if(!choice->described) return stable || (gui && package->count==1);
    return replaced==choice->replace && removed==choice->remove && kept==choice->keep && components==choice->components;
}

#define CATALOG_BYTES 2700u
bool kui_update_online_list(enum kui_internet_route route, enum kui_update_channel channel, struct kui_update_choices *choices,
        struct kui_app_status *status, kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress) {
    if(!choices || !status) return false;
    memset(choices,0,sizeof(*choices));
    memset(status,0,sizeof(*status));
    struct job job={.status=status,.progress=progress,.cancel=cancel};
    bool stable=channel==KUI_UPDATE_RELEASES;
    char problem[128]={0};
    message(&job,stable?"Checking available GitHub releases":"Checking available GitHub nightlies");
    struct kui_internet *session=NULL;
    char *data=NULL;
    bool ok=false;
    if(!stable && channel!=KUI_UPDATE_NIGHTLIES) message(&job,"Invalid official update source");
    else if(!(data=malloc(CATALOG_BYTES+1))) message(&job,"Not enough memory for the update list");
    else if(!(session=kui_internet_open(route,log,cancel,problem))) message(&job,problem[0]?problem:"Cannot open network connection");
    else {
        struct feed_buffer buf={data,CATALOG_BYTES,0};
        const char *url=stable?STABLE_BASE "releases.txt":NIGHTLY_BASE "nightlies-v2.txt";
        enum kui_http_result result=kui_internet_get(session,url,buf.cap,collect,&buf,NULL,NULL);
        if(result==KUI_HTTP_OK) parse_choices(&buf,stable,true,choices);
        if(result==KUI_HTTP_MISSING && !stable) {
            buf.used=0;
            result=kui_internet_get(session,NIGHTLY_BASE "nightlies.txt",buf.cap,collect,&buf,NULL,NULL);
            if(result==KUI_HTTP_OK) parse_choices(&buf,false,false,choices);
        }
        if(result==KUI_HTTP_MISSING) {
            buf.used=0;
            result=kui_internet_get(session,stable?STABLE_BASE "release.txt":NIGHTLY_BASE "nightly.txt",buf.cap,collect,&buf,NULL,NULL);
            if(result==KUI_HTTP_OK && parse_manifest(&buf,stable,&choices->items[0])) choices->count=1;
        }
        ok=choices->count || result==KUI_HTTP_MISSING;
        message(&job,choices->count?"Choose a build to check.":result==KUI_HTTP_MISSING?
            (stable?"No compatible stable update published yet.":"No nightly builds published yet."):
            result==KUI_HTTP_OK?"Invalid update list from GitHub.":kui_internet_problem(session));
    }
    if(session) kui_internet_close(session);
    free(data);
    status->stopped=cancel && cancel();
    status->passed=ok && !status->stopped;
    status->complete=true;
    status->errors=!ok && !status->stopped;
    if(!status->passed) memset(choices,0,sizeof(*choices));
    if(status->stopped) message(&job,"Update list stopped");
    if(progress) progress(status);
    return status->passed;
}

static bool parse_news(struct feed_buffer *buffer, const struct kui_nightly_asset *asset, const char *running,
                       struct kui_update_online_info *out) {
    if(memchr(buffer->data,0,buffer->used)) return false;
    buffer->data[buffer->used]=0;
    struct lines lines={buffer->data};
    const char *magic=next_line(&lines), *commit=next_line(&lines), *digest=next_line(&lines);
    uint32_t count;
    if(!magic || strcmp(magic,"KUI-UPDATE-INFO-1") || !commit || strcmp(commit,asset->commit) ||
       !digest || strcmp(digest,asset->digest) || !decimal(next_line(&lines),64,&count)) return false;
    struct kui_update_online_info parsed={0};
    bool comparable=running && strlen(running)==12 && hex_field(running,12) && strncmp(commit,running,12);
    for(unsigned i=0;i<count;i++) {
        const char *ancestor=next_line(&lines);
        if(!ancestor || strlen(ancestor)!=12 || !hex_field(ancestor,12)) return false;
        if(comparable && !strcmp(ancestor,running)) parsed.newer=true;
    }
    if(!decimal(next_line(&lines),4,&count)) return false;
    for(unsigned i=0;i<count;i++) {
        const char *note=next_line(&lines);
        if(!note || !*note || strlen(note)>76) return false;
        for(const char *p=note;*p;p++) if((unsigned char)*p<32 || (unsigned char)*p>126) return false;
        strcpy(parsed.notes[i],note);
    }
    if(*lines.next) return false;
    parsed.note_count=count;
    *out=parsed;
    return true;
}

static bool ensure_directory(const char *path) {
    FILINFO info;
    FRESULT fr = f_stat(path, &info);
    if(fr == FR_OK) return (info.fattrib & AM_DIR) != 0;
    return fr == FR_NO_FILE && f_mkdir(path) == FR_OK;
}

static void verified_bytes(void *context, uint32_t bytes) {
    struct job *job = context;
    job->status->done = bytes;
    if(!bytes) message(job, "Verifying update on storage");
    else if(job->progress) job->progress(job->status);
}

static bool write_file(void *context, const void *data, size_t size) {
    FIL *file = context;
    FSIZE_t before = f_tell(file);
    UINT written;
    if(f_write(file, data, (UINT)size, &written) != FR_OK || written != size) return false;
    /* Keep long modem transfers resumable after a reset. Rehashing the full
     * file at the end also covers the persisted prefix. */
    const unsigned checkpoint = 128u * 1024u;
    return before / checkpoint == f_tell(file) / checkpoint || f_sync(file) == FR_OK;
}

static void received(void *context, uint32_t bytes, uint32_t total) {
    (void)total;
    struct job *job = context;
    job->status->done = job->offset + bytes;
    if(job->progress) job->progress(job->status);
}

static bool check_room(uint32_t remaining) {
    DWORD clusters;
    FATFS *fs;
    return !remaining || (f_getfree("0:", &clusters, &fs) == FR_OK && fs && fs->csize &&
        (uint64_t)clusters * fs->csize * 512u >= remaining);
}

/* The private partial belongs to the manifest recorded by prepare_partial.
 * Its length is the resume offset. Re-read all bytes before publication. */
static bool download(struct kui_internet *session, const struct kui_nightly_asset *asset,
                     const char *part, const char *url, struct job *job) {
    FIL file;
    if(f_open(&file, part, FA_READ | FA_WRITE | FA_OPEN_ALWAYS) != FR_OK) {
        message(job, "Cannot open update download on storage");
        return false;
    }
    FSIZE_t length = f_size(&file);
    bool ok = f_lseek(&file, length <= asset->size ? length : 0) == FR_OK;
    if(ok && length > asset->size) ok = f_truncate(&file) == FR_OK;
    if(!ok) message(job, "Cannot prepare partial update on storage");
    while(ok && f_tell(&file) < asset->size) {
        job->offset = (uint32_t)f_tell(&file);
        if(!check_room(asset->size - job->offset)) {
            message(job, "Cannot confirm enough free space for update");
            ok = false;
            break;
        }
        job->status->done = job->offset;
        message(job, job->offset ? "Resuming update download" : "Downloading update");
        enum kui_http_result r = kui_internet_get_remainder(session, url, job->offset, asset->size,
            write_file, &file, received, job);
        if(r == KUI_HTTP_RANGE_IGNORED && job->offset) {
            ok = f_lseek(&file, 0) == FR_OK && f_truncate(&file) == FR_OK;
            if(!ok) message(job, "Cannot restart partial download on storage");
            continue;
        }
        ok = r == KUI_HTTP_OK && f_tell(&file) == asset->size;
        if(!ok) {
            const char *problem = kui_internet_problem(session);
            message(job, r == KUI_HTTP_STOPPED ? "Download stopped. Partial retained." :
                *problem ? problem : "Update download incomplete. Retry to resume.");
        }
        break;
    }
    if(f_close(&file) != FR_OK) {
        message(job, "Could not finish writing partial update to storage");
        return false;
    }
    return ok;
}

bool kui_update_online_download(enum kui_internet_route route, enum kui_update_channel channel,
        const struct kui_update_choice *choice, const char *running_build, bool running_official_nightly, unsigned keep,
        char path[KUI_UPDATE_BROWSER_PATH_BYTES], struct kui_update_online_info *info, struct kui_app_status *status,
        kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress) {
    if(!path || !status) return false;
    *path = 0;
    if(info) memset(info, 0, sizeof(*info));
    bool stable = channel==KUI_UPDATE_RELEASES, test_feed=channel==KUI_UPDATE_HARDWARE;
    memset(status, 0, sizeof(*status));
    struct job job = { .status = status, .progress = progress, .cancel = cancel };
    bool ok = false, tracked = true, published = false;
    char problem[128] = "";
    struct kui_internet *session = NULL;
    if((unsigned)channel>KUI_UPDATE_HARDWARE || !kui_update_keep_valid(keep)) {
        message(&job, "Invalid update source or retention setting");
        goto done;
    }
    if(test_feed && !kui_update_test_feed_available()) {
        message(&job, "Hardware test feed unavailable in this build");
        goto done;
    }
    const char *manifest_url = test_feed ? REVIEW_BASE "nightly.txt" : stable ? STABLE_BASE "release.txt" : NIGHTLY_BASE "nightly.txt";
    message(&job, "Checking the selected GitHub update channel");
    session = kui_internet_open(route, log, cancel, problem);
    if(!session) {
        message(&job, problem[0] ? problem : "Cannot open network connection");
        goto done;
    }
    char manifest[272];
    struct feed_buffer buffer = {manifest,sizeof(manifest)-1,0};
    char release_url[sizeof(ASSET_BASE)+104];
    struct kui_nightly_asset asset={0};
    struct kui_update_choice legacy={0};
    const bool selected=choice!=NULL;
    enum kui_http_result r;
    if(!selected) {
        r=kui_internet_get(session,manifest_url,buffer.cap,collect,&buffer,NULL,NULL);
        if(r!=KUI_HTTP_OK) {
            if(stable && r==KUI_HTTP_MISSING) {
                ok=true;
                message(&job,"No compatible stable update published yet.");
            } else message(&job,r==KUI_HTTP_MISSING?"No nightly channel published yet":kui_internet_problem(session));
            goto done;
        }
        if(!parse_manifest(&buffer,stable,&legacy)) {
            message(&job,"GitHub returned an invalid update manifest");
            goto done;
        }
        choice=&legacy;
    }
    if((selected && test_feed) || choice->commit[40] || !hex_field(choice->commit,40) ||
       choice->digest[64] || !hex_field(choice->digest,64) ||
       choice->size<KUI_UPDATE_HEADER_BYTES || choice->size>FILE_LIMIT ||
       (stable && (choice->tag[34] || choice->asset[63] || !stable_tag(choice->tag) || !asset_name(choice->asset))) ||
       (!stable && (choice->tag[0] || choice->asset[0]))) {
        message(&job,"Invalid selected package");
        goto done;
    }
    memcpy(asset.commit,choice->commit,sizeof(asset.commit));
    memcpy(asset.digest,choice->digest,sizeof(asset.digest));
    asset.size=choice->size;
    asset.release=stable;
    if(stable) {
        kui_release_cache_key(&asset,choice->tag);
        snprintf(release_url,sizeof(release_url),ASSET_BASE "%s/%s",choice->tag,choice->asset);
    } else snprintf(release_url,sizeof(release_url),ASSET_BASE "nightly-%s/K-UI.kup",asset.commit);
    if(!selected && channel==KUI_UPDATE_NIGHTLIES && running_official_nightly && running_build &&
       strlen(running_build)==12 && !memcmp(running_build,asset.commit,12)) {
        ok=true;
        goto trim;
    }
    if(!ensure_directory("0:/KUI") || !ensure_directory(KUI_UPDATE_BROWSER_ROOT) ||
       !ensure_directory(KUI_NIGHTLY_DIRECTORY) || !ensure_directory(KUI_NIGHTLY_STATE)) {
        message(&job, "Cannot create update download folders on storage");
        goto done;
    }
    status->total = asset.size;
    snprintf(status->lines[0], sizeof(status->lines[0]), "GitHub %.12s   %lu bytes",
        asset.commit, (unsigned long)asset.size);
    status->line_count = 1;
    char final[KUI_UPDATE_BROWSER_PATH_BYTES];
    snprintf(final, sizeof(final), stable ? KUI_RELEASE_PACKAGE : KUI_NIGHTLY_PACKAGE, kui_nightly_cache_key(&asset));
    enum kui_nightly_verify existing = kui_nightly_verify_file(final, &asset, cancel, verified_bytes, &job);
    if(existing == KUI_NIGHTLY_MISMATCH) {
        message(&job, "A different file occupies the update path. Move it before retrying.");
        goto done;
    }
    if(existing == KUI_NIGHTLY_MISSING) {
        if(!kui_nightly_prepare_partial(&asset)) {
            message(&job, "Cannot prepare the update resume slot on storage");
            goto done;
        }
        const char *url = test_feed ? REVIEW_BASE "K-UI.kup" : release_url;
        if(!download(session, &asset, KUI_NIGHTLY_PART, url, &job)) goto done;
        enum kui_nightly_verify checked = kui_nightly_verify_file(KUI_NIGHTLY_PART, &asset, cancel, verified_bytes, &job);
        if(checked != KUI_NIGHTLY_VERIFIED) {
            if(checked == KUI_NIGHTLY_MISMATCH) {
                if(f_unlink(KUI_NIGHTLY_PART) == FR_OK) message(&job, "Update SHA-256 or size mismatch. Retry to download again.");
                else message(&job, "Update failed verification. Cannot remove damaged partial file.");
            } else message(&job, "Could not verify update on storage");
            goto done;
        }
        if(cancel && cancel()) goto done;
        if(f_rename(KUI_NIGHTLY_PART, final) != FR_OK) {
            message(&job, "Cannot publish verified update on storage");
            goto done;
        }
        published = true;
        /* A reset in this window can leave an untracked verified package.
         * Never pre-claim a destination that publication might fail to fill. */
        tracked = kui_nightly_record_download(&asset, cancel);
    } else if(existing != KUI_NIGHTLY_VERIFIED) {
        message(&job, "Cannot verify existing update package");
        goto done;
    }
    ok = true;
    strcpy(path, final);
    if(info && !test_feed) {
        /* Optional on old feeds. Never attach notes from another publication. */
        char *news=malloc(1351);
        if(news) {
            struct feed_buffer detail={news,1350,0};
            char history_url[sizeof(NIGHTLY_BASE)+70];
            const char *news_url=stable ? STABLE_BASE "update-info.txt" : NIGHTLY_BASE "update-info.txt";
            if(selected && choice->published[0]) {
                snprintf(history_url,sizeof(history_url),"%shistory/%s/update-info.txt",
                         stable?STABLE_BASE:NIGHTLY_BASE,stable?choice->tag:asset.commit);
                news_url=history_url;
            }
            r=kui_internet_get(session,news_url,
                               detail.cap,collect,&detail,NULL,NULL);
            if(r==KUI_HTTP_OK) parse_news(&detail,&asset,running_build,info);
            free(news);
            if(r==KUI_HTTP_STOPPED || (cancel && cancel())) { ok=false; *path=0; goto done; }
        }
        memcpy(info->source_build,asset.commit,12);
        info->source_build[12]=0;
    }
trim:
    kui_internet_close(session);
    session = NULL;
    bool wrote = false;
    bool clean = !tracked || kui_online_trim_report(keep, kui_nightly_cache_key(&asset), stable, cancel, &wrote);
    /* A successful rename, receipt write or prune can disturb sectors verified
     * earlier. Reopen the public path after all writes before offering it. */
    if(*path && (published || wrote) &&
       kui_nightly_verify_file(path, &asset, cancel, verified_bytes, &job) != KUI_NIGHTLY_VERIFIED) {
        *path = 0;
        ok = false;
        message(&job, "Final update on storage failed verification. Check the card before retrying.");
    } else if(!clean) {
        message(&job, *path ? "Update ready. Download cleanup incomplete. Retry the check." :
            "Already running this nightly GUI. Download cleanup incomplete. Retry the check.");
    } else if(!*path) {
        message(&job, "Already running this GitHub nightly GUI");
    } else {
        message(&job, tracked ? (stable ? "Verified stable release ready for review" : "Verified nightly ready for review") :
            "Verified update ready. Could not record it for automatic cleanup.");
    }
    if(cancel && cancel()) {
        ok = false;
        *path = 0;
    }
done:
    if(session) kui_internet_close(session);
    status->complete = true;
    status->passed = ok;
    status->stopped = !ok && cancel && cancel();
    status->errors = !ok && !status->stopped;
    if(status->stopped) snprintf(status->message, sizeof(status->message), "Online update stopped");
    if(progress) progress(status);
    if(log) log("GitHub update: %s", status->message);
    return ok;
}

bool kui_update_nightly_download(enum kui_internet_route route, bool test_feed, const char *running_build, unsigned keep,
        char path[KUI_UPDATE_BROWSER_PATH_BYTES], struct kui_app_status *status,
        kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress) {
    return kui_update_online_download(route, test_feed ? KUI_UPDATE_HARDWARE : KUI_UPDATE_NIGHTLIES,
        NULL,running_build,true,keep,path,NULL,status,log,cancel,progress);
}
