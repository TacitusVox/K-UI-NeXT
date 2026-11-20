/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_download.h"
#include "kui/media_container.h"
#include "kui/artwork_validators.h"
#include "kui/internet_cover.h"
#include "kui/internet_extra.h"
#include "kui/internet_preview.h"
#include "kui/preview_prefix.h"
#include "kui/compact_cover.h"
#include "kui/game_cover.h"
#include "kui/game_image.h"
#include "kui/games_library.h"
#include "kui/games_media_storage.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEDIA_DIR "0:/KUI/media"
/* Owned exclusively by the Games storage worker. A retry reclaims its staging files. */
#define MEDIA_TEMP MEDIA_DIR "/fetch.tmp"
#define VIDEO_TEMP MEDIA_DIR "/video.tmp"
#define SMALL_COVER_MAX (128u * 1024u)
#define SOURCE_VIDEO_MAX (32u * 1024u * 1024u)

static bool stopped(kui_cancel_fn cancel) { return cancel && cancel(); }
const char *kui_artwork_batch_checked(const struct kui_media_game *game,const char *release) {
    const struct kui_artwork_batch *batch = game->artwork_batch;
    if(!batch || !release || !*release || !game->entry->media_disc1_key[0] ||
       strcmp(batch->group,game->entry->media_disc1_key)) return NULL;
    for(unsigned i=0;i<batch->count;i++)
        if(!strcmp(release,batch->checked[i].release)) return batch->checked[i].master;
    return NULL;
}
void kui_artwork_batch_remember(const struct kui_media_game *game,const char *release,const char *master) {
    struct kui_artwork_batch *batch = game->artwork_batch;
    if(!batch || !release || !*release || !master || !*master || !game->entry->media_disc1_key[0] ||
       strlen(master) >= sizeof(batch->checked[0].master)) return;
    const char *group = game->entry->media_disc1_key;
    if(strcmp(batch->group,group)) {
        strcpy(batch->group,group);
        batch->count = 0;
    }
    for(unsigned i=0;i<batch->count;i++)
        if(!strcmp(batch->checked[i].release,release)) {
            if(batch->checked[i].master != master) strcpy(batch->checked[i].master,master);
            return;
        }
    if(batch->count == sizeof(batch->checked)/sizeof(batch->checked[0])) return;
    batch->checked[batch->count].release = release;
    strcpy(batch->checked[batch->count++].master,master);
}
static bool media_path(const struct kui_games_entry *entry, const char *folder, const char *ext,
        char out[KUI_GAMES_FILE_CAP]) {
    return entry && out && memchr(entry->name, 0, sizeof(entry->name)) &&
        memchr(entry->path, 0, sizeof(entry->path)) &&
        kui_games_media_path(entry->path, folder, ext, out, KUI_GAMES_FILE_CAP);
}
static const char *const media_extensions[] = {"kct", "kdm", "kpv"};
static bool set_media_path(const struct kui_games_entry *entry, const char *ext,
        char out[KUI_GAMES_FILE_CAP]) {
    if(!entry || !out) return false;
    const char *key = entry->media_disc1_key;
    if(!key[0]) return media_path(entry, MEDIA_DIR, ext, out);
    if(key[32] || strspn(key,"0123456789abcdef") != 32) return false;
    int n = snprintf(out,KUI_GAMES_FILE_CAP,"%s/%s.%s",MEDIA_DIR,key,ext);
    return n > 0 && n < (int)KUI_GAMES_FILE_CAP;
}
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    if(kind == KUI_MEDIA_COVER) return media_path(entry,KUI_DOWNLOADED_COVERS,"kct",out);
    return (unsigned)kind <= KUI_MEDIA_PREVIEW && set_media_path(entry,media_extensions[kind],out);
}
bool kui_media_local_fallback(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    if(!entry || !entry->media_disc1_key[0] || (kind != KUI_MEDIA_MANUAL && kind != KUI_MEDIA_PREVIEW))
        return false;
    char local[33];
    if(!kui_games_media_key(entry->path,local) || !strcmp(local,entry->media_disc1_key)) return false;
    int n = snprintf(out,KUI_GAMES_FILE_CAP,"%s/%s.%s",MEDIA_DIR,local,media_extensions[kind]);
    return n > 0 && n < (int)KUI_GAMES_FILE_CAP;
}
bool kui_media_original_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    return kind == KUI_MEDIA_MANUAL && set_media_path(entry, "pdf", out);
}
/* FatFs cannot replace a destination atomically. Keep older locally authored
 * or downloaded clips playable throughout a refresh, including a power cut. */
static bool upgraded_preview_path(const char *path, char out[KUI_GAMES_FILE_CAP]) {
    size_t len = strlen(path);
    if(len < 4 || len >= KUI_GAMES_FILE_CAP || strcmp(path+len-4, ".kpv")) return false;
    memcpy(out, path, len+1);
    out[len-1] = '2';
    return true;
}
FRESULT kui_media_open_preview(FIL *file, const char *path) {
    char upgraded[KUI_GAMES_FILE_CAP];
    if(upgraded_preview_path(path, upgraded)) {
        FRESULT r = f_open(file, upgraded, FA_READ);
        if(r != FR_NO_FILE && r != FR_NO_PATH) return r;
    }
    return f_open(file, path, FA_READ);
}
static bool ensure_dir(const char *path) {
    FILINFO info;
    FRESULT r = f_stat(path, &info);
    if(r == FR_OK) return (info.fattrib & AM_DIR) != 0;
    return (r == FR_NO_FILE || r == FR_NO_PATH) && f_mkdir(path) == FR_OK;
}
static bool valid_pdf(FIL *file) {
    uint8_t buf[128];
    FSIZE_t size = f_size(file);
    UINT got = 0;
    if(size < 14 || size > KUI_MANUAL_BYTES_MAX || f_lseek(file, 0) != FR_OK ||
       f_read(file, buf, 8, &got) != FR_OK || got != 8 || memcmp(buf,"%PDF-",5) ||
       buf[5] < '1' || buf[5] > '2' || buf[6] != '.' || buf[7] < '0' || buf[7] > '9')
        return false;
    /* A PDF trailer ends in %%EOF, optionally followed by PDF whitespace. */
    UINT n = size < sizeof(buf) ? (UINT)size : (UINT)sizeof(buf);
    if(f_lseek(file, size - n) != FR_OK || f_read(file, buf, n, &got) != FR_OK || got != n) return false;
    while(n && (buf[n-1] == 0 || buf[n-1] == 9 || buf[n-1] == 10 || buf[n-1] == 12 ||
                buf[n-1] == 13 || buf[n-1] == 32)) --n;
    return n >= 5 && !memcmp(buf+n-5,"%%EOF",5);
}

