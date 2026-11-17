/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the Games worker, release matching, media conversion, cache and
 * two card filesystems. A synthetic HTTPS origin supplies original test media. */
#define _POSIX_C_SOURCE 200809L
#include "kui/internet.h"
#include "kui/games_covers.h"
#include "kui/games_media.h"
#include "kui/games_storage.h"
#include "kui/media.h"
#include "kui/media_container.h"
#include "kui/media_download.h"
#include "kui/storage.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static FILE *card;
static uint64_t sectors;
static const char *fixtures;
static bool card_open;
static unsigned covers,pages,clips;
static bool stopping,cancel_page,completed_prefix;

static void log_line(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}
static uint64_t blocks(void *unused) { (void)unused; return sectors; }
static int read_blocks(void *ctx, uint32_t block, size_t count, uint8_t *data) {
    return fseeko(ctx, (off_t)block*512, SEEK_SET) || fread(data, 512, count, ctx) != count;
}
static int write_blocks(void *ctx, uint32_t block, size_t count, const uint8_t *data) {
    return fseeko(ctx, (off_t)block*512, SEEK_SET) || fwrite(data, 512, count, ctx) != count;
}
static int sync_blocks(void *ctx) { return fflush(ctx); }
unsigned kui_storage_active(void) { return KUI_STORAGE_SCI; }
bool kui_sd_connect(void) {
    assert(!card_open);
    card_open = true;
    struct kui_media_ops media = {card, blocks, read_blocks, write_blocks, sync_blocks};
    kui_media_set(&media);
    return true;
}
void kui_sd_disconnect(void) { assert(card_open); card_open = false; kui_media_set(NULL); }

struct kui_internet { bool online; };
static struct kui_internet mock;
struct kui_internet *kui_internet_open(enum kui_internet_route route,kui_log_fn log,kui_cancel_fn cancel,char problem[128]) {
    (void)log;(void)problem;
    assert(route==KUI_INTERNET_AUTO && !cancel() && card_open && !mock.online);
    mock.online=true;
    return &mock;
}
const char *kui_internet_adapter(const struct kui_internet *s) {assert(s==&mock && s->online);return "Fixture";}
const char *kui_internet_problem(const struct kui_internet *s) {assert(s==&mock && s->online);return "";}
void kui_internet_close(struct kui_internet *s) {assert(s==&mock && s->online);s->online=false;}

enum kui_http_result kui_internet_get_prefix(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn write,void *ctx,kui_http_progress_fn progress,void *progress_ctx,
        bool (*complete)(void *ctx)) {
    assert(s==&mock && s->online && card_open && !stopping && !strncmp(url,"https://",8));
    const char *asset=NULL;
    if(strstr(url,"Named_Boxarts/") && strstr(url,"Crazy%20Taxi") && strstr(url,".png")) {
        asset="box.png";++covers;
    } else if(strstr(url,"/Crazy_Taxi_2000_U/page/n") && strstr(url,"_w512.jpg")) {
        asset="page.jpg";++pages;
    } else if(strstr(url,"/SegaDreamcastVideoSnaps/Crazy%20Taxi") && strstr(url,".ogv")) {
        asset="clip.ogv";++clips;
    }
    if(!asset)fprintf(stderr,"Unexpected origin URL: %s\n",url);
    assert(asset);
    char path[4096];
    int n=snprintf(path,sizeof(path),"%s/%s",fixtures,asset);
    assert(n>0 && (size_t)n<sizeof(path));
    FILE *f=fopen(path,"rb");
    struct stat st;
    assert(f && !fstat(fileno(f),&st) && st.st_size>0 && (uint64_t)st.st_size<=limit);
    uint32_t received=0,total=(uint32_t)st.st_size;
    unsigned char bytes[317];
    while(received<total) {
        size_t size=fread(bytes,1,sizeof(bytes),f);
        assert(size && write(ctx,bytes,size));
        received+=(uint32_t)size;
        if(progress)progress(progress_ctx,received,total);
        if(complete && complete(ctx)) {
            assert(clips==1 && received<total);
            completed_prefix=true;
            break;
        }
        if(cancel_page && asset[0]=='p') {
            cancel_page=false;stopping=true;
            assert(!fclose(f));
            return KUI_HTTP_STOPPED;
        }
    }
    assert(!fclose(f));
    return KUI_HTTP_OK;
}
enum kui_http_result kui_internet_get(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn write,void *ctx,kui_http_progress_fn progress,void *progress_ctx) {
    return kui_internet_get_prefix(s,url,limit,write,ctx,progress,progress_ctx,NULL);
}
static bool cancelled(void) { return stopping; }
static void report(const struct kui_app_status *s) { printf("%s\n",s->message); }
static void verify_media(const struct kui_games_entry *e, enum kui_media_kind kind, unsigned count) {
    char path[KUI_GAMES_FILE_CAP];
    FIL file;
    struct kui_media_index index;
    assert(kui_media_file_path(e,kind,path) && f_open(&file,path,FA_READ)==FR_OK);
    assert(kui_media_container_index(&file,kind==KUI_MEDIA_MANUAL,&index));
    assert(index.count==count && f_close(&file)==FR_OK);
}

