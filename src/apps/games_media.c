/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games_media.h"
#include "kui/artwork_cleanup.h"
#include "kui/media_scan_report.h"
#include "kui/games_covers.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include "kui/media_download.h"
#include "kui/internet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum manual_status { MANUAL_UNCHECKED, MANUAL_READABLE, MANUAL_PDF, MANUAL_MISSING };

struct batch {
    const struct kui_games_media_job *job;
    struct kui_artwork_batch checked_art;
    struct kui_media_report_writer *report;
    struct kui_internet *session;
    char current[104],first_unmatched[KUI_DEST_NAME_CAP];
    char adapter[40];
    char connection_problem[128],failed_asset[KUI_APP_LINE_CAP],failed_site[KUI_APP_LINE_CAP];
    unsigned skipped_connections,consecutive_connections;
    unsigned covers_removed,previews_removed;
    bool cover_cleanup_pending,preview_cleanup_pending;
    bool requested_connection,refresh_artwork;
    struct kui_app_status *out;
    kui_log_fn log;
    kui_cancel_fn cancel;
    kui_app_progress_fn progress;
    enum manual_status manual;
    unsigned games,installed,covers_installed,existing,missing,unknown,owner_art,pdfs,folders_skipped,visited;
};
static bool stopped(const struct batch *b) {return b->cancel && b->cancel();}
static void counts(struct batch *b) {
    struct kui_app_status *s=b->out;
    const char *items=b->job && !b->job->all && b->job->selected.group[0]?"Discs":"Games";
    if(b->refresh_artwork && b->job->kinds==KUI_GAMES_MEDIA_ART) {
        snprintf(s->lines[0],sizeof(s->lines[0]),"%s: %u   Covers installed: %u   Already current: %u",
            items,b->games,b->covers_installed,b->existing);
    } else if(b->refresh_artwork) {
        snprintf(s->lines[0],sizeof(s->lines[0]),"%s: %u   Installed: %u (covers: %u)   Already present: %u",
            items,b->games,b->installed,b->covers_installed,b->existing);
    } else snprintf(s->lines[0],sizeof(s->lines[0]),"%s checked: %u   Downloaded: %u   Already present: %u",
        items,b->games,b->installed,b->existing);
    snprintf(s->lines[1],sizeof(s->lines[1]),"Unavailable assets: %u   Unmatched games: %u",b->missing,b->unknown);
    if(b->owner_art) {
        size_t used=strlen(s->lines[1]);
        snprintf(s->lines[1]+used,sizeof(s->lines[1])-used,"   Owner art kept: %u",b->owner_art);
    }
    s->line_count=2;
    if(b->failed_asset[0]) {
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"%s",b->failed_asset);
        if(b->failed_site[0]) snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"%s",b->failed_site);
    }
    if(b->manual != MANUAL_UNCHECKED) {
        static const char *const labels[] = {
            [MANUAL_READABLE] = "Manual ready for offline reading.",
            [MANUAL_PDF] = "Original PDF on card. No readable page scans available.",
            [MANUAL_MISSING] = "No manual found for this release."
        };
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"%s",labels[b->manual]);
    } else if(b->pdfs || b->folders_skipped) {
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"PDF-only manuals: %u   Folders below scan depth: %u",b->pdfs,b->folders_skipped);
    }
    if(b->adapter[0])
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"Connection: %s",b->adapter);
    if(s->complete && b->first_unmatched[0])
        snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"%s: %.60s",
            b->unknown==1?"Unmatched":"First unmatched",b->first_unmatched);
    if(s->complete) {
        bool pending=b->cover_cleanup_pending || b->preview_cleanup_pending;
        if(b->covers_removed || b->previews_removed)
            snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"Old files removed: %u covers, %u previews%s",
                b->covers_removed,b->previews_removed,pending?". More cleanup pending.":"");
        else if(pending) snprintf(s->lines[s->line_count++],KUI_APP_LINE_CAP,"Storage cleanup incomplete. Retry the media scan.");
    }
}
static void transfer_progress(void *ctx,enum kui_media_phase phase,uint32_t done,uint32_t total) {
    struct batch *b=ctx;
    if(phase==KUI_MEDIA_CONVERTING)
        snprintf(b->out->message,sizeof(b->out->message),"Preparing preview: %.100s",b->current);
    else if(phase==KUI_MEDIA_COVER_SCALING)
        snprintf(b->out->message,sizeof(b->out->message),"Resizing box art: %.102s",b->current);
    else if(phase==KUI_MEDIA_MANUAL_PAGE)
        snprintf(b->out->message,sizeof(b->out->message),"Manual page %u of %u: %.85s",(unsigned)done,(unsigned)total,b->current);
    b->out->done=phase==KUI_MEDIA_RECEIVE?done:0;
    b->out->total=phase==KUI_MEDIA_RECEIVE?total:0;
    if(b->progress)b->progress(b->out);
}
static struct kui_internet *connect_for_download(void *ctx) {
    struct batch *b=ctx;
    b->requested_connection=true;
    if(b->session || stopped(b))return b->session;
    char asset[sizeof(b->out->message)];
    snprintf(asset,sizeof(asset),"%s",b->out->message);
    snprintf(b->out->message,sizeof(b->out->message),"%s",b->job->route==KUI_INTERNET_DREAMPI?
        "Connecting through the DreamPi modem...":"Connecting to the internet...");
    if(b->progress)b->progress(b->out);
    if(stopped(b))return NULL;
    b->session=kui_internet_open(b->job->route,b->log,b->cancel,b->connection_problem);
    if(b->session) {
        snprintf(b->adapter,sizeof(b->adapter),"%s",kui_internet_adapter(b->session));
        snprintf(b->out->message,sizeof(b->out->message),"%s",asset);
        counts(b);
        if(b->progress)b->progress(b->out);
    } else if(!b->connection_problem[0]) {
        snprintf(b->connection_problem,sizeof(b->connection_problem),"Could not connect to the internet.");
    }
    return b->session;
}
static void record_issue(struct batch *b,const struct kui_games_entry *entry,
        const struct kui_games_disc_identity *id,const char *category,const char *asset,const char *reason) {
    struct kui_media_report_entry e={0};
    const char *name=entry->title[0]?entry->title:entry->name;
    if(entry->disc) {
        name=entry->custom_title?name:id && id->title[0]?id->title:entry->name;
        snprintf(e.title,sizeof(e.title),"%.*s (Disc %u)",(int)sizeof(e.title)-20,name,entry->disc);
    } else snprintf(e.title,sizeof(e.title),"%.*s",(int)sizeof(e.title)-1,name);
    if(!b->first_unmatched[0] && !strcmp(category,"Unmatched"))
        snprintf(b->first_unmatched,sizeof(b->first_unmatched),"%s",e.title);
    snprintf(e.path,sizeof(e.path),"%s",entry->path);
    if(id) snprintf(e.product,sizeof(e.product),"%s",id->product);
    snprintf(e.category,sizeof(e.category),"%s",category);
    snprintf(e.asset,sizeof(e.asset),"%s",asset);
    snprintf(e.reason,sizeof(e.reason),"%s",reason);
    kui_media_report_add(b->report,&e);
}
static bool fetch_game(struct batch *b,const struct kui_games_entry *entry,unsigned *shared_done) {
    static const enum kui_media_kind kind[]={KUI_MEDIA_COVER,KUI_MEDIA_MANUAL,KUI_MEDIA_PREVIEW};
    static const char *const name[]={"Artwork","Manual","Preview"};
    if(b->games>=KUI_GAMES_SCAN_MAX) {
        snprintf(b->out->message,sizeof(b->out->message),"Reached the %u-game scan limit.",KUI_GAMES_SCAN_MAX);
        return false;
    }
    b->games++;
    if(entry->disc) {
        snprintf(b->current,sizeof(b->current),"Disc %u - %.82s",entry->disc,entry->name);
        if(b->log)b->log("Game media: %s (%.110s)",b->current,entry->path);
    } else snprintf(b->current,sizeof(b->current),"%.103s",entry->name);
    bool selected_manual=!b->job->all && (shared_done || !strcmp(entry->path,b->job->selected.path));
    bool unidentified=false;
    struct kui_media_game game={.entry=entry,.artwork_batch=&b->checked_art,
        .artwork_mode=b->job->artwork_mode};
    for(unsigned i=0;i<3;i++) {
        unsigned bit=1u<<i;
        if(!(b->job->kinds & bit) || (shared_done && (*shared_done & bit)))continue;
        if(stopped(b))return false;
        snprintf(b->out->message,sizeof(b->out->message),"%s: %.102s",name[i],b->current);
        b->out->done=b->out->total=0;
        counts(b);
        if(b->progress)b->progress(b->out);
        b->requested_connection=false;
        enum kui_media_result r=kui_media_fetch_internet(&game,kind[i],connect_for_download,b->log,b->cancel,transfer_progress,b);
        if(kind[i]==KUI_MEDIA_PREVIEW) {
            b->previews_removed+=game.previews_removed;
            b->preview_cleanup_pending|=game.preview_cleanup_pending;
        }
        if(shared_done && kind[i]!=KUI_MEDIA_COVER && (r==KUI_MEDIA_INSTALLED || r==KUI_MEDIA_EXISTING ||
           r==KUI_MEDIA_ORIGINAL || r==KUI_MEDIA_ORIGINAL_EXISTING ||
           (kind[i]==KUI_MEDIA_PREVIEW && (r==KUI_MEDIA_MISSING || r==KUI_MEDIA_CONNECT_FAILED)))) *shared_done|=bit;
        if(selected_manual && kind[i]==KUI_MEDIA_MANUAL) {
            if(r==KUI_MEDIA_INSTALLED || r==KUI_MEDIA_EXISTING)b->manual=MANUAL_READABLE;
            else if(r==KUI_MEDIA_ORIGINAL || r==KUI_MEDIA_ORIGINAL_EXISTING)b->manual=MANUAL_PDF;
            else if(r==KUI_MEDIA_MISSING)b->manual=MANUAL_MISSING;
        }
        if(r==KUI_MEDIA_INSTALLED || r==KUI_MEDIA_ORIGINAL) b->consecutive_connections=0;
        const struct kui_games_disc_identity *id=kind[i]!=KUI_MEDIA_COVER && game.set_identity.product[0]?
            &game.set_identity:&game.identity;
        const char *reason=kui_media_reason_text(game.reason);
        switch(r) {
        case KUI_MEDIA_INSTALLED:
            b->installed++;
            if(kind[i]==KUI_MEDIA_COVER)b->covers_installed++;
            break;
        case KUI_MEDIA_ORIGINAL:
        case KUI_MEDIA_ORIGINAL_EXISTING:
            if(r==KUI_MEDIA_ORIGINAL) b->installed++; else b->existing++;
            ++b->pdfs;
            record_issue(b,entry,id,"PDF only",name[i],"Original PDF saved. No readable page scans available.");
            break;
        case KUI_MEDIA_EXISTING: b->existing++;break;
        case KUI_MEDIA_OWNER: b->owner_art++;break;
        case KUI_MEDIA_MISSING:
            ++b->missing;
            record_issue(b,entry,id,"Unavailable",name[i],*reason?reason:"Asset unavailable");
            break;
        case KUI_MEDIA_UNIDENTIFIED:
            if(!unidentified) {
                ++b->unknown;
                record_issue(b,entry,id,"Unmatched",name[i],*reason?reason:"Could not identify a matching release");
            }
            unidentified=true;
            /* Later discs may lack their own cover identity while Disc 1
             * supplies a cached or identifiable manual and preview. */
            if(!entry->media_disc1_key[0])return true;
            break;
        case KUI_MEDIA_STOPPED: b->out->stopped=true;return false;
        case KUI_MEDIA_CONNECT_FAILED: {
            const char *problem=kui_internet_problem(b->session);
            record_issue(b,entry,id,"Connection failure",name[i],problem);
            snprintf(b->failed_asset,sizeof(b->failed_asset),"Failed %s: %.58s",name[i],b->current);
            snprintf(b->failed_site,sizeof(b->failed_site),"%.79s",problem);
            if(b->log)b->log("Game media: %s for %s: %s",name[i],b->current,problem);
            ++b->consecutive_connections;
            if(!b->job->all || b->consecutive_connections>=3) {
                snprintf(b->out->message,sizeof(b->out->message),"%s",b->job->all ?
                    "Scan stopped after three connection failures. Retry later." : problem);
                return false;
            }
            b->skipped_connections++;
            break;
        }
        case KUI_MEDIA_LIMITED:
            record_issue(b,entry,id,"Provider limit",name[i],"Provider requested that the scan retry later");
            snprintf(b->out->message,sizeof(b->out->message),"Provider limit reached. Retry later to continue.");
            return false;
        default: {
            const char *problem=b->requested_connection && b->session ? kui_internet_problem(b->session) : b->connection_problem;
            snprintf(b->failed_asset,sizeof(b->failed_asset),"Failed %s: %.58s",name[i],b->current);
            b->failed_site[0]=0;
            record_issue(b,entry,id,"Failed",name[i],*problem?problem:*reason?reason:"Could not install the complete asset");
            if(problem[0])
                snprintf(b->out->message,sizeof(b->out->message),"%.127s",problem);
            else snprintf(b->out->message,sizeof(b->out->message),"%s failed for %.75s. Check diagnostics.",name[i],b->current);
            return false;
        }
        }
    }
    return true;
}

