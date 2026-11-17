/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/games_media.h"
#include "kui/games_covers.h"
#include "kui/games_library.h"
#include "kui/games_storage.h"
#include "kui/media_download.h"
#include "kui/network_probe.h"
#include "ff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADDRESS "0:/KUI/media-relay.txt"
#define PREVIOUS "0:/KUI/media-relay.bak"
#define PENDING "0:/KUI/media-relay.tmp"

static bool usable(const uint8_t ip[4]) {
    return ip[0]!=0 && ip[0]!=127 && ip[0]<224;
}

static bool load_address(uint8_t ip[4]) {
    FIL f;
    FRESULT r=f_open(&f,ADDRESS,FA_READ);
    if(r==FR_NO_FILE || r==FR_NO_PATH)r=f_open(&f,PREVIOUS,FA_READ);
    if(r==FR_NO_FILE || r==FR_NO_PATH) {
        memset(ip,0,4);
        return true;
    }
    if(r!=FR_OK)return false;
    char buf[20]={0};
    FSIZE_t n=f_size(&f);
    UINT got=0;
    bool ok=n>0 && n<sizeof(buf) && f_read(&f,buf,(UINT)n,&got)==FR_OK && got==n;
    if(f_close(&f)!=FR_OK || !ok)return false;
    while(n && (buf[n-1]=='\r' || buf[n-1]=='\n'))buf[--n]=0;
    return strlen(buf)==n && kui_network_parse_ipv4(buf,ip) && usable(ip);
}

static bool save_address(const uint8_t ip[4]) {
    if(!usable(ip))return false;
    FRESULT r=f_mkdir("0:/KUI");
    if(r!=FR_OK && r!=FR_EXIST)return false;
    char buf[20];
    unsigned n=(unsigned)snprintf(buf,sizeof(buf),"%u.%u.%u.%u\n",ip[0],ip[1],ip[2],ip[3]);
    FIL f;
    UINT bytes=0;
    if(f_open(&f,PENDING,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)return false;
    bool ok=f_write(&f,buf,n,&bytes)==FR_OK && bytes==n && f_sync(&f)==FR_OK;
    if(f_close(&f)!=FR_OK || !ok)return false;
    FILINFO stat;
    r=f_stat(ADDRESS,&stat);
    if(r==FR_OK) {
        r=f_unlink(PREVIOUS);
        if((r!=FR_OK && r!=FR_NO_FILE) || f_rename(ADDRESS,PREVIOUS)!=FR_OK)return false;
    } else if(r!=FR_NO_FILE)return false;
    if(f_rename(PENDING,ADDRESS)!=FR_OK)return false;
    uint8_t verified[4]={0};
    return load_address(verified) && !memcmp(ip,verified,4);
}

bool kui_games_media_address(uint8_t ip[4],bool save,kui_log_fn log) {
    if(!ip || kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK)return false;
    bool ok=save?save_address(ip):load_address(ip);
    return kui_games_storage_release() && ok;
}

struct batch {
    const struct kui_games_media_job *job;
    struct kui_bba_session session;
    const struct kui_bba_port *port;
    struct kui_app_status *out;
    kui_log_fn log;
    kui_cancel_fn cancel;
    kui_app_progress_fn progress;
    unsigned games,installed,existing,missing,unknown,folders_skipped,visited;
};
static bool stopped(const struct batch *b) {return b->cancel && b->cancel();}
static void counts(struct batch *b) {
    struct kui_app_status *s=b->out;
    snprintf(s->lines[0],sizeof(s->lines[0]),"Games checked: %u   Downloaded: %u   Already present: %u",
        b->games,b->installed,b->existing);
    snprintf(s->lines[1],sizeof(s->lines[1]),"Unavailable assets: %u   Unmatched games: %u",b->missing,b->unknown);
    s->line_count=2;
    if(b->folders_skipped) {
        snprintf(s->lines[2],sizeof(s->lines[2]),"Folders below the scan depth: %u",b->folders_skipped);
        s->line_count=3;
    }
}
static void transfer_progress(void *ctx,uint32_t received,uint32_t total) {
    struct batch *b=ctx;
    b->out->done=received;b->out->total=total;
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
    for(unsigned i=0;i<3;i++) {
        if(!(b->job->kinds & (1u<<i)))continue;
        if(stopped(b))return false;
        snprintf(b->out->message,sizeof(b->out->message),"%s: %.102s",name[i],entry->name);
        b->out->done=b->out->total=0;
        counts(b);
        if(b->progress)b->progress(b->out);
        enum kui_media_result r=kui_media_fetch_progress(b->port,&b->session,b->job->ip,
            KUI_MEDIA_DEFAULT_PORT,entry,kind[i],b->log,b->cancel,transfer_progress,b);
        switch(r) {
        case KUI_MEDIA_INSTALLED: b->installed++;break;
        case KUI_MEDIA_EXISTING: b->existing++;break;
        case KUI_MEDIA_MISSING: b->missing++;break;
        case KUI_MEDIA_UNIDENTIFIED: b->unknown++;return true;
        case KUI_MEDIA_STOPPED: b->out->stopped=true;return false;
        case KUI_MEDIA_LIMITED:
            snprintf(b->out->message,sizeof(b->out->message),"Provider limit reached. Retry later to continue.");
            return false;
        default:
            snprintf(b->out->message,sizeof(b->out->message),"%s failed for %.75s. Check diagnostics.",name[i],entry->name);
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
    struct batch b={.job=job,.out=out,.log=log,.cancel=cancel,.progress=progress,.port=kui_bba_console_port()};
    bool ok=false;
    if(!job || !usable(job->ip) || !job->kinds || (job->kinds & ~7u) || (!job->all &&
       (!job->selected.path[0] || job->selected.directory || job->selected.disabled))) {
        snprintf(out->message,sizeof(out->message),"Select a game, media type and companion address.");
    } else {
        snprintf(out->message,sizeof(out->message),"Connecting the Dreamcast Broadband Adapter...");
        if(progress)progress(out);
        int online=b.port?b.port->open(&b.session,log,cancel):0;
        if(online==1) {
            ok=job->all?walk(&b,KUI_GAMES_SCAN_ROOT,"",0,NULL):fetch_game(&b,&job->selected);
            b.port->end(&b.session);
        } else snprintf(out->message,sizeof(out->message),"%.110s",b.session.problem[0]?b.session.problem:
            "Broadband Adapter unavailable. Check the adapter and cable.");
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
