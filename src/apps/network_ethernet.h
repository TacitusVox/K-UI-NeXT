/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NETWORK_ETHERNET_H
#define KUI_NETWORK_ETHERNET_H
#include "kui/apps.h"
#include <kos/net.h>

struct kui_ethernet_adapter {
    netif_t *interface;
    int (*owned_shutdown)(void);
};

/* Prefer registered adapters. Inspection may also describe another already
 * detected Ethernet interface. A borrowed registration is never released. */
struct kui_ethernet_adapter kui_ethernet_find(bool inspect, kui_cancel_fn cancel);
void kui_ethernet_release(struct kui_ethernet_adapter *adapter);
#endif
