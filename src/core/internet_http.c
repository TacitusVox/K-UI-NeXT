/* SPDX-License-Identifier: GPL-3.0-only */
#include "internet_internal.h"
#include "kui/media_tls.h"
#include "kui/hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define IO_BYTES 4096u
#define HEADER_LIMIT 32768u
#define CONNECT_MS 30000u
#define IDLE_MS 90000u

struct conditional {
    const struct kui_http_validators *known;
    struct kui_http_validators response;
    bool sent_etag,sent_date,retry_plain;
};
struct transfer {
    struct kui_internet *session;
    struct conditional *check;
    kui_media_tls_t *tls;
    kui_http_write_fn write;
    void *destination;
    kui_http_progress_fn progress;
    void *progress_ctx;
    uint32_t limit, total, received, start, asset_size;
    uint64_t active, last_progress;
    size_t at, end;
    uint8_t input[IO_BYTES];
    char line[IO_BYTES], url[KUI_HTTP_URL], target[KUI_HTTP_URL], host[KUI_HTTP_HOST];
    bool socket_open, finished, retried;
    unsigned retry_ms;
    bool (*complete)(void *);
    int eof;
};

const char *kui_internet_adapter(const struct kui_internet *s) { return s->adapter; }
const char *kui_internet_problem(const struct kui_internet *s) { return s->problem; }

static bool fail(struct kui_internet *s,const char *message) {
    if(!s->problem[0]) snprintf(s->problem,sizeof(s->problem),"%s",message);
    return false;
}
static bool stopped(struct kui_internet *s) { return s->cancel && s->cancel(); }
static bool step(struct transfer *t) {
    struct kui_internet *s=t->session;
    if(stopped(s)) return fail(s,"Scan cancelled");
    uint64_t now=s->now();
    if(now-t->active>IDLE_MS)
        return fail(s,"Website timed out during transfer");
    return s->poll(s) || fail(s,"Network connection lost");
}
static bool hostname(const char *p,size_t n) {
    if(!n || n>=KUI_HTTP_HOST) return false;
    size_t label=0;
    for(size_t i=0;i<n;i++) {
        unsigned c=(unsigned char)p[i];
        if(c=='.') {
            if(!label || p[i-1]=='-') return false;
            label=0;
        } else {
            if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || (c=='-'&&label)))
                return false;
            if(++label>63) return false;
        }
    }
    return label && p[n-1]!='-';
}
static bool split_url(struct transfer *t) {
    if(strncasecmp(t->url,"https://",8)) return false;
    for(const unsigned char *c=(const unsigned char*)t->url;*c;c++)
        if(*c<33 || *c>=127 || *c=='#' || *c=='\\') return false;
    const char *p=t->url+8;
    size_t n=strcspn(p,"/?");
    size_t h=n;
    if(n>4 && !memcmp(p+n-4,":443",4)) h-=4;
    if(!hostname(p,h)) return false;
    memcpy(t->host,p,h);t->host[h]=0;
    p+=n;
    snprintf(t->target,sizeof(t->target),"%s%s",*p=='/'?"":"/",p);
    return true;
}
static bool redirect(struct transfer *t,const char *value) {
    if(!*value) return false;
    int n;
    if(!strncasecmp(value,"https://",8))
        n=snprintf(t->url,sizeof(t->url),"%s",value);
    else if(!strncmp(value,"//",2))
        n=snprintf(t->url,sizeof(t->url),"https:%s",value);
    else if(*value=='/')
        n=snprintf(t->url,sizeof(t->url),"https://%s%s",t->host,value);
    else {
        size_t first=strcspn(value,"/?");
        if(memchr(value,':',first)) return false;
        char *query=strchr(t->target,'?');
        if(query) *query=0;
        if(*value!='?') strrchr(t->target,'/')[1]=0;
        n=snprintf(t->url,sizeof(t->url),"https://%s%s%s",t->host,t->target,value);
    }
    return n>0 && (size_t)n<sizeof(t->url);
}
static bool close_socket(struct transfer *t) {
    if(!t->socket_open) return true;
    t->socket_open=false;
    struct kui_internet *s=t->session;
    return s->net.close(s->net.ctx,0) || fail(s,"Could not release network socket");
}
static enum kui_http_result connect_to(struct transfer *t,const uint8_t ip[4]) {
    struct kui_internet *s=t->session;
    const struct kui_net_sockets *net=&s->net;
    unsigned local=49152u+(s->next_port++&16383u);
    t->socket_open=true;
    if(!net->connect(net->ctx,0,(uint16_t)local,ip,443)) {
        fail(s,"Cannot open website connection");
        return KUI_HTTP_ERROR;
    }
    uint64_t start=s->now();
    while(step(t)) {
        uint8_t state;
        if(!net->state(net->ctx,0,&state)) {
            fail(s,"Cannot read network socket state");
            return KUI_HTTP_ERROR;
        }
        if(state==KUI_NET_ESTABLISHED) return KUI_HTTP_OK;
        if((state!=KUI_NET_CONNECTING && state!=KUI_NET_INIT) || s->now()-start>=CONNECT_MS)
            return KUI_HTTP_CONNECT_FAILED;
        s->pause(2);
    }
    return KUI_HTTP_ERROR;
}
/* Retry only before creating TLS or writing any response bytes. Requery DNS
 * so hosts with multiple A records can be reached through another address. */
