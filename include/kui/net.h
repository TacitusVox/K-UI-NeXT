/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NET_H
#define KUI_NET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Numbered TCP slots for native BBA sockets, the W5500 and the Wi-Fi board.
 * The state values follow the W5500 and the Wi-Fi board's protocol. */
#define KUI_NET_CLOSED 0x00u
#define KUI_NET_LISTEN 0x14u
#define KUI_NET_CONNECTING 0x15u
#define KUI_NET_ESTABLISHED 0x17u
#define KUI_NET_CLOSING 0x18u     /* our FIN is queued or sent */
#define KUI_NET_PEER_CLOSED 0x1cu /* the peer's FIN arrived; we may still send */

struct kui_net_sockets {
    void *ctx;
    unsigned count;
    /* TCP with no delay (every segment acknowledged at once). Opening a
     * socket closes whatever it held. */
    bool (*listen)(void *ctx, unsigned s, uint16_t port);
    /* From local port `local`; an adapter that picks its own may ignore it. */
    bool (*connect)(void *ctx, unsigned s, uint16_t local, const uint8_t ip[4], uint16_t port);
    /* Orderly close: FIN once everything sent has gone. */
    bool (*disconnect)(void *ctx, unsigned s);
    /* Closed at once; unsent and unread data is dropped. */
    bool (*close)(void *ctx, unsigned s);
    bool (*state)(void *ctx, unsigned s, uint8_t *state);
    bool (*peer)(void *ctx, unsigned s, uint8_t ip[4], uint16_t *port);
    /* Optional. Seconds of silence before TCP keep-alive probes. */
    bool (*keepalive)(void *ctx, unsigned s, unsigned seconds);
    /* Bytes waiting, and taking `bytes` of them (at most that many). */
    bool (*received)(void *ctx, unsigned s, size_t *bytes);
    bool (*receive)(void *ctx, unsigned s, void *data, size_t bytes);
    /* Bytes a send takes now (0 while the adapter's buffer is full), and
     * sending at most that many. False: the connection has failed. */
    bool (*room)(void *ctx, unsigned s, size_t *bytes);
    bool (*send)(void *ctx, unsigned s, const void *data, size_t bytes);
    /* Everything sent so far has left the adapter: acknowledged by the peer
     * (W5500), or handed to the adapter's TCP stack (Wi-Fi and BBA). */
    bool (*sent)(void *ctx, unsigned s, bool *all);
};

/* Services probe the expansion-port BBA first. On the SCI port, always
 * probe W5500 before Wi-Fi. Each unavailable port may be NULL. */
struct kui_w5500_port;
struct kui_wifi_port;
struct kui_bba_port;
struct kui_net_ports {
    const struct kui_w5500_port *w5500;
    const struct kui_wifi_port *wifi;
    const struct kui_bba_port *bba;
};
#endif
