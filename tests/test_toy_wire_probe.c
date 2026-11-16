/* SPDX-License-Identifier: GPL-3.0-only
 * Host model for the Toy Commander wire probe (Test 67). It runs the real
 * probe source against a register-level model of the SH-4 SCI receiver, DMAC
 * channel 1, one TMU channel, the SDK's callback table and an SD card in an
 * open CMD18. The model encodes the receive behaviour sci_stream.c documents
 * from the console: a full RDR at the next byte's end overruns, loses that
 * byte and stops the clock; module standby resets the SCI except SPTR.
 * This is a model, not a Dreamcast: it checks the probe's logic and
 * accounting, never bus timing. */
#include "toy_wire_probe.h"
#include "sci_sd_bus.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEG KUI_WIRE_SEGMENT_BYTES
#define FAKE_PHYSICAL UINT32_C(0x0cfd5b00)
#define GAP 3u /* card bytes between a block's CRC and the next token */
#define BLOCK (1u + 512u + 2u + GAP)

uint8_t *kui_wire_test_buffer(void);

static struct {
    uint8_t smr, brr, scr, ssr, rdr, scmr, sptr, stbcr, tstr;
    uint16_t pdtr, ipra, iprb, frqcr, tcr[3];
    uint32_t tcor[3], tcnt[3];
    uint32_t sar, dar, count, chcr, dmaor;
    uint32_t slots[64]; /* 0x8c00f600.. */
    uint32_t vbr, spc;
} hw;

static struct {
    bool acquired, fault, init_fail, selected_by_bus;
    uint64_t next;        /* next card stream byte the clock will shift out */
    uint64_t pio_taken;   /* stream bytes consumed by programmed transfers */
    unsigned phase;       /* 0 idle, 1 command bytes, 2 response, 3 streaming */
    unsigned command_left, response_wait, stops, starts, busy_left;
    uint8_t command[6];
    bool stopping;
} card;

static uint8_t rev8(uint8_t x) {
    x = (uint8_t)(x >> 4 | x << 4);
    x = (uint8_t)((x & 0xccu) >> 2 | (x & 0x33u) << 2);
    return (uint8_t)((x & 0xaau) >> 1 | (x & 0x55u) << 1);
}
static uint16_t crc16(uint16_t crc, uint8_t data) {
    uint32_t x = (crc >> 8) ^ data;
    x ^= x >> 4;
    return (uint16_t)((crc << 8) ^ (x << 12) ^ (x << 5) ^ x);
}
/* Deterministic block data with plenty of 0xfe and 0xff bytes in it. */
static uint8_t data_byte(uint64_t block, unsigned i) {
    uint32_t x = (uint32_t)(block * 2654435761u) ^ (i * 40503u);
    x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
    if((x & 0x3fu) == 7u) return 0xfe;
    if((x & 0x3fu) == 9u) return 0xff;
    return (uint8_t)(x >> 8);
}
/* Logical (MSB-first) stream byte n: two idle bytes, then framed blocks. */
static uint8_t stream_byte(uint64_t n) {
    if(n < 2u) return 0xff;
    n -= 2u;
    const uint64_t block = n / BLOCK;
    const unsigned at = (unsigned)(n % BLOCK);
    if(!at) return 0xfe;
    if(at <= 512u) return data_byte(block, at - 1u);
    if(at <= 514u) {
        uint16_t crc = 0;
        for(unsigned i = 0; i < 512u; ++i) crc = crc16(crc, data_byte(block, i));
        return at == 513u ? (uint8_t)(crc >> 8) : (uint8_t)crc;
    }
    return 0xff;
}

