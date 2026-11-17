/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real batch worker against paginated catalogue snapshots.
 * The separate image-backed network test checks the TCP/file implementation. */
#include "kui/games_media.h"
#include "kui/media_download.h"
#include "kui/games_covers.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    unsigned opens,ends,releases,main_pages,group_pages,folder_pages,requests;
    unsigned seen[14][3],progress_updates;
    bool stopped,online;
    enum kui_media_result inject;
    unsigned inject_at;
} state;

static bool stop(void) {return state.stopped;}
static int bba_open(struct kui_bba_session *s,kui_log_fn log,kui_cancel_fn cancel) {
    (void)s;(void)log;(void)cancel;
    ++state.opens;
    return state.online?1:0;
}
static void bba_end(struct kui_bba_session *s) {(void)s;++state.ends;}
static const struct kui_bba_port adapter={.open=bba_open,.end=bba_end};
const struct kui_bba_port *kui_bba_console_port(void) {return &adapter;}
bool kui_games_storage_release(void) {++state.releases;return true;}

static void game(struct kui_games_entry *entry,unsigned id) {
    snprintf(entry->name,sizeof(entry->name),"Game %u",id);
    snprintf(entry->path,sizeof(entry->path),"/Games/Test %u/disc.gdi",id);
}
bool kui_games_library_rows(const char *root,const char *group,unsigned offset,unsigned view,
        struct kui_games_page *page,kui_log_fn log,kui_cancel_fn cancel) {
    (void)log;
    assert(cancel && !cancel() && view==KUI_GAMES_VIEW_SAVED);
    memset(page,0,sizeof(*page));
    if(!strcmp(root,"/Games/Category")) {
        assert(!*group && !offset);
        ++state.folder_pages;
        game(&page->entries[0],13);
        page->count=page->total=1;
    } else if(*group) {
        assert(!strcmp(root,"/Games") && !strcmp(group,"two-discs") && !offset);
        ++state.group_pages;
        page->count=page->total=2;
        game(&page->entries[0],10);game(&page->entries[1],11);
        strcpy(page->entries[1].cover_path,"/Custom/disc2.png");
    } else {
        assert(!strcmp(root,"/Games") && (offset==0 || offset==8));
        ++state.main_pages;
        page->total=12;page->count=offset?4:8;page->has_more=!offset;
        for(unsigned i=0;i<page->count;i++)game(&page->entries[i],offset+i);
        if(!offset)page->entries[5].disabled=true;
        else {
            strcpy(page->entries[1].group,"two-discs");
            strcpy(page->entries[1].cover_path,"/Custom/group.png");
            page->entries[2].directory=true;strcpy(page->entries[2].path,"/Games/Category");
            game(&page->entries[3],12);
        }
    }
    return true;
}
enum kui_media_result kui_media_fetch_progress(const struct kui_bba_port *port,struct kui_bba_session *session,
        const uint8_t ip[4],uint16_t port_number,const struct kui_games_entry *entry,
        enum kui_media_kind kind,kui_log_fn log,kui_cancel_fn cancel,
        void (*progress)(void *,uint32_t,uint32_t),void *ctx) {
    (void)session;(void)log;
    assert(port==&adapter && ip[0]==192 && port_number==KUI_MEDIA_DEFAULT_PORT && !cancel());
    assert(!entry->directory && !entry->disabled && !entry->group[0]);
    unsigned id;
    assert(sscanf(entry->name,"Game %u",&id)==1 && id<14 && kind<=KUI_MEDIA_PREVIEW);
    assert(!state.seen[id][kind]++);
    if(id==10)assert(!strcmp(entry->cover_path,"/Custom/group.png"));
    if(id==11)assert(!strcmp(entry->cover_path,"/Custom/disc2.png"));
    progress(ctx,512,1024);
    if(++state.requests==state.inject_at) {
        if(state.inject==KUI_MEDIA_STOPPED)state.stopped=true;
        return state.inject;
    }
    if(id==0 && kind==KUI_MEDIA_COVER)return KUI_MEDIA_MISSING;
    if(id==1 && kind==KUI_MEDIA_COVER)return KUI_MEDIA_UNIDENTIFIED;
    if(entry->cover_path[0] && kind==KUI_MEDIA_COVER)return KUI_MEDIA_EXISTING;
    return KUI_MEDIA_INSTALLED;
}
static void report(const struct kui_app_status *out) {
    if(out->total) {
        assert(out->total==1024 && out->done==512);
        ++state.progress_updates;
    }
}
static void reset(void) {memset(&state,0,sizeof(state));state.online=true;}
int main(void) {
    const struct kui_games_media_job job={.all=true,.ip={192,168,1,30},.kinds=7};
    struct kui_app_status status;
    reset();kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.complete && status.passed && !status.stopped && !status.errors);
    assert(state.opens==1 && state.ends==1 && state.releases==1);
    assert(state.main_pages==2 && state.group_pages==1 && state.folder_pages==1);
    assert(state.seen[0][KUI_MEDIA_MANUAL] && state.seen[0][KUI_MEDIA_PREVIEW]); /* Missing cover only. */
    assert(!state.seen[1][KUI_MEDIA_MANUAL] && !state.seen[1][KUI_MEDIA_PREVIEW]); /* No game identity. */
    assert(!state.seen[5][0] && !state.seen[9][0]); /* Disabled row and group representative. */
    assert(state.seen[8][0] && state.seen[10][0] && state.seen[11][0] && state.seen[12][0] && state.seen[13][0]);
    assert(state.requests==34 && state.progress_updates==34);
    assert(strstr(status.lines[0],"Downloaded: 30") && strstr(status.lines[0],"Already present: 2"));
    assert(strstr(status.lines[1],"Unavailable assets: 1") && strstr(status.lines[1],"Unmatched games: 1"));
    const enum kui_media_result failures[]={KUI_MEDIA_LIMITED,KUI_MEDIA_STOPPED,KUI_MEDIA_ERROR};
    for(size_t i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
        reset();state.inject_at=2;state.inject=failures[i];kui_games_media_run(&job,&status,NULL,stop,report);
        assert(status.complete && !status.passed && status.stopped==(state.inject==KUI_MEDIA_STOPPED));
        assert(state.requests==2 && state.ends==1 && state.releases==1 && state.main_pages==1);
    }
    reset();state.online=false;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors==1 && !state.requests && !state.ends && state.releases==1);
    puts("PASS paged media batch, grouped disc covers, folders, skips, progress, rate limit, stop and cleanup");
    return 0;
}
