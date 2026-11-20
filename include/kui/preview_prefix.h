/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_PREVIEW_PREFIX_H
#define KUI_PREVIEW_PREFIX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Curated trailers can skip a short publisher intro before the ten-second clip. */
#define KUI_PREVIEW_START_MAX_SECONDS 20u

/* Start zeroed, then set an optional curated start in whole seconds.
 * Track Ogg page boundaries independently of HTTP/TLS chunks. */
struct kui_preview_prefix {
    uint8_t header[27 + 255], identification[42];
    uint8_t start_seconds;
    unsigned header_bytes, remaining, body_bytes;
    uint32_t serial, next_page, target_packets, packets;
    uint32_t audio_serial, audio_rate;
    bool audio_done, video, disabled, complete;
};

/* Number of bytes of this chunk to stage. complete is set at the end of the
 * first intact sequence of Theora and supported Vorbis pages covering the selected ten-second window.
 * The real decoder still verifies Ogg CRCs and produces the installed file. */
size_t kui_preview_prefix_consume(struct kui_preview_prefix *p, const void *bytes, size_t size);
#endif
