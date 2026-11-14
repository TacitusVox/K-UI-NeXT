/* SPDX-License-Identifier: GPL-3.0-only
 * Lost-byte fill for raw Mode 1 sectors (see kui/mode1_erasure.h).
 *
 * Geometry as checked by src/core/recovery_sector.c parity_ok(): the parity
 * covers bytes 12..2247 of the sector for P and 12..2247 plus the Q area for
 * Q. Counting from byte 12:
 *   P code word j (0..85) is the 26 bytes j, j+86, ... j+86*25; the last two
 *   lie in the P area (2064..2235).
 *   Q code word m (0..51) is the 43 bytes (m/2)*86 + (m&1) + 88*i modulo
 *   2236, plus its two parity bytes 2236+m and 2288+m.
 * Both are Reed-Solomon code words whose generator has the root 1, so the
 * XOR of all bytes of a code word is zero. That one equation is all this
 * needs; no field multiplication is used. Written without division: the
 * freestanding readers have no divide instruction to spare. */
#include "kui/mode1_erasure.h"

#define SYNC_BYTES 12u
#define P_SPAN 2236u   /* bytes 12..2247: header, data, EDC, zeros, P parity */
#define Q_SPAN 2340u   /* the same plus the Q parity */
#define NEEDED 2068u   /* sync, header, user data and EDC */

static unsigned below(unsigned value, unsigned modulus) {
    while(value >= modulus) value -= modulus;
    return value;
}

static unsigned p_word(unsigned index) { return below(index, 86u); }

static unsigned q_word(unsigned index) {
    if(index >= P_SPAN) return below(index - P_SPAN, 52u);
    /* index/2 = 43*d + 44*i modulo 1118 with i < 43: i is index/2 modulo 43. */
    const unsigned word = index >> 1, minor = below(word, 43u);
    unsigned rest = below(word + 2236u - 44u * minor, 1118u), diagonal = 0;
    while(rest >= 43u) {rest -= 43u; ++diagonal;}
    return diagonal * 2u + (index & 1u);
}

static uint8_t p_fill(const uint8_t *body, unsigned index) {
    uint8_t value = 0;
    for(unsigned at = p_word(index); at < P_SPAN; at += 86u)
        if(at != index) value ^= body[at];
    return value;
}

static uint8_t q_fill(const uint8_t *body, unsigned index) {
    const unsigned major = q_word(index);
    unsigned at = (major >> 1) * 86u + (major & 1u);
    uint8_t value = 0;
    for(unsigned minor = 0; minor < 43u; ++minor) {
        if(at != index) value ^= body[at];
        at += 88u;
        if(at >= P_SPAN) at -= P_SPAN;
    }
    if(P_SPAN + major != index) value ^= body[P_SPAN + major];
    if(P_SPAN + 52u + major != index) value ^= body[P_SPAN + 52u + major];
    return value;
}

static uint8_t bcd(unsigned value) {
    unsigned tens = 0;
    while(value >= 10u) {value -= 10u; ++tens;}
    return (uint8_t)(tens << 4 | value);
}

bool kui_mode1_fill_lost(uint8_t raw[2352], const uint16_t *lost, unsigned count, uint32_t fad) {
    if(count > KUI_MODE1_ERASURE_MAX) return false;
    uint16_t open[KUI_MODE1_ERASURE_MAX];
    uint8_t p[KUI_MODE1_ERASURE_MAX], q[KUI_MODE1_ERASURE_MAX];
    unsigned left = 0;
    for(unsigned i = 0; i < count; ++i) {
        const unsigned at = lost[i];
        if(at >= 2352u) return false;
        bool repeated = false;
        for(unsigned j = 0; j < left; ++j) if(open[j] == at - SYNC_BYTES) repeated = true;
        if(repeated) continue;
        if(at < SYNC_BYTES) raw[at] = at == 0u || at == 11u ? 0x00u : 0xffu;
        else if(at < 15u) {
            /* Minutes, seconds, frames of the expected address. */
            unsigned frames = fad, minutes = 0, seconds = 0;
            while(frames >= 4500u) {frames -= 4500u; ++minutes;}
            while(frames >= 75u) {frames -= 75u; ++seconds;}
            raw[at] = bcd(at == 12u ? minutes : at == 13u ? seconds : frames);
        } else if(at == 15u) raw[at] = 1u;
        else if(at >= 2068u && at < 2076u) raw[at] = 0u;
        else {
            const unsigned index = at - SYNC_BYTES;
            open[left] = (uint16_t)index;
            p[left] = index < P_SPAN ? (uint8_t)p_word(index) : 0xffu;
            q[left] = (uint8_t)q_word(index);
            ++left;
        }
    }
    uint8_t *body = raw + SYNC_BYTES;
    for(bool moved = true; moved && left;) {
        moved = false;
        for(unsigned i = 0; i < left; ++i) {
            bool alone_p = p[i] != 0xffu, alone_q = true;
            for(unsigned j = 0; j < left; ++j) {
                if(j == i) continue;
                if(p[j] == p[i]) alone_p = false;
                if(q[j] == q[i]) alone_q = false;
            }
            if(!alone_p && !alone_q) continue;
            body[open[i]] = alone_p ? p_fill(body, open[i]) : q_fill(body, open[i]);
            --left;
            open[i] = open[left]; p[i] = p[left]; q[i] = q[left];
            moved = true;
            --i;
        }
    }
    for(unsigned i = 0; i < left; ++i)
        if(open[i] + SYNC_BYTES < NEEDED) return false;
    return true;
}

bool kui_mode1_parity_holds(const uint8_t raw[2352]) {
    const uint8_t *body = raw + SYNC_BYTES;
    /* Filling any member of a code word from the rest gives that member
     * back exactly when the word XORs to zero. */
    for(unsigned word = 0; word < 86u; ++word)
        if(p_fill(body, word) != body[word]) return false;
    for(unsigned word = 0; word < 52u; ++word)
        if(q_fill(body, P_SPAN + word) != body[P_SPAN + word]) return false;
    return true;
}
