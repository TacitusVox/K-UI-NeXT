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
 * token-looking bytes: blocks come in card order, none skipped or repeated.
 * A block is judged by where the stream took it from: from its own place
 * every byte not listed as lost equals the card's, and a block handed on
 * with no lost byte after a whole CRC16 is never from another. A wrong
 * block, one read from beside its place, is handed on only in the one
 * corner the design leaves to the caller's sector check: behind a byte
 * lost between blocks, and only when its CRC16 was not taken whole. That
 * is one with lost bytes listed (a single one where the CRC16 could not
 * place it and no other reading was open), one whose CRC16 itself lost a
 * byte, or one whose single lost byte the CRC16 seemed to explain, which
 * the remainder of wrong bytes does once in 256 times at large
 * (toy_wire_stream.h; it is why the caller confirms a rebuilt byte from
 * its sector's P parity, or else passes the sector through its EDC).
 * The test counts each kind and makes the last two by construction. Under
 * the model's token-only answer to the bit order question, blocks with
 * lost bytes go on with every byte the other way round until a whole
 * block exposes the answer: those are counted apart.
 *
 * Up to Test 75 the model clocked a byte with every look at the receiver's
 * status, and its twelve courses met the first kind only; the test then
 * said that no other was possible, which it never was entitled to. Test
 * 76's review made the clock a byte for every fourth look, and the courses
 * changed. The second kind came up in the twelve courses of every build
 * that paces its visits and does not end a stream at once (Tests 72 to
 * 75). The third came up in the same test built without the sanitizers,
 * where the stall draw then took its two numbers in another order, and
 * comes up in longer runs. The stream is as it was: toy_wire_stream.h has
 * said since Test 74 what a rebuilt block is worth, and the window has
 * checked accordingly.
 *
 * This is a model, not a Dreamcast: it checks logic and accounting, never
 * bus timing. */
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
static _Alignas(32) uint8_t ring[KUI_WIRE_RING_BYTES];
/* The tables, and with Test 76's APART a block buffer's room before them
 * and another behind them. */
static _Alignas(32) uint8_t table_room[KUI_WIRE_BLOCK_BYTES + KUI_WIRE_TABLE_BYTES + KUI_WIRE_BLOCK_BYTES];
#define tables (table_room + KUI_WIRE_BLOCK_BYTES)
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
static bool card_token;              /* the byte just clocked out was a block's token */
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
    card_token = card_at == gap;
    if(++card_at == gap + 515u) { card_at = 0; ++card_block; }
    ++clocked;
    return v;
}
/* ---- wire ---- */
static unsigned stall_percent_dense = 0, dense_left = 0, stall_every = 0;
static uint32_t until_stall;
static bool receiving(void) { return hw.scr == 0x50 && !(hw.ssr & 0x20) && !(hw.pdtr & 0x80) && !(hw.stbcr & 1); }
static unsigned long overruns;
/* Test 71's diagnostics: one stall, this many bytes on, at whose overrun
 * the channel takes the held byte while the receiver still shows it full,
 * so that the reader stores it a second time. Not something the earlier
 * tests established the hardware does; the console's notes look like it. */
static uint32_t twice_in;
static bool twice_now;
/* Test 74's question to the console, in three answers: the receiver ignores
 * SCSCMR1.SDIR (0), delivers every byte MSB first while it is set (1), or
 * delivers only the token that way and the rest as before (2), which no
 * hardware is known to do and the stream must still not be fooled by. */
static unsigned sdir_answer;
static unsigned looks;      /* at the receiver's status */
/* Card bytes to lose, each a block and a place in its frame of gap, token,
 * data and CRC16: the byte before it stalls, so that it is the one that
 * overruns. */
