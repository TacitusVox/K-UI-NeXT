/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/network_bba.h"
#include "kui/bba_sockets.h"
#include "kui/network_probe.h"
#include "../apps/network_ethernet.h"
#include <dc/g2bus.h>
#include <dc/net/broadband_adapter.h>
#include <kos/irq.h>
#include <kos/thread.h>
#include <kos/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct driver {
    struct kui_ethernet_adapter adapter;
    net_input_func original;
    struct kui_network_probe probe;
    struct kui_network_config saved, leased;
    uint8_t broadcast[4], server[4];
    uint64_t renew_at, rebind_at, expires_at, check_link_at;
    bool hooked, stack, forwarding, probing, renewing;
};
static struct driver *active;

static void set_problem(struct kui_bba_session *s, const char *reason) {
    snprintf(s->problem, sizeof(s->problem), "%s", reason);
}

static bool is_dhcp_reply(const uint8_t *p, int size) {
    if(size < 42 || p[12] != 8 || p[13] != 0 || p[23] != 17) return false;
    unsigned offset = 14 + (p[14] & 15u) * 4;
    return offset >= 34 && offset + 8 <= (unsigned)size &&
        !p[offset] && p[offset+1] == 67 && !p[offset+2] && p[offset+3] == 68;
}

/* Runs on the BBA RX worker. Only DHCP replies and pre-address ARP need
 * the K-UI probe. Forward ordinary traffic only while the stack is live. */
static int input(netif_t *n, const uint8_t *frame, int size) {
    struct driver *d = active;
    if(!d || n != d->adapter.interface || size < 14 || size > (int)KUI_NETWORK_FRAME_MAX) return 0;
    bool dhcp = is_dhcp_reply(frame, size);
    if(dhcp || (!d->forwarding && frame[12] == 8 && frame[13] == 6)) {
        irq_mask_t flags = irq_disable();
        if(d->probing) {
            kui_network_probe_receive(&d->probe, frame, (size_t)size, timer_ms_gettime64());
        }
        irq_restore(flags);
        return 0;
    }
    return d->forwarding && d->original ? d->original(n, frame, size) : 0;
}

static void configure(netif_t *n, const struct kui_network_config *c) {
    memcpy(n->ip_addr, c->ip, 4);
    memcpy(n->netmask, c->mask, 4);
    memcpy(n->gateway, c->gateway, 4);
    memcpy(n->dns, c->dns, 4);
    for(unsigned i = 0; i < 4; ++i) n->broadcast[i] = c->ip[i] | (uint8_t)~c->mask[i];
}

static void end(struct kui_bba_session *s) {
    struct driver *d = s->driver;
    if(!d) return;
    netif_t *n = d->adapter.interface;
    kui_bba_sockets_end(&s->net);
    if(d->hooked) {
        d->forwarding = d->probing = false;
        /* Join any RX callback already inside KOS before freeing protocol
         * state. The GUI-linked driver drains its G2 DMA before returning. */
        if(n->flags & NETIF_RUNNING) n->if_stop(n);
        if(d->stack) net_shutdown();
        if(n->flags & NETIF_INITIALIZED) n->if_shutdown(n);
        net_set_default(NULL);
        configure(n, &d->saved);
        memcpy(n->broadcast, d->broadcast, 4);
        net_input_set_target(d->original);
    }
    kui_ethernet_release(&d->adapter);
    if(active == d) active = NULL;
    free(d);
    s->driver = NULL;
    s->reuse_listener = NULL;
}

static void save_lease(struct driver *d) {
    const struct kui_network_probe *p = &d->probe;
    d->leased = p->config;
    memcpy(d->server, p->server, 4);
    if(p->lease_seconds == UINT32_MAX) {
        d->renew_at = d->rebind_at = d->expires_at = UINT64_MAX;
    } else {
        d->renew_at = p->lease_start_ms + (uint64_t)p->renew_seconds * 1000;
        d->rebind_at = p->lease_start_ms + (uint64_t)p->rebind_seconds * 1000;
        d->expires_at = p->lease_start_ms + (uint64_t)p->lease_seconds * 1000;
    }
    d->renewing = d->probing = false;
    configure(d->adapter.interface, &d->leased);
}