/* ---- the probe's hardware hooks ---- */
static unsigned reads_of_clock;
static bool free_run; /* status polls let the wire advance, as a busy-wait does */
static bool receiving(void);
static bool shift(bool stalled);
uint32_t kui_wire_test_read(uint32_t address, unsigned width) {
    (void)width;
    switch(address) {
    case 0xffe00000u: return hw.smr;
    case 0xffe00004u: return hw.brr;
    case 0xffe00008u: return hw.scr;
    case 0xffe00010u:
        if(free_run && receiving()) (void)shift(false);
        return hw.ssr;
    case 0xffe00014u: return hw.rdr;
    case 0xffe00018u: return hw.scmr;
    case 0xffe0001cu: return hw.sptr;
    case 0xffc00004u: return hw.stbcr;
    case 0xffc00000u: return hw.frqcr;
    case 0xff800030u: return hw.pdtr;
    case 0xffa00010u: return hw.sar;
    case 0xffa00014u: return hw.dar;
    case 0xffa00018u: return hw.count;
    case 0xffa0001cu: return hw.chcr;
    case 0xffa00040u: return hw.dmaor;
    case 0xffd00004u: return hw.ipra;
    case 0xffd00008u: return hw.iprb;
    case 0xffd80004u: return hw.tstr;
    case 0xffd8000cu: ++reads_of_clock; return hw.tcnt[0] -= 3u; /* time passes */
    default: break;
    }
    if(address >= 0xffd80008u && address < 0xffd8002cu) {
        unsigned k = (address - 0xffd80008u) / 12u, field = (address - 0xffd80008u) % 12u;
        return field == 0u ? hw.tcor[k] : field == 4u ? hw.tcnt[k] : hw.tcr[k];
    }
    if(address >= 0x8c00f600u && address < 0x8c00f700u) return hw.slots[(address - 0x8c00f600u) / 4u];
    fprintf(stderr, "unexpected read %08x\n", (unsigned)address);
    abort();
}
/* The console finding of 2026-10-02: after a receive-only run ends in an
 * overrun the receiver is not usable again until a module reset. */
static bool stale;
static void module_reset(void) {
    hw.scr = 0; hw.smr = 0; hw.brr = 0xff; hw.scmr = 0; hw.ssr = 0x84; hw.rdr = 0;
    stale = false;
}
void kui_wire_test_write(uint32_t address, uint32_t value, unsigned width) {
    (void)width;
    switch(address) {
    case 0xffe00000u: hw.smr = (uint8_t)value; return;
    case 0xffe00004u: hw.brr = (uint8_t)value; return;
    case 0xffe00008u: hw.scr = (uint8_t)value; return;
    /* Flags clear by writing zero; TEND is read-only. */
    case 0xffe00010u: hw.ssr = (uint8_t)((hw.ssr & (value | ~0xf8u)) | (hw.ssr & 0x04u)); return;
    case 0xffe00018u: hw.scmr = (uint8_t)value; return;
    case 0xffe0001cu: hw.sptr = (uint8_t)value; return;
    case 0xffc00004u:
        assert(!((hw.stbcr ^ value) & ~1u)); /* only the SCI's standby bit */
        if(!(hw.stbcr & 1u) && (value & 1u)) module_reset();
        hw.stbcr = (uint8_t)value;
        return;
    case 0xff800030u:
        assert(!((hw.pdtr ^ value) & ~0x80u)); /* only the card's chip select */
        hw.pdtr = (uint16_t)value;
        return;
    case 0xffa00010u: hw.sar = value; return;
    case 0xffa00014u: hw.dar = value; return;
    case 0xffa00018u: hw.count = value; return;
    case 0xffa0001cu: hw.chcr = value; return;
    case 0xffd00004u: hw.ipra = (uint16_t)value; return;
    case 0xffd00008u: hw.iprb = (uint16_t)value; return;
    case 0xffd80004u: hw.tstr = (uint8_t)value; return;
    default: break;
    }
    if(address >= 0xffd80014u && address < 0xffd8002cu) { /* TMU1 and TMU2 only */
        unsigned k = (address - 0xffd80008u) / 12u, field = (address - 0xffd80008u) % 12u;
        if(field == 0u) hw.tcor[k] = value;
        else if(field == 4u) hw.tcnt[k] = value;
        else hw.tcr[k] = (uint16_t)value;
        return;
    }
    if(address >= 0x8c00f600u && address < 0x8c00f700u) {
        hw.slots[(address - 0x8c00f600u) / 4u] = value;
        return;
    }
    fprintf(stderr, "unexpected write %08x\n", (unsigned)address);
    abort();
}
void kui_wire_test_settle(unsigned count) { hw.tcnt[0] -= count / 64u; }
void kui_wire_test_fence(void) {}
uint32_t kui_wire_test_physical(const void *area) {
    assert(area == kui_wire_test_buffer());
    return FAKE_PHYSICAL;
}
uint32_t kui_wire_test_spc(void) { return hw.spc; }
uint32_t kui_wire_test_vbr(void) { return hw.vbr; }

