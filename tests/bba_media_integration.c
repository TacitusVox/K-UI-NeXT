/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the Games worker, release matching, media conversion, cache and
 * two card filesystems. A synthetic HTTPS origin supplies original test media. */
#define _POSIX_C_SOURCE 200809L
#include "kui/internet.h"
#include "kui/hover_clip.h"
#include "kui/internet_cover.h"
#include "kui/games_covers.h"
#include "kui/games_media.h"
#include "kui/games_media_storage.h"
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
static unsigned covers, small_requests, pages, clips;
static bool used_small, large_source;
static size_t origin_bytes;
static const char opaque_url[]="https://fixtures.invalid/covers/13284?region=us";

bool __real_kui_internet_cover_url(const struct kui_games_disc_identity *id, char *url, size_t cap);
bool __wrap_kui_internet_cover_url(const struct kui_games_disc_identity *id, char *url, size_t cap) {
    if(!large_source) return __real_kui_internet_cover_url(id,url,cap);
    if(sizeof(opaque_url)>cap) return false;
    memcpy(url,opaque_url,sizeof(opaque_url));
    return true;
}
void kui_test_start_scan_heap(void);
size_t kui_test_stop_scan_heap(void);
static bool stopping,cancel_page,completed_prefix;
static const char *release_url, *book_url;
static unsigned page_count;

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
    if(large_source && !strcmp(url,opaque_url)) {
        asset="origin"; ++covers;
    } else if(strstr(url,"_thumb.jpg")) {
        char path[4096];
        ++small_requests;
        assert(snprintf(path,sizeof(path),"%s/small.jpg",fixtures) < (int)sizeof(path));
        if(access(path,R_OK)) return KUI_HTTP_MISSING;
        used_small=true;
        asset="small.jpg"; ++covers;
    } else if(strstr(url,"Named_Boxarts/") && strstr(url,release_url) && strstr(url,".png")) {
        asset="box.png";++covers;
    } else if(strstr(url,book_url) && strstr(url,"_w512.jpg")) {
        asset="page.jpg";++pages;
    } else if(strstr(url,"/SegaDreamcastVideoSnaps/") && strstr(url,release_url) && strstr(url,".ogv")) {
        asset="clip.ogv";++clips;
    }
    if(!asset)fprintf(stderr,"Unexpected origin URL: %s\n",url);
    assert(asset);
    char path[4096];
    int n=snprintf(path,sizeof(path),"%s/%s",fixtures,asset);
    assert(n>0 && (size_t)n<sizeof(path));
    FILE *f=fopen(path,"rb");
    struct stat st;
    assert(f && !fstat(fileno(f),&st) && st.st_size>0);
    if((uint64_t)st.st_size>limit) {
        assert(!fclose(f));
        return KUI_HTTP_TOO_LARGE;
    }
    if(large_source) origin_bytes=(size_t)st.st_size;
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
static void report(const struct kui_app_status *s) {
    static char previous[sizeof(s->message)];
    if(strcmp(previous,s->message)) {
        snprintf(previous,sizeof(previous),"%s",s->message);
        puts(previous);
    }
}
static void verify_media(const struct kui_games_entry *e, enum kui_media_kind kind, unsigned count) {
    char path[KUI_GAMES_FILE_CAP];
    FIL file;
    struct kui_media_index index;
    assert(kui_media_file_path(e,kind,path) && f_open(&file,path,FA_READ)==FR_OK);
    assert(kui_media_container_index(&file,kind==KUI_MEDIA_MANUAL,&index));
    assert(index.count==count && f_close(&file)==FR_OK);
}

/* The test fixture has one ungrouped game named "Local Game". */
#define RECORD_PATH KUI_GAMES_COVERS_FOLDER "/Local Game.kcv"
#define LOOSE_PATH KUI_GAMES_COVERS_FOLDER "/Local Game.png"
#define PIXEL_BYTES (KUI_COVER_FILE_BYTES - KUI_COVER_HEADER_BYTES)

