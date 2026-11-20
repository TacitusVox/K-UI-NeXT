/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the Games worker, release matching, media conversion, cache and
 * two card filesystems. A synthetic HTTPS origin supplies original test media. */
#define _POSIX_C_SOURCE 200809L
#include "kui/internet.h"
#include "kui/compact_cover.h"
#include "kui/artwork_validators.h"
#include "kui/hover_clip.h"
#include "kui/internet_cover.h"
#include "kui/games_covers.h"
#include "kui/games_media.h"
#include "kui/games_library.h"
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
static bool count_cleanup_reads;
static unsigned cleanup_sectors_read;
static const char *fixtures;
static bool card_open;
static const char *blocked_preview_unlink;
FRESULT __real_f_unlink(const TCHAR *path);
FRESULT __wrap_f_unlink(const TCHAR *path) {
    if(blocked_preview_unlink && !strcmp(path,blocked_preview_unlink)) return FR_DENIED;
    return __real_f_unlink(path);
}
static unsigned covers, small_requests, pages, clips, connections;
static unsigned art_revision, conditional_bodies, unchanged_responses;
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
static bool stopping,cancel_page,completed_prefix,missing_pages,fail_clip;
static unsigned pdfs;
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
    if(count_cleanup_reads) cleanup_sectors_read+=(unsigned)count;
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
    ++connections;
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
        asset=art_revision==2?"custom.png":"box.png";++covers;
    } else if(strstr(url,book_url) && strstr(url,"_w512.jpg")) {
        ++pages;
        if(missing_pages) return KUI_HTTP_MISSING;
        asset="page.jpg";
    } else if(strstr(url,"skies_arcadia.pdf")) {
        asset="original.pdf"; ++pdfs;
    } else if(strstr(url,"/SegaDreamcastVideoSnaps/") && strstr(url,release_url) && strstr(url,".ogv")) {
        asset="clip.ogv";++clips;
        if(fail_clip) return KUI_HTTP_CONNECT_FAILED;
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
            assert(asset && !strcmp(asset,"clip.ogv") && received<total);
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
enum kui_http_result kui_internet_get_conditional(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn sink,void *ctx,kui_http_progress_fn progress,void *progress_ctx,
        const struct kui_http_validators *known,struct kui_http_validators *received) {
    memset(received,0,sizeof(*received));
    if(art_revision) {
        if(strstr(url,"_thumb.jpg")) return KUI_HTTP_MISSING;
        const char *tag=art_revision==2?"\"green\"":"\"original\"";
        strcpy(received->etag,tag);
        struct kui_sha256 hash;kui_sha256_init(&hash);kui_sha256_update(&hash,url,strlen(url));
        kui_sha256_digest(&hash,received->resource);
        if(known && !strcmp(known->etag,tag) && !memcmp(known->resource,received->resource,sizeof(known->resource))) {
            ++unchanged_responses; return KUI_HTTP_NOT_MODIFIED;
        }
        ++conditional_bodies;
    }
    return kui_internet_get(s,url,limit,sink,ctx,progress,progress_ctx);
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
    unsigned previous=covers, attempts=small_requests, opens=connections;
    struct kui_app_status status;
    job->all=false;
    job->kinds=KUI_GAMES_MEDIA_ART;
    kui_games_media_run(job,&status,log_line,cancelled,report);
    assert(status.passed && covers==previous+(expect_fetch?1u:0u) && !card_open && !mock.online);
    if(!expect_fetch) assert(small_requests==attempts);
    assert(connections==opens+(expect_fetch?1u:0u));
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

static void art_preference(enum kui_games_art_preference value) {
    enum kui_games_art_preference loaded;
    assert(kui_games_art_preference_save(value,log_line));
    assert(kui_games_storage_release());
    kui_games_covers_cache_clear();
    assert(kui_games_art_preference_get(&loaded,log_line) && loaded == value);
}

static void check_sources(const struct kui_games_entry *game,const uint16_t *download) {
    static uint16_t actual[PIXEL_BYTES/2];
    unsigned mid=KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2;
    enum kui_games_art_preference loaded;
    assert(kui_games_art_preference_get(&loaded,log_line) && loaded==KUI_GAMES_ART_DOWNLOADED_FIRST);
    /* Model an existing card with only the internet download and its header. */
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    FRESULT r=f_unlink(KUI_GAMES_COVERS_FOLDER "/Local Game.kcd");
    assert((r==FR_OK || r==FR_NO_FILE) && kui_games_storage_release());
    art_preference(KUI_GAMES_ART_DISC_FIRST);
    visible_pixels(game,actual);
    assert(actual[mid]==0x001f && memcmp(actual,download,sizeof(actual)));
    struct kui_games_detail detail;
    assert(kui_games_inspect_cover(game->path,&detail,actual,log_line,cancelled));
    assert(detail.cover && actual[mid]==0x001f);

    /* Remove the synthetic texture directory entry, as on a disc without
     * 0GDTEX, then restore its exact original bytes. */
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    FIL track;
    UINT got;
    const FSIZE_t root_at=20u*2352u+16u;
    uint8_t sector[2048];
    assert(f_open(&track,"0:/Games/Local Game/track03.bin",FA_READ|FA_WRITE)==FR_OK);
    assert(f_lseek(&track,root_at)==FR_OK && f_read(&track,sector,sizeof(sector),&got)==FR_OK && got==sizeof(sector));
    unsigned offset=0;
    while(offset+10<sizeof(sector) && memcmp(sector+offset,"0GDTEX.PVR",10)) offset++;
    assert(offset+10<sizeof(sector) && f_lseek(&track,root_at+offset)==FR_OK);
    assert(f_write(&track,"X",1,&got)==FR_OK && got==1 && f_close(&track)==FR_OK);
    assert(f_unlink(KUI_GAMES_COVERS_FOLDER "/Local Game.kcd")==FR_OK && kui_games_storage_release());
    kui_games_covers_content_clear();
    visible_pixels(game,actual);
    assert(!memcmp(actual,download,sizeof(actual)));
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    FILINFO info;
    assert(f_stat(KUI_GAMES_COVERS_FOLDER "/Local Game.kcd",&info)==FR_OK && info.fsize==KUI_COVER_HEADER_BYTES);
    assert(f_open(&track,"0:/Games/Local Game/track03.bin",FA_WRITE)==FR_OK && f_lseek(&track,root_at+offset)==FR_OK);
    assert(f_write(&track,"0",1,&got)==FR_OK && got==1 && f_close(&track)==FR_OK);
    assert(f_unlink(KUI_GAMES_COVERS_FOLDER "/Local Game.kcd")==FR_OK && kui_games_storage_release());

    art_preference(KUI_GAMES_ART_DOWNLOADED_FIRST);
    visible_pixels(game,actual);
    assert(!memcmp(actual,download,sizeof(actual)));
    assert(kui_games_inspect_cover(game->path,&detail,actual,log_line,cancelled));
    assert(detail.cover && !memcmp(actual,download,KUI_COVER_PIXELS*2u));
    /* Regional sets can both contain a selector named "Local Game".
     * The second is scanned independently and cannot change the first. */
    struct kui_games_entry second=*game;
    strcpy(second.path,"/KUI/Regional/Local Game/disc.gdi");
    bool present=true;
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(kui_games_prepare_media_cover(&second,&present,NULL,log_line,cancelled) && !present);
    assert(kui_games_storage_release());
    visible_pixels(&second,actual);
    assert(actual[mid]==0xffe0);
    assert(kui_games_inspect_cover(second.path,&detail,actual,log_line,cancelled) && detail.cover);
    assert(actual[mid]==0xffe0 && !strcmp(detail.title,"OTHER REGION"));
    visible_pixels(game,actual);
    assert(!memcmp(actual,download,sizeof(actual)));
    /* Temporarily missing master: show disc art, then pick up the restored
     * preferred source even when the console's art cache is warm. */
    char master[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(game,KUI_MEDIA_COVER,master));
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_rename(master,KUI_DOWNLOADED_COVERS "/held.kct")==FR_OK && kui_games_storage_release());
    kui_games_covers_content_clear();
    visible_pixels(game,actual);
    assert(actual[mid]==0x001f);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_rename(KUI_DOWNLOADED_COVERS "/held.kct",master)==FR_OK && kui_games_storage_release());
    visible_pixels(game,actual);
    assert(!memcmp(actual,download,sizeof(actual)));
}

