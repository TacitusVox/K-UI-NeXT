/* SPDX-License-Identifier: GPL-3.0-only */
/* Runs under qemu-sh4 (Linux user mode): the CE reader's per-block check
 * (src/loader/sci_stream_check.h), compiled by sh-elf-gcc with the
 * resident's CPU flags so its SH-4 assembly step executes, against an
 * independent bit-by-bit CRC16 and bit reversal. Freestanding: the only
 * system calls are write and exit. */
#include "sci_stream_check.h"

static long sys_call(long number, long a, long b, long c) {
    register long r3 __asm__("r3") = number;
    register long r4 __asm__("r4") = a;
    register long r5 __asm__("r5") = b;
    register long r6 __asm__("r6") = c;
    register long r0 __asm__("r0");
    __asm__ volatile("trapa #0x13" : "=z"(r0) : "r"(r3), "r"(r4), "r"(r5), "r"(r6) : "memory", "t");
    return r0;
}
static void say(const char *text) {
    long n = 0;
    while(text[n]) ++n;
    (void)sys_call(4, 1, (long)text, n);
}
static uint32_t state = 0x2545f491u;
static uint32_t next(void) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
static uint8_t reverse8(uint8_t x) {
    uint8_t r = 0;
    for(unsigned i = 0; i < 8u; ++i) if(x & (1u << i)) r |= (uint8_t)(0x80u >> i);
    return r;
}
static uint32_t reference_crc(const uint8_t *bytes) {
    uint32_t crc = 0;
    for(unsigned i = 0; i < 512u; ++i) {
        crc ^= (uint32_t)bytes[i] << 8;
        for(unsigned b = 0; b < 8u; ++b)
            crc = crc & 0x8000u ? ((crc << 1) ^ 0x1021u) & 0xffffu : (crc << 1) & 0xffffu;
    }
    return crc;
}
static union { uint32_t words[128]; uint8_t bytes[512]; } block, expect;

static int run(void) {
    unsigned failures = 0, blocks = 0;
    for(unsigned n = 0; n < 4000u; ++n) {
        for(unsigned i = 0; i < 128u; ++i) {
            uint32_t v = next();
            /* Patterns first: zeros, ones, alternating, each byte value. */
            if(n == 0) v = 0;
            else if(n == 1) v = 0xffffffffu;
            else if(n == 2) v = 0xaa55aa55u;
            else if(n == 3) v = (4u * i) | (4u * i + 1u) << 8 | (4u * i + 2u) << 16 | (4u * i + 3u) << 24;
            block.words[i] = v;
        }
        bool wire = n & 1u;
        for(unsigned i = 0; i < 512u; ++i)
            expect.bytes[i] = wire ? reverse8(block.bytes[i]) : block.bytes[i];
        uint32_t crc = kui_sci_check_words(block.words, wire);
        bool bad = crc != reference_crc(expect.bytes);
        for(unsigned i = 0; i < 128u; ++i) bad |= block.words[i] != expect.words[i];
        failures += bad;
        ++blocks;
    }
    if(failures || blocks != 4000u) {say("sh4 block check: FAILED\n"); return 1;}
    say("sh4 block check: 4000 blocks match the bitwise reference\n");
    return 0;
}
void _start(void) __attribute__((noreturn, used));
void _start(void) {
    long code = run();
    for(;;) (void)sys_call(1, code, 0, 0);
}
