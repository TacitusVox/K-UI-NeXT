/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games_covers.h"
#include "kui/games_media_storage.h"
#include "kui/cover_image.h"
#include "kui/compact_cover.h"
#include "kui/game_image.h"
#include "kui/game_launch_settings.h"
#include "kui/game_metadata.h"
#include "kui/games_storage.h"
#include "kui/pvr_texture.h"
#include "kui/retail_image.h"
#include "platform.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Box art lives beside the rest of K-UI's data, never in game folders:
 * KUI/covers/<name>.kcv records (game_cover.h), optional owner images
 * <name>.png/.jpg/.jpeg, and KUI/apps/games/view.txt for the chosen view. */
#define PATH_CAP (KUI_GAMES_FILE_CAP + 3u)
static const char *const view_names[KUI_GAMES_VIEW_COUNT] = {"list", "compact", "gallery", "flow"};
static const char *const user_images[] = {".png", ".jpg", ".jpeg"};
#define COVER_CACHE_SLOTS 32u
#define COVER_CACHE_BYTES (512u * 1024u)
struct cached_cover {
    struct kui_games_cover_result result;
    uint16_t *pixels;
    unsigned used,bytes;
    bool valid;
};
static struct cached_cover covers[COVER_CACHE_SLOTS];
static unsigned cover_clock,cover_bytes,saved_view;
static bool view_known,artwork_known,artwork_exists,art_preference_known;
static enum kui_games_art_preference art_preference;
/* Display titles of rows near the page on screen, read from record headers
 * alone, so a page shows its titles as soon as it is drawn. Same identity
 * rule as the artwork cache: this exact path and row name. */
#define TITLE_CACHE_SLOTS 128u
struct cached_title {
    char path[KUI_GAMES_FILE_CAP],name[KUI_DEST_NAME_CAP],title[KUI_COVER_TITLE_CAP];
    char region[9];
    unsigned used;bool valid;
};
static struct cached_title titles[TITLE_CACHE_SLOTS];
static unsigned title_clock;
void kui_games_covers_cache_clear(void) {
    for(unsigned i=0;i<COVER_CACHE_SLOTS;i++) free(covers[i].pixels);
    memset(covers,0,sizeof(covers));cover_clock=cover_bytes=saved_view=0;
    view_known=artwork_known=artwork_exists=art_preference_known=false;
    memset(titles,0,sizeof(titles));title_clock=0;
}
void kui_games_covers_content_clear(void) {
    for(unsigned i=0;i<COVER_CACHE_SLOTS;i++)free(covers[i].pixels);
    memset(covers,0,sizeof(covers));cover_clock=cover_bytes=0;
    memset(titles,0,sizeof(titles));title_clock=0;
}
static struct cached_title *title_find(const struct kui_games_entry *entry) {
    for(unsigned i=0;i<TITLE_CACHE_SLOTS;i++) if(titles[i].valid &&
        !strcmp(titles[i].path,entry->path) && !strcmp(titles[i].name,entry->name)) return &titles[i];
    return NULL;
}
static void title_store(const struct kui_games_entry *entry,const char *title) {
    struct cached_title *slot=title_find(entry);
    for(unsigned i=0;!slot && i<TITLE_CACHE_SLOTS;i++) if(!titles[i].valid) slot=&titles[i];
    if(!slot) {
        slot=&titles[0];
        for(unsigned i=1;i<TITLE_CACHE_SLOTS;i++) if(titles[i].used<slot->used) slot=&titles[i];
    }
    strcpy(slot->path,entry->path);strcpy(slot->name,entry->name);
    strcpy(slot->region,entry->region);
    snprintf(slot->title,sizeof(slot->title),"%s",title);
    slot->used=++title_clock;slot->valid=true;
}
bool kui_games_cached_region(const struct kui_games_entry *entry,char out[9]) {
    if(!entry || !out)return false;
    struct cached_title *known=title_find(entry);
    if(!known || !known->region[0])return false;
    strcpy(out,known->region);return true;
}

enum kui_cover_size kui_games_view_size(unsigned view) {
    return view == KUI_GAMES_VIEW_GALLERY ? KUI_COVER_SIZE_MEDIUM :
        view == KUI_GAMES_VIEW_COMPACT ? KUI_COVER_SIZE_SMALL : KUI_COVER_SIZE_LARGE;
}
const char *kui_games_view_name(unsigned view) {
    return view < KUI_GAMES_VIEW_COUNT ? view_names[view] : view_names[0];
}
static bool stopped(kui_cancel_fn cancel) { return cancel && cancel(); }
static bool record_path(const char *key, const char *extension, char out[PATH_CAP]) {
    int n = snprintf(out, PATH_CAP, "%s/%s%s", KUI_GAMES_COVERS_FOLDER, key, extension);
    return n > 0 && n < (int)PATH_CAP;
}
static bool make_folders(const char *const *paths, unsigned count, kui_log_fn log) {
    for(unsigned i = 0; i < count; ++i) {
        FILINFO info;
        FRESULT r = f_stat(paths[i], &info);
        if(r == FR_OK && (info.fattrib & AM_DIR)) continue;
        if(r == FR_NO_FILE || r == FR_NO_PATH) r = f_mkdir(paths[i]);
        else if(r == FR_OK) r = FR_EXIST; /* A file is in the way. */
        if(r != FR_OK) {
            if(log) log("Cannot create %s: FatFs=%u", paths[i] + 2, (unsigned)r);
            return false;
        }
    }
    return true;
}

/* A zero-length marker needs no in-place overwrite on FAT. Its absence is
 * the historical preference, so existing cards need no migration. */
#define DISC_PREFERENCE "0:/KUI/apps/games/prefer-disc"
static bool read_art_preference(enum kui_games_art_preference *value) {
    FILINFO info;
    FRESULT r = f_stat(DISC_PREFERENCE, &info);
    if(r == FR_NO_FILE || r == FR_NO_PATH) {
        *value = KUI_GAMES_ART_DOWNLOADED_FIRST;
        return true;
    }
    if(r != FR_OK || (info.fattrib & AM_DIR) || info.fsize) return false;
    *value = KUI_GAMES_ART_DISC_FIRST;
    return true;
}
static bool load_art_preference(void) {
    if(!art_preference_known) art_preference_known = read_art_preference(&art_preference);
    return art_preference_known;
}
bool kui_games_art_preference_get(enum kui_games_art_preference *out, kui_log_fn log) {
    if(!out) return false;
    if(!art_preference_known && kui_games_storage_open(log) == KUI_GAMES_STORAGE_OK) {
        bool ok = load_art_preference();
        if(!kui_games_storage_done(ok)) art_preference_known = false;
    }
    if(art_preference_known) *out = art_preference;
    return art_preference_known;
}
bool kui_games_art_preference_save(enum kui_games_art_preference value, kui_log_fn log) {
    if(value != KUI_GAMES_ART_DOWNLOADED_FIRST && value != KUI_GAMES_ART_DISC_FIRST) return false;
    if(kui_games_storage_open(log) != KUI_GAMES_STORAGE_OK) return false;
    enum kui_games_art_preference current;
    bool ok = read_art_preference(&current);
    if(ok && current != value) {
        if(value == KUI_GAMES_ART_DISC_FIRST) {
            static const char *const parents[] = {"0:/KUI", "0:/KUI/apps", "0:/KUI/apps/games"};
            FIL marker;
            ok = make_folders(parents, 3, log) && f_open(&marker, DISC_PREFERENCE, FA_CREATE_NEW | FA_WRITE) == FR_OK;
            if(ok) {
                ok = f_sync(&marker) == FR_OK;
                if(f_close(&marker) != FR_OK) ok = false;
            }
        } else ok = f_unlink(DISC_PREFERENCE) == FR_OK;
    }
    if(ok) ok = read_art_preference(&current) && current == value;
    if(!kui_games_storage_done(ok)) ok = false;
    if(ok) {
        if(!art_preference_known || art_preference != value) kui_games_covers_content_clear();
        art_preference = value;
        art_preference_known = true;
    }
    return ok;
}

/* ---- The saved view ---- */
static unsigned view_load(void) {
    FIL file;
    char text[16] = {0};
    UINT got = 0;
    if(f_open(&file, KUI_GAMES_VIEW_FILE, FA_READ) != FR_OK) return KUI_GAMES_VIEW_LIST;
    bool ok = f_size(&file) < sizeof(text) && f_read(&file, text, sizeof(text) - 1u, &got) == FR_OK;
    if(f_close(&file) != FR_OK || !ok) return KUI_GAMES_VIEW_LIST;
    text[got] = 0;
    char *end = strchr(text, '\n');
    if(end) *end = 0;
    for(unsigned v = 0; v < KUI_GAMES_VIEW_COUNT; ++v) if(!strcmp(text, view_names[v])) return v;
    return KUI_GAMES_VIEW_LIST;
}
static bool view_save(unsigned view, kui_log_fn log) {
    static const char *const parents[] = {"0:/KUI", "0:/KUI/apps", "0:/KUI/apps/games"};
    if(!make_folders(parents, 3, log)) return false;
    char text[16];
    int n = snprintf(text, sizeof(text), "%s\n", view_names[view]);
    FIL file;
    UINT wrote = 0;
    FRESULT r = f_open(&file, KUI_GAMES_VIEW_FILE, FA_WRITE | FA_CREATE_ALWAYS);
    if(r == FR_OK) {
        r = f_write(&file, text, (UINT)n, &wrote);
        if(r==FR_OK && wrote==(UINT)n)r=f_sync(&file);
        FRESULT closed = f_close(&file);
        if(r == FR_OK) r = closed;
    }
    bool ok=r==FR_OK && wrote==(UINT)n;
    if(ok) {
        char verified[16]={0};UINT got=0;
        r=f_open(&file,KUI_GAMES_VIEW_FILE,FA_READ);
        if(r==FR_OK) {
            ok=f_size(&file)==(FSIZE_t)n && f_read(&file,verified,(UINT)n,&got)==FR_OK &&
                got==(UINT)n && !memcmp(verified,text,(size_t)n);
            if(f_close(&file)!=FR_OK)ok=false;
        } else ok=false;
    }
    if(!ok && log) log("Games view save could not be verified: FatFs=%u", (unsigned)r);
    return ok;
}

/* ---- Reading records ---- */
static UINT record_bytes(enum kui_cover_source source) {
    return source == KUI_COVER_SOURCE_USER || source == KUI_COVER_SOURCE_DISC ?
        KUI_COVER_FILE_BYTES : KUI_COVER_HEADER_BYTES;
}
/* Keep I/O errors distinct from a header that was invalidated during replacement. */
static FRESULT record_read_at(const char *path, FIL *file, struct kui_cover_record *record, bool *invalid) {
    *invalid = false;
    FRESULT r = f_open(file, path, FA_READ);
    if(r != FR_OK) return r;
    uint8_t header[KUI_COVER_HEADER_BYTES];
    UINT got = 0;
    r = f_read(file, header, sizeof(header), &got);
    if(r == FR_OK && (got != sizeof(header) || !kui_cover_header_decode(record, header) ||
       f_size(file) != record_bytes(record->source))) {
        *invalid = true;
        r = FR_INVALID_OBJECT;
    }
    if(r != FR_OK && f_close(file) != FR_OK) {
        *invalid = false;
        return FR_DISK_ERR;
    }
    return r;
}
static FRESULT record_read(const char *key, FIL *file, struct kui_cover_record *record, bool *backup) {
    *backup = false;
    char path[PATH_CAP];
    if(!record_path(key, ".kcv", path)) return FR_INVALID_NAME;
    bool invalid;
    FRESULT r = record_read_at(path, file, record, &invalid);
    if(!invalid) return r;
    *backup = true;
    /* Replacement keeps the canonical name present with an invalid header until
     * its body is complete. Ordinary cache hits and misses take one lookup. */
    if(!record_path(key, ".kcb", path)) return FR_INVALID_NAME;
    r = record_read_at(path, file, record, &invalid);
    return r == FR_NO_FILE ? FR_INVALID_OBJECT : r;
}
/* Keep the existing filename for its rightful owner. Only a verified
 * same-name collision requires a second lookup, and the full selector
 * inside the secondary header must match even when its hash does. */
