/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/dreampi.h"
#include "../src/apps/network_ethernet.h"
#include <dc/modem/modem.h>
#include <kos/irq.h>
#include <ppp/ppp.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum scenario { OK, BUSY, ETHERNET, NO_MODEM, NO_TIMER, NO_TONE, NO_ANSWER,
    CANCEL_TONE, CANCEL_CALL, CANCEL_PPP, TIMEOUT_PPP, NO_DNS, TX_STALL };
static enum scenario mode;
static struct netif_list interfaces = LIST_HEAD_INITIALIZER(interfaces);
static netif_t ppp_link;
netif_t *net_default_dev;
static uint64_t elapsed;
static bool have_modem, have_ppp, have_stack, dialing, carrier;
static int dials, remaining, release_count;
static ppp_device_t *attached;

irq_mask_t irq_disable(void) { return 0; }
void irq_restore(irq_mask_t mask) { (void)mask; }
int irq_inside_int(void) { return 0; }
uint64_t timer_ms_gettime64(void) { return elapsed; }
void thd_sleep(unsigned ms) { elapsed += ms; }
struct netif_list *net_get_if_list(void) { return &interfaces; }
struct kui_ethernet_adapter kui_ethernet_find(bool inspect, kui_cancel_fn cancel) {
    (void)inspect; (void)cancel;
    return (struct kui_ethernet_adapter){ .interface = mode == ETHERNET ? &ppp_link : NULL };
}
void kui_ethernet_release(struct kui_ethernet_adapter *s) { (void)s; ++release_count; }
int modem_init(void) { assert(!have_modem); return have_modem = mode != NO_MODEM; }
int kui_modem_irq_ready(void) { return mode != NO_TIMER; }
int modem_set_mode(int m, int speed) { return m == MODEM_MODE_REMOTE && speed == MODEM_SPEED_V8_AUTO; }
int modem_wait_dialtone(int ms) { elapsed += ms; return mode != NO_TONE && mode != CANCEL_TONE ? 0 : -1; }
int modem_dial(const char *digits) {
    assert(!strcmp(digits, "555") && have_modem);
    ++dials; dialing = true; return 1;
}
int modem_is_connecting(void) {
    if(!dialing || elapsed < 200 || mode == NO_ANSWER || mode == CANCEL_CALL) return dialing;
    dialing = false; carrier = true; return 0;
}
int modem_is_connected(void) { return carrier; }
unsigned long modem_get_connection_rate(void) { return 33600; }
int modem_write_data(unsigned char *data, int size) {
    assert(size > 0 && data && have_modem);
    if(mode == TX_STALL) return 0;
    return --remaining >= 0 ? 0 : 1;
}
int modem_read_data(unsigned char *data, int size) { (void)data; (void)size; return 0; }
void modemIntShutdown(void) { assert(have_modem && !have_ppp && !have_stack); }
void modem_shutdown(void) { assert(have_modem); have_modem = dialing = carrier = false; }
int ppp_init(void) { assert(have_modem && carrier); have_ppp = true; return 0; }
int ppp_set_device(ppp_device_t *device) { assert(have_ppp); attached = device; return 0; }
int ppp_set_login(const char *user, const char *password) {
    return strcmp(user, "dream") || strcmp(password, "cast");
}
int ppp_shutdown(void) { assert(have_ppp); have_ppp = false; return 0; }
netif_t *net_set_default(netif_t *link) {
    netif_t *old = net_default_dev; net_default_dev = link; return old;
}
int net_init(uint32_t ip) {
    assert(!ip && have_ppp && !have_stack); net_set_default(&ppp_link); have_stack = true; return 0;
}
void net_shutdown(void) { assert(!have_ppp && have_stack && !net_default_dev); have_stack = false; }
int kui_ppp_connect_until(int (*stop)(void)) {
    assert(have_stack && attached && carrier);
    if(mode == TIMEOUT_PPP || mode == CANCEL_PPP) {
        while(!stop()) thd_sleep(100);
        return -1;
    }
    ppp_link.ip_addr[0] = 192; ppp_link.dns[0] = mode == NO_DNS ? 0 : 192;
    return 0;
}
static bool stop(void) {
    if(mode == CANCEL_TONE || mode == CANCEL_CALL) return elapsed >= 350;
    return mode == CANCEL_PPP && elapsed >= 600;
}
static void run(enum scenario scenario) {
    mode = scenario; elapsed = 0; dials = release_count = 0; remaining = 2;
    char reason[128] = {0};
    if(mode == BUSY) net_default_dev = &ppp_link;
    int result = kui_dreampi_open(NULL, stop, reason);
    if(mode == BUSY) { assert(net_default_dev == &ppp_link); net_default_dev = NULL; }
    bool success = mode == OK || mode == TX_STALL;
    assert((result == 1) == success && (reason[0] == 0) == success);
    assert(dials == (mode == OK || mode == TX_STALL || mode == NO_ANSWER || mode == NO_DNS ||
        mode == TIMEOUT_PPP || mode == CANCEL_CALL || mode == CANCEL_PPP));
    assert(release_count == (mode != BUSY));
    if(success) {
        assert(kui_dreampi_online());
        assert(kui_dreampi_open(NULL, stop, reason) < 0 && kui_dreampi_online());
        uint8_t frame[3] = {1,2,3};
        assert((attached->tx(attached, frame, sizeof(frame), 0) == 0) == (mode == OK));
        if(mode == TX_STALL) assert(!kui_dreampi_online() && elapsed < 6000);
    }
    kui_dreampi_close(); kui_dreampi_close();
    assert(!have_ppp && !have_modem && !have_stack && !net_default_dev && !kui_dreampi_online());
}
int main(void) {
    for(int mode_id = OK; mode_id <= TX_STALL; ++mode_id) {
        run((enum scenario)mode_id); run(OK);
    }
    puts("DreamPi call, negotiation, timeout, cancellation, teardown and redial checks passed");
    return 0;
}
