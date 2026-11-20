/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_INTERNAL_H
#define KUI_INTERNET_INTERNAL_H
#include "kui/internet.h"
#include "kui/net.h"
#define KUI_HTTP_HOST 254u
#define KUI_HTTP_URL 3072u /* GitHub release redirects carry signed URLs. */
struct kui_internet {
    struct kui_net_sockets net;
    bool (*poll)(struct kui_internet *);
    bool (*resolve)(struct kui_internet *, const char *host, uint8_t ip[4], const uint8_t avoid[4]);
    uint64_t (*now)(void);
    void (*pause)(unsigned ms);
    kui_cancel_fn cancel;
    kui_log_fn log;
    unsigned next_port;
    const char *adapter;
    char problem[128];
};
size_t kui_dns_query(uint8_t *packet, size_t capacity, uint16_t id, const char *host);
bool kui_dns_answer(const uint8_t *packet, size_t length, uint16_t id, const char *host, uint8_t ip[4], uint32_t *ttl,
                    const uint8_t avoid[4]);
#endif
