/* SPDX-License-Identifier: GPL-3.0-only */
#include "../core/internet_internal.h"
#include "kui/bba_sockets.h"
#include "kui/dreampi.h"
#include "kui/network_bba.h"
#include "kui/network_w5500.h"
#include "kui/network_wifi.h"
#include "platform.h"
#include <kos/net.h>
#include <kos/thread.h>
#include <kos/timer.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum link_kind { NONE, BBA, WIRED, WIFI, MODEM };
struct dns_cache { char name[KUI_HTTP_HOST]; uint8_t ip[4]; uint64_t expires; };
struct connection {
    struct kui_internet http; /* First member: public opaque session handle. */
    enum link_kind kind;
    const struct kui_bba_port *bba_port;
    struct kui_bba_session bba;
    struct kui_w5500_session w5500;
    struct kui_wifi_session *wifi;
    struct dns_cache names[4];
    unsigned replace;
    uint16_t dns_id;
    uint8_t ip[4];
};
static void pause_ms(unsigned ms) {thd_sleep(ms);}
static bool problem(struct kui_internet *s,const char *why) {
    snprintf(s->problem,sizeof(s->problem),"%.127s",why);
    return false;
}
static bool poll_link(struct kui_internet *s) {
    struct connection *c=(struct connection *)s;
    if(s->cancel && s->cancel()) return problem(s,"Scan cancelled");
    switch(c->kind) {
    case MODEM: return kui_dreampi_online() || problem(s,"DreamPi connection lost");
    case BBA: return c->bba_port->poll(&c->bba) || problem(s,c->bba.problem);
    case WIFI:
        if(!kui_wifi_session_step(c->wifi)) return problem(s,c->wifi->problem);
        return (c->wifi->host.wifi.state==KWM_WIFI_ONLINE && !memcmp(c->ip,c->wifi->host.wifi.ip,4)) ||
            problem(s,"Wi-Fi connection or address changed");
    case WIRED: {
        uint64_t now=s->now();
        if(!kui_w5500_session_expires_in(&c->w5500,now)) return problem(s,"W5500 address lease expired");
        if(!kui_w5500_session_renew_in(&c->w5500,now)) {
            bool refused=false;
            if(kui_w5500_session_renew(&c->w5500,1,2000,&refused,s->cancel)) {
                if(memcmp(c->ip,c->w5500.config.ip,4)) return problem(s,"W5500 address changed");
            } else if(refused) return problem(s,c->w5500.problem);
        }
        return true;
    }
    default: return false;
    }
}
static bool dns_udp(struct connection *c,const char *host,uint8_t ip[4],uint32_t *ttl,const uint8_t avoid[4]) {
    struct kui_internet *s=&c->http;
    const uint8_t *dns=c->kind==WIRED ? c->w5500.config.dns : net_default_dev ? net_default_dev->dns : NULL;
    if(!dns || !dns[0]) return problem(s,"Connection supplied no DNS resolver");
    const bool wired=c->kind==WIRED;
    uint8_t query[288],reply[1232];
    uint16_t id=++c->dns_id;
    size_t n=kui_dns_query(query,sizeof(query),id,host);
    if(!n) return problem(s,"Invalid website hostname");
    /* Advertise only the UDP payload that fits our receive buffer. */
    static const uint8_t opt[]={0,0,41,4,208,0,0,0,0,0,0};
    query[11]=1; memcpy(query+n,opt,sizeof(opt)); n+=sizeof(opt);
    int fd=-1;
    struct sockaddr_in server={.sin_family=AF_INET,.sin_port=htons(53)};
    memcpy(&server.sin_addr,dns,4);
    if(wired) {
        if(!kui_w5500_open(&c->w5500.chip,2,KUI_W5500_UDP,(uint16_t)(49152u+(s->next_port++&16383u))))
            return problem(s,"Cannot open W5500 DNS socket");
    } else {
        fd=socket(AF_INET,SOCK_DGRAM,0);
        if(fd<0) return problem(s,"Cannot open DNS socket");
    }
    bool ok=false;
    uint64_t start=s->now(),retry=start;
    while(s->now()-start<10000 && poll_link(s)) {
        uint64_t now=s->now();
        if(now>=retry) {
            bool sent=wired ? kui_w5500_datagram_send(&c->w5500.chip,2,dns,53,query,(uint16_t)n) :
                sendto(fd,query,n,0,(struct sockaddr *)&server,sizeof(server))==(ssize_t)n;
            if(!sent) break;
            retry=now+2300;
        }
        size_t got=0;
        uint16_t from_port=0;
        uint8_t from[4];
        if(wired) {
            if(!kui_w5500_datagram_receive(&c->w5500.chip,2,from,&from_port,reply,sizeof(reply),&got)) break;
        } else {
            struct sockaddr_in sender={0}; socklen_t size=sizeof(sender);
            ssize_t received=recvfrom(fd,reply,sizeof(reply),MSG_DONTWAIT,(struct sockaddr *)&sender,&size);
            if(received<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) break;
            if(received>0) {
                got=(size_t)received;
                from_port=ntohs(sender.sin_port); memcpy(from,&sender.sin_addr,4);
            }
        }
        if(got && from_port==53 && !memcmp(from,dns,4) && got>=12 && reply[0]==query[0] && reply[1]==query[1]) {
            ok=kui_dns_answer(reply,got,id,host,ip,ttl,avoid);
            break;
        }
        s->pause(4);
    }
    if(wired) (void)kui_w5500_close(&c->w5500.chip,2);
    else close(fd);
    return ok;
}
static bool resolve(struct kui_internet *s,const char *host,uint8_t ip[4],const uint8_t avoid[4]) {
    struct connection *c=(struct connection *)s;
    uint64_t now=s->now();
    for(unsigned i=0;i<4;i++) {
        if(c->names[i].expires>now && !strcasecmp(c->names[i].name,host)) {
            if(!avoid) {memcpy(ip,c->names[i].ip,4);return true;}
            c->names[i].expires=0;
        }
    }
    uint32_t ttl=30;
    if(c->kind==WIFI) {
        struct kwh *h=&c->wifi->host;
        uint32_t before=h->counts.dns;
        uint8_t tag=(uint8_t)++c->dns_id;
        if(!kwh_dns(h,tag,host)) return problem(s,"Wi-Fi DNS command failed");
        while(h->counts.dns==before || h->dns_tag!=tag) {
            if(s->now()-now>=10000 || !poll_link(s)) return false;
            s->pause(2);
        }
        if(h->dns_status) return false;
        memcpy(ip,h->dns_ip,4);
    } else if(!dns_udp(c,host,ip,&ttl,avoid)) return false;
    struct dns_cache *cached=&c->names[c->replace++%4];
    snprintf(cached->name,sizeof(cached->name),"%s",host);
    memcpy(cached->ip,ip,4);
    cached->expires=now+(uint64_t)(ttl<300?ttl:300)*1000;
    return true;
}
void kui_internet_close(struct kui_internet *s) {
    if(!s) return;
    struct connection *c=(struct connection *)s;
    if(c->kind==BBA) c->bba_port->end(&c->bba);
    if(c->kind==MODEM) {kui_bba_sockets_end(&s->net);kui_dreampi_close();}
    kui_w5500_session_end(&c->w5500);
    if(c->wifi) {kui_wifi_session_end(c->wifi);free(c->wifi);}
    free(c);
}
struct kui_internet *kui_internet_open(enum kui_internet_route route,kui_log_fn log,kui_cancel_fn cancel,char why[128]) {
    if(why) why[0]=0;
    struct connection *c=calloc(1,sizeof(*c));
    if(!c) {if(why) snprintf(why,128,"Insufficient memory for network");return NULL;}
    struct kui_internet *s=&c->http;
    s->log=log;s->cancel=cancel;s->now=timer_ms_gettime64;s->pause=pause_ms;s->poll=poll_link;s->resolve=resolve;
    s->next_port=(unsigned)timer_us_gettime64();c->dns_id=(uint16_t)s->next_port;
    if(cancel && cancel()) {problem(s,"Scan cancelled");goto failed;}
    if((unsigned)route>=KUI_INTERNET_ROUTE_COUNT) {problem(s,"Unknown internet adapter");goto failed;}
    if(route==KUI_INTERNET_DREAMPI) {
        if(kui_dreampi_open(log,cancel,s->problem)<=0) goto failed;
        c->kind=MODEM;
        if(!kui_bba_sockets_init(&s->net)) {problem(s,"Insufficient memory for PPP sockets");goto failed;}
        s->adapter="DreamPi dial-up";
        return s;
    }
    if(route==KUI_INTERNET_AUTO || route==KUI_INTERNET_BBA) {
        c->bba_port=kui_bba_console_port();
        int bba=c->bba_port->open(&c->bba,log,cancel);
        if(bba<0) {problem(s,c->bba.problem);goto failed;}
        if(bba>0) {c->kind=BBA;s->net=c->bba.net;s->adapter="Broadband Adapter";return s;}
        if(route==KUI_INTERNET_BBA) {problem(s,"No Broadband Adapter detected");goto failed;}
    }
    if(kui_storage_sci_reserved()) {
        problem(s,"SCI is in use by storage. Use the BBA, DreamPi, or move storage to SCIF/IDE.");
        goto failed;
    }
    if(route!=KUI_INTERNET_WIFI && kui_w5500_session_find(&c->w5500,kui_w5500_console_port(),log)) {
        static const uint8_t sizes[KUI_W5500_SOCKETS]={8,2,2,0,0,0,0,0};
        if(!kui_w5500_buffers(&c->w5500.chip,sizes,sizes)) {problem(s,"W5500 buffers unavailable");goto failed;}
        if(!kui_w5500_session_link(&c->w5500,cancel) || !kui_w5500_session_dhcp(&c->w5500,true,NULL,log,cancel,NULL)) {
            problem(s,c->w5500.problem);goto failed;
        }
        c->kind=WIRED;s->adapter="W5500 wired";
        memcpy(c->ip,c->w5500.config.ip,4);kui_w5500_session_sockets(&c->w5500,&s->net);
        return s;
    }
    if(route==KUI_INTERNET_W5500 || c->w5500.version==KUI_W5500_VERSION) {problem(s,c->w5500.problem);goto failed;}
    if(cancel && cancel()) {problem(s,"Scan cancelled");goto failed;}
    c->wifi=calloc(1,sizeof(*c->wifi));
    if(!c->wifi) {problem(s,"Insufficient memory for Wi-Fi board");goto failed;}
    if(!kui_wifi_session_find(c->wifi,kui_wifi_console_port(),log)) {
        const char *why_wifi=route==KUI_INTERNET_WIFI ? "No Wi-Fi board detected" : "No BBA, W5500, or Wi-Fi board detected";
        problem(s,c->wifi->answered ? c->wifi->problem : why_wifi);goto failed;
    }
    if(!kui_wifi_session_online(c->wifi,KUI_WIFI_JOIN_MS,cancel,NULL,NULL)) {problem(s,c->wifi->problem);goto failed;}
    c->kind=WIFI;s->adapter="Wi-Fi";
    memcpy(c->ip,c->wifi->host.wifi.ip,4);kui_wifi_session_sockets(c->wifi,&s->net);
    return s;
failed:
    if(why) snprintf(why,128,"%s",s->problem);
    kui_internet_close(s);
    return NULL;
}