/* ---- the shared SCI bus and card reader the probe calls ---- */
enum kui_loader_sd_result kui_sci_sd_acquire(void) {
    if(card.acquired) return KUI_LOADER_SD_NOT_READY;
    card.acquired = true; card.fault = false;
    hw.scr = 0; hw.smr = 0x80; hw.brr = 31; hw.scmr = 0;
    return KUI_LOADER_SD_OK;
}
void kui_sci_sd_release(void) {
    if(!card.acquired) return;
    assert(!(hw.chcr & 1u) || hw.sar != 0x1fe00014u); /* never release under a live receive */
    hw.scr = 0; hw.pdtr |= 0x80u;
    card.acquired = false;
}
bool kui_sci_sd_healthy(void) { return card.acquired && !card.fault; }
bool kui_sci_sd_faulted(void) { return card.fault; }
static void bus_begin(void *ctx) { (void)ctx; }
static void bus_end(void *ctx) { (void)ctx; kui_sci_sd_release(); }
static void bus_select(void *ctx, bool selected) {
    (void)ctx;
    assert(card.acquired);
    hw.pdtr = selected ? hw.pdtr & (uint16_t)~0x80u : hw.pdtr | 0x80u;
}
/* One programmed full-duplex byte: clocks exactly one byte, cannot overrun. */
static uint8_t bus_transfer(void *ctx, uint8_t data, bool slow) {
    (void)ctx;
    assert(card.acquired && !slow);
    assert(hw.scr == 0x30u || hw.scr == 0u); /* never beside a DMA reception */
    assert(!stale);
    hw.scr = 0x30; hw.brr = 0;
    if(hw.pdtr & 0x80u) return 0xff; /* deselected */
    if(card.phase == 1u) {
        card.command[6u - card.command_left] = data;
        if(card.phase == 1u && !--card.command_left) {
            if(card.command[0] == 0x52u) {card.phase = 2u; card.response_wait = 2u; ++card.starts;}
            else if(card.command[0] == 0x4cu) {card.phase = 2u; card.response_wait = 3u; card.stopping = true; ++card.stops;}
            else abort();
        }
        return 0xff;
    }
    if((data & 0xc0u) == 0x40u && card.phase != 2u) {
        card.phase = 1u; card.command_left = 5u; card.command[0] = data;
        return 0xff;
    }
    assert(data == 0xffu);
    if(card.phase == 2u) {
        /* CMD12's stuff byte is arbitrary; idle bytes precede R1. */
        if(card.response_wait--) return card.stopping && card.response_wait == 2u ? 0x7f : 0xff;
        if(card.stopping) {card.phase = 0u; card.stopping = false; card.busy_left = 4u; return 0x00;}
        card.phase = 3u;
        return 0x00;
    }
    if(card.phase == 3u) {++card.pio_taken; return stream_byte(card.next++);}
    if(card.busy_left) {--card.busy_left; return 0x00;}
    return 0xff;
}
static uint32_t bus_ticks(void *ctx) { (void)ctx; return 0; }
static const struct kui_loader_sd_bus bus = {NULL, bus_begin, bus_end, bus_select, bus_transfer, bus_ticks, NULL};
const struct kui_loader_sd_bus *kui_sci_sd_bus(void) { return &bus; }
static uint64_t card_blocks = UINT64_C(0x1000000);
enum kui_loader_sd_result kui_loader_sd_init_bus(struct kui_loader_sd *out, const struct kui_loader_sd_bus *b) {
    assert(card.acquired && b == &bus);
    out->bus = *b;
    if(card.init_fail) {bus_end(NULL); return KUI_LOADER_SD_TIMEOUT;}
    out->blocks = card_blocks; out->high_capacity = true; out->ready = true; out->slow = false;
    hw.scr = 0x30; hw.smr = 0x80; hw.brr = 0; hw.ssr = 0x84; hw.sptr = 0x05;
    card.phase = 0u; card.next = 0; card.pio_taken = 0;
    return KUI_LOADER_SD_OK;
}

/* ---- the receiver and channel ---- */
static uint8_t expect[SEG];
static bool hole[SEG];
static uint64_t clocked; /* bytes the receive-only clock shifted out of the card */
static bool receiving(void) {
    return hw.scr == 0x50u && !(hw.ssr & 0x20u) && !(hw.pdtr & 0x80u) && !(hw.stbcr & 1u);
}
static bool channel_takes(void) {
    return (hw.chcr & 3u) == 1u && (hw.dmaor & 7u) == 1u && hw.sar == 0x1fe00014u;
}
static void dma_take(void) {
    assert(hw.chcr == 0x4911u && hw.count);
    const uint32_t at = hw.dar - FAKE_PHYSICAL;
    assert(at < SEG);
    kui_wire_test_buffer()[at] = hw.rdr;
    expect[at] = hw.rdr; hole[at] = false;
    ++hw.dar; --hw.count;
    hw.ssr &= (uint8_t)~0x40u;
    if(!hw.count) hw.chcr |= 2u;
}
/* Clock one byte out of the card. stalled: the bus is held, the channel
 * cannot take the byte now. Returns false once the receiver has overrun. */