static FRESULT selector_record(char key[KUI_COVER_PATH_CAP], const char *selector, FIL *file,
        struct kui_cover_record *record, bool *backup, bool create, kui_log_fn log) {
    FRESULT r = record_read(key, file, record, backup);
    if(r != FR_OK || !strcmp(record->gdi_path, selector)) return r;
    if(f_close(file) != FR_OK) return FR_DISK_ERR;
    if(strchr(key, '/')) return FR_DENIED;
    char digest[33];
    if(!kui_games_media_key(selector, digest)) return FR_INVALID_NAME;
    snprintf(key, KUI_COVER_PATH_CAP, "by-path/%s", digest);
    r = record_read(key, file, record, backup);
    if(r == FR_OK && strcmp(record->gdi_path, selector))
        return f_close(file) == FR_OK ? FR_DENIED : FR_DISK_ERR;
    if(create && r == FR_NO_PATH) {
        static const char *const folders[] = {KUI_GAMES_COVERS_FOLDER "/by-path"};
        return make_folders(folders, 1, log) ? FR_NO_FILE : FR_DISK_ERR;
    }
    return r;
}
/* Each updated selector binds to immutable RGB565 pixels. Separate FAT
 * sectors hold two generations so a torn write to the inactive slot cannot
 * destroy the previously verified binding. Existing native masters and
 * other selectors borrowing them keep their original meaning. */
#define COMPACT_PATH_CAP (sizeof(KUI_DOWNLOADED_COVERS) + 37u)
#define OVERLAY_SLOT 48u
#define OVERLAY_STRIDE 512u
#define OVERLAY_STAMP (UINT64_C(1) << 63)
struct cover_overlay {
    uint64_t serial[2];
    char key[2][33];
};
static bool compact_key(const char *key) {
    return strlen(key) == 32 && strspn(key, "0123456789abcdef") == 32;
}
static void compact_path(const char *key, char out[COMPACT_PATH_CAP]) {
    snprintf(out, COMPACT_PATH_CAP, "%s/%s.kct", KUI_DOWNLOADED_COVERS, key);
}
static bool managed_name(const char *path, const char *extension) {
    static const char prefix[]=KUI_DOWNLOADED_COVERS "/";
    for(size_t i=0; i<sizeof(prefix)-1; ++i) {
        unsigned a=(unsigned char)path[i], b=(unsigned char)prefix[i];
        if(a>='A' && a<='Z') a+='a'-'A';
        if(b>='A' && b<='Z') b+='a'-'A';
        if(a!=b) return false;
    }
    const char *name=path+sizeof(prefix)-1;
    return strlen(name)==36 && strspn(name,"0123456789abcdef")==32 && !strcmp(name+32,extension);
}
static bool overlay_path(const char *base, char out[COMPACT_PATH_CAP]) {
    if(!managed_name(base,".kct")) return false;
    strcpy(out,base);
    out[strlen(out)-1]='u';
    return true;
}
static FRESULT publish_target(FIL *file, const char *path, char *target, size_t cap) {
    if(target) {
        size_t size=strlen(path)+1;
        if(size>cap) return f_close(file)==FR_OK ? FR_INVALID_NAME : FR_DISK_ERR;
        memmove(target,path,size);
    }
    return FR_OK;
}
static FRESULT open_native(FIL *file,const char *path) {
    FRESULT r=f_open(file,path,FA_READ);
    if(r==FR_OK && f_size(file)!=KUI_COMPACT_COVER_BYTES)
        return f_close(file)==FR_OK ? FR_INVALID_OBJECT : FR_DISK_ERR;
    return r;
}
static FRESULT link_key(FIL *file, char key[33]) {
    uint8_t data[KUI_COMPACT_LINK_BYTES];
    if(f_size(file)!=sizeof(data)) return FR_INVALID_OBJECT;
    UINT got;
    FRESULT r=f_read(file,data,sizeof(data),&got);
    if(r!=FR_OK) return r;
    if(got!=sizeof(data) || memcmp(data,"KCL1",4)) return FR_INVALID_OBJECT;
    memcpy(key,data+4,32);
    key[32]=0;
    return compact_key(key) ? FR_OK : FR_INVALID_OBJECT;
}
static FRESULT open_base(FIL *file,const char *path,char *target,size_t cap) {
    FRESULT r=f_open(file,path,FA_READ);
    if(r!=FR_OK) return r;
    char resolved[COMPACT_PATH_CAP];
    if(f_size(file)==KUI_COMPACT_LINK_BYTES) {
        char key[33];
        r=link_key(file,key);
        if(f_close(file)!=FR_OK) return FR_DISK_ERR;
        if(r!=FR_OK) return r;
        compact_path(key,resolved);
        r=open_native(file,resolved);
        if(r==FR_NO_PATH || r==FR_NO_FILE) return FR_INVALID_OBJECT;
        if(r!=FR_OK) return r;
        path=resolved;
    }
    return publish_target(file,path,target,cap);
}
FRESULT kui_games_open_compact_direct(FIL *file,const char *path) {
    return open_base(file,path,NULL,0);
}
static FRESULT read_slots(FIL *file,struct cover_overlay *slots) {
    memset(slots,0,sizeof(*slots));
    if(f_size(file)>2*OVERLAY_STRIDE) return FR_INVALID_OBJECT;
    for(unsigned n=0;n<2;n++) {
        FSIZE_t at=n*OVERLAY_STRIDE;
        if(f_size(file)<at+OVERLAY_SLOT) continue;
        uint8_t data[OVERLAY_SLOT]; UINT got;
        FRESULT r=f_lseek(file,at);
        if(r==FR_OK) r=f_read(file,data,sizeof(data),&got);
        if(r!=FR_OK) return r;
        if(got!=sizeof(data)) return FR_DISK_ERR;
        if(memcmp(data,"KCU1",4)) continue;
        uint32_t crc=kui_crc32_capture(0,data,44);
        bool intact=true;
        for(unsigned i=0;i<4;i++) if(data[44+i]!=(uint8_t)(crc>>(8*i))) intact=false;
        if(!intact) continue;
        memcpy(slots->key[n],data+12,32);
        if(!compact_key(slots->key[n])) continue;
        uint64_t serial=0;
        for(unsigned i=0;i<8;i++) serial |= (uint64_t)data[4+i]<<(8*i);
        if(serial<OVERLAY_STAMP) slots->serial[n]=serial;
    }
    return FR_OK;
}
FRESULT kui_games_cover_reference_keys(FIL *file,bool overlay,char keys[2][33]) {
    memset(keys,0,2*33);
    if(!overlay) return link_key(file,keys[0]);
    struct cover_overlay slots;
    FRESULT r=read_slots(file,&slots);
    if(r==FR_OK) {
        for(unsigned n=0;n<2;n++)
            if(slots.serial[n]) memcpy(keys[n],slots.key[n],sizeof(slots.key[n]));
    }
    return r;
}
static FRESULT read_overlay(const char *path,struct cover_overlay *slots) {
    FIL file; FRESULT r=f_open(&file,path,FA_READ);
    if(r!=FR_OK) return r;
    r=read_slots(&file,slots);
    return f_close(&file)==FR_OK ? r : FR_DISK_ERR;
}
static FRESULT open_slot(const struct cover_overlay *slots, unsigned n, FIL *file,char *target,size_t cap) {
    if(!slots->serial[n]) return FR_NO_FILE;
    char path[COMPACT_PATH_CAP]; compact_path(slots->key[n],path);
    FRESULT r=open_native(file,path);
    if(r==FR_OK) return publish_target(file,path,target,cap);
    return r;
}
static bool unavailable(FRESULT r) {
    return r==FR_NO_FILE || r==FR_NO_PATH || r==FR_INVALID_OBJECT;
}
static FRESULT verified_slot(const struct cover_overlay *slots,unsigned *slot,kui_cancel_fn cancel);
static FRESULT open_overlay(const char *path,FIL *file,char *target,size_t cap) {
    struct cover_overlay slots;
    FRESULT r=read_overlay(path,&slots);
    if(r!=FR_OK) return r;
    if(target) {
        /* Conditional requests and duplicate checks must identify the
         * actual verified master, including after damage to a newer slot. */
        unsigned valid;
        r=verified_slot(&slots,&valid,NULL);
        return r==FR_OK ? open_slot(&slots,valid,file,target,cap) : r;
    }
    unsigned n=slots.serial[1]>slots.serial[0] ? 1 : 0;
    r=open_slot(&slots,n,file,NULL,0);
    return unavailable(r) ? open_slot(&slots,1-n,file,NULL,0) : r;
}
FRESULT kui_games_open_compact_target(FIL *file,const char *path,char *target,size_t cap) {
    char overlay[COMPACT_PATH_CAP];
    if(overlay_path(path,overlay)) {
        FRESULT r=open_overlay(overlay,file,target,cap);
        if(!unavailable(r)) return r;
    }
    return open_base(file,path,target,cap);
}
FRESULT kui_games_open_compact(FIL *file,const char *path) {
    return kui_games_open_compact_target(file,path,NULL,0);
}
static bool record_pixels(FIL *file, const struct kui_cover_record *record,
                          enum kui_cover_size size, uint16_t *out) {
    if(record->source == KUI_COVER_SOURCE_DOWNLOAD) {
        char path[PATH_CAP];
        FIL compact;
        if(!kui_games_media_path(record->gdi_path,KUI_DOWNLOADED_COVERS,"kct",path,sizeof(path))) return false;
        if(record->user.bytes & OVERLAY_STAMP) {
            char name[COMPACT_PATH_CAP];
            struct cover_overlay slots;
            if(!overlay_path(path,name)) return false;
            FRESULT r=read_overlay(name,&slots);
            if(r==FR_OK) {
                unsigned n=slots.serial[1]>slots.serial[0] ? 1 : 0;
                for(unsigned attempt=0;attempt<2;++attempt,n=1-n) {
                    if(slots.serial[n]>(record->user.bytes & ~OVERLAY_STAMP)) continue;
                    r=open_slot(&slots,n,&compact,NULL,0);
                    if(r==FR_OK) {
                        bool ok=kui_compact_cover_pixels(&compact,size,out);
                        if(f_close(&compact)!=FR_OK) return false;
                        if(ok) return true;
                    } else if(!unavailable(r)) return false;
                }
            } else if(!unavailable(r)) return false;
        }
        if(kui_games_open_compact_direct(&compact,path)!=FR_OK) return false;
        bool ok = kui_compact_cover_pixels(&compact,size,out);
        return f_close(&compact) == FR_OK && ok;
    }
    unsigned edge = kui_cover_edge(size);
    UINT bytes = edge * edge * 2u, got = 0;
    if(f_lseek(file, (FSIZE_t)kui_cover_offset(size)) != FR_OK ||
       f_read(file, out, bytes, &got) != FR_OK || got != bytes) return false;
    /* Stored little-endian; each value is read before it is replaced. */
    const uint8_t *b = (const uint8_t *)out;
    for(unsigned i = 0; i < edge * edge; ++i) out[i] = (uint16_t)(b[2u * i] | b[2u * i + 1u] << 8);
    return true;
}

