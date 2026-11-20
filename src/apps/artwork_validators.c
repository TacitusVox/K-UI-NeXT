/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/artwork_validators.h"
#include "kui/compact_cover.h"
#include "kui/hash.h"
#include <stddef.h>
#include <string.h>

#define MEDIA_DIR "0:/KUI/media"
#define CHECK_DIR MEDIA_DIR "/art-checks"
#define CHECK_TEMP CHECK_DIR "/write.tmp"
#define PATH_BYTES (sizeof(CHECK_DIR) + 37u)

/* Fixed byte layout. The record checksum covers even unused string bytes.
 * The source digest binds the release, provider role, URL and optional pin.
 * resource separately identifies the final URL after HTTP redirects. */
struct record {
    uint8_t magic[4], source[32], pixels[32], resource[32];
    char etag[128], modified[64];
    uint8_t checksum[32];
};
_Static_assert(sizeof(struct record)==324u,"Unexpected artwork validator layout");
_Static_assert(sizeof(((struct kui_http_validators *)0)->etag)==128u &&
               sizeof(((struct kui_http_validators *)0)->modified)==64u &&
               sizeof(((struct kui_http_validators *)0)->resource)==32u,
               "Update the disk format for changed HTTP validator limits");

static void record_checksum(const struct record *r, uint8_t out[32]) {
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,r,offsetof(struct record,checksum));
    kui_sha256_digest(&hash,out);
}

/* File names are stable across both content revisions and provider URL
 * changes. The original version named each file by (native path, URL). */
static void record_path(const uint8_t key[32],char path[PATH_BYTES]) {
    memcpy(path,CHECK_DIR "/",sizeof(CHECK_DIR));
    kui_hex(key,16,path+sizeof(CHECK_DIR));
    memcpy(path+sizeof(CHECK_DIR)+32u,".kav",5);
}
static void legacy_source(const char *master,const char *url,char path[PATH_BYTES],uint8_t key[32]) {
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,master,strlen(master)+1u);
    kui_sha256_update(&hash,url,strlen(url));
    kui_sha256_digest(&hash,key);
    record_path(key,path);
}
static bool stable_source(const char *release,bool small,const char *master,const char *url,
        char path[PATH_BYTES],uint8_t key[32]) {
    if(!release || !*release || !master || !*master || !url || !*url)return false;
    uint8_t slot[32],role=small?1u:0u;
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,"KAV2",4);
    kui_sha256_update(&hash,release,strlen(release)+1u);
    kui_sha256_update(&hash,&role,sizeof(role));
    kui_sha256_digest(&hash,slot);
    record_path(slot,path);
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,slot,sizeof(slot));
    kui_sha256_update(&hash,url,strlen(url));
    kui_sha256_digest(&hash,key);
    return true;
}

static bool cover_digest(const char *master, uint8_t digest[32]) {
    FIL file;
    if(f_open(&file,master,FA_READ)!=FR_OK) return false;
    bool ok=f_size(&file)==KUI_COMPACT_COVER_BYTES;
    struct kui_sha256 hash;
    uint8_t buf[4096];
    kui_sha256_init(&hash);
    for(unsigned pos=0;ok && pos<KUI_COMPACT_COVER_BYTES;) {
        UINT got, n=KUI_COMPACT_COVER_BYTES-pos;
        if(n>sizeof(buf)) n=sizeof(buf);
        ok=f_read(&file,buf,n,&got)==FR_OK && got==n && (pos || !memcmp(buf,"KCT1",4));
        if(ok) kui_sha256_update(&hash,buf,n);
        pos+=n;
    }
    if(f_close(&file)!=FR_OK || !ok) return false;
    kui_sha256_digest(&hash,digest);
    return true;
}

static bool token(const char *s, size_t cap) {
    for(size_t i=0;i<cap;++i) {
        unsigned char c=(unsigned char)s[i];
        if(!c) return true;
        if(c<32u || c==127u) return false;
    }
    return false;
}
static bool valid_tokens(const struct record *r) {
    static const uint8_t zero[32];
    return (r->etag[0] || r->modified[0]) && memcmp(r->resource,zero,sizeof(zero)) &&
        token(r->etag,sizeof(r->etag)) && token(r->modified,sizeof(r->modified));
}

