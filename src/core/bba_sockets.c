/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/bba_sockets.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SLOT_COUNT 8
#define BLOCK_BYTES 4096

/* The shared K-UI API promises exact-sized transfers once it advertises
 * space/data. TCP may short-write or short-read. Stage both directions. */
struct slot {
    int fd;
    uint16_t listening;
    uint8_t state;
    struct sockaddr_in peer;
    size_t rx_at, rx_end, tx_at, tx_end;
    unsigned char rx[BLOCK_BYTES], tx[BLOCK_BYTES];
};
struct listener { int fd; uint16_t port; };
struct table {
    struct slot slots[SLOT_COUNT];
    struct listener listeners[SLOT_COUNT + 2];
    uint16_t permanent[2];
};

static bool later(void) { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR; }

static int nonblocking_fd(int fd) {
    if(fd < 0) return -1;
    if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }
    int yes = 1;
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    return fd;
}

static struct listener *lookup(struct table *t, uint16_t port) {
    for(unsigned i = 0; i < SLOT_COUNT + 2; ++i)
        if(t->listeners[i].port == port) return &t->listeners[i];
    return NULL;
}

static void release_listener(struct table *t, struct slot *s) {
    uint16_t port = s->listening;
    s->listening = 0;
    if(!port || port == t->permanent[0] || port == t->permanent[1]) return;
    for(unsigned i = 0; i < SLOT_COUNT; ++i)
        if(t->slots[i].listening == port) return;
    struct listener *l = lookup(t, port);
    if(l) {
        close(l->fd);
        l->port = 0;
    }
}

static bool close_slot(void *ctx, unsigned id) {
    struct table *t = ctx;
    if(id >= SLOT_COUNT) return false;
    struct slot *s = &t->slots[id];
    release_listener(t, s);
    if(s->fd >= 0) close(s->fd);
    s->fd = -1;
    s->state = KUI_NET_CLOSED;
    s->rx_at = s->rx_end = s->tx_at = s->tx_end = 0;
    return true;
}

static bool listen_slot(void *ctx, unsigned id, uint16_t port) {
    struct table *t = ctx;
    if(!port || !close_slot(ctx, id)) return false;
    struct listener *l = lookup(t, port);
    /* Only the service's control listeners may share a port. A passive
     * data listener must accept connections for exactly one FTP client. */
    if(l && port != t->permanent[0] && port != t->permanent[1]) {
        errno = EADDRINUSE;
        return false;
    }
    if(!l) {
        l = lookup(t, 0);
        if(!l) { errno = ENOBUFS; return false; }
        int fd = nonblocking_fd(socket(AF_INET, SOCK_STREAM, 0));
        if(fd < 0) return false;
        int yes = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
        if(bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, SLOT_COUNT) < 0) {
            int error = errno;
            close(fd);
            errno = error;
            return false;
        }
        l->fd = fd;
        l->port = port;
    }
    t->slots[id].listening = port;
    t->slots[id].state = KUI_NET_LISTEN;
    return true;
}

static bool connect_slot(void *ctx, unsigned id, uint16_t local, const uint8_t ip[4], uint16_t port) {
    (void)local;
    if(!ip || !port || !close_slot(ctx, id)) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    int fd = nonblocking_fd(socket(AF_INET, SOCK_STREAM, 0));
    if(fd < 0) return false;
    s->peer = (struct sockaddr_in){.sin_family = AF_INET, .sin_port = htons(port)};
    memcpy(&s->peer.sin_addr, ip, 4);
    int r = connect(fd, (const struct sockaddr *)&s->peer, sizeof(s->peer));
    if(r < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
        close(fd);
        return false;
    }
    s->fd = fd;
    s->state = r == 0 ? KUI_NET_ESTABLISHED : KUI_NET_CONNECTING;
    return true;
}

static bool flush(struct slot *s) {
    if(s->tx_at == s->tx_end) return true;
#ifdef MSG_NOSIGNAL
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    ssize_t n = send(s->fd, s->tx + s->tx_at, s->tx_end - s->tx_at, flags);
    if(n < 0) return later();
    if(!n) return false;
    s->tx_at += (size_t)n;
    if(s->tx_at == s->tx_end) s->tx_at = s->tx_end = 0;
    return true;
}

static bool fill(struct slot *s) {
    if(s->rx_at < s->rx_end || s->state == KUI_NET_PEER_CLOSED) return true;
    ssize_t n = recv(s->fd, s->rx, sizeof(s->rx), 0);
    if(n < 0) return later();
    s->rx_at = 0;
    s->rx_end = (size_t)n;
    if(!n) s->state = KUI_NET_PEER_CLOSED;
    return true;
}

