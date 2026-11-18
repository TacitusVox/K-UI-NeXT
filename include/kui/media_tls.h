// SPDX-License-Identifier: GPL-3.0-only
#ifndef KUI_MEDIA_TLS_H
#define KUI_MEDIA_TLS_H
#include <stddef.h>

typedef struct kui_media_tls kui_media_tls_t;

/* Each handle owns its TLS record buffer. The caller owns the TCP socket,
 * timeout, cancellation, and scheduling. Call only after choosing HTTPS. */
kui_media_tls_t *kui_media_tls_open(const char *hostname);
void kui_media_tls_free(kui_media_tls_t *tls);

/* Process at most one wire operation. Callbacks return bytes, 0 for would
 * block, -2 for EOF, or -1 on error. pump returns 1 when it advanced,
 * 0 when waiting, -2 on TLS close_notify, and -1 on error (including an
 * unauthenticated TCP EOF). Check read() before pumping the next record. */
int kui_media_tls_pump(kui_media_tls_t *tls, void *ctx,
                       int (*recv_wire)(void *, void *, size_t),
                       int (*send_wire)(void *, const void *, size_t));
/* Copy available authenticated bytes/space, or return 0 when waiting. */
int kui_media_tls_read(kui_media_tls_t *tls, void *dest, size_t size);
int kui_media_tls_write(kui_media_tls_t *tls, const void *source, size_t size);
const char *kui_media_tls_error(const kui_media_tls_t *tls);

#endif
