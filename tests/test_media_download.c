/* SPDX-License-Identifier: GPL-3.0-only */
/* The real install worker sees interrupted HTTP responses and FatFs failures.
 * Media payloads are synthetic. The FAT/exFAT integration suite covers the
 * actual card implementation. */
#include "kui/media_download.h"
#include "kui/internet_cover.h"
#include "kui/internet_extra.h"
#include "kui/media_container.h"
#include "kui/games_media_storage.h"
#include "kui/games_storage.h"
#include "fixtures/cover_images.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define TEMP "0:/KUI/media/fetch.tmp"
#define VIDEO "0:/KUI/media/video.tmp"
static struct {
    unsigned gets, pages, commits, limit_page, fail_page;
    bool mounted, owner_cover, temp, video, saved, short_write, race, dirty, stop, corrupt_png, corrupt_pdf, bad_pages;
    bool has_url, convert, cancel_conversion, fail_identity, small_url, small_bad, small_tiny, small_changed;
    enum kui_http_result small_result;
    enum kui_http_result response;
    unsigned char buffer[90000], clip[200], saved_file[90000];
    size_t length, clip_length, saved_length;
    char dest[KUI_GAMES_FILE_CAP];
    FIL *clip_handle;
} test;
static const struct kui_games_entry game = {.name="Example",.path="/Games/Example/disc.gdi"};
static const uint8_t pdf[] = "%PDF-1.5\n1 0 obj\nwhatever\nendobj\n%%EOF\r\n";
static bool cancel(void) { return test.stop; }
static void reset(void) {
    memset(&test,0,sizeof(test));
    test.has_url=true; test.convert=true;
}
bool kui_internet_cover_small_url(const struct kui_games_disc_identity *id,char *url,size_t cap) {
    assert(!strcmp(id->product,"T-1"));snprintf(url,cap,"small");return test.small_url;
}
bool kui_internet_cover_small_matches(const struct kui_games_disc_identity *id,uint32_t bytes,uint32_t crc) {
    assert(id && bytes && crc);return !test.small_changed;
}
bool kui_internet_cover_url(const struct kui_games_disc_identity *id,char *url,size_t cap) {
    assert(!strcmp(id->product,"T-1"));snprintf(url,cap,"cover");return test.has_url;
}
bool kui_internet_extra_url(const struct kui_games_disc_identity *id,enum kui_media_kind kind,char *url,size_t cap) {
    assert(!strcmp(id->product,"T-1"));
    snprintf(url,cap,"%s",kind==KUI_MEDIA_MANUAL?"manual":"preview");return test.has_url;
}
unsigned kui_internet_manual_pages(const struct kui_games_disc_identity *id) {(void)id;return test.pages;}
bool kui_internet_manual_page_url(const struct kui_games_disc_identity *id,unsigned i,char *url,size_t cap) {
    (void)id;snprintf(url,cap,"page%u",i+1);return true;
}
enum kui_http_result kui_internet_get(struct kui_internet *s,const char *url,uint32_t max,
        kui_http_write_fn sink,void *ctx,kui_http_progress_fn progress,void *progress_ctx) {
    (void)s;(void)progress;(void)progress_ctx;
    ++test.gets;
    const uint8_t *data;
    size_t n;
    if(!strcmp(url,"cover")) {data=cover_png_rgb;n=sizeof(cover_png_rgb)-(test.corrupt_png?1:0);}
    else if(!strcmp(url,"small")) {
        if(test.small_result!=KUI_HTTP_OK) {
            test.stop=test.small_result==KUI_HTTP_STOPPED;
            return test.small_result;
        }
        if(test.small_bad) {data=pdf;n=sizeof(pdf);}
        else {assert(test.small_tiny || test.small_changed);data=cover_jpeg;n=sizeof(cover_jpeg);}
    }
    else if(!strcmp(url,"manual")) {data=pdf;n=sizeof(pdf)-1-(test.corrupt_pdf?6:0);}
    else if(!strcmp(url,"preview")) {data=(const uint8_t *)"OggS";n=4;}
    else {
        unsigned page;assert(sscanf(url,"page%u",&page)==1);
        if(test.fail_page==page)return KUI_HTTP_MISSING;
        if(test.limit_page==page)return KUI_HTTP_LIMITED;
        data=cover_jpeg;n=sizeof(cover_jpeg)-(test.bad_pages?3:0);
    }
    assert(n<max);
    for(size_t pos=0;pos<n;) {
        size_t size=n-pos > 31 ? 31 : n-pos;
        if(!sink(ctx,data+pos,size))return KUI_HTTP_ERROR;
        pos+=size;
        if(test.response!=KUI_HTTP_OK) {
            test.stop=test.response==KUI_HTTP_STOPPED;
            return test.response;
        }
    }
    return KUI_HTTP_OK;
}
enum kui_http_result kui_internet_get_prefix(struct kui_internet *s,const char *url,uint32_t max,
        kui_http_write_fn sink,void *ctx,kui_http_progress_fn progress,void *progress_ctx,bool (*complete)(void *)) {
    assert(!strcmp(url,"preview"));
    enum kui_http_result r = kui_internet_get(s,url,max,sink,ctx,progress,progress_ctx);
    assert(!complete(ctx)); /* The mock origin supplies a short whole-file response. */
    return r;
}
bool kui_internet_preview_convert(FIL *src,FIL *dest,kui_cancel_fn stop,kui_log_fn log) {
    (void)log;assert(f_tell(src)==0 && f_size(src)==4 && !stop());
    if(test.cancel_conversion) {test.stop=true;return false;}
    if(!test.convert)return false;
    uint8_t h[16]={ 'K','P','V','1',160,0,120,0,10,0,0,0,1,0,0,0 };
    static const uint8_t pixels[KUI_PREVIEW_FRAME_BYTES]={0};
    UINT out=0;
    return f_write(dest,h,sizeof(h),&out)==FR_OK && out==sizeof(h) &&
        f_write(dest,pixels,sizeof(pixels),&out)==FR_OK && out==sizeof(pixels);
}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;assert(!test.mounted);test.mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_done(bool keep) {(void)keep;assert(test.mounted);test.mounted=false;return true;}
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,kui_log_fn log,kui_cancel_fn c) {
    assert(entry==&game && test.mounted);(void)log;(void)c;*present=test.owner_cover||test.saved;return true;
}
enum kui_games_identity_result kui_games_media_identity(const char *path,struct kui_games_disc_identity *out,
        kui_log_fn log,kui_cancel_fn c) {
    (void)log;(void)c;assert(!strcmp(path,game.path));memset(out,0,sizeof(*out));
    strcpy(out->product,"T-1");return test.fail_identity?KUI_GAMES_IDENTITY_UNKNOWN:KUI_GAMES_IDENTITY_FOUND;
}
bool kui_game_image_name_supported(const char *name) {return !strcmp(name,"disc.gdi");}
FRESULT f_stat(const TCHAR *path,FILINFO *info) {
    assert(!strcmp(path,"0:/KUI") || !strcmp(path,"0:/KUI/media") || !strcmp(path,KUI_DOWNLOADED_COVERS));
    info->fattrib=AM_DIR;return FR_OK;
}
FRESULT f_mkdir(const TCHAR *path) { (void)path;assert(0);return FR_DENIED; }
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode) {
    memset(f,0,sizeof(*f));
    if(!strcmp(path,TEMP)) {
        assert(mode&FA_CREATE_ALWAYS);test.temp=true;test.length=0;return FR_OK;
    }
    if(!strcmp(path,VIDEO)) {
        assert(mode&FA_CREATE_ALWAYS);test.video=true;test.clip_handle=f;test.clip_length=0;return FR_OK;
    }
    if(!test.saved || strcmp(path,test.dest)) return FR_NO_FILE;
    assert(mode==FA_READ);memcpy(test.buffer,test.saved_file,test.saved_length);
    test.length=test.saved_length;f->obj.objsize=test.saved_length;return FR_OK;
}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got) {
    bool clip=f==test.clip_handle;
    size_t size=clip?test.clip_length:test.length;
    assert(f->fptr<=size);
    if(n>size-f->fptr)n=(UINT)(size-f->fptr);
    memcpy(out,(clip?test.clip:test.buffer)+f->fptr,n);f->fptr+=n;*got=n;return FR_OK;
}
FRESULT f_write(FIL *f,const void *data,UINT n,UINT *wrote) {
    bool clip=f==test.clip_handle;
    assert(test.temp && f->fptr+n<=(clip?sizeof(test.clip):sizeof(test.buffer)));
    if(test.short_write && n) --n;
    memcpy((clip?test.clip:test.buffer)+f->fptr,data,n);
    f->fptr+=n;
    if(f->obj.objsize<f->fptr)f->obj.objsize=f->fptr;
    if(clip)test.clip_length=f->obj.objsize;else test.length=f->obj.objsize;
    *wrote=n;return FR_OK;
}
FRESULT f_truncate(FIL *f) {
    assert(f!=test.clip_handle && test.temp);
    test.length=f->obj.objsize=f->fptr;return FR_OK;
}
FRESULT f_lseek(FIL *f,FSIZE_t at) {assert(at<=f_size(f));f->fptr=at;return FR_OK;}
FRESULT f_sync(FIL *f) {(void)f;return test.dirty?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL *f) {if(f==test.clip_handle)test.clip_handle=NULL;return FR_OK;}
FRESULT f_rename(const TCHAR *from,const TCHAR *to) {
    assert(test.temp && !strcmp(from,TEMP));if(test.race)return FR_EXIST;
    assert(!test.saved && strlen(to)<sizeof(test.dest));
    strcpy(test.dest,to);memcpy(test.saved_file,test.buffer,test.length);test.saved_length=test.length;
    ++test.commits;test.saved=true;test.temp=false;return FR_OK;
}
FRESULT f_unlink(const TCHAR *path) {
    if(!strcmp(path,TEMP)) {assert(test.temp);test.temp=false;}
    else {assert(!strcmp(path,VIDEO) && test.video);test.video=false;}
    return FR_OK;
}
static enum kui_media_result fetch(enum kui_media_kind kind) {
    int dummy;
    enum kui_media_result r=kui_media_fetch_internet((struct kui_internet *)&dummy,&game,kind,NULL,cancel,NULL,NULL);
    assert(!test.temp && !test.video && !test.mounted);
    return r;
}
int main(void) {
    char path[KUI_GAMES_FILE_CAP];
    reset();assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.commits==1);
    assert(kui_media_file_path(&game,KUI_MEDIA_COVER,path) && !strcmp(path,test.dest));
    assert(test.saved_length==79120 && !memcmp(test.saved_file,"KCT1",4));
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.gets==1);
    reset();test.small_url=true;test.small_result=KUI_HTTP_MISSING;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.gets==2 && test.commits==1);
    reset();test.small_url=true;test.small_bad=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.gets==2 && test.commits==1);
    reset();test.small_url=true;test.small_changed=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.gets==2 && test.commits==1);
    reset();test.small_url=true;test.small_tiny=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.gets==2 && test.commits==1);
    reset();test.small_url=true;test.small_result=KUI_HTTP_STOPPED;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_STOPPED && test.gets==1 && !test.saved);
    reset();test.small_url=true;test.small_result=KUI_HTTP_LIMITED;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_LIMITED && test.gets==1 && !test.saved);
    reset();test.owner_cover=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && !test.gets);
    reset();test.fail_identity=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_UNIDENTIFIED && !test.gets);
    reset();test.has_url=false;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_MISSING && !test.gets);
    reset();test.corrupt_png=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_MISSING && !test.saved);
    reset();test.response=KUI_HTTP_STOPPED;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_STOPPED && !test.saved);
    reset();test.short_write=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.race=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.dirty=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.corrupt_pdf=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_MISSING && !test.saved);
    reset();assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.saved);
    assert(kui_media_original_path(&game,KUI_MEDIA_MANUAL,path) && !strcmp(path,test.dest));
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL_EXISTING && test.gets==1);
    reset();test.pages=3;test.has_url=false;
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_INSTALLED && test.gets==3 && !memcmp(test.saved_file,"KDM1\3\0",6));
    assert(kui_media_file_path(&game,KUI_MEDIA_MANUAL,path) && !strcmp(path,test.dest));
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_EXISTING && test.gets==3);
    reset();test.pages=3;test.fail_page=2;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.gets==3);
    reset();test.pages=2;test.bad_pages=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.gets==3);
    reset();test.pages=3;test.limit_page=2;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_LIMITED && !test.saved);
    reset();test.pages=3;test.response=KUI_HTTP_STOPPED;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_STOPPED && !test.saved);
    reset();assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && !memcmp(test.saved_file,"KPV1",4));
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_EXISTING && test.gets==1);
    reset();test.convert=false;assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.cancel_conversion=true;assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_STOPPED && !test.saved);
    puts("PASS direct media install, indexed page assembly, PDF fallback, preview conversion, cancellation and SD failures");
}
