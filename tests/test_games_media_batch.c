/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real batch worker against paginated catalogue snapshots.
 * The separate image-backed network test checks the TCP/file implementation. */
#include "kui/games_media.h"
#include "kui/internet.h"
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
    bool saw_page,saw_conversion,saw_scaling;
    bool stopped,online,opened_sd,fail_mount;
    enum kui_internet_route route;
    enum kui_media_result inject;
    unsigned inject_at;
} state;

static bool stop(void) {return state.stopped;}
struct kui_internet {unsigned active;};
static struct kui_internet mock;
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;
    if(state.fail_mount)return KUI_GAMES_STORAGE_NO_CARD;
    state.opened_sd=true;
    return KUI_GAMES_STORAGE_OK;
}
struct kui_internet *kui_internet_open(enum kui_internet_route route,kui_log_fn log,kui_cancel_fn cancel,char problem[128]) {
    (void)log;
    assert(!cancel() && state.opened_sd && !mock.active);
    ++state.opens;state.route=route;
    if(!state.online) {strcpy(problem,"Adapter link down.");return NULL;}
    mock.active=1;
    return &mock;
}
const char *kui_internet_adapter(const struct kui_internet *s) {assert(s==&mock && s->active);return "Test adapter";}
const char *kui_internet_problem(const struct kui_internet *s) {assert(s==&mock && s->active);return "Certificate check failed.";}
void kui_internet_close(struct kui_internet *s) {assert(s==&mock && s->active && state.opened_sd);++state.ends;s->active=0;}
bool kui_games_storage_release(void) {assert(!mock.active);state.opened_sd=false;++state.releases;return true;}

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
enum kui_media_result kui_media_fetch_internet(struct kui_internet *session,const struct kui_games_entry *entry,
        enum kui_media_kind kind,kui_log_fn log,kui_cancel_fn cancel,kui_media_progress_fn progress,void *ctx) {
    (void)log;
    assert(session==&mock && session->active && !cancel());
    assert(!entry->directory && !entry->disabled && !entry->group[0]);
    unsigned id;
    assert(sscanf(entry->name,"Game %u",&id)==1 && id<14 && kind<=KUI_MEDIA_PREVIEW);
    assert(!state.seen[id][kind]++);
    if(id==10)assert(!strcmp(entry->cover_path,"/Custom/group.png"));
    if(id==11)assert(!strcmp(entry->cover_path,"/Custom/disc2.png"));
    if(kind==KUI_MEDIA_MANUAL)progress(ctx,KUI_MEDIA_MANUAL_PAGE,1,5);
    progress(ctx,KUI_MEDIA_RECEIVE,512,1024);
    if(kind==KUI_MEDIA_PREVIEW)progress(ctx,KUI_MEDIA_CONVERTING,0,0);
    if(kind==KUI_MEDIA_COVER)progress(ctx,KUI_MEDIA_COVER_SCALING,0,0);
    if(++state.requests==state.inject_at) {
        if(state.inject==KUI_MEDIA_STOPPED)state.stopped=true;
        return state.inject;
    }
    if(id==0 && kind==KUI_MEDIA_COVER)return KUI_MEDIA_MISSING;
    if(id==10 && kind==KUI_MEDIA_MANUAL)return KUI_MEDIA_ORIGINAL;
    if(id==1 && kind==KUI_MEDIA_COVER)return KUI_MEDIA_UNIDENTIFIED;
    if(entry->cover_path[0] && kind==KUI_MEDIA_COVER)return KUI_MEDIA_EXISTING;
    return KUI_MEDIA_INSTALLED;
}
static void report(const struct kui_app_status *out) {
    if(!strncmp(out->message,"Manual page 1 of 5:",19))state.saw_page=true;
    if(!strncmp(out->message,"Resizing box art:",17))state.saw_scaling=true;
    if(!strncmp(out->message,"Preparing preview:",18)) {
        assert(!out->total && !out->done);
        state.saw_conversion=true;
    }
    if(out->total) {
        assert(out->total==1024 && out->done==512);
        ++state.progress_updates;
    }
}
static void reset(void) {memset(&state,0,sizeof(state));state.online=true;}
int main(void) {
    struct kui_games_media_job job={.all=true,.kinds=7};
    struct kui_app_status status;
    reset();assert(!kui_games_media_run(&job,&status,NULL,stop,report));
    assert(status.complete && status.passed && !status.stopped && !status.errors);
    assert(state.route==KUI_INTERNET_AUTO && state.opens==1 && state.ends==1 && state.releases==1);
    assert(state.main_pages==2 && state.group_pages==1 && state.folder_pages==1);
    assert(state.seen[0][KUI_MEDIA_MANUAL] && state.seen[0][KUI_MEDIA_PREVIEW]); /* Missing cover only. */
    assert(!state.seen[1][KUI_MEDIA_MANUAL] && !state.seen[1][KUI_MEDIA_PREVIEW]); /* No game identity. */
    assert(!state.seen[5][0] && !state.seen[9][0]); /* Disabled row and group representative. */
    assert(state.seen[8][0] && state.seen[10][0] && state.seen[11][0] && state.seen[12][0] && state.seen[13][0]);
    assert(state.requests==34 && state.progress_updates==34 && state.saw_page && state.saw_conversion && state.saw_scaling);
    assert(strstr(status.lines[0],"Downloaded: 30") && strstr(status.lines[0],"Already present: 2"));
    assert(strstr(status.lines[1],"Unavailable assets: 1") && strstr(status.lines[1],"Unmatched games: 1"));
    assert(strstr(status.lines[2],"PDF-only manuals: 1"));
    const enum kui_media_result failures[]={KUI_MEDIA_LIMITED,KUI_MEDIA_STOPPED,KUI_MEDIA_ERROR};
    for(size_t i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
        reset();state.inject_at=2;state.inject=failures[i];kui_games_media_run(&job,&status,NULL,stop,report);
        assert(status.complete && !status.passed && status.stopped==(state.inject==KUI_MEDIA_STOPPED));
        assert(state.requests==2 && state.ends==1 && state.releases==1 && state.main_pages==1);
    }
    reset();state.online=false;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors==1 && !strcmp(status.message,"Adapter link down.") && !state.requests && !state.ends && state.releases==1);
    reset();state.fail_mount=true;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors && !state.opens && !state.requests);
    reset();job.all=false;job.kinds=KUI_GAMES_MEDIA_ART;job.route=KUI_INTERNET_DREAMPI;game(&job.selected,12);
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && state.route==KUI_INTERNET_DREAMPI && state.requests==1 && state.seen[12][0]);
    reset();state.stopped=true;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.stopped && !state.opens && !state.requests);
    job.kinds=KUI_GAMES_MEDIA_MANUAL;
    const enum kui_media_result manuals[]={KUI_MEDIA_INSTALLED,KUI_MEDIA_EXISTING,KUI_MEDIA_ORIGINAL,
                                          KUI_MEDIA_ORIGINAL_EXISTING,KUI_MEDIA_MISSING};
    for(unsigned i=0;i<sizeof(manuals)/sizeof(manuals[0]);i++) {
        reset();state.inject_at=1;state.inject=manuals[i];
        bool readable=kui_games_media_run(&job,&status,NULL,stop,report);
        assert(readable==(i<2) && status.passed && status.line_count==4);
        const char *expected=readable?"offline reading":i<4?"Original PDF":"No manual";
        assert(strstr(status.lines[2],expected));
    }
    reset();job.kinds=KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW;
    state.inject_at=2;state.inject=KUI_MEDIA_STOPPED;
    assert(kui_games_media_run(&job,&status,NULL,stop,report));
    assert(status.stopped && strstr(status.lines[2],"offline reading"));
    puts("PASS paged media batch, manual availability, grouped discs, cleanup and progress");
    return 0;
}