static bool write_data(void *ctx, const void *data, size_t size) {
    FIL *file = ctx;
    UINT n = (UINT)size, actual = 0;
    return n == size && f_write(file,data,n,&actual) == FR_OK && actual == n;
}
static enum kui_media_result media_result(enum kui_http_result r,enum kui_media_reason *reason) {
    *reason=KUI_MEDIA_REASON_NONE;
    switch(r) {
    case KUI_HTTP_OK:        return KUI_MEDIA_INSTALLED;
    case KUI_HTTP_LIMITED:   return KUI_MEDIA_LIMITED;
    case KUI_HTTP_TOO_LARGE: *reason=KUI_MEDIA_REASON_TOO_LARGE; return KUI_MEDIA_MISSING;
    case KUI_HTTP_MISSING:   *reason=KUI_MEDIA_REASON_PROVIDER_MISSING; return KUI_MEDIA_MISSING;
    case KUI_HTTP_CONNECT_FAILED: return KUI_MEDIA_CONNECT_FAILED;
    case KUI_HTTP_STOPPED:   return KUI_MEDIA_STOPPED;
    default:                 return KUI_MEDIA_ERROR;
    }
}
static enum kui_media_result cached(const char *path, enum kui_media_kind kind, bool pdf,
        unsigned *version) {
    FIL file;
    FRESULT r = f_open(&file, path, FA_READ);
    if(r == FR_NO_FILE || r == FR_NO_PATH) return KUI_MEDIA_MISSING;
    if(r != FR_OK) return KUI_MEDIA_ERROR;
    bool valid;
    if(pdf) valid = valid_pdf(&file);
    else {
        struct kui_media_index index;
        valid = kui_media_container_index(&file, kind == KUI_MEDIA_MANUAL, &index);
        if(valid && version) *version = index.version;
    }
    if(f_close(&file) != FR_OK) valid = false;
    return valid ? KUI_MEDIA_EXISTING : KUI_MEDIA_ERROR;
}

/* Explicit scans can adopt a valid pre-existing Disc 2/3 cache with a metadata
 * rename on the same card. Viewers can still use its original path until then. */
static enum kui_media_result cached_for_set(const struct kui_games_entry *entry, const char *path,
        enum kui_media_kind kind, bool pdf, unsigned *version) {
    enum kui_media_result result = cached(path,kind,pdf,version);
    if(result != KUI_MEDIA_MISSING || !entry->media_disc1_key[0]) return result;
    const char *suffix = strrchr(path,'.');
    if(!suffix) return KUI_MEDIA_ERROR;
    char member[KUI_GAMES_FILE_CAP], local[KUI_GAMES_FILE_CAP];
    for(unsigned i=0;kui_games_library_media_member(entry,i,member);++i) {
        if(!kui_games_media_path(member,MEDIA_DIR,suffix+1,local,sizeof(local))) return KUI_MEDIA_ERROR;
        if(!strcmp(local,path)) continue;
        result = cached(local,kind,pdf,version);
        /* An old preview stays at its original selector until its replacement
         * is durable, even when the first scan was started on another disc. */
        if(result == KUI_MEDIA_EXISTING && !(kind==KUI_MEDIA_PREVIEW && version && *version==1) &&
           f_rename(local,path) != FR_OK) return KUI_MEDIA_ERROR;
        if(result != KUI_MEDIA_MISSING) return result;
    }
    return KUI_MEDIA_MISSING;
}

static bool complete_preview(const char *path,const uint32_t *expected,kui_cancel_fn cancel) {
    FIL file;
    if(stopped(cancel) || f_open(&file,path,FA_READ)!=FR_OK) return false;
    struct kui_media_index index;
    uint8_t header[KUI_PREVIEW_HEADER_BYTES];
    UINT got=0;
    bool ok=kui_media_container_index(&file,false,&index) && index.version==2 &&
        f_lseek(&file,0)==FR_OK && f_read(&file,header,sizeof(header),&got)==FR_OK && got==sizeof(header);
    /* The validated header gives the exact frame and soundtrack lengths.
     * Read every byte through FatFs before retiring the only older copy. */
    uint8_t block[4096];
    uint32_t crc=0;
    while(ok && f_tell(&file)<f_size(&file)) {
        FSIZE_t left=f_size(&file)-f_tell(&file);
        UINT n=left<sizeof(block)?(UINT)left:(UINT)sizeof(block);
        ok=!stopped(cancel) && f_read(&file,block,n,&got)==FR_OK && got==n;
        if(ok && expected) crc=kui_crc32_capture(crc,block,n);
    }
    if(ok && expected) ok=kui_crc32_capture(crc,header,sizeof(header))==*expected;
    return f_close(&file)==FR_OK && ok && !stopped(cancel);
}

