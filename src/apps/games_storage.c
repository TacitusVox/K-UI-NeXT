/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games_storage.h"
#include "platform.h"
#include "kui/media.h"
#include <stdlib.h>
#include <string.h>

/* The mounted work area must outlive each job while the session is open. */
static FATFS fs;
static bool open_session,held;
static unsigned jobs;
static uint64_t mount_serial;
/* About 1,500 covers' worth of exFAT directory entries plus FAT sectors.
 * Allocated on first lookup and freed when the session is released. */
#define LOOKUP_SECTORS 400u
#define LOOKUP_BYTES (LOOKUP_SECTORS*(512u+sizeof(uint32_t)))
static void *lookup_scratch;
static struct kui_media_read_cache_saved lookup_saved;
void kui_games_storage_lookup_discard(void) {
    struct kui_media_read_cache_saved ignored;
    (void)kui_media_read_cache_suspend(lookup_scratch,&ignored);
    free(lookup_scratch);lookup_scratch=NULL;memset(&lookup_saved,0,sizeof(lookup_saved));
}
void kui_games_storage_lookup_begin(void) {
    if(!open_session || !fs.fs_type || kui_media_read_cache_active()) return;
    if(!lookup_scratch) {
        lookup_scratch=aligned_alloc(32u,LOOKUP_BYTES);
        memset(&lookup_saved,0,sizeof(lookup_saved));
        if(!lookup_scratch) return;
    }
    if(!kui_media_read_cache_resume(&lookup_saved))
        (void)kui_media_read_cache_begin(lookup_scratch,LOOKUP_BYTES);
}
void kui_games_storage_lookup_end(void) {
    if(lookup_scratch && !kui_media_read_cache_suspend(lookup_scratch,&lookup_saved))
        memset(&lookup_saved,0,sizeof(lookup_saved));
}

enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    /* FatFs clears fs_type when anything else mounts or unmounts "0:", or
     * when a remount after a media error fails. Every other job disconnects
     * after its unmount, so a cleared session starts again from a new connect. */
    if(open_session && fs.fs_type) return KUI_GAMES_STORAGE_OK;
    if(open_session) kui_games_storage_lookup_discard();
    open_session=false;
    if(!kui_sd_connect()) return KUI_GAMES_STORAGE_NO_CARD;
    if(!kui_mount(&fs,log)) {kui_sd_disconnect();return KUI_GAMES_STORAGE_NO_MOUNT;}
    if(++mount_serial==0)++mount_serial;
    open_session=true;
    return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_release(void) {
    kui_games_storage_lookup_discard();
    if(!open_session) return true;
    open_session=false;
    bool ok=f_mount(NULL,"0:",0)==FR_OK;
    kui_sd_disconnect();
    return ok;
}
enum kui_games_storage_result kui_games_storage_refresh(kui_log_fn log) {
    if(open_session && fs.fs_type && !kui_games_storage_release())return KUI_GAMES_STORAGE_NO_MOUNT;
    open_session=false;
    return kui_games_storage_open(log);
}
bool kui_games_storage_done(bool keep) {
    if(!open_session) return true;
    ++jobs;
    return held && keep?true:kui_games_storage_release();
}
void kui_games_storage_hold(bool hold) {held=hold;}
bool kui_games_storage_active(void) {return open_session;}
uint64_t kui_games_storage_session(void) {return open_session && fs.fs_type?mount_serial:0;}
unsigned kui_games_storage_jobs(void) {return jobs;}
