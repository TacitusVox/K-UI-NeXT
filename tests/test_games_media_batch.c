/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real batch worker against paginated catalogue snapshots.
 * The separate image-backed network test checks the TCP/file implementation. */
#include "kui/games_media.h"
#include "kui/artwork_cleanup.h"
#include "kui/media_scan_report.h"
#include "kui/internet.h"
#include "kui/media_download.h"
#include "kui/games_covers.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    unsigned opens,ends,releases,main_pages,group_pages,folder_pages,requests,checked,cached;
    unsigned seen[14][3],progress_updates;
    enum kui_artwork_mode expected_artwork_mode;
    bool selected_group,independent_media,saw_second_disc;
    bool saw_page,saw_conversion,saw_scaling;
    bool stopped,online,opened_sd,fail_mount;
    enum kui_internet_route route;
    enum kui_media_result inject;
    unsigned inject_at,inject_count;
    struct kui_media_report_entry details[40];
    unsigned detail_count;
    char report_state[32];
    bool report_failed;
    bool fail_cleanup;
    unsigned cleanups;
} state;

struct kui_media_report_writer { int placeholder; };
static struct kui_media_report_writer writer;
struct kui_media_report_writer *kui_media_report_begin(bool all,unsigned kinds,kui_log_fn log) {
    (void)all;(void)kinds;(void)log;
    return &writer;
}
void kui_media_report_add(struct kui_media_report_writer *w,const struct kui_media_report_entry *entry) {
    assert(w==&writer && state.detail_count<40);
    state.details[state.detail_count++]=*entry;
}
bool kui_media_report_finish(struct kui_media_report_writer *w,const char *status,bool publish) {
    if(!w || !publish || state.report_failed) return false;
    snprintf(state.report_state,sizeof(state.report_state),"%s",status);
    return true;
}
static bool stop(void) {return state.stopped;}
bool kui_artwork_cleanup_mounted(unsigned *removed,kui_log_fn log,kui_cancel_fn cancel) {
    (void)log;
    assert(state.opened_sd && !cancel());
    ++state.cleanups; *removed=0;
    return !state.fail_cleanup;
}
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
const char *kui_internet_problem(const struct kui_internet *s) {assert(s==&mock && s->active);return state.inject==KUI_MEDIA_CONNECT_FAILED ? "Cannot connect: media.example.org:443" : "Certificate check failed.";}
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
    if(*group && state.selected_group) {
        assert(!strcmp(root,"/Games/Category") && !strcmp(group,"two-discs") && offset<2);
        ++state.group_pages;
        page->count=1;page->total=2;page->has_more=offset==0;
        game(&page->entries[0],10+offset);
        page->entries[0].disc=1+offset;
        if(!state.independent_media)strcpy(page->entries[0].media_disc1_key,"0123456789abcdef0123456789abcdef");
    } else if(!strcmp(root,"/Games/Category")) {
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
enum kui_media_result kui_media_fetch_internet(struct kui_media_game *game,
        enum kui_media_kind kind,kui_media_connect_fn connect,kui_log_fn log,kui_cancel_fn cancel,kui_media_progress_fn progress,void *ctx) {
    const struct kui_games_entry *entry=game->entry;
    (void)log;
    assert(!cancel());
    assert(!entry->directory && !entry->disabled && !entry->group[0]);
    assert(game->artwork_mode==state.expected_artwork_mode);
    unsigned id;
    assert(sscanf(entry->name,"Game %u",&id)==1 && id<14 && kind<=KUI_MEDIA_PREVIEW);
    assert(!state.seen[id][kind]++);
    if(++state.checked<=state.cached)return KUI_MEDIA_EXISTING;
    char product[11];
    snprintf(product,sizeof(product),"T-%u",id);
    if(game->identity.product[0])assert(!strcmp(game->identity.product,product));
    else strcpy(game->identity.product,product);
    struct kui_internet *session=connect(ctx);
    if(!session)return KUI_MEDIA_ERROR;
    assert(session==&mock && session->active);
    if(id==10 && !state.selected_group)assert(!strcmp(entry->cover_path,"/Custom/group.png"));
    if(id==11 && !state.selected_group)assert(!strcmp(entry->cover_path,"/Custom/disc2.png"));
    if(kind==KUI_MEDIA_MANUAL)progress(ctx,KUI_MEDIA_MANUAL_PAGE,1,5);
    progress(ctx,KUI_MEDIA_RECEIVE,512,1024);
    if(kind==KUI_MEDIA_PREVIEW)progress(ctx,KUI_MEDIA_CONVERTING,0,0);
    if(kind==KUI_MEDIA_COVER)progress(ctx,KUI_MEDIA_COVER_SCALING,0,0);
    ++state.requests;
    if(state.requests==state.inject_at || (state.requests>state.inject_at && state.requests-state.inject_at<state.inject_count)) {
        if(state.inject==KUI_MEDIA_STOPPED)state.stopped=true;
        return state.inject;
    }
    if(id==0 && kind==KUI_MEDIA_COVER) {
        game->reason=KUI_MEDIA_REASON_NOT_LISTED;
        return KUI_MEDIA_MISSING;
    }
    if(id==10 && kind==KUI_MEDIA_MANUAL)return KUI_MEDIA_ORIGINAL;
    if(id==1 && kind==KUI_MEDIA_COVER) {
        game->reason=KUI_MEDIA_REASON_NO_RELEASE;
        return KUI_MEDIA_UNIDENTIFIED;
    }
    if(entry->cover_path[0] && kind==KUI_MEDIA_COVER)return KUI_MEDIA_EXISTING;
    return KUI_MEDIA_INSTALLED;
}
static void report(const struct kui_app_status *out) {
    if(strstr(out->message,"Disc 2 - Game 11"))state.saw_second_disc=true;
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
    assert(state.detail_count==3 && !strcmp(state.report_state,"Completed"));
    assert(!strcmp(status.lines[status.line_count-2],"Unmatched: Game 1"));
    assert(!strcmp(state.details[0].title,"Game 0") && !strcmp(state.details[0].category,"Unavailable") &&
        !strcmp(state.details[0].asset,"Artwork") && !strcmp(state.details[0].reason,"No source listed for this asset") &&
        !strcmp(state.details[0].product,"T-0") && !strcmp(state.details[0].path,"/Games/Test 0/disc.gdi"));
    assert(!strcmp(state.details[1].title,"Game 1") && !strcmp(state.details[1].category,"Unmatched") &&
        strstr(state.details[1].reason,"No matching release"));
    assert(!strcmp(state.details[2].category,"PDF only") && !strcmp(state.details[2].asset,"Manual"));
    reset();state.report_failed=true;
    assert(!kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(strstr(status.lines[status.line_count-1],"Could not save"));
    reset();state.fail_cleanup=true;
    assert(!kui_games_media_run(&job,&status,NULL,stop,report) && status.passed && state.cleanups==2);
    assert(strstr(status.lines[status.line_count-2],"Storage cleanup incomplete") &&
        !strcmp(state.report_state,"Completed"));
    const enum kui_media_result failures[]={KUI_MEDIA_LIMITED,KUI_MEDIA_STOPPED,KUI_MEDIA_ERROR};
    for(size_t i=0;i<sizeof(failures)/sizeof(failures[0]);i++) {
        reset();state.inject_at=2;state.inject=failures[i];kui_games_media_run(&job,&status,NULL,stop,report);
        assert(status.complete && !status.passed && status.stopped==(state.inject==KUI_MEDIA_STOPPED));
        assert(state.requests==2 && state.ends==1 && state.releases==1 && state.main_pages==1);
    }
    reset();state.inject_at=2;state.inject=KUI_MEDIA_CONNECT_FAILED;
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(!status.passed && status.errors==1 && state.requests==34 && state.main_pages==2);
    assert(strstr(status.message,"Retry 1 failed connection") && strstr(status.lines[2],"Failed Manual: Game 0"));
    assert(strstr(status.lines[3],"media.example.org:443") && state.seen[0][KUI_MEDIA_PREVIEW]);
    reset();state.inject_at=1;state.inject_count=3;state.inject=KUI_MEDIA_CONNECT_FAILED;
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(!status.passed && status.errors==3 && state.requests==3 && state.main_pages==1);
    assert(strstr(status.message,"three connection failures") && strstr(status.lines[2],"Failed Preview: Game 0"));
    reset();state.online=false;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors==1 && !strcmp(status.message,"Adapter link down.") && !state.requests && !state.ends && state.releases==1);
    reset();state.online=false;state.cached=4;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors && state.checked==5 && state.opens==1 && state.releases==1 && !state.ends);
    assert(!strcmp(status.message,"Adapter link down.") && strstr(status.lines[0],"Already present: 4"));
    reset();state.online=false;state.cached=36;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && state.checked==36 && !state.opens && !state.ends && state.releases==1);
    assert(strstr(status.lines[0],"Already present: 36"));
    reset();state.fail_mount=true;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.errors && !state.opens && !state.requests);
    reset();job.all=false;job.kinds=KUI_GAMES_MEDIA_ART;job.route=KUI_INTERNET_DREAMPI;game(&job.selected,12);
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && state.route==KUI_INTERNET_DREAMPI && state.requests==1 && state.seen[12][0]);
    reset();state.online=false;state.cached=1;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && !state.opens && !state.requests && state.checked==1 && state.releases==1);
    reset();state.stopped=true;kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.stopped && !state.opens && !state.requests);
    job.kinds=KUI_GAMES_MEDIA_MANUAL;
    const enum kui_media_result manuals[]={KUI_MEDIA_INSTALLED,KUI_MEDIA_EXISTING,KUI_MEDIA_ORIGINAL,
                                          KUI_MEDIA_ORIGINAL_EXISTING,KUI_MEDIA_MISSING};
    for(unsigned i=0;i<sizeof(manuals)/sizeof(manuals[0]);i++) {
        reset();state.inject_at=1;state.inject=manuals[i];
        bool readable=kui_games_media_run(&job,&status,NULL,stop,report);
        assert(readable==(i<2) && status.passed && status.line_count==5);
        const char *expected=readable?"offline reading":i<4?"Original PDF":"No manual";
        assert(strstr(status.lines[2],expected));
    }
    reset();job.kinds=KUI_GAMES_MEDIA_MANUAL|KUI_GAMES_MEDIA_PREVIEW;
    state.inject_at=2;state.inject=KUI_MEDIA_STOPPED;
    assert(kui_games_media_run(&job,&status,NULL,stop,report));
    assert(status.stopped && strstr(status.lines[2],"offline reading"));

    /* Selecting the set inside a category checks each disc's cover, across
     * page boundaries. The successful shared assets are fetched once. */
    reset();state.selected_group=true;
    job=(struct kui_games_media_job){.kinds=7};
    strcpy(job.selected_root,"/Games/Category");game(&job.selected,10);
    strcpy(job.selected.group,"two-discs");
    strcpy(job.selected.media_disc1_key,"0123456789abcdef0123456789abcdef");
    state.inject_at=2;state.inject=KUI_MEDIA_INSTALLED;
    assert(kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(state.group_pages==2 && !state.main_pages && !state.folder_pages && state.saw_second_disc);
    assert(state.seen[10][KUI_MEDIA_COVER] && state.seen[11][KUI_MEDIA_COVER]);
    assert(!state.seen[11][KUI_MEDIA_MANUAL] && !state.seen[11][KUI_MEDIA_PREVIEW]);
    assert(state.requests==4 && strstr(status.lines[0],"Discs checked: 2"));
    assert(strstr(status.lines[2],"offline reading"));

    /* A verified set has a single preview. Even a provider 404 must not
     * trigger a second transfer or a second unavailability for Disc 2. */
    reset();state.selected_group=true;
    state.inject_at=3;state.inject=KUI_MEDIA_MISSING;
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && state.requests==4 && strstr(status.lines[1],"Unavailable assets: 1"));
    assert(state.seen[10][KUI_MEDIA_PREVIEW] && !state.seen[11][KUI_MEDIA_PREVIEW]);
    assert(state.seen[11][KUI_MEDIA_COVER]);

    /* Independent media stays per disc. The report's manual shortcut opens
     * the parent's own selector, whose manual in this case is PDF-only. */
    reset();state.selected_group=state.independent_media=true;
    job.selected.media_disc1_key[0]=0;job.kinds=KUI_GAMES_MEDIA_MANUAL;
    assert(!kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(state.requests==2 && state.seen[11][KUI_MEDIA_MANUAL] && strstr(status.lines[2],"Original PDF"));
    reset();state.selected_group=true;
    job.selected.group[0]=0;game(&job.selected,11);job.selected.disc=2;job.kinds=KUI_GAMES_MEDIA_PREVIEW;
    assert(!kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(state.requests==1 && !state.group_pages && state.seen[11][KUI_MEDIA_PREVIEW]);
    reset();state.selected_group=true;state.inject_at=1;state.inject=KUI_MEDIA_UNIDENTIFIED;
    strcpy(job.selected.media_disc1_key,"0123456789abcdef0123456789abcdef");job.kinds=7;
    assert(kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(state.requests==3 && state.seen[11][KUI_MEDIA_MANUAL] && state.seen[11][KUI_MEDIA_PREVIEW]);
    assert(strstr(status.lines[1],"Unmatched games: 1") && strstr(status.lines[2],"offline reading"));
    reset();state.selected_group=true;state.inject_at=1;state.inject_count=3;state.inject=KUI_MEDIA_UNIDENTIFIED;
    assert(!kui_games_media_run(&job,&status,NULL,stop,report) && status.passed);
    assert(state.requests==3 && strstr(status.lines[1],"Unmatched games: 1"));
    for(unsigned all=0;all<=1;all++)for(unsigned mode=KUI_ART_MISSING;mode<KUI_ART_MODE_COUNT;mode++) {
        reset();state.cached=100;
        state.expected_artwork_mode=(enum kui_artwork_mode)mode;
        job=(struct kui_games_media_job){.all=all,.kinds=KUI_GAMES_MEDIA_ART,.artwork_mode=state.expected_artwork_mode};
        game(&job.selected,12);
        kui_games_media_run(&job,&status,NULL,stop,report);
        assert(status.passed && state.checked && (all || state.checked==1));
    }
    reset();state.expected_artwork_mode=KUI_ART_REDOWNLOAD;
    state.inject_at=1;state.inject=KUI_MEDIA_OWNER;
    job=(struct kui_games_media_job){.kinds=KUI_GAMES_MEDIA_ART,.artwork_mode=KUI_ART_REDOWNLOAD};
    game(&job.selected,12);
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && strstr(status.lines[0],"Covers installed: 0") && strstr(status.lines[1],"Owner art kept: 1"));
    reset();state.expected_artwork_mode=KUI_ART_REDOWNLOAD;
    state.inject_at=1;state.inject=KUI_MEDIA_EXISTING;
    kui_games_media_run(&job,&status,NULL,stop,report);
    assert(status.passed && strstr(status.lines[0],"Covers installed: 0") && strstr(status.lines[0],"Already current: 1"));
    reset();state.expected_artwork_mode=KUI_ART_CHECK;state.cached=1;
    job.artwork_mode=KUI_ART_CHECK;job.kinds|=KUI_GAMES_MEDIA_MANUAL;
    assert(kui_games_media_run(&job,&status,NULL,stop,report));
    assert(status.passed && strstr(status.lines[0],"Installed: 1 (covers: 0)") && strstr(status.lines[0],"Already present: 1"));
    puts("PASS paged media batch, manual availability, grouped discs, cleanup and progress");
    return 0;
}
