/* SPDX-License-Identifier: GPL-3.0-only */
/* Fault-injection for the production download worker. Real-card and TCP
 * integration runs separately in test_bba_media_integration.py. */
#include "kui/media_download.h"
#include "kui/games_media_storage.h"
#include "kui/games_storage.h"
#include "fixtures/cover_images.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    uint8_t response[20000], file[20000];
    char request[1024], final[KUI_GAMES_FILE_CAP];
    size_t response_size, response_at, request_size, file_size, fragment;
    uint64_t now;
    unsigned opens, closes, commits, polls, cut_after;
    bool connected, mounted, temporary, published, selected, connected_wait, stall, short_write, rename_race, sync_fail;
    enum kui_games_identity_result identity;
} test;
static const struct kui_games_entry game = {.name="Example",.path="/Games/Example/disc.gdi"};
static void reset(void) {
    memset(&test,0,sizeof(test));
    test.fragment=13;
    test.identity=KUI_GAMES_IDENTITY_FOUND;
}
static void reply(const char *headers,const void *data,size_t size) {
    size_t n=strlen(headers);
    assert(n+size<=sizeof(test.response));
    memcpy(test.response,headers,n);
    if(size) memcpy(test.response+n,data,size);
    test.response_size=n+size;
}
static void png_response(const uint8_t *png,size_t length) {
    char headers[160];
    snprintf(headers,sizeof(headers),"HTTP/1.0 200 OK\r\nContent-Length: %zu\r\nContent-Type: image/png\r\n\r\n",length);
    reply(headers,png,length);
}
static bool cancel(void) { return test.cut_after && test.response_at>=test.cut_after; }
static bool dial(void *ctx,unsigned slot,uint16_t local,const uint8_t ip[4],uint16_t port) {
    (void)ctx;assert(!slot && !local && ip[0]==192 && port==8732);
    assert(!test.connected);test.connected=true;++test.opens;return true;
}
static bool sock_close(void *ctx,unsigned slot) {
    (void)ctx;assert(!slot && test.connected);test.connected=false;++test.closes;return true;
}
static bool state(void *ctx,unsigned slot,uint8_t *v) {
    (void)ctx;assert(!slot && test.connected);
    *v=test.connected_wait?KUI_NET_CONNECTING:
        test.response_at<test.response_size || test.stall?KUI_NET_ESTABLISHED:KUI_NET_PEER_CLOSED;
    return true;
}
static bool room(void *ctx,unsigned slot,size_t *n) { (void)ctx;(void)slot;*n=50;return true; }
static bool send_data(void *ctx,unsigned slot,const void *data,size_t n) {
    (void)ctx;(void)slot;
    assert(test.request_size+n<sizeof(test.request));
    memcpy(test.request+test.request_size,data,n);test.request_size+=n;return true;
}
static bool available(void *ctx,unsigned slot,size_t *n) {
    (void)ctx;(void)slot;*n=test.response_size-test.response_at;
    if(*n>test.fragment)*n=test.fragment;
    return true;
}
static bool receive(void *ctx,unsigned slot,void *data,size_t n) {
    (void)ctx;(void)slot;assert(test.response_at+n<=test.response_size);
    memcpy(data,test.response+test.response_at,n);test.response_at+=n;return true;
}
static bool poll_net(struct kui_bba_session *s) { (void)s;++test.polls;return true; }
static uint64_t clock_ms(void) { return test.now; }
static void advance(unsigned ms) { assert(ms);test.now+=1000; }
static enum kui_media_result fetch(enum kui_media_kind kind) {
    struct kui_bba_session bba={.net={.count=1,.connect=dial,.close=sock_close,.room=room,.send=send_data,
        .state=state,.received=available,.receive=receive}};
    const struct kui_bba_port port={.poll=poll_net,.now_ms=clock_ms,.pause=advance};
    enum kui_media_result result=kui_media_fetch(&port,&bba,(uint8_t[]){192,168,1,2},8732,&game,kind,NULL,cancel);
    assert(!test.connected && !test.mounted && !test.temporary && test.opens==test.closes);
    return result;
}

enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;assert(!test.mounted);test.mounted=true;return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_done(bool keep) { (void)keep;assert(test.mounted);test.mounted=false;return true; }
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,kui_log_fn log,kui_cancel_fn c) {
    assert(entry==&game && test.mounted);(void)log;(void)c;*present=test.selected||test.published;return true;
}
enum kui_games_identity_result kui_games_media_identity(const char *path,struct kui_games_disc_identity *out,
        kui_log_fn log,kui_cancel_fn c) {
    (void)log;(void)c;assert(!strcmp(path,game.path));memset(out,0,sizeof(*out));
    strcpy(out->product,"T-1 A");strcpy(out->region,"JUE");strcpy(out->title,"A & B?%");return test.identity;
}
bool kui_game_image_name_supported(const char *name) { return !strcmp(name,"disc.gdi"); }
FRESULT f_stat(const TCHAR *path,FILINFO *info) {
    assert(!strcmp(path,"0:/KUI") || !strcmp(path,"0:/KUI/media") || !strcmp(path,KUI_DOWNLOADED_COVERS));info->fattrib=AM_DIR;return FR_OK;
}
FRESULT f_mkdir(const TCHAR *path) { (void)path;assert(0);return FR_DENIED; }
FRESULT f_open(FIL *fp,const TCHAR *path,BYTE mode) {
    memset(fp,0,sizeof(*fp));
    if(!strcmp(path,"0:/KUI/media/fetch.tmp")) {
        assert(mode & FA_CREATE_ALWAYS);test.temporary=true;test.file_size=0;return FR_OK;
    }
    if(!test.published || strcmp(path,test.final))return FR_NO_FILE;
    assert(mode==FA_READ);fp->obj.objsize=test.file_size;return FR_OK;
}
FRESULT f_read(FIL *fp,void *out,UINT n,UINT *got) {
    assert(fp->fptr<=test.file_size);
    if(n>test.file_size-fp->fptr)n=(UINT)(test.file_size-fp->fptr);
    memcpy(out,test.file+fp->fptr,n);fp->fptr+=n;*got=n;return FR_OK;
}
FRESULT f_write(FIL *fp,const void *data,UINT n,UINT *wrote) {
    if(test.short_write && n)n--;
    assert(test.temporary && test.file_size+n<=sizeof(test.file));
    memcpy(test.file+test.file_size,data,n);test.file_size+=n;
    fp->fptr=fp->obj.objsize=test.file_size;*wrote=n;return FR_OK;
}
FRESULT f_lseek(FIL *fp,FSIZE_t at) { assert(at<=test.file_size);fp->fptr=at;return FR_OK; }
FRESULT f_sync(FIL *fp) { (void)fp;return test.sync_fail?FR_DISK_ERR:FR_OK; }
FRESULT f_close(FIL *fp) { (void)fp;return FR_OK; }
FRESULT f_rename(const TCHAR *from,const TCHAR *to) {
    assert(test.temporary && !strcmp(from,"0:/KUI/media/fetch.tmp"));
    if(test.rename_race)return FR_EXIST;
    assert(!test.published && strlen(to)<sizeof(test.final));
    strcpy(test.final,to);test.published=true;test.temporary=false;++test.commits;return FR_OK;
}
FRESULT f_unlink(const TCHAR *path) {
    assert(!strcmp(path,"0:/KUI/media/fetch.tmp") && test.temporary);test.temporary=false;return FR_OK;
}
static void expect_response(const char *response,enum kui_media_result result) {
    reset();reply(response,NULL,0);assert(fetch(KUI_MEDIA_COVER)==result && !test.commits);
}
int main(void) {
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb));assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_INSTALLED);
    char path[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(&game,KUI_MEDIA_COVER,path));
    assert(test.commits==1 && !strcmp(test.final,path) && !strncmp(path,KUI_DOWNLOADED_COVERS "/",strlen(KUI_DOWNLOADED_COVERS)+1));
    assert(strstr(test.request,"product=T-1%20A&region=JUE&title=A%20%26%20B%3F%25 HTTP/1.0\r\n"));
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && test.opens==1);
    reset();test.selected=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_EXISTING && !test.opens);
    reset();test.identity=KUI_GAMES_IDENTITY_UNKNOWN;assert(fetch(KUI_MEDIA_MANUAL)==KUI_MEDIA_UNIDENTIFIED && !test.opens);
    expect_response("HTTP/1.0 404 missing\r\n\r\n",KUI_MEDIA_MISSING);
    expect_response("HTTP/1.0 413 too large\r\n\r\n",KUI_MEDIA_MISSING);
    expect_response("HTTP/1.0 415 unsupported\r\n\r\n",KUI_MEDIA_MISSING);
    expect_response("HTTP/1.0 422 cannot convert\r\n\r\n",KUI_MEDIA_MISSING);
    expect_response("HTTP/1.0 429 quota\r\n\r\n",KUI_MEDIA_LIMITED);
    expect_response("HTTP/1.0 503 waiting\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 302 found\r\nLocation: /v2\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Length: 262145\r\nContent-Type: image/png\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Length: 9\r\nContent-Length: 9\r\nContent-Type: image/png\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Length: 9\r\nTransfer-Encoding: chunked\r\nContent-Type: image/png\r\n\r\n",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Length: 9\r\nContent-Type: text/html\r\n\r\n<html>",KUI_MEDIA_ERROR);
    expect_response("HTTP/1.1 200 OK\r\nContent-Length: 9\r\nContent-Type: image/png\r\n\r\nshort",KUI_MEDIA_ERROR);
    reset();memset(test.response,'x',5000);test.response_size=5000;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR);
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb)-8);assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.commits);
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb));test.cut_after=150;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_STOPPED && !test.commits);
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb));test.short_write=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.commits);
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb));test.rename_race=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.commits);
    reset();png_response(cover_png_rgb,sizeof(cover_png_rgb));test.sync_fail=true;
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && !test.commits);
    reset();test.connected_wait=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && test.now==10000u);
    reset();test.stall=true;assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && test.now==600000u);
    reset();test.stall=true;reply("HTTP/1.0 200 OK\r\nContent-Length: 100\r\nContent-Type: image/png\r\n\r\nx",NULL,0);
    assert(fetch(KUI_MEDIA_COVER)==KUI_MEDIA_ERROR && test.now==20000u);
    puts("PASS bounded media HTTP, local preservation, cancellation, deadlines and SD publication");
}