/* The catalogue groups discs and only indexes immediate children of each
 * browsable folder. Keep snapshots on the heap. There are no open directory
 * iterators while a fetch or a child catalogue accesses the card. */
static bool walk(struct batch *b,const char *root,const char *group,unsigned depth,const struct kui_games_entry *set) {
    struct kui_games_page *page=malloc(sizeof(*page));
    if(!page) {
        snprintf(b->out->message,sizeof(b->out->message),"Out of memory while listing games.");
        return false;
    }
    bool ok=true;
    /* One preview source per verified set, even across pages of a bulk scan.
     * Unidentified members can still fall back to the next disc. */
    unsigned shared_done=0;
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
                else {
                    ++b->folders_skipped;
                    record_issue(b,entry,NULL,"Folder skipped","Library","Below the maximum scan depth");
                }
            } else if(!*group && entry->group[0]) {
                ok=walk(b,root,entry->group,depth,entry);
            } else {
                if(set && set->cover_path[0] && !entry->cover_path[0])
                    snprintf(entry->cover_path,sizeof(entry->cover_path),"%s",set->cover_path);
                bool shared=set && entry->media_disc1_key[0] &&
                    !strcmp(entry->media_disc1_key,set->media_disc1_key);
                ok=fetch_game(b,entry,shared?&shared_done:NULL);
            }
        }
        if(!page->has_more)break;
        offset+=page->count;
    }
    free(page);
    return ok;
}

