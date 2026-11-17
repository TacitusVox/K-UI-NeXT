/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/dreampi.h"
#include "../apps/network_ethernet.h"
#include <dc/modem/modem.h>
#include <kos/irq.h>
#include <kos/thread.h>
#include <kos/timer.h>
#include <ppp/ppp.h>
#include <stdio.h>

int kui_ppp_connect_until(int (*stop_requested)(void));
int kui_modem_irq_ready(void);
void modemIntShutdown(void); /* Pinned KOS modem interrupt and timer teardown. */

static bool owned, modem, ppp, stack;
static volatile bool io_failed;
static kui_cancel_fn cancelled;
static uint64_t negotiation_deadline;

static int stop_negotiating(void) {
    return (cancelled && cancelled()) || io_failed || !modem_is_connected() ||
        timer_ms_gettime64() >= negotiation_deadline;
}

static int ready(ppp_device_t *dev) {
    (void)dev;
    return 0;
}

static int send_bytes(ppp_device_t *dev, const uint8_t *data, size_t length, uint32_t flags) {
    (void)dev;
    (void)flags;
    uint64_t deadline = timer_ms_gettime64() + 5000;
    while(length && !io_failed) {
        if(!modem_is_connected() || timer_ms_gettime64() >= deadline) {
            io_failed = true;
            break;
        }
        int sent = modem_write_data((uint8_t *)data, (int)length);
        if(sent < 0 || (size_t)sent > length || (!sent && irq_inside_int())) {
            io_failed = true;
            break;
        }
        if(sent) {
            data += sent;
            length -= sent;
            deadline = timer_ms_gettime64() + 5000;
        } else {
            thd_sleep(2);
        }
    }
    return length ? -1 : 0;
}

static const uint8_t *receive_bytes(ppp_device_t *dev, ssize_t *length) {
    static uint8_t data[1024];
    (void)dev;
    int count = modem_read_data(data, sizeof(data));
    *length = count > 0 ? count : 0;
    return *length ? data : NULL;
}

static ppp_device_t device = {
    .name = "modem", .descr = "DreamPi modem", .detect = ready, .init = ready,
    .shutdown = ready, .tx = send_bytes, .rx = receive_bytes
};

bool kui_dreampi_online(void) {
    return owned && modem && ppp && stack && !io_failed && modem_is_connected();
}

void kui_dreampi_close(void) {
    if(!owned) return;
    io_failed = true;
    if(ppp) {
        ppp_shutdown(); /* Joins the PPP worker before releasing its buffers. */
        net_set_default(NULL);
    }
    if(stack) net_shutdown();
    if(modem) {
        modemIntShutdown();
        modem_shutdown(); /* Drops the line, including during an interrupted call. */
    }
    cancelled = NULL;
    modem = ppp = stack = false;
    irq_mask_t lock = irq_disable();
    owned = false;
    irq_restore(lock);
}

int kui_dreampi_open(kui_log_fn log, kui_cancel_fn cancel, char problem[128]) {
    const char *reason = "Stopped";
    irq_mask_t lock = irq_disable();
    bool busy = owned || net_default_dev || !LIST_EMPTY(net_get_if_list());
    if(!busy) owned = true;
    irq_restore(lock);
    if(busy) {
        snprintf(problem, 128, "Another network session owns the adapter");
        return -1;
    }
    cancelled = cancel;
    io_failed = false;
    if(cancel && cancel()) goto fail;
    struct kui_ethernet_adapter expansion = kui_ethernet_find(false, cancel);
    bool ethernet = expansion.interface != NULL;
    kui_ethernet_release(&expansion);
    if(ethernet) { reason = "An Ethernet adapter occupies the modem bay"; goto fail; }
    if(cancel && cancel()) goto fail;
    if(!modem_init()) { reason = "Dreamcast modem was not detected"; goto fail; }
    modem = true;
    if(!kui_modem_irq_ready()) { reason = "Insufficient memory for the modem"; goto fail; }
    if(!modem_set_mode(MODEM_MODE_REMOTE, MODEM_SPEED_V8_AUTO)) {
        reason = "Modem could not open the DreamPi line";
        goto fail;
    }
    if(log) log("Waiting for DreamPi dial tone");
    uint64_t deadline = timer_ms_gettime64() + 5000;
    while(modem_wait_dialtone(100) != 0) {
        if(cancel && cancel()) goto fail;
        if(timer_ms_gettime64() >= deadline) { reason = "No DreamPi dial tone"; goto fail; }
    }
    if(cancel && cancel()) goto fail;
    if(!modem_dial("555")) { reason = "Modem could not dial DreamPi"; goto fail; }
    if(log) log("Dialing DreamPi (555)");
    deadline = timer_ms_gettime64() + 60000;
    while(modem_is_connecting() && timer_ms_gettime64() < deadline) {
        if(cancel && cancel()) goto fail;
        thd_sleep(25);
    }
    if(!modem_is_connected()) { reason = "DreamPi did not answer"; goto fail; }
    if(cancel && cancel()) goto fail;
    if(ppp_init() != 0) { reason = "PPP is unavailable"; goto fail; }
    ppp = true;
    if(ppp_set_device(&device) || ppp_set_login("dream", "cast")) {
        reason = "PPP setup failed"; goto fail;
    }
    if(net_init(0) < 0) { reason = "Could not start the TCP/IP stack"; goto fail; }
    stack = true;
    if(log) log("Carrier %lu bps. Negotiating IPv4 and DNS", modem_get_connection_rate());
    negotiation_deadline = timer_ms_gettime64() + 45000;
    if(kui_ppp_connect_until(stop_negotiating)) {
        if(!(cancel && cancel())) reason = "DreamPi PPP negotiation failed or timed out";
        goto fail;
    }
    if(!net_default_dev || !net_default_dev->ip_addr[0] || !net_default_dev->dns[0]) {
        reason = "DreamPi did not supply an IPv4 address and DNS";
        goto fail;
    }
    if(cancel && cancel()) goto fail;
    problem[0] = 0;
    return 1;
fail:
    snprintf(problem, 128, "%s", reason);
    kui_dreampi_close();
    return -1;
}
