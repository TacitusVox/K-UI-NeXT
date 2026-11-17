/* SPDX-License-Identifier: GPL-3.0-only */
/* Run the Games media worker with an image-backed card and the actual socket
 * adapter. Only the console hardware and the PC's chosen LAN address are replaced. */
#define _POSIX_C_SOURCE 200809L
#include "kui/bba_sockets.h"
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
static uint16_t server_port;
static bool card_open;
static unsigned connects;

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

static bool (*real_connect)(void *,unsigned,uint16_t,const uint8_t[4],uint16_t);
static bool dial(void *ctx, unsigned slot, uint16_t local, const uint8_t ip[4], uint16_t port) {
    assert(!memcmp(ip,(uint8_t[]){192,168,1,9},4) && port == KUI_MEDIA_DEFAULT_PORT);
    ++connects;
    return real_connect(ctx,slot,local,(uint8_t[]){127,0,0,1},server_port);
}
static int bba_open(struct kui_bba_session *session, kui_log_fn log, kui_cancel_fn cancel) {
    (void)log; (void)cancel;
    assert(kui_bba_sockets_init(&session->net));
    real_connect = session->net.connect;
    session->net.connect = dial;
    return 1;
}
static bool online(struct kui_bba_session *s) { (void)s; return true; }
static void bba_end(struct kui_bba_session *s) { kui_bba_sockets_end(&s->net); }
static uint64_t now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000 + (uint64_t)ts.tv_nsec/1000000;
}
static void pause_ms(unsigned ms) {
    struct timespec ts = {ms/1000u, (long)(ms%1000u)*1000000};
    nanosleep(&ts,NULL);
}
const struct kui_bba_port *kui_bba_console_port(void) {
    static const struct kui_bba_port bba = {bba_open,online,bba_end,now,pause_ms};
    return &bba;
}
static bool never_cancel(void) { return false; }
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
    if(argc!=3) return 2;
    server_port=(uint16_t)strtoul(argv[2],NULL,10);
    card=fopen(argv[1],"r+b");
    struct stat st;
    assert(card && !fstat(fileno(card),&st));
    sectors=(uint64_t)st.st_size/512;
    struct kui_games_page page;
    assert(kui_games_list("/Games",0,&page,log_line,never_cancel) && page.count==1);
    struct kui_games_media_job job = {.all=true,.kinds=KUI_GAMES_MEDIA_ART|KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW,
        .selected=page.entries[0],.ip={192,168,1,9}};
    uint8_t saved[4]={0};
    assert(kui_games_media_address(job.ip,true,log_line));
    assert(kui_games_media_address(saved,false,log_line) && !memcmp(saved,job.ip,4));
    struct kui_app_status status;
    /* Start with a valid title-only KCV produced by the existing rescan. */
    struct kui_games_scan_counts counts;
    assert(kui_games_scan(&status,&counts,report,log_line,never_cancel) && counts.none==1);
    kui_games_media_run(&job,&status,log_line,never_cancel,report);
    log_line("Result: %s",status.message);
    assert(status.passed && connects==3);
    struct kui_games_cover_result cover;
    static uint16_t pixels[KUI_COVER_PIXELS];
    assert(kui_games_selected_cover(&job.selected,KUI_COVER_SIZE_LARGE,pixels,&cover,log_line,never_cancel));
    assert(cover.cover && pixels[KUI_COVER_PIXELS/2+KUI_COVER_LARGE/2]==0xf800);
    assert(kui_games_storage_open(log_line)==KUI_GAMES_STORAGE_OK);
    verify_media(&job.selected,KUI_MEDIA_MANUAL,2);
    verify_media(&job.selected,KUI_MEDIA_PREVIEW,10);
    assert(kui_games_storage_release());
    /* Selected-game run must reuse the completed files without another GET. */
    job.all=false;
    kui_games_media_run(&job,&status,log_line,never_cancel,report);
    assert(status.passed && connects==3 && !card_open && !fclose(card));
    puts("PASS library download, on-card pixels/pages/frames, and selected-game skip");
    return 0;
}
