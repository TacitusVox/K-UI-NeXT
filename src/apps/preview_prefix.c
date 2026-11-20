/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/preview_prefix.h"
#include <string.h>

static uint32_t le32(const uint8_t *b) {
    return (uint32_t)b[0] | (uint32_t)b[1]<<8 | (uint32_t)b[2]<<16 | (uint32_t)b[3]<<24;
}
static uint32_t be32(const uint8_t *b) {
    return (uint32_t)b[0]<<24 | (uint32_t)b[1]<<16 | (uint32_t)b[2]<<8 | (uint32_t)b[3];
}

static void finish_page(struct kui_preview_prefix *p) {
    const uint8_t *h = p->header;
    uint32_t serial = le32(h + 14), seq = le32(h + 18);
    if(!p->video && (h[5] & 2u) && !seq && h[26] && h[27] == 42 &&
       p->body_bytes >= 42 && !memcmp(p->identification, "\x80theora", 7)) {
        uint32_t fps = be32(p->identification + 22), divisor = be32(p->identification + 26);
        if(fps && divisor && (uint64_t)fps <= (uint64_t)divisor * 120) {
            p->video = true;
            p->serial = serial;
            /* Three headers precede the video packets. Empty packets are
             * duplicate frames and also count towards presentation time. */
            p->target_packets = 3u + (uint32_t)(((uint64_t)fps * (p->start_seconds + 10u) + divisor - 1) / divisor);
        }
    }
    if(!p->audio_rate && (h[5] & 2u) && !seq && p->body_bytes >= 30 &&
       !memcmp(p->identification,"\1vorbis",7) &&
       p->identification[11] && p->identification[11] <= 2) {
        uint32_t rate = le32(p->identification+12);
        if(rate >= 8000u && rate <= 48000u) {
            p->audio_serial = serial;
            p->audio_rate = rate;
        }
    }
    if(p->audio_rate && serial == p->audio_serial) {
        uint64_t pos = le32(h+6) | (uint64_t)le32(h+10)<<32;
        if((h[5] & 4u) || (pos <= INT64_MAX && pos >= p->audio_rate * (p->start_seconds + 10u))) p->audio_done = true;
    }
    if(p->video && serial == p->serial) {
        if(seq != p->next_page) { p->disabled = true; return; }
        ++p->next_page;
        for(unsigned i = 0; i < h[26]; ++i)
            if(h[27+i] < 255) ++p->packets;

    }
    p->complete = p->video && p->packets >= p->target_packets && (!p->audio_rate || p->audio_done);
    p->header_bytes = p->body_bytes = 0;
}

size_t kui_preview_prefix_consume(struct kui_preview_prefix *p, const void *bytes, size_t size) {
    const uint8_t *b = bytes;
    size_t at = 0;
    if(p->complete) return 0;
    while(at < size && !p->disabled) {
        if(!p->remaining) {
            unsigned header_size = p->header_bytes < 27 ? 27u : 27u + p->header[26];
            size_t n = header_size - p->header_bytes;
            if(n > size-at) n = size-at;
            memcpy(p->header + p->header_bytes, b+at, n);
            at += n;
            p->header_bytes += (unsigned)n;
            if(p->header_bytes < 27) continue;
            if(memcmp(p->header, "OggS", 4) || p->header[4]) { p->disabled = true; break; }
            if(p->header_bytes < 27u + p->header[26]) continue;
            for(unsigned i = 0; i < p->header[26]; ++i) p->remaining += p->header[27+i];
        }
        size_t n = p->remaining;
        if(n > size-at) n = size-at;
        if((p->header[5] & 2u) && p->body_bytes < sizeof(p->identification)) {
            size_t save = sizeof(p->identification) - p->body_bytes;
            if(save > n) save = n;
            memcpy(p->identification + p->body_bytes, b+at, save);
        }
        at += n;
        p->body_bytes += (unsigned)n;
        p->remaining -= (unsigned)n;
        if(!p->remaining) {
            finish_page(p);
            if(p->complete) return at;
        }
    }
    return size;
}