/* Packet construction and ACK processing share the probe between the
 * I/O and RX workers. No sleeps or driver calls under the IRQ lock. */
static size_t next_request(struct driver *d, uint8_t *frame, uint64_t now) {
    irq_mask_t flags = irq_disable();
    size_t bytes = kui_network_probe_step(&d->probe, frame, now);
    irq_restore(flags);
    return bytes;
}

static bool lease_step(struct kui_bba_session *s, kui_cancel_fn cancel) {
    struct driver *d = s->driver;
    _Alignas(32) uint8_t frame[KUI_NETWORK_FRAME_MAX];
    uint64_t start = timer_ms_gettime64();
    size_t bytes = next_request(d, frame, start);
    if(bytes && d->renewing && !d->probe.rebinding) {
        const struct kui_network_config *c = &d->leased;
        const uint8_t *next_hop = d->server;
        for(unsigned i = 0; i < 4; ++i)
            if((c->ip[i] ^ d->server[i]) & c->mask[i]) next_hop = c->gateway;
        /* A relayed DHCP ACK need not originate on our subnet. ARP for
         * the actual next hop. The probe retries if ARP is still pending. */
        if(net_arp_lookup(d->adapter.interface, next_hop, frame, NULL, NULL, 0) < 0) return true;
    }
    if(bytes) {
        int result;
        while((result = bba_tx(frame, (int)bytes, BBA_TX_NOWAIT)) == BBA_TX_AGAIN) {
            if((cancel && cancel()) || timer_ms_gettime64() - start >= 500) break;
            thd_sleep(5);
        }
        if(result != BBA_TX_OK) {
            set_problem(s, cancel && cancel() ? "Stopped" : "BBA transmit queue or link unavailable");
            return false;
        }
    }
    return true;
}

static bool poll_session(struct kui_bba_session *s) {
    struct driver *d = s->driver;
    if(!d || !d->stack) return false;
    uint64_t now = timer_ms_gettime64();
    if(now >= d->check_link_at) {
        /* BMSR can latch a previous link loss. */
        const uint32_t bmsr = UINT32_C(0xa1001700) + RT_MII_BMSR;
        (void)g2_read_16(bmsr);
        uint16_t status = g2_read_16(bmsr);
        if(status == UINT16_MAX || !(status & RT_MII_LINK)) {
            set_problem(s, "BBA cable link lost");
            return false;
        }
        d->check_link_at = now + 250;
    }
    if(now >= d->expires_at) {
        set_problem(s, "BBA DHCP lease expired; reconnect");
        return false;
    }
    if(!d->renewing && now < d->renew_at) return true;
    if(!d->renewing) {
        irq_mask_t flags = irq_disable();
        kui_network_probe_renew(&d->probe, s->mac, (uint32_t)now, &d->leased, d->server, now);
        d->renewing = d->probing = true;
        irq_restore(flags);
    }
    if(now >= d->rebind_at) d->probe.rebinding = true;
    if(!lease_step(s, NULL)) return false;
    if(d->probe.stage == KUI_NET_DONE) {
        save_lease(d);
    } else if(d->probe.stage == KUI_NET_FAILED) {
        if(!strcmp(d->probe.failure, "No DHCP ACK within 15 seconds")) {
            /* A missed renewal can be retried while the old lease is valid. */
            d->renewing = d->probing = false;
            d->renew_at = now + 1000;
        } else {
            set_problem(s, d->probe.failure);
            return false;
        }
    }
    return true;
}