static bool preferred_pixels(const char *key, FIL *file, const struct kui_cover_record *record,
    enum kui_cover_size size, uint16_t *pixels, kui_log_fn log, kui_cancel_fn cancel, bool *cacheable);

static bool same_cover(const struct cached_cover *cover,const struct kui_games_entry *entry) {
    return cover->valid && !strcmp(cover->result.path,entry->path) && !strcmp(cover->result.name,entry->name);
}
struct view_job {unsigned requested,view;};
static void page_view(void *ctx,kui_log_fn log,kui_cancel_fn cancel) {
    struct view_job *job=ctx;
    unsigned saved=view_known?saved_view:view_load();
    job->view=job->requested<KUI_GAMES_VIEW_COUNT?job->requested:saved;
    if(!stopped(cancel) && job->view!=saved) (void)view_save(job->view,log);
}
bool kui_games_library_save_view(unsigned view,kui_log_fn log,kui_cancel_fn cancel) {
    if(view>=KUI_GAMES_VIEW_COUNT || stopped(cancel))return false;
    bool ok=kui_games_storage_open(log)==KUI_GAMES_STORAGE_OK;
    if(ok) {
        ok=!stopped(cancel) && view_save(view,log);
        if(!kui_games_storage_done(ok))ok=false;
    }
    /* This is the active in-memory selection, not a persistence receipt.
     * Failed writes must not make later row requests retry every idle pass. */
    saved_view=view;view_known=true;
    return ok && !stopped(cancel);
}
unsigned kui_games_library_view(unsigned view,kui_log_fn log,kui_cancel_fn cancel) {
    if(stopped(cancel))return KUI_GAMES_VIEW_SAVED;
    if(view_known && (view==KUI_GAMES_VIEW_SAVED || view==saved_view))return saved_view;
    if(kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK)return KUI_GAMES_VIEW_SAVED;
    struct view_job job={view,KUI_GAMES_VIEW_LIST};
    page_view(&job,log,cancel);
    bool ok=!stopped(cancel);
    if(!kui_games_storage_done(ok))ok=false;
    if(!ok)return KUI_GAMES_VIEW_SAVED;
    saved_view=job.view;view_known=true;return saved_view;
}
bool kui_games_list_rows(const char *root,unsigned offset,unsigned view,
    struct kui_games_page *out,kui_log_fn log,kui_cancel_fn cancel) {
    struct view_job job={view,view<KUI_GAMES_VIEW_COUNT?view:saved_view};
    bool needs_view=!view_known || (view<KUI_GAMES_VIEW_COUNT && view!=saved_view);
    bool ok=kui_games_list_with(root,offset,out,needs_view?page_view:NULL,&job,log,cancel);
    if(!out) return false;
    if(!ok) {out->view=KUI_GAMES_VIEW_SAVED;return false;}
    saved_view=job.view;view_known=true;out->view=saved_view;
    out->artwork=artwork_known && artwork_exists;
    for(unsigned row=0;row<out->count;row++) {
        struct kui_games_entry *entry=&out->entries[row];
        kui_cover_display_title(NULL,entry->name,entry->title);entry->cover=false;
        const struct cached_cover *latest=NULL;
        for(unsigned i=0;i<COVER_CACHE_SLOTS;i++) if(same_cover(&covers[i],entry) &&
            (!latest || covers[i].used>latest->used)) latest=&covers[i];
        if(latest) strcpy(entry->title,latest->result.title);
        else {
            const struct cached_title *known=title_find(entry);
            if(known) strcpy(entry->title,known->title);
        }
    }
    return true;
}
bool kui_games_list_covers(const char *root, unsigned offset, unsigned view,
    struct kui_games_page *out, uint16_t (*pixels)[KUI_COVER_PIXELS], kui_log_fn log, kui_cancel_fn cancel) {
    (void)pixels;
    return kui_games_list_rows(root,offset,view,out,log,cancel);
}
static void cover_drop(unsigned slot) {
    cover_bytes-=covers[slot].bytes;free(covers[slot].pixels);memset(&covers[slot],0,sizeof(covers[slot]));
}
static unsigned cover_oldest(void) {
    unsigned oldest=COVER_CACHE_SLOTS;
    for(unsigned i=0;i<COVER_CACHE_SLOTS;i++) if(covers[i].valid &&
        (oldest==COVER_CACHE_SLOTS || covers[i].used<covers[oldest].used)) oldest=i;
    return oldest;
}
static void cover_store(const struct kui_games_cover_result *result,const uint16_t *pixels) {
    unsigned edge=kui_cover_edge(result->size),bytes=result->cover?edge*edge*2u:0;
    uint16_t *copy=bytes?malloc(bytes):NULL;
    if(bytes && !copy) return;
    if(copy) memcpy(copy,pixels,bytes);
    while(cover_bytes+bytes>COVER_CACHE_BYTES) cover_drop(cover_oldest());
    unsigned slot=0;
    while(slot<COVER_CACHE_SLOTS && covers[slot].valid) ++slot;
    if(slot==COVER_CACHE_SLOTS) {slot=cover_oldest();cover_drop(slot);}
    covers[slot].result=*result;covers[slot].pixels=copy;covers[slot].bytes=bytes;
    covers[slot].used=++cover_clock;covers[slot].valid=true;cover_bytes+=bytes;
}
/* An owner can assign another game's downloaded PNG. Keep such existing
 * choices working after that managed PNG has been compacted. */
static bool migrated_assignment(char *path) {
    static const char prefix[] = "0:/kui/covers/downloads/";
    if(strncmp(path,prefix,sizeof(prefix)-1)) return false;
    char *name = path + sizeof(prefix)-1;
    if(strlen(name) != 36 || strcmp(name+32,".png") || strspn(name,"0123456789abcdef") != 32) return false;
    memcpy(name+32,".kct",5);
    return true;
}
static bool assigned_cover(const struct kui_games_entry *entry,enum kui_cover_size size,uint16_t *pixels,
    struct kui_games_cover_result *out,kui_log_fn log,kui_cancel_fn cancel) {
    char path[PATH_CAP],key[KUI_GAME_LAUNCH_SETTINGS_PATH_CAP];
    if(!memchr(entry->cover_path,0,sizeof(entry->cover_path)) ||
       !kui_game_launch_settings_normalize(key,entry->cover_path) || strcmp(key,entry->cover_path) ||
       kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK)return false;
    snprintf(path,sizeof(path),"0:%s",entry->cover_path);FIL file;
    kui_games_storage_lookup_begin();
    FRESULT r = stopped(cancel) ? FR_INVALID_OBJECT : f_open(&file,path,FA_READ);
    bool managed = (r == FR_NO_FILE || r == FR_NO_PATH) && migrated_assignment(path);
    if(managed) r = kui_games_open_compact(&file,path);
    bool ok = r == FR_OK;
    uint8_t *data=NULL,*decoded=NULL;
    unsigned width=0,height=0,channels=0;
    if(ok && managed) {
        ok = kui_compact_cover_pixels(&file,size,pixels);
        if(f_close(&file) != FR_OK) ok = false;
    } else if(ok) {
        size_t bytes=(size_t)f_size(&file);data=bytes && bytes<=KUI_COVER_IMAGE_MAX_BYTES?malloc(bytes):NULL;UINT got=0;
        ok=data && f_read(&file,data,(UINT)bytes,&got)==FR_OK && got==bytes;
        if(f_close(&file)!=FR_OK)ok=false;
        if(ok && !stopped(cancel))ok=kui_cover_image_decode(data,bytes,&decoded,&width,&height,&channels)==KUI_COVER_IMAGE_OK;
        else ok=false;
    }
    if(ok && !managed) {kui_cover_reduce(decoded,&width,&height,channels);ok=kui_cover_scale(decoded,width,height,channels,pixels,kui_cover_edge(size));}
    kui_cover_image_free(decoded);free(data);
    kui_games_storage_lookup_end();
    if(!kui_games_storage_done(ok || stopped(cancel)))ok=false;
    if(stopped(cancel)) {out->stopped=true;ok=false;}
    out->cover=ok;
    if(ok) {cover_store(out,pixels);title_store(entry,out->title);}
    else if(log && !out->stopped)log("Assigned cover unavailable for %.72s; choose another image",entry->name);
    return ok;
}
bool kui_games_selected_cover(const struct kui_games_entry *entry,enum kui_cover_size size,
    uint16_t pixels[KUI_COVER_PIXELS],struct kui_games_cover_result *out,kui_log_fn log,kui_cancel_fn cancel) {
    if(!out) return false;
    memset(out,0,sizeof(*out));out->size=size;
    if(!entry || !pixels || (unsigned)size>KUI_COVER_SIZE_SMALL ||
       !memchr(entry->name,0,sizeof(entry->name)) || !memchr(entry->path,0,sizeof(entry->path))) return false;
    strcpy(out->name,entry->name);strcpy(out->path,entry->path);
    kui_cover_display_title(NULL,entry->name,out->title);
    if(entry->custom_title)snprintf(out->title,sizeof(out->title),"%s",entry->title);
    if(stopped(cancel)) {out->stopped=true;return false;}
    if(entry->directory || entry->disabled) return true;
    for(unsigned i=0;i<COVER_CACHE_SLOTS;i++) if(same_cover(&covers[i],entry) && covers[i].result.size==size) {
        *out=covers[i].result;covers[i].used=++cover_clock;
        if(out->cover) memcpy(pixels,covers[i].pixels,covers[i].bytes);
        return true;
    }
    if(entry->cover_path[0])return assigned_cover(entry,size,pixels,out,log,cancel);
    char key[KUI_COVER_PATH_CAP];
    if(!kui_cover_key(entry->name,key)) return false;
    bool mounted=false,opened=false,ok=false,observed=false,exists=false,cacheable=true;
    FIL file;
    const char *problem="SD card unavailable";
    enum kui_games_storage_result storage=kui_games_storage_open(log);
    if(storage==KUI_GAMES_STORAGE_NO_CARD) goto done;
    problem="Cannot mount SD card";
    if(!(mounted=storage==KUI_GAMES_STORAGE_OK)) goto done;
    kui_games_storage_lookup_begin();
    if(stopped(cancel)) goto done;
    if(!artwork_known) {
        FILINFO info;FRESULT r=f_stat(KUI_GAMES_COVERS_FOLDER,&info);
        observed=r==FR_OK || r==FR_NO_FILE || r==FR_NO_PATH;
        exists=r==FR_OK && (info.fattrib&AM_DIR);
    }
    struct kui_cover_record record;
    bool backup;
    FRESULT r=selector_record(key,entry->path,&file,&record,&backup,false,log);
    if(r==FR_NO_FILE || r==FR_NO_PATH) {ok=true;goto done;}
    problem="Cannot read selected box art";
    if(r!=FR_OK) goto done;
    opened=true;
    if(stopped(cancel)) goto done;
    if(!entry->custom_title)kui_cover_display_title(record.title,entry->name,out->title);
    if(record.source!=KUI_COVER_SOURCE_NONE) {
        if(stopped(cancel) || !preferred_pixels(key,&file,&record,size,pixels,log,cancel,&cacheable)) goto done;
        out->cover=true;
    }
    ok=true;
done:
    if(opened && f_close(&file)!=FR_OK) ok=false;
    if(mounted) kui_games_storage_lookup_end();
    if(mounted && !kui_games_storage_done(ok || stopped(cancel))) ok=false;
    if(stopped(cancel)) {ok=false;out->stopped=true;}
    if(ok) {
        if(observed) {artwork_known=true;artwork_exists=exists;}
        if(out->cover) {artwork_known=artwork_exists=true;}
        if(cacheable) cover_store(out,pixels);
        title_store(entry,out->title);
    } else {
        out->cover=false;
        if(log && !out->stopped) log("Games cover %.72s: %s",entry->name,problem);
    }
    return ok;
}

