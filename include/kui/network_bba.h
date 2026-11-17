/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NETWORK_BBA_H
#define KUI_NETWORK_BBA_H
#include "kui/apps.h"
#include "kui/net.h"

/* An explicit expansion-port TCP session. open returns 1 when online,
 * 0 when no adapter is present, and -1 on error/cancellation. On failure
 * everything acquired is released before returning. */
struct kui_bba_session {
    void *driver;
    struct kui_net_sockets net;
    void (*reuse_listener)(void *ctx, uint16_t port);
    uint8_t ip[4], mac[6];
    char name[32], problem[96];
};
struct kui_bba_port {
    int (*open)(struct kui_bba_session *s, kui_log_fn log, kui_cancel_fn cancel);
    bool (*poll)(struct kui_bba_session *s);
    void (*end)(struct kui_bba_session *s);
    uint64_t (*now_ms)(void);
    void (*pause)(unsigned ms);
};
const struct kui_bba_port *kui_bba_console_port(void);
#endif