static int open_exclusive(struct kui_bba_session *s, kui_log_fn log, kui_cancel_fn cancel) {
    memset(s, 0, sizeof(*s));
    if(active || net_default_dev) {
        set_problem(s, "Another network session is already active");
        return -1;
    }
    netif_t *n;
    LIST_FOREACH(n, net_get_if_list(), if_list) {
        if(n->flags & (NETIF_RUNNING | NETIF_INITIALIZED)) {
            set_problem(s, "Network hardware is already in use");
            return -1;
        }
    }
    if(cancel && cancel()) { set_problem(s, "Stopped"); return -1; }
    struct kui_ethernet_adapter a = kui_ethernet_find(false, cancel);
    if(!a.interface || strcmp(a.interface->name, "bba")) {
        kui_ethernet_release(&a);
        if(cancel && cancel()) { set_problem(s, "Stopped"); return -1; }
        set_problem(s, "No Dreamcast Broadband Adapter detected");
        return 0;
    }
    /* net_init starts *all* registered devices. Refuse to disturb another
     * registration instead of starting a different driver behind its owner. */
    LIST_FOREACH(n, net_get_if_list(), if_list) {
        if(n != a.interface) {
            kui_ethernet_release(&a);
            set_problem(s, "Another network interface is already registered");
            return -1;
        }
    }
    struct driver *d = calloc(1, sizeof(*d));
    if(!d) {
        kui_ethernet_release(&a);
        set_problem(s, "Insufficient memory for the BBA");
        return -1;
    }
    s->driver = active = d;
    d->adapter = a;
    n = a.interface;
    memcpy(d->saved.ip, n->ip_addr, 4);
    memcpy(d->saved.mask, n->netmask, 4);
    memcpy(d->saved.gateway, n->gateway, 4);
    memcpy(d->saved.dns, n->dns, 4);
    memcpy(d->broadcast, n->broadcast, 4);
    d->original = net_input_set_target(input);
    d->hooked = true;
    if(log) log("BBA: starting cable link (10 second KOS limit)");
    if(n->if_init(n) < 0 || n->if_start(n) < 0) {
        set_problem(s, "BBA link did not start; check the cable");
        goto fail;
    }

    uint64_t now = timer_ms_gettime64();
    memcpy(s->mac, n->mac_addr, 6);
    kui_network_probe_begin(&d->probe, s->mac, (uint32_t)now ^ UINT32_C(0x424241), NULL, now);
    d->probe.lease_only = d->probing = true;
    if(log) log("BBA: requesting DHCP and checking for an address conflict");
    while(d->probe.stage < KUI_NET_DONE) {
        if(cancel && cancel()) { set_problem(s, "Stopped"); goto fail; }
        if(!lease_step(s, cancel)) goto fail;
        thd_sleep(10);
    }
    if(d->probe.stage != KUI_NET_DONE) { set_problem(s, d->probe.failure); goto fail; }
    save_lease(d);
    /* With a validated address already installed this starts only the KOS
     * socket stack. It skips KOS's synchronous 60-second DHCP request. */
    if(net_init(0) < 0) { set_problem(s, "KOS TCP/IP initialization failed"); goto fail; }
    d->stack = d->forwarding = true;
    memcpy(s->ip, d->leased.ip, 4);
    if(!poll_session(s)) goto fail;
    if(!kui_bba_sockets_init(&s->net)) { set_problem(s, "Insufficient memory for BBA sockets"); goto fail; }
    s->reuse_listener = kui_bba_sockets_reuse_listener;
    snprintf(s->name, sizeof(s->name), "Dreamcast Broadband Adapter");
    if(log) log("BBA: online at %u.%u.%u.%u", s->ip[0], s->ip[1], s->ip[2], s->ip[3]);
    return 1;
fail:
    end(s);
    return -1;
}

static bool opening;
static int open_session(struct kui_bba_session *s, kui_log_fn log, kui_cancel_fn cancel) {
    irq_mask_t flags = irq_disable();
    if(opening || active) {
        irq_restore(flags);
        set_problem(s, "Another BBA session is already active");
        return -1;
    }
    opening = true;
    irq_restore(flags);
    int result = open_exclusive(s, log, cancel);
    flags = irq_disable();
    opening = false;
    irq_restore(flags);
    return result;
}

static void pause_ms(unsigned ms) { thd_sleep(ms); }
const struct kui_bba_port *kui_bba_console_port(void) {
    static const struct kui_bba_port port = {open_session, poll_session, end, timer_ms_gettime64, pause_ms};
    return &port;
}