static bool state(void *ctx, unsigned id, uint8_t *result) {
    struct table *t = ctx;
    if(id >= SLOT_COUNT || !result) return false;
    struct slot *s = &t->slots[id];
    if(s->state == KUI_NET_LISTEN) {
        struct listener *l = lookup(t, s->listening);
        if(!l) return false;
        socklen_t len = sizeof(s->peer);
        int fd = accept(l->fd, (struct sockaddr *)&s->peer, &len);
        if(fd >= 0) {
            s->fd = nonblocking_fd(fd);
            if(s->fd < 0) return false;
            release_listener(t, s);
            s->state = KUI_NET_ESTABLISHED;
        } else if(!later()) return false;
    } else if(s->state == KUI_NET_CONNECTING) {
        struct pollfd p = {.fd = s->fd, .events = POLLOUT};
        int ready = poll(&p, 1, 0);
        if(ready < 0 && !later()) return false;
        if(ready > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            /* KOS reports a reset via POLLHUP; desktop TCP also has SO_ERROR. */
            if((p.revents & (POLLERR | POLLHUP | POLLNVAL)) ||
               getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err)
                close_slot(ctx, id);
            else if(p.revents & POLLOUT) s->state = KUI_NET_ESTABLISHED;
        }
    }
    if(s->state == KUI_NET_CLOSING) {
        if(!flush(s) || !s->tx_end) close_slot(ctx, id);
    } else if(s->state == KUI_NET_ESTABLISHED || s->state == KUI_NET_PEER_CLOSED) {
        if(!flush(s) || !fill(s)) close_slot(ctx, id);
    }
    *result = s->state;
    return true;
}

static bool peer(void *ctx, unsigned id, uint8_t ip[4], uint16_t *port) {
    if(id >= SLOT_COUNT || !ip || !port) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(s->fd < 0) return false;
    memcpy(ip, &s->peer.sin_addr, 4);
    *port = ntohs(s->peer.sin_port);
    return true;
}

#ifndef __DREAMCAST__
static bool keepalive(void *ctx, unsigned id, unsigned seconds) {
    if(id >= SLOT_COUNT) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    int enable = seconds != 0;
    if(s->fd < 0 || setsockopt(s->fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) return false;
#ifdef TCP_KEEPIDLE
    if(enable && setsockopt(s->fd, IPPROTO_TCP, TCP_KEEPIDLE, &seconds, sizeof(seconds)) < 0) return false;
#endif
    return true;
}

#endif

static bool received(void *ctx, unsigned id, size_t *bytes) {
    if(id >= SLOT_COUNT || !bytes) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(s->fd < 0 || !fill(s)) return false;
    *bytes = s->rx_end - s->rx_at;
    return true;
}

static bool receive_slot(void *ctx, unsigned id, void *data, size_t bytes) {
    if(id >= SLOT_COUNT) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(bytes > s->rx_end - s->rx_at) return false;
    memcpy(data, s->rx + s->rx_at, bytes);
    s->rx_at += bytes;
    return true;
}

static bool room(void *ctx, unsigned id, size_t *bytes) {
    if(id >= SLOT_COUNT || !bytes) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(s->fd < 0 || !flush(s)) return false;
    *bytes = sizeof(s->tx) - s->tx_end;
    return true;
}

static bool send_slot(void *ctx, unsigned id, const void *data, size_t bytes) {
    if(id >= SLOT_COUNT) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(s->fd < 0 || bytes > sizeof(s->tx) - s->tx_end) return false;
    memcpy(s->tx + s->tx_end, data, bytes);
    s->tx_end += bytes;
    return flush(s);
}

static bool sent(void *ctx, unsigned id, bool *all) {
    size_t bytes;
    if(!room(ctx, id, &bytes)) return false;
    *all = bytes == BLOCK_BYTES;
    return true;
}

static bool disconnect_slot(void *ctx, unsigned id) {
    if(id >= SLOT_COUNT) return false;
    struct slot *s = &((struct table *)ctx)->slots[id];
    if(s->fd < 0 || !s->tx_end) return close_slot(ctx, id);
    s->state = KUI_NET_CLOSING;
    return true;
}

void kui_bba_sockets_reuse_listener(void *ctx, uint16_t port) {
    struct table *t = ctx;
    for(unsigned i = 0; i < 2; ++i) {
        if(t->permanent[i] == port) return;
        if(!t->permanent[i]) { t->permanent[i] = port; return; }
    }
}

bool kui_bba_sockets_init(struct kui_net_sockets *out) {
    struct table *t = calloc(1, sizeof(*t));
    if(!t) return false;
    for(unsigned i = 0; i < SLOT_COUNT; ++i) t->slots[i].fd = -1;
    *out = (struct kui_net_sockets){t, SLOT_COUNT, listen_slot, connect_slot, disconnect_slot,
        close_slot, state, peer,
#ifdef __DREAMCAST__
        NULL, /* Pinned KOS does not implement TCP keepalive. */
#else
        keepalive,
#endif
        received, receive_slot, room, send_slot, sent};
    return true;
}

void kui_bba_sockets_end(struct kui_net_sockets *net) {
    struct table *t = net->ctx;
    if(!t) return;
    for(unsigned i = 0; i < SLOT_COUNT; ++i) close_slot(t, i);
    for(unsigned i = 0; i < SLOT_COUNT + 2; ++i)
        if(t->listeners[i].port) close(t->listeners[i].fd);
    free(t);
    memset(net, 0, sizeof(*net));
}