bool kui_artwork_validators_load(const char *release,bool small,const char *master,
        const char *url,struct kui_http_validators *out) {
    if(!out)return false;
    memset(out,0,sizeof(*out));
    char path[PATH_BYTES];
    uint8_t key[32],digest[32];
    if(!stable_source(release,small,master,url,path,key))return false;
    FIL file;
    FRESULT opened=f_open(&file,path,FA_READ);
    bool legacy=opened==FR_NO_FILE || opened==FR_NO_PATH;
    if(legacy) {
        legacy_source(master,url,path,key);
        opened=f_open(&file,path,FA_READ);
    }
    if(opened!=FR_OK)return false;
    struct record r;
    UINT got;
    bool ok=f_size(&file)==sizeof(r) && f_read(&file,&r,sizeof(r),&got)==FR_OK && got==sizeof(r);
    if(f_close(&file)!=FR_OK || !ok || memcmp(r.magic,legacy?"KAV1":"KAV2",4) || memcmp(r.source,key,sizeof(key)))
        return false;
    record_checksum(&r,digest);
    if(memcmp(r.checksum,digest,sizeof(digest)) || !valid_tokens(&r) || !cover_digest(master,digest) ||
       memcmp(r.pixels,digest,sizeof(digest))) return false;
    memcpy(out->resource,r.resource,sizeof(r.resource));
    memcpy(out->etag,r.etag,sizeof(r.etag));
    memcpy(out->modified,r.modified,sizeof(r.modified));
    return true;
}

static bool make_dir(const char *path) {
    FRESULT r=f_mkdir(path);
    return r==FR_OK || r==FR_EXIST;
}
static bool remove_if_present(const char *path) {
    FRESULT r=f_unlink(path);
    return r==FR_OK || r==FR_NO_PATH || r==FR_NO_FILE;
}
bool kui_artwork_validators_save(const char *release,bool small,const char *master,const char *url,
        const struct kui_http_validators *in) {
    struct record r={.magic={'K','A','V','2'}};
    char path[PATH_BYTES];
    if(!in || !stable_source(release,small,master,url,path,r.source) ||
       !token(in->etag,sizeof(in->etag)) || !token(in->modified,sizeof(in->modified))) return false;
    char old_path[PATH_BYTES];
    uint8_t old_key[32];
    legacy_source(master,url,old_path,old_key);
    if(!in->etag[0] && !in->modified[0])
        return remove_if_present(old_path) && remove_if_present(path);
    memcpy(r.resource,in->resource,sizeof(r.resource));
    memcpy(r.etag,in->etag,strlen(in->etag)+1u);
    memcpy(r.modified,in->modified,strlen(in->modified)+1u);
    if(!valid_tokens(&r) || !cover_digest(master,r.pixels) || !make_dir(MEDIA_DIR) || !make_dir(CHECK_DIR))
        return false;
    record_checksum(&r,r.checksum);
    FIL temp;
    UINT written;
    if(f_open(&temp,CHECK_TEMP,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK) return false;
    bool ok=f_write(&temp,&r,sizeof(r),&written)==FR_OK && written==sizeof(r) && f_sync(&temp)==FR_OK;
    if(f_close(&temp)!=FR_OK) ok=false;
    if(ok) {
        /* FatFs cannot rename over an existing file. A reset between unlink
         * and rename leaves an unconditional check. Cover pixels stay intact. */
        FRESULT removed=f_unlink(path);
        ok=(removed==FR_OK || removed==FR_NO_FILE) && f_rename(CHECK_TEMP,path)==FR_OK;
    }
    if(!ok)(void)f_unlink(CHECK_TEMP);
    else if(strcmp(old_path,path))(void)remove_if_present(old_path);
    return ok;
}