static bool shift(bool stalled) {
    assert(receiving() && !stale);
    assert(hw.sptr == 0x83u && hw.smr == 0x80u && hw.brr == 0u);
    const uint8_t wire_byte = rev8(stream_byte(card.next++));
    ++clocked;
    if(!(clocked & 1u)) --hw.tcnt[0];
    if(hw.ssr & 0x40u) {hw.ssr |= 0x20u; stale = true; return false;} /* overrun: this byte is lost */
    hw.rdr = wire_byte; hw.ssr |= 0x40u;
    if(!stalled && channel_takes()) dma_take();
    return true;
}
static void interrupt(uint32_t event) {
    assert((hw.iprb & 0xf0u) == 0x10u);
    kui_wire_isr(event);
}
/* Run undisturbed for up to n bytes, serving every segment end. */
static void flow(uint32_t n) {
    while(n-- && receiving())
        if(!shift(false)) interrupt(0x4e0);
}
/* A bus stall: the next byte stays in RDR and the one after it overruns.
 * landed: the bus came back and the channel took the RDR byte before the
 * handler looked. Returns the lost wire position; SEG is the byte just
 * behind the segment. */
static uint32_t cut(bool landed) {
    assert(receiving() && hw.count);
    const uint32_t at = hw.dar - FAKE_PHYSICAL;
    assert(shift(true));
    assert(!shift(true));
    if(landed) dma_take();
    else {expect[at] = hw.rdr; hole[at] = false;}
    if(at + 1u < SEG) hole[at + 1u] = true;
    return at + 1u;
}

static void reset_world(void) {
    memset(&hw, 0, sizeof(hw));
    memset(&card, 0, sizeof(card));
    memset(expect, 0, sizeof(expect));
    memset(hole, 0, sizeof(hole));
    clocked = 0; free_run = false; stale = false;
    hw.ssr = 0x84; hw.stbcr = 0x02; hw.pdtr = 0xe3e3; hw.frqcr = 0x0e0a;
    hw.tstr = 1u; hw.tcor[0] = 0xffffffffu; hw.tcnt[0] = 0x80000000u; hw.tcr[0] = 0x0002u;
    hw.dmaor = 0x8201; hw.sar = 0x11111111u; hw.dar = 0x22222222u; hw.count = 0x333u; hw.chcr = 0x12c0u;
    hw.ipra = 0x2000; hw.iprb = 0x0400; hw.sptr = 0x05;
    hw.vbr = 0x8c00f400u;
    for(unsigned i = 0; i < 64u; ++i) hw.slots[i] = 0x8c0be408u;
    hw.spc = 0x8c04c252u;
}
/* Everything borrowed is back exactly as the game had it. */
static void assert_restored(void) {
    assert(hw.sar == 0x11111111u && hw.dar == 0x22222222u && hw.count == 0x333u && hw.chcr == 0x12c0u);
    assert(hw.ipra == 0x2000u && hw.iprb == 0x0400u);
    for(unsigned i = 0; i < 64u; ++i) assert(hw.slots[i] == 0x8c0be408u);
    assert(hw.sptr == 0x05u && hw.scr == 0u && (hw.pdtr & 0x80u) && hw.stbcr == 0x02u);
    assert(!(hw.tstr & 6u) && !(hw.tcr[1] & 0x20u) && !(hw.tcr[2] & 0x20u));
    assert(!card.acquired && hw.dmaor == 0x8201u);
}
/* The player has exited: the game thread's own hook closes the stream,
 * busy-waiting while the wire runs on to its next overrun. */
static void end_movie(unsigned movie) {
    free_run = true;
    kui_wire_movie_end(movie);
    free_run = false;
}
static uint32_t cuts, pauses;
/* One stall, its error interrupt and, after a short run, the timed wait. */
static uint32_t stall(bool landed) {
    const struct kui_wire_summary *s = kui_wire_summary();
    const uint32_t lost = cut(landed);
    interrupt(0x4e0);
    ++cuts;
    if(s->state == KUI_WIRE_PAUSED) {
        ++pauses;
        assert(!receiving() && hw.scr == 0x30u && (hw.pdtr & 0x80u));
        assert((hw.tstr & 4u) && (hw.tcr[2] & 0x20u) && hw.tcor[2] == 0xffffffffu &&
               hw.tcnt[2] >= KUI_WIRE_BACKOFF_MIN && hw.tcnt[2] <= KUI_WIRE_BACKOFF_MAX);
        kui_wire_poll(); /* a hook visit leaves the timed wait alone */
        assert(s->state == KUI_WIRE_PAUSED && !receiving());
        hw.tcnt[0] -= hw.tcnt[2];
        hw.tcr[2] |= 0x100u; /* underflow */
        assert((hw.ipra & 0xf0u) == 0x10u);
        kui_wire_isr(0x440);
        assert(!(hw.tstr & 4u) && !(hw.tcr[2] & 0x120u));
    }
    assert(s->state == KUI_WIRE_RUN && receiving());
    assert(hw.dar - FAKE_PHYSICAL == (lost + 1u >= SEG ? 0u : lost + 1u));
    return lost;
}

