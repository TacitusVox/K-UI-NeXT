/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_scan_report.h"
#include "kui/games_storage.h"
#include "ff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPORT_TEMP "0:/KUI/media/last-scan.tmp"
#define REPORT_BACKUP "0:/KUI/media/last-scan.bak"
#define HEADER_SIZE 144u
#define HEADINGS "Status\tAsset\tGame\tProduct\tSelector\tDetails\n"
#define FIRST_PAGE (HEADER_SIZE+sizeof(HEADINGS)-1u)
#define BACKUP_CURSOR (1u<<31)
#define ENTRY_BYTES (sizeof(struct kui_media_report_entry)+8u)

struct kui_media_report_writer {
    char buffer[4096];
    unsigned used,written,count,previous,kinds;
    kui_log_fn log;
    bool all,failed;
};
static bool last_saved;
bool kui_media_report_saved(void) { return last_saved; }

static bool absent(FRESULT r) { return r==FR_NO_FILE || r==FR_NO_PATH; }
static bool folder(const char *path) {
    FRESULT r=f_mkdir(path);
    return r==FR_OK || r==FR_EXIST;
}
static bool write_bytes(FIL *f,const void *data,unsigned bytes) {
    UINT n=0;
    return f_write(f,data,bytes,&n)==FR_OK && n==bytes;
}
static bool read_bytes(FIL *f,void *data,unsigned bytes) {
    UINT n=0;
    return f_read(f,data,bytes,&n)==FR_OK && n==bytes;
}
static bool seek_to(FIL *f,unsigned offset) {
    return f_lseek(f,offset)==FR_OK && f_tell(f)==offset;
}
static bool flush(struct kui_media_report_writer *w) {
    if(!w->used) return true;
    if(kui_games_storage_open(w->log)!=KUI_GAMES_STORAGE_OK) return false;
    FIL f;
    bool ok=w->written || (folder("0:/KUI") && folder("0:/KUI/media"));
    if(ok && f_open(&f,REPORT_TEMP,FA_WRITE|(w->written?FA_OPEN_EXISTING:FA_CREATE_ALWAYS))==FR_OK) {
        ok=f_size(&f)==w->written && seek_to(&f,w->written) && write_bytes(&f,w->buffer,w->used);
        if(f_close(&f)!=FR_OK) ok=false;
    } else ok=false;
    if(!kui_games_storage_done(ok)) ok=false;
    if(ok) { w->written+=w->used; w->used=0; }
    return ok;
}
static void append(struct kui_media_report_writer *w,const char *data,unsigned bytes) {
    if(w->failed) return;
    if(bytes>sizeof(w->buffer) || (sizeof(w->buffer)-w->used<bytes && !flush(w))) {
        w->failed=true;
        return;
    }
    memcpy(w->buffer+w->used,data,bytes); w->used+=bytes;
}

struct kui_media_report_writer *kui_media_report_begin(bool all,unsigned kinds,kui_log_fn log) {
    last_saved=false;
    struct kui_media_report_writer *w=calloc(1,sizeof(*w));
    if(w) {
        memset(w->buffer,' ',HEADER_SIZE);
        w->used=HEADER_SIZE;
        w->all=all;w->kinds=kinds;w->log=log;
        append(w,HEADINGS,sizeof(HEADINGS)-1u);
    }
    return w;
}

/* A title supplied by a disc cannot inject a TSV column or an extra entry. */
static void field(struct kui_media_report_writer *w,const char *s,size_t cap,char separator) {
    char clean[KUI_GAMES_FILE_CAP];
    unsigned n=0;
    while(n+1u<cap && s[n]) {
        unsigned char c=(unsigned char)s[n];
        clean[n++]=(c<32u || c==127u)?' ':(char)c;
    }
    append(w,clean,n);
    append(w,&separator,1);
}
void kui_media_report_add(struct kui_media_report_writer *w,const struct kui_media_report_entry *e) {
    if(!w || !e || w->failed) return;
    if(!(w->count%KUI_MEDIA_REPORT_ROWS)) {
        char marker[64];
        unsigned offset=w->written+w->used;
        int n=snprintf(marker,sizeof(marker),"# Page %u\tPrevious %u\n",w->count/KUI_MEDIA_REPORT_ROWS+1u,w->previous);
        append(w,marker,(unsigned)n);
        w->previous=w->count?offset:0;
    }
    field(w,e->category,sizeof(e->category),'\t');
    field(w,e->asset,sizeof(e->asset),'\t');
    field(w,e->title,sizeof(e->title),'\t');
    field(w,e->product,sizeof(e->product),'\t');
    field(w,e->path,sizeof(e->path),'\t');
    field(w,e->reason,sizeof(e->reason),'\n');
    ++w->count;
}

