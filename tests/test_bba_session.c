/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/network_bba.h"
#include <kos/net.h>
#include <kos/irq.h>
#include <dc/net/broadband_adapter.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct netif_list devices;
static netif_t adapter;
netif_t *net_default_dev;
static const uint8_t offered[] = {192,168,1,42}, router[] = {192,168,1,1},
                     server[] = {10,0,0,2}, server2[] = {10,0,0,3}, peer[] = {2,0,0,0,0,2};
static uint64_t now, cancel_at;
static bool present, cable, reply, stack, reject, fail_init;
static unsigned starts, stops, restarts, renewals, rebinds, forwarded, releases, arps, busy;
static net_input_func target;
static int stack_input(netif_t *n, const uint8_t *frame, int size) {
    (void)frame;
    assert(n == &adapter && size >= 14 && stack);
    ++forwarded;
    return 0;
}
static int hw_init(netif_t *n) {
    assert(target != stack_input && !stack);
    n->flags |= NETIF_INITIALIZED;
    return 0;
}
static int hw_start(netif_t *n) {
    assert(n->flags & NETIF_INITIALIZED);
    ++starts;
    if(!cable) return -1;
    n->flags |= NETIF_RUNNING;
    return 0;
}
static int hw_stop(netif_t *n) {
    assert(target != stack_input);
    ++stops;
    n->flags &= ~NETIF_RUNNING;
    /* A final callback during the join cannot enter the socket stack. */
    uint8_t frame[64] = {0};
    target(n,frame,sizeof(frame));
    return 0;
}
static int hw_down(netif_t *n) { n->flags &= ~NETIF_INITIALIZED; return 0; }
int bba_init(void) {
    if(!present) return -1;
    adapter.flags = NETIF_DETECTED | NETIF_REGISTERED;
    LIST_INSERT_HEAD(&devices,&adapter,if_list);
    return 0;
}
int bba_shutdown(void) { assert(!(adapter.flags & NETIF_RUNNING)); return 0; }
int la_init(void) { return -1; }
int la_shutdown(void) { return 0; }
struct netif_list *net_get_if_list(void) { return &devices; }
int net_unreg_device(netif_t *n) { ++releases; LIST_REMOVE(n,if_list); return 0; }
netif_t *net_set_default(netif_t *n) { netif_t *old=net_default_dev; net_default_dev=n; return old; }
net_input_func net_input_set_target(net_input_func next) { net_input_func old=target; target=next; return old; }
int net_init(uint32_t ip) {
    assert(!ip && !memcmp(adapter.ip_addr,offered,4) && adapter.broadcast[3] == 255);
    assert(!stack && (adapter.flags & NETIF_RUNNING));
    net_set_default(&adapter);
    if(fail_init) return -1;
    ++restarts;
    stack=true;
    return 0;
}
void net_shutdown(void) {
    assert(stack && !(adapter.flags & NETIF_RUNNING));
    hw_down(&adapter);
    net_set_default(NULL);
    stack=false;
}
irq_mask_t irq_disable(void) { return 0; }
void irq_restore(irq_mask_t mask) { assert(!mask); }
uint64_t timer_ms_gettime64(void) { return now; }
void thd_sleep(unsigned ms) { now += ms; }
uint16_t g2_read_16(uint32_t address) {
    assert(address == UINT32_C(0xa1001700) + RT_MII_BMSR);
    return cable ? RT_MII_LINK : 0;
}
int net_arp_lookup(netif_t *n, const uint8_t ip[4], uint8_t mac[6],
                   const void *pkt, const uint8_t *data, int len) {
    assert(stack && n==&adapter && !memcmp(ip,router,4) && !pkt && !data && !len);
    memcpy(mac,peer,6);
    ++arps;
    return 0;
}
static void put16(uint8_t *dst,unsigned n) { dst[0]=n>>8; dst[1]=n; }
static void put_option(uint8_t *f,unsigned *k,uint8_t id,const uint8_t value[4]) {
    f[(*k)++]=id; f[(*k)++]=4; memcpy(f+*k,value,4); *k+=4;
}
int bba_tx(const uint8_t *f,int bytes,int wait) {
    assert(wait==BBA_TX_NOWAIT && (adapter.flags & NETIF_RUNNING) && ((uintptr_t)f & 31u)==0);
    if(busy) { --busy; return BBA_TX_AGAIN; }
    if(f[13]==6) return 0; /* no address conflict */
    assert(bytes>=342);
    bool unicast = f[0] != 255;
    bool renewal = !memcmp(f+54,offered,4);
    if(renewal && unicast) { assert(!memcmp(f,peer,6)); ++renewals; }
    if(renewal && !unicast) ++rebinds;
    if(!reply) return 0;
    uint8_t r[512]={0};
    memcpy(r,f,42);
    memcpy(r,adapter.mac_addr,6); memcpy(r+6,peer,6);
    memcpy(r+26,router,4); memset(r+30,255,4);
    put16(r+34,67); put16(r+36,68);
    r[42]=2; r[43]=1; r[44]=6;
    memcpy(r+46,f+46,4); memcpy(r+58,offered,4); memcpy(r+70,adapter.mac_addr,6);
    memcpy(r+278,f+278,4);
    unsigned k=282; r[k++]=53; r[k++]=1; r[k++]=f[284]==1 ? 2 : reject ? 6 : 5;
    put_option(r,&k,54,renewal && !unicast ? server2 : server);
    put_option(r,&k,1,(uint8_t[]){255,255,255,0});
    put_option(r,&k,3,router);
    put_option(r,&k,51,(uint8_t[]){0,0,0,40});
    r[k++]=255;
    put16(r+38,k-34); put16(r+16,k-14); put16(r+24,0);
    unsigned sum=0;
    for(unsigned i=14;i<34;i+=2)sum+=(unsigned)r[i]*256u+r[i+1];
    while(sum>>16)sum=(sum&65535u)+(sum>>16);
    put16(r+24,~sum);
    target(&adapter,r,(int)k);
    return 0;
}
static bool cancelled(void) { return cancel_at && now>=cancel_at; }
static void restored(void) {
    assert(!stack && !net_default_dev && LIST_EMPTY(&devices) && target==stack_input);
    assert(adapter.ip_addr[0]==172 && adapter.broadcast[3]==99);
}
int main(void) {
    const struct kui_bba_port *port=kui_bba_console_port();
    struct kui_bba_session s={0}, other={0};
    adapter=(netif_t){.name="bba",.mac_addr={2,3,4,5,6,7},.ip_addr={172,16,0,42},
        .broadcast={172,16,0,99},.if_init=hw_init,.if_start=hw_start,.if_stop=hw_stop,.if_shutdown=hw_down};
    target=stack_input;
    LIST_INIT(&devices);
    assert(port->open(&s,NULL,cancelled)==0); restored();
    present=cable=reply=true; busy=2;
    for(unsigned i=0;i<3;++i) {
        assert(port->open(&s,NULL,cancelled)==1 && s.net.count==8 && !memcmp(s.ip,offered,4));
        assert(port->open(&other,NULL,cancelled)==-1);
        uint8_t traffic[64]={0}; target(&adapter,traffic,sizeof(traffic)); assert(forwarded==i+1);
        port->end(&s); restored();
    }
    reply=false; cancel_at=now+30;
    assert(port->open(&s,NULL,cancelled)==-1 && strstr(s.problem,"Stopped")); restored();
    cancel_at=0; uint64_t start=now;
    assert(port->open(&s,NULL,cancelled)==-1 && now-start<15100 && strstr(s.problem,"offer")); restored();
    reply=true; fail_init=true;
    assert(port->open(&s,NULL,cancelled)==-1 && strstr(s.problem,"TCP/IP")); restored();
    fail_init=false; cable=false;
    assert(port->open(&s,NULL,cancelled)==-1 && strstr(s.problem,"link")); restored();
    cable=true;
    assert(port->open(&s,NULL,cancelled)==1);
    reply=false; now+=18000;
    assert(port->poll(&s) && renewals==1 && arps==1);
    now+=14000; reply=true;
    assert(port->poll(&s) && rebinds==1 && arps==1);
    now+=5000; assert(port->poll(&s));
    now+=41000; assert(!port->poll(&s) && strstr(s.problem,"expired"));
    port->end(&s); restored();
    assert(port->open(&s,NULL,cancelled)==1);
    now+=300; cable=false;
    assert(!port->poll(&s) && strstr(s.problem,"link")); port->end(&s); restored();
    cable=true;
    assert(port->open(&s,NULL,cancelled)==1);
    now+=21000; reject=true;
    assert(!port->poll(&s) && strstr(s.problem,"rejected")); port->end(&s); restored();
    assert(restarts==6 && starts==10 && stops==9 && releases==10);
    puts("PASS BBA lifecycle: DHCP, conflict-check wait, cancellation, absence, busy TX, three repeat opens, "
         "socket core failure, RX drain, link loss, renewal, rebind, NAK, expiry, settings restoration");
}