static void expect_cover_owner(const struct kui_games_entry *entry,enum kui_games_media_cover_state expected) {
    enum kui_games_media_cover_state state;
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(kui_games_prepare_media_cover_state(entry,&state,NULL,log_line,cancelled) && state==expected);
    assert(kui_games_storage_release());
}

static void exercise_cover(struct kui_games_media_job *job, const char *thumbs) {
    static uint16_t reference[PIXEL_BYTES/2], after[PIXEL_BYTES/2];
    const struct kui_games_entry *game=&job->selected;
    visible_pixels(game,reference);
    expect_cover_owner(game,KUI_GAMES_MEDIA_COVER_MANAGED);
    check_sources(game,reference);
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
        art_preference(KUI_GAMES_ART_DISC_FIRST);
        visible_pixels(game,after);
        assert(after[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0x001f);
        cover_only(job,false);
        art_preference(KUI_GAMES_ART_DOWNLOADED_FIRST);
        assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
        assert_new_layout(game,reference);
    }
    copy_fixture("custom.png",LOOSE_PATH);
    assert(kui_games_storage_release());
    offline_scan();
    expect_cover_owner(game,KUI_GAMES_MEDIA_COVER_OWNER);
    art_preference(KUI_GAMES_ART_DISC_FIRST);
    visible_pixels(game,after);
    assert(after[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0x07e0);
    art_preference(KUI_GAMES_ART_DOWNLOADED_FIRST);
    cover_only(job,false);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_unlink(LOOSE_PATH)==FR_OK && kui_games_storage_release());
    offline_scan();
    visible_pixels(game,after);
    assert(!memcmp(reference,after,sizeof(after)));
    art_preference(KUI_GAMES_ART_DISC_FIRST);
    strcpy(job->selected.cover_path,"/kui/owner-removed.png");
    expect_cover_owner(game,KUI_GAMES_MEDIA_COVER_OWNER);
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
static bool scan_discs(const struct kui_games_entry *disc,unsigned kinds,bool all) {
    struct kui_games_media_job job={.selected=*disc,.kinds=kinds,.all=all};
    struct kui_app_status status;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(!card_open && !mock.online);
    return status.passed;
}

static void verify_legacy_bytes(const char *path) {
    FIL file;
    struct kui_media_index index;
    static uint16_t pixels[KUI_PREVIEW_FRAME_BYTES/2u];
    assert(f_open(&file,path,FA_READ)==FR_OK && kui_media_container_index(&file,false,&index));
    assert(index.version==1 && index.count==1 && index.audio_rate==0);
    assert(kui_preview_read_frame(&file,index.count,0,pixels,NULL));
    for(unsigned i=0; i<KUI_PREVIEW_FRAME_BYTES/2u; ++i) assert(pixels[i]==0xa55a);
    assert(f_close(&file)==FR_OK);
}

static uint64_t preview_free_bytes(void) {
    FATFS *fs;
    DWORD clusters;
    assert(f_getfree("0:",&clusters,&fs)==FR_OK);
    return (uint64_t)clusters*fs->csize*512u;
}
static struct kui_internet *unexpected_preview_connection(void *ctx) {
    (void)ctx; assert(!"Cleanup of an installed preview must work offline"); return NULL;
}

static void card_digest(const char *path,uint8_t digest[32]) {
    FIL file;
    uint8_t buf[8192];
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    assert(f_open(&file,path,FA_READ)==FR_OK);
    while(f_tell(&file)<f_size(&file)) {
        UINT got;
        assert(f_read(&file,buf,sizeof(buf),&got)==FR_OK && got);
        kui_sha256_update(&hash,buf,got);
    }
    assert(f_close(&file)==FR_OK);
    kui_sha256_digest(&hash,digest);
}

static void change_card_byte(const char *path,unsigned offset) {
    FIL f;
    UINT got;
    uint8_t byte;
    assert(f_open(&f,path,FA_READ|FA_WRITE)==FR_OK && f_lseek(&f,offset)==FR_OK);
    assert(f_read(&f,&byte,1,&got)==FR_OK && got==1);
    byte^=0x20;
    assert(f_lseek(&f,offset)==FR_OK && f_write(&f,&byte,1,&got)==FR_OK && got==1 && f_close(&f)==FR_OK);
}

static unsigned validator_count(void) {
    DIR directory;FILINFO info;unsigned count=0;
    assert(f_opendir(&directory,"0:/KUI/media/art-checks")==FR_OK);
    for(;;) {
        assert(f_readdir(&directory,&info)==FR_OK);
        if(!info.fname[0])break;
        const char *ext=strrchr(info.fname,'.');
        if(ext && !strcmp(ext,".kav"))++count;
    }
    assert(f_closedir(&directory)==FR_OK);return count;
}

static void validators_on_card(const char *master) {
    const char *release="Validator fixture (Europe)", *first="https://fixture.invalid/front.jpg#kui-tile-00001000-ab123456";
    const char *second="https://fixture.invalid/front.png";
    struct kui_http_validators a={.etag="\"a\"", .modified="Fri, 20 Nov 2026 05:01:13 GMT",.resource={1}},
        b={.etag="W/\"b\"",.resource={2}}, out;
    assert(!kui_artwork_validators_load(release,true,master,first,&out) && !out.etag[0]);
    FRESULT dir=f_mkdir("0:/KUI/media");assert(dir==FR_OK || dir==FR_EXIST);
    dir=f_mkdir("0:/KUI/media/art-checks");assert(dir==FR_OK || dir==FR_EXIST);
    unsigned before=validator_count();
    /* Old KAV1 files bound their source hash to a specific native filename. */
    uint8_t old[324]={'K','A','V','1'};
    struct kui_sha256 hash;
    kui_sha256_init(&hash);kui_sha256_update(&hash,master,strlen(master)+1u);
    kui_sha256_update(&hash,first,strlen(first));kui_sha256_digest(&hash,old+4);
    card_digest(master,old+36);memcpy(old+68,a.resource,32);
    memcpy(old+100,a.etag,strlen(a.etag));memcpy(old+228,a.modified,strlen(a.modified));
    kui_sha256_init(&hash);kui_sha256_update(&hash,old,292);kui_sha256_digest(&hash,old+292);
    char old_path[96],hex[33];kui_hex(old+4,16,hex);
    snprintf(old_path,sizeof(old_path),"0:/KUI/media/art-checks/%s.kav",hex);
    FIL f;UINT n;
    assert(f_open(&f,old_path,FA_WRITE|FA_CREATE_NEW)==FR_OK);
    assert(f_write(&f,old,sizeof(old),&n)==FR_OK && n==sizeof(old) && f_close(&f)==FR_OK);
    assert(kui_artwork_validators_load(release,true,master,first,&out) && !memcmp(&a,&out,sizeof(a)));
    assert(kui_artwork_validators_save(release,true,master,first,&a) && kui_artwork_validators_save(release,false,master,second,&b));
    FILINFO fi;
    assert(f_stat(old_path,&fi)==FR_NO_FILE && validator_count()==before+2);
    assert(kui_artwork_validators_load(release,true,master,first,&out) && !memcmp(&out,&a,sizeof(out)));
    assert(kui_artwork_validators_load(release,false,master,second,&out) && !memcmp(&out,&b,sizeof(out)));
    /* A stale ETag must not skip a request after other KCT pixels were
     * committed or damaged, even if the KCT header has its original CRCs. */
    change_card_byte(master,10000);
    assert(!kui_artwork_validators_load(release,true,master,first,&out) && !out.etag[0]);
    change_card_byte(master,10000);
    assert(kui_artwork_validators_load(release,true,master,first,&out));
    uint8_t pixels[32],after[32],source[32];
    card_digest(master,pixels);
    /* Make a metadata write fail before publication; the cover and its prior
     * validator are still readable. */
    const char *temp="0:/KUI/media/art-checks/write.tmp";
    assert(f_mkdir(temp)==FR_OK && !kui_artwork_validators_save(release,true,master,first,&b));
    assert(f_unlink(temp)==FR_OK && kui_artwork_validators_load(release,true,master,first,&out) && !strcmp(out.etag,a.etag));
    /* Locate this specific sidecar even if the real downloader has already
     * stored other source validators in the same directory. */
    uint8_t role=1;
    kui_sha256_init(&hash);kui_sha256_update(&hash,"KAV2",4);
    kui_sha256_update(&hash,release,strlen(release)+1u);kui_sha256_update(&hash,&role,1);
    kui_sha256_digest(&hash,source);
    char side[96]; kui_hex(source,16,hex);
    assert(snprintf(side,sizeof(side),"0:/KUI/media/art-checks/%s.kav",hex)<(int)sizeof(side));
    change_card_byte(side,50);
    assert(!kui_artwork_validators_load(release,true,master,first,&out));
    assert(kui_artwork_validators_load(release,false,master,second,&out));
    FIL file;
    assert(f_open(&file,side,FA_WRITE)==FR_OK && f_lseek(&file,9)==FR_OK);
    assert(f_truncate(&file)==FR_OK && f_close(&file)==FR_OK);
    assert(!kui_artwork_validators_load(release,true,master,first,&out));
    assert(kui_artwork_validators_save(release,true,master,first,&a) && kui_artwork_validators_load(release,true,master,first,&out));
    /* Both a changed URL/pin and changed native bytes must invalidate the
     * previous condition. Five different native revisions use those same
     * two slots. Restoring the test master leaves the scan fixture intact. */
    for(unsigned i=0;i<5;i++) {
        char moved[96],source_url[100];
        snprintf(moved,sizeof(moved),"0:/KUI/media/validator-revision-%u.kct",i);
        snprintf(source_url,sizeof(source_url),"https://fixture.invalid/front.jpg#kui-tile-00002000-%08x",i);
        assert(f_rename(master,moved)==FR_OK);
        change_card_byte(moved,512+i);
        assert(!kui_artwork_validators_load(release,true,moved,source_url,&out));
        assert(kui_artwork_validators_save(release,true,moved,source_url,&a));
        assert(kui_artwork_validators_load(release,true,moved,source_url,&out) && !memcmp(&out,&a,sizeof(a)));
        assert(!kui_artwork_validators_load(release,true,moved,first,&out));
        assert(validator_count()==before+2);
        change_card_byte(moved,512+i);
        assert(!kui_artwork_validators_load(release,true,moved,source_url,&out));
        assert(f_rename(moved,master)==FR_OK);
    }
    memset(&a,0,sizeof(a));
    assert(kui_artwork_validators_save(release,true,master,first,&a) && !kui_artwork_validators_load(release,true,master,first,&out));
    assert(kui_artwork_validators_load(release,false,master,second,&out) && !strcmp(out.etag,b.etag));
    card_digest(master,after); assert(!memcmp(pixels,after,sizeof(pixels)));
    puts("PASS bounded artwork validators, legacy migration, changed source/pin, interrupted writes and corrupt records");
}

#include "artwork_publish_faults.inc"
#include "media_scan_report_card.inc"
#include "artwork_cleanup_card.inc"

static void refresh_disc_art(const struct kui_games_entry *e,enum kui_artwork_mode mode,bool all) {
    struct kui_games_media_job job={.selected=*e,.kinds=KUI_GAMES_MEDIA_ART,.all=all,.artwork_mode=mode};
    struct kui_app_status status;
    kui_games_media_run(&job,&status,log_line,cancelled,report);
    assert(status.passed && !mock.online && !card_open);
}
static void grouped_front_covers(const struct kui_games_entry *one,const struct kui_games_entry *two) {
    assert(scan_discs(two,KUI_GAMES_MEDIA_ART,false) && covers);
    unsigned attempts=connections;
    assert(scan_discs(one,KUI_GAMES_MEDIA_ART,false) && connections==attempts);
    char master[KUI_GAMES_FILE_CAP], member[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(one,KUI_MEDIA_COVER,master) && kui_media_file_path(two,KUI_MEDIA_COVER,member));
    assert(strcmp(master,member) && kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    FIL file;
    FILINFO info;
    assert(f_stat(master,&info)==FR_OK && info.fsize==KUI_COMPACT_COVER_BYTES);
    validators_on_card(master);
    char link[36];
    UINT got;
    assert(f_open(&file,member,FA_READ)==FR_OK && f_size(&file)==sizeof(link));
    assert(f_read(&file,link,sizeof(link),&got)==FR_OK && got==sizeof(link) && f_close(&file)==FR_OK);
    assert(!memcmp(link,"KCL1",4) && !memcmp(link+4,strrchr(master,'/')+1,32));
    assert(kui_games_storage_release());
    static uint16_t primary[(KUI_COMPACT_COVER_BYTES-KUI_COMPACT_COVER_HEADER)/2u];
    static uint16_t secondary[(KUI_COMPACT_COVER_BYTES-KUI_COMPACT_COVER_HEADER)/2u];
    visible_pixels(one,primary);
    visible_pixels(two,secondary);
    assert(!memcmp(primary,secondary,sizeof(primary)));
    artwork_publish_faults(one,two,primary,master);
    art_revision=1;conditional_bodies=unchanged_responses=0;
    refresh_disc_art(two,KUI_ART_CHECK,false);
    assert(conditional_bodies==1 && unchanged_responses==0);
    refresh_disc_art(two,KUI_ART_CHECK,false);
    assert(conditional_bodies==1 && unchanged_responses==1);
    refresh_disc_art(two,KUI_ART_REDOWNLOAD,false);
    assert(conditional_bodies==2 && unchanged_responses==1);
    art_revision=2;
    refresh_disc_art(one,KUI_ART_CHECK,true);
    assert(conditional_bodies==3 && unchanged_responses==1);
    visible_pixels(one,primary);
    visible_pixels(two,secondary);
    assert(!memcmp(primary,secondary,sizeof(primary)) && primary[80u*160u+80u]==0x07e0);
    unsigned checked=connections;
    refresh_disc_art(one,KUI_ART_MISSING,true);
    assert(connections==checked && conditional_bodies==3);
    refresh_disc_art(one,KUI_ART_CHECK,true);
    assert(conditional_bodies==3 && unchanged_responses==2);
    art_revision=0;
    puts("PASS legacy cover validation, no-body 304, forced transfer and one changed regional cover for both discs");
}
static void multidisc_media(void) {
    kui_games_storage_hold(true);
    struct kui_games_page rows, members;
    assert(kui_games_library_scan("/Games",false,log_line,cancelled));
    assert(kui_games_library_rows("/Games","",0,KUI_GAMES_VIEW_LIST,&rows,log_line,cancelled));
    assert(rows.count==2);
    struct kui_games_entry *group=rows.entries, *unrelated=rows.entries+1;
    if(!group->group[0]) { group=rows.entries+1; unrelated=rows.entries; }
    assert(group->group[0] && !unrelated->group[0]);
    assert(kui_games_library_rows("/Games",group->group,0,KUI_GAMES_VIEW_LIST,&members,log_line,cancelled));
    assert(members.count==2 && members.entries[0].disc==1 && members.entries[1].disc==2);
    struct kui_games_entry *one=members.entries,*two=members.entries+1;
    grouped_front_covers(one,two);
    char kdm[KUI_GAMES_FILE_CAP],kpv[KUI_GAMES_FILE_CAP],pdf[KUI_GAMES_FILE_CAP],path[KUI_GAMES_FILE_CAP];
    assert(kui_media_file_path(one,KUI_MEDIA_MANUAL,kdm) && kui_media_file_path(two,KUI_MEDIA_MANUAL,path) && !strcmp(kdm,path));
    assert(kui_media_file_path(one,KUI_MEDIA_PREVIEW,kpv) && kui_media_file_path(two,KUI_MEDIA_PREVIEW,path) && !strcmp(kpv,path));
    assert(kui_media_original_path(one,KUI_MEDIA_MANUAL,pdf) && kui_media_original_path(two,KUI_MEDIA_MANUAL,path) && !strcmp(pdf,path));
    assert(kui_games_media_path(one->path,"0:/KUI/media","kpv",path,sizeof(path)) && !strcmp(kpv,path));
    assert(kui_media_file_path(unrelated,KUI_MEDIA_PREVIEW,path) && strcmp(kpv,path));
    assert(kui_media_file_path(unrelated,KUI_MEDIA_MANUAL,path) && strcmp(kdm,path));
    assert(kui_media_original_path(unrelated,KUI_MEDIA_MANUAL,path) && strcmp(pdf,path));
    /* Request Disc 2 before its sibling. A missing page source falls back to
     * the original PDF. Both siblings subsequently find the same file offline. */
    missing_pages=true;
    assert(scan_discs(two,KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,false));
    assert(pages==1 && clips==1 && pdfs==1);
    unsigned opens=connections;
    assert(scan_discs(one,KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,false));
    assert(connections==opens);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    FILINFO info;
    assert(f_stat(pdf,&info)==FR_OK && info.fsize>64);
    verify_media(one,KUI_MEDIA_PREVIEW,100);
    struct kui_hover_clip hover={0};
    assert(kui_hover_clip_open(&hover,one,log_line,cancelled) && hover.audio_rate==16000);
    int16_t *pcm=kui_preview_load_audio(&hover.file,hover.frames,hover.audio_rate,NULL);
    assert(pcm);
    unsigned audible=0;
    for(unsigned i=0;i<160000u;++i) if(pcm[i]>32 || pcm[i]<-32) ++audible;
    assert(audible>100000u);
    free(pcm);
    kui_hover_clip_close(&hover);
    assert(f_unlink(pdf)==FR_OK);
    char peer[KUI_GAMES_FILE_CAP],other[KUI_GAMES_FILE_CAP];
    assert(kui_games_media_path(two->path,"0:/KUI/media","kpv",peer,sizeof(peer)) && strcmp(peer,kpv));
    assert(kui_media_file_path(unrelated,KUI_MEDIA_PREVIEW,other));
    copy_fixture("legacy.kpv",kpv);
    assert(f_rename(kpv,peer)==FR_OK);
    verify_legacy_bytes(peer);
    assert(kui_games_storage_release());
    /* New readable pages share the same group cache; the unrelated title
     * has its own key and has no matching catalogue assets. */
    missing_pages=false;
    assert(scan_discs(two,KUI_GAMES_MEDIA_MANUAL,false));
    assert(pages==page_count+1 && pdfs==1);
    opens=connections;
    assert(scan_discs(one,KUI_GAMES_MEDIA_MANUAL,false));
    assert(connections==opens && clips==1);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    verify_media(one,KUI_MEDIA_MANUAL,page_count);
    verify_legacy_bytes(peer);
    assert(kui_games_storage_release());
    /* A whole-library scan attempts the older preview once for the set.
     * On failure it remains playable, and on the next scan both discs
     * receive the new soundtrack without any per-disc selection. */
    fail_clip=true;
    assert(!scan_discs(one,KUI_GAMES_MEDIA_PREVIEW,true) && clips==2);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    verify_legacy_bytes(peer);
    assert(f_stat(kpv,&info)==FR_NO_FILE);
    assert(kui_hover_clip_open(&hover,two,log_line,cancelled) && hover.frames==1 && !hover.audio_rate);
    kui_hover_clip_close(&hover);
    assert(kui_games_storage_release());
    fail_clip=false;
    assert(scan_discs(one,KUI_GAMES_MEDIA_PREVIEW,true) && clips==3);
    opens=connections;
    assert(scan_discs(one,KUI_GAMES_MEDIA_PREVIEW,true) && connections==opens && clips==3);
    assert(scan_discs(one,KUI_GAMES_MEDIA_PREVIEW,false) && connections==opens && clips==3);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_stat(kpv,&info)==FR_NO_FILE && f_stat(peer,&info)==FR_NO_FILE);
    assert(kui_hover_clip_open(&hover,one,log_line,cancelled) && hover.frames==100 && hover.audio_rate==16000);
    kui_hover_clip_close(&hover);
    /* Model an earlier build (or a reset just after installing KPV2) with
     * legacy copies in both member slots plus a separate title's clip. */
    copy_fixture("legacy.kpv",kpv); copy_fixture("legacy.kpv",peer); copy_fixture("legacy.kpv",other);
    uint64_t with_obsolete=preview_free_bytes();
    assert(kui_games_storage_release());
    struct kui_media_game retry={.entry=two};
    blocked_preview_unlink=kpv;
    assert(kui_media_fetch_internet(&retry,KUI_MEDIA_PREVIEW,unexpected_preview_connection,log_line,
        cancelled,NULL,NULL)==KUI_MEDIA_EXISTING && retry.preview_cleanup_pending && !retry.previews_removed);
    assert(kui_games_storage_release());
    blocked_preview_unlink=NULL;
    assert(kui_media_fetch_internet(&retry,KUI_MEDIA_PREVIEW,unexpected_preview_connection,log_line,
        cancelled,NULL,NULL)==KUI_MEDIA_EXISTING && !retry.preview_cleanup_pending && retry.previews_removed==2);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(f_stat(kpv,&info)==FR_NO_FILE && f_stat(peer,&info)==FR_NO_FILE);
    verify_legacy_bytes(other);
    uint64_t reclaimed=preview_free_bytes()-with_obsolete;
    assert(reclaimed>=2u*(KUI_PREVIEW_HEADER_BYTES+KUI_PREVIEW_FRAME_BYTES));
    printf("PASS reclaimed %llu bytes across two superseded previews on an offline retry\n",
           (unsigned long long)reclaimed);
    assert(kui_games_storage_release());
    /* Older builds may have downloaded both assets only for Disc 2. Model
     * that card, then scan Disc 1 first without any shared file to find. */
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    char shared[2][KUI_GAMES_FILE_CAP], old[2][KUI_GAMES_FILE_CAP];
    uint8_t before[2][32], after[32];
    const char *formats[]={"kdm","kp2"};
    for(unsigned i=0;i<2;++i) {
        assert(kui_games_media_path(one->path,"0:/KUI/media",formats[i],shared[i],sizeof(shared[i])));
        assert(kui_games_media_path(two->path,"0:/KUI/media",formats[i],old[i],sizeof(old[i])));
        assert(strcmp(old[i],shared[i]) && f_rename(shared[i],old[i])==FR_OK);
        card_digest(old[i],before[i]);
        assert(f_stat(shared[i],&info)==FR_NO_FILE);
    }
    assert(kui_games_storage_release());
    opens=connections;
    assert(scan_discs(one,KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,false));
    assert(connections==opens && kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    for(unsigned i=0;i<2;++i) {
        card_digest(shared[i],after);
        assert(!memcmp(before[i],after,sizeof(after)) && f_stat(old[i],&info)==FR_NO_FILE);
    }
    verify_media(one,KUI_MEDIA_MANUAL,page_count);
    assert(kui_hover_clip_open(&hover,one,log_line,cancelled) && hover.frames==100 && hover.audio_rate==16000);
    kui_hover_clip_close(&hover);
    assert(kui_games_storage_release());
    puts("PASS grouped disc media, PDF fallback, offline reuse, interrupted and successful legacy audio refresh");
}

int main(int argc,char **argv) {
    fault_card_path=argc>1 ? argv[1] : NULL;
    if(argc<3 || argc>5) return 2;
    large_source=argc==5 && !strcmp(argv[4],"4k");
    bool discs=argc==5 && !strcmp(argv[4],"discs");
    bool skies=discs || (argc==5 && !strcmp(argv[4],"skies"));
    release_url=skies?"Skies%20of%20Arcadia":"Crazy%20Taxi";
    book_url=skies?"/Skies_Of_Arcadia_2000_U/page/n":"/Crazy_Taxi_2000_U/page/n";
    page_count=skies?18:13;
    fixtures=argv[2];
    card=fopen(argv[1],"r+b");
    struct stat st;
    assert(card && !fstat(fileno(card),&st));
    sectors=(uint64_t)st.st_size/512;
    if(argc==5 && !strcmp(argv[4],"cleanup")) {
        artwork_cleanup_card();
        assert(!fclose(card));
        return 0;
    }
    if(discs) {
        media_scan_report_card();
        multidisc_media();
        assert(!fclose(card));
        return 0;
    }
    struct kui_games_page page;
    assert(kui_games_list("/Games",0,&page,log_line,cancelled) && page.count==1);
    struct kui_games_media_job job = {.all=true,.kinds=KUI_GAMES_MEDIA_ART|KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,
        .selected=page.entries[0]};
    struct kui_app_status status;
    /* A rescan first caches the blue disc texture; internet box art supersedes it. */
    struct kui_games_scan_counts counts;
    assert(kui_games_scan(&status,&counts,report,log_line,cancelled) && counts.disc==1);
    kui_games_storage_hold(true);
    /* A fresh cover record can supply the same IP identity as a direct
     * image read. A previously built record cannot vouch for a live IP. */
    struct kui_games_entry probe=job.selected;
    strcpy(probe.name,"Identity test");
    struct kui_games_disc_identity identity={0},direct;
    bool present;
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    assert(kui_games_media_identity(probe.path,&direct,log_line,cancelled)==KUI_GAMES_IDENTITY_FOUND);
    assert(kui_games_prepare_media_cover(&probe,&present,&identity,log_line,cancelled) && !present);
    assert(identity.number==1 && identity.total==1 && !strcmp(identity.version,"V1.000"));
    assert(identity.number==direct.number && identity.total==direct.total && !strcmp(identity.version,direct.version));
    assert(identity.product[0] && !strcmp(identity.product,direct.product) &&
        !strcmp(identity.title,direct.title) && !strcmp(identity.region,direct.region));
    memset(&identity,0,sizeof(identity));
    assert(kui_games_prepare_media_cover(&probe,&present,&identity,log_line,cancelled) && !present && !identity.product[0]);
    assert(kui_games_storage_release());
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
    /* Both scan scopes reuse completed files without opening a connection. */
    unsigned opens=connections;
    for(unsigned scope=0;scope<2;++scope) {
        job.all=scope==0;
        kui_games_media_run(&job,&status,log_line,cancelled,report);
        assert(status.passed && connections==opens && small_requests==1 &&
            covers==1 && pages==1+page_count && clips==1 && !card_open);
    }
    exercise_cover(&job,argc>=4 && strcmp(argv[3],"-") ? argv[3] : NULL);
    assert(!card_open && !fclose(card));
    puts("PASS library download, on-card pixels/pages/frames, and selected-game skip");
    return 0;
}