static void trim(char *s) {
    size_t n=strlen(s);
    while(n && s[n-1]==' ') s[--n]=0;
}
static bool header(FIL *f,char status[32],unsigned *count,char scope[32],char kinds[32]) {
    char h[HEADER_SIZE+1u]; unsigned bytes=0;
    if(!seek_to(f,0) || !read_bytes(f,h,HEADER_SIZE) || h[HEADER_SIZE-1u]!='\n') return false;
    h[HEADER_SIZE-1u]=0;
    if(sscanf(h,"K-UI media scan v1 | %31[^|]| Entries %9u | Bytes %9u | %31[^|]| %31[^|]",
              status,count,&bytes,scope,kinds)!=5 ||
       f_size(f)!=bytes || bytes<FIRST_PAGE || *count>bytes/8u) return false;
    trim(status);trim(scope);trim(kinds);
    return status[0] && scope[0] && kinds[0];
}
static bool valid_last(bool *present) {
    FIL f;
    FRESULT r=f_open(&f,KUI_MEDIA_REPORT_PATH,FA_READ);
    if(absent(r)) { *present=false;return true; }
    if(r!=FR_OK) return false;
    *present=true;
    char status[32],scope[32],kinds[32]; unsigned n;
    bool valid=header(&f,status,&n,scope,kinds);
    bool io=f_error(&f)!=0;
    if(f_close(&f)!=FR_OK || io) return false;
    if(!valid) {
        if(f_unlink(KUI_MEDIA_REPORT_PATH)!=FR_OK) return false;
        *present=false;
    }
    return true;
}
static bool publish_file(void) {
    bool present=false;
    if(!valid_last(&present)) return false;
    if(present) {
        FRESULT r=f_unlink(REPORT_BACKUP);
        if((r!=FR_OK && !absent(r)) || f_rename(KUI_MEDIA_REPORT_PATH,REPORT_BACKUP)!=FR_OK) return false;
    }
    if(f_rename(REPORT_TEMP,KUI_MEDIA_REPORT_PATH)==FR_OK) return true;
    if(present) (void)f_rename(REPORT_BACKUP,KUI_MEDIA_REPORT_PATH);
    return false;
}
bool kui_media_report_finish(struct kui_media_report_writer *w,const char *status,bool publish) {
    last_saved=false;
    if(!w) return false;
    bool ok=false;
    if(publish && !w->failed && flush(w) && kui_games_storage_open(w->log)==KUI_GAMES_STORAGE_OK) {
        char h[HEADER_SIZE];
        int len=snprintf(h,sizeof(h),"K-UI media scan v1 | %.28s | Entries %u | Bytes %u | %s |%s%s%s",
            status,w->count,w->written,w->all?"All games":"Selected game or set",
            w->kinds&1?" Artwork":"",w->kinds&2?" Manual":"",w->kinds&4?" Preview":"");
        FIL f;
        if(len>0 && (size_t)len<sizeof(h) && f_open(&f,REPORT_TEMP,FA_WRITE|FA_READ)==FR_OK) {
            memset(h+len,' ',sizeof(h)-(size_t)len);
            h[sizeof(h)-1u]='\n';
            char check[32],scope[32],kinds[32]; unsigned count;
            ok=f_size(&f)==w->written && write_bytes(&f,h,sizeof(h)) && f_sync(&f)==FR_OK &&
               header(&f,check,&count,scope,kinds) && count==w->count;
            if(f_close(&f)!=FR_OK) ok=false;
            if(ok) ok=publish_file();
        }
        if(!kui_games_storage_release()) ok=false;
    }
    free(w);
    last_saved=ok;
    return ok;
}