/* The probe is a module with static state: each scenario is its own process. */
static void scenario_clean(void) {
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    assert(s->state == KUI_WIRE_RUN && !s->error && s->mode == 2u);
    assert(card.starts == 1u && card.pio_taken == 3u); /* two idle bytes and the token */
    assert(hw.slots[(0x65cu - 0x600u) / 4u] == 0x8cfd1234u && hw.slots[(0x660u - 0x600u) / 4u] == 0x8cfd1234u);
    assert(hw.slots[(0x648u - 0x600u) / 4u] == 0x8cfd1234u && (hw.ipra & 0x00f0u) == 0x0010u);
    assert(hw.iprb == 0x0410u);
    flow(100u);
    kui_wire_gd(true); /* a disc read in flight before the first frame counts from the frame */
    flow(50u);
    kui_wire_frame(0);
    flow(950u);
    kui_wire_gd(false);
    flow(2u * SEG);
    kui_wire_gd(true);
    flow(4000u);
    kui_wire_gd(true); /* the next read accepted before the last one's status was seen */
    flow(1000u);
    kui_wire_gd(false);
    flow(SEG + 5000u - 950u - 5000u - 300u);
    kui_wire_gd(true); /* and one still in flight when the player exits */
    flow(300u);
    assert(s->state == KUI_WIRE_RUN && s->segments == 3u);
    end_movie(0);
    kui_wire_gd(false);
    assert(s->window[0].gd_bytes == 950u + 5000u + 300u && !s->window[0].gd_cuts);
    assert(s->window[0].gd_ticks >= (950u + 5000u + 300u) / 2u && s->window[0].gd_ticks < 3300u);
    assert(s->state == KUI_WIRE_RUN && card.stops == 0u); /* the bumper's end leaves the stream running */
    assert(s->window[0].closed && !s->window[0].total.cuts && !s->window[0].total.pauses);
    assert(s->window[0].total.bytes == clocked - 150u);
    assert(s->window[0].total.irqs == 3u && s->window[0].total.isr_ticks && s->window[0].span);
    kui_wire_movie_start(1);
    assert(card.starts == 1u && !s->restarts);
    flow(700u);
    kui_wire_frame(1);
    const uint64_t began = clocked;
    flow(9000u);
    end_movie(1);
    assert(s->state == KUI_WIRE_DONE && !s->error && card.stops == 1u);
    assert(s->window[1].closed && s->window[1].total.bytes == 9000u && clocked > began + 9000u);
    assert(!s->window[1].gd_bytes && !s->window[1].gd_ticks);
    kui_wire_shutdown();
    assert_restored();
    assert(s->tail_bytes == SEG && !s->tail_bad && !s->tail_holed);
    assert(s->tail_ok >= SEG / BLOCK - 1u && s->tail_ok <= SEG / BLOCK);
    /* A second pass over either movie must not start anything. */
    kui_wire_movie_start(0); kui_wire_movie_start(1);
    kui_wire_frame(0); end_movie(0); end_movie(1);
    assert(card.starts == 1u && card.stops == 1u && s->state == KUI_WIRE_DONE);
    assert_restored();
}

