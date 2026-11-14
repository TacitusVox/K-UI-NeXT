/* SPDX-License-Identifier: GPL-3.0-only
 * Host model for the Toy Commander Test 68 card stream. It runs the real
 * src/loader/toy_wire_stream.c against a small register model of the SCI
 * receiver, DMAC channel 1, the spare TMU channels and a card in an open
 * CMD18, and checks every block the ring reader hands on against the card.
 *
 * What the model encodes (established on the console by earlier tests and
 * documented in sci_stream.c and toy_wire_probe.c): bytes arrive
 * bit-reversed; the receiver holds one byte; if it is still full when the
 * next byte completes, that byte is lost, ORER is set and the clock stops;
 * the channel may still take the held byte late; behind an exhausted count
 * one byte is held and the next overruns; after an overrun the SCI must be
 * reset through module standby before it receives again. The wire advances
 * in pieces of random length, so the reader also sees runs still in flight.
 *
 * Checked on every block, over stall patterns from none to far denser than
 * Test 67 measured, card gaps of 0 to 37 bytes, and data full of idle- and
 * token-looking bytes: blocks come in card order, none skipped or repeated;
 * every byte not listed as lost equals the card's; a block handed on with
 * no lost byte after a clean CRC16 or a rebuild is never wrong. A wrong
 * block may be handed on only in the one corner the design leaves to the
 * caller's sector check: a block that cannot be CRC-checked (several lost
 * bytes) behind a byte lost between blocks; the test counts those and
 * proves each had lost bytes. This is a model, not a Dreamcast: it checks
 * logic and accounting, never bus timing. */
