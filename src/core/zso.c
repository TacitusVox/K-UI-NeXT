/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/zso.h"
#if KUI_ZSO_CODECS & 2
#include "kui/lzo1x.h"
#endif
#if KUI_ZSO_CODECS & 4
#include "kui/deflate.h"
#endif
#include <string.h>

/* Independent implementation of the ZSO container and LZ4 block formats:
 * unknownbrackets/maxcso README_ZSO.md; lz4/lz4 doc/lz4_Block_format.md.
 * No frame decoder, dependent dictionary or codec guessing. The historical
 * LZO1X dialect uses its own independently implemented streaming decoder. */
static uint32_t little32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int valid(const struct kui_zso_info *info) {
    return info && info->total_bytes && info->total_bytes <= KUI_ZSO_MAX_BYTES &&
        !(info->total_bytes % KUI_ZSO_BLOCK_BYTES) &&
        info->block_count == info->total_bytes / KUI_ZSO_BLOCK_BYTES &&
        info->index_bytes == (info->block_count + 1u) * 4u &&
        info->shift <= KUI_ZSO_MAX_SHIFT && info->codec <= KUI_ZSO_DEFLATE &&
        (KUI_ZSO_CODECS & (1u << info->codec)) &&
        info->file_bytes >= KUI_ZSO_HEADER_BYTES + info->index_bytes;
}
enum kui_zso_result kui_zso_header_parse(const uint8_t header[KUI_ZSO_HEADER_BYTES],
    uint64_t file_bytes, struct kui_zso_info *info) {
    return kui_zso_header_parse_codec(header, file_bytes, KUI_ZSO_LZ4, info);
}
enum kui_zso_result kui_zso_header_parse_codec(const uint8_t header[KUI_ZSO_HEADER_BYTES],
    uint64_t file_bytes, enum kui_zso_codec codec, struct kui_zso_info *info) {
    if(!header || !info) return KUI_ZSO_INVALID;
    if((unsigned)codec > KUI_ZSO_DEFLATE || !(KUI_ZSO_CODECS & (1u << codec)))
        return KUI_ZSO_UNSUPPORTED;
    if(memcmp(header, codec == KUI_ZSO_DEFLATE ? "CISO" : "ZISO", 4) || header[20] > 1u ||
       (codec != KUI_ZSO_LZO1X && (little32(header + 4) != KUI_ZSO_HEADER_BYTES || header[20] != 1u)) ||
       little32(header + 16) != KUI_ZSO_BLOCK_BYTES ||
       header[21] > KUI_ZSO_MAX_SHIFT) return KUI_ZSO_UNSUPPORTED;
    if(codec != KUI_ZSO_LZO1X && (header[22] || header[23])) return KUI_ZSO_INVALID;
    uint64_t total = (uint64_t)little32(header + 8) | (uint64_t)little32(header + 12) << 32;
    if(!total || total % KUI_ZSO_BLOCK_BYTES) return KUI_ZSO_INVALID;
    if(total > KUI_ZSO_MAX_BYTES || file_bytes > UINT32_MAX) return KUI_ZSO_RANGE;
    struct kui_zso_info candidate = {
        .file_bytes = (uint32_t)file_bytes, .total_bytes = (uint32_t)total,
        .block_count = (uint32_t)(total / KUI_ZSO_BLOCK_BYTES), .shift = header[21], .codec = (uint8_t)codec
    };
    candidate.index_bytes = (candidate.block_count + 1u) * 4u;
    if(!valid(&candidate)) return KUI_ZSO_RANGE;
    *info = candidate;
    return KUI_ZSO_OK;
}
static enum kui_zso_result position(const struct kui_zso_info *info,
    uint32_t entry, uint32_t *offset) {
    uint32_t value = entry & UINT32_C(0x7fffffff);
    /* Equivalent to the wider shifted bound, without a 64-bit shift helper
     * and its resident call frame on SH-4. Callers have validated shift. */
    if(value > (UINT32_MAX >> info->shift)) return KUI_ZSO_RANGE;
    value <<= info->shift;
    if(value < KUI_ZSO_HEADER_BYTES + info->index_bytes || value > info->file_bytes)
        return KUI_ZSO_RANGE;
    *offset = value;
    return KUI_ZSO_OK;
}
static enum kui_zso_result pair(const struct kui_zso_info *info,
    uint32_t entry, uint32_t next, uint32_t *start, uint32_t *end) {
    enum kui_zso_result result = position(info, entry, start);
    if(result != KUI_ZSO_OK) return result;
    result = position(info, next, end);
    if(result != KUI_ZSO_OK) return result;
    if(*end <= *start || *end - *start > KUI_ZSO_MAX_SPAN) return KUI_ZSO_RANGE;
    if((entry & UINT32_C(0x80000000)) &&
       (*end - *start < KUI_ZSO_BLOCK_BYTES ||
        *end - *start >= KUI_ZSO_BLOCK_BYTES + (1u << info->shift)))
        return KUI_ZSO_RANGE;
    return KUI_ZSO_OK;
}
typedef uint32_t zso_word __attribute__((__may_alias__));
/* A build may set KUI_ZSO_WORD_COPY to 0 to copy with memcpy instead (word
 * stores only when source and destination share alignment): the low CSO
 * resident, whose raw blocks are its only bulk copies, has no room for this. */