bool kui_games_prefetch_titles(const struct kui_games_page *page,kui_log_fn log,kui_cancel_fn cancel) {
    if(!page) return false;
    bool opened=false,worked=false,failed=false;
    for(unsigned row=0;row<page->count && row<KUI_GAMES_ROWS && !failed;row++) {
        const struct kui_games_entry *entry=&page->entries[row];
        if(entry->directory || entry->disabled || !memchr(entry->name,0,sizeof(entry->name)) ||
           !memchr(entry->path,0,sizeof(entry->path)) || title_find(entry)) continue;
        bool cached=false;
        for(unsigned i=0;i<COVER_CACHE_SLOTS && !cached;i++) cached=same_cover(&covers[i],entry);
        char key[KUI_COVER_PATH_CAP];
        if(cached || !kui_cover_key(entry->name,key)) continue;
        if(stopped(cancel)) break;
        if(!opened) {
            if(kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK) return worked;
            opened=true;kui_games_storage_lookup_begin();
        }
        char title[KUI_COVER_TITLE_CAP],region[9]={0};kui_cover_display_title(NULL,entry->name,title);
        FIL file;struct kui_cover_record record;bool backup;
        FRESULT r=selector_record(key,entry->path,&file,&record,&backup,false,log);
        bool ok=r==FR_NO_FILE || r==FR_NO_PATH;
        if(r==FR_OK) {
            ok=true;
            kui_cover_display_title(record.title,entry->name,title);strcpy(region,record.region);
            if(f_close(&file)!=FR_OK) ok=false;
        }
        if(!ok) {
            failed=true;
            if(log) log("Games title %.72s: cannot read box art record",entry->name);
        } else {title_store(entry,title);struct cached_title *stored=title_find(entry);if(stored)strcpy(stored->region,region);worked=true;}
    }
    if(opened) {kui_games_storage_lookup_end();(void)kui_games_storage_done(!failed);}
    return worked;
}

struct detail_job { const char *path; struct kui_games_detail *detail; uint16_t *pixels; };
static void detail_cover(void *ctx, kui_log_fn log, kui_cancel_fn cancel) {
    struct detail_job *job = ctx;
    const char *slash = strrchr(job->path, '/');
    if(!slash || !job->pixels) return;
    /* A game folder names its record; a GDI sharing a folder names its own. */
    const char *start = slash;
    while(start > job->path && start[-1] != '/') --start;
    char folder[KUI_COVER_PATH_CAP] = "";
    size_t n = (size_t)(slash - start);
    if(n < sizeof(folder)) { memcpy(folder, start, n); folder[n] = 0; }
    const char *names[2] = {folder, slash + 1};
    for(unsigned k = 0; k < 2 && !stopped(cancel); ++k) {
        char key[KUI_COVER_PATH_CAP];
        FIL file;
        struct kui_cover_record record;
        bool backup;
        if(!kui_cover_key(names[k], key) ||
           selector_record(key,job->path,&file,&record,&backup,false,log)!=FR_OK) continue;
        if(record.source != KUI_COVER_SOURCE_NONE) {
            bool cacheable = true;
            job->detail->cover = preferred_pixels(key, &file, &record, KUI_COVER_SIZE_LARGE,
                job->pixels, log, cancel, &cacheable);
        }
        f_close(&file);
        return;
    }
}
bool kui_games_inspect_cover(const char *path, struct kui_games_detail *out,
    uint16_t pixels[KUI_COVER_PIXELS], kui_log_fn log, kui_cancel_fn cancel) {
    return kui_games_inspect_cover_codec(path,KUI_ZSO_LZ4,out,pixels,log,cancel);
}
bool kui_games_inspect_cover_codec(const char *path,enum kui_zso_codec codec,struct kui_games_detail *out,
    uint16_t pixels[KUI_COVER_PIXELS],kui_log_fn log,kui_cancel_fn cancel) {
    struct detail_job job = {path,out,pixels};
    return kui_games_inspect_with_codec(path,codec,out,detail_cover,&job,log,cancel);
}

/* ---- The scan ---- */
struct scan {
    kui_log_fn log;
    kui_cancel_fn cancel;
    struct kui_games_scan_counts *counts;
    /* Found games as "key\0path\0" pairs. */
    char *names;
    size_t used, capacity;
    unsigned found;
    uint8_t *record, *raw;       /* One record file; sixteen raw sectors. */
    uint16_t *scaled;
    bool disc_only; /* Build a disposable secondary copy without touching the primary record. */
    uint64_t overlay_generation;
    bool compact_verified; /* Single-game internet scan has checked the master. */
    struct kui_games_disc_identity *identity;
};
static bool same_name(const char *a, const char *b) {
    for(;; ++a, ++b) {
        int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if(x != y) return false;
        if(!x) return true;
    }
}
static const char *nth_path(const char *key) { return key + strlen(key) + 1u; }
static void add_game(struct scan *s, const char *name, const char *path) {
    char key[KUI_COVER_PATH_CAP];
    if(!kui_cover_key(name, key)) {
        if(s->log) s->log("Box art: no cover name possible for %s", path);
        ++s->counts->failed;
        return;
    }
    size_t need = strlen(key) + strlen(path) + 2u;
    if(s->found >= KUI_GAMES_SCAN_MAX) { ++s->counts->failed; return; }
    if(s->used + need > s->capacity) {
        size_t capacity = s->capacity ? s->capacity * 2u : 8192u;
        while(capacity < s->used + need) capacity *= 2u;
        char *names = realloc(s->names, capacity);
        if(!names) { ++s->counts->failed; return; }
        s->names = names;
        s->capacity = capacity;
    }
    memcpy(s->names + s->used, key, strlen(key) + 1u);
    memcpy(s->names + s->used + strlen(key) + 1u, path, strlen(path) + 1u);
    s->used += need;
    ++s->found;
}
/* Shares the Games payload filter: a folder with one image takes the folder
 * name; other folders are categories; loose selectors take their filename. */
struct walk_job {struct scan *scan;const char *folder;unsigned depth;};
static bool walk(struct scan *s,const char *folder,unsigned depth);
static bool walk_entry(void *ctx,const char *name,bool directory) {
    struct walk_job *job=ctx;struct scan *s=job->scan;
    if(stopped(s->cancel)) return false;
    if(directory) {
        char child[KUI_DEST_ROOT_CAP],image[KUI_GAMES_FILE_CAP];
        if(!kui_destination_join(child,job->folder,name)) return true;
        if(kui_games_single_image(child,image,s->cancel)) add_game(s,name,image);
        else if(job->depth+1u<KUI_GAMES_SCAN_DEPTH && !walk(s,child,job->depth+1u)) {
            if(stopped(s->cancel)) return false;
            ++s->counts->failed;if(s->log) s->log("Box art: %s: cannot read image folder",child);
        }
    } else {
        char image[KUI_GAMES_FILE_CAP];
        int n=snprintf(image,sizeof(image),"%s%s%s",job->folder,strcmp(job->folder,"/")?"/":"",name);
        if(n>0 && n<(int)sizeof(image)) add_game(s,name,image);
    }
    return !stopped(s->cancel);
}
static bool walk(struct scan *s,const char *folder,unsigned depth) {
    struct walk_job job={s,folder,depth};
    return kui_games_visit_images(folder,walk_entry,&job,s->cancel);
}

/* Track reads keep the current file open: reopening would walk its
 * allocation chain again for every chunk. */
struct image_files {
    char root[KUI_GAMES_FILE_CAP], open_name[KUI_GAME_NAME_CAP];
    kui_cancel_fn cancel;
    FIL reader;
    bool reader_open;
};
static bool image_join(const struct image_files *f, const char *name, char out[PATH_CAP]) {
    if(!kui_destination_name_valid(name)) return false;
    int n = snprintf(out, PATH_CAP, "0:%s%s%s", f->root, strcmp(f->root, "/") ? "/" : "", name);
    return n > 0 && n < (int)PATH_CAP;
}
static enum kui_game_result image_stat(void *ctx, const char *name, uint64_t *bytes) {
    struct image_files *f = ctx;
    char path[PATH_CAP];
    FILINFO info;
    if(stopped(f->cancel)) return KUI_GAME_CANCELLED;
    if(!image_join(f, name, path)) return KUI_GAME_INVALID;
    FRESULT r = f_stat(path, &info);
    if(r != FR_OK) return r == FR_NO_FILE || r == FR_NO_PATH ? KUI_GAME_NOT_FOUND : KUI_GAME_IO;
    if(info.fattrib & AM_DIR) return KUI_GAME_FILE_SIZE;
    *bytes = info.fsize;
    return KUI_GAME_OK;
}
static bool image_close(struct image_files *f) {
    if(!f->reader_open) return true;
    f->reader_open = false;
    return f_close(&f->reader) == FR_OK;
}
static enum kui_game_result image_read(void *ctx, const char *name, uint64_t offset, void *out, size_t bytes) {
    struct image_files *f = ctx;
    char path[PATH_CAP];
    if(stopped(f->cancel)) return KUI_GAME_CANCELLED;
    if(!image_join(f, name, path) || bytes > UINT_MAX || (uint64_t)(FSIZE_t)offset != offset)
        return KUI_GAME_INVALID;
    if(f->reader_open && strcmp(f->open_name, name) && !image_close(f)) return KUI_GAME_IO;
    if(!f->reader_open) {
        if(f_open(&f->reader, path, FA_READ) != FR_OK) return KUI_GAME_IO;
        f->reader_open = true;
        snprintf(f->open_name, sizeof(f->open_name), "%s", name);
    }
    FIL *file = &f->reader;
    bool ok = offset <= f_size(file) && bytes <= f_size(file) - offset;
    UINT got = 0;
    if(ok && f_tell(file) != offset) ok = f_lseek(file, (FSIZE_t)offset) == FR_OK && f_tell(file) == offset;
    if(ok) ok = f_read(file, out, (UINT)bytes, &got) == FR_OK && got == bytes;
    return stopped(f->cancel) ? KUI_GAME_CANCELLED : ok ? KUI_GAME_OK : KUI_GAME_IO;
}
static enum kui_game_metadata_io_result metadata_sector(void *ctx, uint32_t lba, uint8_t out[2048]) {
    enum kui_game_result r = kui_game_image_read(ctx, lba, 1, KUI_GAME_SECTOR_MODE1, out, 2048);
    return r == KUI_GAME_OK ? KUI_GAME_METADATA_IO_OK :
        r == KUI_GAME_CANCELLED ? KUI_GAME_METADATA_IO_CANCELLED : KUI_GAME_METADATA_IO_ERROR;
}
static bool metadata_range(void *ctx, uint32_t lba, uint32_t count) {
    return kui_game_image_check(ctx, lba, count, KUI_GAME_SECTOR_MODE1) == KUI_GAME_OK;
}