static bool retire_preview(const char *legacy,const char *active,bool *checked,unsigned *removed,kui_cancel_fn cancel) {
    if(!strcmp(legacy,active)) return true;
    FIL file;
    FRESULT r=f_open(&file,legacy,FA_READ);
    if(r==FR_NO_FILE || r==FR_NO_PATH) return true;
    if(r!=FR_OK) return false;
    struct kui_media_index index;
    bool old=kui_media_container_index(&file,false,&index) && index.version==1;
    bool readable=!f_error(&file);
    if(f_close(&file)!=FR_OK || !readable) return false;
    if(!old) return true; /* Keep other formats and independently authored current clips. */
    if(!*checked) *checked=complete_preview(active,NULL,cancel);
    if(!*checked || stopped(cancel)) return false;
    /* Both paths are independent files. The published KPV2 never moves;
     * a stopped or failed unlink can be retried offline on the next scan. */
    if(f_unlink(legacy)!=FR_OK) return false;
    ++*removed;
    return true;
}

static enum kui_media_result finish_preview(struct kui_media_game *game,const char *active,
        enum kui_media_result result,bool checked,kui_cancel_fn cancel) {
    const struct kui_games_entry *entry=game->entry;
    char primary[KUI_GAMES_FILE_CAP],member[KUI_GAMES_FILE_CAP],local[KUI_GAMES_FILE_CAP];
    if(!kui_media_file_path(entry,KUI_MEDIA_PREVIEW,primary)) return KUI_MEDIA_ERROR;
    bool ok=retire_preview(primary,active,&checked,&game->previews_removed,cancel);
    for(unsigned i=0;ok && entry->media_disc1_key[0] && kui_games_library_media_member(entry,i,member);++i) {
        if(!kui_games_media_path(member,MEDIA_DIR,"kpv",local,sizeof(local))) return KUI_MEDIA_ERROR;
        if(strcmp(local,primary)) ok=retire_preview(local,active,&checked,&game->previews_removed,cancel);
    }
    if(!ok) {
        game->preview_cleanup_pending=true;
        if(stopped(cancel)) return KUI_MEDIA_STOPPED;
        if(!checked) return KUI_MEDIA_ERROR;
    }
    return result;
}

struct transfer {
    struct kui_internet *session;
    const struct kui_games_disc_identity *id;
    enum kui_media_reason reason;
    kui_media_progress_fn progress;
    void *ctx;
    kui_cancel_fn cancel;
    kui_log_fn log;
    char url[768];
};
static void received(void *ctx,uint32_t bytes,uint32_t total) {
    struct transfer *t = ctx;
    if(t->progress) t->progress(t->ctx,KUI_MEDIA_RECEIVE,bytes,total);
}
static enum kui_media_result get(struct transfer *t,uint32_t max,FIL *file) {
    return media_result(kui_internet_get(t->session,t->url,max,write_data,file,received,t),&t->reason);
}
static enum kui_media_result unavailable(struct transfer *t,enum kui_media_reason reason) {
    t->reason=reason;
    return KUI_MEDIA_MISSING;
}
struct small_image {
    FIL *file;
    uint32_t bytes, crc;
};
static bool write_small_image(void *ctx,const void *data,size_t size) {
    struct small_image *image = ctx;
    if(size > SMALL_COVER_MAX-image->bytes || !write_data(image->file,data,size)) return false;
    image->bytes += (uint32_t)size;
    image->crc = kui_crc32_capture(image->crc,data,size);
    return true;
}
static enum kui_media_result get_cover(struct transfer *t,FIL *file,bool small,
        const struct kui_http_validators *known,struct kui_http_validators *response) {
    struct small_image image={.file=file};
    enum kui_http_result r=kui_internet_get_conditional(t->session,t->url,
        small?SMALL_COVER_MAX:KUI_INTERNET_COVER_MAX_BYTES,small?write_small_image:write_data,
        small?(void *)&image:(void *)file,received,t,known,response);
    if(r==KUI_HTTP_NOT_MODIFIED) return known?KUI_MEDIA_EXISTING:KUI_MEDIA_ERROR;
    if(r==KUI_HTTP_OK && small && !kui_internet_cover_small_matches(t->id,image.bytes,image.crc))
        return unavailable(t,KUI_MEDIA_REASON_INVALID_ASSET);
    return media_result(r,&t->reason);
}
static void le32(uint8_t *dst, uint32_t value) {
    for(unsigned i = 0; i < 4; ++i) dst[i] = (uint8_t)(value >> (8*i));
}
static enum kui_media_result fetch_pages(struct transfer *t, FIL *file, unsigned count) {
    if(!count || count > KUI_MANUAL_PAGES_MAX) return unavailable(t,KUI_MEDIA_REASON_TOO_LARGE);
    /* Save a contiguous JPEG per page and a small seek table, exactly the format
     * used by the local manual viewer. No page bitmap is needed during a scan. */
    uint8_t header[8 + 4 * (KUI_MANUAL_PAGES_MAX + 1)] = {'K','D','M','1'};
    header[4] = (uint8_t)count;
    header[5] = (uint8_t)(count >> 8);
    size_t header_bytes = 8 + 4 * (count + 1);
    if(!write_data(file,header,header_bytes)) return KUI_MEDIA_ERROR;
    for(unsigned i = 0; i < count; ++i) {
        if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
        if(!kui_internet_manual_page_url(t->id,i,t->url,sizeof(t->url))) return unavailable(t,KUI_MEDIA_REASON_NOT_LISTED);
        uint32_t begin = (uint32_t)f_tell(file);
        le32(header+8+i*4,begin);
        uint32_t limit = KUI_MANUAL_BYTES_MAX-begin;
        if(limit > KUI_MANUAL_PAGE_BYTES_MAX) limit = KUI_MANUAL_PAGE_BYTES_MAX;
        if(!limit) return unavailable(t,KUI_MEDIA_REASON_TOO_LARGE);
        if(t->log) t->log("Manual: fetching page %u of %u",i+1,count);
        if(t->progress) t->progress(t->ctx,KUI_MEDIA_MANUAL_PAGE,i+1,count);
        enum kui_media_result result = get(t,limit,file);
        if(result != KUI_MEDIA_INSTALLED) return result;
        if(f_tell(file) <= begin || f_tell(file)-begin > limit) return KUI_MEDIA_ERROR;
    }
    le32(header+8+count*4,(uint32_t)f_tell(file));
    if(f_lseek(file,0) != FR_OK || !write_data(file,header,header_bytes)) return KUI_MEDIA_ERROR;
    return kui_manual_check_pages(file,t->cancel) ? KUI_MEDIA_INSTALLED :
           stopped(t->cancel) ? KUI_MEDIA_STOPPED : unavailable(t,KUI_MEDIA_REASON_INVALID_ASSET);
}