static enum kui_http_result connect_host(struct transfer *t) {
    struct kui_internet *s=t->session;
    uint8_t ip[4],previous[4];
    for(unsigned attempt=0;;attempt++) {
        if(!s->resolve(s,t->host,ip,attempt ? previous : NULL)) {
            if(!s->problem[0]) snprintf(s->problem,sizeof(s->problem),"DNS lookup failed: %.100s",t->host);
            return KUI_HTTP_ERROR;
        }
        t->active=s->now();
        enum kui_http_result r=connect_to(t,ip);
        if(r!=KUI_HTTP_CONNECT_FAILED) return r;
        if(attempt) {
            snprintf(s->problem,sizeof(s->problem),"Cannot connect: %.100s:443",t->host);
            return r;
        }
        if(!close_socket(t)) return KUI_HTTP_ERROR;
        memcpy(previous,ip,4);
        if(s->log) s->log("HTTPS: retrying %s:443 after connection to %u.%u.%u.%u failed",
                          t->host,ip[0],ip[1],ip[2],ip[3]);
        uint64_t start=s->now();
        while(s->now()-start<500) {
            if(!step(t)) return KUI_HTTP_ERROR;
            s->pause(25);
        }
        if(!step(t)) return KUI_HTTP_ERROR;
    }
}
static int receive_wire(void *ctx,void *bytes,size_t count) {
    struct transfer *t=ctx;
    const struct kui_net_sockets *net=&t->session->net;
    size_t ready;
    uint8_t state;
    if(!net->state(net->ctx,0,&state) || !net->received(net->ctx,0,&ready)) return -1;
    if(!ready) return state==KUI_NET_ESTABLISHED ? 0 : -2;
    if(count>ready) count=ready;
    if(!net->receive(net->ctx,0,bytes,count)) return -1;
    t->active=t->session->now();
    return (int)count;
}
static int send_wire(void *ctx,const void *bytes,size_t count) {
    struct transfer *t=ctx;
    const struct kui_net_sockets *net=&t->session->net;
    size_t room;
    if(!net->room(net->ctx,0,&room)) return -1;
    if(!room) return 0;
    if(count>room) count=room;
    if(!net->send(net->ctx,0,bytes,count)) return -1;
    t->active=t->session->now();
    return (int)count;
}
static bool pump(struct transfer *t) {
    if(!step(t)) return false;
    int r=kui_media_tls_pump(t->tls,t,receive_wire,send_wire);
    if(r<0) {
        t->eof=r;
        if(r==-1) fail(t->session,kui_media_tls_error(t->tls));
        return false;
    }
    if(!r) t->session->pause(2);
    return true;
}
/* An authenticated TLS close is zero. An unclean EOF is an error. */
static int fill(struct transfer *t) {
    if(t->at<t->end) return 1;
    while(!t->eof && !stopped(t->session)) {
        int n=kui_media_tls_read(t->tls,t->input,sizeof(t->input));
        if(n>0) {t->at=0;t->end=(size_t)n;return 1;}
        if(n<0 || !pump(t)) break;
    }
    return t->eof==-2 ? 0 : -1;
}
static bool line(struct transfer *t,size_t *budget) {
    size_t n=0;
    while(*budget && n+1<sizeof(t->line)) {
        if(fill(t)<=0) return false;
        unsigned c=t->input[t->at++];
        --*budget;
        if(c=='\r') {
            if(!*budget || fill(t)<=0 || t->input[t->at++]!='\n') return false;
            --*budget;
            t->line[n]=0;
            return true;
        }
        if((c<32 && c!='\t') || c==127) return false;
        t->line[n++]=(char)c;
    }
    return false;
}
static bool number(const char *p,unsigned base,uint32_t *out) {
    if(!*p) return false;
    uint32_t value=0;
    do {
        unsigned c=(unsigned char)*p++, digit;
        if(c>='0' && c<='9') digit=c-'0';
        else if(c>='a' && c<='f') digit=c-'a'+10;
        else if(c>='A' && c<='F') digit=c-'A'+10;
        else return false;
        if(digit>=base || value>(UINT32_MAX-digit)/base) return false;
        value=value*base+digit;
    } while(*p);
    *out=value;
    return true;
}
static enum kui_http_result body(struct transfer *t,uint32_t count,bool until_eof) {
    while(count || until_eof) {
        if(stopped(t->session)) return KUI_HTTP_STOPPED;
        int r=fill(t);
        if(!r && until_eof) break;
        if(r<=0) return KUI_HTTP_ERROR;
        size_t n=t->end-t->at;
        if(!until_eof && n>count) n=count;
        if(n>t->limit-t->received) {
            if(!t->complete || t->received==t->limit) return KUI_HTTP_TOO_LARGE;
            n=t->limit-t->received;
        }
        if(!t->write(t->destination,t->input+t->at,n)) {
            fail(t->session,"Cannot save downloaded media to storage");
            return KUI_HTTP_ERROR;
        }
        t->at+=n;t->received+=(uint32_t)n;
        if(!until_eof) count-=(uint32_t)n;
        if(t->complete && t->complete(t->destination)) {
            t->finished=true;
            return KUI_HTTP_OK;
        }
        if(t->received==t->limit && count) return KUI_HTTP_TOO_LARGE;
        uint64_t now=t->session->now();
        if(t->progress && now-t->last_progress>=250) {
            t->progress(t->progress_ctx,t->received,t->total);
            t->last_progress=now;
        }
    }
    return KUI_HTTP_OK;
}
static enum kui_http_result chunks(struct transfer *t) {
    for(;;) {
        size_t budget=512;
        if(!line(t,&budget)) return KUI_HTTP_ERROR;
        char *extension=strchr(t->line,';');
        if(extension) *extension=0;
        uint32_t bytes;
        if(!number(t->line,16,&bytes)) return KUI_HTTP_ERROR;
        if(!bytes) break;
        if(!t->complete && bytes>t->limit-t->received) return KUI_HTTP_TOO_LARGE;
        enum kui_http_result r=body(t,bytes,false);
        if(r!=KUI_HTTP_OK || t->finished) return r;
        budget=2;
        if(!line(t,&budget) || *t->line) return KUI_HTTP_ERROR;
    }
    size_t trailers=HEADER_LIMIT;
    do {if(!line(t,&trailers)) return KUI_HTTP_ERROR;} while(*t->line);
    return KUI_HTTP_OK;
}
static bool valid_etag(const char *value,size_t n) {
    if(n>=2 && value[0]=='W' && value[1]=='/') {value+=2;n-=2;}
    if(n<2 || value[0]!='"' || value[n-1]!='"') return false;
    for(size_t i=1;i<n-1;i++) {
        unsigned char c=(unsigned char)value[i];
        if(c<33 || c=='"' || c>126) return false;
    }
    return true;
}
static bool valid_date(const char *v,size_t n) {
    /* IMF-fixdate only. Other date forms leave the next GET unconditional. */
    if(n!=29 || memcmp(v+3,", ",2) || v[7]!=' ' || v[11]!=' ' || v[16]!=' ' ||
       v[19]!=':' || v[22]!=':' || memcmp(v+25," GMT",4)) return false;
    static const char days[][4]={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    static const char months[][4]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static const unsigned char digits[]={5,6,12,13,14,15,17,18,20,21,23,24};
    for(size_t i=0;i<sizeof(digits);i++) if(v[digits[i]]<'0' || v[digits[i]]>'9') return false;
    bool weekday=false;unsigned month=0;
    for(unsigned i=0;i<7;i++) if(!memcmp(v,days[i],3)) weekday=true;
    for(unsigned i=0;i<12;i++) if(!memcmp(v+8,months[i],3)) month=i+1;
    if(!month || !weekday) return false;
    unsigned day=(unsigned)(v[5]-'0')*10u+(unsigned)(v[6]-'0');
    unsigned year=(unsigned)(v[12]-'0')*1000u+(unsigned)(v[13]-'0')*100u+
                  (unsigned)(v[14]-'0')*10u+(unsigned)(v[15]-'0');
    static const unsigned days_in_month[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap=month==2 && year%4==0 && (year%100!=0 || year%400==0);
    return day && year && day<=days_in_month[month]+leap && memcmp(v+17,"23",2)<=0 &&
           memcmp(v+20,"59",2)<=0 && memcmp(v+23,"60",2)<=0;
}
static bool stored_validator(const char *s,size_t cap,bool date) {
    const char *end=memchr(s,0,cap);
    return end && (date?valid_date(s,(size_t)(end-s)):valid_etag(s,(size_t)(end-s)));
}
static void conditional_header(struct transfer *t,char *out,size_t cap) {
    struct conditional *c=t->check;
    if(!c) return;
    memset(&c->response,0,sizeof(c->response));
    struct kui_sha256 sha;
    kui_sha256_init(&sha);kui_sha256_update(&sha,t->url,strlen(t->url));kui_sha256_digest(&sha,c->response.resource);
    c->sent_etag=c->sent_date=false;
    const struct kui_http_validators *v=c->known;
    if(!v || memcmp(v->resource,c->response.resource,sizeof(v->resource))) return;
    if(stored_validator(v->etag,sizeof(v->etag),false)) {
        c->sent_etag=true;
        snprintf(out,cap,"If-None-Match: %s\r\n",v->etag);
    } else if(stored_validator(v->modified,sizeof(v->modified),true)) {
        c->sent_date=true;
        snprintf(out,cap,"If-Modified-Since: %s\r\n",v->modified);
    }
}
static enum kui_http_result unchanged(struct conditional *c,bool saw_tag,bool saw_date) {
    const struct kui_http_validators *known=c->known;
    struct kui_http_validators *v=&c->response;
    /* If-None-Match uses weak comparison, so a provider may change the W/
     * marker. A conflicting or malformed echoed condition merits a plain
     * GET before retaining cached bytes. */
    const char *prior=known->etag,*current=v->etag;
    if(c->sent_etag && prior[0]=='W') prior+=2;
    if(current[0]=='W') current+=2;
    if((c->sent_etag && saw_tag && (!v->etag[0] || strcmp(prior,current))) ||
       (c->sent_date && saw_date && strcmp(known->modified,v->modified))) {
        c->retry_plain=true;c->known=NULL;
        return KUI_HTTP_OK;
    }
    if(!saw_tag && stored_validator(known->etag,sizeof(known->etag),false)) strcpy(v->etag,known->etag);
    if(!saw_date && stored_validator(known->modified,sizeof(known->modified),true)) strcpy(v->modified,known->modified);
    return KUI_HTTP_NOT_MODIFIED;
}
static enum kui_http_result response(struct transfer *t,bool *again) {
    size_t budget=HEADER_LIMIT;
    if(!line(t,&budget) || (strncmp(t->line,"HTTP/1.1 ",9) && strncmp(t->line,"HTTP/1.0 ",9)))
        return KUI_HTTP_ERROR;
    const char *p=t->line+9;
    if(strlen(p)<3 || (p[3] && p[3]!=' ')) return KUI_HTTP_ERROR;
    unsigned code=0;
    for(unsigned i=0;i<3;i++) {
        if(p[i]<'0' || p[i]>'9') return KUI_HTTP_ERROR;
        code=code*10u+(unsigned)(p[i]-'0');
    }
    bool follow=code==301 || code==302 || code==303 || code==307 || code==308;
    /* Retry only an explicit temporary response, before any payload reaches
     * the sink. A Retry-After outside our interactive wait budget takes
     * precedence, including absolute dates we cannot safely interpret here. */
    bool temporary=!t->retried && (code==408 || code==502 || code==503 || code==504);
    unsigned wait_ms=1000;
    bool retry_header=false;
    if(t->start && code==200) return KUI_HTTP_RANGE_IGNORED;
    struct conditional *check=t->check;
    bool conditional_sent=check && (check->sent_etag || check->sent_date);
    bool not_modified=code==304 && conditional_sent;
    if(code==412 && conditional_sent) {
        check->retry_plain=true;check->known=NULL;
        return KUI_HTTP_OK;
    }
    unsigned expected=t->start ? 206u : 200u;
    if(code!=expected && !follow && !temporary && !not_modified) {
        snprintf(t->session->problem,sizeof(t->session->problem),"Website returned HTTP %u",code);
        return code==404 || code==410 ? KUI_HTTP_MISSING : code==429 ? KUI_HTTP_LIMITED : KUI_HTTP_ERROR;
    }
    bool located=false,length=false,chunked=false,range=false,saw_tag=false,saw_date=false;
    while(line(t,&budget)) {
        if(!*t->line) {
            if(temporary) {
                if(!wait_ms) {
                    snprintf(t->session->problem,sizeof(t->session->problem),"Website returned HTTP %u. Retry later.",code);
                    return KUI_HTTP_ERROR;
                }
                t->retry_ms=wait_ms;
                return KUI_HTTP_OK;
            }
            if(follow) {
                if(!located) return KUI_HTTP_ERROR;
                *again=true;
                return KUI_HTTP_OK;
            }
            if(not_modified) return unchanged(check,saw_tag,saw_date);
            if(chunked && length) return KUI_HTTP_ERROR;
            if((t->start && !range) || (t->asset_size && length && t->total!=t->limit)) {
                fail(t->session,"Website returned the wrong download length or range");
                return KUI_HTTP_ERROR;
            }
            if(!t->complete && t->total>t->limit) return KUI_HTTP_TOO_LARGE;
            /* Some CDNs ignore the condition yet return the very same strong
             * representation tag. Close at the headers; a date alone can
             * coincide for two different images in the same second. */
            if(code==200 && check && check->sent_etag && check->response.etag[0]=='"' &&
               (!length || t->total) && !strcmp(check->known->etag,check->response.etag))
                return unchanged(check,saw_tag,saw_date);
            if(t->progress) t->progress(t->progress_ctx,0,t->total);
            return chunked ? chunks(t) : body(t,t->total,!length);
        }
        char *value=strchr(t->line,':');
        if(!value || value==t->line) return KUI_HTTP_ERROR;
        for(char *c=t->line;c<value;c++) if((unsigned char)*c<=32 || (unsigned char)*c>=127) return KUI_HTTP_ERROR;
        *value++=0;
        while(*value==' ' || *value=='\t') value++;
        char *end=value+strlen(value);
        while(end>value && (end[-1]==' ' || end[-1]=='\t')) *--end=0;
        if(temporary) {
            if(!strcasecmp(t->line,"Retry-After")) {
                uint32_t seconds;
                if(retry_header || !number(value,10,&seconds) || seconds>5) wait_ms=0;
                else wait_ms=seconds ? seconds*1000u : 1u;
                retry_header=true;
            }
        } else if(follow) {
            if(!strcasecmp(t->line,"Location")) {
                if(located || !redirect(t,value)) return KUI_HTTP_ERROR;
                located=true;
            }
        } else if(check && !strcasecmp(t->line,"ETag")) {
            size_t n=strlen(value);
            if(!saw_tag && n<sizeof(check->response.etag) && valid_etag(value,n))
                memcpy(check->response.etag,value,n+1);
            else memset(check->response.etag,0,sizeof(check->response.etag));
            saw_tag=true;
        } else if(check && !strcasecmp(t->line,"Last-Modified")) {
            size_t n=strlen(value);
            if(!saw_date && valid_date(value,n)) memcpy(check->response.modified,value,n+1);
            else memset(check->response.modified,0,sizeof(check->response.modified));
            saw_date=true;
        } else if(!strcasecmp(t->line,"Content-Range")) {
            char expected_range[64];
            if(!t->start || range) return KUI_HTTP_ERROR;
            snprintf(expected_range,sizeof(expected_range),"bytes %lu-%lu/%lu",
                (unsigned long)t->start,(unsigned long)t->asset_size-1u,(unsigned long)t->asset_size);
            if(strcmp(value,expected_range)) return KUI_HTTP_ERROR;
            range=true;
        } else if(!strcasecmp(t->line,"Content-Length")) {
            if(length || !number(value,10,&t->total)) return KUI_HTTP_ERROR;
            length=true;
        } else if(!strcasecmp(t->line,"Transfer-Encoding")) {
            if(chunked || strcasecmp(value,"chunked")) return KUI_HTTP_ERROR;
            chunked=true;
        } else if(!strcasecmp(t->line,"Content-Encoding") && strcasecmp(value,"identity")) return KUI_HTTP_ERROR;
    }
    return KUI_HTTP_ERROR;
}
static enum kui_http_result request(struct transfer *t,bool *again) {
    struct kui_internet *s=t->session;
    if(!split_url(t)) {fail(s,"Provider returned an unsupported HTTPS URL");return KUI_HTTP_ERROR;}
    t->at=t->end=t->eof=t->total=0;
    enum kui_http_result connected=connect_host(t);
    if(connected!=KUI_HTTP_OK) return connected;
    t->tls=kui_media_tls_open(t->host);
    if(!t->tls) {fail(s,"TLS setup failed; check console date and free memory");return KUI_HTTP_ERROR;}
    char condition[160]="",range_header[48]="";
    conditional_header(t,condition,sizeof(condition));
    if(t->start) snprintf(range_header,sizeof(range_header),"Range: bytes=%lu-\r\n",(unsigned long)t->start);
    int n=snprintf(t->line,sizeof(t->line),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: K-UI-Dreamcast/1\r\nAccept: */*\r\nAccept-Encoding: identity\r\n%s%sConnection: close\r\n\r\n",
        t->target,t->host,range_header,condition);
    if(n<=0 || (size_t)n>=sizeof(t->line)) return KUI_HTTP_ERROR;
    size_t sent=0;
    while(sent<(size_t)n) {
        int wrote=kui_media_tls_write(t->tls,t->line+sent,(size_t)n-sent);
        if(wrote<0 || (!wrote && !pump(t))) return KUI_HTTP_ERROR;
        sent+=(size_t)wrote;
    }
    return response(t,again);
}
static enum kui_http_result get(struct kui_internet *s,const char *url,uint32_t limit,uint32_t start,uint32_t asset_size,
        kui_http_write_fn write,void *dest,kui_http_progress_fn progress,void *progress_ctx,bool (*complete)(void *),struct conditional *check) {
    if(!s) return KUI_HTTP_ERROR;
    s->problem[0]=0;
    if(stopped(s)) return KUI_HTTP_STOPPED;
    if(!url || strlen(url)>=KUI_HTTP_URL || !write || !limit) {
        fail(s,"Invalid website request");
        return KUI_HTTP_ERROR;
    }
    struct transfer *t=calloc(1,sizeof(*t));
    if(!t) {fail(s,"Insufficient memory for HTTPS");return KUI_HTTP_ERROR;}
    t->start=start;t->asset_size=asset_size;
    t->session=s;t->check=check;t->write=write;t->destination=dest;t->limit=limit;t->complete=complete;
    t->progress=progress;t->progress_ctx=progress_ctx;
    strcpy(t->url,url);
    enum kui_http_result result=KUI_HTTP_ERROR;
    for(unsigned redirects=0;;) {
        bool again=false;
        result=request(t,&again);
        if(t->tls) {kui_media_tls_free(t->tls);t->tls=NULL;}
        if(!close_socket(t)) result=KUI_HTTP_ERROR;
        if(result!=KUI_HTTP_OK) break;
        if(t->retry_ms) {
            t->retried=true;
            if(s->log) s->log("Website temporarily unavailable. Retrying the same file.");
            uint64_t start=s->now();
            while(s->now()-start<t->retry_ms) {
                if(!step(t)) {result=KUI_HTTP_ERROR;break;}
                s->pause(25);
            }
            t->retry_ms=0;
            if(result!=KUI_HTTP_OK) break;
            continue;
        }
        if(check && check->retry_plain) {check->retry_plain=false;continue;}
        if(!again) break;
        if(++redirects>4) {fail(s,"Too many website redirects");result=KUI_HTTP_ERROR;break;}
    }
    if(result==KUI_HTTP_OK && asset_size && t->received!=limit) {
        fail(s,"Incomplete download");
        result=KUI_HTTP_ERROR;
    }
    if(stopped(s)) result=KUI_HTTP_STOPPED;
    if(result==KUI_HTTP_OK && progress) progress(progress_ctx,t->received,t->total);
    if(result==KUI_HTTP_TOO_LARGE) fail(s,"Website file exceeds the download size limit");
    if(result==KUI_HTTP_ERROR) fail(s,"Incomplete or unsupported website response");
    if(s->problem[0] && s->log) s->log("Internet scan: %s: %s",t->host,s->problem);
    free(t);
    return result;
}
enum kui_http_result kui_internet_get_prefix(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn write,void *dest,kui_http_progress_fn progress,void *progress_ctx,bool (*complete)(void *)) {
    return get(s,url,limit,0,0,write,dest,progress,progress_ctx,complete,NULL);
}
enum kui_http_result kui_internet_get_remainder(struct kui_internet *s,const char *url,uint32_t start,uint32_t size,
        kui_http_write_fn write,void *dest,kui_http_progress_fn progress,void *progress_ctx) {
    if(start>=size) return KUI_HTTP_ERROR;
    return get(s,url,size-start,start,size,write,dest,progress,progress_ctx,NULL,NULL);
}
enum kui_http_result kui_internet_get(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn write,void *dest,kui_http_progress_fn progress,void *progress_ctx) {
    return kui_internet_get_prefix(s,url,limit,write,dest,progress,progress_ctx,NULL);
}
enum kui_http_result kui_internet_get_conditional(struct kui_internet *s,const char *url,uint32_t limit,
        kui_http_write_fn write,void *dest,kui_http_progress_fn progress,void *progress_ctx,
        const struct kui_http_validators *known,struct kui_http_validators *received) {
    struct conditional check={.known=known};
    enum kui_http_result r=get(s,url,limit,0,0,write,dest,progress,progress_ctx,NULL,&check);
    if(received && (r==KUI_HTTP_OK || r==KUI_HTTP_NOT_MODIFIED)) *received=check.response;
    return r;
}
