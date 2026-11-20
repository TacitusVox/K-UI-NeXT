/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/artwork_cleanup.h"
#include "kui/compact_cover.h"
#include "kui/games_covers.h"
#include "kui/games_media_storage.h"
#include "kui/games_metadata.h"
#include <stdlib.h>
#include <string.h>

#define CLEAN_STAMP "0:/KUI/media/art-clean.idx"
#define NATIVE_PATH_CAP (sizeof(KUI_DOWNLOADED_COVERS)+37u)
#define INVENTORY_LIMIT (8u*KUI_GAMES_SCAN_MAX)

struct native_cover {
    char key[33];
    bool live;
};
struct inventory {
    struct native_cover *files;
    size_t count,capacity;
    kui_cancel_fn cancel;
};
static bool stopped(const struct inventory *inventory) {
    return inventory->cancel && inventory->cancel();
}
static bool absent(FRESULT r) {
    return r==FR_NO_FILE || r==FR_NO_PATH;
}
static unsigned lower(unsigned c) {
    return c>='A' && c<='Z' ? c+('a'-'A') : c;
}
static bool extension(const char *name,const char *ext) {
    const char *at=strrchr(name,'.');
    if(!at) return false;
    while(*at && *ext && lower((unsigned char)*at)==(unsigned char)*ext) {++at;++ext;}
    return !*at && !*ext;
}
static bool key_from_name(const char *name,char key[33]) {
    if(strlen(name)!=36 || name[32]!='.') return false;
    for(unsigned i=0;i<32;i++) {
        unsigned c=lower((unsigned char)name[i]);
        if(!(c>='0' && c<='9') && !(c>='a' && c<='f')) return false;
        key[i]=(char)c;
    }
    key[32]=0;
    return true;
}
static bool append(struct inventory *list,const char key[33]) {
    if(list->count==list->capacity) {
        size_t next=list->capacity ? 2*list->capacity : 32;
        if(next>INVENTORY_LIMIT) next=INVENTORY_LIMIT;
        if(next==list->capacity) return false;
        struct native_cover *files=realloc(list->files,next*sizeof(*files));
        if(!files) return false;
        list->files=files;
        list->capacity=next;
    }
    struct native_cover *file=&list->files[list->count++];
    memcpy(file->key,key,sizeof(file->key));
    file->live=false;
    return true;
}
static int by_key(const void *a,const void *b) {
    const struct native_cover *left=a,*right=b;
    return strcmp(left->key,right->key);
}
static void mark(struct inventory *inventory,const char *key) {
    if(!key[0]) return;
    struct native_cover query;
    memcpy(query.key,key,sizeof(query.key));
    struct native_cover *found=bsearch(&query,inventory->files,inventory->count,
        sizeof(*inventory->files),by_key);
    if(found) found->live=true;
}
/* The optional stamp can skip a whole collection. It never authorizes an
 * unlink: a changed directory is always re-inventoried on the mounted card.
 * FAT timestamps are coarse. An in-place change within a tick may defer
 * cleanup until the next new blob, which necessarily changes the stamp. */