int main(int argc,char **argv) {
    if(argc!=3 && argc!=4) return 2;
    fixtures=argv[2];
    card=fopen(argv[1],"r+b");
    struct stat st;
    assert(card && !fstat(fileno(card),&st));
    sectors=(uint64_t)st.st_size/512;
    struct kui_games_page page;
    assert(kui_games_list("/Games",0,&page,log_line,cancelled) && page.count==1);
    struct kui_games_media_job job = {.all=true,.kinds=KUI_GAMES_MEDIA_ART|KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,
        .selected=page.entries[0]};
    struct kui_app_status status;
    /* A rescan first caches the blue disc texture; internet box art supersedes it. */
    struct kui_games_scan_counts counts;
    assert(kui_games_scan(&status,&counts,report,log_line,cancelled) && counts.disc==1);
    kui_games_storage_hold(true);
    cancel_page=true;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(status.stopped && covers==1 && pages==1 && !clips && !card_open && !mock.online);
    stopping=false;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    log_line("Result: %s",status.message);
    assert(status.passed && covers==1 && pages==14 && clips==1);
    struct kui_games_cover_result cover;
    static uint16_t pixels[KUI_COVER_PIXELS];
    assert(kui_games_selected_cover(&job.selected,KUI_COVER_SIZE_LARGE,pixels,&cover,log_line,cancelled));
    assert(cover.cover);
    if(argc==4) {
        FILE *thumbs=fopen(argv[3],"wb");
        assert(thumbs);
        for(enum kui_cover_size size=KUI_COVER_SIZE_LARGE;size<=KUI_COVER_SIZE_SMALL;++size) {
            assert(kui_games_selected_cover(&job.selected,size,pixels,&cover,log_line,cancelled) && cover.cover);
            unsigned edge=kui_cover_edge(size);
            for(unsigned i=0;i<edge*edge;++i) {
                assert(fputc(pixels[i]&255,thumbs)!=EOF && fputc(pixels[i]>>8,thumbs)!=EOF);
            }
        }
        assert(!fclose(thumbs));
    } else {
        assert(pixels[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0xf800);
    }
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    verify_media(&job.selected,KUI_MEDIA_MANUAL,13);
    verify_media(&job.selected,KUI_MEDIA_PREVIEW,100);
    assert(completed_prefix);
    assert(kui_games_storage_release());
    /* Selected-game run must reuse the completed files without another GET. */
    job.all=false;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(status.passed && covers==1 && pages==14 && clips==1 && !card_open && !fclose(card));
    puts("PASS library download, on-card pixels/pages/frames, and selected-game skip");
    return 0;
}
