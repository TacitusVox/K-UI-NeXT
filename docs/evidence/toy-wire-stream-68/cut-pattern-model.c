/* How many sectors does the parity fill save, for cut patterns shaped like
 * the Test 67 run histogram? A model for sizing only. */
#include "kui/mode1_erasure.h"
#include "kui/hash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t ecc_f[256], ecc_b[256];
static uint32_t seed = 12345u;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
static void parity(const uint8_t *src, unsigned major_count, unsigned minor_count, unsigned major_mult, unsigned minor_inc, uint8_t *out) {
    const unsigned size = major_count * minor_count;
    for(unsigned major = 0; major < major_count; ++major) {
        unsigned index = (major >> 1) * major_mult + (major & 1u);
        uint8_t a = 0, b = 0;
        for(unsigned minor = 0; minor < minor_count; ++minor) {
            const uint8_t v = src[index]; index = (index + minor_inc) % size; a ^= v; b ^= v; a = ecc_f[a];
        }
        a = ecc_b[ecc_f[a] ^ b]; out[major] = a; out[major + major_count] = a ^ b;
    }
}
static uint8_t bcd(unsigned v) { return (uint8_t)((v / 10u) << 4 | v % 10u); }
static void build(uint8_t raw[2352], uint32_t fad) {
    memset(raw, 0, 2352u); memset(raw + 1, 0xff, 10u);
    raw[12] = bcd(fad / 4500u); raw[13] = bcd(fad / 75u % 60u); raw[14] = bcd(fad % 75u); raw[15] = 1u;
    for(unsigned i = 16; i < 2064u; ++i) raw[i] = (uint8_t)rnd();
    const uint32_t edc = kui_cd_edc(raw, 2064u);
    for(unsigned i = 0; i < 4u; ++i) raw[2064u + i] = (uint8_t)(edc >> 8 * i);
    parity(raw + 12, 86u, 24u, 2u, 86u, raw + 2076); parity(raw + 12, 52u, 43u, 86u, 88u, raw + 2248);
}
/* Run length before the next cut. dense: stay in the busy phase with probability stay%. */
static unsigned run_length(int *dense, unsigned stay) {
    if(*dense && rnd() % 100u < stay) return rnd() % 100u < 41u ? 1u + rnd() % 15u : 16u + rnd() % 48u; /* 0x713 : 0xA24 */
    *dense = 0;
    unsigned pick = rnd() % 1597u; /* 586 <256, 190 <1K, 399 <4K, 422 more */
    unsigned run = pick < 586u ? 64u + rnd() % 192u : pick < 776u ? 256u + rnd() % 768u : pick < 1175u ? 1024u + rnd() % 3072u : 4096u + rnd() % 12000u;
    *dense = 1; /* a cut starts a busy phase */
    return run;
}
int main(int argc, char **argv) {
    for(unsigned i = 0; i < 256u; ++i) { unsigned j = i << 1; if(j & 0x100u) j ^= 0x11du; ecc_f[i] = (uint8_t)j; ecc_b[i ^ j] = (uint8_t)i; }
    const unsigned stay = argc > 1 ? (unsigned)atoi(argv[1]) : 80u;
    const unsigned sectors = 40000u;
    static uint8_t good[2352], raw[2352];
    unsigned clean = 0, crc_only = 0, filled = 0, knot = 0, over = 0; unsigned long cuts = 0, shortcuts = 0;
    int dense = 0; long next = (long)run_length(&dense, stay);
    /* Wire framing: 515 bytes per block plus a gap; holes in non-data bytes cost nothing here. */
    for(unsigned n = 0; n < sectors; ++n) {
        build(good, 45150u + n); memcpy(raw, good, sizeof raw);
        uint16_t lost[4096]; unsigned count = 0; unsigned per_block[8] = {0};
        for(unsigned at = 0; at < 2352u; ++at) {
            if(--next < 0) { lost[count++] = (uint16_t)at; per_block[at / 512u]++; ++cuts; unsigned r = run_length(&dense, stay); if(r < 64u) ++shortcuts; next = (long)r; raw[at] ^= 0x5a; }
        }
        if(!count) {++clean; continue;}
        /* A block with exactly one lost byte is rebuilt from its CRC16. */
        uint16_t left[4096]; unsigned m = 0;
        for(unsigned i = 0; i < count; ++i) if(per_block[lost[i] / 512u] != 1u) left[m++] = lost[i]; else raw[lost[i]] = good[lost[i]];
        if(!m) {++crc_only; continue;}
        if(m > KUI_MODE1_ERASURE_MAX) {++over; continue;}
        if(kui_mode1_fill_lost(raw, left, m, 45150u + n) && !memcmp(raw, good, 2068u)) ++filled; else ++knot;
    }
    printf("stay %u%%: cuts %lu (%.0f%% short) over %u sectors: clean %u, CRC16 alone %u, parity fill %u, knot %u, over 32 lost %u => %.2f%% of sectors need a re-read\n",
        stay, cuts, 100.0 * shortcuts / cuts, sectors, clean, crc_only, filled, knot, over, 100.0 * (knot + over) / sectors);
    return 0;
}