struct reader { FIL f; char buf[512]; unsigned pos,used,at; };
/* Read only the requested page and at most one sector of its successor. */
static bool line(struct reader *r,char *out,size_t cap) {
    size_t n=0;
    while(n+1<cap) {
        if(r->pos==r->used) {
            UINT got=0;
            if(f_read(&r->f,r->buf,sizeof(r->buf),&got)!=FR_OK || !got) return false;
            r->used=got;r->pos=0;
        }
        char c=r->buf[r->pos++]; ++r->at;
        if(c=='\n') { out[n]=0;return true; }
        if(!c || ((unsigned char)c<32u && c!='\t')) return false;
        out[n++]=c;
    }
    return false;
}
static bool parse_entry(char *text,struct kui_media_report_entry *e) {
    char *fields[]={e->category,e->asset,e->title,e->product,e->path,e->reason};
    const size_t caps[]={sizeof(e->category),sizeof(e->asset),sizeof(e->title),sizeof(e->product),sizeof(e->path),sizeof(e->reason)};
    for(unsigned i=0;i<6;i++) {
        char *next=strchr(text,'\t');
        if((next!=NULL)!=(i<5)) return false;
        if(next)*next++=0;
        if(strlen(text)>=caps[i]) return false;
        strcpy(fields[i],text);text=next;
    }
    return e->category[0] && e->title[0];
}
static bool read_page(struct reader *r,unsigned cursor,struct kui_media_report_page *out) {
    if(!out->total) return cursor==0 && f_size(&r->f)==FIRST_PAGE;
    unsigned at=cursor?cursor:FIRST_PAGE;
    if(at<FIRST_PAGE || at>=f_size(&r->f) || !seek_to(&r->f,at)) return false;
    r->at=at;r->used=r->pos=0;
    char text[ENTRY_BYTES]; unsigned page=0,prev=0;
    if(!line(r,text,sizeof(text)) || sscanf(text,"# Page %9u\tPrevious %9u",&page,&prev)!=2 ||
       !page || page>(out->total-1u)/KUI_MEDIA_REPORT_ROWS+1u || (at==FIRST_PAGE)!=(page==1) ||
       (page<=2 && prev) || (page>2 && (prev<=FIRST_PAGE || prev>=at))) return false;
    out->first=(page-1u)*KUI_MEDIA_REPORT_ROWS;out->previous=prev;
    for(unsigned i=out->first;i<out->total && out->count<KUI_MEDIA_REPORT_ROWS;i++) {
        if(!line(r,text,sizeof(text)) || !parse_entry(text,&out->entries[out->count])) return false;
        ++out->count;
    }
    if(out->first+out->count<out->total) { out->next=r->at;return r->at<f_size(&r->f); }
    return r->at==f_size(&r->f);
}
enum kui_media_report_result kui_media_report_read_page(unsigned cursor,struct kui_media_report_page *out,kui_log_fn log) {
    if(!out) return KUI_MEDIA_REPORT_ERROR;
    memset(out,0,sizeof(*out));
    if(kui_games_storage_open(log)!=KUI_GAMES_STORAGE_OK) return KUI_MEDIA_REPORT_ERROR;
    struct reader r;
    enum kui_media_report_result state=KUI_MEDIA_REPORT_ERROR;
    /* Cursor offsets point into one file. A later page from the current scan
     * must never silently switch to an older report with different offsets. */
    bool backup_cursor=(cursor & BACKUP_CURSOR)!=0;
    bool can_fallback=!cursor;
    cursor &= ~BACKUP_CURSOR;
    bool damaged=false;
    for(unsigned slot=backup_cursor?1u:0u;slot<2;slot++) {
        if(slot && !backup_cursor && !can_fallback) break;
        FRESULT result=f_open(&r.f,slot?REPORT_BACKUP:KUI_MEDIA_REPORT_PATH,FA_READ);
        if(absent(result)) {
            if(slot && !damaged) state=KUI_MEDIA_REPORT_NONE;
            continue;
        }
        if(result!=FR_OK) break;
        bool valid=header(&r.f,out->status,&out->total,out->scope,out->kinds);
        bool parsed=valid && read_page(&r,cursor,out);
        bool io=f_error(&r.f)!=0;
        if(f_close(&r.f)!=FR_OK || io) break;
        if(!valid) { damaged=true;continue; }
        if(parsed) {
            state=KUI_MEDIA_REPORT_OK;
            if(slot) {
                if(out->next) out->next |= BACKUP_CURSOR;
                if(out->first) out->previous |= BACKUP_CURSOR;
                char prior[32];snprintf(prior,sizeof(prior),"Previous: %.21s",out->status);strcpy(out->status,prior);
            }
        }
        break;
    }
    if(!kui_games_storage_done(state!=KUI_MEDIA_REPORT_ERROR)) state=KUI_MEDIA_REPORT_ERROR;
    if(state!=KUI_MEDIA_REPORT_OK) memset(out,0,sizeof(*out));
    return state;
}