static void scenario_cuts(void) {
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    kui_wire_frame(0);
    uint32_t seed = 12345u;
    for(unsigned round = 0; round < 1500u; ++round) {
        seed = seed * 1664525u + 1013904223u;
        /* Mostly long runs, with spells of short ones. */
        flow((seed >> 9) % ((round / 50u) & 1u ? 90u : 2500u));
        assert(s->state == KUI_WIRE_RUN);
        if(hw.count <= 2u) continue; /* let this segment end by itself */
        hw.spc = 0x8c04c200u + ((seed >> 3) % 5u) * 0x40u + (seed & 0x3eu);
        const uint32_t stored = hw.dar - FAKE_PHYSICAL;
        assert(stall(seed & 0x100000u) == stored + 1u);
    }
    assert(cuts > 1000u && pauses > 100u && pauses < cuts);
    /* A last segment with exactly two cuts in it, far apart. */
    for(const uint32_t segments = s->segments; s->segments == segments;) flow(64u);
    flow(1000u);
    const uint32_t first = stall(true);
    flow(3000u);
    const uint32_t second = stall(false);
    flow(2000u);
    assert(first > 1000u && second > first + 3000u);
    end_movie(0);
    const struct kui_wire_window *v = &s->window[0];
    assert(v->total.cuts == cuts && v->total.pauses == pauses && v->total.paused_ticks && v->pause_max);
    assert(v->total.bytes == clocked);
    uint32_t histogram = 0;
    for(unsigned i = 0; i < KUI_WIRE_RUN_BUCKETS; ++i) histogram += v->total.run[i];
    assert(histogram == cuts && v->total.run[0] && v->total.run[4]);
    assert(v->total.irqs == cuts + pauses + s->segments);
    end_movie(1);
    assert(s->state == KUI_WIRE_DONE && !s->error && card.stops == 1u);
    kui_wire_shutdown();
    /* What stands in the last segment is exactly what the wire carried,
     * position for position, except the two logged lost bytes. */
    const uint8_t *buffer = kui_wire_test_buffer();
    assert(s->tail_bytes == SEG && hole[first] && hole[second]);
    for(uint32_t i = 0; i < SEG; ++i)
        if(i != first && i != second) assert(!hole[i] && buffer[i] == expect[i]);
    assert(!s->tail_bad && s->tail_holed >= 1u && s->tail_holed <= 2u);
    assert(s->tail_ok + s->tail_holed >= SEG / BLOCK - 1u);
    /* The sample scales back to about the cuts it stands for. */
    const uint32_t sampled = v->top_count[0] + v->top_count[1];
    assert(v->pc_count > KUI_WIRE_PC_SAMPLES / 2u && v->pc_count <= KUI_WIRE_PC_SAMPLES && v->pc_mask >= 7u);
    assert(sampled > cuts / 4u && sampled <= cuts + v->pc_mask + 1u);
    for(unsigned i = 0; i < 2u; ++i)
        assert(v->top_pc[i] >= 0x8c04c200u && v->top_pc[i] <= 0x8c04c300u && !(v->top_pc[i] & 63u));
    assert(v->top_count[0] >= v->top_count[1] && v->top_count[1] && v->top_pc[0] != v->top_pc[1]);
    assert(!v->gd_cuts && !v->gd_bytes);
    assert_restored();
}

static void scenario_edges(void) {
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    kui_wire_frame(0);
    /* Stalls on each of the last places of a segment, taken late or not. */
    for(unsigned variant = 0; variant < 8u; ++variant) {
        const uint32_t segments = s->segments, left = 1u + (variant >> 1);
        flow(300u);
        while(hw.count > left) assert(shift(false));
        const uint32_t lost = stall(variant & 1u);
        assert(lost == SEG - left + 1u);
        assert(s->segments == segments + (lost + 1u >= SEG ? 1u : 0u));
        if(lost + 1u < SEG) {flow(8u); assert(s->segments == segments + 1u);}
        assert(s->state == KUI_WIRE_RUN && !s->error);
    }
    assert(s->window[0].total.bytes == 0u); /* still open */
    /* Short runs one after another wait twice as long each time, up to the
     * limit; one long run returns to the shortest wait. */
    flow(500u);
    (void)cut(true);
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_RUN);
    for(unsigned round = 0; round < 2u; ++round) {
        for(uint32_t want = KUI_WIRE_BACKOFF_MIN; want <= 4u * KUI_WIRE_BACKOFF_MAX; want <<= 1) {
            flow(20u);
            (void)cut(want & 0x80u);
            interrupt(0x4e0);
            assert(s->state == KUI_WIRE_PAUSED);
            assert(hw.tcnt[2] == (want < KUI_WIRE_BACKOFF_MAX ? want : KUI_WIRE_BACKOFF_MAX));
            kui_wire_isr(0x440);
            assert(s->state == KUI_WIRE_RUN);
        }
        flow(KUI_WIRE_GOOD_RUN);
        (void)cut(false);
        interrupt(0x4e0);
        assert(s->state == KUI_WIRE_RUN && receiving()); /* exactly long enough */
        flow(KUI_WIRE_GOOD_RUN - 1u);
        (void)cut(false);
        interrupt(0x4e0);
        assert(s->state == KUI_WIRE_PAUSED && hw.tcnt[2] == KUI_WIRE_BACKOFF_MIN); /* one byte short */
        kui_wire_isr(0x440);
        flow(2000u);
        (void)cut(false);
        interrupt(0x4e0);
        assert(s->state == KUI_WIRE_RUN);
    }
    /* An overrun served late: the wire stood still that long. And a cut
     * while the disc drive transfers is counted apart. */
    const uint32_t late = s->window[0].total.late_ticks; /* still zero: open */
    (void)late;
    flow(1000u);
    kui_wire_gd(true);
    flow(600u);
    (void)cut(true);
    hw.tcnt[0] -= 5000u;
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_RUN);
    flow(400u);
    kui_wire_gd(false);
    flow(700u);
    (void)cut(false);
    interrupt(0x4e0);
    /* A stale timer event while running changes nothing. */
    const uint32_t dar = hw.dar;
    kui_wire_isr(0x440);
    assert(s->state == KUI_WIRE_RUN && hw.dar == dar && receiving());
    /* The receive interrupt is never ours to serve. */
    kui_wire_isr(0x500);
    assert(s->spurious == 1u && receiving());
    /* The SDK rewrites the priority register: the next hook visit repairs it. */
    hw.iprb = 0x0400;
    kui_wire_poll();
    assert(hw.iprb == 0x0410u);
    end_movie(0);
    assert(s->window[0].total.bytes == clocked);
    assert(s->window[0].total.late_ticks >= 5000u && s->window[0].total.late_ticks < 5600u);
    assert(s->window[0].gd_cuts == 1u && s->window[0].gd_bytes == 1002u && s->window[0].gd_ticks >= 5500u);
    end_movie(1);
    kui_wire_shutdown();
    assert(s->state == KUI_WIRE_DONE && !s->error && !s->tail_bad);
    assert_restored();
}

