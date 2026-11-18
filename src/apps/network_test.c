/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/network_test.h"
#include "kui/network_w5500.h"
#include "kui/network_wifi.h"
#include "network_ethernet.h"
#include <kos/net.h>
#include <dc/g2bus.h>
#include <dc/net/broadband_adapter.h>
#include <stdio.h>
#include <string.h>

/* Pinned upstream KOS (fcfa7d869471): driver registration detects BBA/LAN and
 * reads saved configuration without net_init's uncancellable 60-second DHCP
 * wait. Do not add INIT_NET at boot, start packet reception without net_core,
 * or replace the configuration with an invented static address.
 * The PHY query below is read-only and is permitted only for an ALREADY active
 * BBA. Address/bit definitions follow KOS broadband_adapter.c and its header. */
#define KUI_BBA_NIC_BASE UINT32_C(0xa1001700)
static void snapshot(struct kui_network_snapshot *s,const netif_t *n) {
    memset(s,0,sizeof(*s));if(!n) return;
    snprintf(s->name,sizeof(s->name),"%s",n->name?n->name:"unknown");
    snprintf(s->description,sizeof(s->description),"%s",n->descr?n->descr:s->name);
    s->present=(n->flags&NETIF_DETECTED)!=0;s->ethernet=(n->flags&NETIF_NOETH)==0;
    s->initialized=(n->flags&NETIF_INITIALIZED)!=0;s->running=(n->flags&NETIF_RUNNING)!=0;
    memcpy(s->ip,n->ip_addr,4);memcpy(s->netmask,n->netmask,4);
    memcpy(s->gateway,n->gateway,4);memcpy(s->dns,n->dns,4);
    if(s->present && s->ethernet && s->initialized && !strcmp(s->name,"bba")) {
        /* BMSR's link bit may latch a prior loss; take its current second read. */
        (void)g2_read_16(KUI_BBA_NIC_BASE+RT_MII_BMSR);
        uint16_t status=g2_read_16(KUI_BBA_NIC_BASE+RT_MII_BMSR);
        if(status!=UINT16_MAX) s->link=status&RT_MII_LINK?KUI_NETWORK_LINK_UP:KUI_NETWORK_LINK_DOWN;
    }
}
void kui_network_app_run(struct kui_app_status *out,kui_log_fn log,kui_cancel_fn cancel) {
    if(!out) return;
    memset(out,0,sizeof(*out));
    if(cancel && cancel()) goto stopped;
    struct kui_ethernet_adapter adapter=kui_ethernet_find(true,cancel);
    netif_t *interface=adapter.interface;
    if(!(cancel && cancel())) {
        /* No BBA or LAN adapter: a W5500 or the Wi-Fi board on the SCI
         * port reports itself. */
        if(!interface && (kui_w5500_network_inspect(out,log,cancel) || kui_wifi_network_inspect(out,log,cancel))) return;
        struct kui_network_snapshot state;snapshot(&state,interface);
        kui_network_describe(out,&state);
    }
    kui_ethernet_release(&adapter);
    if(cancel && cancel()) goto stopped;
    if(log) {
        log("Network inspection: %s",out->message);
        for(unsigned i=0;i<out->line_count;++i) log("%s",out->lines[i]);
    }
    return;
stopped:
    out->stopped=true;out->complete=false;out->passed=false;
    snprintf(out->message,sizeof(out->message),"Network inspection stopped");
    if(log) log("Network inspection stopped; no DHCP request or reachability test");
}
