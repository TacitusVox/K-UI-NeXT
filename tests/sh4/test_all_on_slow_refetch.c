/* SPDX-License-Identifier: GPL-3.0-only */
/* Runs under qemu-sh4 (Linux user mode): the slow-refetch patch for the
 * delivered Armada Virtual VMU On reader (src/loader/armada_all_on_slow_refetch.S).
 * User mode cannot map P1 or the P4 registers, so the code runs 0x40000000
 * lower and its literal pool's absolute addresses are moved with it (P1 RAM
 * 0x8cXXXXXX -> 0x4cXXXXXX, SCI 0xffeXXXXX -> 0x7feXXXXX, DMAC 0xffaXXXXX ->
 * 0x7faXXXXX); branches and literal loads are PC-relative. The register
 * pages are plain RAM, so every store the code makes can be read back.
 * IMAGE_FILE: the bytes from 0x8c009028 to 0x8c009682 (start_dma, then the
 * span of finish holding the overrun update), patched or the original
 * 67b0df9 bytes. The harness supplies finish's stats literal and an rts
 * after the span. ORIGINAL=1 checks the unpatched start_dma's fast path
 * against the same expectations. */
#include <stdbool.h>
#include <stdint.h>

#ifndef ORIGINAL
#define ORIGINAL 0
#endif
#define MOVE 0x40000000u
#define BASE (0x8c009028u - MOVE)
#define S (0x8c00d05cu - MOVE)
#define SLOW (S + 75u)
#define STATS (0x8c00d0bcu - MOVE)
#define SCI 0x7fe00000u
#define DMAC 0x7fa00000u
#if ORIGINAL
#define POOL_BEGIN 0x8c00908cu
#define POOL_END 0x8c0090c0u
#else
#define POOL_BEGIN 0x8c0090a8u
#define POOL_END 0x8c0090bcu
#endif