struct preview_sink {
    FIL *file;
    struct kui_preview_prefix track;
};
static bool write_video(void *ctx,const void *data,size_t size) {
    struct preview_sink *sink = ctx;
    size_t needed = kui_preview_prefix_consume(&sink->track,data,size);
    return needed <= size && write_data(sink->file,data,needed);
}
static bool preview_complete(void *ctx) {
    struct preview_sink *sink = ctx;
    return sink->track.complete;
}
static enum kui_media_result fetch_preview(struct transfer *t, FIL *output, unsigned start, uint32_t *readback_crc) {
    FIL source;
    if(f_open(&source,VIDEO_TEMP,FA_READ|FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) return KUI_MEDIA_ERROR;
    struct preview_sink sink = {.file=&source, .track.start_seconds=start};
    enum kui_media_result result = media_result(kui_internet_get_prefix(t->session,t->url,SOURCE_VIDEO_MAX,
        write_video,&sink,received,t,preview_complete),&t->reason);
    if(result == KUI_MEDIA_INSTALLED) {
        if(t->progress) t->progress(t->ctx,KUI_MEDIA_CONVERTING,0,0);
        if(f_lseek(&source,0) != FR_OK) result = KUI_MEDIA_ERROR;
        else if(!kui_internet_preview_convert(&source,output,start,t->cancel,t->log,readback_crc)) result = KUI_MEDIA_ERROR;
        else if(!kui_media_container_check(output,false)) result = KUI_MEDIA_ERROR;
    }
    if(f_close(&source) != FR_OK) result = KUI_MEDIA_ERROR;
    if(f_unlink(VIDEO_TEMP) != FR_OK) result = KUI_MEDIA_ERROR;
    return result;
}

/* A grouped selector always keeps its local cover name. Its compact pixels
 * can live under Disc 1 (including an older cached download), or in a second
 * master for an explicitly different catalogue cover within the same set. */
#define COVER_MASTER_CAP (sizeof(KUI_DOWNLOADED_COVERS) + 38u)
static bool cover_master(const struct kui_games_entry *entry,
        const struct kui_games_disc_identity *id, char master[COVER_MASTER_CAP],
        kui_log_fn log, kui_cancel_fn cancel) {
    if(!entry->media_disc1_key[0]) return true;
    char first[KUI_GAMES_FILE_CAP];
    if(!kui_games_library_media_disc1(entry,first) || !strcmp(first,entry->path)) return true;
    const char *name = kui_internet_cover_name(id);
    if(!name) return true;
    struct kui_games_disc_identity other;
    enum kui_games_identity_result result = kui_games_media_identity(first,&other,log,cancel);
    if(result == KUI_GAMES_IDENTITY_ERROR || stopped(cancel)) return false;
    const char *other_name = result == KUI_GAMES_IDENTITY_FOUND ? kui_internet_cover_name(&other) : NULL;
    if(!other_name) return true;
    if(!strcmp(name,other_name))
        return kui_games_media_path(first,KUI_DOWNLOADED_COVERS,"kct",master,COVER_MASTER_CAP);
    struct kui_sha256 hash;
    uint8_t digest[32];
    char key[33];
    static const char domain[] = "K-UI alternate front cover";
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,domain,sizeof(domain));
    kui_sha256_update(&hash,entry->media_disc1_key,32);
    kui_sha256_update(&hash,name,strlen(name));
    kui_sha256_digest(&hash,digest);
    kui_hex(digest,16,key);
    snprintf(master,COVER_MASTER_CAP,KUI_DOWNLOADED_COVERS "/%s.kct",key);
    return true;
}

static enum kui_media_result master_present(char master[COVER_MASTER_CAP], kui_cancel_fn cancel) {
    FIL file;
    char direct[COVER_MASTER_CAP];
    FRESULT r = kui_games_open_compact_target(&file,master,direct,sizeof(direct));
    if(r == FR_NO_FILE || r == FR_NO_PATH) return KUI_MEDIA_MISSING;
    if(r == FR_INVALID_OBJECT) { master[0]=0; return KUI_MEDIA_MISSING; }
    if(r != FR_OK) return KUI_MEDIA_ERROR;
    enum kui_compact_result checked = kui_compact_cover_verify(&file,cancel);
    if(f_close(&file) != FR_OK || checked == KUI_COMPACT_ERROR) return KUI_MEDIA_ERROR;
    if(checked == KUI_COMPACT_INVALID) {
        /* Keep this sibling usable even when the owner's file needs repair.
         * A later scan of that owner can rebuild it in place. */
        master[0] = 0;
        return KUI_MEDIA_MISSING;
    }
    strcpy(master,direct);
    return KUI_MEDIA_EXISTING;
}

/* First compare the header containing the CRCs of all three sizes. Only a
 * candidate with the same header warrants reading its full pixel payload. */
static bool compact_equal(FIL *staged,FIL *other,kui_cancel_fn cancel,bool *equal) {
    *equal=false;
    if(f_size(other)!=KUI_COMPACT_COVER_BYTES) return true;
    if(f_lseek(staged,0)!=FR_OK) return false;
    uint8_t left[512],right[512];
    unsigned count=KUI_COMPACT_COVER_HEADER;
    for(unsigned offset=0;offset<KUI_COMPACT_COVER_BYTES;offset+=count) {
        unsigned remaining=KUI_COMPACT_COVER_BYTES-offset;
        count=offset==0?KUI_COMPACT_COVER_HEADER:remaining<sizeof(left)?remaining:(unsigned)sizeof(left);
        UINT a,b;
        if(stopped(cancel) || f_read(staged,left,count,&a)!=FR_OK || a!=count ||
           f_read(other,right,count,&b)!=FR_OK || b!=count) return false;
        if(memcmp(left,right,count)) return true;
    }
    *equal=true;
    return true;
}

