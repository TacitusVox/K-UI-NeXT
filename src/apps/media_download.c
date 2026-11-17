/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_download.h"
#include "kui/media_container.h"
#include "kui/internet_cover.h"
#include "kui/internet_extra.h"
#include "kui/internet_preview.h"
#include "kui/preview_prefix.h"
#include "kui/cover_image.h"
#include "kui/game_cover.h"
#include "kui/game_image.h"
#include "kui/games_media_storage.h"
#include "kui/games_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEDIA_DIR "0:/KUI/media"
/* Owned exclusively by the Games storage worker. A retry reclaims its staging files. */
#define MEDIA_TEMP MEDIA_DIR "/fetch.tmp"
#define VIDEO_TEMP MEDIA_DIR "/video.tmp"
#define SOURCE_VIDEO_MAX (32u * 1024u * 1024u)

static bool stopped(kui_cancel_fn cancel) { return cancel && cancel(); }
static bool media_path(const struct kui_games_entry *entry, const char *folder, const char *ext,
        char out[KUI_GAMES_FILE_CAP]) {
    return entry && out && memchr(entry->name, 0, sizeof(entry->name)) &&
        memchr(entry->path, 0, sizeof(entry->path)) &&
        kui_games_media_path(entry->path, folder, ext, out, KUI_GAMES_FILE_CAP);
}
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    static const char *const ext[] = {"png", "kdm", "kpv"};
    return (unsigned)kind <= KUI_MEDIA_PREVIEW &&
        media_path(entry, kind == KUI_MEDIA_COVER ? KUI_DOWNLOADED_COVERS : MEDIA_DIR, ext[kind], out);
}
bool kui_media_original_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
        char out[KUI_GAMES_FILE_CAP]) {
    return kind == KUI_MEDIA_MANUAL && media_path(entry, MEDIA_DIR, "pdf", out);
}
static uint32_t big32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static bool complete_png(const uint8_t *data, size_t size) {
    size_t at = 8;
    while(size-at >= 12) {
        uint32_t n = big32(data+at);
        if(n > size-at-12 || kui_crc32_capture(0, data+at+4, n+4) != big32(data+at+8+n)) return false;
        if(!memcmp(data+at+4, "IEND", 4)) return !n && at+12 == size;
        at += n+12;
    }
    return false;
}
static bool valid_png(FIL *f) {
    size_t size = (size_t)f_size(f);
    if(size < 33 || size > KUI_COVER_IMAGE_MAX_BYTES) return false;
    uint8_t *bytes = malloc(size), *pixels = NULL;
    unsigned width = 0, height = 0, channels = 0;
    UINT got = 0;
    bool ok = bytes && f_read(f, bytes, (UINT)size, &got) == FR_OK && got == size &&
        !memcmp(bytes, "\211PNG\r\n\032\n\0\0\0\rIHDR", 16);
    if(ok) {
        /* Use the same dimension ceiling as the Games artwork cache. */
        uint32_t w = big32(bytes+16), h = big32(bytes+20);
        ok = w && h && w <= 8192u && h <= 8192u && (uint64_t)w*h <= KUI_COVER_IMAGE_MAX_PIXELS && complete_png(bytes, size) &&
            kui_cover_image_decode(bytes, size, &pixels, &width, &height, &channels) == KUI_COVER_IMAGE_OK;
    }
    kui_cover_image_free(pixels);
    free(bytes);
    return ok;
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
static enum kui_media_result media_result(enum kui_http_result r) {
    switch(r) {
    case KUI_HTTP_OK:        return KUI_MEDIA_INSTALLED;
    case KUI_HTTP_LIMITED:   return KUI_MEDIA_LIMITED;
    case KUI_HTTP_TOO_LARGE:
    case KUI_HTTP_MISSING:   return KUI_MEDIA_MISSING;
    case KUI_HTTP_STOPPED:   return KUI_MEDIA_STOPPED;
    default:                 return KUI_MEDIA_ERROR;
    }
}
static enum kui_media_result get(struct kui_internet *session, const char *url, uint32_t max,
        FIL *file, kui_http_progress_fn progress, void *ctx) {
    return media_result(kui_internet_get(session,url,max,write_data,file,progress,ctx));
}
static enum kui_media_result cached(const char *path, enum kui_media_kind kind, bool pdf) {
    FIL file;
    FRESULT r = f_open(&file, path, FA_READ);
    if(r == FR_NO_FILE || r == FR_NO_PATH) return KUI_MEDIA_MISSING;
    if(r != FR_OK) return KUI_MEDIA_ERROR;
    bool valid = pdf ? valid_pdf(&file) : kui_media_container_check(&file, kind == KUI_MEDIA_MANUAL);
    if(f_close(&file) != FR_OK) valid = false;
    return valid ? KUI_MEDIA_EXISTING : KUI_MEDIA_ERROR;
}

struct transfer {
    struct kui_internet *session;
    const struct kui_games_disc_identity *id;
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
static void le32(uint8_t *dst, uint32_t value) {
    for(unsigned i = 0; i < 4; ++i) dst[i] = (uint8_t)(value >> (8*i));
}
static enum kui_media_result fetch_pages(struct transfer *t, FIL *file, unsigned count) {
    if(!count || count > KUI_MANUAL_PAGES_MAX) return KUI_MEDIA_MISSING;
    /* Save a contiguous JPEG per page and a small seek table, exactly the format
     * used by the local manual viewer. No page bitmap is needed during a scan. */
    uint8_t header[8 + 4 * (KUI_MANUAL_PAGES_MAX + 1)] = {'K','D','M','1'};
    header[4] = (uint8_t)count;
    header[5] = (uint8_t)(count >> 8);
    size_t header_bytes = 8 + 4 * (count + 1);
    if(!write_data(file,header,header_bytes)) return KUI_MEDIA_ERROR;
    for(unsigned i = 0; i < count; ++i) {
        if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
        if(!kui_internet_manual_page_url(t->id,i,t->url,sizeof(t->url))) return KUI_MEDIA_MISSING;
        uint32_t begin = (uint32_t)f_tell(file);
        le32(header+8+i*4,begin);
        uint32_t limit = KUI_MANUAL_BYTES_MAX-begin;
        if(limit > KUI_MANUAL_PAGE_BYTES_MAX) limit = KUI_MANUAL_PAGE_BYTES_MAX;
        if(!limit) return KUI_MEDIA_MISSING;
        if(t->log) t->log("Manual: fetching page %u of %u",i+1,count);
        if(t->progress) t->progress(t->ctx,KUI_MEDIA_MANUAL_PAGE,i+1,count);
        enum kui_media_result result = get(t->session,t->url,limit,file,received,t);
        if(result != KUI_MEDIA_INSTALLED) return result;
        if(f_tell(file) <= begin || f_tell(file)-begin > limit) return KUI_MEDIA_ERROR;
    }
    le32(header+8+count*4,(uint32_t)f_tell(file));
    if(f_lseek(file,0) != FR_OK || !write_data(file,header,header_bytes)) return KUI_MEDIA_ERROR;
    return kui_manual_check_pages(file,t->cancel) ? KUI_MEDIA_INSTALLED :
           stopped(t->cancel) ? KUI_MEDIA_STOPPED : KUI_MEDIA_MISSING;
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
static enum kui_media_result fetch_preview(struct transfer *t, FIL *output) {
    FIL source;
    if(f_open(&source,VIDEO_TEMP,FA_READ|FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) return KUI_MEDIA_ERROR;
    struct preview_sink sink = {.file=&source};
    enum kui_media_result result = media_result(kui_internet_get_prefix(t->session,t->url,SOURCE_VIDEO_MAX,
        write_video,&sink,received,t,preview_complete));
    if(result == KUI_MEDIA_INSTALLED) {
        if(t->progress) t->progress(t->ctx,KUI_MEDIA_CONVERTING,0,0);
        if(f_lseek(&source,0) != FR_OK) result = KUI_MEDIA_ERROR;
        else if(!kui_internet_preview_convert(&source,output,t->cancel,t->log)) result = KUI_MEDIA_ERROR;
        else if(!kui_media_container_check(output,false)) result = KUI_MEDIA_ERROR;
    }
    if(f_close(&source) != FR_OK) result = KUI_MEDIA_ERROR;
    if(f_unlink(VIDEO_TEMP) != FR_OK) result = KUI_MEDIA_ERROR;
    return result;
}

static enum kui_media_result fetch_mounted(struct transfer *t, const struct kui_games_entry *entry,
        enum kui_media_kind kind, char final[KUI_GAMES_FILE_CAP]) {
    enum kui_media_result result;
    if(kind == KUI_MEDIA_COVER) {
        if(entry->cover_path[0]) return KUI_MEDIA_EXISTING;
        const char *leaf = strrchr(entry->path, '/');
        if(!leaf || !kui_game_image_name_supported(leaf+1)) return KUI_MEDIA_UNIDENTIFIED;
        if(strlen(entry->path) >= KUI_COVER_PATH_CAP) return KUI_MEDIA_MISSING;
        char key[KUI_COVER_PATH_CAP];
        if(!kui_cover_key(entry->name,key)) return KUI_MEDIA_MISSING;
        bool present = false;
        if(!kui_games_prepare_media_cover(entry,&present,t->log,t->cancel)) return KUI_MEDIA_ERROR;
        if(present) return KUI_MEDIA_EXISTING;
    } else {
        result = cached(final,kind,false);
        if(result != KUI_MEDIA_MISSING) return result;
    }

    struct kui_games_disc_identity identity;
    enum kui_games_identity_result found = kui_games_media_identity(entry->path,&identity,t->log,t->cancel);
    if(found != KUI_GAMES_IDENTITY_FOUND) return found == KUI_GAMES_IDENTITY_UNKNOWN ?
        KUI_MEDIA_UNIDENTIFIED : KUI_MEDIA_ERROR;
    t->id = &identity;
    if(stopped(t->cancel)) return KUI_MEDIA_STOPPED;
    unsigned pages = kind == KUI_MEDIA_MANUAL ? kui_internet_manual_pages(&identity) : 0;
    bool has_url = kind == KUI_MEDIA_COVER ? kui_internet_cover_url(&identity,t->url,sizeof(t->url)) :
        kui_internet_extra_url(&identity,kind,t->url,sizeof(t->url));
    if(!pages && !has_url) return KUI_MEDIA_MISSING;
    if(!ensure_dir("0:/KUI") || !ensure_dir(MEDIA_DIR) ||
       (kind == KUI_MEDIA_COVER && !ensure_dir(KUI_DOWNLOADED_COVERS))) return KUI_MEDIA_ERROR;

    FILE_RETRY:
    if(kind == KUI_MEDIA_MANUAL && !pages) {
        if(!kui_media_original_path(entry,kind,final)) return KUI_MEDIA_ERROR;
        result = cached(final,kind,true);
        if(result != KUI_MEDIA_MISSING) return result;
        /* Page requests used the same URL scratch space. */
        if(!kui_internet_extra_url(&identity,kind,t->url,sizeof(t->url))) return KUI_MEDIA_MISSING;
    }
    FIL file;
    if(f_open(&file,MEDIA_TEMP,FA_READ|FA_WRITE|FA_CREATE_ALWAYS) != FR_OK) return KUI_MEDIA_ERROR;
    if(pages) result = fetch_pages(t,&file,pages);
    else if(kind == KUI_MEDIA_PREVIEW) result = fetch_preview(t,&file);
    else {
        uint32_t limit = kind == KUI_MEDIA_COVER ? KUI_COVER_IMAGE_MAX_BYTES : KUI_MANUAL_BYTES_MAX;
        result = get(t->session,t->url,limit,&file,received,t);
        if(result == KUI_MEDIA_INSTALLED) {
            if(f_lseek(&file,0) != FR_OK) result = KUI_MEDIA_ERROR;
            else if(kind == KUI_MEDIA_COVER ? !valid_png(&file) : !valid_pdf(&file)) result = KUI_MEDIA_MISSING;
        }
    }
    if(result == KUI_MEDIA_INSTALLED && f_sync(&file) != FR_OK) result = KUI_MEDIA_ERROR;
    if(f_close(&file) != FR_OK) result = KUI_MEDIA_ERROR;
    if(stopped(t->cancel)) result = KUI_MEDIA_STOPPED;
    if(result == KUI_MEDIA_INSTALLED && f_rename(MEDIA_TEMP,final) != FR_OK) result = KUI_MEDIA_ERROR;
    if(result != KUI_MEDIA_INSTALLED) {
        if(f_unlink(MEDIA_TEMP) != FR_OK) return KUI_MEDIA_ERROR;
        if(result == KUI_MEDIA_MISSING && pages && has_url) {
            pages = 0;
            goto FILE_RETRY;
        }
        return result;
    }
    if(kind == KUI_MEDIA_COVER) {
        bool present = false;
        if(!kui_games_prepare_media_cover(entry,&present,t->log,t->cancel) || !present) return KUI_MEDIA_ERROR;
    }
    return kind == KUI_MEDIA_MANUAL && !pages ? KUI_MEDIA_ORIGINAL : KUI_MEDIA_INSTALLED;
}

enum kui_media_result kui_media_fetch_internet(struct kui_internet *session,
        const struct kui_games_entry *entry, enum kui_media_kind kind, kui_log_fn log,
        kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx) {
    if(stopped(cancel)) return KUI_MEDIA_STOPPED;
    char final[KUI_GAMES_FILE_CAP];
    if(!session || !entry || entry->disabled || entry->directory) return KUI_MEDIA_ERROR;
    if(!kui_media_file_path(entry,kind,final)) return KUI_MEDIA_MISSING;
    if(kui_games_storage_open(log) != KUI_GAMES_STORAGE_OK) return KUI_MEDIA_ERROR;
    struct transfer t = {.session=session,.progress=progress,.ctx=ctx,.cancel=cancel,.log=log};
    enum kui_media_result r = fetch_mounted(&t,entry,kind,final);
    if(r == KUI_MEDIA_ERROR && log) log("Media: could not install the complete asset for %.80s",entry->name);
    if(!kui_games_storage_done(r != KUI_MEDIA_ERROR)) return KUI_MEDIA_ERROR;
    return stopped(cancel) ? KUI_MEDIA_STOPPED : r;
}