static long sys_call6(long number, long a, long b, long c, long d, long e, long f) {
    register long r3 __asm__("r3") = number;
    register long r4 __asm__("r4") = a;
    register long r5 __asm__("r5") = b;
    register long r6 __asm__("r6") = c;
    register long r7 __asm__("r7") = d;
    register long r0 __asm__("r0") = e;
    register long r1 __asm__("r1") = f;
    __asm__ volatile("trapa #0x16" : "+z"(r0) : "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r1) : "memory", "t");
    return r0;
}
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
static bool map(uint32_t at, uint32_t bytes) {
    /* mmap2(at, bytes, RWX, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) */
    return (uint32_t)sys_call6(192, at, bytes, 7, 0x32, -1, 0) == at;
}
extern const uint8_t image[], image_end[];
__asm__(".section .rodata\n.balign 4\n_image:\n.incbin \"" IMAGE_FILE "\"\n_image_end:\n.text\n");

#define R8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define R16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
static unsigned failures, checks;
static void check(bool ok, const char *what) {
    ++checks;
    if(!ok) {say("FAIL: "); say(what); say("\n"); ++failures;}
}
static uint32_t moved(uint32_t v) {
    if((v & 0xff000000u) == 0x8c000000u) return v - MOVE;
    if((v & 0xfff00000u) == 0xffe00000u) return SCI | (v & 0xfffffu);
    if((v & 0xfff00000u) == 0xffa00000u) return DMAC | (v & 0xfffffu);
    return v;
}
typedef uint32_t (*start_dma_fn)(uint32_t, uint32_t);

static void start_case(uint8_t slow, uint32_t area, uint32_t offset, uint8_t want_slow, bool want_brr) {
    R8(SCI + 4) = 0xa5;                  /* BRR sentinel: untouched unless slow */
    R8(SCI + 8) = 0x30;
    R8(SCI + 0x1c) = 0;
    R32(DMAC + 0x10) = 0x11111110; R32(DMAC + 0x14) = 0x22222220;
    R32(DMAC + 0x18) = 0x33333330; R32(DMAC + 0x1c) = 0x44444440;
    R32(S + 4) = 0x8c0c0000u; R32(S + 8) = 0xac0d0020u;
    R32(S + 12) = 9; R8(SLOW) = slow;
    for(unsigned i = 0; i < 4; ++i) R32(S + 36 + 4 * i) = 0;
    __asm__ volatile("" ::: "memory");
    uint32_t r = ((start_dma_fn)(uintptr_t)BASE)(area, offset);
    check(r == 0, "start_dma returns OK");
    check(R8(SCI + 0x1c) == 0x83, "SPTR = 0x83");
    check(R32(S + 36) == 0x11111110 && R32(S + 40) == 0x22222220 &&
          R32(S + 44) == 0x33333330 && R32(S + 48) == 0x44444440, "saved SAR/DAR/TCR/CHCR");
    check(R32(DMAC + 0x10) == 0x1fe00014u, "SAR = SCRDR1");
    check(R32(DMAC + 0x14) == ((area ? 0x0c0d0020u : 0x0c0c0000u) + offset), "DAR = physical area + offset");
    check(R32(DMAC + 0x18) == 513u - offset, "TCR = 513 - offset");
    check(R32(DMAC + 0x1c) == 0x4911u, "CHCR = receive DMA");
    check(R8(SCI + 8) == 0x50, "SCR = RIE|RE");
    check(R32(S + 12) == 2, "state = DMA");
    check(R8(SCI + 4) == (want_brr ? 1 : 0xa5), "BRR only for a slow reception");
    check(R8(SLOW) == want_slow, "slow state after start");
}
#if !ORIGINAL
static uint32_t run_overrun(uint32_t status, uint8_t slow, uint32_t *r1, uint32_t *r3) {
    R8(SLOW) = slow;
    register uint32_t a0 __asm__("r0");
    register uint32_t a1 __asm__("r1");
    register uint32_t a3 __asm__("r3");
    register uint32_t fn __asm__("r4") = BASE + (0x8c00965cu - 0x8c009028u);
    register uint32_t st __asm__("r5") = status;
    __asm__ volatile("mov r5,r12\n\tmov #0,r1\n\tmov #0,r3\n\tjsr @r4\n\tnop"
        : "=z"(a0), "=r"(a1), "=r"(a3) : "r"(fn), "r"(st)
        : "r2", "r12", "pr", "t", "memory");
    *r1 = a1; *r3 = a3;
    return a0;
}
static void overrun_case(uint32_t status, uint8_t slow, uint8_t want_slow) {
    uint32_t r1, r3, r0 = run_overrun(status, slow, &r1, &r3);
    check(r0 == status, "r0 = status on exit");
    check(r1 == STATS, "r1 = stats pointer on exit");
    check(r3 == ((status & 0x40u) ? 1u : 0u), "r3 = held");
    check(R8(SLOW) == want_slow, "slow state after an incomplete reception");
}
#endif
static int run(void) {
    if(!map(0x8c008000u - MOVE, 0x8000u) || !map(SCI, 0x1000u) || !map(DMAC, 0x1000u)) {
        say("sh4 slow refetch: cannot map the test address space\n");
        return 1;
    }
    uint32_t bytes = (uint32_t)(image_end - image);
    if(bytes != 0x8c009682u - 0x8c009028u) {say("sh4 slow refetch: wrong image size\n"); return 1;}
    for(uint32_t i = 0; i < bytes; ++i) R8(BASE + i) = image[i];
    for(uint32_t at = POOL_BEGIN; at < POOL_END; at += 4u) {
        uint32_t p = BASE + (at - 0x8c009028u);
        R32(p) = moved(R32(p));
    }
    R32(0x8c009738u - MOVE) = STATS;     /* finish's own literal */
    R16(0x8c009682u - MOVE) = 0x000b;    /* rts after the span */
    R16(0x8c009684u - MOVE) = 0x0009;
    __asm__ volatile("" ::: "memory");
    start_case(0, 1, 5, 0, false);
    start_case(0, 0, 0, 0, false);
#if !ORIGINAL
    start_case(0x80, 0, 0, 0, false);    /* the last slow reception ended */
    start_case(64, 1, 0, 0xbf, true);    /* 64 left: this one slow, 63 more */
    start_case(0x85, 0, 7, 0x84, true);
    start_case(1, 0, 0, 0x80, true);
    overrun_case(0x00, 0, 0);            /* no ORER: unchanged */
    overrun_case(0x00, 0x83, 0x83);
    overrun_case(0x20, 0, 64);           /* fast reception overran */
    overrun_case(0x60, 0x05, 64);
    overrun_case(0x20, 0x85, 0);         /* slow reception overran */
    overrun_case(0x60, 0xbf, 0);
    overrun_case(0x40, 0x10, 0x10);
    /* 64 slow receptions, then fast again. */
    R8(SLOW) = 64;
    for(unsigned i = 0; i < 64; ++i) start_case(R8(SLOW), 0, 0, (uint8_t)(0x80u | (63u - i)), true);
    start_case(R8(SLOW), 0, 0, 0, false);
#endif
    if(failures) {say("sh4 slow refetch: FAILED\n"); return 1;}
    say(ORIGINAL ? "sh4 slow refetch: the original start_dma's fast path meets the same checks\n"
                 : "sh4 slow refetch: patched start_dma and overrun span behave as specified\n");
    return 0;
}
void _start(void) __attribute__((noreturn, used));
void _start(void) {
    long code = run();
    for(;;) (void)sys_call(1, code, 0, 0);
}
