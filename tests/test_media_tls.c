// SPDX-License-Identifier: GPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "kui/media_tls.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* The epoch override belongs to this binary only. */
time_t __real_time(time_t *);
time_t __wrap_time(time_t *value) {
    const char *override = getenv("KUI_TEST_TLS_TIME");
    if(!override) return __real_time(value);
    time_t result = (time_t)strtoll(override, NULL, 10);
    if(value) *value = result;
    return result;
}

struct socket_io { int fd; unsigned operations; };

static int recv_wire(void *ctx, void *buffer, size_t length) {
    struct socket_io *io = ctx;
    if(++io->operations % 5 == 0) return 0;
    ssize_t n = recv(io->fd, buffer, length > 59 ? 59 : length, 0);
    if(n == 0) return -2;
    if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return (int)n;
}

static int send_wire(void *ctx, const void *buffer, size_t length) {
    struct socket_io *io = ctx;
    if(++io->operations % 5 == 0) return 0;
    ssize_t n = send(io->fd, buffer, length > 23 ? 23 : length, 0);
    if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return (int)n;
}

int main(int argc, char **argv) {
    assert(argc == 3);
    signal(SIGPIPE, SIG_IGN);
    struct socket_io io = { socket(AF_INET, SOCK_STREAM, 0), 0 };
    assert(io.fd >= 0);
    struct sockaddr_in addr = { .sin_family = AF_INET,
                               .sin_port = htons((uint16_t)atoi(argv[2])) };
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(io.fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(fcntl(io.fd, F_SETFL, O_NONBLOCK) == 0);
    kui_media_tls_t *tls = kui_media_tls_open(argv[1]);
    assert(tls);
    char request[400];
    int request_len = snprintf(request, sizeof(request),
                               "GET /cover HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                               argv[1]);
    int sent = 0;
    char response[231];
    int rc;
    do {
        sent += kui_media_tls_write(tls, request + sent, (size_t)(request_len - sent));
        int n;
        while((n = kui_media_tls_read(tls, response, sizeof(response))) > 0)
            assert(fwrite(response, 1, (size_t)n, stdout) == (size_t)n);
        rc = kui_media_tls_pump(tls, &io, recv_wire, send_wire);
        if(rc == 0) {
            struct timespec pause = { .tv_nsec = 100000 };
            nanosleep(&pause, NULL);
        }
    } while(rc >= 0);
    if(rc == -1) fprintf(stderr, "%s\n", kui_media_tls_error(tls));
    kui_media_tls_free(tls);
    close(io.fd);
    return rc == -2 ? 0 : 1;
}