/* Recover leftovers before transfers need space. After the scan, collect
 * generations displaced by this run. A clean card only checks the marker. */
static void cleanup_artwork(struct batch *b) {
    if(!(b->job->kinds & KUI_GAMES_MEDIA_ART) || stopped(b)) return;
    unsigned removed=0;
    b->cover_cleanup_pending=kui_games_storage_open(b->log)!=KUI_GAMES_STORAGE_OK ||
        !kui_artwork_cleanup_mounted(&removed,b->log,b->cancel);
    b->covers_removed+=removed;
}

bool kui_games_media_run(const struct kui_games_media_job *job,struct kui_app_status *out,
        kui_log_fn log,kui_cancel_fn cancel,kui_app_progress_fn progress) {
    memset(out,0,sizeof(*out));
    struct batch b={.job=job,.out=out,.log=log,.cancel=cancel,.progress=progress};
    bool ok=false;
    b.refresh_artwork=job && (job->kinds & KUI_GAMES_MEDIA_ART) && job->artwork_mode!=KUI_ART_MISSING;
    if(!job || !job->kinds || (unsigned)job->artwork_mode>=KUI_ART_MODE_COUNT ||
       (unsigned)job->route>=KUI_INTERNET_ROUTE_COUNT || (job->kinds & ~7u) || (!job->all &&
       (!job->selected.path[0] || job->selected.directory || job->selected.disabled))) {
        snprintf(out->message,sizeof(out->message),"Select a game and choose a media type.");
    } else if(!stopped(&b) && kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK) {
        snprintf(out->message,sizeof(out->message),"Cannot open the SD card to store game media.");
    } else if(!stopped(&b)) {
        snprintf(out->message,sizeof(out->message),"%s",b.refresh_artwork?
            "Starting artwork scan...":"Checking the card for missing game media...");
        if(progress)progress(out);
        b.report=kui_media_report_begin(job->all,job->kinds,log);
        cleanup_artwork(&b);
        if(job->all) ok=walk(&b,KUI_GAMES_SCAN_ROOT,"",0,NULL);
        else if(job->selected.group[0]) {
            const char *root=job->selected_root[0]?job->selected_root:KUI_GAMES_SCAN_ROOT;
            ok=walk(&b,root,job->selected.group,0,&job->selected);
        } else ok=fetch_game(&b,&job->selected,NULL);
        if(b.session)kui_internet_close(b.session);
        if(ok || b.covers_installed) cleanup_artwork(&b);
    }
    bool released=kui_games_storage_release();
    if(!released) {
        b.manual=MANUAL_UNCHECKED;
        ok=false;snprintf(out->message,sizeof(out->message),"Could not release SD storage. Check diagnostics.");
    }
    out->complete=true;counts(&b);out->done=out->total=0;
    out->stopped=out->stopped || stopped(&b);out->errors=b.skipped_connections+(!ok && !out->stopped);
    out->passed=ok && !out->errors && !out->stopped;
    if(out->stopped)snprintf(out->message,sizeof(out->message),"Stopped. Completed downloads are on the card.");
    else if(out->passed)snprintf(out->message,sizeof(out->message),"%s",b.refresh_artwork?
        "Game media scan finished.":"Missing-media scan finished.");
    else if(ok) snprintf(out->message,sizeof(out->message),"Scan finished. Retry %u failed connection%s.",
        b.skipped_connections,b.skipped_connections==1?"":"s");
    if(log)log("Game media: %s; %s; %s",out->message,out->lines[0],out->lines[1]);
    bool publish=released && (ok || b.games>0);
    const char *state=out->stopped?"Stopped":ok?(b.skipped_connections?"Completed with errors":"Completed"):"Incomplete";
    bool saved=kui_media_report_finish(b.report,state,publish);
    if(publish && out->line_count<KUI_APP_LINES)
        snprintf(out->lines[out->line_count++],KUI_APP_LINE_CAP,"%s",saved?
            "Scan report: /KUI/media/last-scan.txt":"Could not save this scan report.");
    return b.manual==MANUAL_READABLE;
}
