/* SPDX-License-Identifier: GPL-3.0-only */
/* The real install worker sees interrupted HTTP responses and FatFs failures.
 * Media payloads are synthetic. The FAT/exFAT integration suite covers the
 * actual card implementation. */
#include "kui/media_download.h"
#include "kui/artwork_validators.h"
#include "kui/internet_cover.h"
#include "kui/internet_extra.h"
#include "kui/media_container.h"
#include "kui/games_media_storage.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include "fixtures/cover_images.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define TEMP "0:/KUI/media/fetch.tmp"
#define VIDEO "0:/KUI/media/video.tmp"
#define COVER_REF_TEMP KUI_DOWNLOADED_COVERS "/cover-ref.tmp"
#define FIRST_DISC "/Games/Set/1/disc.gdi"
static struct {
    unsigned gets, pages, commits, limit_page, fail_page, identities, connects;
    bool mounted, owner_cover, temp, video, saved, short_write, race, dirty, stop, corrupt_png, corrupt_pdf, bad_pages;
    bool has_url, convert, cancel_conversion, fail_identity, small_url, small_bad, small_tiny, small_changed, cover_identity;
    bool legacy, deny_cleanup, corrupt_readback, pending_cleanup;
    unsigned removed;
    char legacy_path[KUI_GAMES_FILE_CAP];
    enum kui_http_result small_result;
    enum kui_media_reason last_reason;
    bool no_release, alternate_clips, runabout;
    unsigned missing_previews, preview_requests;
    enum kui_http_result response;
    unsigned char buffer[90000], clip[200], saved_file[90000];
    size_t length, clip_length, saved_length;
    char dest[KUI_GAMES_FILE_CAP], ref_paths[8][KUI_GAMES_FILE_CAP], ref[KUI_COMPACT_LINK_BYTES];
    char targets[8][KUI_GAMES_FILE_CAP], second_dest[KUI_GAMES_FILE_CAP];
    uint8_t ref_staging[KUI_COMPACT_LINK_BYTES], second_file[90000];
    size_t second_bytes;
    bool ref_temp;
    FIL *ref_handle, *saved_handle;
    const uint8_t *read_pixels;
    unsigned refs;
    bool different, different_pixels, fail_reference, new_pixels, offers_validator;
    char saved_url[64],validator_master[KUI_GAMES_FILE_CAP],etag[24];
    unsigned conditional, bodies;
    enum kui_artwork_mode artwork_mode;
    FIL *clip_handle;
} test;
static struct kui_games_entry game = {.name="Example",.path="/Games/Example/disc.gdi"};
static const uint8_t pdf[] = "%PDF-1.5\n1 0 obj\nwhatever\nendobj\n%%EOF\r\n";
static const uint8_t legacy_header[16]={'K','P','V','1',160,0,120,0,10,0,0,0,1,0,0,0};
static bool cancel(void) { return test.stop; }
static void reset(void) {
    memset(&test,0,sizeof(test));
    test.has_url=true; test.convert=true;
}
bool kui_games_library_media_disc1(const struct kui_games_entry *entry, char path[KUI_GAMES_FILE_CAP]) {
    assert(entry == &game && entry->media_disc1_key[0]);
    strcpy(path,FIRST_DISC);
    return true;
}
bool kui_games_library_media_member(const struct kui_games_entry *entry, unsigned index, char path[KUI_GAMES_FILE_CAP]) {
    if(index == 0) return kui_games_library_media_disc1(entry,path);
    if(index>3) return false;
    snprintf(path,KUI_GAMES_FILE_CAP,"/Games/Set/%u/disc.gdi",index+1);
    return true;
}
const char *kui_internet_cover_name(const struct kui_games_disc_identity *id) {
    if(test.no_release) return NULL;
    return !strcmp(id->product,"T-1") ? "Example (USA)" : "Example (USA) (Alt front)";
}
bool kui_internet_cover_small_url(const struct kui_games_disc_identity *id,char *url,size_t cap) {
    assert(id->product[0]);snprintf(url,cap,"small");return test.small_url;
}
bool kui_internet_cover_small_pin(const struct kui_games_disc_identity *id,uint32_t *bytes,uint32_t *crc) {
    (void)id;(void)bytes;(void)crc;return false;
}
bool kui_internet_cover_small_matches(const struct kui_games_disc_identity *id,uint32_t bytes,uint32_t crc) {
    assert(id && bytes && crc);return !test.small_changed;
}
bool kui_internet_cover_url(const struct kui_games_disc_identity *id,char *url,size_t cap) {
    assert(id->product[0]);snprintf(url,cap,"cover");return test.has_url;
}
bool kui_internet_extra_url(const struct kui_games_disc_identity *id,enum kui_media_kind kind,char *url,size_t cap) {
    assert(!strcmp(id->product,"T-1"));
    snprintf(url,cap,"%s",kind==KUI_MEDIA_MANUAL?"manual":"preview");return test.has_url;
}
bool kui_internet_preview_fallback_url(const struct kui_games_disc_identity *id,unsigned *attempt,char *url,size_t cap,unsigned *start_seconds) {
    assert(!strcmp(id->product,"T-1"));
    if(!test.alternate_clips || *attempt>=2) return false;
    ++*attempt;
    *start_seconds=test.runabout?18u:0u;
    if(test.runabout) snprintf(url,cap,"https://archive.org/download/016860/016860.ogv");
    else snprintf(url,cap,"preview-%u",*attempt);
    return true;
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
    if(!strcmp(url,"cover")) {
        if(test.new_pixels || (test.different_pixels && !strcmp(game.path,FIRST_DISC))) {data=cover_png_rgba;n=sizeof(cover_png_rgba);}
        else {data=cover_png_rgb;n=sizeof(cover_png_rgb)-(test.corrupt_png?1:0);}
    }
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
bool kui_artwork_validators_load(const char *release,bool small,const char *master,const char *url,struct kui_http_validators *out) {
    assert(release);(void)small;
    memset(out,0,sizeof(*out));
    if(!test.etag[0] || strcmp(master,test.validator_master) || strcmp(url,test.saved_url)) return false;
    strcpy(out->etag,test.etag);
    return true;
}
bool kui_artwork_validators_save(const char *release,bool small,const char *master,const char *url,const struct kui_http_validators *in) {
    assert(release);(void)small;
    strcpy(test.etag,in->etag);
    strcpy(test.saved_url,url);
    strcpy(test.validator_master,master);
    return true;
}
enum kui_http_result kui_internet_get_conditional(struct kui_internet *s,const char *url,uint32_t max,
        kui_http_write_fn sink,void *ctx,kui_http_progress_fn progress,void *progress_ctx,
        const struct kui_http_validators *known,struct kui_http_validators *received) {
    const char *tag=test.new_pixels?"new":"original";
    if(test.offers_validator) strcpy(received->etag,tag);
    if(known) {
        ++test.conditional;
        if(test.offers_validator && !strcmp(tag,known->etag)) { ++test.gets;return KUI_HTTP_NOT_MODIFIED; }
    }
    ++test.bodies;
    return kui_internet_get(s,url,max,sink,ctx,progress,progress_ctx);
}
enum kui_http_result kui_internet_get_prefix(struct kui_internet *s,const char *url,uint32_t max,
        kui_http_write_fn sink,void *ctx,kui_http_progress_fn progress,void *progress_ctx,bool (*complete)(void *)) {
    unsigned order = test.preview_requests++ + (test.has_url ? 0u : 1u);
    char expected[24]="preview";
    if(order) snprintf(expected,sizeof(expected),"preview-%u",order);
    if(test.runabout) assert(!strcmp(url,"https://archive.org/download/016860/016860.ogv"));
    else if(test.alternate_clips) assert(!strcmp(url,expected));
    else assert(!strcmp(url,"preview"));
    if(test.missing_previews) {
        --test.missing_previews;
        return KUI_HTTP_MISSING;
    }
    enum kui_http_result r = kui_internet_get(s,"preview",max,sink,ctx,progress,progress_ctx);
    assert(!complete(ctx)); /* The mock origin supplies a short whole-file response. */
    return r;
}
bool kui_internet_preview_convert(FIL *src,FIL *dest,unsigned start,kui_cancel_fn stop,kui_log_fn log,uint32_t *crc) {
    (void)log;assert(start==(test.runabout ? 18u : 0u));assert(f_tell(src)==0 && f_size(src)==4 && !stop());
    if(test.cancel_conversion) {test.stop=true;return false;}
    if(!test.convert)return false;
    uint8_t h[16]={ 'K','P','V','2',160,0,120,0,10,0,0,0,1,0,0,0 };
    static const uint8_t pixels[KUI_PREVIEW_FRAME_BYTES]={0};
    if(crc) *crc=kui_crc32_capture(kui_crc32_capture(0,pixels,sizeof(pixels)),h,sizeof(h));
    UINT out=0;
    return f_write(dest,h,sizeof(h),&out)==FR_OK && out==sizeof(h) &&
        f_write(dest,pixels,sizeof(pixels),&out)==FR_OK && out==sizeof(pixels);
}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;assert(!test.mounted);test.mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_done(bool keep) {(void)keep;assert(test.mounted);test.mounted=false;return true;}
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,
        struct kui_games_disc_identity *identity,kui_log_fn log,kui_cancel_fn c) {
    assert(entry==&game && test.mounted);(void)log;(void)c;
    if(identity && test.cover_identity)strcpy(identity->product,"T-1");
    char cover[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(entry,KUI_MEDIA_COVER,cover));
    *present = test.owner_cover || (test.saved && !strcmp(cover,test.dest)) ||
        (test.second_bytes && !strcmp(cover,test.second_dest));
    for(unsigned i=0;i<test.refs;++i) if(!strcmp(cover,test.ref_paths[i])) *present=true;
    return true;
}
bool kui_games_prepare_media_cover_state(const struct kui_games_entry *entry,enum kui_games_media_cover_state *state,
        struct kui_games_disc_identity *id,kui_log_fn log,kui_cancel_fn stop) {
    bool present;
    if(!kui_games_prepare_media_cover(entry,&present,id,log,stop)) return false;
    *state=test.owner_cover?KUI_GAMES_MEDIA_COVER_OWNER:present?KUI_GAMES_MEDIA_COVER_MANAGED:KUI_GAMES_MEDIA_COVER_MISSING;
    return true;
}
enum kui_games_identity_result kui_games_media_identity(const char *path,struct kui_games_disc_identity *out,
        kui_log_fn log,kui_cancel_fn c) {
    (void)log;(void)c;assert(!strcmp(path,game.path) || !strcmp(path,FIRST_DISC));memset(out,0,sizeof(*out));
    ++test.identities;
    if(test.fail_identity)return KUI_GAMES_IDENTITY_UNKNOWN;
    strcpy(out->product,test.different && !strcmp(path,FIRST_DISC) ? "T-2" : "T-1");
    return KUI_GAMES_IDENTITY_FOUND;
}
bool kui_game_image_name_supported(const char *name) {return !strcmp(name,"disc.gdi");}
FRESULT f_stat(const TCHAR *path,FILINFO *info) {
    assert(!strcmp(path,"0:/KUI") || !strcmp(path,"0:/KUI/media") || !strcmp(path,KUI_DOWNLOADED_COVERS));
    info->fattrib=AM_DIR;return FR_OK;
}
FRESULT f_mkdir(const TCHAR *path) { (void)path;assert(0);return FR_DENIED; }
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode) {
    memset(f,0,sizeof(*f));
    if(!strcmp(path,COVER_REF_TEMP)) {
        assert(mode&FA_CREATE_ALWAYS);test.ref_temp=true;test.ref_handle=f;return FR_OK;
    }
    if(!strcmp(path,TEMP)) {
        if(mode==FA_READ) {
            assert(test.temp);
            f->obj.objsize=test.length;
            if(test.corrupt_readback) test.buffer[test.length-1]^=1;
            return FR_OK;
        }
        assert(mode&FA_CREATE_ALWAYS);test.temp=true;test.length=0;return FR_OK;
    }
    if(!strcmp(path,VIDEO)) {
        assert(mode&FA_CREATE_ALWAYS);test.video=true;test.clip_handle=f;test.clip_length=0;return FR_OK;
    }
    if(test.legacy && !strcmp(path,test.legacy_path)) {
        assert(mode==FA_READ);
        memcpy(test.buffer,legacy_header,sizeof(legacy_header));
        memset(test.buffer+sizeof(legacy_header),0x35,KUI_PREVIEW_FRAME_BYTES);
        test.length=f->obj.objsize=sizeof(legacy_header)+KUI_PREVIEW_FRAME_BYTES;
        return FR_OK;
    }
    if(test.saved && !strcmp(path,test.dest)) {
        test.read_pixels=test.saved_file;f->obj.objsize=test.saved_length;
    } else if(test.second_bytes && !strcmp(path,test.second_dest)) {
        test.read_pixels=test.second_file;f->obj.objsize=test.second_bytes;
    } else return FR_NO_FILE;
    assert(mode==FA_READ);test.saved_handle=f;return FR_OK;
}
FRESULT kui_games_open_compact_target(FIL *f,const char *path,char *direct,size_t cap) {
    const char *actual=path;
    char overlay[KUI_GAMES_FILE_CAP];
    strcpy(overlay,path);overlay[strlen(overlay)-1]='u';
    for(unsigned i=0;i<test.refs;i++) {
        if(!strcmp(test.ref_paths[i],path)) actual=test.targets[i];
        if(!strcmp(test.ref_paths[i],overlay)) { actual=test.targets[i];break; }
    }
    assert(strlen(actual)<cap);
    strcpy(direct,actual);
    return f_open(f,actual,FA_READ);
}
FRESULT kui_games_open_compact_direct(FIL *f,const char *path) { return f_open(f,path,FA_READ); }
bool kui_games_publish_cover(const char *staged,const char *final,kui_cancel_fn stop) {
    return !stop() && f_rename(staged,final)==FR_OK;
}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got) {
    if(f==test.saved_handle) {
        if(n>f_size(f)-f->fptr)n=(UINT)(f_size(f)-f->fptr);
        memcpy(out,test.read_pixels+f->fptr,n);f->fptr+=n;*got=n;return FR_OK;
    }
    bool clip=f==test.clip_handle;
    size_t size=clip?test.clip_length:test.length;
    assert(f->fptr<=size);
    if(n>size-f->fptr)n=(UINT)(size-f->fptr);
    memcpy(out,(clip?test.clip:test.buffer)+f->fptr,n);f->fptr+=n;*got=n;return FR_OK;
}
FRESULT f_write(FIL *f,const void *data,UINT n,UINT *wrote) {
    if(f==test.ref_handle) {
        assert(test.ref_temp && n==sizeof(test.ref_staging));
        memcpy(test.ref_staging,data,n);*wrote=n;f->obj.objsize=f->fptr=n;return FR_OK;
    }
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
FRESULT f_close(FIL *f) {
    if(f==test.saved_handle)test.saved_handle=NULL;
    if(f==test.ref_handle)test.ref_handle=NULL;
    if(f==test.clip_handle)test.clip_handle=NULL;
    return FR_OK;
}
FRESULT f_rename(const TCHAR *from,const TCHAR *to) {
    if(!strcmp(from,COVER_REF_TEMP)) {
        assert(test.ref_temp);
        if(test.fail_reference) return FR_DISK_ERR;
        assert(test.refs<8 && strlen(to)<sizeof(test.ref_paths[0]));
        strcpy(test.ref_paths[test.refs],to);
        memcpy(test.ref,test.ref_staging,sizeof(test.ref));
        snprintf(test.targets[test.refs++],sizeof(test.targets[0]),KUI_DOWNLOADED_COVERS "/%.32s.kct",test.ref+4);
        test.ref_temp=false;
        return FR_OK;
    }
    assert(test.temp && !strcmp(from,TEMP));if(test.race)return FR_EXIST;
    assert(strlen(to)<sizeof(test.dest));
    if(test.saved) {
        assert(!test.second_bytes && strcmp(to,test.dest));
        strcpy(test.second_dest,to);memcpy(test.second_file,test.buffer,test.length);
        test.second_bytes=test.length;test.temp=false;++test.commits;return FR_OK;
    }
    strcpy(test.dest,to);memcpy(test.saved_file,test.buffer,test.length);test.saved_length=test.length;
    ++test.commits;test.saved=true;test.temp=false;return FR_OK;
}
FRESULT f_unlink(const TCHAR *path) {
    if(test.legacy && !strcmp(path,test.legacy_path)) {
        if(test.deny_cleanup) return FR_DENIED;
        test.legacy=false;
    } else if(!strcmp(path,COVER_REF_TEMP)) {assert(test.ref_temp);test.ref_temp=false;}
    else if(!strcmp(path,TEMP)) {assert(test.temp);test.temp=false;}
    else {assert(!strcmp(path,VIDEO) && test.video);test.video=false;}
    return FR_OK;
}
static struct kui_internet *connect_for_test(void *ctx) {
    static int dummy;
    (void)ctx;assert(test.mounted);
    ++test.connects;
    return (struct kui_internet *)&dummy;
}
static enum kui_media_result fetch(enum kui_media_kind kind) {
    struct kui_media_game selected={.entry=&game,.artwork_mode=test.artwork_mode};
    enum kui_media_result r=kui_media_fetch_internet(&selected,kind,connect_for_test,NULL,cancel,NULL,NULL);
    test.last_reason=selected.reason;
    test.removed=selected.previews_removed;
    test.pending_cleanup=selected.preview_cleanup_pending;
    assert(!test.temp && !test.video && !test.mounted);
    return r;
}
static void grouped_covers(void) {
    char first_cov[KUI_GAMES_FILE_CAP], extra_cov[KUI_GAMES_FILE_CAP];
    assert(kui_games_media_path(FIRST_DISC,KUI_DOWNLOADED_COVERS,"kct",first_cov,sizeof(first_cov)));
    const char *key=strrchr(first_cov,'/')+1;
    memcpy(game.media_disc1_key,key,32);
    for(unsigned old=0;old<2;++old) {
        reset();
        if(old) {
            strcpy(game.path,FIRST_DISC);
            assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED);
        }
        strcpy(game.path,"/Games/Set/2/disc.gdi");
        assert(fetch(KUI_MEDIA_COVER)==(old ? KUI_MEDIA_EXISTING : KUI_MEDIA_INSTALLED));
        assert(kui_media_file_path(&game,KUI_MEDIA_COVER,extra_cov));
        assert(!strcmp(first_cov,test.dest) && test.refs==1 && !strcmp(test.ref_paths[0],extra_cov));
        assert(!memcmp(test.ref,"KCL1",4) && !memcmp(test.ref+4,key,32));
        strcpy(game.path,FIRST_DISC);
        assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING);
        strcpy(game.path,"/Games/Set/3/disc.gdi");
        assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING);
        assert(test.refs==2 && test.commits==1 && test.connects==1 && test.gets==1);
    }
    reset();test.different=true;
    strcpy(game.path,"/Games/Set/2/disc.gdi");
    test.fail_reference=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && test.refs==0 && test.commits==1);
    assert(strcmp(first_cov,test.dest));
    test.fail_reference=false;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING);
    strcpy(game.path,"/Games/Set/3/disc.gdi");
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING);
    assert(test.refs==2 && test.gets==1 && !memcmp(test.ref+4,strrchr(test.dest,'/')+1,32));
    test.saved_file[1000]^=1;
    strcpy(game.path,"/Games/Set/4/disc.gdi");
    test.response=KUI_HTTP_CONNECT_FAILED;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_CONNECT_FAILED && test.refs==2 && test.gets==2);
    assert(test.saved_file[0]=='K' && test.commits==1);
    for(unsigned order=0;order<2;order++) {
        reset();test.different=true;
        strcpy(game.path,order?FIRST_DISC:"/Games/Set/2/disc.gdi");
        assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED);
        strcpy(game.path,order?"/Games/Set/2/disc.gdi":FIRST_DISC);
        assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED);
        assert(test.commits==1 && test.gets==2 && !test.second_bytes && test.refs>=2);
        strcpy(game.path,"/Games/Set/3/disc.gdi");
        assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.gets==2);
        for(unsigned i=0;i<test.refs;i++) assert(!strcmp(test.targets[i],test.dest));
    }
    reset();test.different=true;test.different_pixels=true;
    strcpy(game.path,"/Games/Set/2/disc.gdi");
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED);
    strcpy(game.path,FIRST_DISC);
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.commits==2 && test.gets==2);
    assert(test.second_bytes==test.saved_length && memcmp(test.saved_file,test.second_file,test.second_bytes));
    assert(test.refs==1 && !strcmp(test.targets[0],test.dest) && !strcmp(first_cov,test.second_dest));
    game.media_disc1_key[0]=0;
    strcpy(game.path,"/Games/Example/disc.gdi");
}
static void refresh_covers(void) {
    reset();test.offers_validator=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.commits==1 && test.bodies==1);
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.bodies==1);
    test.artwork_mode=KUI_ART_CHECK;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.conditional==1 && test.bodies==1);
    test.artwork_mode=KUI_ART_REDOWNLOAD;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.conditional==1 && test.bodies==2 && test.commits==1);
    test.artwork_mode=KUI_ART_CHECK;
    test.new_pixels=true;
    test.response=KUI_HTTP_ERROR;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.second_bytes && test.refs==0);
    test.response=KUI_HTTP_OK;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.commits==2 && test.refs==1);
    assert(test.second_bytes==test.saved_length && memcmp(test.second_file,test.saved_file,test.saved_length));
    assert(test.ref_paths[0][strlen(test.ref_paths[0])-1]=='u' && !strcmp(test.targets[0],test.second_dest));
    unsigned bodies=test.bodies;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.bodies==bodies);
    test.owner_cover=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_OWNER && test.bodies==bodies);
}
int main(void) {
    refresh_covers();
    {
        struct kui_artwork_batch checked={0};
        struct kui_games_entry disc={0};
        struct kui_media_game owner={.entry=&disc,.artwork_batch=&checked};
        strcpy(disc.media_disc1_key,"11111111111111111111111111111111");
        assert(!kui_artwork_batch_checked(&owner,"EU front"));
        kui_artwork_batch_remember(&owner,"EU front","first.kct");
        assert(!strcmp(kui_artwork_batch_checked(&owner,"EU front"),"first.kct"));
        assert(!kui_artwork_batch_checked(&owner,"Disc 3 front"));
        kui_artwork_batch_remember(&owner,"Disc 3 front","third.kct");
        assert(!strcmp(kui_artwork_batch_checked(&owner,"Disc 3 front"),"third.kct"));
        strcpy(disc.media_disc1_key,"22222222222222222222222222222222");
        assert(!kui_artwork_batch_checked(&owner,"EU front"));
        kui_artwork_batch_remember(&owner,"EU front","other.kct");
        assert(checked.count==1 && !strcmp(kui_artwork_batch_checked(&owner,"EU front"),"other.kct"));
        disc.media_disc1_key[0]=0;
        assert(!kui_artwork_batch_checked(&owner,"EU front"));
    }
    grouped_covers();
    char path[KUI_GAMES_FILE_CAP];
    reset();assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED && test.commits==1);
    assert(kui_media_file_path(&game,KUI_MEDIA_COVER,path) && !strcmp(path,test.dest));
    assert(test.saved_length==79120 && !memcmp(test.saved_file,"KCT1",4));
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.gets==1 && test.connects==1 && test.identities==1);
    for(unsigned from_cover=0;from_cover<2;++from_cover) {
        reset();test.cover_identity=from_cover;test.response=KUI_HTTP_MISSING;
        struct kui_media_game selected={.entry=&game};
        for(enum kui_media_kind k=KUI_MEDIA_COVER;k<=KUI_MEDIA_PREVIEW;++k)
            assert(kui_media_fetch_internet(&selected,k,connect_for_test,NULL,cancel,NULL,NULL)==KUI_MEDIA_MISSING);
        assert(test.identities==(from_cover?0u:1u) && test.gets==3 && !test.temp && !test.video);
    }
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
    reset();test.small_url=true;test.small_result=KUI_HTTP_CONNECT_FAILED;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_CONNECT_FAILED && test.gets==1 && !test.saved && !test.temp);
    reset();test.small_url=true;test.small_result=KUI_HTTP_LIMITED;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_LIMITED && test.gets==1 && !test.saved);
    reset();test.owner_cover=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_OWNER && !test.connects && !test.identities);
    reset();test.fail_identity=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_UNIDENTIFIED && !test.connects);
    assert(test.last_reason==KUI_MEDIA_REASON_DISC_ID);
    reset();test.has_url=false;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_MISSING && !test.connects);
    assert(test.last_reason==KUI_MEDIA_REASON_NOT_LISTED);
    for(enum kui_media_kind k=KUI_MEDIA_COVER;k<=KUI_MEDIA_PREVIEW;++k) {
        reset();test.has_url=false;test.no_release=true;
        assert(fetch(k)==KUI_MEDIA_UNIDENTIFIED && test.last_reason==KUI_MEDIA_REASON_NO_RELEASE && !test.connects);
        test.no_release=false;
        assert(fetch(k)==KUI_MEDIA_MISSING && test.last_reason==KUI_MEDIA_REASON_NOT_LISTED && !test.connects);
        test.has_url=true;test.response=KUI_HTTP_MISSING;
        assert(fetch(k)==KUI_MEDIA_MISSING && test.last_reason==KUI_MEDIA_REASON_PROVIDER_MISSING);
        test.response=KUI_HTTP_TOO_LARGE;
        assert(fetch(k)==KUI_MEDIA_MISSING && test.last_reason==KUI_MEDIA_REASON_TOO_LARGE);
    }
    reset();test.corrupt_png=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_MISSING && !test.saved);
    assert(test.last_reason==KUI_MEDIA_REASON_INVALID_ASSET);
    reset();test.response=KUI_HTTP_STOPPED;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_STOPPED && !test.saved);
    reset();test.short_write=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.race=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.dirty=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.corrupt_pdf=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_MISSING && !test.saved);
    reset();assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.saved);
    assert(kui_media_original_path(&game,KUI_MEDIA_MANUAL,path) && !strcmp(path,test.dest));
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL_EXISTING && test.gets==1 && test.connects==1);
    reset();test.pages=3;test.has_url=false;
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_INSTALLED && test.gets==3 && !memcmp(test.saved_file,"KDM1\3\0",6));
    assert(kui_media_file_path(&game,KUI_MEDIA_MANUAL,path) && !strcmp(path,test.dest));
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_EXISTING && test.gets==3);
    reset();test.pages=3;test.fail_page=2;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.gets==3);
    assert(test.last_reason==KUI_MEDIA_REASON_NONE);
    test.pages=0;test.has_url=false;test.no_release=true;
    assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL_EXISTING && test.gets==3);
    reset();test.pages=2;test.bad_pages=true;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_ORIGINAL && test.gets==3);
    reset();test.pages=3;test.limit_page=2;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_LIMITED && !test.saved);
    reset();test.pages=3;test.response=KUI_HTTP_STOPPED;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_STOPPED && !test.saved);
    reset();assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && !memcmp(test.saved_file,"KPV2",4));
    /* The mock converter writes KPV2 with zero audio rate: a genuinely
     * silent current clip is already complete in either scope. */
    assert(!test.saved_file[10] && !test.saved_file[11]);
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_EXISTING && test.gets==1 && test.connects==1);
    reset();
    test.legacy=true;
    assert(kui_media_file_path(&game,KUI_MEDIA_PREVIEW,test.legacy_path));
    for(unsigned i=0;i<4;i++) {
        test.response=i==0?KUI_HTTP_MISSING:KUI_HTTP_OK;
        test.dirty=i==1; test.race=i==2; test.cancel_conversion=i==3;
        assert(fetch(KUI_MEDIA_PREVIEW)==(i==0?KUI_MEDIA_MISSING:i==3?KUI_MEDIA_STOPPED:KUI_MEDIA_ERROR));
        test.stop=false;
        FIL old;
        struct kui_media_index index;
        assert(!test.saved && kui_media_open_preview(&old,test.legacy_path)==FR_OK &&
            kui_media_container_index(&old,false,&index) && index.version==1 && f_close(&old)==FR_OK);
    }
    test.cancel_conversion=false;
    test.corrupt_readback=true;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_ERROR && !test.saved && test.legacy && !test.removed);
    test.corrupt_readback=false;test.deny_cleanup=true;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && test.commits==1 && test.legacy &&
        test.pending_cleanup && !test.removed);
    assert(strstr(test.dest,".kp2") && memcmp(test.saved_file,legacy_header,4));
    FIL preferred;
    struct kui_media_index index;
    assert(kui_media_open_preview(&preferred,test.legacy_path)==FR_OK &&
        kui_media_container_index(&preferred,false,&index) && index.version==2 && f_close(&preferred)==FR_OK);
    unsigned completed_gets=test.gets;
    test.deny_cleanup=false;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_EXISTING && test.gets==completed_gets &&
        !test.pending_cleanup && test.removed==1 && !test.legacy);
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_EXISTING && test.gets==completed_gets && !test.removed);
    reset();test.alternate_clips=true;test.missing_previews=2;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && test.preview_requests==3 && test.connects==1);
    reset();test.alternate_clips=true;test.has_url=false;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && test.preview_requests==1);
    reset();test.alternate_clips=true;test.has_url=false;test.runabout=true;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_INSTALLED && test.preview_requests==1);
    reset();test.alternate_clips=true;test.missing_previews=3;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_MISSING && test.last_reason==KUI_MEDIA_REASON_PROVIDER_MISSING &&
           test.preview_requests==3 && !test.saved);
    reset();test.alternate_clips=true;test.response=KUI_HTTP_CONNECT_FAILED;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_CONNECT_FAILED && test.preview_requests==1);
    reset();test.alternate_clips=true;test.response=KUI_HTTP_LIMITED;
    assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_LIMITED && test.preview_requests==1);
    reset();test.convert=false;assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_ERROR && !test.saved);
    reset();test.cancel_conversion=true;assert(fetch(KUI_MEDIA_PREVIEW)==KUI_MEDIA_STOPPED && !test.saved);
    puts("PASS direct media install, indexed page assembly, PDF fallback, preview conversion, cancellation and SD failures");
}