static bool find_existing_pixels(FIL *staged,const struct kui_games_entry *entry,
        char match[COVER_MASTER_CAP],kui_cancel_fn cancel) {
    if(!entry->media_disc1_key[0]) return true;
    char sibling[KUI_GAMES_FILE_CAP], candidate[COVER_MASTER_CAP], direct[COVER_MASTER_CAP];
    for(unsigned i=0;kui_games_library_media_member(entry,i,sibling);++i) {
        if(stopped(cancel)) return false;
        if(!strcmp(sibling,entry->path)) continue;
        if(!kui_games_media_path(sibling,KUI_DOWNLOADED_COVERS,"kct",candidate,sizeof(candidate))) return false;
        FIL file;
        FRESULT r=kui_games_open_compact_target(&file,candidate,direct,sizeof(direct));
        if(r==FR_NO_FILE || r==FR_NO_PATH || r==FR_INVALID_OBJECT) continue;
        if(r!=FR_OK) return false;
        bool equal=false;
        bool ok=compact_equal(staged,&file,cancel,&equal);
        if(f_close(&file)!=FR_OK || !ok) return false;
        if(equal) { strcpy(match,direct); break; }
    }
    return true;
}

static bool publish_cover_link(const char *master, const char *selector, kui_cancel_fn cancel) {
    const char *temp = KUI_DOWNLOADED_COVERS "/cover-ref.tmp";
    char reference[KUI_COMPACT_LINK_BYTES] = {'K','C','L','1'};
    memcpy(reference+4,master+sizeof(KUI_DOWNLOADED_COVERS),32);
    FIL file;
    if(f_open(&file,temp,FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) return false;
    bool ok = write_data(&file,reference,sizeof(reference)) && f_sync(&file) == FR_OK;
    if(f_close(&file) != FR_OK) ok = false;
    if(ok && !stopped(cancel) && kui_games_publish_cover(temp,selector,cancel)) return true;
    (void)f_unlink(temp);
    return false;
}

static bool finish_cover(const struct kui_games_entry *entry, const char *master,
        const char *selector, struct transfer *t) {
    if(master[0] && strcmp(master,selector) && !publish_cover_link(master,selector,t->cancel)) return false;
    bool present = false;
    return kui_games_prepare_media_cover(entry,&present,NULL,t->log,t->cancel) && present;
}

struct cover_update {
    enum kui_games_media_cover_state state;
    char master[COVER_MASTER_CAP], current[COVER_MASTER_CAP], reuse[COVER_MASTER_CAP];
    const char *release;
    struct kui_http_validators response,known;
    bool conditional;
};

/* An installed native file may belong to another selector that previously
 * had identical pixels. Publish a new content-addressed native file on
 * change; the selector's independent overlay can then be replaced safely. */
static bool content_master(FIL *file,char out[COVER_MASTER_CAP],kui_cancel_fn cancel) {
    if(f_lseek(file,0)!=FR_OK || f_size(file)!=KUI_COMPACT_COVER_BYTES) return false;
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    uint8_t block[1024],digest[32];
    unsigned remaining=KUI_COMPACT_COVER_BYTES;
    while(remaining) {
        UINT count=remaining<sizeof(block)?remaining:sizeof(block),got;
        if(stopped(cancel) || f_read(file,block,count,&got)!=FR_OK || got!=count) return false;
        kui_sha256_update(&hash,block,got);
        remaining-=got;
    }
    char key[33];
    kui_sha256_digest(&hash,digest);
    kui_hex(digest,16,key);
    snprintf(out,COVER_MASTER_CAP,KUI_DOWNLOADED_COVERS "/%s.kct",key);
    return true;
}

static bool compare_native(FIL *staged,const char *path,bool *equal,kui_cancel_fn cancel) {
    FIL stored;
    FRESULT r=kui_games_open_compact_direct(&stored,path);
    *equal=false;
    if(r==FR_NO_FILE || r==FR_NO_PATH) return true;
    if(r!=FR_OK) return false;
    bool ok=compact_equal(staged,&stored,cancel,equal);
    return f_close(&stored)==FR_OK && ok;
}

static bool bind_cover(struct kui_media_game *game,struct transfer *t,struct cover_update *c,
        const char *selector,const char *native) {
    if(!strcmp(c->current,native) && c->state==KUI_GAMES_MEDIA_COVER_MANAGED) return true;
    char overlay[COVER_MASTER_CAP];
    const char *binding=selector;
    if(c->state==KUI_GAMES_MEDIA_COVER_MANAGED) {
        size_t n=strlen(selector);
        if(n>=sizeof(overlay) || n<4 || strcmp(selector+n-4,".kct")) return false;
        memcpy(overlay,selector,n+1);
        overlay[n-1]='u';
        binding=overlay;
    }
    return finish_cover(game->entry,native,binding,t);
}

/* Pinned archive tiles sometimes keep the exact same URL after the compiled
 * expected image changes. Scope cached HTTP evidence to that expectation. */
static const char *cover_source(const struct transfer *t,bool small,char key[800]) {
    uint32_t bytes,crc;
    if(!small || !kui_internet_cover_small_pin(t->id,&bytes,&crc)) return t->url;
    snprintf(key,800,"%s#kui-tile-%08lx-%08lx",t->url,(unsigned long)bytes,(unsigned long)crc);
    return key;
}
static bool load_cover_validator(const struct transfer *t,bool small,struct cover_update *c) {
    char key[800];
    return kui_artwork_validators_load(c->release,small,c->current,cover_source(t,small,key),&c->known);
}

static enum kui_media_result cover_transfer(struct kui_media_game *game,struct transfer *t,
        struct cover_update *c,const char *selector,bool small) {
    FIL staged;
    if(f_open(&staged,MEDIA_TEMP,FA_READ|FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return KUI_MEDIA_ERROR;
    memset(&c->response,0,sizeof(c->response));
    enum kui_media_result result=get_cover(t,&staged,small,c->conditional?&c->known:NULL,&c->response);
    if(result==KUI_MEDIA_INSTALLED) {
        if(t->progress) t->progress(t->ctx,KUI_MEDIA_COVER_SCALING,0,0);
        if(f_lseek(&staged,0)!=FR_OK) result=KUI_MEDIA_ERROR;
        else {
            enum kui_compact_result encoded=kui_compact_cover_encode(&staged,&staged,small,t->cancel);
            if(encoded!=KUI_COMPACT_OK) result=encoded==KUI_COMPACT_INVALID?
                unavailable(t,KUI_MEDIA_REASON_INVALID_ASSET):KUI_MEDIA_ERROR;
        }
    }
    char content[COVER_MASTER_CAP]="";
    bool identical=false;
    if(result==KUI_MEDIA_INSTALLED) {
        if(f_sync(&staged)!=FR_OK) result=KUI_MEDIA_ERROR;
        else if(c->current[0] && !compare_native(&staged,c->current,&identical,t->cancel)) result=KUI_MEDIA_ERROR;
        else if(identical) result=KUI_MEDIA_EXISTING;
        else if(!find_existing_pixels(&staged,game->entry,c->reuse,t->cancel)) result=KUI_MEDIA_ERROR;
        else if(c->current[0] && !c->reuse[0]) {
            if(!content_master(&staged,content,t->cancel) ||
               !compare_native(&staged,content,&identical,t->cancel)) result=KUI_MEDIA_ERROR;
            else if(identical) strcpy(c->reuse,content);
        }
    }
    if(f_close(&staged)!=FR_OK) result=KUI_MEDIA_ERROR;
    if(stopped(t->cancel)) result=KUI_MEDIA_STOPPED;
    const char *native=c->current;
    bool promoted=false;
    if(result==KUI_MEDIA_INSTALLED) {
        if(c->reuse[0]) {
            native=c->reuse;
            /* Remember the source-specific file on first publication even
             * when an unrelated source already supplied its exact pixels. */
            if(!c->current[0] && c->master[0] && strcmp(c->master,native) &&
               !publish_cover_link(native,c->master,t->cancel)) result=KUI_MEDIA_ERROR;
        } else {
            native=content[0]?content:c->master[0]?c->master:selector;
            promoted=kui_games_publish_cover(MEDIA_TEMP,native,t->cancel);
            if(!promoted) result=KUI_MEDIA_ERROR;
        }
    }
    if(!promoted && f_unlink(MEDIA_TEMP)!=FR_OK) return KUI_MEDIA_ERROR;
    if(result!=KUI_MEDIA_INSTALLED && result!=KUI_MEDIA_EXISTING) return result;
    if(!native[0] || !bind_cover(game,t,c,selector,native)) return KUI_MEDIA_ERROR;
    char key[800];
    (void)kui_artwork_validators_save(c->release,small,native,cover_source(t,small,key),&c->response);
    if(game->artwork_mode!=KUI_ART_MISSING) kui_artwork_batch_remember(game,c->release,native);
    return result;
}

static enum kui_media_result fetch_cover(struct kui_media_game *game,struct transfer *t,
        kui_media_connect_fn connect,const char *selector) {
    const struct kui_games_entry *entry=game->entry;
    if(entry->cover_path[0]) return KUI_MEDIA_OWNER;
    const char *leaf=strrchr(entry->path,'/');
    if(!leaf || !kui_game_image_name_supported(leaf+1)) return KUI_MEDIA_UNIDENTIFIED;
    char key[KUI_COVER_PATH_CAP];
    if(strlen(entry->path)>=KUI_COVER_PATH_CAP || !kui_cover_key(entry->name,key))
        return unavailable(t,KUI_MEDIA_REASON_LOCAL_PATH);
    struct cover_update c={0};
    if(!kui_games_prepare_media_cover_state(entry,&c.state,&game->identity,t->log,t->cancel)) return KUI_MEDIA_ERROR;
    if(c.state==KUI_GAMES_MEDIA_COVER_OWNER)
        return KUI_MEDIA_OWNER;
    if(c.state==KUI_GAMES_MEDIA_COVER_MANAGED && game->artwork_mode==KUI_ART_MISSING) return KUI_MEDIA_EXISTING;
    if(!game->identity.product[0]) {
        enum kui_games_identity_result found=kui_games_media_identity(entry->path,&game->identity,t->log,t->cancel);
        if(found!=KUI_GAMES_IDENTITY_FOUND) return found==KUI_GAMES_IDENTITY_UNKNOWN?KUI_MEDIA_UNIDENTIFIED:KUI_MEDIA_ERROR;
    }
    t->id=&game->identity;
    c.release=kui_internet_cover_name(t->id);
    bool small=kui_internet_cover_small_url(t->id,t->url,sizeof(t->url));
    if(!small && !kui_internet_cover_url(t->id,t->url,sizeof(t->url))) {
        if(c.release) return unavailable(t,KUI_MEDIA_REASON_NOT_LISTED);
        t->reason=KUI_MEDIA_REASON_NO_RELEASE;
        return KUI_MEDIA_UNIDENTIFIED;
    }
    if(c.state==KUI_GAMES_MEDIA_COVER_MANAGED) {
        FIL selected;
        if(kui_games_open_compact_target(&selected,selector,c.current,sizeof(c.current))!=FR_OK) return KUI_MEDIA_ERROR;
        if(f_close(&selected)!=FR_OK) return KUI_MEDIA_ERROR;
    }
    if(game->artwork_mode!=KUI_ART_MISSING) {
        const char *done=kui_artwork_batch_checked(game,c.release);
        if(done) return bind_cover(game,t,&c,selector,done)?KUI_MEDIA_EXISTING:KUI_MEDIA_ERROR;
    }
    if(!c.current[0]) {
        if(!cover_master(entry,t->id,c.master,t->log,t->cancel)) return KUI_MEDIA_ERROR;
        if(c.master[0]) {
            enum kui_media_result present=master_present(c.master,t->cancel);
            if(present==KUI_MEDIA_EXISTING) {
                if(game->artwork_mode==KUI_ART_MISSING)
                    return finish_cover(entry,c.master,selector,t)?KUI_MEDIA_EXISTING:KUI_MEDIA_ERROR;
                strcpy(c.current,c.master);
            } else if(present!=KUI_MEDIA_MISSING) return present;
        }
    }
    if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
    t->session=connect(t->ctx);
    if(!t->session || !ensure_dir("0:/KUI") || !ensure_dir(MEDIA_DIR) || !ensure_dir(KUI_DOWNLOADED_COVERS))
        return KUI_MEDIA_ERROR;
    if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
    bool preferred_png=false;
    if(game->artwork_mode==KUI_ART_CHECK && c.current[0]) {
        c.conditional=load_cover_validator(t,small,&c);
        /* Recheck the provider that actually supplied this version before
         * paying for a repeat attempt at a missing or rejected thumbnail. */
        if(small && !c.conditional && kui_internet_cover_url(t->id,t->url,sizeof(t->url))) {
            c.conditional=load_cover_validator(t,false,&c);
            if(c.conditional) { small=false; preferred_png=true; }
            else if(!kui_internet_cover_small_url(t->id,t->url,sizeof(t->url))) return KUI_MEDIA_ERROR;
        }
    }
    enum kui_media_result result=cover_transfer(game,t,&c,selector,small);
    if(result==KUI_MEDIA_MISSING && (small || preferred_png)) {
        bool alternate=small?kui_internet_cover_url(t->id,t->url,sizeof(t->url)):
            kui_internet_cover_small_url(t->id,t->url,sizeof(t->url));
        if(alternate) {
            c.conditional=game->artwork_mode==KUI_ART_CHECK && c.current[0] &&
                load_cover_validator(t,!small,&c);
            result=cover_transfer(game,t,&c,selector,!small);
        }
    }
    return result;
}

static enum kui_media_result fetch_mounted(struct transfer *t, struct kui_media_game *game,
        enum kui_media_kind kind, kui_media_connect_fn connect, char final[KUI_GAMES_FILE_CAP]) {
    const struct kui_games_entry *entry = game->entry;
    enum kui_media_result result;
    bool replacing_preview=false;
    uint32_t preview_crc=0;
    if(kind==KUI_MEDIA_COVER) return fetch_cover(game,t,connect,final);
    {
        unsigned version = 0;
        char upgraded[KUI_GAMES_FILE_CAP];
        if(kind == KUI_MEDIA_PREVIEW) {
            if(!upgraded_preview_path(final,upgraded)) return KUI_MEDIA_ERROR;
            result = cached_for_set(entry,upgraded,kind,false,&version);
            if(result == KUI_MEDIA_EXISTING) return version==2 ?
                finish_preview(game,upgraded,result,false,t->cancel) : KUI_MEDIA_ERROR;
            if(result != KUI_MEDIA_MISSING) return result;
        }
        result = cached_for_set(entry,final,kind,false,&version);
        /* KPV1 discarded source audio. Either scan scope can upgrade it
         * alongside the original. A current KPV2 may intentionally be silent. */
        if(result == KUI_MEDIA_EXISTING && kind == KUI_MEDIA_PREVIEW && version == 1) {
            strcpy(final,upgraded);
            replacing_preview=true;
            result = KUI_MEDIA_MISSING;
        }
        if(result==KUI_MEDIA_EXISTING && kind==KUI_MEDIA_PREVIEW)
            return finish_preview(game,final,result,false,t->cancel);
        if(result != KUI_MEDIA_MISSING) return result;
        if(kind == KUI_MEDIA_MANUAL && entry->media_disc1_key[0]) {
            char pdf[KUI_GAMES_FILE_CAP];
            if(!kui_media_original_path(entry,kind,pdf)) return KUI_MEDIA_ERROR;
            result = cached_for_set(entry,pdf,kind,true,NULL);
            if(result == KUI_MEDIA_EXISTING) return KUI_MEDIA_ORIGINAL_EXISTING;
            if(result != KUI_MEDIA_MISSING) return result;
        }
    }

    /* The box manual and preview belong to Disc 1, even when a later disc
     * is scanned first and its product serial differs. */
    if(kind != KUI_MEDIA_COVER && entry->media_disc1_key[0] && !game->set_identity.product[0]) {
        char first[KUI_GAMES_FILE_CAP];
        if(kui_games_library_media_disc1(entry,first) && strcmp(entry->path,first)) {
            enum kui_games_identity_result found = kui_games_media_identity(first,&game->set_identity,t->log,t->cancel);
            if(found == KUI_GAMES_IDENTITY_ERROR) return KUI_MEDIA_ERROR;
        }
    }
    t->id = kind != KUI_MEDIA_COVER && game->set_identity.product[0] ? &game->set_identity : &game->identity;
    if(!t->id->product[0]) {
        enum kui_games_identity_result found = kui_games_media_identity(entry->path,&game->identity,t->log,t->cancel);
        if(found != KUI_GAMES_IDENTITY_FOUND) return found == KUI_GAMES_IDENTITY_UNKNOWN ?
            KUI_MEDIA_UNIDENTIFIED : KUI_MEDIA_ERROR;
    }
    if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
    unsigned pages = kind == KUI_MEDIA_MANUAL ? kui_internet_manual_pages(t->id) : 0;
    bool has_url=kui_internet_extra_url(t->id,kind,t->url,sizeof(t->url));
    unsigned preview_attempt=0, preview_start=0;
    if(kind==KUI_MEDIA_PREVIEW && !has_url)
        has_url=kui_internet_preview_fallback_url(t->id,&preview_attempt,t->url,sizeof(t->url),&preview_start);
    if(!pages && !has_url) {
        /* A manual saved by a previous catalogue is still available offline.
         * Disc sets already checked their shared PDF before reading the ID. */
        if(kind==KUI_MEDIA_MANUAL && !entry->media_disc1_key[0]) {
            if(!kui_media_original_path(entry,kind,final)) return KUI_MEDIA_ERROR;
            result=cached(final,kind,true,NULL);
            if(result==KUI_MEDIA_EXISTING) return KUI_MEDIA_ORIGINAL_EXISTING;
            if(result!=KUI_MEDIA_MISSING) return result;
        }
        if(kui_internet_cover_name(t->id)) return unavailable(t,KUI_MEDIA_REASON_NOT_LISTED);
        t->reason=KUI_MEDIA_REASON_NO_RELEASE;
        return KUI_MEDIA_UNIDENTIFIED;
    }
    FILE_RETRY:
    if(kind == KUI_MEDIA_MANUAL && !pages) {
        if(!kui_media_original_path(entry,kind,final)) return KUI_MEDIA_ERROR;
        result = cached_for_set(entry,final,kind,true,NULL);
        if(result == KUI_MEDIA_EXISTING) return KUI_MEDIA_ORIGINAL_EXISTING;
        if(result != KUI_MEDIA_MISSING) return result;
        /* Page requests used the same URL scratch space. */
        if(!kui_internet_extra_url(t->id,kind,t->url,sizeof(t->url))) return KUI_MEDIA_MISSING;
    }
    if(!t->session) {
        if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
        t->session = connect(t->ctx);
        if(!t->session) return KUI_MEDIA_ERROR;
        if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
        if(!ensure_dir("0:/KUI") || !ensure_dir(MEDIA_DIR)) return KUI_MEDIA_ERROR;
    }
    FIL file;
    if(f_open(&file,MEDIA_TEMP,FA_READ|FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) return KUI_MEDIA_ERROR;
    if(pages) result = fetch_pages(t,&file,pages);
    else if(kind == KUI_MEDIA_PREVIEW) result = fetch_preview(t,&file,preview_start,replacing_preview?&preview_crc:NULL);
    else {
        result=get(t,KUI_MANUAL_BYTES_MAX,&file);
        if(result==KUI_MEDIA_INSTALLED) {
            if(f_lseek(&file,0)!=FR_OK) result=KUI_MEDIA_ERROR;
            else if(!valid_pdf(&file)) result=unavailable(t,KUI_MEDIA_REASON_INVALID_ASSET);
        }
    }
    if(result==KUI_MEDIA_INSTALLED && f_sync(&file)!=FR_OK) result=KUI_MEDIA_ERROR;
    if(f_close(&file)!=FR_OK) result=KUI_MEDIA_ERROR;
    if(result==KUI_MEDIA_INSTALLED && replacing_preview && !complete_preview(MEDIA_TEMP,&preview_crc,t->cancel))
        result=KUI_MEDIA_ERROR;
    if(stopped(t->cancel)) result=KUI_MEDIA_STOPPED;
    if(result==KUI_MEDIA_INSTALLED && f_rename(MEDIA_TEMP,final)!=FR_OK) result=KUI_MEDIA_ERROR;
    if(result != KUI_MEDIA_INSTALLED) {
        if(f_unlink(MEDIA_TEMP) != FR_OK) return KUI_MEDIA_ERROR;
        if(kind==KUI_MEDIA_PREVIEW && result==KUI_MEDIA_MISSING &&
           t->reason==KUI_MEDIA_REASON_PROVIDER_MISSING &&
           kui_internet_preview_fallback_url(t->id,&preview_attempt,t->url,sizeof(t->url),&preview_start)) goto FILE_RETRY;
        if(result == KUI_MEDIA_MISSING && pages && has_url) {
            pages = 0;
            goto FILE_RETRY;
        }
        return result;
    }
    if(replacing_preview) {
        unsigned version=0;
        if(cached(final,kind,false,&version)!=KUI_MEDIA_EXISTING || version!=2) {
            game->preview_cleanup_pending=true;
            return KUI_MEDIA_ERROR;
        }
        return finish_preview(game,final,KUI_MEDIA_INSTALLED,true,t->cancel);
    }
    return kind == KUI_MEDIA_MANUAL && !pages ? KUI_MEDIA_ORIGINAL : KUI_MEDIA_INSTALLED;
}

enum kui_media_result kui_media_fetch_internet(struct kui_media_game *game,
        enum kui_media_kind kind, kui_media_connect_fn connect, kui_log_fn log,
        kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx) {
    if(game) {
        game->reason=KUI_MEDIA_REASON_NONE;
        game->previews_removed=0;
        game->preview_cleanup_pending=false;
    }
    if(stopped(cancel)) return KUI_MEDIA_STOPPED;
    char final[KUI_GAMES_FILE_CAP];
    if(!connect || !game || !game->entry || game->entry->disabled || game->entry->directory) return KUI_MEDIA_ERROR;
    if(!kui_media_file_path(game->entry,kind,final)) {
        game->reason=KUI_MEDIA_REASON_LOCAL_PATH;
        return KUI_MEDIA_MISSING;
    }
    if(kui_games_storage_open(log) != KUI_GAMES_STORAGE_OK) return KUI_MEDIA_ERROR;
    struct transfer t = {.progress=progress,.ctx=ctx,.cancel=cancel,.log=log};
    enum kui_media_result r = fetch_mounted(&t,game,kind,connect,final);
    if(r == KUI_MEDIA_ERROR && log) log("Media: could not install the complete asset for %.80s",game->entry->name);
    if(!kui_games_storage_done(r != KUI_MEDIA_ERROR)) return KUI_MEDIA_ERROR;
    if(stopped(cancel)) return KUI_MEDIA_STOPPED;
    if(r==KUI_MEDIA_UNIDENTIFIED)
        game->reason=t.reason==KUI_MEDIA_REASON_NO_RELEASE?t.reason:KUI_MEDIA_REASON_DISC_ID;
    else if(r==KUI_MEDIA_MISSING) game->reason=t.reason;
    return r;
}
