/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/deflate.h"

/* Independently implemented from RFC 1951 sections 3.2.1--3.2.7 and general
 * canonical-Huffman decoding, not from another decoder's source.
 *
 * Small-memory design for the detached reader: input bytes are taken in place
 * from the source's views into a 32-bit LSB-first bit buffer, refilled to at
 * least 25 bits, so one refill serves a whole symbol. A symbol's code is
 * found by a canonical walk over buffered bits (one compare per code length,
 * no call per bit). Its rank among codes of that length becomes the symbol by
 * scanning the packed 4-bit widths a word (eight widths) at a time; per-length
 * counts for each 32-symbol literal/length bucket let the scan skip whole
 * buckets (unless a build sets KUI_DEFLATE_BUCKETS to 0). Fixed codes map
 * ranks arithmetically. There are no symbol or lookup tables, which keeps the
 * workspace to the widths and a few counts.
 *
 * Source errors are sticky: zero bytes are supplied afterwards and decoding
 * stops at the next failed check, returning the source's code. Past the
 * stream bound up to four virtual zero bytes may be buffered; a stream that
 * consumes any of their bits is rejected, as reading past the bound is. */
#define BAD 0x7fffu
typedef uint32_t deflate_word __attribute__((__may_alias__));

/* Buffer at least 25 bits, then take n <= 16 of them. */
static unsigned bits(struct kui_deflate_workspace *w, unsigned n) {
    while(w->bits <= 24u) {
        uint32_t byte = 0;
        if(w->in.p != w->in.e) byte = *w->in.p++;
        else if(w->error == KUI_ZSO_OK) {
            if(w->in.at != w->in.end) {
                if((w->error = kui_zso_cursor_fill(&w->in)) == KUI_ZSO_OK) continue;
            } else if(++w->over > 4u) {
                /* At most 24 bits are buffered before a refill, so a fifth
                 * virtual byte proves that a virtual bit was consumed. */
                w->error = KUI_ZSO_INVALID;
            }
        }
        w->hold |= byte << w->bits;
        w->bits += 8u;
    }
    unsigned value = w->hold & ((1u << n) - 1u);
    w->hold >>= n;
    w->bits -= n;
    return value;
}
__attribute__((optimize("no-tree-loop-distribute-patterns")))
static void zero_words(void *target, unsigned words) {
    deflate_word *word = target;
    do *word++ = 0; while(--words);
}
static unsigned width(const uint32_t *widths, unsigned index) {
    return ((const uint8_t *)widths)[index >> 1] >> ((index & 1u) << 2) & 15u;
}
/* Validate per-length counts and turn them into the largest code of each
 * length (limit[0] = -1). Kind 0 is the code-length code, which must be
 * complete; 1 and 2 are literal/length and distance trees, which may hold a
 * single one-bit code; a distance tree may also be empty. */
static int canonical(int16_t *limit, unsigned maximum, unsigned kind) {
    unsigned left = 1u, total = 0u, code = 0u, single = (uint16_t)limit[1];
    limit[0] = -1;
    for(unsigned length = 1u; length <= maximum; ++length) {
        unsigned count = (uint16_t)limit[length];
        left <<= 1;
        if(count > left) return 0; /* Oversubscribed. */
        left -= count;
        total += count;
        code += count;
        limit[length] = (int16_t)(code - 1u);
        code <<= 1;
    }
    if(!total) return kind == 2u;
    return !left || (kind && total == 1u && single == 1u);
}
/* One symbol from at least 15 buffered bits: canonical walk, then rank to
 * symbol by scanning packed widths (widths == NULL selects the fixed codes,
 * mapped arithmetically). Leaf function. */