static void scenario_polls(void) {
    /* Both spare timer channels belong to the game: hook visits resume. */
    reset_world();
    hw.tstr |= 4u; hw.ipra |= 0x0300u;
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    assert(s->state == KUI_WIRE_RUN && s->mode == 0u);
    assert(hw.slots[(0x648u - 0x600u) / 4u] == 0x8c0be408u && hw.slots[(0x644u - 0x600u) / 4u] == 0x8c0be408u);
    kui_wire_frame(0);
    flow(20u);
    (void)cut(true);
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_PAUSED && hw.tstr == 5u && hw.ipra == 0x2300u);
    hw.tcnt[0] -= 500u;
    kui_wire_poll();
    assert(s->state == KUI_WIRE_RUN && receiving());
    flow(3000u);
    (void)cut(false);
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_RUN); /* a long run resumes at once */
    flow(10u);
    (void)cut(false);
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_PAUSED); /* and stays so to the end */
    end_movie(0);
    end_movie(1);
    assert(s->state == KUI_WIRE_DONE && card.stops == 1u && s->window[0].total.pauses == 2u);
    assert(s->window[0].total.bytes == clocked && s->window[0].total.paused_ticks >= 500u);
    kui_wire_shutdown();
    assert(!s->error && !s->tail_bad && hw.tstr == 5u && hw.ipra == 0x2300u);
    hw.tstr &= (uint8_t)~4u; hw.ipra &= (uint16_t)~0x0300u;
    assert_restored();
}

static void scenario_refusals(unsigned which) {
    reset_world();
    const struct kui_wire_summary *s = kui_wire_summary();
    uint32_t want = 0;
    switch(which) {
    case 0u: hw.tcr[0] = 0x0001u; want = KUI_WIRE_E_CLOCK; break;
    case 1u: card.acquired = true; want = KUI_WIRE_E_ACQUIRE; break;
    case 2u: card.init_fail = true; want = KUI_WIRE_E_INIT; break;
    case 3u: card_blocks = KUI_WIRE_FIRST_BLOCK + KUI_WIRE_SPAN_BLOCKS - 1u; want = KUI_WIRE_E_CARD; break;
    case 4u: hw.vbr = 0x8c010000u; want = KUI_WIRE_E_VECTOR; break;
    case 5u: hw.slots[(0x660u - 0x600u) / 4u] = 0x8c012345u; want = KUI_WIRE_E_VECTOR; break;
    case 6u: hw.chcr |= 1u; want = KUI_WIRE_E_CHANNEL; break;
    default: hw.dmaor = 0x8200u; want = KUI_WIRE_E_CHANNEL; break;
    }
    const uint32_t slot = hw.slots[(0x660u - 0x600u) / 4u], chcr = hw.chcr;
    kui_wire_movie_start(0);
    assert(s->state == KUI_WIRE_OFF && s->error == want);
    kui_wire_frame(0);
    kui_wire_poll();
    end_movie(0);
    end_movie(1);
    kui_wire_shutdown();
    assert(s->state == KUI_WIRE_OFF && !s->window[0].total.bytes && !s->tail_bytes && !clocked);
    /* A stream that reached CMD18 was stopped again; nothing stays borrowed. */
    assert(card.stops == (which >= 4u ? 1u : 0u));
    assert(hw.slots[(0x660u - 0x600u) / 4u] == slot && hw.slots[(0x65cu - 0x600u) / 4u] == 0x8c0be408u);
    assert(hw.slots[(0x648u - 0x600u) / 4u] == 0x8c0be408u);
    assert(hw.chcr == chcr && hw.ipra == 0x2000u && hw.iprb == 0x0400u && hw.sar == 0x11111111u);
    assert(!(hw.tstr & 6u) && hw.sptr == 0x05u);
    if(which == 1u) card.acquired = false;
    assert(!card.acquired);
}