#include "toy_wire_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHYS 0x0c500000u
static struct {
    uint8_t smr, brr, scr, ssr, rdr, scmr, sptr, stbcr, tstr;
    uint16_t pdtr, ipra, tcr[3];
    uint32_t tcor[3], tcnt[3], sar, dar, count, chcr, dmaor, slots[64], tcnt0;
} hw;
static uint8_t ring[KUI_WIRE_RING_BYTES], tables[KUI_WIRE_TABLE_BYTES];
static uint32_t seed = 1;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
static uint8_t rev8(uint8_t x) { uint8_t r = 0; for(int i = 0; i < 8; ++i) if(x & 1 << i) r |= 0x80 >> i; return r; }
static uint16_t crc16(const uint8_t *p, unsigned n) {
    uint16_t c = 0;
    for(unsigned i = 0; i < n; ++i) { c ^= (uint16_t)(p[i] << 8); for(int b = 0; b < 8; ++b) c = (uint16_t)(c & 0x8000 ? c << 1 ^ 0x1021 : c << 1); }
    return c;
}
/* ---- card: blocks with per-block gap, adversarial data ---- */
#define BLOCKS 60000u
static uint8_t gap_of(uint32_t b) { uint32_t x = b * 2654435761u; x ^= x >> 15; unsigned g = x % 37u; return (uint8_t)(x % 97u == 0 ? 0 : g + 1u); }
static uint8_t data_byte(uint32_t b, unsigned i) {
    uint32_t x = (b * 40503u + 7u) ^ (i * 2654435761u); x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
    if(b % 7u == 3u && i < 3u) return i < 2u ? 0xff : 0xfe;     /* FF FF FE ... */
    if(b % 11u == 5u && i == 0u) return 0xfe;
    if(b % 13u == 6u) return 0xff;                                /* a block of idle-looking bytes */
    if((x & 0x1f) == 7) return 0xff;
    if((x & 0x1f) == 9) return 0xfe;
    return (uint8_t)(x >> 8);
}
static uint32_t card_block, card_at; /* position inside [gap][FE][512][2] of card_block */
static uint64_t clocked;
/* Test 69's note: one block's last idle byte replaced by something else. */
static uint32_t stray_block;
static uint8_t stray_value;
static uint8_t card_byte(void) {
    const unsigned gap = gap_of(card_block);
    uint8_t v;
    if(stray_block && card_block == stray_block && card_at + 1u == gap) v = stray_value;
    else if(card_at < gap) v = 0xff;
    else if(card_at == gap) v = 0xfe;
    else if(card_at <= gap + 512u) v = data_byte(card_block, card_at - gap - 1u);
    else { uint8_t d[512]; for(unsigned i = 0; i < 512u; ++i) d[i] = data_byte(card_block, i); uint16_t c = crc16(d, 512u); v = card_at == gap + 513u ? (uint8_t)(c >> 8) : (uint8_t)c; }
    if(++card_at == gap + 515u) { card_at = 0; ++card_block; }
    ++clocked;
    return v;
}
/* ---- wire ---- */
static unsigned stall_percent_dense = 0, dense_left = 0, stall_every = 0;
static uint32_t until_stall;
static bool receiving(void) { return hw.scr == 0x50 && !(hw.ssr & 0x20) && !(hw.pdtr & 0x80) && !(hw.stbcr & 1); }
static unsigned long overruns;
static void shift_one(void) {
    uint8_t wire = rev8(card_byte());
    bool stall = false;
    if(stall_every) { if(!until_stall--) { stall = true; if(dense_left) { --dense_left; until_stall = 1u + rnd() % (rnd() % 100u < 41u ? 15u : 63u); } else { until_stall = rnd() % 100u < stall_percent_dense ? (dense_left = 3u + rnd() % 30u, 1u + rnd() % 40u) : 64u + rnd() % (2u * stall_every); } } }
    bool dma = (hw.chcr & 3u) == 1u && hw.count && hw.sar == 0x1fe00014u && (hw.dmaor & 7u) == 1u;
    if(dma && !stall && !(hw.ssr & 0x40)) {
        assert(hw.dar >= PHYS && hw.dar < PHYS + KUI_WIRE_RING_BYTES);
        ring[hw.dar - PHYS] = wire; ++hw.dar; if(!--hw.count) hw.chcr |= 2u;
    } else if(!(hw.ssr & 0x40)) { hw.rdr = wire; hw.ssr |= 0x40; }
    else { hw.ssr |= 0x20; ++overruns;
        /* the channel, back on the bus, may still take the held byte */
        if(dma && (rnd() & 1u)) { ring[hw.dar - PHYS] = hw.rdr; ++hw.dar; if(!--hw.count) hw.chcr |= 2u; hw.ssr &= (uint8_t)~0x40; } }
}
static void pump(unsigned n) { while(n-- && receiving()) shift_one(); }
uint32_t kui_wire_test_read(uint32_t a, unsigned width) {
    (void)width;
    switch(a) {
    case 0xffe00000u: return hw.smr; case 0xffe00004u: return hw.brr; case 0xffe00008u: return hw.scr;
    case 0xffe00010u: if(receiving()) shift_one(); return hw.ssr; /* time passes with every poll */
    case 0xffe00014u: return hw.rdr; case 0xffe00018u: return hw.scmr; case 0xffe0001cu: return hw.sptr;
    case 0xffc00004u: return hw.stbcr; case 0xff800030u: return hw.pdtr;
    case 0xffa00010u: return hw.sar; case 0xffa00014u: return hw.dar; case 0xffa00018u: return hw.count; case 0xffa0001cu: return hw.chcr;
    case 0xffa00040u: return hw.dmaor; case 0xffd00004u: return hw.ipra; case 0xffd80004u: return hw.tstr;
    case 0xffd8000cu: return hw.tcnt0 -= 3u;
    default: break;
    }
    if(a >= 0xffd80008u && a < 0xffd8002cu) { unsigned k = (a - 0xffd80008u) / 12u, f = (a - 0xffd80008u) % 12u; return f == 0 ? hw.tcor[k] : f == 4 ? hw.tcnt[k] : hw.tcr[k]; }
    if(a >= 0x8c00f600u && a < 0x8c00f700u) return hw.slots[(a - 0x8c00f600u) / 4u];
    fprintf(stderr, "read %08x\n", a); abort();
}
static bool stale;
void kui_wire_test_write(uint32_t a, uint32_t v, unsigned width) {
    (void)width;
    switch(a) {
    case 0xffe00000u: hw.smr = (uint8_t)v; return; case 0xffe00004u: hw.brr = (uint8_t)v; return;
    case 0xffe00008u:
        if(v == 0x50) { assert(!stale && hw.sptr == 0x83 && !(hw.pdtr & 0x80) && hw.chcr == 0x4911u && hw.count); }
        /* The receiver is only ever switched off once it has stopped by
         * itself, at an overrun: the card is then on a whole byte. */
        if(v != 0x50) assert(!receiving());
        hw.scr = (uint8_t)v; return;
    case 0xffe00010u: if((hw.ssr & 0x20) && !(v & 0x20)) stale = true; hw.ssr = (uint8_t)((hw.ssr & (v | ~0xf8u)) | (hw.ssr & 0x04u)); return;
    case 0xffe00018u: hw.scmr = (uint8_t)v; return; case 0xffe0001cu: hw.sptr = (uint8_t)v; return;
    case 0xffc00004u: if(!(hw.stbcr & 1u) && (v & 1u)) { hw.scr = 0; hw.smr = 0; hw.brr = 0xff; hw.scmr = 0; hw.ssr = 0x84; hw.rdr = 0; stale = false; } hw.stbcr = (uint8_t)v; return;
    case 0xff800030u: hw.pdtr = (uint16_t)v; return;
    case 0xffa00010u: hw.sar = v; return; case 0xffa00014u: hw.dar = v; return; case 0xffa00018u: hw.count = v; return; case 0xffa0001cu: hw.chcr = v; return;
    case 0xffd00004u: hw.ipra = (uint16_t)v; return; case 0xffd80004u: hw.tstr = (uint8_t)v; return;
    default: break;
    }
    if(a >= 0xffd80014u && a < 0xffd8002cu) { unsigned k = (a - 0xffd80008u) / 12u, f = (a - 0xffd80008u) % 12u; if(f == 0) hw.tcor[k] = v; else if(f == 4) hw.tcnt[k] = v; else hw.tcr[k] = (uint16_t)v; return; }
    if(a >= 0x8c00f600u && a < 0x8c00f700u) { hw.slots[(a - 0x8c00f600u) / 4u] = v; return; }
    fprintf(stderr, "write %08x\n", a); abort();
}
void kui_wire_test_settle(unsigned n) { (void)n; }
void kui_wire_test_fence(void) {}
uint32_t kui_wire_test_physical(const void *p) { assert(p == ring); return PHYS; }
uint32_t kui_wire_test_vbr(void) { return 0x8c00f400u; }
void kui_wire_test_purge(const void *p, unsigned n) { assert(p == ring && n == KUI_WIRE_RING_BYTES); }