static unsigned decode(struct kui_deflate_workspace *w, const int16_t *limit,
                       const uint32_t *widths, const uint8_t *bucket) {
    uint32_t hold = w->hold, code = 0, lanes;
    unsigned length = 0;
    do {
        code = (code << 1) | (hold & 1u);
        hold >>= 1;
        if((int32_t)code <= limit[++length]) goto found;
    } while(length < 15u);
    return BAD;
found:
    w->hold = hold;
    w->bits -= length;
    code -= 2u * (unsigned)(limit[length - 1u] + 1); /* Rank within this length. */
    if(!widths) {
        if(length == 5u) return code;          /* Fixed distance codes. */
        if(length == 7u) return code + 256u;
        if(length == 9u) return code + 144u;
        return code < 144u ? code : code + 136u;
    }
    const uint32_t ones = UINT32_C(0x11111111), pattern = length * ones;
    unsigned symbol = 0;
#if KUI_DEFLATE_BUCKETS
    if(bucket) {
        bucket += length;
        for(unsigned row = 0; row < KUI_DEFLATE_BUCKETS && code >= *bucket; ++row, bucket += 16) {
            code -= *bucket;
            symbol += KUI_DEFLATE_BUCKET;
        }
        widths += symbol / 8u;
    }
#else
    (void)bucket;
#endif
    for(;;) {
        lanes = *widths++ ^ pattern;  /* Matching widths become zero nibbles. */
        lanes |= lanes >> 1;
        lanes |= lanes >> 2;
        lanes = ~lanes & ones;        /* Bit 0 of each matching nibble. */
        unsigned matches = (lanes * ones) >> 28;
        if(code < matches) break;
        code -= matches;
        symbol += 8u;
    }
    while(code--) lanes &= lanes - 1u; /* Drop earlier matches in this word. */
    /* Lowest remaining lane k: (16^k * 0x01234567) has k in its top nibble. */
    return symbol + (((lanes & (0u - lanes)) * UINT32_C(0x01234567)) >> 28);
}
/* decode() needs 15 buffered bits; refill only below that. */
#define NEED15(w) do { if((w)->bits < 15u) bits((w), 0u); } while(0)