static void copy_fixture(const char *name, const char *to) {
    char path[4096];
    assert(snprintf(path,sizeof(path),"%s/%s",fixtures,name) < (int)sizeof(path));
    FILE *src=fopen(path,"rb");
    FIL dst;
    assert(src && f_open(&dst,to,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    uint8_t buf[4096];
    size_t n;
    while((n=fread(buf,1,sizeof(buf),src))) {
        UINT wrote=0;
        assert(f_write(&dst,buf,(UINT)n,&wrote)==FR_OK && wrote==n);
    }
    assert(!ferror(src) && !fclose(src) && f_close(&dst)==FR_OK);
}

static void cover_only(struct kui_games_media_job *job, bool expect_fetch) {
    unsigned previous=covers, attempts=small_requests;
    struct kui_app_status status;
    job->all=false;
    job->kinds=KUI_GAMES_MEDIA_ART;
    kui_games_media_run(job,&status,log_line,cancelled,report);
    assert(status.passed && covers==previous+(expect_fetch?1u:0u) && !card_open && !mock.online);
    if(!expect_fetch) assert(small_requests==attempts);
}

static void offline_scan(void) {
    struct kui_app_status status;
    struct kui_games_scan_counts counts;
    kui_games_covers_cache_clear();
    assert(kui_games_scan(&status,&counts,report,log_line,cancelled) && counts.games==1u);
}

static void visible_pixels(const struct kui_games_entry *game, uint16_t *all) {
    struct kui_games_cover_result cover;
    for(enum kui_cover_size size=KUI_COVER_SIZE_LARGE;size<=KUI_COVER_SIZE_SMALL;++size) {
        unsigned edge=kui_cover_edge(size);
        assert(kui_games_selected_cover(game,size,all,&cover,log_line,cancelled) && cover.cover);
        all+=edge*edge;
    }
}

static void assert_new_layout(const struct kui_games_entry *game, const uint16_t *pixels) {
    char compact[KUI_GAMES_FILE_CAP], legacy[KUI_GAMES_FILE_CAP];
    FILINFO info;
    assert(kui_media_file_path(game,KUI_MEDIA_COVER,compact) && strstr(compact,".kct"));
    assert(kui_games_media_path(game->path,KUI_DOWNLOADED_COVERS,"png",legacy,sizeof(legacy)));
    assert(f_stat(legacy,&info)==FR_NO_FILE);
    assert(f_stat(RECORD_PATH,&info)==FR_OK && info.fsize==KUI_COVER_HEADER_BYTES);
    DIR directory;
    unsigned assets=0;
    assert(f_opendir(&directory,KUI_DOWNLOADED_COVERS)==FR_OK);
    while(true) {
        assert(f_readdir(&directory,&info)==FR_OK);
        if(!info.fname[0]) break;
        assert(!strcmp(info.fname,strrchr(compact,'/')+1));
        ++assets;
    }
    assert(assets==1 && f_closedir(&directory)==FR_OK);
    FIL file;
    uint8_t head[16], sample[1024];
    UINT got;
    assert(f_open(&file,compact,FA_READ)==FR_OK && f_size(&file)==sizeof(head)+PIXEL_BYTES);
    assert(f_read(&file,head,sizeof(head),&got)==FR_OK && got==sizeof(head));
    assert(!memcmp(head,"KCT1",4));
    for(unsigned at=0;at<PIXEL_BYTES;) {
        UINT n=PIXEL_BYTES-at<sizeof(sample) ? PIXEL_BYTES-at : sizeof(sample);
        assert(f_read(&file,sample,n,&got)==FR_OK && got==n);
        for(unsigned i=0;i<n;i+=2)
            assert((uint16_t)(sample[i] | sample[i+1]<<8)==pixels[(at+i)/2]);
        at+=n;
    }
    assert(f_close(&file)==FR_OK);
}

/* Model an installed, complete old-format download. Send no new request when
 * migrating the full-sized source, even if a valid old cache already exists. */
static void seed_old_download(const struct kui_games_entry *game) {
    char compact[KUI_GAMES_FILE_CAP], legacy[KUI_GAMES_FILE_CAP];
    FIL source, record;
    UINT n;
    uint8_t header[KUI_COVER_HEADER_BYTES], bytes[4096];
    struct kui_cover_record old;
    assert(kui_games_media_path(game->path,KUI_DOWNLOADED_COVERS,"png",legacy,sizeof(legacy)));
    assert(kui_media_file_path(game,KUI_MEDIA_COVER,compact));
    copy_fixture("box.png",legacy);
    FILINFO info;
    assert(f_stat(legacy,&info)==FR_OK);
    assert(f_open(&record,RECORD_PATH,FA_READ|FA_WRITE)==FR_OK);
    assert(f_read(&record,header,sizeof(header),&n)==FR_OK && n==sizeof(header));
    assert(kui_cover_header_decode(&old,header));
    old.source=KUI_COVER_SOURCE_USER;
    old.user=(struct kui_cover_stamp){info.fsize,info.fdate,info.ftime};
    assert(kui_cover_header_encode(header,&old) && f_lseek(&record,0)==FR_OK);
    assert(f_write(&record,header,sizeof(header),&n)==FR_OK && n==sizeof(header));
    assert(f_open(&source,compact,FA_READ)==FR_OK && f_lseek(&source,16)==FR_OK);
    do {
        UINT wrote;
        assert(f_read(&source,bytes,sizeof(bytes),&n)==FR_OK);
        assert(f_write(&record,bytes,n,&wrote)==FR_OK && wrote==n);
    } while(n==sizeof(bytes));
    assert(f_size(&record)==KUI_COVER_FILE_BYTES);
    assert(f_close(&source)==FR_OK && f_close(&record)==FR_OK && f_unlink(compact)==FR_OK);
    printf("Previous on-card cover %lu bytes; compact cover %u bytes including metadata\n",
           (unsigned long)(info.fsize+KUI_COVER_FILE_BYTES), (unsigned)(16u+PIXEL_BYTES+KUI_COVER_HEADER_BYTES));
}

static void exercise_cover(struct kui_games_media_job *job, const char *thumbs) {
    static uint16_t reference[PIXEL_BYTES/2], after[PIXEL_BYTES/2];
    const struct kui_games_entry *game=&job->selected;
    visible_pixels(game,reference);
    if(thumbs) {
        FILE *out=fopen(thumbs,"wb");
        assert(out);
        for(unsigned i=0;i<PIXEL_BYTES/2;++i)
            assert(fputc(reference[i]&255,out)!=EOF && fputc(reference[i]>>8,out)!=EOF);
        assert(!fclose(out));
    } else assert(reference[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0xf800);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert_new_layout(game,reference);
    assert(f_unlink(RECORD_PATH)==FR_OK && kui_games_storage_release());
    offline_scan();
    visible_pixels(game,after);
    assert(!memcmp(reference,after,sizeof(after)));
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert_new_layout(game,reference);
    if(!used_small) {
        seed_old_download(game);
        assert(kui_games_storage_release());
        kui_games_covers_cache_clear();
        cover_only(job,false);
        assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
        assert_new_layout(game,reference);
    }
    copy_fixture("custom.png",LOOSE_PATH);
    assert(kui_games_storage_release());
    offline_scan();
    visible_pixels(game,after);
    assert(after[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0x07e0);
    cover_only(job,false);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_unlink(LOOSE_PATH)==FR_OK && kui_games_storage_release());
    offline_scan();
    visible_pixels(game,after);
    assert(!memcmp(reference,after,sizeof(after)));
    strcpy(job->selected.cover_path,"/kui/owner.png");
    kui_games_covers_cache_clear();
    cover_only(job,false);
    visible_pixels(game,after);
    assert(after[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0x07e0);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert_new_layout(game,reference);
    assert(kui_games_storage_release());

    /* A second game's explicit cover assignment can point at the managed PNG
     * created by the prior version. Its on-card path must still open. */
    struct kui_games_entry assigned=*game;
    strcpy(assigned.name,"Second game");
    strcpy(assigned.path,"/Games/Second game/disc.gdi");
    char legacy[KUI_GAMES_FILE_CAP];
    assert(kui_games_media_path(game->path,KUI_DOWNLOADED_COVERS,"png",legacy,sizeof(legacy)));
    char normalized[KUI_GAME_LAUNCH_SETTINGS_PATH_CAP];
    assert(kui_game_launch_settings_normalize(normalized,legacy));
    assert(strlen(normalized)<sizeof(assigned.cover_path));
    strcpy(assigned.cover_path,normalized);
    visible_pixels(&assigned,after);
    assert(!memcmp(reference,after,sizeof(after)));
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);

    /* Explicit scans repair the managed pixels if the metadata survives a
     * missing master or a damaged block in the middle plane. */
    char compact[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(game,KUI_MEDIA_COVER,compact));
    job->selected.cover_path[0]=0;
    for(unsigned missing=0;missing<2;++missing) {
        if(missing) {
            assert(f_unlink(compact)==FR_OK);
        } else {
            FIL f;
            uint8_t b;
            UINT n;
            FSIZE_t position=16u+KUI_COVER_LARGE*KUI_COVER_LARGE*2u+6;
            assert(f_open(&f,compact,FA_READ|FA_WRITE)==FR_OK);
            assert(f_lseek(&f,position)==FR_OK && f_read(&f,&b,1,&n)==FR_OK && n==1);
            b^=0xff;
            assert(f_lseek(&f,position)==FR_OK && f_write(&f,&b,1,&n)==FR_OK && n==1 && f_close(&f)==FR_OK);
        }
        assert(kui_games_storage_release());
        kui_games_covers_cache_clear();
        cover_only(job,true);
        assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
        assert_new_layout(game,reference);
    }
    assert(kui_games_storage_release());
}
int main(int argc,char **argv) {
    if(argc<3 || argc>5) return 2;
    large_source=argc==5 && !strcmp(argv[4],"4k");
    bool skies=argc==5 && !strcmp(argv[4],"skies");
    release_url=skies?"Skies%20of%20Arcadia":"Crazy%20Taxi";
    book_url=skies?"/Skies_Of_Arcadia_2000_U/page/n":"/Crazy_Taxi_2000_U/page/n";
    page_count=skies?18:13;
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
    if(large_source) {
        static uint16_t pixels[PIXEL_BYTES/2];
        job.all=false;
        job.kinds=KUI_GAMES_MEDIA_ART;
        kui_test_start_scan_heap();
        kui_games_media_run(&job,&status,log_line,cancelled,report);
        size_t peak=kui_test_stop_scan_heap();
        assert(status.passed && covers==1 && small_requests==1 && origin_bytes>3u*1024u*1024u);
        /* Both streaming decoders fit these 4K fixtures into a 1 MiB scan budget. */
        assert(peak>0 && peak<1024u*1024u);
        kui_games_covers_cache_clear();
        visible_pixels(&job.selected,pixels);
        assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
        assert_new_layout(&job.selected,pixels);
        assert(kui_games_storage_release());
        cover_only(&job,false);
        FILE *out=fopen(argv[3],"wb");
        assert(out);
        for(unsigned i=0;i<PIXEL_BYTES/2;++i)
            assert(fputc(pixels[i]&255,out)!=EOF && fputc(pixels[i]>>8,out)!=EOF);
        assert(!fclose(out) && !fclose(card));
        printf("4K Games scan: %zu input bytes, %zu peak bytes of new heap allocations, %u thumbnail bytes\n",
               origin_bytes,peak,(unsigned)(16+PIXEL_BYTES+KUI_COVER_HEADER_BYTES));
        return 0;
    }
    cancel_page=true;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(status.stopped && covers==1 && pages==1 && !clips && !card_open && !mock.online);
    stopping=false;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    log_line("Result: %s",status.message);
    assert(status.passed && small_requests==1 && covers==1 && pages==1+page_count && clips==1);
    char primary[4096];
    assert(snprintf(primary,sizeof(primary),"%s/small.jpg",fixtures)<(int)sizeof(primary));
    assert(used_small==(access(primary,R_OK)==0));
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    verify_media(&job.selected,KUI_MEDIA_MANUAL,page_count);
    verify_media(&job.selected,KUI_MEDIA_PREVIEW,100);
    /* Browse the same saved clip from an open card, then skip to its last
     * frame as a late background reader would. No network session is active. */
    struct kui_hover_clip hover = {0};
    static uint16_t first_frame[KUI_PREVIEW_FRAME_BYTES/2], last_frame[KUI_PREVIEW_FRAME_BYTES/2];
    assert(kui_hover_clip_open(&hover,&job.selected,log_line,cancelled) && hover.frames == 100);
    assert(kui_hover_clip_read(&hover,0,first_frame,cancelled));
    assert(kui_hover_clip_read(&hover,99,last_frame,cancelled));
    assert(memcmp(first_frame,last_frame,sizeof(first_frame)));
    kui_hover_clip_close(&hover);
    assert(!hover.open && !mock.online);
    assert(completed_prefix);
    assert(kui_games_storage_release());
    /* Selected-game run must reuse the completed files without another GET. */
    job.all=false;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(status.passed && small_requests==1 && covers==1 && pages==1+page_count && clips==1 && !card_open);
    exercise_cover(&job,argc>=4 && strcmp(argv[3],"-") ? argv[3] : NULL);
    assert(!card_open && !fclose(card));
    puts("PASS library download, on-card pixels/pages/frames, and selected-game skip");
    return 0;
}