#ifndef KUI_ZSO_WORD_COPY
#define KUI_ZSO_WORD_COPY 1
#endif
#if KUI_ZSO_WORD_COPY
/* Copy n bytes from a view into out with aligned word stores: the destination
 * is usually the game's uncached buffer, where every store is a bus
 * transaction. A misaligned source is assembled from its bytes, so no load
 * leaves the view. Kept out of line: as a leaf it needs no saved registers,
 * and its callers' frames stay small. */
__attribute__((noinline))
static void copy_out(uint8_t *out, const uint8_t *src, uint32_t n) {
    for(; n && ((uintptr_t)out & 3u); --n) *out++ = *src++;
    if(!((uintptr_t)src & 3u)) {
        for(; n >= 4u; n -= 4u, out += 4, src += 4)
            *(zso_word *)(void *)out = *(const zso_word *)(const void *)src;
    } else {
        for(; n >= 4u; n -= 4u, out += 4, src += 4) {
            uint32_t v = src[3];
            v = v << 8 | src[2];
            v = v << 8 | src[1];
            *(zso_word *)(void *)out = v << 8 | src[0];
        }
    }
    for(; n; --n) *out++ = *src++;
}
#else
#define copy_out memcpy
#endif
#if defined(KUI_RETAIL_ZSO) && KUI_RETAIL_ZSO
/* Detached disc requests are serialized with interrupts guarded. Keep
 * transient reader state in resident BSS to preserve the IRQ stack budget;
 * these profiles are nonreentrant, like their physical sector cache. One
 * cursor serves kui_zso_copy and kui_zso_read_block, which never nest. */
