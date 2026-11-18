/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_PREVIEW_PREFIX_H
#define KUI_PREVIEW_PREFIX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Start zeroed. Track Ogg page boundaries independently of HTTP/TLS chunks. */
struct kui_preview_prefix {
    uint8_t header[27 + 255], identification[42];
    unsigned header_bytes, remaining, body_bytes;
    uint32_t serial, next_page, target_packets, packets;
    bool video, disabled, complete;
};

/* Number of bytes of this chunk to stage. complete is set at the end of the
 * first intact sequence of Theora pages containing ten seconds of packets.
 * The real decoder still verifies Ogg CRCs and produces the installed file. */
size_t kui_preview_prefix_consume(struct kui_preview_prefix *p, const void *bytes, size_t size);
#endif
