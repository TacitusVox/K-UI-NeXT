/* SPDX-License-Identifier: GPL-3.0-only */
#include "network_ethernet.h"
#include <dc/net/broadband_adapter.h>
#include <dc/net/lan_adapter.h>
#include <string.h>

static netif_t *registered(const char *name) {
    netif_t *n;
    LIST_FOREACH(n, net_get_if_list(), if_list)
        if(n->name && !strcmp(n->name, name)) return n;
    return NULL;
}

struct kui_ethernet_adapter kui_ethernet_find(bool inspect, kui_cancel_fn cancel) {
    static const struct {
        const char *name;
        int (*init)(void), (*shutdown)(void);
    } drivers[] = {{"bba", bba_init, bba_shutdown}, {"la", la_init, la_shutdown}};
    netif_t *existing[2];
    struct kui_ethernet_adapter adapter = {0};

    for(unsigned i = 0; i < 2; ++i) {
        netif_t *n = existing[i] = registered(drivers[i].name);
        if(!n) continue;
        /* KOS LAN if_shutdown clears DETECTED but keeps a borrowed netif
         * registered. Re-detect before use; re-registration would corrupt
         * KOS's intrusive device list. Never reset a live interface. */
        if(!(n->flags & (NETIF_DETECTED | NETIF_INITIALIZED | NETIF_RUNNING)) && n->if_detect)
            n->if_detect(n);
        if(n->flags & NETIF_DETECTED) {
            adapter.interface = n;
            return adapter;
        }
    }
    if(inspect) {
        netif_t *n;
        LIST_FOREACH(n, net_get_if_list(), if_list) {
            if((n->flags & NETIF_DETECTED) && !(n->flags & NETIF_NOETH)) {
                adapter.interface = n;
                return adapter;
            }
        }
    }
    for(unsigned i = 0; i < 2; ++i) {
        if(existing[i]) continue;
        if(drivers[i].init() == 0) {
            adapter.interface = registered(drivers[i].name);
            if(adapter.interface) {
                adapter.owned_shutdown = drivers[i].shutdown;
                break;
            }
        }
        /* BBA allocates a semaphore even on a detection miss. */
        drivers[i].shutdown();
        if(cancel && cancel()) break;
    }
    return adapter;
}

void kui_ethernet_release(struct kui_ethernet_adapter *adapter) {
    if(adapter->owned_shutdown) {
        adapter->owned_shutdown();
        if(adapter->interface) net_unreg_device(adapter->interface);
    }
}