#if KUI_ZSO_CODECS & 4
static struct kui_deflate_workspace transient;
#define TRANSIENT_CURSOR (&transient.in)
#else
static struct kui_zso_cursor transient;
#define TRANSIENT_CURSOR (&transient)
#endif
#endif
enum kui_zso_result kui_zso_cursor_fill(struct kui_zso_cursor *c) {
    if(c->at >= c->end) return KUI_ZSO_INVALID; /* Stream runs past its range. */
    enum kui_zso_result result = c->view(c->context, c->at, c->end - c->at, &c->p, &c->got);
    /* Everything after the view is recomputed from c, so only c is held. */
    if(result == KUI_ZSO_OK && (!c->got || c->got > c->end - c->at)) result = KUI_ZSO_RANGE;
    if(result != KUI_ZSO_OK) { c->e = c->p = NULL; return result; }
    c->e = c->p + c->got;
    c->at += c->got;
    return KUI_ZSO_OK;
}
enum kui_zso_result kui_zso_cursor_take(struct kui_zso_cursor *c, void *out, uint32_t size) {
    uint8_t *to = out;
    while(size) {
        if(c->p == c->e) {
            enum kui_zso_result result = kui_zso_cursor_fill(c);
            if(result != KUI_ZSO_OK) return result;
        }
        /* Advance first, so only c, to and size live across the copy. */
        const uint8_t *from = c->p;
        uint32_t n = (uint32_t)(c->e - from);
        if(n > size) n = size;
        c->p = from + n; size -= n; to += n;
        copy_out(to - n, from, n);
    }
    return KUI_ZSO_OK;
}
enum kui_zso_result kui_zso_copy(kui_zso_view_fn view, void *context,
    uint32_t offset, void *out, uint32_t size) {
    if(size > UINT32_MAX - offset) return KUI_ZSO_RANGE;
#ifdef TRANSIENT_CURSOR
    struct kui_zso_cursor *c = TRANSIENT_CURSOR;
#else
    struct kui_zso_cursor local, *c = &local;
#endif
    *c = (struct kui_zso_cursor){view, context, NULL, NULL, offset, offset + size, 0};
    return kui_zso_cursor_take(c, out, size);
}
enum kui_zso_result kui_zso_validate_index(const struct kui_zso_info *info,
    kui_zso_view_fn view, void *context) {
    if(!valid(info) || !view) return KUI_ZSO_INVALID;
    /* Preparation/stage only. The detached reader needs only its chosen pair.
     * Entries are consumed in place; one split across two views is copied. */
    uint32_t previous = 0, terminal = 0, available = 0;
    uint32_t at = KUI_ZSO_HEADER_BYTES, end = at + info->index_bytes;
    const uint8_t *data = NULL;
    for(uint32_t i = 0; i <= info->block_count; ++i, at += 4u) {
        enum kui_zso_result result;
        if(available < 4u) {
            result = view(context, at, end - at, &data, &available);
            if(result != KUI_ZSO_OK) return result;
            if(!available || available > end - at) return KUI_ZSO_RANGE;
        }
        uint8_t split[4];
        const uint8_t *bytes = data;
        if(available >= 4u) { data += 4; available -= 4u; }
        else {
            result = kui_zso_copy(view, context, at, split, 4u);
            if(result != KUI_ZSO_OK) return result;
            bytes = split; available = 0;
        }
        uint32_t entry = little32(bytes), start;
        if(i) result = pair(info, previous, entry, &start, &terminal);
        else result = position(info, entry, &terminal);
        if(result != KUI_ZSO_OK) return result;
        previous = entry;
    }
    return info->file_bytes - terminal < (1u << info->shift) ? KUI_ZSO_OK : KUI_ZSO_RANGE;
}
#if KUI_ZSO_CODECS & 1
#define NEXT(value) do { \
        if(in->p == in->e && (result = kui_zso_cursor_fill(in)) != KUI_ZSO_OK) return result; \
        (value) = *in->p++; \
    } while(0)
/* Overlap-exact match copy (distance >= 1 already checked): distance one is a
 * byte fill and multiples of four copy aligned words; anything else copies
 * bytes in increasing order, which also implements overlap. */