enum outcome { OUT_UNCHANGED, OUT_DISC, OUT_USER, OUT_NONE, OUT_FAILED };
static bool read_file(const char *path, uint64_t limit, uint8_t **data, size_t *size) {
    FIL file;
    *data = NULL;
    *size = 0;
    if(f_open(&file, path, FA_READ) != FR_OK) return false;
    bool ok = f_size(&file) && f_size(&file) <= limit;
    if(ok) {
        *size = (size_t)f_size(&file);
        *data = malloc(*size);
        UINT got = 0;
        ok = *data && f_read(&file, *data, (UINT)*size, &got) == FR_OK && got == *size;
    }
    if(f_close(&file) != FR_OK) ok = false;
    if(!ok) { free(*data); *data = NULL; *size = 0; }
    return ok;
}
static void stamp_of(const FILINFO *info, struct kui_cover_stamp *stamp) {
    stamp->bytes = info->fsize;
    stamp->date = info->fdate;
    stamp->time = info->ftime;
}
static bool same_stamp(const struct kui_cover_stamp *a, const struct kui_cover_stamp *b) {
    return a->bytes == b->bytes && a->date == b->date && a->time == b->time;
}
/* Scales one decoded picture into the record's three sizes. */
static bool fill_sizes(struct scan *s, uint8_t *pixels, unsigned width, unsigned height, unsigned channels) {
    kui_cover_reduce(pixels, &width, &height, channels);
    for(unsigned z = 0; z < 3; ++z) {
        enum kui_cover_size size = (enum kui_cover_size)z;
        unsigned edge = kui_cover_edge(size);
        if(!kui_cover_scale(pixels, width, height, channels, s->scaled, edge)) return false;
        uint8_t *out = s->record + kui_cover_offset(size);
        for(unsigned i = 0; i < edge * edge; ++i) {
            out[2u * i] = (uint8_t)s->scaled[i];
            out[2u * i + 1u] = (uint8_t)(s->scaled[i] >> 8);
        }
    }
    return true;
}
static bool user_art(struct scan *s, const char *key, const char *path) {
    uint8_t *data = NULL, *pixels = NULL;
    size_t size = 0;
    unsigned w = 0, h = 0, channels = 0;
    if(!read_file(path, KUI_COVER_IMAGE_MAX_BYTES, &data, &size)) {
        if(s->log) s->log("Box art: %s: cannot read %s (over 3 MB or unreadable)", key, path + 2);
        return false;
    }
    enum kui_cover_image_status status = kui_cover_image_decode(data, size, &pixels, &w, &h, &channels);
    free(data);
    if(status != KUI_COVER_IMAGE_OK) {
        if(s->log) s->log("Box art: %s: %s: %s", key, path + 2, kui_cover_image_status_text(status));
        return false;
    }
    bool ok = fill_sizes(s, pixels, w, h, channels);
    kui_cover_image_free(pixels);
    return ok;
}
/* Read cooked user bytes through the image adapter, preserving address checks
 * for the original raw GDI path. False with *io set means a read problem. */
static bool disc_art(struct scan *s, const char *key, const struct kui_game_image *image,
                     uint32_t lba, uint32_t bytes, bool *io) {
    uint32_t sectors = (bytes + 2047u) / 2048u;
    uint8_t *file = malloc((size_t)sectors * 2048u);
    if(!file) { *io = true; return false; }
    for(uint32_t done = 0; done < sectors;) {
        uint32_t n = sectors - done > 16u ? 16u : sectors - done;
        bool raw=image->format==KUI_GAME_IMAGE_GDI &&
            kui_game_image_check(image,lba+done,n,KUI_GAME_SECTOR_RAW)==KUI_GAME_OK;
        enum kui_game_result r = kui_game_image_read(image, lba + done, n,
            raw?KUI_GAME_SECTOR_RAW:KUI_GAME_SECTOR_MODE1,s->raw,
            (size_t)n*(raw?KUI_GAME_RAW_BYTES:KUI_GAME_DATA_BYTES));
        if(r != KUI_GAME_OK) {
            *io = r == KUI_GAME_IO || r == KUI_GAME_CANCELLED;
            if(s->log) s->log("Box art: %s: artwork unreadable: %s", key, kui_game_result_name(r));
            free(file);
            return false;
        }
        if(!raw) memcpy(file+(size_t)done*2048u,s->raw,(size_t)n*2048u);
        else for(uint32_t k = 0; k < n; ++k) {
            const uint8_t *sector = s->raw + (size_t)k * KUI_GAME_RAW_BYTES;
            if(kui_retail_sector_header(sector, lba + done + k) != KUI_RETAIL_HEADER_OK) {
                if(s->log) s->log("Box art: %s: artwork sector %lu is not data", key, (unsigned long)(lba + done + k));
                free(file);
                return false;
            }
            memcpy(file + (size_t)(done + k) * 2048u, sector + 16, 2048);
        }
        done += n;
    }
    struct kui_pvr_info info;
    enum kui_pvr_status status = kui_pvr_parse(file, bytes, &info);
    uint8_t *rgba = status == KUI_PVR_OK ? malloc((size_t)info.width * info.height * 4u) : NULL;
    if(status == KUI_PVR_OK && !rgba) { free(file); *io = true; return false; }
    if(status == KUI_PVR_OK) status = kui_pvr_decode(file, bytes, &info, rgba);
    free(file);
    bool ok = status == KUI_PVR_OK && fill_sizes(s, rgba, info.width, info.height, 4);
    if(status != KUI_PVR_OK && s->log) s->log("Box art: %s: %s", key, kui_pvr_status_text(status));
    else if(ok && s->log) s->log("Box art: %s: disc artwork %ux%u%s", key, info.width, info.height, info.vq ? " VQ" : "");
    free(rgba);
    return ok;
}
static FRESULT write_exact(FIL *file, const void *data, UINT bytes) {
    UINT wrote = 0;
    FRESULT r = f_write(file, data, bytes, &wrote);
    return r == FR_OK && wrote != bytes ? FR_DISK_ERR : r;
}
static FRESULT close_record(FIL *file, FRESULT r) {
    FRESULT closed = f_close(file);
    return r == FR_OK ? closed : r;
}
/* Validate every source plane before exposing new pixels. A cancelled or
 * corrupt transfer never participates in selecting the next active slot. */
static bool checked_compact(const char *path,kui_cancel_fn cancel) {
    FIL file;
    if(kui_games_open_compact_direct(&file,path)!=FR_OK) return false;
    bool ok=kui_compact_cover_verify(&file,cancel)==KUI_COMPACT_OK;
    return f_close(&file)==FR_OK && ok && !stopped(cancel);
}
static FRESULT verify_slot(const struct cover_overlay *slots,unsigned n,kui_cancel_fn cancel) {
    FIL file;
    FRESULT r=open_slot(slots,n,&file,NULL,0);
    if(r!=FR_OK) return r;
    enum kui_compact_result result=kui_compact_cover_verify(&file,cancel);
    if(f_close(&file)!=FR_OK || result==KUI_COMPACT_ERROR) return FR_DISK_ERR;
    return result==KUI_COMPACT_OK ? FR_OK : FR_INVALID_OBJECT;
}
static FRESULT verified_slot(const struct cover_overlay *slots,unsigned *slot,kui_cancel_fn cancel) {
    unsigned n=slots->serial[1]>slots->serial[0] ? 1 : 0;
    FRESULT r=verify_slot(slots,n,cancel);
    if(unavailable(r)) {n=1-n; r=verify_slot(slots,n,cancel);}
    if(r==FR_OK) *slot=n;
    return r;
}
/* Explicit scans validate all RGB565 planes. The catalogue remembers this
 * generation, keeping untouched selectors' ordinary art reads on one path. */
static FRESULT overlay_generation(const char *base,uint64_t *generation,kui_cancel_fn cancel) {
    char path[COMPACT_PATH_CAP];
    if(!overlay_path(base,path)) return FR_INVALID_NAME;
    struct cover_overlay slots;
    FRESULT r=read_overlay(path,&slots);
    if(r!=FR_OK) return r;
    unsigned n;
    r=verified_slot(&slots,&n,cancel);
    if(r==FR_OK) *generation=slots.serial[n];
    return r;
}
static FRESULT seek_slot(FIL *file,FSIZE_t offset) {
    /* FatFs can return FR_OK with a shorter seek when a volume is full. */
    FRESULT r=f_lseek(file,offset);
    return r==FR_OK && f_tell(file)!=offset ? FR_DISK_ERR : r;
}
static void forget_slot(const char *path,unsigned n) {
    FIL f;
    if(f_open(&f,path,FA_WRITE)!=FR_OK) return;
    if(seek_slot(&f,n*OVERLAY_STRIDE)==FR_OK && write_exact(&f,"",1)==FR_OK) (void)f_sync(&f);
    (void)f_close(&f);
}
static bool publish_overlay(const char *staged,const char *final,kui_cancel_fn cancel) {
    FIL f;
    if(f_open(&f,staged,FA_READ)!=FR_OK) return false;
    char key[33];
    FRESULT r=link_key(&f,key);
    if(f_close(&f)!=FR_OK || r!=FR_OK) return false;
    struct cover_overlay slots;
    r=read_overlay(final,&slots);
    if(r==FR_NO_FILE) memset(&slots,0,sizeof(slots));
    else if(r!=FR_OK) return false;
    unsigned active=1;
    r=verified_slot(&slots,&active,cancel);
    if(!unavailable(r) && r!=FR_OK) return false;
    if(stopped(cancel)) return false;
    if(r==FR_OK && !strcmp(slots.key[active],key)) return true;
    unsigned n=1-active;
    uint64_t last=slots.serial[0]>slots.serial[1] ? slots.serial[0] : slots.serial[1];
    if(last>=OVERLAY_STAMP-1) return false;
    uint64_t next=last+1;
    uint8_t data[OVERLAY_SLOT]="KCU1";
    for(unsigned i=0;i<8;i++) data[4+i]=(uint8_t)(next>>(8*i));
    memcpy(data+12,key,32);
    uint32_t crc=kui_crc32_capture(0,data,44);
    for(unsigned i=0;i<4;i++) data[44+i]=(uint8_t)(crc>>(8*i));

    r=f_open(&f,final,FA_READ|FA_WRITE|FA_OPEN_ALWAYS);
    if(r!=FR_OK) return false;
    if(f_size(&f)<2*OVERLAY_STRIDE) {
        /* FatFs leaves seek-created gaps undefined. Explicitly clear every
         * new byte so a slot from a previously freed cluster can never win. */
        static const uint8_t empty[OVERLAY_STRIDE]={0};
        r=seek_slot(&f,f_size(&f));
        while(r==FR_OK && f_tell(&f)<2*OVERLAY_STRIDE) {
            FSIZE_t left=2*OVERLAY_STRIDE-f_tell(&f);
            r=write_exact(&f,empty,left<sizeof(empty)?(UINT)left:(UINT)sizeof(empty));
        }
        if(r==FR_OK) r=f_sync(&f);
    }
    if(r==FR_OK && stopped(cancel)) r=FR_DENIED;
    if(r==FR_OK) r=seek_slot(&f,n*OVERLAY_STRIDE);
    bool attempted=r==FR_OK;
    if(attempted) r=write_exact(&f,data,sizeof(data));
    if(r==FR_OK) r=f_sync(&f);
    if(f_close(&f)!=FR_OK) r=FR_DISK_ERR;
    if(r==FR_OK && !stopped(cancel)) {
        r=read_overlay(final,&slots);
        if(r==FR_OK && slots.serial[n]==next && slots.serial[1-n]<next &&
           !strcmp(slots.key[n],key)) return true;
    }
    if(attempted) forget_slot(final,n);
    return false;
}
bool kui_games_publish_cover(const char *staged,const char *final,kui_cancel_fn cancel) {
    if(!staged || !final || !strcmp(staged,final) || stopped(cancel)) return false;
    bool overlay=managed_name(final,".kcu");
    if(!overlay && !managed_name(final,".kct")) return false;
    if(!checked_compact(staged,cancel)) return false;
    if(overlay) {
        if(!publish_overlay(staged,final,cancel)) return false;
        (void)f_unlink(staged);
    } else {
        /* A native master may already serve other discs. FAT's non-replacing
         * rename protects it even if a later selector lost its group. */
        if(f_rename(staged,final)!=FR_OK) return false;
    }
    kui_games_covers_content_clear();
    return true;
}
/* The source is a complete, validated primary. Before editing it, copy its
 * bytes to independent clusters. A failed backup leaves the primary intact. */