enum kui_zso_result kui_deflate_decode(const struct kui_zso_cursor *in,
    uint8_t out[KUI_ZSO_BLOCK_BYTES], struct kui_deflate_workspace *w, uint32_t *consumed) {
    static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    /* The remaining range must hold 1..KUI_ZSO_MAX_SPAN bytes. */
    if(!in || !out || !w || !consumed || in->at > in->end ||
       in->end - in->at + (uint32_t)(in->e - in->p) - 1u >= KUI_ZSO_MAX_SPAN) return KUI_ZSO_INVALID;
    if(in != &w->in) w->in = *in;
    /* File offset of the first unread byte, kept in the workspace rather
     * than a register held across every call. */
    w->start = w->in.at - (uint32_t)(w->in.e - w->in.p);
    w->over = 0; w->error = KUI_ZSO_OK; w->hold = 0; w->bits = 0;
    uint8_t *o = out, *const stop = out + KUI_ZSO_BLOCK_BYTES;
    unsigned final;
    do {
        unsigned header = bits(w, 3u), type = header >> 1;
        final = header & 1u;
        if(!type) {
            /* Stored blocks begin on a byte boundary: at most 29 bits remain
             * buffered after the header, so this drops 0-7 of them. */
            bits(w, w->bits & 7u);
            uint32_t count = bits(w, 16u);
            /* Real input left: unread view and file bytes plus whole buffered
             * bytes that are not virtual. */
            if(bits(w, 16u) != (count ^ 0xffffu) || count > (uint32_t)(stop - o) ||
               w->over * 8u > w->bits ||
               count > w->in.end - w->in.at + (uint32_t)(w->in.e - w->in.p) + w->bits / 8u - w->over)
                goto invalid;
            while(count--) *o++ = (uint8_t)bits(w, 8u);
            continue;
        }
        if(type == 3u) goto invalid;
        /* Packed widths of a dynamic block (literal/length, then distance at
         * +36 words); NULL selects the fixed codes. */
        const uint32_t *widths = NULL;
        zero_words(w->limit, sizeof(w->limit) / 4u);
        if(type == 1u) {
            w->limit[0][7] = 24; w->limit[0][8] = 152; w->limit[0][9] = 112; w->limit[1][5] = 32;
        } else {
            unsigned literals = bits(w, 5u) + 257u, distances = bits(w, 5u) + 1u,
                     codes = bits(w, 4u) + 4u;
            if(literals > 286u) goto invalid;
            uint32_t *code_widths = w->lengths.widths;
            zero_words(&w->lengths, sizeof(w->lengths) / 4u);
            for(unsigned i = 0; i < codes; ++i) {
                unsigned n = order[i];
                ((uint8_t *)code_widths)[n >> 1] |= (uint8_t)(bits(w, 3u) << ((n & 1u) << 2));
            }
            for(unsigned n = 0; n < 19u; ++n) ++w->lengths.limit[width(code_widths, n)];
            if(!canonical(w->lengths.limit, 7u, 0u)) goto invalid;
            zero_words(w->widths, sizeof(w->widths) / 4u);
#if KUI_DEFLATE_BUCKETS
            zero_words(w->bucket, sizeof(w->bucket) / 4u);
#endif
            unsigned at = 0, size = literals + distances, previous = 0;
            while(at < size) {
                /* The code-length code is complete: a symbol always matches by
                 * length 7, so the walk never reads past its limit[7]. */
                NEED15(w);
                unsigned value = decode(w, w->lengths.limit, code_widths, NULL), count = 1u;
                if(value >= 16u) {
                    if(value == 16u) {
                        if(!at) goto invalid;
                        count = 3u + bits(w, 2u);
                        value = previous;
                    } else {
                        count = value == 17u ? 3u + bits(w, 3u) : 11u + bits(w, 7u);
                        value = 0;
                    }
                    if(count > size - at) goto invalid;
                }
                previous = value;
                for(; count; --count, ++at) {
                    if(!value) continue;
                    unsigned index = at;
                    if(at < literals) {
                        ++w->limit[0][value];
#if KUI_DEFLATE_BUCKETS
                        if(at / KUI_DEFLATE_BUCKET < KUI_DEFLATE_BUCKETS)
                            ++w->bucket[at / KUI_DEFLATE_BUCKET][value];
#endif
                    } else {
                        index += 288u - literals;
                        ++w->limit[1][value];
                    }
                    ((uint8_t *)w->widths)[index >> 1] |= (uint8_t)(value << ((index & 1u) << 2));
                }
            }
            if(!width(w->widths, 256u)) goto invalid; /* End-of-block needs a code. */
            widths = w->widths;
        }
        if(!canonical(w->limit[0], 15u, 1u) || !canonical(w->limit[1], 15u, 2u)) goto invalid;
        for(;;) {
            NEED15(w);
#if KUI_DEFLATE_BUCKETS
            unsigned symbol = decode(w, w->limit[0], widths, widths ? &w->bucket[0][0] : NULL);
#else
            unsigned symbol = decode(w, w->limit[0], widths, NULL);
#endif
            if(symbol < 256u) {
                if(o == stop) goto invalid;
                *o++ = (uint8_t)symbol;
                continue;
            }
            if(symbol == 256u) break;
            if(symbol > 285u) goto invalid;
            unsigned count = symbol - 254u;
            if(symbol == 285u) count = 258u;
            else if(symbol >= 265u) {
                unsigned extra = (symbol - 261u) >> 2;
                count = ((4u + ((symbol - 265u) & 3u)) << extra) + 3u + bits(w, extra);
            }
            if(count > (unsigned)(stop - o)) goto invalid;
            NEED15(w);
            symbol = decode(w, w->limit[1], widths ? widths + 36 : NULL, NULL);
            if(symbol > 29u) goto invalid;
            unsigned distance = symbol + 1u;
            if(symbol >= 4u) {
                unsigned extra = (symbol >> 1) - 1u;
                distance = ((2u + (symbol & 1u)) << extra) + 1u + bits(w, extra);
            }
            if(distance > KUI_ZSO_BLOCK_BYTES - (unsigned)(stop - o)) goto invalid;
            /* Increasing byte order also implements overlapping copies. */
            uint8_t *last = o + count;
            do *o = o[-(long)distance]; while(++o != last);
        }
    } while(!final);
    if(o != stop || w->over * 8u > w->bits || w->error != KUI_ZSO_OK) goto invalid;
    *consumed = w->in.at - (uint32_t)(w->in.e - w->in.p) - w->start - (w->bits / 8u - w->over);
    w->in.view = NULL; w->in.context = NULL;
    return KUI_ZSO_OK;
invalid:
    w->in.view = NULL; w->in.context = NULL;
    return w->error != KUI_ZSO_OK ? w->error : KUI_ZSO_INVALID;
}
