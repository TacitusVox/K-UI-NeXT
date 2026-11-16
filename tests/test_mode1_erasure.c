/* SPDX-License-Identifier: GPL-3.0-only
 * Lost-byte fill for raw Mode 1 sectors, checked against the repository's
 * own sector verifier (src/core/recovery_sector.c), which was written for
 * real GD-ROM sectors and shares no code with the fill. Sectors are built
 * here with the standard parity generator and admitted only if that verifier
 * accepts sync, address, EDC, reserved bytes and both parities. */
#include "kui/mode1_erasure.h"
#include "kui/hash.h"
#include "kui/recovery_checks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t ecc_f[256], ecc_b[256];
static uint32_t seed = 0x1234567u;
static uint32_t next(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

static void parity(const uint8_t *src, unsigned major_count, unsigned minor_count,
                   unsigned major_mult, unsigned minor_inc, uint8_t *out) {
    const unsigned size = major_count * minor_count;
    for(unsigned major = 0; major < major_count; ++major) {
        unsigned index = (major >> 1) * major_mult + (major & 1u);
        uint8_t a = 0, b = 0;
        for(unsigned minor = 0; minor < minor_count; ++minor) {
            const uint8_t v = src[index];
            index = (index + minor_inc) % size;
            a ^= v; b ^= v;
            a = ecc_f[a];
        }
        a = ecc_b[ecc_f[a] ^ b];
        out[major] = a;
        out[major + major_count] = a ^ b;
    }
}
static uint8_t bcd(unsigned v) { return (uint8_t)((v / 10u) << 4 | v % 10u); }
static void build(uint8_t raw[2352], uint32_t fad) {
    memset(raw, 0, 2352u);
    memset(raw + 1, 0xff, 10u);
    raw[12] = bcd(fad / 4500u); raw[13] = bcd(fad / 75u % 60u); raw[14] = bcd(fad % 75u); raw[15] = 1u;
    for(unsigned i = 16; i < 2064u; ++i) raw[i] = (uint8_t)next();
    const uint32_t edc = kui_cd_edc(raw, 2064u);
    for(unsigned i = 0; i < 4u; ++i) raw[2064u + i] = (uint8_t)(edc >> 8 * i);
    parity(raw + 12, 86u, 24u, 2u, 86u, raw + 2076);
    parity(raw + 12, 52u, 43u, 86u, 88u, raw + 2248);
    assert(kui_recovery_sector_check(raw, 2352u, fad) == 0u);
    assert(kui_sector_edc_valid_at(raw, fad));
}

static unsigned filled, refused, clustered, knotted;
/* Lose the listed bytes, fill them, and require the exact original back in
 * everything a reader uses whenever the fill says it succeeded. */
static bool round_trip(const uint8_t good[2352], uint32_t fad, const uint16_t *lost, unsigned count) {
    uint8_t raw[2352];
    memcpy(raw, good, sizeof(raw));
    for(unsigned i = 0; i < count; ++i) raw[lost[i]] = (uint8_t)(good[lost[i]] ^ (1u + next() % 255u));
    const bool ok = kui_mode1_fill_lost(raw, lost, count, fad);
    if(!ok) {++refused; return false;}
    ++filled;
    assert(!memcmp(raw, good, 2068u));
    assert(kui_sector_edc_valid_at(raw, fad));
    return true;
}

int main(void) {
    for(unsigned i = 0; i < 256u; ++i) {
        unsigned j = i << 1;
        if(j & 0x100u) j ^= 0x11du;
        ecc_f[i] = (uint8_t)j;
        ecc_b[i ^ j] = (uint8_t)i;
    }
    uint8_t good[2352];
    uint16_t lost[64];

    for(unsigned sector = 0; sector < 40u; ++sector) {
        const uint32_t fad = 45150u + next() % 500000u;
        build(good, fad);
        /* Every single position. Whatever is lost alone is recovered. */
        for(unsigned at = 0; at < 2352u; at += sector ? 7u : 1u) {
            lost[0] = (uint16_t)at;
            assert(round_trip(good, fad, lost, 1u));
        }
        /* A solid burst: neighbours sit in different P code words. */
        for(unsigned length = 2; length <= 32u; length += 5u) {
            const unsigned start = next() % (2352u - length);
            for(unsigned i = 0; i < length; ++i) lost[i] = (uint16_t)(start + i);
            assert(round_trip(good, fad, lost, length));
        }
        /* What a stalled bus leaves: a handful of bytes inside a few hundred.
         * Almost every such pattern unravels; the rare knot is refused. */
        for(unsigned round = 0; round < 200u; ++round) {
            const unsigned count = 2u + next() % 14u, start = next() % 1900u;
            for(unsigned i = 0; i < count; ++i) lost[i] = (uint16_t)(start + next() % 450u);
            ++clustered;
            if(!round_trip(good, fad, lost, count)) ++knotted;
        }
        /* Two in one P code word are separated by Q, and the reverse. */
        for(unsigned round = 0; round < 20u; ++round) {
            const unsigned at = 16u + next() % 1900u;
            lost[0] = (uint16_t)at; lost[1] = (uint16_t)(at + 86u);
            assert(round_trip(good, fad, lost, 2u));
            lost[1] = (uint16_t)(at + 88u <= 2247u ? at + 88u : at - 88u);
            assert(round_trip(good, fad, lost, 2u));
        }
        /* Scattered over the whole sector, up to the limit. */
        for(unsigned round = 0; round < 50u; ++round) {
            const unsigned count = 1u + next() % KUI_MODE1_ERASURE_MAX;
            for(unsigned i = 0; i < count; ++i) lost[i] = (uint16_t)(next() % 2352u);
            (void)round_trip(good, fad, lost, count);
        }
    }
    assert(filled > 8000u);
    assert(clustered == 8000u && knotted * 50u < clustered); /* under 2% */

    /* Four bytes on two P code words and the same two Q code words cannot be
     * told apart by XOR alone: the fill must say so, not guess. Any three of
     * them still unravel. */
    build(good, 123456u);
    const unsigned before = refused;
    lost[0] = 112u; lost[1] = 112u + 86u; lost[2] = 112u + 88u; lost[3] = 112u + 174u;
    assert(!round_trip(good, 123456u, lost, 4u));
    assert(refused == before + 1u);
    assert(round_trip(good, 123456u, lost, 3u));
    assert(round_trip(good, 123456u, lost + 1, 3u));
    /* A parity byte that cannot be filled does not matter by itself. */
    lost[0] = 2253u; lost[1] = 2253u + 52u; /* both parity bytes of one Q word */
    assert(round_trip(good, 123456u, lost, 2u));
    /* Refusals without touching anything out of range. */
    lost[0] = 2352u;
    assert(!kui_mode1_fill_lost(good, lost, 1u, 123456u));
    for(unsigned i = 0; i < 64u; ++i) lost[i] = (uint16_t)(20u + i);
    assert(!kui_mode1_fill_lost(good, lost, KUI_MODE1_ERASURE_MAX + 1u, 123456u));
    assert(kui_recovery_sector_check(good, 2352u, 123456u) == 0u);
    /* A byte that is wrong but not declared lost is never touched: the fill
     * of a declared one then comes out wrong and only the EDC can tell. */
    uint8_t raw[2352];
    memcpy(raw, good, sizeof(raw));
    raw[500] ^= 0x40u;
    lost[0] = 500u + 86u;
    raw[lost[0]] = 0u;
    assert(kui_mode1_fill_lost(raw, lost, 1u, 123456u));
    assert(!kui_sector_edc_valid_at(raw, 123456u));
    printf("PASS Mode 1 lost-byte fill: %u patterns filled exactly, %u refused; "
           "%u of %u clustered patterns knotted\n", filled, refused, knotted, clustered);
    return 0;
}