static FRESULT backup_record(FIL *source, const char *backup, uint8_t *scratch) {
    FIL out;
    FRESULT r = f_lseek(source, 0);
    if(r != FR_OK) return r;
    r = f_open(&out, backup, FA_CREATE_ALWAYS | FA_WRITE);
    if(r != FR_OK) return r;
    while(r == FR_OK && f_tell(source) < f_size(source)) {
        FSIZE_t left = f_size(source) - f_tell(source);
        UINT chunk = left < 16u * KUI_GAME_RAW_BYTES ? (UINT)left : 16u * KUI_GAME_RAW_BYTES;
        UINT got = 0;
        r = f_read(source, scratch, chunk, &got);
        if(r == FR_OK) r = got == chunk ? write_exact(&out, scratch, chunk) : FR_DISK_ERR;
    }
    return close_record(&out, r);
}
static FRESULT replace_record(const char *path, const uint8_t *data, UINT bytes) {
    static const uint8_t pending[KUI_COVER_HEADER_BYTES] = {0};
    FIL file;
    FRESULT r = f_open(&file, path, FA_OPEN_EXISTING | FA_WRITE);
    if(r != FR_OK) return r;
    r = write_exact(&file, pending, sizeof(pending));
    if(r == FR_OK) r = f_sync(&file); /* readers must see fallback before any pixel changes */
    if(r == FR_OK && bytes > sizeof(pending))
        r = write_exact(&file, data + sizeof(pending), bytes - sizeof(pending));
    if(r == FR_OK && f_size(&file) != bytes) r = f_truncate(&file);
    if(r == FR_OK) r = f_sync(&file);
    if(r == FR_OK) r = f_lseek(&file, 0);
    if(r == FR_OK) r = write_exact(&file, data, sizeof(pending));
    return close_record(&file, r);
}
static bool record_write(struct scan *s, const char *key, const struct kui_cover_record *record) {
    char final[PATH_CAP], backup[PATH_CAP];
    if(!record_path(key, ".kcv", final) || !record_path(key, ".kcb", backup) ||
       !kui_cover_header_encode(s->record, record)) return false;
    UINT size = record_bytes(record->source);
    FIL file;
    struct kui_cover_record old;
    bool invalid;
    FRESULT r = record_read_at(final, &file, &old, &invalid);
    /* The replacement already makes a verified old-record copy. Retain its
     * embedded disc art when an internet scan first installs box art. */
    bool keep_disc = r == FR_OK && old.source == KUI_COVER_SOURCE_DISC &&
        record->source == KUI_COVER_SOURCE_DOWNLOAD && !strcmp(old.gdi_path, record->gdi_path) &&
        same_stamp(&old.gdi, &record->gdi);
    if(r == FR_NO_FILE) {
        /* First publication has no earlier copy to protect. */
        char temp[PATH_CAP];
        if(!record_path(key, ".new", temp)) return false;
        r = f_open(&file, temp, FA_WRITE | FA_CREATE_ALWAYS);
        if(r == FR_OK) {
            r = close_record(&file, write_exact(&file, s->record, size));
            if(r == FR_OK) r = f_rename(temp, final);
            if(r != FR_OK) f_unlink(temp);
        }
    } else {
        if(r == FR_OK) r = close_record(&file, backup_record(&file, backup, s->raw));
        else if(invalid) r = FR_OK; /* retain any backup from an interrupted replacement */
        if(r == FR_OK) r = replace_record(final, s->record, size);
        if(r == FR_OK) {
            char disc[PATH_CAP];
            if(keep_disc && record_path(key, ".kcd", disc) && f_rename(backup, disc) == FR_OK) return true;
            FILINFO info;
            if(f_stat(backup, &info) == FR_OK && !(info.fattrib & AM_DIR)) (void)f_unlink(backup);
        }
    }
    if(r != FR_OK && s->log) s->log("Box art: %s: cannot save cover: FatFs=%u", key, (unsigned)r);
    return r == FR_OK;
}
static bool media_identity(const struct kui_game_metadata *meta,struct kui_games_disc_identity *out) {
    if(!(meta->native_gd || meta->native_cd || meta->windows_ce) ||
       !meta->title[0] || !meta->product[0] || !meta->region[0]) return false;
    snprintf(out->title, sizeof(out->title), "%s", meta->title);
    snprintf(out->product, sizeof(out->product), "%s", meta->product);
    snprintf(out->version, sizeof(out->version), "%s", meta->version);
    snprintf(out->region, sizeof(out->region), "%s", meta->region);
    out->number = meta->disc_number;
    out->total = meta->disc_total;
    return true;
}
static enum outcome build(struct scan *s, const char *name, const char *gdi_path) {
    char key[KUI_COVER_PATH_CAP];
    snprintf(key, sizeof(key), "%s", name);
    const char *slash = strrchr(gdi_path, '/');
    if(!slash || !kui_game_image_name_supported(slash+1)) {
        if(s->log) s->log("Box art: %s: compressed image requires offline import",key);
        return OUT_FAILED;
    }
    size_t leaf_bytes=strlen(slash+1);
    bool zso=leaf_bytes>=4u && same_name(slash+1+leaf_bytes-4u,".zso");
    struct kui_game_launch_settings choice;
    kui_game_launch_settings_default(&choice);
    /* The scan already owns the mounted volume. An unread preference file
     * is not permission to guess the codec or overwrite its cover record. */
    if(zso && !kui_game_launch_settings_load(gdi_path,&choice,s->log))return OUT_FAILED;
    struct kui_cover_record record;
    memset(&record, 0, sizeof(record));
    char path[PATH_CAP], user[PATH_CAP] = "";
    FILINFO info;
    snprintf(path, sizeof(path), "0:%s", gdi_path);
    if(strlen(gdi_path) >= sizeof(record.gdi_path) || f_stat(path, &info) != FR_OK) return OUT_FAILED;
    strcpy(record.gdi_path, gdi_path);
    if(zso && s->disc_only) record.user.bytes = 1u + (unsigned)choice.codec;
    stamp_of(&info, &record.gdi);
    struct kui_cover_record old;
    bool current = false;
    if(!s->disc_only) {
        FIL file;
        bool backup;
        FRESULT found = selector_record(key, gdi_path, &file, &old, &backup, true, s->log);
        if(found == FR_OK) {
            if(f_close(&file) != FR_OK) return OUT_FAILED;
            current = !backup;
        } else if(found != FR_NO_FILE && !(found == FR_INVALID_OBJECT && backup)) return OUT_FAILED;
        if(s->counts && strchr(key, '/')) ++s->counts->duplicates;
    }
    /* Explicit owner images take priority over internet box art. All
     * images feed the same thumbnail cache, including on later rescans. */
    unsigned owner_count = sizeof(user_images) / sizeof(user_images[0]);
    bool compact = false;
    for(unsigned i = 0; !s->disc_only && i < owner_count + 2; ++i) {
        if(i < owner_count) {
            if(!record_path(key, user_images[i], user)) return OUT_FAILED;
        } else if(!kui_games_media_path(gdi_path, KUI_DOWNLOADED_COVERS,
                i == owner_count ? "kct" : "png", user, sizeof(user))) {
            user[0] = 0;
            break; /* Offline cover scans also accept some legacy image paths. */
        }
        if(i==owner_count) {
            uint64_t generation=s->overlay_generation;
            if(!s->compact_verified) {
                FRESULT r=overlay_generation(user,&generation,s->cancel);
                if(r!=FR_OK && !unavailable(r)) return OUT_FAILED;
            }
            if(generation) {
                record.user.bytes=OVERLAY_STAMP | generation;
                compact=true;
                break;
            }
        }
        FRESULT art_stat = f_stat(user, &info);
        if(art_stat == FR_OK && !(info.fattrib & AM_DIR)) {
            compact = i == owner_count;
            stamp_of(&info, &record.user);
            break;
        }
        if(art_stat != FR_OK && art_stat != FR_NO_FILE && art_stat != FR_NO_PATH) return OUT_FAILED;
        user[0] = 0;
    }
    bool check_old_title = false;
    if(current) {
        /* Old records do not carry a codec. Recheck ZSO during explicit
         * scans so a changed preference never reuses another decoder's
         * result; selected-art RAM/disk caches remain unchanged. Records
         * with a missing or legacy question-mark title also need a retry. */
        bool pending_download = !strncmp(user, KUI_DOWNLOADED_COVERS "/", sizeof(KUI_DOWNLOADED_COVERS)) &&
            old.source != (compact ? KUI_COVER_SOURCE_DOWNLOAD : KUI_COVER_SOURCE_USER);
        if(!zso && !pending_download && !(old.source == KUI_COVER_SOURCE_DOWNLOAD && !compact) &&
           old.title[0] && !strcmp(old.gdi_path, gdi_path) &&
           same_stamp(&old.gdi, &record.gdi) && same_stamp(&old.user, &record.user)) {
            if(!strchr(old.title, '?')) return OUT_UNCHANGED;
            check_old_title = true;
        }
    }
    /* Bounded image metadata, then IP and the root directory for title/art. */
    struct image_files files = {.cancel = s->cancel};
    size_t root = (size_t)(slash - gdi_path);
    if(root >= sizeof(files.root)) return OUT_FAILED;
    if(root) { memcpy(files.root, gdi_path, root); files.root[root] = 0; } else strcpy(files.root, "/");
    struct kui_game_image *image = malloc(sizeof(*image));
    bool io = false, have_image = false, art = false;
    struct kui_game_metadata meta;
    memset(&meta, 0, sizeof(meta));
    if(!image) io = true;
    else {
        struct kui_game_file_ops ops = {&files, image_stat, image_read};
        enum kui_game_result r = kui_game_image_open_named_codec(slash+1,&ops,(enum kui_zso_codec)choice.codec,image);
        io = r == KUI_GAME_IO || r == KUI_GAME_CANCELLED || r == KUI_GAME_NOT_FOUND;
        have_image = r == KUI_GAME_OK;
        if(!have_image && !io && s->log) s->log("Box art: %s: %s", key, kui_game_result_name(r));
    }
    struct kui_game_metadata_ops meta_ops = {image, metadata_sector, metadata_range};
    if(have_image) {
        enum kui_game_metadata_status m = kui_game_metadata_read(&meta_ops,image->data_lba,&meta);
        if(m == KUI_GAME_METADATA_IO || m == KUI_GAME_METADATA_CANCELLED) io = true;
        if(meta.ip_valid) {
            if(s->identity) media_identity(&meta,s->identity);
            snprintf(record.title, sizeof(record.title), "%s", kui_game_metadata_display_title(&meta));
            snprintf(record.product, sizeof(record.product), "%s", meta.product);
            snprintf(record.version, sizeof(record.version), "%s", meta.version);
            snprintf(record.region, sizeof(record.region), "%s", meta.region);
        }
    }
    /* Even a single half-width kana used to become '?'. Read the IP title
     * before touching existing artwork: a literal question mark is valid.
     * Keep the previous record when the disc no longer supplies a title. */
    if(check_old_title && (!record.title[0] || !strcmp(old.title, record.title))) {
        if(!image_close(&files)) io = true;
        free(image);
        return io || stopped(s->cancel) ? OUT_FAILED : OUT_UNCHANGED;
    }
    if(!io && compact) {
        FIL saved;
        if(s->compact_verified || (record.user.bytes & OVERLAY_STAMP)) art = true;
        else if(kui_games_open_compact(&saved,user) != FR_OK) io = true;
        else {
            art = kui_compact_cover_verify(&saved,s->cancel) == KUI_COMPACT_OK;
            if(f_close(&saved) != FR_OK || !art) io = true;
        }
        if(art) {
            record.source = KUI_COVER_SOURCE_DOWNLOAD;
            if(s->log) s->log("Box art: %s: downloaded cover",key);
        }
    } else if(!io && user[0] && user_art(s, key, user)) {
        record.source = KUI_COVER_SOURCE_USER;
        art = true;
        if(s->log) s->log("Box art: %s: your image %s", key, user + 2);
    }
    if(!io && !art && have_image && meta.root_bytes) {
        uint32_t lba = 0, bytes = 0;
        enum kui_game_metadata_status m = kui_game_metadata_find(&meta_ops, &meta, "0GDTEX.PVR",
            KUI_GAMES_ART_MAX_BYTES, &lba, &bytes);
        if(m == KUI_GAME_METADATA_OK && disc_art(s, key, image, lba, bytes, &io)) {
            record.source = KUI_COVER_SOURCE_DISC;
            art = true;
        } else if(m == KUI_GAME_METADATA_IO || m == KUI_GAME_METADATA_CANCELLED) io = true;
        else if(m != KUI_GAME_METADATA_OK && s->log) s->log("Box art: %s: no artwork on the disc", key);
    }
    if(!image_close(&files)) io = true;
    free(image);
    if(io || stopped(s->cancel)) return OUT_FAILED;
    if(s->disc_only) {
        if(!kui_cover_header_encode(s->record, &record)) return OUT_FAILED;
    } else if(!record_write(s, key, &record)) return OUT_FAILED;
    if(record.source == KUI_COVER_SOURCE_DOWNLOAD || record.source == KUI_COVER_SOURCE_USER) return OUT_USER;
    return record.source == KUI_COVER_SOURCE_DISC ? OUT_DISC : OUT_NONE;
}
/* The compact network master and its reference stay intact. Older cards
 * with only that reference generate a disc copy on first use. A header-only
 * negative cache also avoids repeatedly opening images with no 0GDTEX. */