static void scenario_fault(void) {
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    kui_wire_frame(0);
    flow(5000u);
    /* The game takes channel 1 for itself during a stall. */
    (void)cut(true);
    hw.sar = 0x0c123400u; hw.dar = 0x0c200000u; hw.count = 64u; hw.chcr = 0x12c1u;
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_FAULT && s->error == KUI_WIRE_E_FOREIGN && s->detail == 0x0c200000u);
    assert(hw.chcr == 0x12c1u && hw.sar == 0x0c123400u); /* its transfer is left alone */
    assert(hw.scr == 0u && !receiving() && (hw.pdtr & 0x80u));
    end_movie(0);
    /* The intro tries once more from a clean card. */
    hw.sar = 0x11111111u; hw.dar = 0x22222222u; hw.count = 0x333u; hw.chcr = 0x12c0u;
    kui_wire_movie_start(1);
    assert(s->state == KUI_WIRE_RUN && s->restarts == 1u && card.starts == 2u && card.stops == 1u);
    kui_wire_frame(1);
    flow(40000u);
    end_movie(1);
    kui_wire_shutdown();
    assert(s->state == KUI_WIRE_DONE && s->error == KUI_WIRE_E_FOREIGN && card.stops == 2u);
    assert(s->window[1].total.bytes == 40000u && !s->tail_bad && s->tail_ok);
    assert_restored();
}

static void scenario_moved(void) {
    /* Channel 1 still points at the receiver but its address and count no
     * longer agree: someone else wrote it. It must not be stopped for them. */
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    flow(5000u);
    (void)cut(true);
    hw.dar -= 64u;
    interrupt(0x4e0);
    assert(s->state == KUI_WIRE_FAULT && s->error == KUI_WIRE_E_FOREIGN);
    assert(hw.chcr == 0x4910u && hw.count == SEG - 5001u);
    hw.chcr = 0x12c0u; hw.sar = 0x11111111u; hw.dar = 0x22222222u; hw.count = 0x333u;
    kui_wire_shutdown();
    assert(s->state == KUI_WIRE_FAULT && card.stops == 1u && !s->tail_bytes);
    assert_restored();
}

static void scenario_damage(void) {
    /* A byte that changed without a logged cut must show as a bad block. */
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    flow(SEG / 2u);
    kui_wire_test_buffer()[BLOCK * 6u + 100u] ^= 0x10u;
    end_movie(1);
    kui_wire_shutdown();
    assert(s->state == KUI_WIRE_DONE && s->tail_bad == 1u && s->tail_ok >= SEG / BLOCK - 3u);
    assert_restored();
}

static void scenario_abandoned(void) {
    /* The intro never ends before the report is asked for. */
    reset_world();
    kui_wire_movie_start(0);
    const struct kui_wire_summary *s = kui_wire_summary();
    kui_wire_frame(0);
    flow(70000u);
    free_run = true;
    kui_wire_shutdown();
    free_run = false;
    assert(s->state == KUI_WIRE_DONE && !s->error && card.stops == 1u);
    assert(s->window[0].closed && s->window[0].total.bytes == 70000u && s->tail_ok && !s->tail_bad);
    assert_restored();
}

int main(int argc, char **argv) {
    if(argc != 2) return 2;
    const unsigned which = (unsigned)atoi(argv[1]);
    switch(which) {
    case 0u: scenario_clean(); break;
    case 1u: scenario_cuts(); break;
    case 2u: scenario_edges(); break;
    case 3u: scenario_polls(); break;
    case 4u: scenario_fault(); break;
    case 5u: scenario_damage(); break;
    case 6u: scenario_abandoned(); break;
    case 7u: scenario_moved(); break;
    default:
        if(which < 8u || which > 15u) return 2;
        scenario_refusals(which - 8u);
        break;
    }
    printf("PASS Toy wire probe model scenario %u\n", which);
    return 0;
}
