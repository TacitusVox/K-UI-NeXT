/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_BBA_SOCKETS_H
#define KUI_BBA_SOCKETS_H
#include "kui/net.h"
bool kui_bba_sockets_init(struct kui_net_sockets *out);
void kui_bba_sockets_end(struct kui_net_sockets *net);
/* Keep up to two long-lived service listeners until session end. KOS
 * rejects a re-bind while a child connection is in TIME_WAIT. */
void kui_bba_sockets_reuse_listener(void *ctx, uint16_t port);
#endif