static enum outcome alternative_disc(const char *key, const struct kui_cover_record *primary,
        enum kui_cover_size size, uint16_t *pixels, kui_log_fn log, kui_cancel_fn cancel) {
    char path[PATH_CAP], temp[PATH_CAP];
    if(!record_path(key, ".kcd", path) || !record_path(key, ".kdn", temp)) return OUT_FAILED;
    FIL file;
    struct kui_cover_record disc;
    bool invalid;
    const char *extension = strrchr(primary->gdi_path, '.');
    bool zso = extension && same_name(extension, ".zso");
    struct kui_game_launch_settings choice;
    if(zso && !kui_game_launch_settings_load(primary->gdi_path, &choice, log)) return OUT_FAILED;
    FRESULT r = record_read_at(path, &file, &disc, &invalid);
    bool replace = r == FR_OK || invalid;
    if(r == FR_OK) {
        bool match = !strcmp(disc.gdi_path, primary->gdi_path) && same_stamp(&disc.gdi, &primary->gdi) &&
            (!zso || disc.user.bytes == 1u + (unsigned)choice.codec) &&
            (disc.source == KUI_COVER_SOURCE_DISC || disc.source == KUI_COVER_SOURCE_NONE);
        bool visible = match && disc.source == KUI_COVER_SOURCE_DISC;
        bool ok = !visible || record_pixels(&file, &disc, size, pixels);
        if(f_close(&file) != FR_OK || !ok) return OUT_FAILED;
        if(match) return visible ? OUT_DISC : OUT_NONE;
    } else if(!invalid && r != FR_NO_FILE && r != FR_NO_PATH) return OUT_FAILED;
    struct scan scan = {.log = log, .cancel = cancel, .disc_only = true};
    scan.record = malloc(KUI_COVER_FILE_BYTES);
    scan.raw = malloc(16u * KUI_GAME_RAW_BYTES);
    scan.scaled = malloc(KUI_COVER_PIXELS * sizeof(*scan.scaled));
    enum outcome found = OUT_FAILED;
    if(scan.record && scan.raw && scan.scaled) found = build(&scan, key, primary->gdi_path);
    if(found == OUT_DISC || found == OUT_NONE) {
        UINT bytes = found == OUT_DISC ? KUI_COVER_FILE_BYTES : KUI_COVER_HEADER_BYTES;
        if(found == OUT_DISC) {
            unsigned edge = kui_cover_edge(size);
            const uint8_t *data = scan.record + kui_cover_offset(size);
            for(unsigned i = 0; i < edge * edge; ++i)
                pixels[i] = (uint16_t)(data[2*i] | data[2*i+1] << 8);
        }
        /* The cache is reproducible. On a full or read-only card, still
         * display the newly decoded pixels; only publish a complete file. */
        r = f_open(&file, temp, FA_CREATE_ALWAYS | FA_WRITE);
        if(r == FR_OK) {
            r = close_record(&file, write_exact(&file, scan.record, bytes));
            if(r == FR_OK && replace) r = f_unlink(path);
            if(r == FR_OK) r = f_rename(temp, path);
            if(r != FR_OK) (void)f_unlink(temp);
        }
    }
    free(scan.scaled);
    free(scan.raw);
    free(scan.record);
    return found;
}
static bool preferred_pixels(const char *key, FIL *file, const struct kui_cover_record *record,
        enum kui_cover_size size, uint16_t *pixels, kui_log_fn log, kui_cancel_fn cancel, bool *cacheable) {
    if(record->source == KUI_COVER_SOURCE_DISC) return record_pixels(file, record, size, pixels);
    if(!load_art_preference()) *cacheable = false;
    bool disc_first = art_preference_known && art_preference == KUI_GAMES_ART_DISC_FIRST;
    if(record->source == KUI_COVER_SOURCE_USER) {
        /* Legacy internet PNGs and loose owner art shared the same source
         * tag. Only the managed PNG with a matching stamp may yield to disc. */
        char path[PATH_CAP];
        FILINFO info;
        if(!disc_first || !kui_games_media_path(record->gdi_path, KUI_DOWNLOADED_COVERS,
                "png", path, sizeof(path)) || f_stat(path, &info) != FR_OK ||
                (info.fattrib & AM_DIR)) return record_pixels(file, record, size, pixels);
        struct kui_cover_stamp legacy;
        stamp_of(&info, &legacy);
        if(!same_stamp(&legacy, &record->user)) return record_pixels(file, record, size, pixels);
        for(unsigned i = 0; i < sizeof(user_images) / sizeof(user_images[0]); ++i) {
            if(!record_path(key, user_images[i], path)) return false;
            FRESULT r = f_stat(path, &info);
            if(r != FR_NO_FILE && r != FR_NO_PATH) return record_pixels(file, record, size, pixels);
        }
    }
    if(!disc_first && record_pixels(file, record, size, pixels)) return true;
    if(stopped(cancel)) return false;
    enum outcome disc = alternative_disc(key, record, size, pixels, log, cancel);
    if(disc == OUT_FAILED || !disc_first) *cacheable = false;
    if(disc == OUT_DISC) return true;
    return disc_first && !stopped(cancel) && record_pixels(file, record, size, pixels);
}

/* Old network downloads use a dedicated, path-hashed PNG. Preserve this
 * original until the complete compact replacement is on the card and its
 * three pixel checksums have been read back. A retry finishes any cleanup. */
