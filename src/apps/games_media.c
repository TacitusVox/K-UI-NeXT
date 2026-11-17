/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games_media.h"
#include "kui/games_covers.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include "kui/media_download.h"
#include "kui/internet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct batch {
    const struct kui_games_media_job *job;
    struct kui_internet *session;
    const struct kui_games_entry *current;
    char adapter[40];
    struct kui_app_status *out;
    kui_log_fn log;
    kui_cancel_fn cancel;
    kui_app_progress_fn progress;
    unsigned games,installed,existing,missing,unknown,pdfs,folders_skipped,visited;
};
static bool stopped(const struct batch *b) {return b->cancel && b->cancel();}
static void counts(struct batch *b) {
    struct kui_app_status *s=b->out;
    snprintf(s->lines[0],sizeof(s->lines[0]),"Games checked: %u   Downloaded: %u   Already present: %u",
        b->games,b->installed,b->existing);
    snprintf(s->lines[1],sizeof(s->lines[1]),"Unavailable assets: %u   Unmatched games: %u",b->missing,b->unknown);
    s->line_count=2;
    if(b->pdfs || b->folders_skipped) {
        snprintf(s->lines[2],sizeof(s->lines[2]),"PDF-only manuals: %u   Folders below scan depth: %u",b->pdfs,b->folders_skipped);
        s->line_count=3;
    }
    if(b->adapter[0])
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"Connection: %s",b->adapter);
}
static void transfer_progress(void *ctx,enum kui_media_phase phase,uint32_t done,uint32_t total) {
    struct batch *b=ctx;
    if(phase==KUI_MEDIA_CONVERTING)
        snprintf(b->out->message,sizeof(b->out->message),"Preparing preview: %.100s",b->current->name);
    else if(phase==KUI_MEDIA_MANUAL_PAGE)
        snprintf(b->out->message,sizeof(b->out->message),"Manual page %u of %u: %.85s",(unsigned)done,(unsigned)total,b->current->name);
    b->out->done=phase==KUI_MEDIA_RECEIVE?done:0;
    b->out->total=phase==KUI_MEDIA_RECEIVE?total:0;
    if(b->progress)b->progress(b->out);
}
static bool fetch_game(struct batch *b,const struct kui_games_entry *entry) {
    static const enum kui_media_kind kind[]={KUI_MEDIA_COVER,KUI_MEDIA_MANUAL,KUI_MEDIA_PREVIEW};
    static const char *const name[]={"Artwork","Manual","Preview"};
    if(b->games>=KUI_GAMES_SCAN_MAX) {
        snprintf(b->out->message,sizeof(b->out->message),"Reached the %u-game scan limit.",KUI_GAMES_SCAN_MAX);
        return false;
    }
    b->games++;
    b->current=entry;
    for(unsigned i=0;i<3;i++) {
        if(!(b->job->kinds & (1u<<i)))continue;
        if(stopped(b))return false;
        snprintf(b->out->message,sizeof(b->out->message),"%s: %.102s",name[i],entry->name);
        b->out->done=b->out->total=0;
        counts(b);
        if(b->progress)b->progress(b->out);
        enum kui_media_result r=kui_media_fetch_internet(b->session,entry,kind[i],b->log,b->cancel,transfer_progress,b);
        switch(r) {
        case KUI_MEDIA_INSTALLED: b->installed++;break;
        case KUI_MEDIA_ORIGINAL: b->installed++;b->pdfs++;break;
        case KUI_MEDIA_EXISTING: b->existing++;break;
        case KUI_MEDIA_MISSING: b->missing++;break;
        case KUI_MEDIA_UNIDENTIFIED: b->unknown++;return true;
        case KUI_MEDIA_STOPPED: b->out->stopped=true;return false;
        case KUI_MEDIA_LIMITED:
            snprintf(b->out->message,sizeof(b->out->message),"Provider limit reached. Retry later to continue.");
            return false;
        default:
            if(kui_internet_problem(b->session)[0])
                snprintf(b->out->message,sizeof(b->out->message),"%.127s",kui_internet_problem(b->session));
            else snprintf(b->out->message,sizeof(b->out->message),"%s failed for %.75s. Check diagnostics.",name[i],entry->name);
            return false;
        }
    }
    return true;
}