static bool download_inventory(struct inventory *list,struct kui_sha256 *stamp,bool collect) {
    DIR dir;
    FRESULT r=f_opendir(&dir,KUI_DOWNLOADED_COVERS);
    if(absent(r)) return true;
    if(r!=FR_OK) return false;
    FILINFO info;
    bool ok=true;
    while((r=f_readdir(&dir,&info))==FR_OK && info.fname[0]) {
        if(stopped(list)) {ok=false;break;}
        char key[33];
        if((info.fattrib & AM_DIR) || !key_from_name(info.fname,key) ||
                (!extension(info.fname,".kct") && !extension(info.fname,".kcu"))) continue;
        kui_sha256_update(stamp,info.fname,strlen(info.fname)+1);
        kui_sha256_update(stamp,&info.fsize,sizeof(info.fsize));
        kui_sha256_update(stamp,&info.fdate,sizeof(info.fdate));
        kui_sha256_update(stamp,&info.ftime,sizeof(info.ftime));
        kui_sha256_update(stamp,&info.fattrib,sizeof(info.fattrib));
        if(collect && !(info.fattrib&(AM_RDO|AM_SYS)) && info.fsize==KUI_COMPACT_COVER_BYTES &&
                extension(info.fname,".kct") && !append(list,key)) {ok=false;break;}
    }
    return f_closedir(&dir)==FR_OK && r==FR_OK && ok;
}
static bool links(struct inventory *list) {
    DIR dir;
    FRESULT r=f_opendir(&dir,KUI_DOWNLOADED_COVERS);
    if(r!=FR_OK) return false;
    FILINFO info;
    bool ok=true;
    while((r=f_readdir(&dir,&info))==FR_OK && info.fname[0]) {
        if(stopped(list)) {ok=false;break;}
        char key[33],refs[2][33];
        if((info.fattrib & AM_DIR) || !key_from_name(info.fname,key)) continue;
        bool overlay=extension(info.fname,".kcu");
        if(!overlay && (info.fsize!=KUI_COMPACT_LINK_BYTES || !extension(info.fname,".kct"))) continue;
        char path[NATIVE_PATH_CAP];
        snprintf(path,sizeof(path),"%s/%s.%s",KUI_DOWNLOADED_COVERS,key,overlay?"kcu":"kct");
        FIL file;
        r=f_open(&file,path,FA_READ);
        if(r!=FR_OK) break;
        r=kui_games_cover_reference_keys(&file,overlay,refs);
        if(f_close(&file)!=FR_OK) r=FR_DISK_ERR;
        if(r!=FR_OK) break;
        if(overlay) mark(list,key); /* direct base is the reader's final fallback */
        mark(list,refs[0]);
        mark(list,refs[1]);
    }
    return f_closedir(&dir)==FR_OK && r==FR_OK && ok;
}
static void mark_assignment(void *ctx,const char *path) {
    /* Metadata decoder only admits canonical root-relative, lowercase paths. */
    static const char prefix[]="/kui/covers/downloads/";
    char key[33];
    if(!strncmp(path,prefix,sizeof(prefix)-1) &&
            key_from_name(path+sizeof(prefix)-1,key) &&
            (extension(path,".png") || extension(path,".kct"))) mark(ctx,key);
}
static bool selector_bases(struct inventory *list,const char *folder) {
    DIR dir;
    FRESULT r=f_opendir(&dir,folder);
    if(absent(r)) return true;
    if(r!=FR_OK) return false;
    FILINFO info;
    bool ok=true;
    while((r=f_readdir(&dir,&info))==FR_OK && info.fname[0]) {
        if(stopped(list)) {ok=false;break;}
        if((info.fattrib & AM_DIR) || info.fsize<KUI_COVER_HEADER_BYTES ||
                !(extension(info.fname,".kcv") || extension(info.fname,".kcb") ||
                  extension(info.fname,".kcd"))) continue;
        char path[sizeof(KUI_GAMES_COVERS_FOLDER "/by-path/")+FF_MAX_LFN];
        int len=snprintf(path,sizeof(path),"%s/%s",folder,info.fname);
        if(len<0 || (size_t)len>=sizeof(path)) {ok=false;break;}
        FIL file;
        r=f_open(&file,path,FA_READ);
        if(r!=FR_OK) break;
        uint8_t header[KUI_COVER_HEADER_BYTES];
        UINT got=0;
        r=f_read(&file,header,sizeof(header),&got);
        if(f_close(&file)!=FR_OK || got!=sizeof(header)) r=FR_DISK_ERR;
        if(r!=FR_OK) break;
        struct kui_cover_record record;
        char key[33];
        if(kui_cover_header_decode(&record,header) && kui_games_media_key(record.gdi_path,key))
            mark(list,key);
    }
    return f_closedir(&dir)==FR_OK && r==FR_OK && ok;
}
/* A short or torn cache record is a miss. One fixed-size optional record. */
static bool already_clean(const uint8_t expected[36]) {
    FIL file;
    if(f_open(&file,CLEAN_STAMP,FA_READ)!=FR_OK) return false;
    uint8_t record[36];
    UINT got=0;
    bool ok=f_size(&file)==sizeof(record) && f_read(&file,record,sizeof(record),&got)==FR_OK &&
        got==sizeof(record) && !memcmp(record,expected,sizeof(record));
    return f_close(&file)==FR_OK && ok;
}
static void remember_clean(const uint8_t record[36]) {
    FIL file;
    FRESULT r=f_open(&file,CLEAN_STAMP,FA_WRITE|FA_CREATE_ALWAYS);
    if(r==FR_NO_PATH && f_mkdir("0:/KUI/media")==FR_OK)
        r=f_open(&file,CLEAN_STAMP,FA_WRITE|FA_CREATE_ALWAYS);
    if(r!=FR_OK) return;
    UINT put=0;
    if(f_write(&file,record,36,&put)==FR_OK && put==36) (void)f_sync(&file);
    (void)f_close(&file);
}
static bool sweep(struct inventory *list,unsigned *removed) {
    for(size_t i=0;i<list->count;i++) {
        if(stopped(list)) return false;
        const struct native_cover *native=&list->files[i];
        if(native->live) continue;
        char path[NATIVE_PATH_CAP];
        snprintf(path,sizeof(path),"%s/%s.kct",KUI_DOWNLOADED_COVERS,native->key);
        FIL file;
        if(f_open(&file,path,FA_READ)!=FR_OK) return false;
        uint8_t digest[32];
        enum kui_compact_result checked=kui_compact_cover_verify_digest(&file,list->cancel,digest);
        if(f_close(&file)!=FR_OK || checked==KUI_COMPACT_ERROR || stopped(list)) return false;
        if(checked!=KUI_COMPACT_OK) continue;
        char key[33];
        kui_hex(digest,16,key);
        if(strcmp(key,native->key)) continue;
        /* No directory handle is open. The sole mounted storage worker has
         * excluded publishers and FTP for this entire mark/verify pass. */
        if(f_unlink(path)!=FR_OK) return false;
        ++*removed;
    }
    return true;
}
static bool scan_stamp(struct inventory *list,uint8_t stamp[36],bool collect) {
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    if(stopped(list) || !download_inventory(list,&hash,collect)) return false;
    if(list->count) {
        uint8_t metadata[2][40];
        if(!kui_games_metadata_cover_stamp(metadata)) return false;
        kui_sha256_update(&hash,metadata,sizeof(metadata));
    }
    memcpy(stamp,"KGC1",4);
    kui_sha256_digest(&hash,stamp+4);
    return !stopped(list);
}
bool kui_artwork_cleanup_mounted(unsigned *removed,kui_log_fn log,kui_cancel_fn cancel) {
    unsigned count=0;
    struct inventory list={.cancel=cancel};
    uint8_t stamp[36];
    bool ok=scan_stamp(&list,stamp,true);
    if(ok && list.count && !already_clean(stamp)) {
        qsort(list.files,list.count,sizeof(*list.files),by_key);
        ok=links(&list) && kui_games_metadata_visit_covers(mark_assignment,&list) &&
            selector_bases(&list,KUI_GAMES_COVERS_FOLDER) &&
            selector_bases(&list,KUI_GAMES_COVERS_FOLDER "/by-path") && sweep(&list,&count);
        /* Record the actual remaining directory after unlinking. Verifying the
         * unchanged inventory on later scans costs no native-pixel reads. */
        if(ok && count) ok=scan_stamp(&list,stamp,false);
        if(ok) remember_clean(stamp);
    }
    free(list.files);
    if(removed) *removed=count;
    if(!ok && log) log("Artwork cleanup deferred");
    return ok;
}