static bool migrate_download(const char *game,uint64_t *generation,kui_cancel_fn cancel,kui_log_fn log) {
    char png[PATH_CAP], compact[PATH_CAP];
    if(!kui_games_media_path(game,KUI_DOWNLOADED_COVERS,"png",png,sizeof(png)) ||
       !kui_games_media_path(game,KUI_DOWNLOADED_COVERS,"kct",compact,sizeof(compact))) return false;
    FRESULT overlay=overlay_generation(compact,generation,cancel);
    if(overlay==FR_OK) return true;
    if(!unavailable(overlay)) return false;
    FILINFO info;
    FRESULT r = f_stat(png,&info);
    bool has_png = r == FR_OK;
    if((has_png && (info.fattrib & AM_DIR)) || (!has_png && r != FR_NO_FILE && r != FR_NO_PATH)) return false;

    FIL saved;
    r = kui_games_open_compact_direct(&saved,compact);
    if(r == FR_OK) {
        enum kui_compact_result checked = kui_compact_cover_verify(&saved,cancel);
        if(f_close(&saved) != FR_OK || checked == KUI_COMPACT_ERROR || stopped(cancel)) return false;
        if(checked == KUI_COMPACT_OK) return !has_png || f_unlink(png) == FR_OK;
        /* Only generated data with a confirmed bad length, magic or checksum
         * may be reclaimed by this explicit scan. Leave SD read errors alone. */
        if(f_unlink(compact) != FR_OK) return false;
        if(log) log("Box art: replacing a damaged downloaded cover for %s",game);
    } else if(r == FR_INVALID_OBJECT) {
        /* A broken generated link may be reclaimed on this explicit scan.
         * The shared target and other discs' links remain intact. */
        if(f_unlink(compact) != FR_OK) return false;
    } else if(r != FR_NO_FILE && r != FR_NO_PATH) return false;
    if(!has_png) return true;

    static const char temp[] = KUI_DOWNLOADED_COVERS "/compact.tmp";
    FIL source;
    if(f_open(&source,png,FA_READ) != FR_OK) return false;
    r = f_open(&saved,temp,FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
    bool ok = r == FR_OK;
    if(ok) {
        ok = kui_compact_cover_encode(&source,&saved,false,cancel) == KUI_COMPACT_OK &&
            f_sync(&saved) == FR_OK;
        if(f_close(&saved) != FR_OK) ok = false;
    }
    if(f_close(&source) != FR_OK) ok = false;
    if(!ok || stopped(cancel) || f_rename(temp,compact) != FR_OK) {
        if(r == FR_OK) f_unlink(temp);
        return false;
    }
    if(f_open(&saved,compact,FA_READ) != FR_OK) return false;
    ok = kui_compact_cover_verify(&saved,cancel) == KUI_COMPACT_OK;
    if(f_close(&saved) != FR_OK || !ok || stopped(cancel) || f_unlink(png) != FR_OK) return false;
    if(log) log("Box art: compacted existing download for %s",game);
    return true;
}
bool kui_games_prepare_media_cover_state(const struct kui_games_entry *entry,enum kui_games_media_cover_state *state,
        struct kui_games_disc_identity *identity, kui_log_fn log, kui_cancel_fn cancel) {
    if(!entry || !state) return false;
    *state = KUI_GAMES_MEDIA_COVER_MISSING;
    if(entry->cover_path[0]) { *state=KUI_GAMES_MEDIA_COVER_OWNER; return true; }
    char key[KUI_COVER_PATH_CAP], path[PATH_CAP];
    if(!kui_cover_key(entry->name, key) || stopped(cancel)) return false;
    static const char *const folders[] = {"0:/KUI", KUI_GAMES_COVERS_FOLDER};
    if(!make_folders(folders, 2, log)) return false;
    FIL file;
    struct kui_cover_record old;
    bool backup;
    FRESULT r = selector_record(key, entry->path, &file, &old, &backup, true, log);
    if(r == FR_OK) {
        if(f_close(&file) != FR_OK) return false;
    } else if(r != FR_NO_FILE && !(r == FR_INVALID_OBJECT && backup)) return false;
    /* An explicit owner file stays protected even when its decoder fails. */
    for(unsigned i=0;i<sizeof(user_images)/sizeof(user_images[0]);++i) {
        FILINFO info;
        if(!record_path(key,user_images[i],path)) return false;
        r=f_stat(path,&info);
        if(r==FR_OK) { *state=KUI_GAMES_MEDIA_COVER_OWNER; break; }
        if(r!=FR_NO_FILE && r!=FR_NO_PATH) return false;
    }
    uint64_t generation=0;
    if(*state!=KUI_GAMES_MEDIA_COVER_OWNER && !migrate_download(entry->path,&generation,cancel,log)) return false;
    /* Build the catalogue reference, including after an interrupted install. */
    struct scan scan = {.log = log, .cancel = cancel, .compact_verified = *state!=KUI_GAMES_MEDIA_COVER_OWNER,
        .identity = identity, .overlay_generation = generation};
    scan.record = malloc(KUI_COVER_FILE_BYTES);
    scan.raw = malloc(16u * KUI_GAME_RAW_BYTES);
    scan.scaled = malloc(KUI_COVER_PIXELS * sizeof(*scan.scaled));
    enum outcome result = OUT_FAILED;
    if(scan.record && scan.raw && scan.scaled) result = build(&scan, key, entry->path);
    free(scan.record);
    free(scan.raw);
    free(scan.scaled);
    if(result == OUT_FAILED || stopped(cancel)) return false;
    if(result != OUT_UNCHANGED) {
        kui_games_covers_content_clear();
        artwork_known = false;
    }
    if(*state==KUI_GAMES_MEDIA_COVER_OWNER) return true;
    if(generation) { *state=KUI_GAMES_MEDIA_COVER_MANAGED; return true; }
    if(!kui_games_media_path(entry->path,KUI_DOWNLOADED_COVERS,"kct",path,sizeof(path))) return false;
    FILINFO info;
    r=f_stat(path,&info);
    if(r==FR_NO_FILE || r==FR_NO_PATH) return true;
    if(r!=FR_OK || (info.fattrib & AM_DIR) ||
       (info.fsize!=KUI_COMPACT_COVER_BYTES && info.fsize!=KUI_COMPACT_LINK_BYTES)) return false;
    *state=KUI_GAMES_MEDIA_COVER_MANAGED;
    return true;
}
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,
        struct kui_games_disc_identity *identity,kui_log_fn log,kui_cancel_fn cancel) {
    enum kui_games_media_cover_state state;
    bool ok=kui_games_prepare_media_cover_state(entry,&state,identity,log,cancel);
    *present=ok && state!=KUI_GAMES_MEDIA_COVER_MISSING;
    return ok;
}

enum kui_games_identity_result kui_games_media_identity(const char *path,
        struct kui_games_disc_identity *out, kui_log_fn log, kui_cancel_fn cancel) {
    memset(out, 0, sizeof(*out));
    const char *slash = strrchr(path, '/');
    if(!slash || (size_t)(slash-path) >= KUI_GAMES_FILE_CAP ||
       !kui_game_image_name_supported(slash+1)) return KUI_GAMES_IDENTITY_UNKNOWN;
    struct image_files files = {.cancel = cancel};
    if(slash == path) strcpy(files.root, "/");
    else memcpy(files.root, path, (size_t)(slash-path));
    struct kui_game_launch_settings choice;
    kui_game_launch_settings_default(&choice);
    size_t len = strlen(slash+1);
    if(len >= 4 && same_name(slash+1+len-4, ".zso") &&
       !kui_game_launch_settings_load(path, &choice, log)) return KUI_GAMES_IDENTITY_ERROR;
    struct kui_game_image *image = malloc(sizeof(*image));
    uint8_t *sector = malloc(2048);
    struct kui_game_file_ops ops = {&files, image_stat, image_read};
    enum kui_game_result result = KUI_GAME_IO;
    enum kui_games_identity_result found = KUI_GAMES_IDENTITY_UNKNOWN;
    if(image && sector) {
        result = kui_game_image_open_named_codec(slash+1, &ops, (enum kui_zso_codec)choice.codec, image);
        if(result == KUI_GAME_OK)
            result = kui_game_image_read(image, image->data_lba, 1, KUI_GAME_SECTOR_MODE1, sector, 2048);
        struct kui_game_metadata meta;
        if(result == KUI_GAME_OK && kui_game_metadata_ip(sector, &meta) && media_identity(&meta,out))
            found = KUI_GAMES_IDENTITY_FOUND;
    }
    free(sector);
    free(image);
    if(!image_close(&files) || stopped(cancel) || result == KUI_GAME_IO ||
       result == KUI_GAME_NOT_FOUND || result == KUI_GAME_CANCELLED) return KUI_GAMES_IDENTITY_ERROR;
    return found;
}
static void summary(struct kui_app_status *status, const struct kui_games_scan_counts *c) {
    status->line_count = 0;
    char (*l)[KUI_APP_LINE_CAP] = status->lines;
    snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "Games found: %u", c->games);
    snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "New covers from discs: %u", c->disc);
    snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "New covers from your images: %u", c->user);
    snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "No artwork found: %u", c->none);
    snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "Unchanged since last scan: %u", c->unchanged);
    if(c->duplicates)
        snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "Separate records for same-name games: %u", c->duplicates);
    if(c->failed) snprintf(l[status->line_count++], KUI_APP_LINE_CAP, "Could not be read or saved: %u", c->failed);
}
bool kui_games_scan(struct kui_app_status *status, struct kui_games_scan_counts *counts,
    kui_app_progress_fn progress, kui_log_fn log, kui_cancel_fn cancel) {
    if(!status || !counts) return false;
    kui_games_covers_cache_clear();
    memset(status, 0, sizeof(*status));
    memset(counts, 0, sizeof(*counts));
    struct scan s = {.log = log, .cancel = cancel, .counts = counts};
    snprintf(status->message, sizeof(status->message), "Finding games...");
    if(progress) progress(status);
    const char *problem = NULL;
    bool connected = false, mounted = false;
    FATFS fs;
    if(stopped(cancel)) problem = "Box art scan stopped";
    else if(!(connected = kui_sd_connect())) problem = "SD card unavailable";
    else if(!(mounted = kui_mount(&fs, log))) problem = "Cannot mount SD card";
    static const char *const folders[] = {"0:/KUI", KUI_GAMES_COVERS_FOLDER};
    if(!problem && !make_folders(folders, 2, log)) problem = "Cannot create KUI/covers";
    if(!problem) {
        s.record = malloc(KUI_COVER_FILE_BYTES);
        s.raw = malloc(16u * KUI_GAME_RAW_BYTES);
        s.scaled = malloc(KUI_COVER_PIXELS * sizeof(uint16_t));
        if(!s.record || !s.raw || !s.scaled) problem = "Insufficient memory for the box art scan";
    }
    if(!problem && !walk(&s, KUI_GAMES_SCAN_ROOT, 0))
        problem = stopped(cancel) ? "Box art scan stopped" : "Cannot read the /Games folder";
    counts->games = s.found;
    const char *at = s.names;
    for(unsigned i = 0; !problem && i < s.found; ++i) {
        const char *key = at, *path = nth_path(at);
        at = path + strlen(path) + 1u;
        if(stopped(cancel)) { problem = "Box art scan stopped"; break; }
        status->done = i;
        status->total = s.found;
        snprintf(status->message, sizeof(status->message), "Box art %u of %u: %.90s", i + 1u, s.found, key);
        summary(status, counts);
        if(progress) progress(status);
        switch(build(&s, key, path)) {
        case OUT_UNCHANGED: ++counts->unchanged; break;
        case OUT_DISC: ++counts->disc; break;
        case OUT_USER: ++counts->user; break;
        case OUT_NONE: ++counts->none; break;
        default:
            if(stopped(cancel)) problem = "Box art scan stopped";
            else { ++counts->failed; if(log) log("Box art: %s: skipped after a read or save problem", key); }
            break;
        }
        status->done = i + 1u;
    }
    free(s.names); free(s.record); free(s.raw); free(s.scaled);
    if(mounted && f_mount(NULL, "0:", 0) != FR_OK && !problem) problem = "Cannot release SD filesystem";
    if(connected) kui_sd_disconnect();
    summary(status, counts);
    status->complete = true;
    status->stopped = problem && stopped(cancel);
    status->errors = counts->failed;
    status->passed = !problem && !counts->failed;
    if(problem) snprintf(status->message, sizeof(status->message), "%s after %u of %u games", problem,
                         (unsigned)status->done, counts->games);
    else snprintf(status->message, sizeof(status->message), "Box art ready: %u new, %u unchanged, %u without art",
                  counts->disc + counts->user, counts->unchanged, counts->none);
    if(log) {
        log("%s", status->message);
        for(unsigned i = 0; i < status->line_count; ++i) log("  %s", status->lines[i]);
    }
    if(progress) progress(status);
    return !problem;
}