static struct { uint32_t block, at; } lose[32];
static unsigned lose_count;
static void shift_one(void) {
    const uint32_t now_block = card_block, now_at = card_at;
    const uint8_t sent = card_byte();
    uint8_t wire = (hw.scmr & 8u) && (sdir_answer == 1u || (sdir_answer == 2u && card_token)) ? sent : rev8(sent);
    bool stall = false;
    if(twice_in && !--twice_in) { stall = true; twice_now = true; }
    for(unsigned i = 0; i < lose_count; ++i) if(lose[i].block == now_block && lose[i].at == now_at + 1u) stall = true;
    if(stall_every) { if(!until_stall--) { stall = true; if(dense_left) { --dense_left; const uint32_t within = rnd() % 100u < 41u ? 15u : 63u; until_stall = 1u + rnd() % within; } else { until_stall = rnd() % 100u < stall_percent_dense ? (dense_left = 3u + rnd() % 30u, 1u + rnd() % 40u) : 64u + rnd() % (2u * stall_every); } } }
    bool dma = (hw.chcr & 3u) == 1u && hw.count && hw.sar == 0x1fe00014u && (hw.dmaor & 7u) == 1u;
    if(dma && !stall && !(hw.ssr & 0x40)) {
        assert(hw.dar >= PHYS && hw.dar < PHYS + KUI_WIRE_RING_BYTES);
        ring[hw.dar - PHYS] = wire; ++hw.dar; if(!--hw.count) hw.chcr |= 2u;
    } else if(!(hw.ssr & 0x40)) { hw.rdr = wire; hw.ssr |= 0x40; }
    else { hw.ssr |= 0x20; ++overruns;
        if(twice_now) {
            twice_now = false;
            assert(dma);
            ring[hw.dar - PHYS] = hw.rdr; ++hw.dar; if(!--hw.count) hw.chcr |= 2u;
        } else
        /* the channel, back on the bus, may still take the held byte */
        if(dma && (rnd() & 1u)) { ring[hw.dar - PHYS] = hw.rdr; ++hw.dar; if(!--hw.count) hw.chcr |= 2u; hw.ssr &= (uint8_t)~0x40; } }
}
static void pump(unsigned n) { while(n-- && receiving()) shift_one(); }
uint32_t kui_wire_test_read(uint32_t a, unsigned width) {
    (void)width;
    switch(a) {
    case 0xffe00000u: return hw.smr; case 0xffe00004u: return hw.brr; case 0xffe00008u: return hw.scr;
    /* Time passes with every poll: a byte for every fourth, since a look
     * at the status is far shorter than a byte on the wire. */
    case 0xffe00010u: if(receiving() && !(++looks & 3u)) shift_one(); return hw.ssr;
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
        /* Command framing never runs with the bit order changed. */
        if(v == 0x30) assert(!hw.scmr);
        hw.scr = (uint8_t)v; return;
    case 0xffe00010u: if((hw.ssr & 0x20) && !(v & 0x20)) stale = true; hw.ssr = (uint8_t)((hw.ssr & (v | ~0xf8u)) | (hw.ssr & 0x04u)); return;
    case 0xffe00018u:
        /* Written only with the SCI stopped, and only zero or SDIR. */
        assert(!hw.scr && (v == 0 || v == 8) && (KUI_TOY_WIRE_MSB || !v));
        hw.scmr = (uint8_t)v; return;
    case 0xffe0001cu: hw.sptr = (uint8_t)v; return;
    /* The module reset is not relied on to clear SDIR: it is zero already. */
    case 0xffc00004u: if(!(hw.stbcr & 1u) && (v & 1u)) { assert(!hw.scmr); hw.scr = 0; hw.smr = 0; hw.brr = 0xff; hw.ssr = 0x84; hw.rdr = 0; stale = false; } hw.stbcr = (uint8_t)v; return;
    case 0xff800030u: hw.pdtr = (uint16_t)v; return;
    case 0xffa00010u: hw.sar = v; return; case 0xffa00014u: hw.dar = v; return; case 0xffa00018u: hw.count = v; return;
    /* The channel's end flag is cleared by writing it as zero, never set by writing it. */
    case 0xffa0001cu: hw.chcr = (v & ~2u) | (hw.chcr & v & 2u); return;
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
#if KUI_TOY_WIRE_RING
/* Test 75 reads the ring through the cache. This is the ring as a cache
 * that never lets go of a line would show it: each line holds what memory
 * held when the reader last dropped it, and until its first drop nothing
 * the card ever sent. A line read without being dropped first gives the
 * reader old bytes, which the checks on every word run then meet. */
static _Alignas(32) uint8_t seen[KUI_WIRE_RING_BYTES];
static unsigned long drops;
const uint8_t *kui_wire_test_view(const uint8_t *area) { assert(area == ring); return seen; }
void kui_wire_test_drop(const uint8_t *line) {
    /* Any address inside the line names it, as for the instruction. */
    assert(line >= seen && line < seen + sizeof(seen));
    const size_t at = (size_t)(line - seen) & ~(size_t)31u;
    memcpy(seen + at, ring + at, 32u);
    ++drops;
}
#endif

static struct kui_wire_stream w;
static struct kui_wire_counters n;
/* A stream is given up. How long the card is clocked on if it was
 * receiving: up to Test 75 until the run's own next overrun, which is up
 * to a segment away; with Test 76's QUIT the byte the receiver then holds
 * and the one that overruns it. */
static unsigned long stops_receiving, stop_bytes, stop_most;
#if KUI_TOY_WIRE_APART
static unsigned long apart[2];      /* blocks taken into the first buffer and into the second */
#endif
static bool stream_stop(void) {
    const uint64_t before = clocked;
    const bool ran = w.state == KUI_WIRE_RUN;
    const bool usable = kui_wire_stop(&w);
    const unsigned long waited = (unsigned long)(clocked - before);
    if(ran) {
        ++stops_receiving;
        stop_bytes += waited;
        if(waited > stop_most) stop_most = waited;
#if KUI_TOY_WIRE_QUIT
        assert(waited <= 2u);
#endif
    } else assert(!waited);
    return usable;
}
static void command_framing(void) { hw.scr = 0x30; hw.smr = 0x80; hw.brr = 0; hw.scmr = 0; hw.ssr = 0x84; hw.sptr = 0x05; hw.pdtr |= 0x80; hw.chcr = 0; stale = false; }
static bool timer_armed(void) { return w.mode && (hw.tstr & (1u << w.mode)) && (hw.tcr[w.mode] & 0x20); }

/* By construction: the card from block `first` on with the listed bytes
 * lost and no other stall, no timer free (a wait ends at the next call),
 * the receiver's answer to the bit order question as given. */
static void construct_start(uint32_t first, unsigned answer, bool counters) {
    seed = 31u;
    memset(&hw, 0, sizeof hw); hw.dmaor = 0x8201u; hw.tstr = 6u;
    for(unsigned i = 0; i < 64; ++i) hw.slots[i] = 0x8c0be408u;
    stall_every = 0; until_stall = 100u; dense_left = 0;
    sdir_answer = answer;
    card_block = first; card_at = 0;
    memset(&w, 0, sizeof w);
    if(counters) memset(&n, 0, sizeof n);
    command_framing();
    memset(ring, 0xa5, sizeof ring);
    assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
}
/* Blocks, until `want` have come or the stream says something else. */
static enum kui_wire_parse construct_take(unsigned want, struct kui_wire_block *b, unsigned *came) {
    enum kui_wire_parse p = KUI_WIRE_NEED;
    for(unsigned turn = 0; turn < 100000u && p >= KUI_WIRE_NEED && *came < want; ++turn) {
        pump(1u + rnd() % 3000u);
        kui_wire_service(&w);
        while(*came < want && (p = kui_wire_next(&w, b)) == KUI_WIRE_BLOCK) ++*came;
    }
    return p;
}
static void lose_byte(uint32_t block, uint32_t at) {
    /* The byte before it stalls: it has to be one the card sends in the same block. */
    assert(at && lose_count < sizeof lose / sizeof lose[0]);
    lose[lose_count].block = block; lose[lose_count++].at = at;
}

int main(int argc, char **argv) {
    /* What the statements below rest on besides the CRC16 is the card's:
     * no block of idle-looking bytes has a CRC16 byte that looks idle too.
     * Such a block, its token lost, could be passed over whole as a gap. */
    for(uint32_t block = 1; block <= BLOCKS / 4u + 2u; ++block) {
        if(block % 13u != 6u) continue;
        uint8_t data[512];
        for(unsigned i = 0; i < 512u; ++i) data[i] = data_byte(block, i);
        const uint16_t crc = crc16(data, 512u);
        assert(crc >> 8 != 0xffu && (crc & 0xffu) != 0xffu);
    }
    unsigned long total = 0, restarts = 0, wrong[3] = {0}, wrong_same = 0, rewinds[4] = {0};
#if KUI_TOY_WIRE_CLEAN
    unsigned long kinds[3] = {0};
#endif
#if KUI_TOY_WIRE_MSB
    unsigned long orders[3] = {0}, unsettled = 0, backs = 0;
#endif
    printf("Test %u card stream on the wire model:\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_SEQ ? 77u : KUI_TOY_WIRE_FLUID ? 76u : KUI_TOY_WIRE_STEADY ? 75u : KUI_TOY_WIRE_LEAN ? 74u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u);
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
        /* Where in the stream the wanted block's data begins. A stream
         * begins with the wanted block's gap, and every byte the card
         * sends has a position, lost or not. */
        uint32_t place = gap_of(expect) + 1u;
#if KUI_TOY_WIRE_MSB
        unsigned undecided = 0;
        bool backed = false;    /* an honoured order given up */
        sdir_answer = run % 3u;
#endif
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
#if KUI_TOY_WIRE_PACE
            /* Overdue is exactly: an overrun not yet answered, a wait whose
             * timer has run out, or a wait without a timer. */
            if(w.state == KUI_WIRE_RUN) {
                const bool was = (hw.ssr & 0x20u) != 0u, overdue = kui_wire_overdue(&w);
                assert((!was || overdue) && (!overdue || (hw.ssr & 0x20u)));
            } else if(w.state == KUI_WIRE_WAIT) {
                assert(kui_wire_overdue(&w) == (!w.mode || (hw.tcr[w.mode] & 0x100u)));
            } else assert(!kui_wire_overdue(&w));
#endif
            if(w.state == KUI_WIRE_WAIT && timer_armed() && !(rnd() % 3u)) {
                /* Some waits end with a caller waiting for a position a few
                 * bytes on: the run is cut to it, also right at the ring's end. */
                if(run % 3u == 2u) w.want = w.head + 1u + rnd() % 60u;
                hw.tcr[w.mode] |= 0x100u;
#if KUI_TOY_WIRE_PACE
                assert(kui_wire_overdue(&w));
#endif
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
                const uint32_t unchecked_before = n.unchecked, rebuilt_before = n.rebuilt;
#if KUI_TOY_WIRE_MSB
                const uint32_t order_before = n.order;
#endif
                const enum kui_wire_parse p = kui_wire_next(&w, &b);
                if(p == KUI_WIRE_NEED) break;
                if(p != KUI_WIRE_BLOCK) {
                    /* the caller's rewind: stop, the low reader seeks to the block still wanted */
#if KUI_TOY_WIRE_MSB
                    /* A lost byte ahead of the very first token leaves the
                     * bit order undecided: that stream is given up. (No
                     * course here loses one: a run's first stall comes a
                     * hundred bytes in, behind its first token. The case
                     * is made by construction below.) */
                    if(p == KUI_WIRE_LOST_FRAME && n.order == KUI_WIRE_ORDER_TRY) ++undecided;
                    else
#endif
                    ++rewinds[-(int)p];
#if KUI_TOY_WIRE_MSB
                    /* An honoured order is given up in one case: before a
                     * whole block has settled it, a reading with no lost
                     * byte fails its CRC16 with no other open. The stream
                     * cannot tell a wrong order from a wrong place there,
                     * and reads the old way from then on, which is right
                     * either way. */
                    if(sdir_answer == 1u && n.order == KUI_WIRE_ORDER_BACK && !backed) {
                        assert(p == KUI_WIRE_BAD_CRC && order_before == KUI_WIRE_ORDER_SEEN);
                        backed = true;
                    }
#endif
                    assert(stream_stop());
                    assert(hw.scr == 0x30 && hw.smr == 0x80 && !hw.scmr && (hw.pdtr & 0x80) && hw.sar == 0x11 && hw.dar == 0x22 && hw.count == 0x33 && !(hw.chcr & 1u));
                    card_block = expect; card_at = 0; ++restarts; dense_left = 0;
                    command_framing(); hw.sar = 0x11; hw.dar = 0x22; hw.count = 0x33;
                    memset(ring, 0xa5, sizeof ring);
                    assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
                    /* What the stream given up read of its gaps is not the next one's. */
                    last_gaps = n.gap_idle + n.gap_token;
                    place = gap_of(expect) + 1u;
                    break;
                }
                unsigned li = 0; bool bad = false;
                for(unsigned i = 0; i < 512u; ++i) {
                    if(li < b.lost_count && b.lost[li] == i) { ++li; continue; }
                    if(b.data[i] != data_byte(expect, i)) bad = true;
                }
                assert(li == b.lost_count);
                /* A block is judged by its place before its bytes: where
                 * the stream took it from against where the card has it.
                 * Its bytes alone do not say: the card has blocks of FF
                 * that read the same from three bytes beside. */
                const bool misplaced = w.tail - 514u != place;
#if KUI_TOY_WIRE_APART
                {
                    /* Test 76 (APART): the block lies in one of the two
                     * buffers behind the tables, and no cache line of that
                     * buffer has the place (address bits 5 to 13) of a
                     * line its bytes were read from in the ring through
                     * the cache. A block that runs over the ring's end is
                     * placed by where it begins. */
                    const uint8_t *const first = table_room;
                    assert(b.data == first || b.data == tables + KUI_WIRE_TABLE_BYTES);
                    ++apart[b.data != first];
#if KUI_TOY_WIRE_RING
                    const uint32_t from = (w.tail - 514u) & (KUI_WIRE_RING_BYTES - 1u);
                    if(from + 514u <= KUI_WIRE_RING_BYTES)
                        for(uintptr_t line = (uintptr_t)(seen + from) >> 5; line <= (uintptr_t)(seen + from + 513u) >> 5; ++line)
                            assert(((line - ((uintptr_t)b.data >> 5)) & 511u) >= 16u);
                    /* The second is used for nothing else. */
                    if(b.data != first)
                        assert(((((uintptr_t)(seen + from) >> 5) - ((uintptr_t)first >> 5) + 17u) & 511u) < 33u);
#endif
                }
#endif
#if KUI_TOY_WIRE_CLEAN
                /* What the caller may build on: a block called clean has
                 * every byte, every byte right, and is the block wanted.
                 * One called rebuilt has
                 * every byte; that the bytes are right is for the caller to
                 * confirm (toy_wire_stream.h, and the corner below). The
                 * kinds are exactly what the counters say: rebuilt, or left
                 * with lost bytes or with a CRC16 that lost one. */
                assert(b.kind == KUI_WIRE_CLEAN || b.kind == KUI_WIRE_REBUILT || b.kind == KUI_WIRE_LOOSE);
                if(b.kind != KUI_WIRE_LOOSE) assert(!b.lost_count);
                if(b.kind == KUI_WIRE_CLEAN) assert(!bad && !misplaced);
                if(b.kind == KUI_WIRE_REBUILT) { assert(b.rebuilt_at < 512u); ++kinds[2]; }
                else ++kinds[b.kind == KUI_WIRE_CLEAN];
                assert((b.kind == KUI_WIRE_REBUILT) == (n.rebuilt != rebuilt_before));
                assert((b.kind == KUI_WIRE_LOOSE) == (b.lost_count || n.unchecked != unchecked_before));
#endif
#if KUI_TOY_WIRE_MSB
                /* Nothing but a block with no lost byte settles the order.
                 * Before one has, a block is handed on only for the
                 * sector's check, with lost bytes listed or a CRC16 that
                 * lost one: never as clean or rebuilt. With the token-only
                 * answer such a block, taken from its own place, has every
                 * byte the other way round, and the sector's check is what
                 * refuses it. One taken from beside its place is the corner
                 * below, whatever the answer. */
                if(n.order == KUI_WIRE_ORDER_SEEN) {
                    assert(sdir_answer && (b.lost_count || n.unchecked != unchecked_before));
#if KUI_TOY_WIRE_CLEAN
                    assert(b.kind == KUI_WIRE_LOOSE);
#endif
                    if(sdir_answer == 2u && !misplaced) {
                        for(unsigned i = 0, l = 0; i < 512u; ++i) {
                            if(l < b.lost_count && b.lost[l] == i) { ++l; continue; }
                            assert(b.data[i] == rev8(data_byte(expect, i)));
                        }
                        ++unsettled; ++expect; place += 515u + gap_of(expect);
                        last_gaps = n.gap_idle + n.gap_token;
                        continue;
                    }
                } else if(sdir_answer == 1u && backed) assert(n.order == KUI_WIRE_ORDER_BACK);
                else assert((n.order == KUI_WIRE_ORDER_MSB) == (sdir_answer == 1u) &&
                    (n.order == KUI_WIRE_ORDER_LSB) == (sdir_answer == 0u));
#endif
                /* From its own place every byte not listed as lost is the card's. */
                if(!misplaced) assert(!bad);
                else {
                    /* Only the corner the caller's sector check covers: a
                     * byte lost in the gap before the block (since the
                     * block before it, or since this stream began), so that
                     * its beginning was read wrong, and a block whose CRC16
                     * was not taken whole: lost bytes listed, a lost byte
                     * in the CRC16 itself, or one lost byte that the
                     * remainder of the wrong bytes seemed to explain. Never
                     * a block that passed its CRC16 whole. */
                    assert(n.gap_idle + n.gap_token != last_gaps);
                    const unsigned how = b.lost_count ? 0u : n.unchecked != unchecked_before ? 1u : 2u;
                    if(how == 2u) assert(n.rebuilt != rebuilt_before);
                    ++wrong[how];
                    /* Its bytes may be the card's all the same. */
                    if(!bad) ++wrong_same;
                    assert(stream_stop());
                    card_block = expect; card_at = 0; ++restarts; dense_left = 0;
                    command_framing(); hw.sar = 0x11; hw.dar = 0x22; hw.count = 0x33;
                    assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
                    last_gaps = n.gap_idle + n.gap_token;
                    place = gap_of(expect) + 1u;
                    break;
                }
                ++expect; ++total; idle = 0; last_gaps = n.gap_idle + n.gap_token;
                place += 515u + gap_of(expect);
            }
            if(++idle > 5000u) { fprintf(stderr, "stuck: state %u head %u tail %u floor %u phase %u holes %u run %u\n", w.state, w.head, w.tail, w.floor, w.phase, w.hole_put - w.hole_get, run); return 1; }
        }
        assert(stream_stop());
#if KUI_TOY_WIRE_LEAN && !KUI_TOY_WIRE_STEADY
        /* Runs cut again soon after they began: each was a cut short of a
         * good run, which is what a wait answers; the rest of the waits
         * answer a first run's cut, or one behind a full run or a stop for
         * room. */
        {
            uint32_t again = 0;
            for(unsigned i = 0; i < 2u; ++i) for(unsigned j = 0; j < 3u; ++j) again += n.again[i][j];
            assert(again <= n.waits && n.waits - again <= n.segments + n.starts + n.fulls);
            if(stall_every && stall_percent_dense) assert(n.again[0][0] + n.again[0][1] + n.again[0][2] && n.again[1][0] + n.again[1][1] + n.again[1][2]);
            if(!stall_every) assert(!again);
        }
#endif
#if KUI_TOY_WIRE_MSB
        /* The console's answer was read right: ignored, honoured, or the
         * token alone, which the first block's CRC16 exposed once. */
        assert(n.order == (sdir_answer == 0u ? KUI_WIRE_ORDER_LSB : sdir_answer == 1u && !backed ? KUI_WIRE_ORDER_MSB : KUI_WIRE_ORDER_BACK));
        backs += backed;
        assert(!hw.scmr && n.lost_frame == undecided && n.undecided == undecided && undecided < 4u);
        ++orders[sdir_answer];
#endif
        /* Both spare timers are as the game left them. */
        assert(hw.tstr == timers_before && !(hw.tcr[1] & 0x20u) && !(hw.tcr[2] & 0x20u));
        /* Every overrun the model produced was accounted as a cut or a run's end
         * (the last run of each stream is stopped without being counted). */
        assert(n.cuts + n.segments <= overruns - overruns_before && overruns - overruns_before <= n.cuts + n.segments + n.starts);
#if KUI_TOY_WIRE_MSB
        /* Set aside what settling the order cost: the streams given up
         * undecided, and for the token-only answer the one block that
         * exposed it. The counts below are then as without the question. */
        if(sdir_answer == 2u || backed) { assert(n.bad_crc >= 1u && n.starts >= 2u); --n.bad_crc; --n.starts; }
        n.lost_frame = 0; n.starts -= undecided;
#endif
        assert(n.blocks >= 15000u - 1u && n.lost_frame == 0u);
        /* Without stalls the only lost bytes are the one behind each run. */
        if(!stall_every) assert(!n.cuts && !n.waits && !n.unsolved && !n.bad_crc && !n.ambiguous && n.starts == 1u &&
            n.rebuilt + n.holed <= n.segments);
        assert(w.mode == 0u && n.unsolved * 50u <= n.blocks);
        printf("  run %2u (timer %u, stall every ~%u, %u%% busy): blocks %u, rebuilt %u, unsolved %u, holed %u, cuts %u, runs %u, waits %u, fulls %u, gap idle/token %u/%u, stops crc/ambiguous %u/%u\n",
            run, run % 5u == 4u ? 0u : run % 2u ? 1u : 2u, stall_every, stall_percent_dense, n.blocks, n.rebuilt, n.unsolved, n.holed, n.cuts, n.segments, n.waits, n.fulls, n.gap_idle, n.gap_token, n.bad_crc, n.ambiguous);
    }
    assert(!rewinds[1]);
#if KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_PACE
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
        assert(stream_stop());
    }
    stray_block = 0;
    printf("  a byte between blocks that is neither idle nor the token: stream stopped, bytes around it kept\n");
#endif
#if KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_PACE
    /* A held byte stored twice: the block it lies in has one position too
     * many, the reader finds that block's own last CRC byte where the space
     * behind it should begin, and the report says what it found there. */
    for(unsigned trial = 0; trial < 4u; ++trial) {
        seed = 900u + trial;
        memset(&hw, 0, sizeof hw); hw.dmaor = 0x8201u;
        for(unsigned i = 0; i < 64; ++i) hw.slots[i] = 0x8c0be408u;
        stall_every = 0; until_stall = 100u; dense_left = 0;
        card_block = 1; card_at = 0;
        memset(&w, 0, sizeof w); memset(&n, 0, sizeof n);
        command_framing();
        memset(ring, 0xa5, sizeof ring);
        assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
        /* Well inside a block some thousand bytes on, away from a run's end. */
        twice_in = 20000u + 3000u * trial;
        uint32_t handed = 0;
        enum kui_wire_parse p = KUI_WIRE_NEED;
        for(unsigned turn = 0; turn < 100000u && p >= KUI_WIRE_NEED; ++turn) {
            pump(1u + rnd() % 3000u);
            kui_wire_service(&w);
            struct kui_wire_block b;
            while((p = kui_wire_next(&w, &b)) == KUI_WIRE_BLOCK) ++handed;
        }
        if(twice_in || twice_now || p != KUI_WIRE_LOST_FRAME || n.lost_frame != 1u)
            fprintf(stderr, "twice trial %u: in %u now %d parse %d lost_frame %u handed %u bad_crc %u unsolved %u\n",
                trial, (unsigned)twice_in, twice_now, (int)p, (unsigned)n.lost_frame, (unsigned)handed,
                (unsigned)n.bad_crc, (unsigned)n.unsolved);
        assert(!twice_in && !twice_now && p == KUI_WIRE_LOST_FRAME && n.lost_frame == 1u);
        assert(n.frame_end == 1u && n.frame_held == 1u && n.frame_twice == 1u);
        const struct kui_wire_frame_note *note = &n.frame_note[0];
        /* Directly behind the block's end, after a block handed on with a
         * lost byte the CRC16 could not explain. */
        assert(!(note->gap & 0xffffu) && note->blocks >> 28 == 3u && n.unsolved == 1u);
        assert(note->since_lost && note->since_lost < 514u);
        assert(stream_stop());
    }
    printf("  a held byte stored twice: found as an unexpected byte directly behind its block, and counted as such\n");
#endif
    /* The corner by construction. A block whose data begins like two idle
     * bytes and a token, and whose own token is lost: the reader first
     * takes the token-looking byte for the token and reads three bytes
     * late. With nothing else lost that reading fails its CRC16 and the
     * other is taken. What the CRC16 cannot refuse:
     * (0) the second of the two bytes the late reading takes for its CRC16
     *     is lost as well, and the byte behind it looks like the space
     *     between blocks: handed on, wrong, unchecked;
     * (1) the same with a data byte behind it: the look behind refuses the
     *     late reading, and the block is handed on right and clean;
     * (2) one data byte is lost, and the late reading's remainder is one a
     *     single byte at that place can leave: handed on rebuilt, wrong. A
     *     block with its CRC16 has remainder zero, so the late reading's
     *     remainder comes from the three bytes it drops and the three it
     *     takes in alone, and with it the places where a lost byte fits:
     *     the same few for every such block, whatever its data. (The
     *     design reckons with one reading in 256 fitting for wrong data at
     *     large; this shape has its own share.) Every place from 8 to 499
     *     is tried on two blocks.
     * In each try the bytes the stream logs as lost are exactly the two
     * listed. The streams given up here, their overruns and the lines
     * they read are not the courses': the counts of those are put back
     * behind this. */
    {
        const unsigned long stops_before = stops_receiving, bytes_before = stop_bytes, most_before = stop_most;
        const unsigned long overruns_kept = overruns;
#if KUI_TOY_WIRE_RING
        const unsigned long drops_before = drops;
#endif
        unsigned long tries = 0, right = 0;
        unsigned made[3] = {0, 0, 0}, fits[2] = {0, 0}, place[2][8];
        for(uint32_t block = 10u; block < 2000u && (!made[0] || !made[1] || made[2] < 2u); block += 7u) {
            /* Every block here is 3 of 7: its data begins FF FF FE. */
            if(!gap_of(block)) continue;
            /* The late reading ends on the next gap's third byte. (0) and
             * (2) want idle bytes there and behind; (1) wants the next
             * block's token there and a byte of its data behind, neither
             * idle nor token. */
            const unsigned which = gap_of(block + 1u) >= 5u ? (made[0] ? 2u : 0u) :
                gap_of(block + 1u) == 2u && data_byte(block + 1u, 0) < 0xfeu ? 1u : 3u;
            if(which == 3u || made[which] >= (which == 2u ? 2u : 1u)) continue;
            /* Where the block's token lies in a stream begun two blocks ahead of it. */
            const uint32_t token = gap_of(block - 2u) + 515u + gap_of(block - 1u) + 515u + gap_of(block);
            for(unsigned k = 8u; k < (which == 2u ? 500u : 9u); ++k) {
                seed = 31u + k;
                memset(&hw, 0, sizeof hw); hw.dmaor = 0x8201u; hw.tstr = 6u; /* no timer: a wait ends at the next call */
                for(unsigned i = 0; i < 64; ++i) hw.slots[i] = 0x8c0be408u;
                stall_every = 0; until_stall = 100u; dense_left = 0;
#if KUI_TOY_WIRE_MSB
                sdir_answer = 0;
#endif
                lose[0].block = block; lose[0].at = gap_of(block);
                if(which == 2u) { lose[1].block = block; lose[1].at = gap_of(block) + 1u + k; }
                else { lose[1].block = block + 1u; lose[1].at = 2u; }
                lose_count = 2u;
                card_block = block - 2u; card_at = 0;
                memset(&w, 0, sizeof w); memset(&n, 0, sizeof n);
                command_framing();
                memset(ring, 0xa5, sizeof ring);
                assert(kui_wire_start(&w, &n, ring, tables, 0x05, 0x8c421234u));
                uint32_t next = block - 2u;
                struct kui_wire_block b;
                enum kui_wire_parse p = KUI_WIRE_NEED;
                bool bad = false;
                for(unsigned turn = 0; turn < 100000u && p >= KUI_WIRE_NEED && next <= block; ++turn) {
                    pump(1u + rnd() % 3000u);
                    kui_wire_service(&w);
                    while(next <= block && (p = kui_wire_next(&w, &b)) == KUI_WIRE_BLOCK) {
                        bad = false;
                        for(unsigned i = 0, li = 0; i < 512u; ++i) {
                            if(li < b.lost_count && b.lost[li] == i) { ++li; continue; }
                            if(b.data[i] != data_byte(next, i)) bad = true;
                        }
                        /* The two blocks before it are whole and right. */
                        if(next < block) assert(!bad && !b.lost_count);
                        ++next;
                    }
                }
                /* Handed on either way, never refused: a stream is not given up for it. */
                assert(p == KUI_WIRE_BLOCK && next == block + 1u && !b.lost_count && n.blocks == 3u);
                /* The stream lost what the list says, and nothing else. */
                assert(w.hole_put == 2u && w.hole[0] == token && w.hole[1] == token + (which == 2u ? 1u + k : 517u));
                ++tries;
                if(which == 0u) {
                    /* The late reading, with nothing to check it by. */
                    assert(bad && n.unchecked == 1u && !n.rebuilt && n.gap_idle == 1u && !n.gap_token);
#if KUI_TOY_WIRE_CLEAN
                    assert(b.kind == KUI_WIRE_LOOSE);
#endif
                } else if(which == 1u) {
                    /* The late reading refused for what lies behind it; the other is whole. */
                    assert(!bad && !n.unchecked && !n.rebuilt && n.gap_token == 1u && !n.gap_idle);
#if KUI_TOY_WIRE_CLEAN
                    assert(b.kind == KUI_WIRE_CLEAN);
#endif
                } else if(bad) {
                    /* The late reading, its lost byte three places earlier in it. */
                    assert(n.rebuilt == 1u && !n.unchecked && n.gap_idle == 1u && !n.gap_token);
#if KUI_TOY_WIRE_CLEAN
                    assert(b.kind == KUI_WIRE_REBUILT && b.rebuilt_at == k - 3u);
#endif
                    assert(fits[made[2]] < 8u);
                    place[made[2]][fits[made[2]]++] = k;
                } else {
                    /* The late reading refused and the other taken: the byte rebuilt right. */
                    assert(n.rebuilt == 1u && !n.unchecked && n.gap_token == 1u && !n.gap_idle);
#if KUI_TOY_WIRE_CLEAN
                    assert(b.kind == KUI_WIRE_REBUILT && b.rebuilt_at == k);
#endif
                    ++right;
                }
                assert(stream_stop());
            }
            ++made[which];
        }
        lose_count = 0;
        assert(made[0] == 1u && made[1] == 1u && made[2] == 2u);
        /* Some places fit and most do not, and they are the same for both blocks. */
        assert(fits[0] && fits[0] == fits[1] && right == 2u * (492u - fits[0]));
        for(unsigned i = 0; i < fits[0]; ++i) assert(place[0][i] == place[1][i]);
        printf("  the corner by construction, a block read three bytes late behind its lost token: with a byte of that "
               "reading's CRC16 lost it is handed on unchecked where the byte behind looks like a gap, and refused for the "
               "right reading where it does not; with one data byte lost it is handed on rebuilt, and wrong, for %u of 492 "
               "places of that byte (", fits[0]);
        for(unsigned i = 0; i < fits[0]; ++i) printf("%s%u", i ? ", " : "", place[0][i]);
        printf("; the same on a second block) and rebuilt right for the others; %lu streams, the bytes lost in each "
               "exactly the two listed\n", tries);
        stops_receiving = stops_before; stop_bytes = bytes_before; stop_most = most_before;
        overruns = overruns_kept;
#if KUI_TOY_WIRE_RING
        drops = drops_before;
#endif
    }
    /* A block from beside its place that equals the card all the same. The
     * card has blocks of FF behind FF FF FE. With a gap byte lost three
     * ahead of the token, the reading from the token on is the right one;
     * with more lost bytes in it than a block is handed on with, the stream
     * takes the other reading, three bytes early, without a check: two gap
     * bytes and the token stand where FF FF FE belong, and the block's own
     * third byte, the FE, is among the lost. Every byte not listed is the
     * card's. The stream is three bytes out behind it, meets the block's
     * last byte and its CRC16 where a gap should be, and says so. */
    {
        const unsigned long stops_before = stops_receiving, bytes_before = stop_bytes, most_before = stop_most;
        const unsigned long overruns_kept = overruns;
#if KUI_TOY_WIRE_RING
        const unsigned long drops_before = drops;
#endif
        uint32_t block = 45u;
        while(gap_of(block) < 4u) block += 91u;
        assert(block % 7u == 3u && block % 13u == 6u);
        const uint32_t token = gap_of(block);
        lose_count = 0;
        lose_byte(block, token - 3u);
        lose_byte(block, token + 1u + 2u);
        for(unsigned k = 10u; k <= 230u; k += 10u) lose_byte(block, token + 1u + k);
        lose_byte(block, token + 1u + 510u);
        assert(lose_count == KUI_WIRE_BLOCK_LOST + 2u);
        construct_start(block - 2u, 0u, true);
        struct kui_wire_block b;
        unsigned came = 0;
        enum kui_wire_parse p = construct_take(3u, &b, &came);
        const uint32_t place = gap_of(block - 2u) + 515u + gap_of(block - 1u) + 515u + token + 1u;
        assert(p == KUI_WIRE_BLOCK && came == 3u && n.blocks == 3u && w.tail - 514u == place - 3u);
        assert(b.lost_count == KUI_WIRE_BLOCK_LOST && n.gap_token == 1u && !n.gap_idle && !n.rebuilt && !n.unchecked);
#if KUI_TOY_WIRE_CLEAN
        assert(b.kind == KUI_WIRE_LOOSE);
#endif
        for(unsigned i = 0, l = 0; i < 512u; ++i) {
            if(l < b.lost_count && b.lost[l] == i) { ++l; continue; }
            assert(b.data[i] == data_byte(block, i));
        }
        came = 0;
        p = construct_take(1u, &b, &came);
        assert(p == KUI_WIRE_LOST_FRAME && !came && n.lost_frame == 1u);
        assert(stream_stop());
        lose_count = 0;
        printf("  a block of FF read three bytes early, behind a lost gap byte and with more lost bytes than the right "
               "reading may have: handed on unchecked with %u lost bytes listed, every other byte the card's; the stream "
               "then finds itself beside the blocks and stops\n", (unsigned)KUI_WIRE_BLOCK_LOST);
#if KUI_TOY_WIRE_MSB
        /* The bit order left undecided. A stream that asks and loses a
         * byte ahead of its first token is given up; after four of them
         * the asking ends, and the next stream reads the old way without
         * asking, whatever the receiver would have answered. */
        for(unsigned ask = 1u; ask <= 4u; ++ask) {
            lose_count = 0;
            lose_byte(1u, gap_of(1u));
            construct_start(1u, 1u, ask == 1u);
            assert(hw.scmr == 8u);
            came = 0;
            p = construct_take(1u, &b, &came);
            assert(p == KUI_WIRE_LOST_FRAME && !came && n.undecided == ask && n.lost_frame == ask);
            assert(n.order == (ask < 4u ? KUI_WIRE_ORDER_TRY : KUI_WIRE_ORDER_LSB));
            assert(stream_stop());
        }
        lose_count = 0;
        construct_start(1u, 1u, false);
        assert(!hw.scmr);
        came = 0;
        p = construct_take(2u, &b, &came);
        assert(p == KUI_WIRE_BLOCK && came == 2u && n.order == KUI_WIRE_ORDER_LSB && !b.lost_count);
        for(unsigned i = 0; i < 512u; ++i) assert(b.data[i] == data_byte(2u, i));
        assert(stream_stop());
        /* An honoured order given up. Before a whole block has settled
         * the order, a block that begins FF FF FE loses its token and its
         * second byte: both readings left are from beside its place and
         * whole, both fail their CRC16, and the stream, which cannot tell
         * a wrong order from a wrong place, stops asking. The next stream
         * reads the old way, and right. */
        assert(3u % 7u == 3u && gap_of(3u));
        lose_count = 0;
        lose_byte(1u, gap_of(1u) + 1u + 89u);
        lose_byte(2u, gap_of(2u) + 1u + 411u);
        lose_byte(2u, gap_of(2u) + 1u + 441u);
        lose_byte(2u, gap_of(2u) + 1u + 491u);
        lose_byte(3u, gap_of(3u));
        lose_byte(3u, gap_of(3u) + 1u + 1u);
        construct_start(1u, 1u, true);
        came = 0;
        p = construct_take(3u, &b, &came);
        assert(p == KUI_WIRE_BAD_CRC && came == 2u && n.order == KUI_WIRE_ORDER_BACK && n.bad_crc == 1u && n.holed == 2u);
        assert(stream_stop());
        lose_count = 0;
        construct_start(3u, 1u, false);
        assert(!hw.scmr);
        came = 0;
        p = construct_take(1u, &b, &came);
        assert(p == KUI_WIRE_BLOCK && came == 1u && n.order == KUI_WIRE_ORDER_BACK && !b.lost_count);
        for(unsigned i = 0; i < 512u; ++i) assert(b.data[i] == data_byte(3u, i));
#if KUI_TOY_WIRE_CLEAN
        assert(b.kind == KUI_WIRE_CLEAN);
#endif
        assert(stream_stop());
        printf("  the bit order by construction: four streams that lose a byte ahead of their first token end the asking; "
               "an honoured order is given up when a whole reading from beside its place fails its CRC16 before a block "
               "has settled it; either way the next stream reads the old way, and right\n");
#endif
        stops_receiving = stops_before; stop_bytes = bytes_before; stop_most = most_before;
        overruns = overruns_kept;
#if KUI_TOY_WIRE_RING
        drops = drops_before;
#endif
    }
#if KUI_TOY_WIRE_CLEAN
    printf("  blocks handed on as clean %lu, rebuilt %lu, left to the sector check %lu: none of the first two kinds had a "
           "missing byte, none called clean a wrong one\n", kinds[1], kinds[2], kinds[0]);
    assert(kinds[1] && kinds[2] && kinds[0]);
#endif
#if KUI_TOY_WIRE_MSB
    printf("  bit order asked of the receiver: ignored in %lu runs, honoured in %lu, token only in %lu; each read right, "
           "but for %lu honoured ones given up before a whole block had settled the order "
           "(%lu blocks with lost bytes went to the sector check before a whole block had exposed the token-only answer)\n",
        orders[0], orders[1], orders[2], backs, unsettled);
    assert(orders[0] && orders[1] && orders[2]);
#endif
#if KUI_TOY_WIRE_RING
    /* A whole block's data is sixteen lines and a part of one or two more. */
    printf("  the ring read through the cache: %lu lines dropped and fetched afresh for %lu blocks\n", drops, total);
    assert(drops >= 16u * (total / 2u) && drops <= 18u * total + 18u * overruns);
#endif
    printf("  streams given up while they received: %lu, the card clocked on for %lu bytes in all, %lu at most\n",
        stops_receiving, stop_bytes, stop_most);
#if KUI_TOY_WIRE_APART
    printf("  block buffers: %lu blocks taken into the first, %lu into the second; none shared a place in the cache "
           "with its bytes in the ring\n", apart[0], apart[1]);
    /* The ring's 32 KiB pass the first buffer's places twice a lap: about one block in sixteen. */
    assert(KUI_TOY_WIRE_RING ? apart[1] > total / 32u && apart[1] < total / 8u : !apart[1]);
#endif
    /* Without QUIT the model shows the wait Test 76 removes. */
    assert(stops_receiving > 100u && (KUI_TOY_WIRE_QUIT ? stop_most <= 2u : stop_most > 1000u));
    printf("PASS Test %u card stream: %lu blocks checked against the card over %lu overruns; %lu streams restarted "
           "(clean block failing CRC16 %lu, too many lost bytes %lu); %lu blocks handed on from beside their place, "
           "each behind a byte lost between blocks and none after a whole CRC16: %lu with lost bytes listed, %lu with "
           "a byte of the CRC16 lost, %lu rebuilt from a CRC16 that fitted (%lu of them equal to the card in every byte "
           "not listed all the same); all left to the sector check\n",
        KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_SEQ ? 77u : KUI_TOY_WIRE_FLUID ? 76u : KUI_TOY_WIRE_STEADY ? 75u : KUI_TOY_WIRE_LEAN ? 74u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u, total, overruns, restarts, rewinds[2], rewinds[3],
        wrong[0] + wrong[1] + wrong[2], wrong[0], wrong[1], wrong[2], wrong_same);
    return 0;
}