static void match_copy(uint8_t *out, uint32_t distance, uint32_t count) {
    const uint8_t *from = out - distance;
    if(distance == 1u) {
        uint32_t value = *from;
        for(; count && ((uintptr_t)out & 3u); --count) *out++ = (uint8_t)value;
        zso_word *to = (zso_word *)(void *)out;
        for(value *= UINT32_C(0x01010101); count >= 4u; count -= 4u) *to++ = value;
        for(out = (uint8_t *)to; count; --count) *out++ = (uint8_t)value;
        return;
    }
    if(!(distance & 3u)) {
        for(; count && ((uintptr_t)out & 3u); --count) *out++ = *from++;
        zso_word *to = (zso_word *)(void *)out;
        const zso_word *word = (const zso_word *)(const void *)from;
        for(; count >= 4u; count -= 4u) *to++ = *word++;
        out = (uint8_t *)to; from = (const uint8_t *)word;
    }
    for(; count >= 4u; count -= 4u, out += 4, from += 4) {
        out[0] = from[0]; out[1] = from[1]; out[2] = from[2]; out[3] = from[3];
    }
    while(count--) *out++ = *from++;
}
#endif
enum kui_zso_result kui_zso_read_block(const struct kui_zso_info *info,
    uint32_t block, kui_zso_view_fn view, void *context, uint8_t out[KUI_ZSO_BLOCK_BYTES]) {
    if(!valid(info) || !view || !out) return KUI_ZSO_INVALID;
    if(block >= info->block_count) return KUI_ZSO_RANGE;
    /* One cursor reads the entry pair, then the block's own bytes. A DEFLATE
     * build decodes from the workspace's own cursor. */
#ifdef TRANSIENT_CURSOR
    struct kui_zso_cursor *const in = TRANSIENT_CURSOR;
#if KUI_ZSO_CODECS & 4
    struct kui_deflate_workspace *const workspace = &transient;
#endif
#elif KUI_ZSO_CODECS & 4
    struct kui_deflate_workspace local, *const workspace = &local;
    struct kui_zso_cursor *const in = &local.in;
#else
    struct kui_zso_cursor local, *const in = &local;
#endif
    uint32_t at = KUI_ZSO_HEADER_BYTES + block * 4u;
    *in = (struct kui_zso_cursor){view, context, NULL, NULL, at, at + 8u, 0};
    /* The pair is staged in out, which the block then overwrites. */
    enum kui_zso_result result = kui_zso_cursor_take(in, out, 8u);
    if(result != KUI_ZSO_OK) return result;
    uint32_t entry = little32(out), start, end;
    result = pair(info, entry, little32(out + 4), &start, &end);
    if(result != KUI_ZSO_OK) return result;
    in->p = in->e = NULL; in->at = start; in->end = end;
    if(entry & UINT32_C(0x80000000)) return kui_zso_cursor_take(in, out, KUI_ZSO_BLOCK_BYTES);
#if KUI_ZSO_CODECS & 2
    if(info->codec == KUI_ZSO_LZO1X) {
        uint32_t consumed;
        result = kui_lzo1x_decode(in, out, &consumed);
        if(result != KUI_ZSO_OK) return result;
        return end - start - consumed < (1u << info->shift) ? KUI_ZSO_OK : KUI_ZSO_INVALID;
    }
#endif
#if KUI_ZSO_CODECS & 4
    if(info->codec == KUI_ZSO_DEFLATE) {
        uint32_t consumed;
        result = kui_deflate_decode(workspace, out, &consumed); /* in is workspace->in. */
        if(result != KUI_ZSO_OK) return result;
        return end - start - consumed < (1u << info->shift) ? KUI_ZSO_OK : KUI_ZSO_INVALID;
    }
#endif
#if KUI_ZSO_CODECS & 1
    /* Tokens and lengths are read in place from the source's views; literal
     * runs are copied straight from them. */
    uint32_t done = 0, last_match = UINT32_MAX;
    for(;;) {
        uint32_t token, literals, extra;
        NEXT(token);
        literals = token >> 4;
        if(literals == 15u) {
            do {
                NEXT(extra);
                if(extra > KUI_ZSO_BLOCK_BYTES - literals) return KUI_ZSO_INVALID;
                literals += extra;
            } while(extra == 255u);
        }
        if(literals > KUI_ZSO_BLOCK_BYTES - done) return KUI_ZSO_INVALID;
        result = kui_zso_cursor_take(in, out + done, literals);
        if(result != KUI_ZSO_OK) return result;
        done += literals;
        if(done == KUI_ZSO_BLOCK_BYTES) {
            /* Unread input is the view's rest and the range after it. */
            if(literals < 5u || (last_match != UINT32_MAX && last_match > KUI_ZSO_BLOCK_BYTES - 12u) ||
               in->end - in->at + kui_zso_cursor_buffered(in) >= (1u << info->shift)) return KUI_ZSO_INVALID;
            return KUI_ZSO_OK;
        }
        uint32_t low, high, count;
        NEXT(low);
        NEXT(high);
        uint32_t distance = low | high << 8;
        if(!distance || distance > done) return KUI_ZSO_INVALID;
        count = token & 15u;
        if(count == 15u) {
            do {
                NEXT(extra);
                if(extra > KUI_ZSO_BLOCK_BYTES - count) return KUI_ZSO_INVALID;
                count += extra;
            } while(extra == 255u);
        }
        count += 4u;
        if(count > KUI_ZSO_BLOCK_BYTES - done) return KUI_ZSO_INVALID;
        last_match = done;
        match_copy(out + done, distance, count);
        done += count;
    }
#else
    return KUI_ZSO_UNSUPPORTED;
#endif
}
const char *kui_zso_result_name(enum kui_zso_result result) {
    switch(result) {
    case KUI_ZSO_OK: return "OK";
    case KUI_ZSO_INVALID: return "Invalid ZSO/CSO block or header";
    case KUI_ZSO_UNSUPPORTED: return "Unsupported ZSO/CSO format or codec; independent 2048-byte blocks required";
    case KUI_ZSO_IO: return "ZSO/CSO read failed";
    case KUI_ZSO_CANCELLED: return "Cancelled";
    case KUI_ZSO_RANGE: return "ZSO/CSO container outside admitted bounds";
    default: return "Unknown ZSO/CSO error";
    }
}
