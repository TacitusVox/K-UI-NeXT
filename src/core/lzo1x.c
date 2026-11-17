/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/lzo1x.h"

/* Independent byte-stream state machine for the LZO1X token grammar.
 * The public miniLZO distribution is used as a format reference and test
 * oracle only. Explicit counts and distances avoid forming an invalid
 * pointer before validating a match. */
/* Read one byte in place; the cursor refills only when its view is empty. */
#define NEXT(value) do { \
        if(in->p == in->e && (result = kui_zso_cursor_fill(in)) != KUI_ZSO_OK) return result; \
        (value) = *in->p++; \
    } while(0)
__attribute__((always_inline)) static inline enum kui_zso_result extended(struct kui_zso_cursor *in, uint32_t base, uint32_t *count) {
    enum kui_zso_result result;
    uint32_t extra;
    do {
        NEXT(extra);
        uint32_t add = extra ? extra : 255u;
        if(add > KUI_ZSO_BLOCK_BYTES - base) return KUI_ZSO_INVALID;
        base += add;
    } while(!extra);
    *count = base;
    return KUI_ZSO_OK;
}
enum kui_zso_result kui_lzo1x_decode(struct kui_zso_cursor *in,
    uint8_t out[KUI_ZSO_BLOCK_BYTES], uint32_t *consumed) {
    if(!in || !out || !consumed) return KUI_ZSO_INVALID;
    /* File offset of the first unread byte. */
    const uint32_t start = in->at - kui_zso_cursor_buffered(in);
    uint32_t done = 0, count = 0, token, low, high;
    /* A zero state admits a literal run; one follows a long literal run
     * (whose short token has the special 2049+ distance); two admits only
     * a match after the preceding match's attached one-to-three literals. */
    unsigned state = 0;
    enum kui_zso_result result;
    NEXT(token);
    if(token > 17u) {
        count = token - 17u;
        state = count < 4u ? 2u : 1u;
        goto literals;
    }
    for(;;) {
        if(!state && token < 16u) {
            count = token;
            if(!count) {
                result = extended(in, 15u, &count);
                if(result != KUI_ZSO_OK) return result;
            }
            count += 3u;
            state = 1;
            goto literals;
        }
        uint32_t distance, trailing;
        if(token < 16u) {
            NEXT(low);
            distance = 1u + (token >> 2) + (low << 2);
            count = 2;
            if(state == 1u) { distance += 2048u; count = 3; }
            trailing = token & 3u;
        } else if(token >= 64u) {
            NEXT(low);
            distance = 1u + ((token >> 2) & 7u) + (low << 3);
            count = (token >> 5) + 1u;
            trailing = token & 3u;
        } else {
            count = token & (token >= 32u ? 31u : 7u);
            if(!count) {
                result = extended(in, token >= 32u ? 31u : 7u, &count);
                if(result != KUI_ZSO_OK) return result;
            }
            NEXT(low);
            NEXT(high);
            distance = (low >> 2) + (high << 6);
            trailing = low & 3u;
            if(token >= 32u) ++distance;
            else {
                distance += (token & 8u) << 11;
                if(!distance) {
                    if(token != 17u || low || done != KUI_ZSO_BLOCK_BYTES)
                        return KUI_ZSO_INVALID;
                    *consumed = in->at - kui_zso_cursor_buffered(in) - start;
                    return KUI_ZSO_OK;
                }
                distance += 16384u;
            }
            count += 2u;
        }
        if(distance > done || count > KUI_ZSO_BLOCK_BYTES - done)
            return KUI_ZSO_INVALID;
        /* Increasing byte order also implements distance-one repetitions. */
        while(count--) { out[done] = out[done - distance]; ++done; }
        count = trailing;
        state = count ? 2u : 0u;
literals:
        if(count > KUI_ZSO_BLOCK_BYTES - done) return KUI_ZSO_INVALID;
        result = kui_zso_cursor_take(in, out + done, count);
        if(result != KUI_ZSO_OK) return result;
        done += count;
        NEXT(token);
    }
}
