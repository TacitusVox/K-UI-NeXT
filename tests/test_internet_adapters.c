/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the production selector, SCI storage interlock, DNS/DHCP slot
 * separation, explicit dialing and symmetric cleanup with fake adapters. */
#include "../src/core/internet_internal.h"
#include "kui/bba_sockets.h"
#include "kui/dreampi.h"
#include "kui/network_bba.h"
#include "kui/network_w5500.h"
#include "kui/network_wifi.h"
#include <kos/net.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int bba_result,wired_result,wifi_result,modem_result;
static bool occupied,cancelled,joined,lease,slot_table,due,changed;
static unsigned tries[4],ends[4],renewals,queries;
static uint64_t now;
static uint8_t query[288];
static unsigned query_bytes;
static const uint8_t peer[4]={192,0,2,53};
netif_t *net_default_dev;
bool kui_storage_sci_reserved(void) {return occupied;}
uint64_t timer_ms_gettime64(void) {return now;}
uint64_t timer_us_gettime64(void) {return now*1000;}
void thd_sleep(unsigned ms) {now+=ms;}
static bool cancel(void) {return cancelled;}
static bool bba_poll(struct kui_bba_session *s) {(void)s;return true;}
static int bba_open(struct kui_bba_session *s,kui_log_fn log,kui_cancel_fn fn) {
    (void)log;(void)fn;tries[0]++;
    if(bba_result<0) strcpy(s->problem,"BBA link failed");
    return bba_result;
}
static void bba_end(struct kui_bba_session *s) {(void)s;ends[0]++;}
const struct kui_bba_port *kui_bba_console_port(void) {
    static const struct kui_bba_port port={.open=bba_open,.poll=bba_poll,.end=bba_end}; return &port;
}
const struct kui_w5500_port *kui_w5500_console_port(void) {static const struct kui_w5500_port p={0};return &p;}
bool kui_w5500_session_find(struct kui_w5500_session *s,const struct kui_w5500_port *p,kui_log_fn log) {
    (void)p;(void)log;tries[1]++;assert(!occupied);
    if(wired_result<0) {s->version=KUI_W5500_VERSION;strcpy(s->problem,"Wiring check failed");}
    return s->open=s->found=wired_result>0;
}
bool kui_w5500_buffers(struct kui_w5500 *w,const uint8_t rx[KUI_W5500_SOCKETS],const uint8_t tx[KUI_W5500_SOCKETS]) {
    (void)w;assert(rx[0] && rx[1] && rx[2] && tx[0] && tx[1] && tx[2]);return true;
}
bool kui_w5500_session_link(struct kui_w5500_session *s,kui_cancel_fn fn) {(void)s;return !fn();}
bool kui_w5500_session_dhcp(struct kui_w5500_session *s,bool only,struct kui_network_probe *probe,
    kui_log_fn log,kui_cancel_fn fn,void (*stage)(enum kui_network_stage)) {
    (void)probe;(void)log;(void)fn;(void)stage;assert(only);
    memcpy(s->config.dns,peer,4);s->config.ip[0]=10;
    if(!lease) strcpy(s->problem,"DHCP failed");
    return lease;
}
void kui_w5500_session_sockets(struct kui_w5500_session *s,struct kui_net_sockets *net) {net->ctx=s;}
uint32_t kui_w5500_session_expires_in(const struct kui_w5500_session *s,uint64_t t) {(void)s;(void)t;return lease?100:0;}
uint32_t kui_w5500_session_renew_in(const struct kui_w5500_session *s,uint64_t t) {(void)s;(void)t;return due?0:30;}
bool kui_w5500_session_renew(struct kui_w5500_session *s,unsigned slot,unsigned limit,bool *refused,kui_cancel_fn fn) {
    (void)limit;(void)fn;*refused=false;assert(slot==1);renewals++;due=false;
    if(changed) s->config.ip[3]++;
    return true;
}
void kui_w5500_session_end(struct kui_w5500_session *s) {if(s->open)ends[1]++;}
bool kui_w5500_open(struct kui_w5500 *w,unsigned slot,uint8_t mode,uint16_t port) {
    (void)w;assert(slot==2 && mode==KUI_W5500_UDP && port>=49152);return true;
}
bool kui_w5500_close(struct kui_w5500 *w,unsigned slot) {(void)w;assert(slot==2);return true;}
bool kui_w5500_datagram_send(struct kui_w5500 *w,unsigned slot,const uint8_t ip[4],uint16_t port,const void *p,uint16_t bytes) {
    (void)w;assert(slot==2 && !memcmp(ip,peer,4) && port==53 && bytes<=sizeof(query));
    memcpy(query,p,bytes);query_bytes=bytes;queries++;return true;
}
bool kui_w5500_datagram_receive(struct kui_w5500 *w,unsigned slot,uint8_t ip[4],uint16_t *port,void *p,size_t cap,size_t *bytes) {
    (void)w;assert(slot==2 && query_bytes>12);
    unsigned base=query_bytes-11; /* exclude query's EDNS pseudo-record */
    assert(cap>=base+16);
    memcpy(ip,peer,4);*port=53;
    uint8_t *reply=p;memcpy(reply,query,base);reply[2]=0x81;reply[3]=0x80;reply[7]=1;reply[11]=0;
    const uint8_t rr[]={0xc0,12,0,1,0,1,0,0,0,30,0,4,192,0,2,90};
    memcpy(reply+base,rr,sizeof(rr));*bytes=base+sizeof(rr);return true;
}
const struct kui_wifi_port *kui_wifi_console_port(void) {static const struct kui_wifi_port p={0};return &p;}
bool kui_wifi_session_find(struct kui_wifi_session *s,const struct kui_wifi_port *p,kui_log_fn log) {
    (void)log;(void)p;tries[2]++;assert(!occupied);return s->open=s->found=wifi_result>0;
}
bool kui_wifi_session_online(struct kui_wifi_session *s,unsigned limit,kui_cancel_fn fn,void (*stage)(void*,const char*),void *ctx) {
    (void)limit;(void)fn;(void)stage;(void)ctx;
    if(joined) {s->host.wifi.state=KWM_WIFI_ONLINE;s->host.wifi.ip[0]=192;}
    else strcpy(s->problem,"Wi-Fi is offline");
    return joined;
}
bool kui_wifi_session_step(struct kui_wifi_session *s) {if(changed) s->host.wifi.ip[0]=198;return true;}
bool kwh_dns(struct kwh *h,uint8_t tag,const char *name) {
    assert(!strcmp(name,"media.example.org"));queries++;
    h->dns_tag=tag;h->counts.dns++;h->dns_status=0;h->dns_ip[3]=93;
    return true;
}
void kui_wifi_session_sockets(struct kui_wifi_session *s,struct kui_net_sockets *net) {net->ctx=s;}
void kui_wifi_session_end(struct kui_wifi_session *s) {if(s->open) ends[2]++;}
int kui_dreampi_open(kui_log_fn log,kui_cancel_fn fn,char why[128]) {
    (void)log;(void)fn;tries[3]++;if(modem_result<=0) strcpy(why,"Modem offline");return modem_result;
}
bool kui_dreampi_online(void) {return modem_result>0;}
void kui_dreampi_close(void) {ends[3]++;}
bool kui_bba_sockets_init(struct kui_net_sockets *n) {(void)n;return slot_table;}
void kui_bba_sockets_end(struct kui_net_sockets *n) {(void)n;}
static char why[128];
static struct kui_internet *open_test(enum kui_internet_route route,int bba,int wired,int wifi,int modem) {
    bba_result=bba;wired_result=wired;wifi_result=wifi;modem_result=modem;renewals=queries=0;
    memset(tries,0,sizeof(tries));memset(ends,0,sizeof(ends));return kui_internet_open(route,NULL,cancel,why);
}
int main(void) {
    joined=lease=slot_table=true;
    struct kui_internet *s=open_test(KUI_INTERNET_AUTO,1,1,1,1);
    assert(s && !strcmp(s->adapter,"Broadband Adapter") && !tries[1] && !tries[3]);
    kui_internet_close(s);assert(ends[0]==1);
    assert(!open_test(KUI_INTERNET_AUTO,-1,1,1,1) && !strcmp(why,"BBA link failed") && !tries[1] && !tries[3]);
    occupied=true;
    assert(!open_test(KUI_INTERNET_AUTO,0,1,1,1) && strstr(why,"SCI") && !tries[1] && !tries[2] && !tries[3]);
    s=open_test(KUI_INTERNET_DREAMPI,1,1,1,1);assert(s && !tries[0] && !tries[1] && tries[3]==1 && s->poll(s));
    kui_internet_close(s);assert(ends[3]==1);
    slot_table=false;
    assert(!open_test(KUI_INTERNET_DREAMPI,1,1,1,1) && ends[3]==1 && !tries[0]);slot_table=true;
    assert(!open_test(KUI_INTERNET_DREAMPI,1,1,1,-1) && !ends[3] && !tries[0] && !strcmp(why,"Modem offline"));
    occupied=false;
    assert(!open_test(KUI_INTERNET_AUTO,0,-1,1,1) && !tries[2] && !tries[3]);
    s=open_test(KUI_INTERNET_AUTO,0,1,1,1);assert(s && !tries[2] && !tries[3]);due=true;uint8_t ip[4];
    assert(s->resolve(s,"media.example.org",ip) && ip[3]==90 && renewals==1 && queries==1);
    assert(s->resolve(s,"MEDIA.EXAMPLE.ORG",ip) && queries==1);
    now+=31000;assert(s->resolve(s,"media.example.org",ip) && queries==2);
    due=changed=true;assert(!s->poll(s));kui_internet_close(s);assert(ends[1]==1);changed=false;
    lease=false;assert(!open_test(KUI_INTERNET_AUTO,0,1,1,1) && ends[1]==1 && !tries[2]);lease=true;
    s=open_test(KUI_INTERNET_AUTO,0,0,1,1);assert(s && !tries[3]);
    assert(s->resolve(s,"media.example.org",ip) && ip[3]==93 && queries==1);
    changed=true;assert(!s->poll(s));kui_internet_close(s);assert(ends[2]==1);changed=false;
    joined=false;assert(!open_test(KUI_INTERNET_AUTO,0,0,1,1) && ends[2]==1 && !tries[3]);
    assert(!open_test(KUI_INTERNET_AUTO,0,0,0,1) && !tries[3]);
    cancelled=true;assert(!open_test(KUI_INTERNET_DREAMPI,1,1,1,1) && !tries[0] && !tries[3]);
    cancelled=false;joined=true;
    s=open_test(KUI_INTERNET_BBA,1,1,1,1);assert(s && tries[0] && !tries[1] && !tries[3]);kui_internet_close(s);
    assert(!open_test(KUI_INTERNET_BBA,0,1,1,1) && !tries[1] && !tries[2] && !tries[3]);
    s=open_test(KUI_INTERNET_W5500,-1,1,1,1);assert(s && !tries[0] && tries[1] && !tries[2]);kui_internet_close(s);
    assert(!open_test(KUI_INTERNET_W5500,1,0,1,1) && !tries[0] && tries[1] && !tries[2]);
    s=open_test(KUI_INTERNET_WIFI,-1,1,1,1);assert(s && !tries[0] && !tries[1] && tries[2]);kui_internet_close(s);
    assert(!open_test(KUI_INTERNET_WIFI,1,1,0,1) && !tries[0] && !tries[1] && !tries[3]);
    occupied=true;
    assert(!open_test(KUI_INTERNET_W5500,1,1,1,1) && !tries[0] && !tries[1] && !tries[2]);
    assert(!open_test(KUI_INTERNET_WIFI,1,1,1,1) && !tries[0] && !tries[1] && !tries[2]);
    puts("Internet adapter selection, SCI interlock, explicit PPP, DNS cache, lease and cleanup passed");
}