/* The catalogue groups discs and only indexes immediate children of each
 * browsable folder. Keep snapshots on the heap. There are no open directory
 * iterators while a fetch or a child catalogue accesses the card. */
static bool walk(struct batch *b,const char *root,const char *group,unsigned depth,const char *group_cover) {
    struct kui_games_page *page=malloc(sizeof(*page));
    if(!page) {
        snprintf(b->out->message,sizeof(b->out->message),"Out of memory while listing games.");
        return false;
    }
    bool ok=true;
    for(unsigned offset=0;ok;) {
        if(stopped(b)) {ok=false;break;}
        if(!kui_games_library_rows(root,group,offset,KUI_GAMES_VIEW_SAVED,page,b->log,b->cancel) ||
           (!page->count && page->has_more)) {
            snprintf(b->out->message,sizeof(b->out->message),"Cannot list %.80s. Check the Games library.",root);
            ok=false;break;
        }
        for(unsigned i=0;ok && i<page->count;i++) {
            struct kui_games_entry *entry=&page->entries[i];
            if(++b->visited>KUI_GAMES_SCAN_MAX*4u) {
                snprintf(b->out->message,sizeof(b->out->message),"Reached the folder and entry scan limit.");
                ok=false;break;
            }
            if(entry->disabled)continue;
            if(entry->directory) {
                if(depth<KUI_GAMES_SCAN_DEPTH)ok=walk(b,entry->path,"",depth+1u,NULL);
                else b->folders_skipped++;
            } else if(!*group && entry->group[0]) {
                ok=walk(b,root,entry->group,depth,entry->cover_path);
            } else {
                if(group_cover && *group_cover && !entry->cover_path[0])
                    snprintf(entry->cover_path,sizeof(entry->cover_path),"%s",group_cover);
                ok=fetch_game(b,entry);
            }
        }
        if(!page->has_more)break;
        offset+=page->count;
    }
    free(page);
    return ok;
}

void kui_games_media_run(const struct kui_games_media_job *job,struct kui_app_status *out,
        kui_log_fn log,kui_cancel_fn cancel,kui_app_progress_fn progress) {
    memset(out,0,sizeof(*out));
    struct batch b={.job=job,.out=out,.log=log,.cancel=cancel,.progress=progress};
    bool ok=false;
    if(!job || !job->kinds || (unsigned)job->route>=KUI_INTERNET_ROUTE_COUNT || (job->kinds & ~7u) || (!job->all &&
       (!job->selected.path[0] || job->selected.directory || job->selected.disabled))) {
        snprintf(out->message,sizeof(out->message),"Select a game and choose a media type.");
    } else if(!stopped(&b) && kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK) {
        snprintf(out->message,sizeof(out->message),"Cannot open the SD card to store game media.");
    } else if(!stopped(&b)) {
        snprintf(out->message,sizeof(out->message),"%s",job->route==KUI_INTERNET_DREAMPI?
            "Connecting through the DreamPi modem...":"Connecting to the internet...");
        if(progress)progress(out);
        char problem[128]={0};
        b.session=kui_internet_open(job->route,log,cancel,problem);
        if(b.session) {
            snprintf(b.adapter,sizeof(b.adapter),"%s",kui_internet_adapter(b.session));
            ok=job->all?walk(&b,KUI_GAMES_SCAN_ROOT,"",0,NULL):fetch_game(&b,&job->selected);
            kui_internet_close(b.session);
        } else snprintf(out->message,sizeof(out->message),"%.127s",problem[0]?problem:"Could not connect to the internet.");
    }
    if(!kui_games_storage_release()) {
        ok=false;snprintf(out->message,sizeof(out->message),"Could not release SD storage. Check diagnostics.");
    }
    counts(&b);out->complete=true;out->done=out->total=0;
    out->stopped=out->stopped || stopped(&b);out->passed=ok && !out->stopped;
    if(out->stopped)snprintf(out->message,sizeof(out->message),"Stopped. Completed downloads are on the card.");
    else if(out->passed)snprintf(out->message,sizeof(out->message),"Missing-media scan finished.");
    else out->errors=1;
    if(log)log("Game media: %s; %s; %s",out->message,out->lines[0],out->lines[1]);
}