static struct kui_wire_stream w;
static struct kui_wire_counters n;
static void command_framing(void) { hw.scr = 0x30; hw.smr = 0x80; hw.brr = 0; hw.scmr = 0; hw.ssr = 0x84; hw.sptr = 0x05; hw.pdtr |= 0x80; hw.chcr = 0; stale = false; }
static bool timer_armed(void) { return w.mode && (hw.tstr & (1u << w.mode)) && (hw.tcr[w.mode] & 0x20); }

int main(int argc, char **argv) {
    unsigned long total = 0, restarts = 0, wrong = 0, rewinds[4] = {0};
    printf("Test %u card stream on the wire model:\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_CACHED ? 70u : 68u);
    for(unsigned run = 0; run < (argc > 1 ? (unsigned)atoi(argv[1]) : 12u); ++run) {
        seed = 77u + run * 1000u;
        memset(&hw, 0, sizeof hw); hw.dmaor = 0x8201u; hw.sar = 0x11; hw.dar = 0x22; hw.count = 0x33;
        for(unsigned i = 0; i < 64; ++i) hw.slots[i] = 0x8c0be408u;
        if(run % 5u == 4u) hw.tstr = 6u;        /* no free timer */
        else if(run % 2u) hw.tstr = 4u;          /* TMU2 busy: TMU1 */
        stall_every = run % 4u == 0 ? 0u : 600u << (run % 3u);
        stall_percent_dense = run % 4u == 0 ? 0u : 10u * (run % 7u);
        until_stall = 100u; dense_left = 0;
        uint32_t expect = 1; card_block = 1; card_at = 0;
        const unsigned long overruns_before = overruns;
        const uint8_t timers_before = hw.tstr;
#if KUI_TOY_WIRE_TICK
        uint32_t asked = 0;
#endif
        memset(&w, 0, sizeof w); memset(&n, 0, sizeof n);
        command_framing();
        memset(ring, 0xa5, sizeof ring);
        assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
        unsigned idle = 0; uint32_t last_gaps = 0;
        while(expect < BLOCKS / 4u) {
            pump(1u + rnd() % 3000u);
            if(rnd() % 3u || (hw.ssr & 0x20)) kui_wire_service(&w);
            if(w.state == KUI_WIRE_WAIT && timer_armed() && !(rnd() % 3u)) {
                /* Some waits end with a caller waiting for a position a few
                 * bytes on: the run is cut to it, also right at the ring's end. */
                if(run % 3u == 2u) w.want = w.head + 1u + rnd() % 60u;
                hw.tcr[w.mode] |= 0x100u;
                if(rnd() & 1u) kui_wire_timer(&w); else kui_wire_service(&w);
            }
            if(w.state == KUI_WIRE_FAULT) { fprintf(stderr, "fault %u %x\n", w.error, w.detail); return 1; }
#if KUI_TOY_WIRE_TICK
            /* A caller that ran out of time asks for one more timer call.
             * It gets one exactly while the stream receives or holds for
             * room and a timer is free; a wait keeps its own timing. */
            if(rnd() % 6u == 0u) {
                const bool waiting = w.state == KUI_WIRE_WAIT, was_armed = timer_armed();
                const uint32_t count = waiting && w.mode ? hw.tcnt[w.mode] : 0u;
                const bool armed = kui_wire_tick(&w, 50u + rnd() % 500u);
                assert(armed == (w.mode && (w.state == KUI_WIRE_RUN || w.state == KUI_WIRE_FULL)));
                if(armed) { ++asked; assert(timer_armed() && ((hw.ipra >> (12u - 4u * w.mode)) & 15u)); }
                else if(waiting && w.mode) assert(timer_armed() == was_armed && hw.tcnt[w.mode] == count);
            }
            /* The call comes: nothing but the timer changes unless a wait ends. */
            if(w.state != KUI_WIRE_WAIT && timer_armed() && rnd() % 2u) {
                const uint32_t state = w.state, head = w.head;
                hw.tcr[w.mode] |= 0x100u;
                kui_wire_timer(&w);
                assert(!timer_armed() && w.state == state && w.head == head);
            }
            assert(n.ticks == asked);
#endif
            /* Now and then a caller waits for a position just ahead: the
             * next run then ends there, wherever in the ring that is. */
            if(run % 3u == 2u && rnd() % 5u == 0u) w.want = w.head + 1u + rnd() % 300u;
            else if(rnd() % 4u == 0u) w.want = 0;
            if(rnd() % 4u == 0u) continue; /* the reader is sometimes away */
            for(unsigned k = 0; k < 1u + rnd() % 40u; ++k) {
                struct kui_wire_block b;
                const enum kui_wire_parse p = kui_wire_next(&w, &b);
                if(p == KUI_WIRE_NEED) break;
                if(p != KUI_WIRE_BLOCK) {
                    /* the caller's rewind: stop, the low reader seeks to the block still wanted */
                    ++rewinds[-(int)p];
                    assert(kui_wire_stop(&w));
                    assert(hw.scr == 0x30 && hw.smr == 0x80 && (hw.pdtr & 0x80) && hw.sar == 0x11 && hw.dar == 0x22 && hw.count == 0x33 && !(hw.chcr & 1u));
                    card_block = expect; card_at = 0; ++restarts; dense_left = 0;
                    command_framing(); hw.sar = 0x11; hw.dar = 0x22; hw.count = 0x33;
                    memset(ring, 0xa5, sizeof ring);
                    assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
                    break;
                }
                unsigned li = 0; bool bad = false;
                for(unsigned i = 0; i < 512u; ++i) {
                    if(li < b.lost_count && b.lost[li] == i) { ++li; continue; }
                    if(b.data[i] != data_byte(expect, i)) bad = true;
                }
                assert(li == b.lost_count);
                if(bad) {
                    /* Only the corner the caller's sector check covers: lost
                     * bytes in the block and a lost byte in the gap before it. */
                    assert(b.lost_count && n.gap_idle + n.gap_token != last_gaps);
                    ++wrong;
                    assert(kui_wire_stop(&w));
                    card_block = expect; card_at = 0; ++restarts; dense_left = 0;
                    command_framing(); hw.sar = 0x11; hw.dar = 0x22; hw.count = 0x33;
                    assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
                    break;
                }
                ++expect; ++total; idle = 0; last_gaps = n.gap_idle + n.gap_token;
            }
            if(++idle > 5000u) { fprintf(stderr, "stuck: state %u head %u tail %u floor %u phase %u holes %u run %u\n", w.state, w.head, w.tail, w.floor, w.phase, w.hole_put - w.hole_get, run); return 1; }
        }
        assert(kui_wire_stop(&w));
        /* Both spare timers are as the game left them. */
        assert(hw.tstr == timers_before && !(hw.tcr[1] & 0x20u) && !(hw.tcr[2] & 0x20u));
        /* Every overrun the model produced was accounted as a cut or a run's end
         * (the last run of each stream is stopped without being counted). */
        assert(n.cuts + n.segments <= overruns - overruns_before && overruns - overruns_before <= n.cuts + n.segments + n.starts);
        assert(n.blocks >= 15000u - 1u && n.lost_frame == 0u);
        /* Without stalls the only lost bytes are the one behind each run. */
        if(!stall_every) assert(!n.cuts && !n.waits && !n.unsolved && !n.bad_crc && !n.ambiguous && n.starts == 1u &&
            n.rebuilt + n.holed <= n.segments);
        assert(w.mode == 0u && n.unsolved * 50u <= n.blocks);
        printf("  run %2u (timer %u, stall every ~%u, %u%% busy): blocks %u, rebuilt %u, unsolved %u, holed %u, cuts %u, runs %u, waits %u, fulls %u, gap idle/token %u/%u, stops crc/ambiguous %u/%u\n",
            run, run % 5u == 4u ? 0u : run % 2u ? 1u : 2u, stall_every, stall_percent_dense, n.blocks, n.rebuilt, n.unsolved, n.holed, n.cuts, n.segments, n.waits, n.fulls, n.gap_idle, n.gap_token, n.bad_crc, n.ambiguous);
    }
    assert(!rewinds[1]);
#if KUI_TOY_WIRE_MEASURE
    /* A byte between blocks that is neither idle nor the token: the stream
     * cannot be followed, and the report keeps what lay around the byte. */
    for(unsigned trial = 0; trial < 3u; ++trial) {
        seed = 5u + trial;
        memset(&hw, 0, sizeof hw); hw.dmaor = 0x8201u;
        for(unsigned i = 0; i < 64; ++i) hw.slots[i] = 0x8c0be408u;
        stall_every = 0; until_stall = 100u; dense_left = 0;
        stray_block = 40u + trial * 23u;
        while(gap_of(stray_block) < 5u) ++stray_block;
        stray_value = trial == 0u ? 0xfd : trial == 1u ? 0x7e : 0x00; /* card order */
        card_block = 1; card_at = 0;
        memset(&w, 0, sizeof w); memset(&n, 0, sizeof n);
        command_framing();
        memset(ring, 0xa5, sizeof ring);
        assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
        uint32_t handed = 0;
        enum kui_wire_parse p = KUI_WIRE_NEED;
        for(unsigned turn = 0; turn < 100000u && p >= KUI_WIRE_NEED; ++turn) {
            pump(1u + rnd() % 3000u);
            kui_wire_service(&w);
            struct kui_wire_block b;
            while((p = kui_wire_next(&w, &b)) == KUI_WIRE_BLOCK) ++handed;
        }
        assert(p == KUI_WIRE_LOST_FRAME && n.lost_frame == 1u && handed == stray_block - 1u);
        const struct kui_wire_frame_note *note = &n.frame_note[0];
        const uint8_t wire = rev8(stray_value);
        unsigned bit = 0;
        while(wire >> bit & 1u) ++bit;
        /* Four idle bytes ahead of it, itself, and behind it the token and
         * the block's first bytes as far as they had arrived. */
        assert(note->before == 0xffffffffu && note->after >> 24 == wire);
        const unsigned arrived = note->gap >> 24 & 15u;
        assert(arrived >= 1u && arrived <= 4u);
        if(arrived >= 2u) assert((note->after >> 16 & 0xffu) == 0x7fu);
        if(arrived >= 3u) assert((note->after >> 8 & 0xffu) == rev8(data_byte(stray_block, 0)));
        assert((note->gap & 0xffffu) == gap_of(stray_block) - 1u);
        assert((note->blocks & 0x0fffffffu) == handed && note->blocks >> 28 >= 1u && note->blocks >> 28 <= 3u);
        /* Without stalls the only lost bytes are those behind each run. */
        assert(note->since_lost == 0xffffffffu || note->since_lost <= KUI_WIRE_SEGMENT_MAX + 2u);
        assert(!(note->gap >> 16 & 0xffu) || note->since_lost <= 4u);
        assert((n.frame_bit[bit >> 2] >> 8u * (bit & 3u) & 0xffu) == 1u &&
            n.frame_bit[0] + n.frame_bit[1] == 1u << 8u * (bit & 3u));
        assert(n.frame_kind == 1u << 8u * (note->blocks >> 28));
        assert(kui_wire_stop(&w));
    }
    stray_block = 0;
    printf("  a byte between blocks that is neither idle nor the token: stream stopped, bytes around it kept\n");
#endif
    printf("PASS Test %u card stream: %lu blocks checked against the card over %lu overruns; %lu streams restarted "
           "(clean block failing CRC16 %lu, too many lost bytes %lu); %lu uncheckable blocks handed on wrong, "
           "each with lost bytes behind a lost gap byte, left to the sector check\n",
        KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_CACHED ? 70u : 68u, total, overruns, restarts, rewinds[2], rewinds[3], wrong);
    return 0;
}
