/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_DEFLATE_H
#define KUI_DEFLATE_H
#include "kui/zso.h"

/* Literal/length symbols per rank bucket, and buckets with stored counts
 * (symbols 0..255; the last bucket, 256..287, needs no row). A build may set
 * KUI_DEFLATE_BUCKETS to 0: rank scans then always start at symbol 0, which
 * is slower on large dynamic trees but needs 128 bytes less workspace. The
 * low CSO resident does this, as its reservation has no room for the rows. */
#define KUI_DEFLATE_BUCKET 32u
#ifndef KUI_DEFLATE_BUCKETS
#define KUI_DEFLATE_BUCKETS 8u
#endif
_Static_assert(KUI_DEFLATE_BUCKETS == 0u || KUI_DEFLATE_BUCKETS * KUI_DEFLATE_BUCKET == 256u,
               "DEFLATE rank buckets cover literals 0..255 exactly, or are absent");

/* Caller-owned temporary state, reusable after each call. Packed 4-bit code
 * widths keep the detached reader's memory bounded without a heap, symbol
 * tables or a 32 KiB window. The input cursor, the bit buffer and the sticky
 * source error live here too, so the decoder's own stack frames stay small.
 * The cursor's view and context are cleared before return; no source context,
 * output pointer or decoded data is retained. */
struct kui_deflate_workspace {
    struct kui_zso_cursor in;
    uint32_t start;         /* File offset where this stream began. */
    uint32_t final;         /* The current DEFLATE block is the last. */
    /* Packed widths of the current dynamic block (literal/length, then
     * distance at +36 words); NULL selects the fixed codes. */
    const uint32_t *tree;
    uint32_t over;          /* Zero bytes supplied past the bound. */
    uint32_t hold, bits;    /* LSB-first bit buffer. */
    enum kui_zso_result error;
    /* Literal/length and distance trees: per-length counts, then the largest
     * canonical code of each length (index 0 holds -1). */
    int16_t limit[2][16];
    struct { int16_t limit[8]; uint32_t widths[4]; } lengths; /* Code-length code. */
    uint32_t widths[40];    /* Literal/length 0..287, then distance 288..319. */
#if KUI_DEFLATE_BUCKETS
    uint8_t bucket[KUI_DEFLATE_BUCKETS][16]; /* Widths per bucket, per length. */
#endif
};

/* Decode one independent raw RFC 1951 DEFLATE stream to exactly 2048 bytes
 * from workspace->in, which the caller sets first; its remaining range holds
 * 1..KUI_ZSO_MAX_SPAN bytes. Stored, fixed and dynamic Huffman blocks,
 * including multiple DEFLATE blocks, are supported. Every view stays inside
 * that range, and matches can use only this call's output. On success
 * consumed includes the byte containing the final end-of-block bit, leaving
 * container alignment padding for the caller to check. Remaining bits in that
 * byte are ignored as RFC 1951 permits. The bit buffer may look up to three
 * bytes past that byte, always inside the range. All pointers are required;
 * workspace must not overlap out. On failure consumed is unchanged and
 * partially changed out is invalid; a source error is returned unchanged.
 * Calls using the same workspace must not overlap. Little-endian targets only
 * (SH-4 -ml and the host test builds). */
enum kui_zso_result kui_deflate_decode(struct kui_deflate_workspace *workspace,
    uint8_t out[KUI_ZSO_BLOCK_BYTES], uint32_t *consumed);
#endif
