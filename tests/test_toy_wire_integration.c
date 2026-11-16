/* SPDX-License-Identifier: GPL-3.0-only
 * Toy Commander Test 68 end to end on the host: the real GD service, low
 * reader, SDK SCI callback routing, auxiliary and continuous card stream
 * against the shared card and register model. The game's side is what a
 * console run has: GD requests, the recognised EXEC polls, SCI error
 * interrupts through the resident's own dispatch, and the stream's timer
 * through the SDK table. Every request must complete with exactly the
 * image's bytes at the game's destination.
 *
 * The model delivers a DMA run whole at the first register read after the
 * test lets time pass; it checks the reader's logic and ownership, never bus
 * timing. toy_wire_stream.c's partial arrivals are covered by its own test. */
#include "kui/hash.h"
#include "kui/toy_wire_report.h"
#include "toy_wire_stream.h"

#include "kui/native_sdk_sci_runtime.h"
/* The shared fixture derives the title's vector base, and with it the
 * place of the title's SDK table, from this owner. Test 81 gives another
 * title a base of its own. The production sources are compiled apart,
 * with the real header. */
#define WIRE_TOY_VBR UINT32_C(0x8c00f400)
_Static_assert(WIRE_TOY_VBR == KUI_NATIVE_SDK_SCI_OWNER_VBR, "Toy Commander's vector base");
static uint32_t wire_vbr = WIRE_TOY_VBR;
#undef KUI_NATIVE_SDK_SCI_OWNER_VBR
#define KUI_NATIVE_SDK_SCI_OWNER_VBR wire_vbr
#define KUI_TOY_SDK_LIVENESS_MAIN toy_retained_sdk_tests
#include "test_toy_sdk_liveness.c"
/* A command of the game's that gives a window up, where a scenario only
 * needs one: since Test 80 a command that only asks does not. */
#define GONE_BY_COMMAND (KUI_TOY_WIRE_QUERY ? (uint32_t)KUI_GD_STOP : (uint32_t)KUI_GD_GETTOC2)
/* And a number no command has, beyond those the report can name by a bit:
 * whatever stands in the service's place for a command gives the window
 * up unless it is one that only asks. */
#define GONE_BY_UNKNOWN (KUI_TOY_WIRE_QUERY ? UINT32_C(95) : (uint32_t)KUI_GD_GETTOC2)

#if !KUI_TOY_WIRE_STREAM
#error "This fixture is the Test 68 reader's"
#endif

#define RING_PHYSICAL 0x0c300000u
enum { RING_BYTES = 32768u, FIRST = MOVIE_LBA, REQUEST = 13u, WINDOW_END = 45291u };
void kui_toy_wire_timer_callback(uint32_t event);
void kui_toy_movie_present_boundary(uint32_t ctx, uintptr_t witness, unsigned phase);
bool kui_toy_movie_refill_defer(uint32_t avi, uint32_t file, uint32_t bytes);
#define PLAYER UINT32_C(0x8c00dc90)
static unsigned long gate_visits;
#if KUI_TOY_WIRE_IDLE
void kui_toy_movie_ahead_update(uint32_t ctx, uint32_t entered);
/* The player's memory as Test 73's hook reads it: its output and skip
 * counters, its lag counter, and its buffer behind three pointers. */
static struct { uint32_t out, skip, lag, capacity, released, filled, stream, frame; } player;
#define PLAYER_STREAM UINT32_C(0x8c200000)
#define PLAYER_CHANNEL UINT32_C(0x8c200100)
#define PLAYER_AVI UINT32_C(0x8c200200)
#define PLAYER_LAG UINT32_C(0x8c1138c4)
static bool player_word(uint32_t address, uint32_t *value) {
    switch(address) {
    case PLAYER + 0xacu: *value = player.out; return true;
    case PLAYER + 0x90u: *value = player.skip; return true;
    case PLAYER + 0x1c0u: *value = player.frame; return true;
    case PLAYER_LAG: *value = player.lag; return true;
    case PLAYER + 8u: *value = player.stream; return true;
    case PLAYER_STREAM + 4u: *value = PLAYER_CHANNEL; return true;
    case PLAYER_CHANNEL + 68u: *value = PLAYER_AVI; return true;
    case PLAYER_AVI + 0x14u: *value = player.capacity; return true;
    case PLAYER_AVI + 0x64u: *value = player.released; return true;
    case PLAYER_AVI + 0x68u: *value = player.filled; return true;
    default: return false;
    }
}
#else
void kui_toy_movie_ahead_update(uint32_t ctx);
#endif

/* ---- what the stream needs beyond the shared model ---- */
static struct {
    uint8_t tstr;
    uint16_t ipra, tcr[3];
    uint32_t tcor[3], tcnt[3], slots[64], tcnt0;
} wx;
/* What one look at the clock costs the stream's own timing. */
static uint32_t clock_step = 5u;
/* The stream's clock is the reader's (kui_wire_test_read). */
static bool wire_clock_shared;
/* The reader looks at its clock once for every block it reads. With no
 * extra cost a visit reads all there is; 70 ticks a look is the console's
 * measured pace of about six blocks to an interrupt visit's 390 ticks. */
static void reader_pace(uint32_t ticks) {
    refill_clock_extra_ticks = ticks;
    clock_step = ticks ? ticks : 5u;
}
#if KUI_TOY_WIRE_STEADY
/* Test 75's phase timers look at the clock where the reader did not look
 * before. In this fixture a look moves the clock, and the reader's pace in
 * it is what the other checks were written for. So these looks are told
 * apart: each sees the clock as it stands, less three ticks for every one
 * of them so far. A span between two of them is then three ticks and
 * whatever the reader's own looks in between cost, and the reader's pace is
 * Test 74's to the tick. */
enum { PHASE_LOOK = 3u };
static uint32_t phase_spent;
static unsigned long phase_looks;
uint32_t kui_toy_wire_test_phase(void) {
    ++phase_looks;
    phase_spent += PHASE_LOOK;
    return clock_counter - phase_spent;
}
#endif
static const struct kui_toy_wire_report *report(void) {
    return (const void *)(arena + KUI_TOY_WIRE_REPORT_OFFSET);
}
#if KUI_TOY_WIRE_LATER || KUI_TOY_WIRE_QUIT || KUI_TOY_WIRE_ANY
/* The stream's own state: behind the window's slots in the arena. */
static const struct kui_wire_stream *wire_state(void) {
    return (const void *)(arena + 62u * 2352u);
}
#endif
/* The first word behind the report is the stream's "on" flag. */
static bool stream_on(void) {
    uint32_t on;
    memcpy(&on, arena + KUI_TOY_WIRE_REPORT_OFFSET + sizeof(struct kui_toy_wire_report), sizeof(on));
    return on != 0u;
}
/* The title's SDK table hangs on the title's vector base: Toy Commander's,
 * or another title's own (Test 81). The stream is told that base by the
 * low resident's word and asks the processor for the live one. */
#define WIRE_TABLE (wire_vbr + 0x200u)
uint32_t kui_wire_test_read(uint32_t address, unsigned width) {
    if(address == 0xffd00004u) return wx.ipra;
    if(address == 0xffd80004u) return wx.tstr;
    /* The stream reckons its waits' length by the game's clock, and here
     * on a count of its own, which its looks alone move: the scenarios
     * that account for every tick of the reader's clock rest on that.
     * Test 79 has the stream wait by that clock where it has no timer; a
     * scenario of that asks for the one clock the console has. */
    if(address == 0xffd8000cu) return wire_clock_shared ? (clock_counter -= clock_step) : (wx.tcnt0 -= clock_step);
    if(address >= 0xffd80008u && address < 0xffd8002cu) {
        const unsigned k = (address - 0xffd80008u) / 12u, field = (address - 0xffd80008u) % 12u;
        return field == 0u ? wx.tcor[k] : field == 4u ? wx.tcnt[k] : wx.tcr[k];
    }
    if(address >= WIRE_TABLE && address < WIRE_TABLE + 0x100u) return wx.slots[(address - WIRE_TABLE) / 4u];
    return kui_sci_stream_test_read(address, width);
}
static unsigned long sdir_writes; /* times MSB first was asked of the receiver */
void kui_wire_test_write(uint32_t address, uint32_t value, unsigned width) {
    /* The bit order: written only with the SCI stopped, only by a build
     * that asks, and back at zero before the module reset, which is not
     * relied on to clear it. */
    if(address == SCMR) {
        CHECK(!m.scr && (value == 0u || value == 8u) && (KUI_TOY_WIRE_MSB || !value));
        if(value) ++sdir_writes;
    }
    if(address == STBCR && (value & 1u)) CHECK(!m.scmr);
    if(address == PDTR) {
        /* The stream moves the card's chip select itself, as the probe did. */
        m.cs_high = (value & 0x80u) != 0u;
        if(m.cs_high) card.cmd_len = 0;
        return;
    }
    if(address == 0xffd00004u) {wx.ipra = (uint16_t)value; return;}
    if(address == 0xffd80004u) {wx.tstr = (uint8_t)value; return;}
    if(address >= 0xffd80014u && address < 0xffd8002cu) { /* TMU1 and TMU2 only */
        const unsigned k = (address - 0xffd80008u) / 12u, field = (address - 0xffd80008u) % 12u;
        if(field == 0u) wx.tcor[k] = value;
        else if(field == 4u) wx.tcnt[k] = value;
        else wx.tcr[k] = (uint16_t)value;
        return;
    }
    if(address >= WIRE_TABLE && address < WIRE_TABLE + 0x100u) {wx.slots[(address - WIRE_TABLE) / 4u] = value; return;}
    kui_sci_stream_test_write(address, value, width);
}
void kui_wire_test_settle(unsigned count) { kui_sci_stream_test_settle(count); }
void kui_wire_test_fence(void) { kui_sci_stream_test_fence(); }
uint32_t kui_wire_test_physical(const void *area) {
    CHECK(area == arena + sizeof(arena) - RING_BYTES);
    return RING_PHYSICAL;
}
uint32_t kui_wire_test_vbr(void) { return wire_vbr; }
void kui_wire_test_purge(const void *area, unsigned bytes) {
    CHECK(area == arena + sizeof(arena) - RING_BYTES && bytes == RING_BYTES);
}
#if KUI_TOY_WIRE_RING
/* Test 75 reads the ring through the cache. This is the ring as a cache
 * that never lets go of a line would show it: each line holds what memory
 * held when the reader last dropped it. A line read without being dropped
 * first gives the reader old bytes, and what it then hands on is not the
 * image's. */
static _Alignas(32) uint8_t ring_seen[RING_BYTES];
static unsigned long ring_drops;
const uint8_t *kui_wire_test_view(const uint8_t *area) {
    CHECK(area == arena + sizeof(arena) - RING_BYTES && !((uintptr_t)area & 31u));
    return ring_seen;
}
void kui_wire_test_drop(const uint8_t *line) {
    /* Any address inside the line names it, as for the instruction. */
    CHECK(line >= ring_seen && line < ring_seen + sizeof(ring_seen));
    const size_t at = (size_t)(line - ring_seen) & ~(size_t)31u;
    memcpy(ring_seen + at, arena + sizeof(arena) - RING_BYTES + at, 32u);
    ++ring_drops;
}
#endif
/* The auxiliary's gate: masked for the whole visit and never re-entered. */
uint32_t kui_native_readahead_aux_visit(uint32_t event, uint32_t arg,
    kui_retail_async_region_type *r, uint32_t reserved) {
    static unsigned depth;
    CHECK(!depth);
    ++depth;
    ++gate_visits;
    const uint32_t sr = hw.sr;
    hw.sr |= UINT32_C(0x100000f0);
    const uint32_t result = kui_native_readahead_aux_dispatch(event, arg, r, reserved);
    hw.sr = sr;
    --depth;
    return result;
}

/* ---- stalls ---- */
static uint32_t stall_seed, stall_mean, stall_left;
static unsigned stall_dense, burst_left;
static unsigned long stalls;
static uint32_t stall_random(void) { stall_seed = stall_seed * 1664525u + 1013904223u; return stall_seed >> 8; }
/* One byte of one card block lost on purpose: data byte cut_index of block
 * cut_lba, or with 512 the first byte of its CRC. The byte before it is
 * held in the receiver and the byte itself overruns. */
static uint32_t cut_lba;
static unsigned cut_index;
/* This many streams lose their second byte: the first is held in the
 * receiver and the next overruns, ahead of the stream's first token. */
static unsigned first_cuts;
/* The first stream loses the byte behind its hundredth: one byte of its
 * first block's data. */
static bool early_cut;
static bool stall(void) {
    static bool held;
    /* Only the stream's runs: the low reader's own block receptions keep
     * the shared model's own overrun injection. */
    if(m.dar < RING_PHYSICAL || m.dar > RING_PHYSICAL + RING_BYTES) return false;
    if(first_cuts && m.dar == RING_PHYSICAL && m.tcr == KUI_WIRE_SEGMENT_MAX) {
        if(!(m.ssr & RDRF)) return true;
        --first_cuts;
        m.late_take = true;
        return true;
    }
    if(early_cut && m.dar == RING_PHYSICAL + 100u && m.tcr == KUI_WIRE_SEGMENT_MAX - 100u) {
        if(!(m.ssr & RDRF)) return true;
        early_cut = false;
        m.late_take = true;
        return true;
    }
    if(cut_lba && card.streaming && card.lba == cut_lba && card.frame >= card.gap + 2u) {
        /* The byte just clocked out is this data index of the block. */
        const unsigned at = card.frame - card.gap - 2u;
        if(at + 1u == cut_index) return true;
        if(at == cut_index) {cut_lba = 0u; return true;}
    }
    if(!stall_mean) return false;
    /* A stall that matters lasts past the next byte: the receiver holds
     * one byte and the following one overruns. */
    if(held) {held = false; return true;}
    if(stall_left--) return false;
    held = true;
    ++stalls;
    m.late_take = stall_random() & 1u;
    if(burst_left) {
        --burst_left;
        stall_left = 1u + stall_random() % 40u;
    } else if(stall_random() % 100u < stall_dense) {
        burst_left = 2u + stall_random() % 50u;
        stall_left = 1u + stall_random() % 40u;
    } else stall_left = 64u + stall_random() % (2u * stall_mean);
    return true;
}
/* No programmed bus byte may be clocked while the stream owns the SCI, nor
 * with the receiver's bit order changed. */
static void bus_check(void) { CHECK(!stream_on() && !m.scmr); }

/* ---- raw sectors with address, EDC and real P/Q parity ---- */
static uint8_t ecc_f[256], ecc_b[256];
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
static uint8_t bcd_field(uint32_t n) { return (uint8_t)((n / 10u) * 16u + n % 10u); }
/* Where the movie's track begins in its file's first block: nought, but
 * for the one fixture that wants a track no piece of which lies on a
 * multiple of four. */
static uint32_t track_file_offset;
static void install_raw_sector(uint32_t lba, bool with_parity) {
    uint8_t raw[KUI_GAME_RAW_BYTES];
    const struct kui_retail_track *track = &manifest.slots[2].track;
    uint32_t first = (lba - track->start_lba) * KUI_GAME_RAW_BYTES;
    for(unsigned i = 0u; i < sizeof(raw); ++i) raw[i] = source(2u, first + i);
    first += track_file_offset;
    const uint32_t fad = lba + 150u;
    raw[12] = bcd_field(fad / 4500u);
    raw[13] = bcd_field((fad / 75u) % 60u);
    raw[14] = bcd_field(fad % 75u);
    const uint32_t edc = kui_cd_edc(raw, 16u + KUI_GAME_DATA_BYTES);
    for(unsigned i = 0u; i < 4u; ++i) raw[16u + KUI_GAME_DATA_BYTES + i] = (uint8_t)(edc >> (8u * i));
    memset(raw + 2068, 0, 8u);
    if(with_parity) {
        parity(raw + 12, 86u, 24u, 2u, 86u, raw + 2076);
        parity(raw + 12, 52u, 43u, 86u, 88u, raw + 2248);
    }
    CHECK(kui_sector_edc_valid_at(raw, fad));
    for(unsigned i = 0u; i < sizeof(raw); ++i) {
        const uint32_t file_block = (first + i) / 512u;
        const struct kui_retail_extent *extent = NULL;
        for(unsigned e = 0; e < track->extent_count; ++e) {
            const struct kui_retail_extent *x = &manifest.slots[kui_retail_track_first_extent(track) + e].extent;
            if(file_block >= x->file_block && file_block - x->file_block < x->blocks) extent = x;
        }
        CHECK(extent);
        image_card[(extent->card_lba + file_block - extent->file_block) * 512u + (first + i) % 512u] = raw[i];
    }
}

/* ---- the console's side ---- */
static unsigned long eris, timers, polls;
#if KUI_TOY_WIRE_TIMER
/* Test 81: whether the other title's scenario under way has that title
 * using both spare timers, and whether the next one shall. */
static bool other_timers_used, other_title_busy;
#endif
#if KUI_TOY_WIRE_CLEAN
static uint32_t light_bad_expected;
#endif
/* The player's update has returned after `ticks` of its own, and the hook
 * calls the reader's side. Test 73's stub hands over the clock value it
 * read before the original. */
static void player_update_for(uint32_t ticks) {
#if KUI_TOY_WIRE_IDLE
    /* The reader's own clock in this fixture. */
    const uint32_t entered = clock_counter;
    clock_counter -= ticks;
    kui_toy_movie_ahead_update(PLAYER, entered);
#else
    (void)ticks;
    kui_toy_movie_ahead_update(PLAYER);
#endif
}
static void player_update(void) { player_update_for(0u); }
/* What the model's receiver does with SCSCMR1.SDIR in the runs that follow. */
static unsigned sdir_answer;
/* The disc in the drive: the raw track, and which of its sectors are the
 * second movie's file. With `playing` the game's player has started that
 * movie, as every scenario up to Test 76 has it. */
#if KUI_TOY_WIRE_SEQ
/* The next card has its large data track as 2,048-byte sectors. */
static bool card_cooked;
#endif
static void open_card(unsigned extent_blocks, bool scattered, bool with_parity,
    struct kui_toy_movie_range file, bool playing) {
    wire_vbr = WIRE_TOY_VBR;
    setup_movie_card_extents(1u, extent_blocks, scattered);
#if KUI_TOY_WIRE_SEQ
    if(card_cooked) {
        /* setup_movie_card_extents() again, with the base fixture's other
         * disc: its first and third tracks hold 2,048-byte sectors. */
        cooked_fixture = true;
        setup(KUI_NATIVE_SDK_SCI_OWNER_VBR, X, extent_blocks, scattered);
        mode(2048u);
        card.nac_first = 1u;
        kui_retail_hook_active = kui_retail_hook_fault = 0u;
        R.vector600[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX] = UINT32_C(0x8c1add0c);
        R.vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX] = KUI_NATIVE_SDK_SCI_OWNER_VBR;
        R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = KUI_NATIVE_SDK_SCI_COOKIE;
        eri_slot = rxi_slot = callback();
        cooked_fixture = false;
        CHECK(kui_retail_track_sector_bytes(&manifest.slots[2].track) == 2048u && !track_file_offset);
    } else
#endif
    {
    /* A track that begins inside its file's first block ends inside one
     * more than the fixture mapped: its last sector is left out. */
    manifest.slots[2].track.first_extent |= (uint16_t)(track_file_offset << 8u);
    for(uint32_t lba = manifest.slots[2].track.start_lba; lba < manifest.slots[2].track.end_lba - !!track_file_offset; ++lba)
        install_raw_sector(lba, with_parity);
    }
    /* (The fixture's reference reader follows the same map.) */
    struct kui_native_ra_config config = movie_configuration();
    config.movie_ranges[3] = file;
    bind_movie_configuration(&config);
    if(playing) {
        fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
        /* The player has started the second movie: its presentation clock
         * runs from here, and with it the report's course of that movie. */
        kui_toy_movie_present_boundary(PLAYER,
            (uintptr_t)&kui_native_readahead_aux_context.movie_exit[1], KUI_TOY_MOVIE_EXIT_STARTED);
    }
#if !KUI_TOY_WIRE_OPEN
    /* The intro's quiet opening phase is over: the window may serve. */
    kui_native_readahead_aux_context.onset[1].quiet_ctx = UINT32_MAX;
#else
    /* Test 74: the player's refills are never held back. */
    CHECK(!kui_toy_movie_refill_defer(UINT32_C(0x8c200200), UINT32_C(0x8c200300), 13u * 2048u));
#endif
    /* Test 74 has no such phase: the window serves the second movie from
     * its first request, with the hold's state as the player's start left it. */
    m.sdir_answer = sdir_answer;
    memset(&wx, 0, sizeof(wx));
    for(unsigned i = 0; i < 64u; ++i) wx.slots[i] = UINT32_C(0x8c0be408);
    wx.tcnt0 = UINT32_C(0x60000000);
    wx.tstr = 1u; /* the game's clock, TMU0, runs */
#if KUI_TOY_WIRE_TIMER
    /* Test 81: the stream takes its timer by what the low resident holds
     * of the title's table. The table modelled here is Toy Commander's:
     * so is the default the resident admitted. (The base fixture has
     * another title's word there, which no one read before.) */
    R.vector600[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX] = UINT32_C(0x8c0be408);
    other_timers_used = false;
#endif
    wire_clock_shared = false;
    m.extra = arena + sizeof(arena) - RING_BYTES;
    m.extra_base = RING_PHYSICAL;
    m.extra_bytes = RING_BYTES;
    m.stall_hook = stall;
    m.bus_check = bus_check;
    m.delay = 0u;
    stall_mean = 0u; stall_dense = 0u; stall_left = 0u; burst_left = 0u; stall_seed = 4242u;
    cut_lba = 0u; first_cuts = 0u; early_cut = false;
    eris = timers = polls = 0;
#if KUI_TOY_WIRE_IDLE
    /* A megabyte of buffer, three quarters full. */
    memset(&player, 0, sizeof(player));
    player.stream = PLAYER_STREAM;
    player.capacity = 0x100000u; player.released = 0x4000u; player.filled = 0x4000u + 0xc0000u;
    chunk_word_other = player_word;
#endif
}
/* The second movie's file over all of the track the scenarios read, the
 * movie playing, and its first refill about to be asked for at FIRST. */
static void open_movie(unsigned extent_blocks, bool scattered, bool with_parity) {
    open_card(extent_blocks, scattered, with_parity,
        (struct kui_toy_movie_range){FIRST - 386u, WINDOW_END - (FIRST - 386u)}, true);
#if KUI_TOY_WIRE_SEQ
    /* Test 77 opens a window for a read that begins where the read before
     * it ended. The scenarios begin where they always have, at the movie's
     * first refill: the game has just read the opening fill, which ends
     * one sector before it, and that is where the service's drive position
     * stands. (In this fixture it stands there from the start, the track's
     * first sector being the one before; the line says what the scenarios
     * rest on. The sequence scenarios read their predecessors through the
     * low reader.) */
    R.shared.service.position_lba = FIRST - 1u;
#endif
}
static void invariants(void) {
    /* The resident routes the SCI's interrupt to the auxiliary, and keeps
     * its level, exactly while its stream state says a reception is ours. */
    if(stream_on()) {
        CHECK(R.shared.stream.state == 2u && R.engine.hooked);
        CHECK((hw.iprb & 0x00f0u) == 0x0010u);
    } else {
        /* Nothing may be left receiving into the ring. */
        CHECK(!(m.rx && m.dar >= RING_PHYSICAL && m.dar <= RING_PHYSICAL + RING_BYTES));
    }
    CHECK(!R.shared.port.fault && m.healthy);
#if KUI_TOY_WIRE_CLEAN
    /* The lighter sector check has never passed a sector its EDC refuses,
     * but where a test stores wrong data on the card to see it found. */
    CHECK(report()->light_bad <= light_bad_expected);
#endif
}
static bool timer_running(unsigned *channel) {
    for(unsigned k = 1u; k <= 2u; ++k)
        if((wx.tstr & (1u << k)) && (wx.tcr[k] & 0x0020u)) {*channel = k; return true;}
    return false;
}
/* One moment of console time: whatever interrupt is due, else nothing.
 * With `wire_pace` the wire brings that many bytes a moment: the model
 * lands a run whole, so the moments its bytes took are owed before the
 * next run may land, and the stream's timer can call meanwhile. */
static unsigned run_owed;
static bool hardware_at(unsigned wire_pace) {
    unsigned channel;
    if(stream_on()) {
        /* A run in flight ends at its overrun; the SCI then asks for its
         * error interrupt at the resident's level. */
        if(m.rx) {
            if(run_owed) --run_owed;
            else {
                const unsigned before = m.received;
                elapse();
                (void)kui_sci_stream_test_read(SSR, 1u);
                if(wire_pace) run_owed = (m.received - before + wire_pace - 1u) / wire_pace - 1u;
            }
        }
        if((m.ssr & ORER) && m.scr == 0x50u) {
            CHECK(kui_retail_sdk_sci_dispatch(0x4e0u) == 0u);
            check_original_sdk_owner();
            ++eris;
            invariants();
            return true;
        }
        if(timer_running(&channel)) {
            CHECK(wx.slots[(0x8c00f65cu - (0x4e0u >> 3) + ((0x400u + 0x20u * channel) >> 3) - 0x8c00f600u) / 4u] ==
                (uint32_t)(uintptr_t)kui_toy_wire_timer_callback);
            CHECK((wx.ipra >> (12u - 4u * channel)) & 15u);
            wx.tcr[channel] |= 0x0100u;
            kui_toy_wire_timer_callback(0x400u + 0x20u * channel);
            ++timers;
            invariants();
            return true;
        }
        /* Nothing is due, but the wire runs on. */
        return m.rx;
    } else if(kui_sci_stream_busy()) {
        independent_sdk_irq();
        return true;
    }
    return false;
}
static bool hardware(void) { return hardware_at(0u); }
static void game_poll(void) {
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
    ++polls;
    invariants();
}
/* Read one request as the game does and check what arrived. */
static void read_request(uint32_t lba, uint32_t count, unsigned poll_every) {
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(lba, count, OUTPUT);
    invariants();
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 200000u && !R.engine.failed);
        const bool moved = hardware();
        if(!moved || (poll_every && n % poll_every == 0u)) game_poll();
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == count * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(lba, count, OUTPUT);
    invariants();
}
static void idle(unsigned moments) {
    for(unsigned n = 0u; n < moments; ++n) if(!hardware()) break;
}
#if KUI_TOY_WIRE_FLUID
/* Wait ticks the report placed in a piece of the course, over all runs. */
#if !KUI_TOY_WIRE_ANY
static uint64_t pause_seen;
#endif
#endif
#if KUI_TOY_WIRE_COPY
/* Test 76's copy into the window. The hand-written routine is SH-4 code,
 * which tests/test_toy_wire_linked_copy.py runs as linked. Here its place
 * is taken by one that holds the caller to that routine's terms: whole
 * words at places that are multiples of four, inside the window. */
static unsigned long copies, copied;
void kui_wire_test_copy(uint8_t *out, const uint8_t *in, uint32_t bytes) {
    CHECK(!(((uintptr_t)out | (uintptr_t)in | bytes) & 3u) && bytes && bytes <= 512u);
    CHECK(out >= arena && out + bytes <= arena + 62u * KUI_GAME_RAW_BYTES);
    memcpy(out, in, bytes);
    ++copies;
    copied += bytes;
}
#endif
#if KUI_TOY_WIRE_MEASURE
/* The report's account of the reader's visits agrees with the console's side. */
static void visits_accounted(void) {
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_SCI] == trace[5] && w->visit_calls[KUI_TOY_WIRE_VISIT_GAME] == trace[4]);
    CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_TIMER] == timers);
    for(unsigned i = 0; i < KUI_TOY_WIRE_VISITS; ++i) {
#if !KUI_TOY_WIRE_FLUID
        CHECK(w->visit_max[i] <= w->visit_ticks[i]);
#endif
        CHECK(!w->visit_calls[i] == !w->visit_ticks[i]);
    }
    /* A timer call was asked for only by a visit that ran out of time, and
     * only Test 69 asks. */
    CHECK(w->n.ticks <= w->budget_ends && (KUI_TOY_WIRE_TICK || !w->n.ticks));
#if !KUI_TOY_WIRE_FLUID
    for(unsigned i = 0; i < 2u; ++i) CHECK(w->wait_max[i] <= w->wait_ticks[i] && !w->wait_calls[i] == !w->wait_ticks[i]);
    CHECK(!w->wait_calls[0]);
#endif
    const uint32_t restarts = w->stops[KUI_TOY_WIRE_STOP_SECTOR] + w->stops[KUI_TOY_WIRE_STOP_FRAME] +
        w->stops[KUI_TOY_WIRE_STOP_FAULT];
#if KUI_TOY_WIRE_STEADY
    /* Test 75 takes the reader's time apart by the clock. Nothing looks at
     * this fixture's clock while a block is taken out of the ring, while
     * the stream is served, while a sector is checked or while a request
     * is copied out, so each of those spans is one phase look and the
     * counts are exact. A block's feeding is one look and two more for a
     * sector it completes, the check's own. A reopening runs the low
     * reader, which looks at the clock itself. And the parts lie inside
     * the visits they were taken in. */
    {
        const uint32_t look = PHASE_LOOK, fed = look * w->n.blocks;
        CHECK(w->feed_ticks >= fed && (w->feed_ticks - fed) % (2u * look) == 0u);
        CHECK(w->feed_ticks - fed <= 2u * w->check_ticks);
        CHECK(w->check_ticks % look == 0u);
#if KUI_TOY_WIRE_CLEAN
        /* Every sector taken was checked once; so was every one refused. */
        CHECK(w->check_ticks / look >= w->light + w->edc);
        CHECK(w->check_ticks / look <= w->light + w->edc + w->check_bad);
#endif
        CHECK(w->parse_ticks >= fed && w->parse_ticks % look == 0u);
        /* The stream is served before every block asked of it. */
        CHECK(w->service_ticks >= w->parse_ticks && w->service_ticks % look == 0u);
        CHECK(w->publish_ticks == look * kui_native_readahead_aux_context.stats.hit_requests);
        CHECK(!w->seek_blocks == !w->seek_ticks || (w->seek_ticks && w->declined));
        CHECK(w->seek_ticks < UINT32_C(0x40000000) && w->parse_ticks < UINT32_C(0x40000000));
        CHECK(w->service_ticks < UINT32_C(0x40000000) && w->check_ticks < UINT32_C(0x40000000));
        CHECK(w->parse_ticks + w->feed_ticks <=
            w->visit_ticks[KUI_TOY_WIRE_VISIT_SCI] + w->visit_ticks[KUI_TOY_WIRE_VISIT_GAME] +
            w->visit_ticks[KUI_TOY_WIRE_VISIT_TIMER]);
    }
#endif
#if KUI_TOY_WIRE_FLUID
    /* Test 76 times the ending of streams: each one is the one look the
     * stop itself is in this fixture, whatever the stream stood at. */
    {
        uint32_t ended = 0;
        for(unsigned i = 0; i < KUI_TOY_WIRE_STOPS; ++i) ended += w->stops[i];
        CHECK(w->halt_ticks == PHASE_LOOK * ended);
    }
    (void)restarts;
#elif KUI_TOY_WIRE_IDLE
    /* Test 73 does not time restarts or place them in the course. */
    CHECK(!w->restart_ticks && !w->restart_max);
#else
    CHECK(w->restart_max <= w->restart_ticks && (restarts || !w->restart_ticks));
#endif
#if KUI_TOY_WIRE_ANY
    /* Test 78 keeps nothing by piece of the course. */
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i)
        CHECK(!w->course_frames[i] && !w->course_sectors[i] && !w->course_wait[i] && !w->course_interrupt[i] &&
              !w->course_game[i] && !w->course_free[i] && !w->course_pause[i]);
    CHECK(!w->buffer_capacity);
#elif KUI_TOY_WIRE_WORDS
    /* The second movie's course adds up to the same totals: everything in
     * these runs happens while it plays. */
    uint32_t wait = 0, interrupt = 0, game = 0, sectors = 0, stops = 0;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        wait += w->course_wait[i]; interrupt += w->course_interrupt[i]; game += w->course_game[i];
        sectors += w->course_sectors[i];
#if !KUI_TOY_WIRE_FLUID
        stops += w->course_stops[i];
#endif
    }
#if KUI_TOY_WIRE_FLUID
    (void)wait;
    CHECK(game == w->visit_ticks[KUI_TOY_WIRE_VISIT_GAME]);
    /* The wire's standstill by piece is the stream's own count, less what
     * no visit has come by to take since. */
    uint32_t pause = 0;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) pause += w->course_pause[i];
    CHECK(pause <= w->n.wait_ticks && (!w->n.waits || w->n.wait_ticks));
    pause_seen += pause;
#else
    /* Test 74 keeps the waits by piece only. */
    CHECK((KUI_TOY_WIRE_LEAN ? !w->wait_ticks[1] : wait == w->wait_ticks[1]) && game == w->visit_ticks[KUI_TOY_WIRE_VISIT_GAME]);
#endif
    CHECK(interrupt == w->visit_ticks[KUI_TOY_WIRE_VISIT_SCI] + w->visit_ticks[KUI_TOY_WIRE_VISIT_TIMER]);
    CHECK(sectors == trace[1] && stops == (KUI_TOY_WIRE_IDLE ? 0u : restarts));
#endif
}
#else
static void visits_accounted(void) {}
#endif
static void closed(void) {
    /* The SCI is the low reader's again, in its command framing. */
    CHECK(!stream_on() && m.scr == 0x30u && m.smr == 0x80u && m.brr == 0u && !m.scmr && m.cs_high);
    CHECK(!(m.chcr & 1u) && m.sar == 0x11111111u && m.dar == 0x22222222u && m.tcr == 0x33u);
    unsigned channel;
    CHECK(!timer_running(&channel) && !(wx.ipra & 0x0ff0u));
    for(unsigned i = 0; i < 64u; ++i) CHECK(wx.slots[i] == UINT32_C(0x8c0be408));
}

static void steady(unsigned extent_blocks, bool scattered, uint32_t mean, unsigned dense, unsigned poll_every) {
    open_movie(extent_blocks, scattered, true);
    stall_mean = mean; stall_dense = dense;
    const struct kui_toy_wire_report *w = report();
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, poll_every);
    lba += REQUEST;
    if(getenv("WIRE_VERBOSE") && kui_native_readahead_aux_context.stats.hit_requests != 1u)
        fprintf(stderr, "first request: hits %u starts %u order %u undecided %u lost_frame %u bad_crc %u blocks %u drops %u %u %u %u %u %u stops %u %u %u %u %u declined %u first_error %u seek %u\n",
            (unsigned)kui_native_readahead_aux_context.stats.hit_requests, (unsigned)w->n.starts,
#if KUI_TOY_WIRE_LEAN
            (unsigned)w->n.order, (unsigned)w->n.undecided,
#else
            0u, 0u,
#endif
            (unsigned)w->n.lost_frame, (unsigned)w->n.bad_crc, (unsigned)w->n.blocks,
            (unsigned)w->drops[0], (unsigned)w->drops[1], (unsigned)w->drops[2], (unsigned)w->drops[3], (unsigned)w->drops[4], (unsigned)w->drops[5],
            (unsigned)w->stops[0], (unsigned)w->stops[1], (unsigned)w->stops[2], (unsigned)w->stops[3], (unsigned)w->stops[4],
            (unsigned)w->declined, (unsigned)w->first_error, (unsigned)w->seek_blocks);
    CHECK(w->n.starts >= 1u && kui_native_readahead_aux_context.stats.hit_requests == 1u);
    unsigned warm = 0;
    while(lba + REQUEST <= WINDOW_END) {
        /* Between requests the game runs and the stream goes on. */
        idle(1u + lba % 5u);
        const uint32_t visits = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][5];
        const unsigned before = (unsigned)polls;
        read_request(lba, REQUEST, poll_every);
        if(polls == before && visits == kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][5]) ++warm;
        lba += REQUEST;
    }
    const unsigned requests = (WINDOW_END - FIRST) / REQUEST;
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == requests);
    CHECK(w->magic == KUI_TOY_WIRE_REPORT_MAGIC && !w->first_error && !w->declined);
    CHECK(w->parity == (KUI_TOY_WIRE_CLEAN ? 0u : 1u) && w->timer_mode == 2u);
    CHECK(w->n.cuts + w->n.segments >= 1u && w->n.blocks >= 1u);
#if KUI_TOY_WIRE_CLEAN
    /* Every sector the window took was taken one way or the other; the
     * low reader's blocks, the first of every stream among them, never the
     * lighter way; and in this image, with real parity and no wrong byte
     * anywhere, every rebuilt byte the parity was asked about fitted. */
    const uint32_t took = kui_native_readahead_aux_context.movie_ahead.verified - FIRST;
    CHECK(w->light + w->edc >= took && w->light + w->edc <= took + w->check_bad + w->refused);
    if(getenv("WIRE_VERBOSE") || w->mend_bad || !w->light || !w->edc)
        fprintf(stderr, "sector checks: light %u edc %u mends %u mend_bad %u light_bad %u check_bad %u starts %u took %u\n",
            (unsigned)w->light, (unsigned)w->edc, (unsigned)w->mends, (unsigned)w->mend_bad, (unsigned)w->light_bad,
            (unsigned)w->check_bad, (unsigned)w->n.starts, (unsigned)took);
    CHECK(w->edc && !w->mend_bad);
    /* On a quiet bus, and with stalls as rare as a console run's, most
     * sectors are taken the lighter way; with stalls far denser than any
     * console run nearly every sector has a block the stream could not
     * check, and the EDC does the work as before. */
    /* Each stream's first sector comes from the low reader. */
    if(!mean) CHECK(w->edc >= w->n.starts);
    /* On a quiet bus with nothing scattered this model is the same run
     * every time, and so is which of its 290 sectors takes the EDC: the
     * low reader's first, and those a rebuilt block reaches whose byte
     * could not be confirmed inside them. A change to those rules shows
     * here, also one that only makes the reader check more than it must. */
#if KUI_TOY_WIRE_SEQ
    /* Test 77's window reads on to the track's end, nine sectors behind
     * the file's: the same sixteen take the EDC, and the nine more are
     * taken the lighter way with three rebuilt bytes among them. */
    if(!mean && !scattered && !refill_clock_extra_ticks)
        CHECK(took == 299u && w->light == 283u && w->edc == 16u && w->mends == 84u);
#else
    if(!mean && !scattered && !refill_clock_extra_ticks)
        CHECK(w->light == 274u && w->edc == 16u && w->mends == 81u);
#endif
    if(!mean && !scattered) CHECK(w->light > 4u * w->edc && w->mends);
    else if(!mean || mean >= 6000u) CHECK(w->light > (scattered ? 1u : 2u) * w->edc && w->mends);
#endif
#if KUI_TOY_WIRE_MSB
    /* The receiver's answer was read right. With stalls far denser than
     * any console run the question can go unanswered: four streams in a row
     * that lose a byte ahead of their first token end the asking, and a
     * token read MSB first stays unconfirmed while no block arrives whole.
     * Either way the data is the image's. */
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "bit order %u, undecided %u\n", (unsigned)w->n.order, (unsigned)w->n.undecided);
    if(!sdir_answer) CHECK(w->n.order == KUI_WIRE_ORDER_LSB);
    else if(!mean || mean >= 1500u) CHECK(w->n.order == KUI_WIRE_ORDER_MSB);
    else CHECK(w->n.order == KUI_WIRE_ORDER_MSB || (w->n.order == KUI_WIRE_ORDER_SEEN && !w->light) ||
        (w->n.order == KUI_WIRE_ORDER_LSB && w->n.undecided == 4u));
#endif
    if(!mean) {
        CHECK(!w->n.cuts && !w->n.waits && !w->refused && !w->check_bad && !w->n.unsolved);
        CHECK(!w->stops[KUI_TOY_WIRE_STOP_SECTOR] && !w->stops[KUI_TOY_WIRE_STOP_FRAME] &&
            !w->stops[KUI_TOY_WIRE_STOP_FAULT]);
        /* At the console's pace how many requests find the window ready
         * depends on the time between them: playing() below checks that. */
        if(!scattered) CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u &&
            (refill_clock_extra_ticks || warm >= requests - 3u));
        else CHECK(w->stops[KUI_TOY_WIRE_STOP_JUMP] >= 2u && card.cmd18 >= 3u);
    }
    if(w->stops[KUI_TOY_WIRE_STOP_FAULT] || w->n.unsolved)
        fprintf(stderr, "fault stops %u, unsolved %u, first error %u detail %x, mean %u dense %u\n",
            (unsigned)w->stops[KUI_TOY_WIRE_STOP_FAULT], (unsigned)w->n.unsolved,
            (unsigned)w->first_error, (unsigned)w->first_detail, (unsigned)mean, dense);
    CHECK(!w->stops[KUI_TOY_WIRE_STOP_FAULT] && !w->n.unsolved);
    visits_accounted();
    printf("  %s extents, stall every ~%u bytes, %u%% busy phases, poll every %u: %u requests, %u served on arrival; "
           "streams %u, cuts %u, waits %u, fulls %u, blocks %u (rebuilt %u, holed %u), sectors filled %u (most lost %u), "
           "re-read %u+%u, stream ends close/jump/sector/frame %u/%u/%u/%u, CMD18 %u, CMD12 %u, ERI %lu, timer %lu\n",
        scattered ? "scattered" : "contiguous", (unsigned)mean, dense, poll_every, requests, warm,
        (unsigned)w->n.starts, (unsigned)w->n.cuts, (unsigned)w->n.waits, (unsigned)w->n.fulls, (unsigned)w->n.blocks,
        (unsigned)w->n.rebuilt, (unsigned)w->n.holed, (unsigned)w->filled, (unsigned)w->most_lost,
        (unsigned)w->refused, (unsigned)w->check_bad, (unsigned)w->stops[0], (unsigned)w->stops[1],
        (unsigned)w->stops[2], (unsigned)w->stops[3], card.cmd18, card.cmd12, eris, timers);
}

/* The console's Test 68 run. While the movie plays the game calls once a
 * frame, for the frame's sectors, and decodes in between: only interrupts
 * reach the reader then. Reading a block takes the reader real time, so an
 * interrupt visit's 390 ticks read about six blocks where a full run brings
 * sixteen. Test 68 left the rest in the ring until the game's next call,
 * the stream stood still for room, and the window ran dry. Test 69 goes on
 * reading from the stream's timer. */
static unsigned playing(uint32_t mean, unsigned dense, uint32_t pace) {
    /* A moment is 1.3 ms: the wire brings 2048 bytes, and one timer call
     * with its visit fits. A frame is 25 moments, in which the wire could
     * bring 51 KB where the movie needs under 31. */
    enum { WIRE_PACE = 2048u, FRAME_MOMENTS = 25u };
    open_movie(2000u, false, true);
    stall_mean = mean; stall_dense = dense;
    reader_pace(pace);
    /* A run lands when its time has passed, not when a visit looks. */
    m.delay = 100000u;
    run_owed = 0u;
    const struct kui_toy_wire_report *w = report();
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, 0u);
    lba += REQUEST;
    unsigned requests = 0, warm = 0, ready = 0;
    while(lba + REQUEST <= WINDOW_END) {
        /* A frame is decoded: interrupts come as they are due, no call. */
        for(unsigned moment = 0u; moment < FRAME_MOMENTS; ++moment) if(!hardware_at(WIRE_PACE)) break;
        /* Is the frame's data there when the game asks, or does the reader
         * still have to read it inside the game's call? */
        if(kui_native_readahead_aux_context.movie_ahead.variant &&
           kui_native_readahead_aux_context.movie_ahead.first == lba &&
           kui_native_readahead_aux_context.movie_ahead.verified - lba >= REQUEST) ++ready;
        const unsigned before = (unsigned)polls;
        read_request(lba, REQUEST, 0u);
        if(polls == before) ++warm;
        ++requests;
        lba += REQUEST;
    }
    reader_pace(0u);
    m.delay = 0u;
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == requests + 1u);
    CHECK(!w->first_error && !w->declined && !w->stops[KUI_TOY_WIRE_STOP_FAULT] && w->timer_mode == 2u);
    visits_accounted();
#if KUI_TOY_WIRE_TICK
    /* With stalls far denser than any console run the wire itself and the
     * sectors read again set the pace: only the data is checked there. */
    const bool paced_by_reader = !dense;
    /* Every frame's data is there when the game asks, but for the few
     * behind a stream that had to be started again. */
    const unsigned again = (unsigned)(w->n.starts - 1u);
    if(paced_by_reader) {
        CHECK(ready + 2u * again >= requests && w->n.ticks && w->budget_ends);
        CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_TIMER] && w->wait_calls[1] <= 1u + 2u * again);
    }
    CHECK(w->wait_calls[1] >= requests - warm);
    printf("  playing, stall every ~%u bytes, %u%% busy phases: data ready for %u of %u frames when asked; "
           "visits out of time %u, timer calls asked %u, timer visits %lu, SCI visits %lu, fulls %u, streams %u, "
           "requests that waited %u for %u ticks (longest %u)\n", (unsigned)mean, dense, ready, requests,
        (unsigned)w->budget_ends, (unsigned)w->n.ticks, timers, eris, (unsigned)w->n.fulls, (unsigned)w->n.starts,
        (unsigned)w->wait_calls[1], (unsigned)w->wait_ticks[1], (unsigned)w->wait_max[1]);
#else
#if KUI_TOY_WIRE_MEASURE
    /* Without the timer's help the report still says what happened: visits
     * that ran out of time, and requests that had to wait. */
    if(!mean && pace >= 70u) CHECK(w->budget_ends && !w->n.ticks);
#if !KUI_TOY_WIRE_LEAN
    if(!mean && pace >= 70u) CHECK(w->wait_calls[1] >= requests / 2u);
    CHECK(w->wait_calls[1] >= requests - warm);
#endif
#endif
    /* The stall the console showed. On a quiet bus nearly all the data
     * comes in full runs, an interrupt visit reads a third of each, and
     * the stream stands still for room until the game's next call: no
     * frame finds its data ready. */
#if !KUI_TOY_WIRE_LATER
    if(!mean && pace >= 70u) CHECK(ready * 2u < requests && w->n.fulls >= requests);
#else
    /* Test 75: an interrupt visit that reads at all reads as long as a
     * call of the game's may. With no call between the requests it is
     * interrupt visits that read here, every one of them because the ring
     * was half unread or the stream stood still, and every frame but the
     * first few finds its data ready. */
    if(!mean && pace >= 70u) CHECK(ready + 2u >= requests && w->later);
#endif
    printf("  playing, stall every ~%u bytes, %u%% busy phases, %u ticks a block, without the timer's help: data "
           "ready for %u of %u frames when asked; fulls %u, SCI visits %lu, timer visits %lu\n", (unsigned)mean, dense,
        (unsigned)pace, ready, requests, (unsigned)w->n.fulls, eris, timers);
#endif
    (void)warm;
    return ready;
}

#if KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_ANY
/* The course's pieces are 2^22 ticks of the movie's own clock, and what
 * comes after the twelfth is kept in it. */
static void course_pieces(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, 3u); lba += REQUEST;
    CHECK(w->course_sectors[0] == REQUEST && !w->course_sectors[3]);
    clock_counter -= UINT32_C(3) << KUI_TOY_WIRE_BUCKET_SHIFT;
    read_request(lba, REQUEST, 3u); lba += REQUEST;
    read_request(lba, REQUEST, 3u); lba += REQUEST;
    CHECK(w->course_sectors[0] == REQUEST && w->course_sectors[3] == 2u * REQUEST && w->course_game[3]);
    clock_counter -= UINT32_C(40) << KUI_TOY_WIRE_BUCKET_SHIFT;
    read_request(lba, REQUEST, 3u);
    CHECK(w->course_sectors[KUI_TOY_WIRE_BUCKETS - 1u] == REQUEST && w->course_game[KUI_TOY_WIRE_BUCKETS - 1u]);
    for(unsigned i = 1; i < KUI_TOY_WIRE_BUCKETS - 1u; ++i) if(i != 3u) CHECK(!w->course_sectors[i] && !w->course_game[i]);
    /* Once the movie is over nothing more is added. */
    kui_toy_movie_present_boundary(UINT32_C(0x8c00dc90),
        (uintptr_t)&kui_native_readahead_aux_context.movie_exit[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    const uint32_t game = w->course_game[KUI_TOY_WIRE_BUCKETS - 1u];
    game_poll();
    CHECK(w->course_game[KUI_TOY_WIRE_BUCKETS - 1u] == game);
    printf("  the second movie's course: pieces of 2^22 ticks from its start, the last keeps the rest\n");
}
#endif

/* The player's update calls. While a movie plays the game calls the
 * player's update about 2,500 times a second, between and during its
 * frames, and the hook passes the calls on to the reader. Test 71 passed
 * every one on; Test 72 passes on those the reader has work for and
 * otherwise one in four milliseconds. What the game is served must not
 * change, and its data must still be ready when it asks. */
static void updating(uint32_t mean, unsigned dense) {
    open_movie(2000u, false, true);
    stall_mean = mean; stall_dense = dense;
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    reader_pace(70u);
    m.delay = 100000u;
    run_owed = 0u;
    unsigned long calls = 0;
    /* Before the window opens an update visit has nothing to do. */
    const unsigned long gate = gate_visits;
    for(unsigned i = 0; i < 50u; ++i, ++calls) player_update();
    CHECK(!trace[4]);
#if KUI_TOY_WIRE_PACE
    CHECK(gate_visits == gate);
#else
    CHECK(gate_visits == gate + 50u);
#endif
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, 0u);
    lba += REQUEST;
    unsigned requests = 0, ready = 0;
    const unsigned long first_gate = gate_visits;
    const unsigned long first_calls = calls;
#if KUI_TOY_WIRE_PACE
    uint32_t pumps = 0, passed_on = 0;
#endif
    while(lba + REQUEST <= WINDOW_END) {
        for(unsigned moment = 0u; moment < 25u; ++moment) {
            /* Three update calls to a moment of 1.3 ms. */
            for(unsigned i = 0; i < 3u; ++i, ++calls) {
#if KUI_TOY_WIRE_PACE
                /* A call that is passed on reads: it is not held back a
                 * second time behind the gate. */
                const uint32_t pumped = trace[4], on = w->update_work + w->update_due;
                player_update();
                pumps += trace[4] - pumped;
                passed_on += w->update_work + w->update_due - on;
#else
                player_update();
#endif
                invariants();
            }
            (void)hardware_at(2048u);
        }
        if(kui_native_readahead_aux_context.movie_ahead.variant &&
           kui_native_readahead_aux_context.movie_ahead.first == lba &&
           kui_native_readahead_aux_context.movie_ahead.verified - lba >= REQUEST) ++ready;
        read_request(lba, REQUEST, 0u);
        ++requests;
        lba += REQUEST;
    }
    reader_pace(0u);
    m.delay = 0u;
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == requests + 1u);
    CHECK(!w->first_error && !w->declined && !w->stops[KUI_TOY_WIRE_STOP_FAULT]);
    visits_accounted();
    const unsigned long passed = gate_visits - first_gate, made = calls - first_calls;
#if KUI_TOY_WIRE_PACE
    CHECK(w->updates == calls && w->update_due);
    CHECK(passed_on == w->update_work + w->update_due && pumps == passed_on);
    /* Far fewer visits than calls, and still the frame's data is ready
     * unless streams had to be started again. */
    CHECK((w->update_work + w->update_due) * 4u <= made);
    if(!dense && ready + 2u * (w->n.starts - 1u) < requests)
        fprintf(stderr, "updating %u/%u: ready %u of %u, work %u due %u starts %u fulls %u\n", (unsigned)mean, dense,
            ready, requests, (unsigned)w->update_work, (unsigned)w->update_due, (unsigned)w->n.starts, (unsigned)w->n.fulls);
    if(!dense) CHECK(ready + 2u * (w->n.starts - 1u) >= requests);
    /* On a quiet bus the ring fills between interrupt visits: those are
     * the calls passed on for work. */
    if(!mean) CHECK(w->update_work);
    printf("  update calls, stall every ~%u bytes, %u%% busy phases: %lu calls, %u passed on for work and %u as due; "
           "all visits %lu; data ready for %u of %u frames when asked; fulls %u, streams %u\n",
        (unsigned)mean, dense, calls, (unsigned)w->update_work, (unsigned)w->update_due, passed, ready, requests,
        (unsigned)w->n.fulls, (unsigned)w->n.starts);
#else
    /* Every update call with a window is a visit. */
    CHECK(passed >= made);
    printf("  update calls, stall every ~%u bytes, %u%% busy phases: %lu calls, each one a visit; all visits %lu; "
           "data ready for %u of %u frames when asked; fulls %u, streams %u\n",
        (unsigned)mean, dense, calls, passed, ready, requests, (unsigned)w->n.fulls, (unsigned)w->n.starts);
#endif
}

#if KUI_TOY_WIRE_PACE
/* The game's own GD calls. While a movie plays it calls EXEC and CHECK
 * about 1,550 times a second for some thirty requests; the low reader
 * passes each on as a delivery visit. Up to Test 72 every one that found
 * no request ended in the reader's foreground pump; Test 73 pumps on those
 * the reader has work for and otherwise one in four milliseconds. What
 * the game is served must not change, and its data must still be ready
 * when it asks. */
static void polling(uint32_t mean, unsigned dense) {
    open_movie(2000u, false, true);
    stall_mean = mean; stall_dense = dense;
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    reader_pace(70u);
    m.delay = 100000u;
    run_owed = 0u;
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, 0u);
    lba += REQUEST;
    unsigned requests = 0, ready = 0;
    unsigned long idle = 0;
    const uint32_t first_pumps = trace[4];
    uint32_t idle_pumps = 0;
    while(lba + REQUEST <= WINDOW_END) {
        for(unsigned moment = 0u; moment < 25u; ++moment) {
            /* Two calls to a moment of 1.3 ms, with nothing pending. */
            for(unsigned i = 0; i < 2u; ++i, ++idle) {
                const uint32_t pumped = trace[4];
                game_poll();
                idle_pumps += trace[4] - pumped;
            }
            (void)hardware_at(2048u);
        }
        if(kui_native_readahead_aux_context.movie_ahead.variant &&
           kui_native_readahead_aux_context.movie_ahead.first == lba &&
           kui_native_readahead_aux_context.movie_ahead.verified - lba >= REQUEST) ++ready;
        read_request(lba, REQUEST, 0u);
        ++requests;
        lba += REQUEST;
    }
    reader_pace(0u);
    m.delay = 0u;
    /* The window was never given up for these calls, and served them all. */
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == requests + 1u);
    CHECK(!w->first_error && !w->declined && !w->stops[KUI_TOY_WIRE_STOP_FAULT]);
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
    visits_accounted();
#if KUI_TOY_WIRE_IDLE
    /* Every such call is seen, and so is the call or two behind each
     * request that finds it served. Those that pump are the ones counted
     * as work or as due, and they are few. The update calls' counts are
     * not touched. */
    const uint32_t tails = w->idles - (uint32_t)idle, counted = w->idle_work + w->idle_due;
    CHECK(w->idles >= idle && tails <= 2u * (requests + 1u));
    CHECK(idle_pumps <= counted && counted - idle_pumps <= tails && w->idle_due);
    CHECK(idle_pumps * 4u <= idle && !w->updates && !w->update_work && !w->update_due);
    /* Each of the game's calls is one delivery visit, pumped or not; an
     * interrupt's visit is not one of them. */
#if KUI_TOY_WIRE_QUICK
    /* Test 74: a call that finds nothing to decide does not take the whole
     * path, and only those that do are timed as delivery visits. The
     * idle calls are counted where their rule is applied, as before. */
    CHECK(w->deliver_calls >= requests && w->deliver_ticks && w->deliver_calls <= 4u * (requests + 1u));
    CHECK(w->deliver_calls + w->idles >= idle + requests);
#else
    CHECK(w->deliver_calls >= idle + requests && w->deliver_ticks);
    CHECK(w->deliver_calls <= polls + 4u * (requests + 1u));
#endif
#if KUI_TOY_WIRE_LEAN && !KUI_TOY_WIRE_FLUID
    /* Each of the game's EXEC calls behind the first movie request is
     * timed whole, from the resident's entry visit to its exit visit. */
    CHECK(w->gd_calls >= idle && w->gd_calls <= polls + 4u * (requests + 1u) && w->gd_ticks >= w->gd_calls);
    /* An exit visit with no entry visit before it is not a call. */
    const uint32_t timed = w->gd_calls;
    CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_DEMAND_GD_LEAVE, KUI_GD_EXEC, &R, 0u) == 0u);
    CHECK(w->gd_calls == timed);
#endif
    if(!dense) CHECK(ready + 2u * (w->n.starts - 1u) >= requests);
    if(!mean) CHECK(w->idle_work);
    printf("  the game's idle GD calls, stall every ~%u bytes, %u%% busy phases: %lu calls, %u pumped for work and %u as due; "
           "all foreground pumps %u; data ready for %u of %u frames when asked; fulls %u, streams %u\n",
        (unsigned)mean, dense, idle, (unsigned)w->idle_work, (unsigned)w->idle_due,
        (unsigned)(trace[4] - first_pumps), ready, requests, (unsigned)w->n.fulls, (unsigned)w->n.starts);
#else
    CHECK(idle_pumps == idle);
    printf("  the game's idle GD calls, stall every ~%u bytes, %u%% busy phases: %lu calls, each one a pump; "
           "all foreground pumps %u; data ready for %u of %u frames when asked; fulls %u, streams %u\n",
        (unsigned)mean, dense, idle, (unsigned)(trace[4] - first_pumps), ready, requests,
        (unsigned)w->n.fulls, (unsigned)w->n.starts);
#endif
}

#if KUI_TOY_WIRE_IDLE
/* A request the window cannot serve yet is not an idle call: every call of
 * the game's that waits for it pumps, as before. */
static void waiting(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    read_request(FIRST, REQUEST, 3u);
    /* Runs stay in flight: nothing more arrives unless the wire is given
     * its time, so a request far ahead of the window stays pending. */
    reader_pace(70u);
    m.delay = 100000u;
    uint32_t lba = FIRST + REQUEST;
    for(unsigned i = 0; i < 4u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(lba, REQUEST, OUTPUT);
    unsigned waited = 0;
    while(R.shared.service.pending && waited < 6u) {
        const uint32_t pumped = trace[4], idles = w->idles;
        game_poll();
        if(!R.shared.service.pending) break;
        CHECK(trace[4] == pumped + 1u && w->idles == idles);
        ++waited;
    }
    CHECK(waited >= 3u);
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 200000u && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    compare(lba, REQUEST, OUTPUT);
    reader_pace(0u);
    m.delay = 0u;
    invariants();
    printf("  a request the window cannot serve yet: every call that waits for it pumps\n");
}

/* What Test 73 records of the player itself, from its own counters and its
 * own memory as the hook finds them when an update call has returned. */
/* The player's buffer and lag as Test 73 samples them; Test 74 has given
 * that sampling up for room and must leave its fields alone. */
#if KUI_TOY_WIRE_FLUID
/* Test 76 samples the buffer again, by a rule of its own: buffer_seen(). */
#define SAMPLED(x) (void)0
#elif KUI_TOY_WIRE_LEAN
#define SAMPLED(x) CHECK(!w->update_ticks && !w->buffer_capacity && !w->course_free[0] && !w->course_lag[0])
#else
#define SAMPLED(x) CHECK(x)
#endif
static void player_measured(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_toy_wire_player *intro = &w->player[1], *bumper = &w->player[0];
    /* The reader at the console's pace, and runs that land when their
     * time has passed: interrupt visits come, and take time. `step` is
     * what one look at the clock costs the reader in this fixture. */
    reader_pace(70u);
    m.delay = 100000u;
    const uint32_t step = refill_clock_extra_ticks;
    read_request(FIRST, REQUEST, 3u);
    player.lag = 3u;
    /* A movie's first call only takes the counters as they stand. */
    player.out = 7u; player.skip = 2u;
    player_update_for(100u);
    CHECK(!intro->out && !intro->skip);
    SAMPLED(w->update_ticks == 100u + step);
    SAMPLED(w->buffer_capacity == 0x100000u && w->course_free[0] == 0x40000u && w->course_lag[0] == 3u);
    /* A frame put out in an update of 20,000 ticks, after a request and
     * its interrupt visits: the update's own time, the time since the
     * update before returned, and the reader's interrupt time in between. */
    const uint32_t before = w->visit_ticks[KUI_TOY_WIRE_VISIT_SCI] + w->visit_ticks[KUI_TOY_WIRE_VISIT_TIMER];
    read_request(FIRST + REQUEST, REQUEST, 3u);
    read_request(FIRST + 2u * REQUEST, REQUEST, 3u);
    const uint32_t between = w->visit_ticks[KUI_TOY_WIRE_VISIT_SCI] + w->visit_ticks[KUI_TOY_WIRE_VISIT_TIMER] - before;
    CHECK(between);
    ++player.out;
    player_update_for(20000u);
    CHECK(intro->out == 1u && intro->out_ticks == 20000u + step && !intro->skip);
    CHECK(intro->out_cycle > intro->out_ticks + between && intro->out_interrupt == between);
    const uint32_t cycle = intro->out_cycle;
#if KUI_TOY_WIRE_STEADY
    /* No skip yet, whatever frame the player stands at. */
    CHECK(!w->skip_first);
    player.frame = 41u;
#endif
    /* A frame skipped in a short one. */
    ++player.skip;
    player_update_for(300u);
    CHECK(intro->skip == 1u && intro->skip_ticks == 300u + step && intro->out == 1u);
#if KUI_TOY_WIRE_STEADY
    /* The second movie's first skip, by the player's own frame count. */
    CHECK(w->skip_first == 42u);
    player.frame = 77u;
#endif
    /* Neither: a call that only waited. */
    player_update_for(50u);
    CHECK(intro->out == 1u && intro->skip == 1u);
    /* Both counters moved: the call put a frame out. */
    ++player.out; ++player.skip;
    player_update_for(700u);
    CHECK(intro->out == 2u && intro->out_ticks == 20700u + 2u * step && intro->skip == 1u);
    CHECK(intro->out_interrupt == between && intro->skip_ticks == 300u + step);
#if KUI_TOY_WIRE_STEADY
    /* A later skip is not the first. */
    CHECK(w->skip_first == 42u);
#endif
    /* From the call before: its own 700 ticks and the few looks at the
     * clock since. */
    CHECK(intro->out_cycle - cycle >= 700u && intro->out_cycle - cycle < 900u);
    SAMPLED(w->update_ticks == 100u + 20000u + 300u + 50u + 700u + 5u * step);
    /* The buffer's emptiest moment in the piece is kept, and the lag's
     * highest; a fuller buffer and a lower lag afterwards change nothing. */
    player.filled -= 0x80000u; player.lag = 9u;
    player_update_for(10u);
    SAMPLED(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
    player.filled += 0xa0000u; player.lag = 2u;
    player_update_for(10u);
    SAMPLED(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
    /* A buffer that cannot be one is left out: no capacity, more in it
     * than it holds, a pointer outside the game's memory. */
#if !KUI_TOY_WIRE_FLUID
    const uint32_t ticks = w->update_ticks;
#endif
    player.capacity = 0u; player.filled = player.released;
    player_update_for(10u);
    player.capacity = 0x100000u; player.filled = player.released + 0x100001u;
    player_update_for(10u);
    player.filled = player.released; player.stream = UINT32_C(0x8c000004);
    player_update_for(10u);
    SAMPLED(w->course_free[0] == 0xc0000u && w->buffer_capacity == 0x100000u);
    SAMPLED(w->update_ticks == ticks + 3u * (10u + step));
    /* Later pieces of the course are their own. */
    player.stream = PLAYER_STREAM; player.filled = player.released + 0xf0000u; player.lag = 1u;
    clock_counter -= UINT32_C(3) << KUI_TOY_WIRE_BUCKET_SHIFT;
    player_update_for(10u);
    SAMPLED(w->course_free[3] == 0x10000u && w->course_lag[3] == 1u && !w->course_free[1] && !w->course_lag[2]);
    SAMPLED(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
    /* Nothing of the reader's own was disturbed: only the game's memory
     * was read. */
    CHECK(intro->out == 2u && intro->skip == 1u && !bumper->out && !bumper->skip);
    invariants();

    /* The first movie's player is counted apart, and has no course. */
    kui_toy_movie_present_boundary(PLAYER,
        (uintptr_t)&kui_native_readahead_aux_context.movie_exit[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    kui_native_readahead_aux_context.movie_exit[0].start_status = 0x07dbu;
    kui_toy_movie_present_boundary(PLAYER,
        (uintptr_t)&kui_native_readahead_aux_context.movie_exit[0], KUI_TOY_MOVIE_EXIT_STARTED);
#if !KUI_TOY_WIRE_FLUID
    const uint32_t all = w->update_ticks;
#endif
    player.filled = player.released; player.lag = 50u;
    ++player.out;
    player_update_for(900u); /* its first call: the counters as they stand */
    CHECK(!bumper->out && !bumper->skip);
    ++player.out;
    player_update_for(15000u);
    ++player.skip;
    player_update_for(40u);
    CHECK(bumper->out == 1u && bumper->out_ticks == 15000u + step && bumper->skip == 1u && bumper->skip_ticks == 40u + step);
#if KUI_TOY_WIRE_STEADY
    /* The first movie's skips are not the second's. */
    CHECK(w->skip_first == 42u);
#endif
#if KUI_TOY_WIRE_FLUID
    CHECK(intro->out == 2u && intro->skip == 1u);
#else
    CHECK(intro->out == 2u && intro->skip == 1u && w->update_ticks == all);
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) SAMPLED(w->course_free[i] <= 0xc0000u && w->course_lag[i] <= 9u);
    (void)ticks;
#endif
    reader_pace(0u);
    m.delay = 0u;
    printf("  the player itself: frames put out and skipped with their update's time, the reader's interrupt time "
           "around them%s; each movie apart\n", KUI_TOY_WIRE_LEAN ? "" : ", its buffer's emptiest and its lag's highest in each piece");
}
#undef SAMPLED
#if KUI_TOY_WIRE_STEADY
/* The first movie's player skips before the second movie's ever has: its
 * skips never stand for the second movie's first one. */
static void first_skip_apart(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    struct kui_toy_movie_exit_witness *exits = kui_native_readahead_aux_context.movie_exit;
    read_request(FIRST, REQUEST, 3u);
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    exits[0].start_status = 0x07dbu;
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[0], KUI_TOY_MOVIE_EXIT_STARTED);
    player.frame = 9u;
    player_update_for(100u); /* a movie's first call only takes the counters */
    ++player.skip;
    player_update_for(100u);
    CHECK(w->player[0].skip == 1u && !w->player[1].skip && !w->skip_first);
    /* The second movie begins. Its first call counts nothing, and its
     * first skip is told by its own frame count. */
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[0], KUI_TOY_MOVIE_EXIT_TERMINAL);
    exits[1].start_status = 0x07dbu;
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[1], KUI_TOY_MOVIE_EXIT_STARTED);
    player.frame = 30u;
    ++player.skip;
    player_update_for(100u);
    CHECK(!w->skip_first && !w->player[1].skip);
    ++player.skip;
    player_update_for(100u);
    CHECK(w->skip_first == 31u && w->player[1].skip == 1u && w->player[0].skip == 1u);
    printf("  the second movie's first skip: its own frame, not a skip of the first movie's\n");
}
#endif
#if KUI_TOY_WIRE_FLUID
#if !KUI_TOY_WIRE_ANY
/* Test 76: what the player's own buffer held, by piece of the second
 * movie's course. The most of it that was free is kept, and it is looked
 * at only in a call in which the player took a frame: shown or skipped. */
static void buffer_seen(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    struct kui_toy_movie_exit_witness *exits = kui_native_readahead_aux_context.movie_exit;
    read_request(FIRST, REQUEST, 3u);
    /* The fixture's player begins with a quarter of its megabyte free,
     * and its counters stand somewhere: a movie's first call only takes
     * them as they are, and looks at nothing. */
    player.out = 5u; player.skip = 1u;
    player_update_for(100u);
    CHECK(!w->buffer_capacity && !w->course_free[0]);
    /* A call that took no frame looks at nothing, however empty the buffer. */
    player.filled -= 0x80000u;
    player_update_for(100u);
    CHECK(!w->buffer_capacity && !w->course_free[0]);
    player.filled += 0x80000u;
    /* A frame shown: a quarter free. */
    ++player.out;
    player_update_for(100u);
    CHECK(w->buffer_capacity == 0x100000u && w->course_free[0] == 0x40000u);
    /* A frame skipped with the buffer emptier: three quarters free. */
    player.filled -= 0x80000u;
    ++player.skip;
    player_update_for(100u);
    CHECK(w->course_free[0] == 0xc0000u);
    /* A fuller buffer afterwards changes nothing: the piece keeps its emptiest. */
    player.filled += 0xa0000u;
    ++player.out;
    player_update_for(100u);
    CHECK(w->course_free[0] == 0xc0000u);
    /* A buffer that cannot be one is left out: no capacity, more in it
     * than it holds, a pointer outside the game's memory. */
    player.capacity = 0u; player.filled = player.released;
    ++player.out;
    player_update_for(100u);
    player.capacity = 0x100000u; player.filled = player.released + 0x100001u;
    ++player.out;
    player_update_for(100u);
    player.filled = player.released; player.stream = UINT32_C(0x8c000004);
    ++player.out;
    player_update_for(100u);
    CHECK(w->course_free[0] == 0xc0000u && w->buffer_capacity == 0x100000u);
    /* Later pieces of the course are their own. */
    player.stream = PLAYER_STREAM; player.filled = player.released + 0xf0000u;
    clock_counter -= UINT32_C(3) << KUI_TOY_WIRE_BUCKET_SHIFT;
    ++player.out;
    player_update_for(100u);
    CHECK(w->course_free[3] == 0x10000u && !w->course_free[1] && !w->course_free[2] && w->course_free[0] == 0xc0000u);
    /* The first movie has no course: its player's buffer, of another
     * size here, is not looked at. */
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    exits[0].start_status = 0x07dbu;
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[0], KUI_TOY_MOVIE_EXIT_STARTED);
    player.capacity = 0x80000u; player.filled = player.released;
    player_update_for(100u);
    ++player.out;
    player_update_for(100u);
    CHECK(w->buffer_capacity == 0x100000u);
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) CHECK(w->course_free[i] <= 0xc0000u);
    invariants();
    printf("  the player's buffer: its emptiest in each piece, looked at when the player took a frame\n");
}
#endif

#if KUI_TOY_WIRE_QUIT
/* Test 76 (QUIT) ends a stream that receives by taking the channel off the
 * receiver. A channel that is no longer the stream's is not its to take
 * off: the stop finds it so, touches nothing of it, and says whose it is.
 * Twice: the channel's source changed, and its source the receiver still
 * but its destination outside the run. */
static void quit_foreign(bool elsewhere) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_wire_stream *ws = wire_state();
    /* A run in flight. */
    m.delay = 100000u;
    read_request(FIRST, REQUEST, 0u);
    for(unsigned n = 0u; n < 100000u && !m.rx; ++n) if(!hardware()) game_poll();
    CHECK(m.rx && stream_on() && ws->state == KUI_WIRE_RUN && (m.chcr & 1u));
    const uint32_t source = m.sar, destination = m.dar;
    if(elsewhere) m.dar = RING_PHYSICAL + ((ws->seg_begin + ws->seg_len + 4096u) & (RING_BYTES - 1u));
    else m.sar = UINT32_C(0x11111111);
    const uint32_t control = m.chcr;
#if KUI_TOY_WIRE_SEQ
    /* A command that is not a read: the window is given up and its stream
     * stopped. (Test 77's window does not end with the movie.) */
    R.shared.service.command = GONE_BY_COMMAND;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
    R.shared.service.command = 0u;
#else
    /* The movie is over: the window is given up and its stream stopped. */
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
#endif
    CHECK(!stream_on() && w->first_error && R.shared.port.fault);
    /* The channel is as its owner had it: enabled, and its end flag its own. */
    CHECK((m.chcr & ~2u) == (control & ~2u) && (m.chcr & 1u));
    m.sar = source; m.dar = destination;
    m.delay = 0u;
    R.shared.port.fault = false;
    printf("  a stream stopped with its channel %s: the channel left as it was, the fault reported\n",
        elsewhere ? "pointed outside the run" : "taken by another source");
}
#endif
#if KUI_TOY_WIRE_COPY
/* A movie whose track begins two bytes into its file's first block: no
 * piece of a block then lies at a multiple of four in both the block and
 * its sector. The hand-written copy would stop the processor on such a
 * piece; every one of them is copied as before, and served right. */
static void misaligned(void) {
    track_file_offset = 2u;
    open_movie(2000u, false, true);
    track_file_offset = 0u;
    const unsigned long before = copies;
    uint32_t lba = FIRST;
    for(unsigned i = 0; i < 6u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == 6u && copies == before);
    invariants();
    printf("  a track that begins two bytes into its block: six requests served from the window, "
           "none of their pieces given to the word copy\n");
}
#endif
#if !KUI_TOY_WIRE_ANY
/* How long the wire stood still for the stream's timer waits, by piece of
 * the second movie's course: what the stream's own count gains while that
 * movie plays, to the tick, and nothing of what it gains otherwise. */
static uint32_t pause_placed_sum(const struct kui_toy_wire_report *w) {
    uint32_t sum = 0;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) sum += w->course_pause[i];
    return sum;
}
static void pause_placed(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    struct kui_toy_movie_exit_witness *exits = kui_native_readahead_aux_context.movie_exit;
    /* A busy bus: runs cut short and answered with timer waits. */
    stall_mean = 150u; stall_dense = 80u;
    uint32_t lba = FIRST;
    for(unsigned i = 0; i < 3u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(w->n.waits && w->n.wait_ticks && pause_placed_sum(w) == w->n.wait_ticks);
    /* The second movie's presentation is over for a while (the window
     * lives by the movie's own witness and goes on): what the wire waits
     * now is in no piece. */
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    const uint32_t placed = pause_placed_sum(w), before = w->n.wait_ticks;
    for(unsigned i = 0; i < 3u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    const uint32_t outside = w->n.wait_ticks - before;
    CHECK(outside && pause_placed_sum(w) == placed);
    /* It plays again: the course counts from here, without what was
     * waited in between. */
    kui_toy_movie_present_boundary(PLAYER, (uintptr_t)&exits[1], KUI_TOY_MOVIE_EXIT_STARTED);
    for(unsigned i = 0; i < 3u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(w->n.wait_ticks > before + outside && pause_placed_sum(w) == w->n.wait_ticks - outside);
    stall_mean = 0u; stall_dense = 0u;
    invariants();
    printf("  the wire's standstill by piece: %u ticks placed while the second movie played, %u outside it left out\n",
        (unsigned)pause_placed_sum(w), (unsigned)outside);
}
#endif
#endif
#endif

/* The wire runs on while the game holds interrupts off: a run lands, a
 * timer runs out, and nobody is told. */
static void wire_unheard(void) {
    unsigned channel;
    if(!stream_on()) return;
    if(m.rx) {
        elapse();
        (void)kui_sci_stream_test_read(SSR, 1u);
    } else if(timer_running(&channel)) wx.tcr[channel] |= 0x0100u;
}
/* One request waited for as the game's own loop does it (its caller at
 * 0x8c09712a): EXEC and CHECK in turn with interrupts held off, nothing
 * else of the game running, until the request is complete. The rounds. */
static unsigned waited_request(uint32_t lba, uint32_t count, bool wire) {
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(lba, count, OUTPUT);
    invariants();
    unsigned rounds = 0u;
    for(;; ++rounds) {
        CHECK(rounds < 200000u && !R.engine.failed);
        if(wire) wire_unheard();
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        invariants();
        if(!R.shared.service.pending) break;
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_blocking_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == count * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(lba, count, OUTPUT);
    invariants();
    return rounds;
}
/* Requests the game waits for in a loop of its own. Up to Test 74 the
 * first such call gives the window up and the low reader reads the
 * request; Test 75 keeps the window and the stream serves it, with no
 * interrupt getting through at any time. */
static void waited_for(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t *trace = c->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    /* A run lands when the wire has brought it, not when a call looks. */
    m.delay = 100000u;
    enum { WAITED = 8u };
    uint32_t lba = FIRST;
    unsigned rounds = 0u;
    for(unsigned i = 0u; i < WAITED; ++i, lba += REQUEST) rounds += waited_request(lba, REQUEST, true);
    const unsigned long heard = eris + timers;
    CHECK(!heard);
#if KUI_TOY_WIRE_STAY
    /* One window, one stream, one card command for all of them. The first
     * request opens the window and waits for the stream to begin; later
     * ones find theirs read ahead or wait for the wire, never for the low
     * reader. */
    CHECK(stream_on() && c->movie_ahead.variant && trace[3] == 1u && card.cmd18 == 1u && !card.cmd12);
    CHECK(c->stats.hit_requests == WAITED && !c->stats.abandoned);
    for(unsigned i = 0u; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
    CHECK(w->stays >= 1u && w->stays <= WAITED);
    printf("  %u requests the game waited for with interrupts held off: one window, one stream, %u of them kept "
           "waiting for the wire, %u rounds of the game's loop\n", (unsigned)WAITED, (unsigned)w->stays, rounds);
#else
    /* Every request opens a window with its first call and gives it up
     * with its second; the low reader reads them all. */
    CHECK(w->drops[KUI_TOY_WIRE_DROP_WAITING] == WAITED && trace[3] == WAITED && !c->stats.hit_requests);
    CHECK(!c->movie_ahead.variant && !stream_on());
    printf("  %u requests the game waited for with interrupts held off: %u windows opened and given up, the low "
           "reader read them all, %u rounds of the game's loop\n", (unsigned)WAITED,
        (unsigned)w->drops[KUI_TOY_WIRE_DROP_WAITING], rounds);
#endif
    m.delay = 0u;
}
#if KUI_TOY_WIRE_STAY
/* A wait the stream does not end: the wire brings nothing more. After a
 * quarter of a second without a sector the window is given up as it always
 * was, and the low reader reads the request. */
static void waited_in_vain(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u;
    (void)waited_request(FIRST, REQUEST, true);
    CHECK(stream_on() && c->movie_ahead.variant && c->stats.hit_requests == 1u);
    /* The window serves what it already holds; then a request waits, and
     * the run in flight never lands. */
    uint32_t next = FIRST + REQUEST;
    while(c->movie_ahead.verified - c->movie_ahead.first >= REQUEST) {
        (void)waited_request(next, REQUEST, false);
        next += REQUEST;
    }
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(next, REQUEST, OUTPUT);
    const uint32_t stays = w->stays;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0); /* its first call */
    for(unsigned n = 0u; n < 50u; ++n) {
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        CHECK(R.shared.service.pending && stream_on() && c->movie_ahead.variant);
    }
    CHECK(w->stays == stays + 1u && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    /* Just short of the limit it is still kept; with the limit passed, the
     * very next call gives the window up. All the calls so far have not
     * taken a tenth of it. */
    clock_counter -= 195312u - 19000u;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
    CHECK(R.shared.service.pending && stream_on() && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    clock_counter -= 19000u;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
    CHECK(w->drops[KUI_TOY_WIRE_DROP_WAITING] == 1u && !stream_on() && !c->movie_ahead.variant);
    unsigned rounds = 0u;
    for(; R.shared.service.pending; ++rounds) {
        CHECK(rounds < 200000u && !R.engine.failed);
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        invariants();
    }
    CHECK(w->drops[KUI_TOY_WIRE_DROP_WAITING] == 1u && w->stays == stays + 1u);
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_blocking_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == REQUEST * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(next, REQUEST, OUTPUT);
    invariants();
    m.delay = 0u;
    printf("  a request waited for in vain: kept for a quarter of a second, then the window given up and the "
           "low reader read it\n");
}
/* A wait the stream is slow to end, but under which the window grows: it
 * is kept for as long as sectors come, and what has been read is not
 * thrown away. Only a quarter of a second without one gives it up. */
static void waited_slowly(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u;
    (void)waited_request(FIRST, REQUEST, true);
    uint32_t next = FIRST + REQUEST;
    while(c->movie_ahead.verified - c->movie_ahead.first >= REQUEST) {
        (void)waited_request(next, REQUEST, false);
        next += REQUEST;
    }
    /* A request of two frames' sectors: several runs of the wire. */
    enum { LARGE = 2u * REQUEST };
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(next, LARGE, OUTPUT);
    const uint32_t stays = w->stays;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0); /* its first call */
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0); /* the wait begins */
    CHECK(R.shared.service.pending && w->stays == stays + 1u);
    for(unsigned round = 0u; round < 3u; ++round) {
        /* Nearly a quarter of a second with nothing new: still kept. */
        clock_counter -= 180000u;
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        CHECK(R.shared.service.pending && stream_on() && c->movie_ahead.variant);
        /* Then the wire brings a run: the window grows, the request is
         * not complete, and the quarter second begins again. */
        const uint32_t verified = c->movie_ahead.verified;
        wire_unheard();
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        CHECK(c->movie_ahead.verified > verified && R.shared.service.pending);
    }
    CHECK(w->stays == stays + 1u && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    /* More than half a second in all by now. With nothing more coming,
     * the limit counts from the game's first call that finds the window
     * grown: the next one of its loop, here at once. */
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
    CHECK(R.shared.service.pending);
    clock_counter -= 195312u - 19000u;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
    CHECK(R.shared.service.pending && stream_on() && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    clock_counter -= 19000u;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
    CHECK(w->drops[KUI_TOY_WIRE_DROP_WAITING] == 1u && !stream_on() && !c->movie_ahead.variant);
    for(unsigned rounds = 0u; R.shared.service.pending; ++rounds) {
        CHECK(rounds < 200000u && !R.engine.failed);
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        invariants();
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_blocking_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == LARGE * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(next, LARGE, OUTPUT);
    invariants();
    m.delay = 0u;
    printf("  a request waited for while the wire brings a run now and then: kept through three quarter seconds, "
           "given up a quarter second after the last sector\n");
}
/* What a wait must not keep: a window that does not stand at the request's
 * first sector, and any window while the clock that bounds the wait is not
 * the expected one. Both are given up as they always were. */
static void waited_elsewhere(bool clock) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u;
    (void)waited_request(FIRST, REQUEST, true);
    CHECK(stream_on() && c->movie_ahead.variant && c->stats.hit_requests == 1u);
    uint32_t next = FIRST + REQUEST;
    while(c->movie_ahead.verified - c->movie_ahead.first >= REQUEST) {
        (void)waited_request(next, REQUEST, false);
        next += REQUEST;
    }
    const uint32_t stays = w->stays, hits = c->stats.hit_requests;
    if(clock) next += REQUEST; /* the game skips a request's worth */
    else refill_clock_valid = false;
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(next, REQUEST, OUTPUT);
    unsigned rounds = 0u;
    for(; R.shared.service.pending; ++rounds) {
        CHECK(rounds < 200000u && !R.engine.failed);
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_blocking_poll) == 0);
        invariants();
    }
    refill_clock_valid = true;
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_blocking_poll) == KUI_GD_COMPLETED);
    compare(next, REQUEST, OUTPUT);
    /* Not kept, not counted as kept, and read by the low reader. */
    CHECK(w->stays == stays && c->stats.hit_requests == hits);
    CHECK(w->drops[clock ? KUI_TOY_WIRE_DROP_OTHER : KUI_TOY_WIRE_DROP_WAITING] == 1u);
    CHECK(w->drops[clock ? KUI_TOY_WIRE_DROP_WAITING : KUI_TOY_WIRE_DROP_OTHER] == 0u);
    invariants();
    m.delay = 0u;
    printf("  a request waited for %s: its window given up at once\n",
        clock ? "that is not the window's next" : "without the clock that bounds the wait");
}
#endif

#if KUI_TOY_WIRE_LATER
/* Test 75: an interrupt visit with no request waiting answers the stream
 * and leaves the reading for later, while the stream goes on and less than
 * half the ring is unread. Visit by visit: one that left read nothing and
 * left the stream receiving below that mark; one that read did so because
 * the mark was reached or the stream stood still. With a request waiting
 * no visit leaves. */
static void later_visits(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_wire_stream *ws = wire_state();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    /* A bus that cuts a run every two kilobytes or so, as the console's
     * does: several interrupts to a ring half full. */
    stall_mean = 2000u;
    reader_pace(70u);
    m.delay = 100000u;
    run_owed = 0u;
    read_request(FIRST, REQUEST, 0u);
    CHECK(stream_on() && ws->ring && ws->state != KUI_WIRE_OFF);
    unsigned left = 0u, read = 0u, most = 0u;
    for(unsigned n = 0u; n < 2000u; ++n) {
        const uint32_t blocks = w->n.blocks, later = w->later, visits = w->visit_calls[KUI_TOY_WIRE_VISIT_SCI];
        const uint32_t verified = c->movie_ahead.verified, service = w->service_ticks, waits = w->n.waits;
        const unsigned long before = eris;
        if(!hardware_at(2048u)) break;
        if(eris == before) continue; /* the wire ran on, or the stream's timer called */
        CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_SCI] == visits + 1u);
        const uint32_t unread = ws->head - ws->tail;
        if(w->later != later) {
            /* Left: the stream served, nothing read, the stream receiving.
             * Served once, and once more if that began a wait. */
            CHECK(w->later == later + 1u && w->n.blocks == blocks && c->movie_ahead.verified == verified);
            CHECK(ws->state == KUI_WIRE_RUN || ws->state == KUI_WIRE_WAIT);
            CHECK(unread < KUI_WIRE_RING_BYTES / 2u);
            CHECK(w->service_ticks == service + (w->n.waits != waits ? 2u : 1u) * PHASE_LOOK);
            CHECK((w->n.waits != waits) == (ws->state == KUI_WIRE_WAIT));
            if(unread > most) most = unread;
            if(++left == 2u) {
                /* What the interrupts left, a call of the game's reads:
                 * the next one that is due, four milliseconds on. */
                clock_counter -= 3200u;
                const uint32_t pumps = w->visit_calls[KUI_TOY_WIRE_VISIT_GAME];
                game_poll();
                CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_GAME] == pumps + 1u && w->n.blocks > blocks);
                CHECK(ws->head - ws->tail < unread && w->later == later + 1u);
            }
        } else {
            /* Read: blocks were taken, unless the window had no room. */
            CHECK(w->n.blocks > blocks || c->movie_ahead.verified - c->movie_ahead.first >= 61u);
            ++read;
        }
    }
    /* Both happened, and the ring came near its mark before a visit read. */
    CHECK(left >= 3u && read >= 1u && most >= KUI_WIRE_RING_BYTES / 4u && w->later == left);
    /* The window is full now and the stream stands still. The game takes
     * what the window holds; then a request waits, and while it does no
     * interrupt visit leaves the reading. */
    uint32_t lba = FIRST + REQUEST;
    unsigned waited = 0u;
    for(unsigned n = 0u; n < 12u && lba + REQUEST <= WINDOW_END; ++n, lba += REQUEST) {
        const bool ready = c->movie_ahead.first == lba && c->movie_ahead.verified - lba >= REQUEST;
        const uint32_t later = w->later;
        const unsigned long before = eris;
        memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
        const uint32_t token = (uint32_t)request(lba, REQUEST, OUTPUT);
        for(unsigned moment = 0u; R.shared.service.pending; ++moment) {
            CHECK(moment < 200000u && !R.engine.failed && !ready);
            /* Only interrupts: the game does not call while it decodes. */
            if(!hardware_at(2048u)) game_poll();
        }
        CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
        compare(lba, REQUEST, OUTPUT);
        if(!ready && eris != before) {
            CHECK(w->later == later);
            ++waited;
        }
    }
    CHECK(waited >= 1u);
    visits_accounted();
    reader_pace(0u);
    m.delay = 0u;
    printf("  interrupt visits with no request waiting: %u answered the stream and left the reading (the ring "
           "up to %u bytes unread), %u read; %u requests waited and no visit left meanwhile\n", left, most, read, waited);
}
/* A bus so busy that runs end after a few dozen bytes: the stream's log of
 * lost positions is half full long before half the ring is unread. The
 * interrupt visit that finds it so reads at once, and the stream never
 * stands still for a reader that was not coming. */
static void later_standstill(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_wire_stream *ws = wire_state();
    stall_mean = 40u; stall_dense = 100u;
    reader_pace(70u);
    m.delay = 100000u;
    run_owed = 0u;
    read_request(FIRST, REQUEST, 0u);
    unsigned left = 0u, pressed = 0u, most = 0u;
    for(unsigned n = 0u; n < 20000u && !pressed; ++n) {
        const uint32_t blocks = w->n.blocks, later = w->later, unread = ws->head - ws->tail;
        const unsigned long before = eris;
        if(!hardware_at(2048u)) break;
        if(eris == before) continue;
        const uint32_t logged = ws->hole_put - ws->hole_get;
        if(w->later != later) {
            CHECK(ws->state == KUI_WIRE_RUN || ws->state == KUI_WIRE_WAIT);
            CHECK(logged < KUI_WIRE_HOLES / 2u && ws->head - ws->tail < KUI_WIRE_RING_BYTES / 2u);
            if(logged > most) most = logged;
            ++left;
        } else if(w->n.blocks > blocks && unread + KUI_WIRE_SEGMENT_MAX + 2u < KUI_WIRE_RING_BYTES / 2u) {
            /* Read with the ring far from its mark: for the log. */
            ++pressed;
        }
    }
    /* Up to the mark every visit left the reading; the one at the mark
     * read. (So busy a bus leaves few sectors whole: what becomes of the
     * stream afterwards is the other fixtures' matter.) */
    CHECK(left == KUI_WIRE_HOLES / 2u - 1u && pressed == 1u && most == KUI_WIRE_HOLES / 2u - 1u);
    CHECK(!w->n.fulls && !w->first_error);
    reader_pace(0u);
    m.delay = 0u;
    printf("  a bus that cuts every run after a few bytes: %u interrupt visits left the reading, and the one that "
           "found the log of lost positions half full read, the ring far from half unread; the stream never "
           "stood still\n", left);
}
/* A wait that begins in a visit which then leaves must not be left to
 * itself. Without a timer nothing else would end it: the visit ends it.
 * With one, the SDK may have taken the timer's interrupt level away since
 * the stream began: the visit gives it back, or the wait's end would never
 * be told. Interrupts only, no call of the game's, as while it decodes. */
static void later_waits(bool timer) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_wire_stream *ws = wire_state();
    /* Short runs: every cut is answered with a wait. */
    stall_mean = 40u; stall_dense = 100u;
    if(!timer) wx.tstr = 7u; /* both spare timers run for the game */
    reader_pace(70u);
    m.delay = 100000u;
    run_owed = 0u;
    read_request(FIRST, REQUEST, 0u);
    CHECK(stream_on() && (timer ? ws->mode != 0u : ws->mode == 0u));
#if KUI_TOY_WIRE_TITLES
    if(!timer) {
        /* Test 79: without a timer the visit that begins a wait does not
         * end it, and with interrupts alone nothing does: the stream
         * stands, with no interrupt of its own, until a call of the
         * game's comes after the wait's time. */
        bool waited = false;
        for(unsigned n = 0u; n < 4000u && !waited; ++n) {
            const uint32_t waits = w->n.waits;
            if(!hardware_at(2048u)) break;
            waited = w->n.waits != waits && ws->state == KUI_WIRE_WAIT;
        }
        CHECK(waited && stream_on());
        const unsigned long heard = eris;
        for(unsigned n = 0u; n < 200u; ++n) (void)hardware_at(2048u);
        CHECK(ws->state == KUI_WIRE_WAIT && eris == heard && !timers);
        wx.tcnt0 -= 2u * KUI_WIRE_BACKOFF_MAX;
        game_poll();
        CHECK(ws->state != KUI_WIRE_WAIT && !w->first_error);
        reader_pace(0u);
        m.delay = 0u;
        printf("  a wait begun by a visit that leaves the reading, no timer: left standing by that visit and by "
               "every interrupt, ended by the game's next call after its time\n");
        return;
    }
#endif
    unsigned left = 0u, levels = 0u;
    for(unsigned n = 0u; n < 4000u && left < 40u; ++n) {
        const uint32_t later = w->later, waits = w->n.waits;
        const unsigned long before = eris;
        if(timer && m.rx) {
            /* The SDK rewrites the priority register for its own devices
             * while a run is in flight. */
            wx.ipra &= (uint16_t)~(15u << ws->timer_shift);
            ++levels;
        }
        if(!hardware_at(2048u)) break;
        if(eris == before || w->later == later) continue;
        ++left;
        if(w->n.waits == waits) continue;
        /* This visit began a wait and left the reading. */
        if(!timer) CHECK(ws->state == KUI_WIRE_RUN);
        else CHECK(ws->state == KUI_WIRE_WAIT && ((wx.ipra >> ws->timer_shift) & 15u));
    }
    CHECK(left == 40u && w->n.waits >= 20u && !w->first_error && (timer ? levels && timers : !timers));
    reader_pace(0u);
    m.delay = 0u;
    printf("  a wait begun by a visit that leaves the reading, %s: %s\n", timer ? "the timer's level taken away" :
        "no timer", timer ? "the visit gives the level back and the timer ends the wait" : "ended by that visit");
}
/* A stream that is no longer in order is not left for later either: the
 * interrupt visit that finds the channel taken from it ends it there and
 * then, as every visit did before. */
static void later_fault(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_wire_stream *ws = wire_state();
    m.delay = 100000u;
    run_owed = 0u;
    read_request(FIRST, REQUEST, 0u);
    for(unsigned n = 0u; n < 100000u && !m.rx; ++n) if(!hardware()) game_poll();
    CHECK(m.rx && stream_on() && ws->state == KUI_WIRE_RUN && ws->head - ws->tail < KUI_WIRE_RING_BYTES / 2u);
    /* The run lands; before its interrupt is taken the channel's source
     * is no longer the receiver. */
    wire_unheard();
    CHECK((m.ssr & ORER) && m.scr == 0x50u);
    const uint32_t later = w->later, source = m.sar;
    m.sar = UINT32_C(0x11111111);
    CHECK(kui_retail_sdk_sci_dispatch(0x4e0u) == 0u);
    CHECK(w->later == later && w->stops[KUI_TOY_WIRE_STOP_FAULT] == 1u && !stream_on());
    CHECK(w->first_error == KUI_WIRE_E_FOREIGN);
    m.sar = source;
    m.delay = 0u;
    printf("  a stream whose channel was taken from it: ended by the interrupt visit that found it so\n");
}
#endif

/* An overrun whose interrupt does not come (held off by the game, or its
 * level taken by the SDK) is answered by the next update call, whatever
 * the spacing says. */
static void overdue(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    read_request(FIRST, REQUEST, 3u);
    /* Let the window fill and the reader come to rest, then make room. */
    idle(100000u);
    read_request(FIRST + REQUEST, REQUEST, 3u);
    read_request(FIRST + 2u * REQUEST, REQUEST, 3u);
    CHECK(stream_on());
    /* A run stays in flight until the wire has brought it; then it
     * reaches its end and nobody is told. */
    m.delay = 100000u;
    for(unsigned n = 0u; n < 100000u && !m.rx; ++n)
        if(!hardware()) player_update();
    CHECK(m.rx && stream_on());
    /* While it is arriving nothing is overdue: a call so soon after the
     * last one that read is not passed on. */
    unsigned long gate = gate_visits;
    uint32_t work = w->update_work, due = w->update_due;
    player_update();
    CHECK(gate_visits == gate && w->update_work == work && w->update_due == due);
    elapse();
    (void)kui_sci_stream_test_read(SSR, 1u);
    CHECK((m.ssr & ORER) && m.scr == 0x50u);
    player_update();
    CHECK(gate_visits == gate + 1u && w->update_work == work + 1u && w->update_due == due);
    CHECK(!(m.ssr & ORER));
    m.delay = 0u;
    invariants();
    printf("  an overrun whose interrupt does not come: answered by the next update call\n");
}
#endif

/* ---- what the sector's check must still refuse ----
 * Test 74 takes a sector on its blocks' CRC16 where it can and keeps the EDC
 * for the rest. Here the card sends blocks that are wrong in ways a real
 * stream needs a miscount or a CRC16 collision for, each with exactly one
 * byte lost on purpose so that the block arrives rebuilt or unchecked, and
 * placed so that only the rule under test stands between the wrong byte and
 * the game. In every build the game must get the image, after one sector
 * was refused and read again. */
static uint32_t raw_block(uint32_t lba, uint32_t at, unsigned *offset) {
    const struct kui_retail_track *track = &manifest.slots[2].track;
    const struct kui_retail_extent *extent = &manifest.slots[track->first_extent].extent;
    const uint32_t byte = (lba - track->start_lba) * KUI_GAME_RAW_BYTES + at;
    *offset = byte % 512u;
    return extent->card_lba + byte / 512u;
}
/* The CRC16 remainder of one changed byte of a block. */
static uint16_t crc_of_change(unsigned index, uint8_t change) {
    uint8_t zero[512] = {0};
    zero[index] = change;
    return crc16(zero, sizeof(zero));
}
enum { WRONG_REBUILD, WRONG_AHEAD, WRONG_BEHIND, WRONG_UNCHECKED, WRONG_ADDRESS, WRONG_KINDS };
/* Placements of WRONG_UNCHECKED in which the damaged block lost nothing but its CRC16 byte. */
static unsigned wrong_unchecked_alone;
static bool wrong_block(unsigned kind, unsigned shift) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    uint32_t lba = FIRST;
    read_request(lba, REQUEST, 3u);
    lba += REQUEST;
    /* Well ahead of what the stream has read. */
    const uint32_t sector = FIRST + 150u + shift;
    unsigned offset;
    uint32_t block = raw_block(sector + 1u, 0u, &offset);
    /* `offset` bytes of that block end `sector`, the rest begin the next. */
    switch(kind) {
    case WRONG_REBUILD:
        /* A block whole inside the sector, sent with the CRC16 of a block
         * that differs in the one byte then lost: the stream rebuilds that
         * byte wrong, in good faith. Its P code word must not confirm it. */
        block -= 2u;
        cut_index = 200u;
        card.crc_lba = block; card.crc_count = 1u; card.crc_xor = crc_of_change(cut_index, 0x5au);
        break;
    case WRONG_AHEAD:
        /* The block across the two sectors, stored wrong in a byte of the
         * first one's user data with a CRC16 that fits, and one byte lost in
         * the second: rebuilt right there, which says nothing for the first
         * sector, complete before that byte's code word can be asked. */
        if(offset < 300u || offset > 480u) return false;
        card.corrupt_custom = true; card.corrupt_index = 0u; card.corrupt_fits = true;
        card.corrupt_lba = block; card.corrupt_count = 1u;
        cut_index = offset + 20u;
        break;
    case WRONG_BEHIND:
        /* The same block stored wrong in a byte of the second sector, and
         * the byte lost in the first one's Q parity, which no P code word
         * covers: the second sector must not take the block as checked. */
        if(offset < 60u || offset > 470u) return false;
        card.corrupt_custom = true; card.corrupt_index = offset + 30u; card.corrupt_fits = true;
        card.corrupt_lba = block; card.corrupt_count = 1u;
        cut_index = offset - 50u;
        break;
    case WRONG_UNCHECKED:
        /* A block damaged on the wire whose first CRC byte is lost: its
         * CRC16 cannot be taken, and nothing but the EDC can refuse it. */
        block -= 2u;
        card.corrupt_lba = block; card.corrupt_count = 1u;
        cut_index = 512u;
        break;
    default:
        /* A sector that carries another address than its place, in blocks
         * whose CRC16 fits and with no byte lost: the stream finds nothing
         * wrong with it. Whichever way the sector is taken, its address is
         * looked at. */
        block = raw_block(sector, 14u, &offset);
        card.corrupt_custom = true; card.corrupt_index = offset; card.corrupt_fits = true;
        card.corrupt_lba = block; card.corrupt_count = 1u;
        block = 0u;
        break;
    }
    cut_lba = block;
    while(lba <= sector + 2u * REQUEST) {read_request(lba, REQUEST, 3u); lba += REQUEST;}
    CHECK(!cut_lba && !card.corrupt_count && !card.crc_count);
    /* A run's own end in the same block leaves it with two lost bytes: the
     * parity then fills both and nothing wrong ever arrives. Not this case. */
    if(w->n.holed > (kind == WRONG_UNCHECKED ? 1u : 0u)) return false;
    CHECK(w->check_bad == 1u && w->stops[KUI_TOY_WIRE_STOP_SECTOR] == 1u && !w->refused && !w->n.bad_crc);
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "wrong block kind %u shift %u: holed %u unchecked %u rebuilt %u unsolved %u bad %u\n",
        kind, shift, (unsigned)w->n.holed, (unsigned)w->n.unchecked, (unsigned)w->n.rebuilt, (unsigned)w->n.unsolved,
        (unsigned)w->check_bad);
    /* The damaged block is the one handed on unchecked (or, where a run
     * also ended in it, with a data byte for the parity as well). */
    CHECK(kind == WRONG_UNCHECKED ? w->n.holed == 1u && w->n.unchecked <= 1u : kind == WRONG_ADDRESS || w->n.rebuilt >= 1u);
    if(kind == WRONG_UNCHECKED && w->n.unchecked == 1u) ++wrong_unchecked_alone;
#if KUI_TOY_WIRE_CLEAN
    CHECK(w->mend_bad == (kind == WRONG_REBUILD ? 1u : 0u) && !w->light_bad && w->light > w->edc);
#endif
    card.corrupt_custom = card.corrupt_fits = false;
    return true;
}
static void wrong_blocks(void) {
    static const char *const names[WRONG_KINDS] = {
        "a lost byte rebuilt wrong from a CRC16 that fits other data",
        "a rebuilt block's wrong byte in the sector before its lost byte",
        "a rebuilt block's wrong byte in the sector behind its lost byte, lost where no P code word reaches",
        "a damaged block whose CRC16 was lost",
        "a sector with another address than its place, its blocks' CRC16 fitting"};
    wrong_unchecked_alone = 0u;
    for(unsigned kind = 0; kind < WRONG_KINDS; ++kind) {
        unsigned tried = 0;
        for(unsigned shift = 0; shift < 12u; ++shift) if(wrong_block(kind, shift)) ++tried;
        CHECK(tried >= 3u);
        /* The case as its name says it, nothing of the block lost but its CRC16 byte, is among them. */
        if(kind == WRONG_UNCHECKED) CHECK(wrong_unchecked_alone >= 3u);
        printf("  %s: refused by the sector's check in %u placements, the sector read again, data exact\n", names[kind], tried);
    }
}
#if KUI_TOY_WIRE_CLEAN
/* What the lighter check gives up, and its watch. A block stored wrong on
 * the card with a CRC16 that fits, no byte lost: every check of the stream
 * passes it, and only the EDC knows. Test 73 refused such a sector; Test 74
 * serves it as stored unless it is the one in sixteen passed through the EDC
 * as a sample, and then the lighter check is switched off for good. */
static void stored_wrong(void) {
    unsigned found = 0, served = 0;
    for(unsigned shift = 0; shift < 40u; ++shift) {
        open_movie(2000u, false, true);
        const struct kui_toy_wire_report *w = report();
        uint32_t lba = FIRST;
        read_request(lba, REQUEST, 3u);
        lba += REQUEST;
        const uint32_t sector = FIRST + 150u + shift;
        unsigned offset;
        const uint32_t block = raw_block(sector + 1u, 0u, &offset) - 2u;
        card.corrupt_fits = true;
        card.corrupt_lba = block; card.corrupt_count = 1u;
        light_bad_expected = 1u;
        unsigned differing = 0;
        uint32_t light = 0, edc = 0;
        bool tripped = false;
        while(lba <= sector + 4u * REQUEST) {
            memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
            const uint32_t token = (uint32_t)request(lba, REQUEST, OUTPUT);
            for(unsigned n = 0u; R.shared.service.pending; ++n) {
                CHECK(n < 200000u && !R.engine.failed);
                if(!hardware()) game_poll();
            }
            CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
            /* Which of the request's sectors differ from the image. */
            CHECK(kui_retail_image_read(&reference, lba, REQUEST, KUI_GAME_SECTOR_MODE1, expected,
                sizeof(expected)) == KUI_GAME_OK);
            for(uint32_t i = 0; i < REQUEST; ++i)
                if(memcmp(ram + (OUTPUT - BEGIN) + i * KUI_GAME_DATA_BYTES, expected + i * KUI_GAME_DATA_BYTES,
                    KUI_GAME_DATA_BYTES)) {++differing; CHECK(lba + i == sector);}
            lba += REQUEST;
            if(w->light_bad && !tripped) {tripped = true; light = w->light; edc = w->edc;}
        }
        card.corrupt_fits = false;
        CHECK(!card.corrupt_count);
        if(w->light_bad) {
            /* The sample: refused, read again, right; and from then on
             * every sector takes the EDC. */
            CHECK(w->light_bad == 1u && w->check_bad == 1u && !differing);
            CHECK(w->light == light && w->edc >= edc + REQUEST);
            ++found;
        } else if(differing) {
            CHECK(differing == 1u && !w->check_bad);
            ++served;
        } else CHECK(w->check_bad == 1u); /* the sector took the EDC for another reason */
        light_bad_expected = 0u;
    }
    CHECK(found >= 1u && found <= 4u && served >= 24u);
    printf("  a block stored wrong with a fitting CRC16, in 40 places: served as stored %u times, found by the one-in-sixteen "
           "EDC sample %u times, which then switched the lighter check off\n", served, found);
}
#endif

/* A whole block arrives damaged without a lost byte: its CRC16 stops the
 * stream, the low reader seeks again and the game still gets the image. */
static void damaged_block(void) {
    open_movie(2000u, false, true);
    read_request(FIRST, REQUEST, 3u);
    const struct kui_retail_extent *extent = &manifest.slots[manifest.slots[2].track.first_extent].extent;
    /* Far enough ahead that the stream has not read it yet. */
    card.corrupt_lba = extent->card_lba + ((FIRST + 150u - manifest.slots[2].track.start_lba) * 2352u) / 512u;
    card.corrupt_count = 1u;
    uint32_t lba = FIRST + REQUEST;
    for(unsigned i = 0; i < 14u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    const struct kui_toy_wire_report *w = report();
    CHECK(!card.corrupt_count && w->n.bad_crc == 1u && w->stops[KUI_TOY_WIRE_STOP_FRAME] == 1u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && w->n.starts == 2u && !w->check_bad);
#if KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_IDLE
    /* From the stream given up to its successor receiving. */
    CHECK(w->restart_ticks && w->restart_max == w->restart_ticks);
#endif
    visits_accounted();
    printf("  a block damaged on the wire: CRC16 refused it, one CMD12 and CMD18, data exact\n");
}

/* An image whose raw sectors carry no parity: the fill cannot work, every
 * sector that needs it fails its EDC and is read again by programmed
 * transfers. Slow, and still exact. */
static void no_parity(void) {
    open_movie(2000u, false, false);
    stall_mean = 300u; stall_dense = 30u;
    uint32_t lba = FIRST;
    for(unsigned i = 0; i < 8u; ++i, lba += REQUEST) read_request(lba, REQUEST, 2u);
    const struct kui_toy_wire_report *w = report();
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "no parity: parity %u filled %u bad %u refused %u sector stops %u seek %u\n",
        (unsigned)w->parity, (unsigned)w->filled, (unsigned)w->check_bad, (unsigned)w->refused,
        (unsigned)w->stops[KUI_TOY_WIRE_STOP_SECTOR], (unsigned)w->seek_blocks);
    /* A sector whose only losses lay in its parity bytes still passes. */
#if KUI_TOY_WIRE_CLEAN
    /* Test 74 does not ask once whether the image has parity: a rebuilt
     * byte no code word confirms goes to the EDC, which passes it here. */
    CHECK(!w->parity && w->check_bad >= 1u && w->stops[KUI_TOY_WIRE_STOP_SECTOR] >= 1u);
#else
    CHECK(w->parity == 2u && w->check_bad >= 1u && w->stops[KUI_TOY_WIRE_STOP_SECTOR] >= 1u);
#endif
    CHECK(w->check_bad + w->refused == w->stops[KUI_TOY_WIRE_STOP_SECTOR] && w->seek_blocks >= 5u);
    printf("  image without parity: %u sectors re-read by programmed transfers, data exact\n",
        (unsigned)w->stops[KUI_TOY_WIRE_STOP_SECTOR]);
#if KUI_TOY_WIRE_CLEAN
    /* The same image with stalls as rare as a console run's: single lost
     * bytes are rebuilt from the CRC16, no code word confirms them, and
     * the EDC passes every one, since they are right. Sectors without a
     * rebuilt byte are still taken the lighter way. */
    open_movie(2000u, false, false);
    stall_mean = 6000u;
    lba = FIRST;
    for(unsigned i = 0; i < 16u; ++i, lba += REQUEST) read_request(lba, REQUEST, 2u);
    w = report();
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "no parity, rare stalls: light %u edc %u mends %u mend_bad %u bad %u\n",
        (unsigned)w->light, (unsigned)w->edc, (unsigned)w->mends, (unsigned)w->mend_bad, (unsigned)w->check_bad);
    /* A code word of bytes that are no parity still sums to zero once in
     * 256 times, and a rebuilt sync byte is confirmed without one. */
    CHECK(w->mend_bad >= 8u && w->mend_bad > 8u * w->mends && w->light);
    /* What does fail is, as above, a sector with several lost bytes. */
    CHECK(w->check_bad + w->refused == w->stops[KUI_TOY_WIRE_STOP_SECTOR]);
    printf("  image without parity, rare stalls: %u rebuilt bytes no code word confirmed, each passed by the EDC\n",
        (unsigned)w->mend_bad);
#endif
}

/* The game reads something else while the stream runs: the window is given
 * up, the stream stops, the low reader ends the CMD18 and serves the read. */
static void other_read(void) {
    open_movie(2000u, false, true);
    stall_mean = 2000u;
    read_request(FIRST, REQUEST, 3u);
    CHECK(stream_on());
    const struct kui_toy_wire_report *w = report();
    const unsigned cmd12 = card.cmd12;
    read_and_compare(45295u, 2u, 0u, 1u);
    CHECK(!stream_on() && card.cmd12 == cmd12 + 1u && !kui_native_readahead_aux_context.movie_ahead.variant);
    CHECK(w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == 1u);
    closed();
#if KUI_TOY_WIRE_SEQ
    /* The movie goes on. Its next read does not begin where the read
     * before it ended, so it is the low reader's, by the movie path; the
     * one after that does, and has a new window and a new stream. */
    read_request(FIRST + REQUEST, REQUEST, 3u);
    CHECK(!stream_on() && !kui_native_readahead_aux_context.movie_ahead.variant &&
        kui_native_readahead_aux_context.stats.hit_requests == 1u);
    read_request(FIRST + 2u * REQUEST, REQUEST, 3u);
    CHECK(stream_on() && kui_native_readahead_aux_context.movie_ahead.variant == 4u);
    CHECK(w->n.lost_frame == (KUI_TOY_WIRE_MSB ? 1u : 0u) && w->n.starts == 2u + w->n.lost_frame);
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == 2u);
#else
    /* The movie goes on: a new window, a new stream. */
    read_request(FIRST + REQUEST, REQUEST, 3u);
    read_request(FIRST + 2u * REQUEST, REQUEST, 3u);
    /* This run's first stall falls on the first stream's first byte: with
     * the bit order still to be asked, that stream is given up undecided
     * and the next one asks again. */
    CHECK(w->n.lost_frame == (KUI_TOY_WIRE_MSB ? 1u : 0u) && w->n.starts == 2u + w->n.lost_frame);
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == 3u);
#endif
#if KUI_TOY_WIRE_MSB
    CHECK(w->n.undecided == 1u && w->n.order == (sdir_answer ? KUI_WIRE_ORDER_MSB : KUI_WIRE_ORDER_LSB));
#endif
    printf("  another read in the middle of the movie: stream stopped, CMD12 from the foreground, read exact\n");
}

#if KUI_TOY_WIRE_SEQ
/* ---- Test 77: the window opened by consecutive reads ---- */
/* For these the second movie's file lies in the middle of the track, a
 * hundred sectors of it, and no movie plays: every read is the game's
 * ordinary loading, by callers the reader knows nothing of. The track is
 * 45000 to 45299; the service's drive position after its start is 45000. */
enum { FILE_FIRST = 45101u, FILE_SECTORS = 100u, OTHER_WINDOW = 5u };
static const struct kui_toy_movie_range file_amid = {FILE_FIRST, FILE_SECTORS};
/* A read by a caller the reader does not know: the request, then CHECK in
 * a loop with an EXEC every third round, and whatever interrupt is due in
 * between. Its sectors must be the image's. The EXEC comes first, as the
 * SDK's does behind a request, unless plain_check_first says otherwise. */
static bool plain_check_first;
static void plain_finish(int32_t token, uint32_t lba, uint32_t count) {
    invariants();
    int32_t status = KUI_GD_PROCESSING;
    for(unsigned n = 0u; status == KUI_GD_PROCESSING; ++n) {
        CHECK(n < 200000u);
        (void)hardware();
        if(n % 3u == (plain_check_first ? 2u : 0u)) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        status = gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
        invariants();
    }
    CHECK(status == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == count * R.shared.service.sector_bytes && !get(STATUS + 4u));
    compare(lba, count, OUTPUT);
}
static void plain_read(uint32_t lba, uint32_t count) {
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    plain_finish(request(lba, count, OUTPUT), lba, count);
}
static void no_drops(const struct kui_toy_wire_report *w) {
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
}

/* Ordinary reads one behind the other. The first is the low reader's. The
 * second begins where the first ended: a window, and the stream takes the
 * card over where the low reader left it, with no card command. The window
 * serves what follows, is given up for a read elsewhere and for one too
 * large for it, and ends with its track. */
static void sequence_plain(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t *trace = c->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    plain_read(45010u, 5u);
    CHECK(!c->movie_ahead.variant && !stream_on() && !c->stats.hit_requests && !w->other_windows);
    CHECK(card.cmd18 == 1u && !card.cmd12 && !w->n.starts);
    plain_read(45015u, 7u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && w->other_sectors == 7u);
    CHECK(c->stats.hit_requests == 1u && card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u);
    plain_read(45022u, 13u);
    /* The largest request the window takes. */
    plain_read(45035u, 60u);
    CHECK(c->stats.hit_requests == 3u && w->other_sectors == 80u && w->other_windows == 1u);
    CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u && c->movie_ahead.first == 45095u);
    no_drops(w);
    /* Nothing of it is the movies' in the report. */
    CHECK(!trace[0] && !trace[1] && !trace[2] && !trace[3]);
    /* The game goes on without reading: the window stays as it stands,
     * filled, with the card inside its command. */
    for(unsigned n = 0u; n < 200u; ++n) {
        (void)hardware();
        if(n % 4u == 0u) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        invariants();
    }
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->movie_ahead.first == 45095u &&
        c->movie_ahead.verified - c->movie_ahead.first >= 60u && card.cmd18 == 1u && !card.cmd12);
    no_drops(w);
    /* A read elsewhere: the window is given up, the low reader ends the
     * card's command and reads. */
    plain_read(45210u, 3u);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && c->stats.hit_requests == 3u);
    closed();
    /* The next consecutive read has a window again, again without a card
     * command. */
    plain_read(45213u, 4u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u && c->stats.hit_requests == 4u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u);
#if KUI_TOY_WIRE_ANY
    /* Test 78: one sector more than a window used to take, at the
     * window's first sector. The window serves it, a slice at a time,
     * and stays. */
    plain_read(45217u, 61u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u &&
        c->stats.hit_requests == 5u && w->large_requests == 1u && w->large_sectors == 61u && !w->large_returned);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && w->other_windows == 2u);
    /* To the track's last sector, from the same window. */
    plain_read(45278u, 10u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u);
    plain_read(45288u, 12u);
    CHECK(c->stats.hit_requests == 7u && c->movie_ahead.variant == OTHER_WINDOW &&
        c->movie_ahead.first == 45300u && c->movie_ahead.verified == 45300u);
    CHECK(w->other_sectors == 80u + 4u + 61u + 10u + 12u && !w->first_error);
#else
    /* One sector more than the window takes, at the window's first
     * sector: the low reader's, and the window is given up for it. */
    plain_read(45217u, 61u);
    CHECK(!c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 2u && c->stats.hit_requests == 4u);
    /* To the track's last sector. */
    plain_read(45278u, 10u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 3u);
    plain_read(45288u, 12u);
    CHECK(c->stats.hit_requests == 6u && c->movie_ahead.variant == OTHER_WINDOW &&
        c->movie_ahead.first == 45300u && c->movie_ahead.verified == 45300u);
    CHECK(w->other_sectors == 80u + 4u + 10u + 12u && !w->first_error);
#endif
    /* The window has served its track's every sector and stands at the
     * next track's first. A read there is not its to serve, though it
     * begins at the window's first sector: it would wait for ever. The
     * window is given up for it by the request's own call, not by a later
     * one in which a game that waits has waited long enough. */
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    /* (Test 78 had one window less to give up, and served one request more.) */
    const unsigned gone = KUI_TOY_WIRE_ANY ? 2u : 3u, hits = KUI_TOY_WIRE_ANY ? 7u : 6u;
    const int32_t over = request(45300u, 2u, OUTPUT);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->drops[KUI_TOY_WIRE_DROP_OTHER] == gone);
    plain_finish(over, 45300u, 2u);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->drops[KUI_TOY_WIRE_DROP_OTHER] == gone);
    CHECK(c->stats.hit_requests == hits && !c->stats.abandoned);
    CHECK(!w->drops[KUI_TOY_WIRE_DROP_ERROR] && !w->drops[KUI_TOY_WIRE_DROP_IDLE] &&
        !w->drops[KUI_TOY_WIRE_DROP_COMMAND] && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    CHECK(!trace[0] && !trace[1] && !trace[2] && !trace[3]);
    printf("  ordinary reads one behind the other: the second opens a window with no card command; %u requests of "
           "%u served from %u windows, %u sectors; %s; CMD18 %u, CMD12 %u\n", (unsigned)c->stats.hit_requests, 9u,
        (unsigned)w->other_windows, (unsigned)w->other_sectors,
        KUI_TOY_WIRE_ANY ? "the read of 61 sectors served from its window in slices; a window given up for a read "
            "elsewhere and one at the track's end" :
            "given up for a read elsewhere, for one of 61 sectors and at the track's end", card.cmd18, card.cmd12);
}

/* Reads that reach over the track's end into the next track. One that
 * begins at an open window's first sector is given up for, like any read
 * the window cannot serve whole. A consecutive one with no window open is
 * given a window, which the request's next visit gives up. The low reader
 * reads both. */
static void sequence_track_end(bool check_first) {
    open_card(2000u, false, true, file_amid, false);
    plain_check_first = check_first;
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45270u, 5u);
    plain_read(45275u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->movie_ahead.first == 45280u && w->other_windows == 1u);
    /* 45280 to 45301: the track's last twenty sectors and the next track's two. */
    plain_read(45280u, 22u);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u);
    CHECK(c->stats.hit_requests == 1u && w->other_windows == 1u);
    plain_read(45200u, 5u);
#if KUI_TOY_WIRE_ANY && !KUI_TOY_WIRE_STAY
    /* Test 78 without STAY: a read of more than sixty sectors is never all
     * in the window at once, and a read the game waits for keeps no window
     * without STAY. The rest of this scenario needs such a read served:
     * here it is the low reader's again, exact, and the scenario ends. */
    plain_read(45205u, 61u);
    CHECK(R.shared.service.position_lba == 45265u && !w->first_error && !c->stats.abandoned);
    plain_check_first = false;
    printf("  reads that reach over the track's end, the game's %s call first, without STAY: the read at an open "
           "window's first sector given up for and read by the low reader, exact; a read of 61 sectors the game "
           "waits for is the low reader's too\n", check_first ? "CHECK" : "EXEC");
    return;
#endif
    plain_read(45205u, 61u);
#if KUI_TOY_WIRE_ANY
    /* Test 78: the consecutive read of 61 sectors has a window, and is
     * served from it. */
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u && w->large_requests == 1u &&
        R.shared.service.position_lba == 45265u);
#else
    CHECK(!c->movie_ahead.variant && w->other_windows == 1u && R.shared.service.position_lba == 45265u);
#endif
    plain_read(45266u, 36u);
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "track end: variant %u on %d windows %u drops %u %u %u %u %u %u hits %u abandoned %u\n",
        (unsigned)c->movie_ahead.variant, stream_on(), (unsigned)w->other_windows, (unsigned)w->drops[0], (unsigned)w->drops[1],
        (unsigned)w->drops[2], (unsigned)w->drops[3], (unsigned)w->drops[4], (unsigned)w->drops[5],
        (unsigned)c->stats.hit_requests, (unsigned)c->stats.abandoned);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->other_windows == 2u &&
        w->drops[KUI_TOY_WIRE_DROP_OTHER] == 2u && c->stats.hit_requests == (KUI_TOY_WIRE_ANY ? 2u : 1u));
#if KUI_TOY_WIRE_ANY
    /* Test 78: the read of 61 sectors above had a window, so the one over
     * the track's end met an open window. Here one that opens a window
     * for itself: behind a read elsewhere, with no window open, 45255 to
     * 45301. Its next visit gives the window up, as one the window does
     * not serve. */
    plain_read(45250u, 5u);
    CHECK(!c->movie_ahead.variant && w->other_windows == 2u);
    plain_read(45255u, 47u);
    CHECK(!c->movie_ahead.variant && !stream_on() && w->other_windows == 3u &&
        w->drops[KUI_TOY_WIRE_DROP_OTHER] == 3u && c->stats.hit_requests == 2u && !w->large_returned);
#endif
    /* Neither window is counted as given up for a read the game waited
     * for, whichever of the game's calls found it: the window had not the
     * sectors to give. */
    CHECK(!w->drops[KUI_TOY_WIRE_DROP_ERROR] && !w->drops[KUI_TOY_WIRE_DROP_WAITING] && !c->stats.abandoned);
    plain_check_first = false;
    printf("  reads that reach over the track's end, the game's %s call first: never the window's, one at an open "
           "window's first sector and one that opened a window for itself; both read by the low reader, exact\n",
        check_first ? "CHECK" : "EXEC");
}

/* A token search of the low reader's left standing. A read on its movie
 * path looks for its block's token a little a visit; the game ends that
 * read before the token has come, and then asks again from the same
 * sector. No window is opened on top of that search: its seek would carry
 * the search on as the low reader's own reception, which nothing ends. The
 * low reader reads the request, and the read behind it has its window.
 * Three games: one that polls as the movie's player does, one the reader
 * does not know, and one that makes no call at all. */
static void sequence_pending(unsigned polls) {
    static const char *const names[3] = {"the player's polls", "EXEC and CHECK", "no call at all"};
    /* The movie's file over the whole track, no movie playing. */
    open_card(2000u, false, true, (struct kui_toy_movie_range){FIRST - 386u, WINDOW_END - (FIRST - 386u)}, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
#if KUI_TOY_WIRE_ANY
    /* Test 78: no read is too large for a window, and a read behind the
     * last one has a window. It is the low reader's all the same where a
     * window is refused it: where one was given up for this very request,
     * or could not be begun for it. The reader remembers such a request
     * by its token, and the fixture says so for the one to come. */
    enum { PENDING_AT = 45032u };
    const unsigned before = 0u;
    plain_read(45020u, 12u);
    CHECK(!c->movie_ahead.variant && R.shared.service.position_lba == 45031u);
    kui_native_readahead_aux_context.movie_ahead.failed_token = R.shared.service.token + 1u;
    card.gap = 400u; /* the next block's token comes late */
    const int32_t first = request(45032u, 61u, OUTPUT);
    CHECK((uint32_t)first == kui_native_readahead_aux_context.movie_ahead.failed_token);
    CHECK(R.shared.stream.toy_suite_active && R.shared.service.pending && !c->movie_ahead.variant);
#else
    enum { PENDING_AT = 45032u };
    const unsigned before = 0u;
    plain_read(45020u, 12u);
    CHECK(!c->movie_ahead.variant && R.shared.service.position_lba == 45031u);
    card.gap = 400u; /* the next block's token comes late */
    /* Too large for a window: the low reader's, on its movie path. */
    const int32_t first = request(45032u, 61u, OUTPUT);
    CHECK(R.shared.stream.toy_suite_active && R.shared.service.pending && !c->movie_ahead.variant);
#endif
    CHECK(gd(KUI_GD_ABORT, (uint32_t)first, 0) == 0);
    CHECK(gd(KUI_GD_CHECK, (uint32_t)first, STATUS) == KUI_GD_FAILED);
    /* The search stands, and the drive has not moved. */
    CHECK(R.shared.stream.state == 5u && R.shared.service.position_lba == PENDING_AT - 1u);
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const int32_t token = request(PENDING_AT, REQUEST, OUTPUT);
    CHECK(!c->movie_ahead.variant && !stream_on());
    unsigned rounds = 0u;
    for(; R.shared.service.pending; ++rounds) {
        CHECK(rounds < 20000u && !R.engine.failed);
        const bool moved = hardware();
        if(polls == 0u && rounds % 5u == 4u) (void)sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll);
        if(polls == 1u && rounds % 5u == 4u) {
            CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
            if(R.shared.service.pending) (void)gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
        }
        /* With no call of the game's, interrupts and the controller's wake
         * are all there is. */
        if(polls == 2u && !moved) (void)independent_demand_wake();
        invariants();
    }
    card.gap = 0u;
    CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == REQUEST * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(PENDING_AT, REQUEST, OUTPUT);
    CHECK(!c->movie_ahead.variant && c->stats.hit_requests == before && w->n.starts == (before ? 1u : 0u));
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i)
        CHECK(w->drops[i] == (i == KUI_TOY_WIRE_DROP_ERROR && before ? 1u : 0u));
    /* The read behind it is consecutive and finds the low reader at rest. */
    read_request(PENDING_AT + REQUEST, REQUEST, 3u);
    CHECK(c->movie_ahead.variant == 4u && c->stats.hit_requests == before + 1u);
    printf("  a token search left standing by a read the game ended, the game then asking again with %s: no window "
           "on top of it, the low reader's read in %u rounds, the next read's window as ever\n", names[polls], rounds);
}

#if KUI_TOY_WIRE_STAY
/* The card's stop command fails once when a window is given up for a read
 * elsewhere. That is not the read's failure: the window is closed all the
 * same and the low reader, which ends the card's command before its own
 * read, sends the command again. */
static unsigned bad_stop;
static bool bad_stop_done;
static uint8_t bad_stop_transfer(void *context, uint8_t value, bool slow) {
    const unsigned before = card.cmd12;
    const uint8_t out = bus_transfer(context, value, slow);
    if(card.cmd12 != before && card.cmd12 == bad_stop && !bad_stop_done && card.qlen >= 2u) {
        card.queue[(card.qhead + 1u) % sizeof(card.queue)] = 0x20u; /* its answer, with an error bit */
        bad_stop_done = true;
    }
    return out;
}
static void sequence_stop_failed(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    plain_read(45015u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on() && !card.cmd12);
    R.shared.card.device.sd.bus.transfer = bad_stop_transfer;
    bad_stop = card.cmd12 + 1u; bad_stop_done = false;
    plain_read(45200u, 3u);
    R.shared.card.device.sd.bus.transfer = bus_transfer;
    CHECK(bad_stop_done && card.cmd12 == 2u && !c->movie_ahead.variant && !stream_on());
    CHECK(w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u && !w->drops[KUI_TOY_WIRE_DROP_ERROR] && !c->stats.abandoned);
    closed();
    /* And the reads go on as after any window. */
    plain_read(45203u, 4u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u && c->stats.hit_requests == 2u);
    printf("  the card's stop command failing once as a window is given up for a read elsewhere: the window closed, "
           "the command sent again by the low reader, the read exact; CMD12 %u\n", card.cmd12);
}
#endif

#if KUI_TOY_WIRE_STAY
/* The same across extents that lie apart on the card, and with stalls. */
static void sequence_scattered(uint32_t mean, unsigned dense) {
    open_card(28u, true, true, file_amid, false);
    stall_mean = mean; stall_dense = dense;
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    unsigned requests = 0u;
    for(uint32_t lba = 45003u; lba + 11u <= 45300u; lba += 11u, ++requests) plain_read(lba, 11u);
    /* All but the first, and those a window in trouble handed back. */
    CHECK(c->stats.hit_requests >= requests - 1u - w->drops[KUI_TOY_WIRE_DROP_ERROR] -
        w->drops[KUI_TOY_WIRE_DROP_WAITING] - c->stats.abandoned);
    CHECK(w->stops[KUI_TOY_WIRE_STOP_JUMP] >= 2u && !w->drops[KUI_TOY_WIRE_DROP_OTHER] &&
        !w->drops[KUI_TOY_WIRE_DROP_IDLE] && !w->drops[KUI_TOY_WIRE_DROP_COMMAND]);
    /* On a quiet bus no window is given up, where a read that has to wait
     * keeps its window (STAY). */
    if(!mean && KUI_TOY_WIRE_STAY)
        CHECK(c->stats.hit_requests == requests - 1u && w->other_windows == 1u && !c->stats.abandoned);
    printf("  the same over scattered extents, stall every ~%u bytes, %u%% busy phases: %u requests, %u from the "
           "window; streams %u, ends close/jump/sector/frame %u/%u/%u/%u, windows %u, CMD18 %u, CMD12 %u\n",
        (unsigned)mean, dense, requests, (unsigned)c->stats.hit_requests, (unsigned)w->n.starts,
        (unsigned)w->stops[0], (unsigned)w->stops[1], (unsigned)w->stops[2], (unsigned)w->stops[3],
        (unsigned)w->other_windows, card.cmd18, card.cmd12);
}

/* A window and the movie's file. One opened ahead of the file reads on
 * into it and through it; one opened inside it is that movie's in the
 * report, playing or not, and reads on behind the file's end. */
static void sequence_file(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t *trace = c->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    plain_read(FILE_FIRST - 20u, 5u);
    plain_read(FILE_FIRST - 15u, 10u);
    plain_read(FILE_FIRST - 5u, 10u);  /* over the file's first sector */
    plain_read(FILE_FIRST + 5u, 10u);  /* inside the file */
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->stats.hit_requests == 3u && w->other_windows == 1u);
    CHECK(w->other_sectors == 30u && !trace[1] && !trace[3] && card.cmd18 == 1u && !card.cmd12);
    no_drops(w);
    plain_read(45250u, 2u);
    plain_read(FILE_FIRST + 30u, 4u);
    CHECK(!c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u);
    plain_read(FILE_FIRST + 34u, 6u);
    CHECK(c->movie_ahead.variant == 4u && trace[3] == 1u && trace[1] == 6u && w->other_windows == 1u);
    plain_read(FILE_FIRST + 40u, 60u); /* to the file's last sector */
    plain_read(FILE_FIRST + 100u, 5u); /* behind it */
    CHECK(c->movie_ahead.variant == 4u && trace[3] == 1u && trace[1] == 71u && !trace[0] && !trace[2]);
    CHECK(w->other_windows == 1u && w->other_sectors == 30u && c->stats.hit_requests == 6u);
    CHECK(w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u);
    /* A read that begins in the file and reaches beyond its last sector is
     * not the file's: the window it opens is counted with the others. */
    plain_read(45250u, 2u);
    plain_read(FILE_FIRST + 85u, 5u);
    plain_read(FILE_FIRST + 90u, 15u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u && w->other_sectors == 45u);
    CHECK(trace[3] == 1u && trace[1] == 71u && c->stats.hit_requests == 7u);
    printf("  a window and the movie's file: one opened ahead of the file served %u sectors into it, one opened "
           "inside it %u sectors to behind its end, counted as that movie's; one opened by a read over the "
           "file's end counted with the others\n", 30u, (unsigned)trace[1]);
}

/* A command of the game's that is not a read, to its acknowledgment. */
static void plain_command(uint32_t command, uint32_t word) {
    put(PARAM, word); put(PARAM + 4u, 0u); put(PARAM + 8u, 0u); put(PARAM + 12u, 0u);
    const int32_t token = gd(KUI_GD_REQUEST, command, PARAM);
    CHECK(token > 0);
    CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_COMPLETED);
    invariants();
}
#if KUI_TOY_WIRE_QUERY
/* A command of the game's with four words of its own, to its
 * acknowledgment, with whatever interrupt is due before each call. */
static void asked_command(uint32_t command, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    put(PARAM, a); put(PARAM + 4u, b); put(PARAM + 8u, c); put(PARAM + 12u, d);
    (void)hardware();
    const int32_t token = gd(KUI_GD_REQUEST, command, PARAM);
    CHECK(token > 0);
    (void)hardware();
    CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    (void)hardware();
    CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_COMPLETED && !get(STATUS + 4u));
    invariants();
}
/* Each of the seven commands that only ask, by its number's bit. */
#define ASKED_LOW (UINT32_C(1) << KUI_RETAIL_GD_GETTOC | UINT32_C(1) << KUI_GD_GETTOC2 | UINT32_C(1) << KUI_GD_NOP | \
    UINT32_C(1) << KUI_RETAIL_GD_REQ_MODE)
#define ASKED_HIGH (UINT32_C(1) << (KUI_RETAIL_GD_GETSCD - 32u) | UINT32_C(1) << (KUI_RETAIL_GD_REQ_STAT - 32u) | \
    UINT32_C(1) << (KUI_RETAIL_GD_GET_VERS - 32u))
static void asked_all(void) {
    asked_command(KUI_GD_NOP, 0u, 0u, 0u, 0u);
    asked_command(KUI_RETAIL_GD_GETSCD, 1u, 14u, STATUS + 32u, 0u);
    asked_command(KUI_RETAIL_GD_REQ_STAT, STATUS + 32u, STATUS + 36u, STATUS + 40u, STATUS + 44u);
    asked_command(KUI_RETAIL_GD_REQ_MODE, STATUS + 32u, 0u, 0u, 0u);
    asked_command(KUI_RETAIL_GD_GET_VERS, STATUS + 32u, 0u, 0u, 0u);
    asked_command(KUI_RETAIL_GD_GETTOC, 0u, OUTPUT, 0u, 0u);
    asked_command(KUI_GD_GETTOC2, 0u, OUTPUT, 0u, 0u);
}
#endif
/* Commands between consecutive reads. One that is not a read gives the
 * window up; the drive has not moved, and the next read has a window
 * again. A seek moves the drive: the read that would have been consecutive
 * is the low reader's. INIT, RESET and the abort of a waiting read end the
 * window and leave the drive where it was: the read that follows on, and a
 * read asked again after its abort, are consecutive as before. */
static void sequence_commands(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    plain_read(45015u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on() && card.cmd18 == 1u && !card.cmd12);
#if KUI_TOY_WIRE_QUERY
    /* Test 80: a command that only asks the drive something, or does
     * nothing, leaves the window, whichever of the seven it is; each is
     * counted once and named by its number's bit. The window has moved no
     * sector for them. One that stops the drive gives the window up, as
     * every command that is not a read did. */
    {
        const uint32_t first = c->movie_ahead.first;
        asked_all();
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on() && card.cmd18 == 1u && !card.cmd12);
        CHECK(c->movie_ahead.first == first && R.shared.service.position_lba == first - 1u);
        no_drops(w);
        CHECK(w->queries == 7u && w->command_seen[0] == ASKED_LOW && w->command_seen[1] == ASKED_HIGH);
        plain_command(KUI_GD_STOP, 0u);
        CHECK(w->queries == 7u && w->command_seen[0] == ASKED_LOW &&
            w->command_seen[1] == (ASKED_HIGH | UINT32_C(1) << (KUI_GD_STOP - 32u)));
    }
#else
    plain_command(KUI_GD_NOP, 0u);
#endif
    CHECK(!c->movie_ahead.variant && !stream_on() && w->drops[KUI_TOY_WIRE_DROP_COMMAND] == 1u && card.cmd12 == 1u);
    closed();
    plain_read(45020u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 2u && c->stats.hit_requests == 2u);
    CHECK(card.cmd18 == 2u && card.cmd12 == 1u);
    plain_command(KUI_RETAIL_GD_SEEK, 45100u + 150u);
    CHECK(!c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_COMMAND] == 2u && R.shared.service.position_lba == 45100u);
    plain_read(45025u, 5u);
    CHECK(!c->movie_ahead.variant && w->other_windows == 2u && c->stats.hit_requests == 2u);
    plain_read(45030u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 3u && c->stats.hit_requests == 3u);
    /* A seek to the sector before a read makes that read consecutive. */
    plain_command(KUI_RETAIL_GD_SEEK, 45059u + 150u);
    plain_read(45060u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 4u && c->stats.hit_requests == 4u);
    /* INIT and RESET end the window with the request state. They do not
     * move the service's drive position, so the read that follows on from
     * before them is consecutive still. */
    for(unsigned how = 0u; how < 2u; ++how) {
        const uint32_t windows = w->other_windows, hits = c->stats.hit_requests;
        const uint32_t at = c->movie_ahead.first;
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && R.shared.service.position_lba == at - 1u);
        CHECK(gd(how ? KUI_GD_RESET : KUI_GD_INIT, 0, 0) == 0);
        CHECK(!c->movie_ahead.variant && !stream_on() && R.shared.service.position_lba == at - 1u);
        closed();
        mode(2048u);
        plain_read(at, 5u);
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == windows + 1u &&
            c->stats.hit_requests == hits + 1u);
    }
    /* A read that waits on a window is aborted. The window is opened by
     * that read, behind one elsewhere, while the wire brings nothing. */
    {
        plain_read(45200u, 3u);
        const uint32_t windows = w->other_windows, hits = c->stats.hit_requests;
        CHECK(!c->movie_ahead.variant && R.shared.service.position_lba == 45202u);
        m.delay = 100000u;
        const int32_t token = request(45203u, 40u, OUTPUT);
        CHECK(R.shared.service.pending && R.engine.active && c->movie_ahead.variant == OTHER_WINDOW &&
            w->other_windows == windows + 1u && c->movie_ahead.verified - c->movie_ahead.first < 40u);
        CHECK(gd(KUI_GD_ABORT, (uint32_t)token, 0) == 0 && !R.shared.service.pending);
        CHECK(!c->movie_ahead.variant && !stream_on());
        m.delay = 0u;
        for(unsigned i = 0u; i < 4u; ++i) {(void)gd(KUI_GD_EXEC, 0, 0); (void)gd(KUI_GD_CHECK, (uint32_t)token, STATUS);}
        closed();
        /* Nothing of it was delivered, so the drive has not moved: asked
         * again it is consecutive as it was, and a new window serves it. */
        CHECK(R.shared.service.position_lba == 45202u);
        plain_read(45203u, 40u);
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == windows + 2u &&
            c->stats.hit_requests == hits + 1u);
    }
    /* Sectors asked for whole, 2,352 bytes each, at the window's first
     * sector: never the window's. */
    {
        const uint32_t at = c->movie_ahead.first, hits = c->stats.hit_requests;
        const uint32_t other = w->drops[KUI_TOY_WIRE_DROP_OTHER];
        mode(2352u);
        plain_read(at, 3u);
        mode(2048u);
        CHECK(!c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_OTHER] == other + 1u &&
            c->stats.hit_requests == hits);
    }
    CHECK(!w->drops[KUI_TOY_WIRE_DROP_ERROR] && !w->drops[KUI_TOY_WIRE_DROP_WAITING] && !c->stats.abandoned);
    printf("  commands between consecutive reads: a window given up for a command that is not a read (%u), ended "
           "by INIT, RESET and an abort, never opened across a seek, and reopened by the next consecutive read: "
           "%u windows\n", (unsigned)w->drops[KUI_TOY_WIRE_DROP_COMMAND], (unsigned)w->other_windows);
}

#endif
/* Ordinary reads the game waits for in a loop of its own with interrupts
 * held off, one behind the other: the window keeps them (STAY) and the
 * stream serves them with no interrupt getting through. */
static void sequence_waited(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u;
    enum { WAITED = 8u };
    unsigned rounds = 0u;
    uint32_t lba = 45010u;
    for(unsigned i = 0u; i < WAITED; ++i, lba += REQUEST) rounds += waited_request(lba, REQUEST, true);
    CHECK(!eris && !timers);
#if KUI_TOY_WIRE_STAY
    CHECK(stream_on() && c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u);
    CHECK(c->stats.hit_requests == WAITED - 1u && !c->stats.abandoned && card.cmd18 == 1u && !card.cmd12);
    no_drops(w);
    CHECK(w->stays >= 1u && w->stays <= WAITED - 1u);
    printf("  %u ordinary reads the game waited for with interrupts held off: the first the low reader's, then one "
           "window and one stream, %u kept waiting for the wire, %u rounds of the game's loop\n", (unsigned)WAITED,
        (unsigned)w->stays, rounds);
#else
    /* Without STAY each opens a window with its first call and gives it
     * up with its second; the low reader reads them all. */
    CHECK(w->drops[KUI_TOY_WIRE_DROP_WAITING] == WAITED - 1u && !c->stats.hit_requests);
    printf("  %u ordinary reads the game waited for with interrupts held off: %u windows opened and given up, the "
           "low reader read them all, %u rounds of the game's loop\n", (unsigned)WAITED,
        (unsigned)w->drops[KUI_TOY_WIRE_DROP_WAITING], rounds);
#endif
    m.delay = 0u;
}

#if KUI_TOY_WIRE_STAY
/* A request served in an interrupt visit. Outside the movie files the
 * resident reports it complete only from the game's next call, and until
 * then the request state is that of a read under way with nothing left to
 * do. That is no failed request: the window stays through the interrupt
 * visits in between and through the call that reports. */
static void sequence_interrupt(void) {
    open_card(2000u, false, true, file_amid, false);
    stall_mean = 900u; /* interrupts keep coming */
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    plain_read(45015u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW);
    /* Requests as large as the window takes: after one, the next has to
     * wait for the stream. */
    enum { LARGE = 60u };
    unsigned served = 0u;
    for(uint32_t lba = 45020u; lba + LARGE <= 45300u; lba += LARGE) {
        if(c->movie_ahead.variant != OTHER_WINDOW || c->movie_ahead.first != lba) {
            /* A window in trouble was handed back: go on from a new one. */
            plain_read(lba, LARGE);
            continue;
        }
        memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
        const uint32_t drops = w->drops[KUI_TOY_WIRE_DROP_IDLE];
        const uint32_t closes = w->stops[KUI_TOY_WIRE_STOP_CLOSE];
        const unsigned cmd12 = card.cmd12;
        const int32_t token = request(lba, LARGE, OUTPUT);
        /* Interrupts only, until the window has served it or needs the
         * foreground. */
        for(unsigned n = 0u; n < 20000u && R.engine.active; ++n) if(!hardware()) break;
        if(!R.engine.active && R.shared.service.pending) {
            /* Served, and not reported. */
            CHECK(R.shared.service.status == KUI_GD_PROCESSING && !R.shared.stream.toy_suite_active);
            CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->movie_ahead.first == lba + LARGE);
            for(unsigned n = 0u; n < 6u; ++n) (void)hardware();
            CHECK(R.shared.service.pending && c->movie_ahead.variant == OTHER_WINDOW &&
                c->movie_ahead.first == lba + LARGE && w->drops[KUI_TOY_WIRE_DROP_IDLE] == drops);
            /* Nor was its stream stopped for it: an interrupt visit that
             * took the request for a failed one would end the stream there
             * and leave the window to the next call. */
            CHECK(w->stops[KUI_TOY_WIRE_STOP_CLOSE] == closes && card.cmd12 == cmd12);
            CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_COMPLETED);
            CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->drops[KUI_TOY_WIRE_DROP_IDLE] == drops);
            CHECK(w->stops[KUI_TOY_WIRE_STOP_CLOSE] == closes && card.cmd12 == cmd12);
            ++served;
        } else {
            int32_t status = R.shared.service.pending ? KUI_GD_PROCESSING : gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
            for(unsigned n = 0u; status == KUI_GD_PROCESSING; ++n) {
                CHECK(n < 200000u);
                (void)hardware();
                if(n % 3u == 0u) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
                status = gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
            }
            CHECK(status == KUI_GD_COMPLETED);
        }
        CHECK(get(STATUS + 8u) == LARGE * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
        compare(lba, LARGE, OUTPUT);
        invariants();
    }
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "served in an interrupt visit: %u\n", served);
    CHECK(served >= 2u && !w->drops[KUI_TOY_WIRE_DROP_IDLE]);
    printf("  ordinary reads served in an interrupt visit and reported by the game's next call: %u, the window kept "
           "through each\n", served);
}

/* The card is asked of the reader while a window is open, as the report's
 * save asks at the end of a console run: up to Test 76 the movie's end had
 * closed the window by then. The window is given up, its stream stopped
 * and the card's command ended before the card is handed over; when it
 * comes back the game reads on. Once with the window filled and its stream
 * standing still, once with a run in flight. */
static void sequence_lent(bool receiving) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    plain_read(45015u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on());
    if(receiving) {
        m.delay = 100000u;
        plain_read(45020u, 30u); /* room in the window */
        for(unsigned n = 0u; n < 100000u && !m.rx; ++n) if(!hardware()) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        CHECK(m.rx && stream_on() && (m.chcr & 1u));
    } else {
        for(unsigned n = 0u; n < 200u; ++n) {
            (void)hardware();
            if(n % 4u == 0u) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        }
        CHECK(stream_on() && !m.rx && c->movie_ahead.verified - c->movie_ahead.first >= 60u);
    }
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && !R.shared.service.pending && !R.engine.active);
    const uint32_t next = c->movie_ahead.first;
    const unsigned cmd12 = card.cmd12;
    const uint32_t sr = hw.sr;
    hw.sr = 0xf0u; /* the lender calls with interrupts held off */
    struct kui_loader_sd copied;
    CHECK(kui_retail_native_async_loan_begin(&copied) == KUI_VMU_NATIVE_ASYNC_LOAN_OK);
    m.delay = 0u;
    CHECK(!c->movie_ahead.variant && !stream_on() && card.cmd12 == cmd12 + 1u);
    CHECK(!R.engine.hooked && !R.engine.opened && !R.shared.port.acquired && !R.shared.stream.state &&
        R.engine.native_loan_state == KUI_VMU_NATIVE_ASYNC_LOAN_WRITER);
    /* Nothing of the stream's is left on the channel, the timers or the
     * SDK's table. */
    CHECK(!(m.chcr & 1u) && m.sar == 0x11111111u && m.dar == 0x22222222u && m.tcr == 0x33u);
    unsigned channel;
    CHECK(!timer_running(&channel) && !(wx.ipra & 0x0ff0u));
    for(unsigned i = 0; i < 64u; ++i) CHECK(wx.slots[i] == UINT32_C(0x8c0be408));
    /* It was no reason of the game's: nothing is counted as given up. */
    no_drops(w);
    CHECK(kui_retail_native_async_loan_end(true) == KUI_VMU_NATIVE_ASYNC_LOAN_OK);
    hw.sr = sr;
    CHECK(R.engine.native_loan_state == KUI_VMU_NATIVE_ASYNC_LOAN_IDLE && !kui_retail_hook_active);
    /* The game reads on where it was, and what follows that is served
     * from a window again. */
    const uint32_t hits = c->stats.hit_requests;
    plain_read(next, 5u);
    plain_read(next + 5u, 5u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->stats.hit_requests > hits);
    printf("  the card asked of the reader with a window open, its stream %s: window given up, stream stopped, "
           "CMD12, the card handed over; given back, the game reads on and has a window again\n",
        receiving ? "receiving" : "standing still");
}

#endif
/* Where there is no window. Without the resident's SCI route the stream
 * could not take the card over; and outside the verified title, or on a
 * track the stage did not find raw, the reader is as it always was. */
static void sequence_nowhere(unsigned what) {
    static const char *const names[4] = {"the SCI route not selected", "another title",
        "a source the stage did not find raw", "a track of 2,048-byte sectors on a disc whose movie track is raw"};
    /* The last: the stage found the movie's own track raw, and the reads
     * are on another data track of the disc that is not. */
    card_cooked = what == 3u;
    open_card(2000u, false, true, file_amid, false);
    card_cooked = false;
    if(what == 1u || what == 2u) {
        struct kui_native_ra_config config = movie_configuration();
        config.movie_ranges[3] = file_amid;
        if(what == 1u) config.toy_poll_owner = 0u;
        else config.suite_source = 2u;
        bind_movie_configuration(&config);
    } else if(!what) R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = 0u; /* the SDK's table is not the resident's yet */
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    for(uint32_t lba = 45010u; lba < 45100u; lba += 9u) {
        read_and_compare(lba, 9u, 0u, 2u);
        CHECK(!c->movie_ahead.variant && !stream_on());
    }
    CHECK(!w->other_windows && !c->stats.hit_requests && !w->n.starts && card.cmd18 == 1u && !card.cmd12);
    no_drops(w);
    printf("  no window with %s: ten consecutive reads, all the low reader's, one CMD18\n", names[what]);
}

/* A movie's start as the game reads it: the file's first sectors in one
 * large read, which is the low reader's, and then the refills. The first
 * refill begins where the large read ended and opens the window, with no
 * card command; that it lies 386 sectors into a playing movie's file
 * decides nothing. Then the movie ends and is played again, which the
 * player's hook does not see: its refills are served all the same. */
static void sequence_movie(void) {
    /* The file's 386th sector at 45062. */
    enum { REFILLS = 45062u };
    open_card(2000u, false, true, (struct kui_toy_movie_range){REFILLS - 386u, 200u + 386u}, true);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t *trace = c->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    /* 61 sectors up to it, by a caller that waits. */
    read_and_compare(REFILLS - 61u, 61u, 0u, 1u);
#if KUI_TOY_WIRE_ANY && !KUI_TOY_WIRE_STAY
    /* Test 78 without STAY: the large read is given a window and, waited
     * for, loses it again; the low reader reads it, exact (compared
     * above). What follows needs that read served from its window, and
     * is not checked in this variant. */
    CHECK(!w->first_error && !c->stats.abandoned && !w->large_requests);
    printf("  a movie's start as the game reads it, without STAY: the read of 61 sectors the game waits for is the "
           "low reader's, exact\n");
    (void)trace;
    return;
#endif
#if KUI_TOY_WIRE_ANY
    /* Test 78: the large read begins behind the drive's position and has
     * the window itself; the refills are served from the same one. (In the
     * game the two sectors of the file's start come before it, and are
     * the low reader's.) */
    enum { LARGE = 61u, OPENING = 1u };
    CHECK(c->movie_ahead.variant == 4u && trace[3] == 1u && card.cmd18 == 1u && !card.cmd12 &&
        w->large_requests == 1u && w->large_sectors == LARGE && c->stats.hit_requests == 1u);
#else
    enum { LARGE = 0u, OPENING = 0u };
    CHECK(!c->movie_ahead.variant && !stream_on() && !trace[3] && card.cmd18 == 1u && !card.cmd12);
#endif
    uint32_t lba = REFILLS;
    for(unsigned i = 0u; i < 6u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(c->movie_ahead.variant == 4u && trace[3] == 1u && trace[1] == LARGE + 6u * REQUEST && !w->other_windows);
    CHECK(c->stats.hit_requests == OPENING + 6u && card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u);
    no_drops(w);
    /* The movie is over. Its window stays until something else is read. */
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    kui_toy_movie_present_boundary(PLAYER,
        (uintptr_t)&kui_native_readahead_aux_context.movie_exit[1], KUI_TOY_MOVIE_EXIT_TERMINAL);
    for(unsigned n = 0u; n < 50u; ++n) {(void)hardware(); if(n % 4u == 0u) game_poll();}
    CHECK(c->movie_ahead.variant == 4u && !card.cmd12);
    no_drops(w);
    /* Played again from its start: the large read gives the window up,
     * and the first refill opens the next one. */
    read_and_compare(REFILLS - 61u, 61u, 0u, 1u);
    CHECK(!c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u && card.cmd12 == 1u && card.cmd18 == 2u);
    lba = REFILLS;
    for(unsigned i = 0u; i < 6u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(c->movie_ahead.variant == 4u && trace[3] == 2u && trace[1] == LARGE + 12u * REQUEST && !w->other_windows);
    CHECK(c->stats.hit_requests == OPENING + 12u && card.cmd18 == 2u && card.cmd12 == 1u);
#if KUI_TOY_WIRE_ANY
    printf("  a movie's start as the game reads it: the read of 61 sectors opens the window and comes through it, "
           "the refills from the same window; played again after its end, that read gives the window up and the "
           "first refill opens a second; %u refills and the one large read served\n",
        (unsigned)c->stats.hit_requests - OPENING);
#else
    printf("  a movie's start as the game reads it: a read of 61 sectors by the low reader, the first refill a "
           "window with no card command; played again after its end, a second window; %u refills served\n",
        (unsigned)c->stats.hit_requests);
#endif
}

#if KUI_TOY_WIRE_ANY && KUI_TOY_WIRE_STAY
/* ---- Test 78: a window serves a request of any size ---- */
#define ANY_OUTPUT (BEGIN + 0x100000u)
enum { ANY_SLICE = 16u };
/* What a request delivered, against the image, as much of it at a time as
 * the fixture's comparison buffer holds; and the byte behind it. */
static void any_compare(uint32_t lba, uint32_t count) {
    for(uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done > 64u ? 64u : count - done;
        CHECK(kui_retail_image_read(&reference, lba + done, n, KUI_GAME_SECTOR_MODE1, expected,
            sizeof(expected)) == KUI_GAME_OK);
        CHECK(!memcmp(ram + (ANY_OUTPUT - BEGIN) + (size_t)done * 2048u, expected, (size_t)n * 2048u));
        done += n;
    }
    CHECK(ram[ANY_OUTPUT - BEGIN + (size_t)count * 2048u] == 0xa5);
}
static int32_t any_begin(uint32_t lba, uint32_t count) {
    memset(ram + (ANY_OUTPUT - BEGIN), 0xa5, (size_t)count * 2048u + 16u);
    return request(lba, count, ANY_OUTPUT);
}
/* The request's own count of sectors, looked at after every event. While
 * a window serves it the count never goes back, and no event hands over
 * more than two slices of a large request; one of up to sixty sectors
 * comes whole. */
static unsigned any_events;
static void any_step(uint32_t *seen, bool sliced) {
    const uint32_t now = R.engine.cursor.done;
    if(sliced) CHECK(now >= *seen && (R.shared.service.count <= 60u || now - *seen <= 2u * ANY_SLICE));
    if(now != *seen) ++any_events;
    *seen = now;
    invariants();
}
/* A read of any size by a caller the reader does not know: CHECK in a
 * loop with an EXEC every third round and whatever interrupt is due in
 * between, as plain_finish. What the game is told of the request's
 * progress never goes back, whoever serves it. */
static void any_finish(int32_t token, uint32_t lba, uint32_t count, bool sliced) {
    int32_t status = KUI_GD_PROCESSING;
    uint32_t seen = R.engine.cursor.done, told = 0u;
    for(unsigned n = 0u; status == KUI_GD_PROCESSING; ++n) {
        CHECK(n < 400000u);
        (void)hardware();
        any_step(&seen, sliced);
        if(n % 3u == 0u) {
            CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
            any_step(&seen, sliced);
        }
        status = gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
        any_step(&seen, sliced);
        CHECK(get(STATUS + 8u) >= told);
        told = get(STATUS + 8u);
    }
    CHECK(status == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == count * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    any_compare(lba, count);
}
static void any_read(uint32_t lba, uint32_t count, bool sliced) {
    any_finish(any_begin(lba, count), lba, count, sliced);
}

/* Evolution 2's refills as its console capture of 25 October 2026 has
 * them: 73 and 50 sectors by turns, each beginning where the last ended.
 * The first is the low reader's. The second opens a window, and from
 * there every request is served from that one window, the large ones in
 * slices, with no card command and nothing given up. */
static void any_evolution(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    uint32_t lba = 45002u;
    any_read(lba, 73u, false); lba += 73u;
    CHECK(!c->movie_ahead.variant && !c->stats.hit_requests && card.cmd18 == 1u && !card.cmd12 &&
        !w->large_requests && !w->other_windows);
    any_read(lba, 50u, true); lba += 50u;
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && c->stats.hit_requests == 1u &&
        w->other_sectors == 50u && !w->large_requests && w->n.starts == 1u);
    /* The game plays on, and the window fills behind the request. */
    for(unsigned n = 0u; n < 100000u && c->movie_ahead.verified - c->movie_ahead.first < 60u; ++n)
        if(!hardware()) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    CHECK(c->movie_ahead.first == lba && c->movie_ahead.verified - c->movie_ahead.first >= 60u);
    /* 73 sectors: more than the window holds. What it holds, in slices,
     * and the rest as the stream brings it. */
    any_events = 0u;
    const uint32_t delivered = w->deliver_calls;
    const int32_t large = any_begin(lba, 73u);
    /* The request's own visit has handed over two slices of a full
     * window, and no more. (Without QUICK the call that brings the
     * request makes a whole visit of its own first, with nothing to hand
     * over yet: two visits are counted.) */
    CHECK(w->deliver_calls - delivered == (KUI_TOY_WIRE_QUICK ? 1u : 2u) && R.engine.cursor.done == 2u * ANY_SLICE &&
        R.shared.service.pending);
    any_finish(large, lba, 73u, true); lba += 73u;
    const unsigned events = any_events;
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->movie_ahead.first == lba && w->other_windows == 1u);
    CHECK(c->stats.hit_requests == 2u && w->large_requests == 1u && w->large_sectors == 73u && !w->large_returned);
    any_read(lba, 50u, true); lba += 50u;
    /* The track's last 52. */
    any_read(lba, 52u, true); lba += 52u;
    CHECK(lba == 45300u && c->stats.hit_requests == 4u && w->other_sectors == 50u + 73u + 50u + 52u);
    CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u && w->other_windows == 1u && !c->stats.abandoned);
    CHECK(w->large_requests == 1u && !w->large_returned && !w->first_error);
    no_drops(w);
    printf("  Evolution 2's refills, 73 and 50 sectors by turns: the first the low reader's, the second opens a "
           "window, four requests served from it, the one of 73 sectors in slices (%u events after its request); "
           "CMD18 %u, CMD12 %u\n", events, card.cmd18, card.cmd12);
}

/* One request of two hundred sectors behind a small one: more than three
 * windows' worth, through the one window as the stream brings it. On a
 * quiet bus and on two busy ones. */
static void any_long(uint32_t mean, unsigned dense, uint32_t pace) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    stall_mean = mean; stall_dense = dense;
    /* (At the console's pace a visit reads about six blocks, not all there is.) */
    reader_pace(pace);
    plain_read(45010u, 5u);
    CHECK(!c->movie_ahead.variant && card.cmd18 == 1u);
    any_events = 0u;
    any_read(45015u, 200u, true);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && c->stats.hit_requests == 1u);
    CHECK(w->large_requests == 1u && w->large_sectors == 200u && !w->large_returned && w->other_sectors == 200u);
    /* Two hundred sectors, two slices an event at most. */
    CHECK(any_events >= (200u - 4u * ANY_SLICE) / (2u * ANY_SLICE));
    const unsigned events = any_events;
    plain_read(45215u, 12u);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && c->stats.hit_requests == 2u);
    CHECK(!w->first_error && !c->stats.abandoned);
    no_drops(w);
    if(!mean) CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u);
    stall_mean = 0u; stall_dense = 0u;
    reader_pace(0u);
    printf("  a request of 200 sectors behind a small one, stalls %u/%u%s: opened a window and came through it in %u "
           "events, the read behind it from the same window; streams begun %u, CMD18 %u, CMD12 %u\n",
        (unsigned)mean, dense, pace ? ", at the console's pace" : "", events, (unsigned)w->n.starts, card.cmd18,
        card.cmd12);
}

/* A large request the window has begun to serve and cannot finish: the
 * window is marked in error under it. The request goes back to the low
 * reader whole, and the low reader delivers every sector of it. What the
 * game was told of its progress never goes back. The read behind it has a
 * window again. And the same where the card's stop command fails once as
 * the window is given up: the low reader sends it again. */
static void any_returned(bool stop_fails) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    const int32_t token = any_begin(45015u, 150u);
    for(unsigned n = 0u; n < 100000u && R.engine.cursor.done < 40u; ++n)
        if(!hardware()) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    const uint32_t had = R.engine.cursor.done;
    CHECK(had >= 40u && had < 150u && R.shared.service.pending && c->movie_ahead.variant == OTHER_WINDOW &&
        c->movie_ahead.first == 45015u + had && R.shared.service.position_lba == 45015u + had - 1u);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    if(stop_fails) {
        R.shared.card.device.sd.bus.transfer = bad_stop_transfer;
        bad_stop = card.cmd12 + 1u; bad_stop_done = false;
    }
    kui_native_readahead_aux_context.movie_ahead.error = true;
    any_finish(token, 45015u, 150u, false);
    R.shared.card.device.sd.bus.transfer = bus_transfer;
    CHECK(bad_stop_done == stop_fails || !stop_fails);
    CHECK(w->large_returned == 1u && !w->large_requests && w->drops[KUI_TOY_WIRE_DROP_ERROR] == 1u);
    CHECK(!c->movie_ahead.variant && !c->stats.hit_requests && card.cmd12 == (stop_fails ? 2u : 1u) &&
        card.cmd18 == 2u);
    CHECK(w->other_sectors == had && R.shared.service.position_lba == 45164u);
    plain_read(45165u, 12u);
    /* (45165 lies in the fixture's movie file: the window is that file's.) */
    CHECK(c->movie_ahead.variant == 4u && w->other_windows == 1u && c->stats.hit_requests == 1u &&
        c->movie_ahead.first == 45177u);
    printf("  a request of 150 sectors whose window failed after %u of them%s: back to the low reader whole, read "
           "again from its first sector, data exact; the read behind it has a window\n", (unsigned)had,
        stop_fails ? ", the card's stop command failing once" : "");
}

/* A request of 61 sectors the game waits for in a loop of its own, with no
 * interrupt getting through: opened by its request, kept while the
 * window grows, and served to its end. */
static void any_waited(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u;
    unsigned rounds = waited_request(45010u, REQUEST, true);
    CHECK(!c->movie_ahead.variant && card.cmd18 == 1u);
    /* (The fixture's own comparison takes 64 sectors at most.) */
    rounds += waited_request(45010u + REQUEST, 61u, true);
    CHECK(!eris && !timers);
    CHECK(stream_on() && c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u);
    CHECK(c->stats.hit_requests == 1u && w->large_requests == 1u && w->large_sectors == 61u && !w->large_returned);
    CHECK(card.cmd18 == 1u && !card.cmd12 && !c->stats.abandoned && w->stays == 1u);
    no_drops(w);
    m.delay = 0u;
    printf("  a request of 61 sectors the game waited for with interrupts held off: one window, kept while it grew, "
           "%u rounds of the game's loop\n", rounds);
}

/* The scenarios below came of the review of Test 78 (16 November 2026).
 *
 * A window filled to its most behind two small reads, at 45020. */
static void any_filled(void) {
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    open_card(2000u, false, true, file_amid, false);
    plain_read(45010u, 5u);
    plain_read(45015u, 5u);
    for(unsigned n = 0u; n < 100000u && c->movie_ahead.verified - c->movie_ahead.first < 61u; ++n)
        if(!hardware()) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    CHECK(c->movie_ahead.variant == OTHER_WINDOW && c->movie_ahead.first == 45020u &&
        c->movie_ahead.verified - c->movie_ahead.first == 61u);
}

/* A begun large request whose wire then brings nothing. The window hands
 * over what it holds. Then the game waits a quarter of a second without a
 * sector more: the window is given up as one the game waited for, the
 * request goes back to the low reader whole, and what the game was told
 * of it does not go back. */
static void any_in_vain(void) {
    any_filled();
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u; /* from here no run lands */
    const int32_t token = any_begin(45020u, 150u);
    for(unsigned calls = 0u, same = 0u; same < 4u; ++calls) {
        CHECK(calls < 200u);
        const uint32_t before = R.engine.cursor.done;
        CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_PROCESSING);
        same = R.engine.cursor.done == before ? same + 1u : 0u;
    }
    const uint32_t had = R.engine.cursor.done;
    CHECK(had >= 61u && had < 150u && R.shared.service.pending && c->movie_ahead.variant == OTHER_WINDOW);
    CHECK(get(STATUS + 8u) == had * KUI_GAME_DATA_BYTES && !w->large_returned && !w->drops[KUI_TOY_WIRE_DROP_WAITING]);
    clock_counter -= 195312u + 1000u;
    CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
    CHECK(!c->movie_ahead.variant && w->large_returned == 1u && w->drops[KUI_TOY_WIRE_DROP_WAITING] == 1u);
    CHECK(gd(KUI_GD_CHECK, (uint32_t)token, STATUS) == KUI_GD_PROCESSING && get(STATUS + 8u) >= had * KUI_GAME_DATA_BYTES);
    m.delay = 0u;
    any_finish(token, 45020u, 150u, false);
    CHECK(!w->large_requests && w->large_returned == 1u && !w->first_error && !c->stats.abandoned);
    printf("  a large request the game waited for in vain after %u of 150 sectors: the window given up after a "
           "quarter second, back to the low reader whole, data exact\n", (unsigned)had);
}

/* A large read over the track's end that opens a window for itself and
 * is handed sectors by the very visit that opens it: with DMA channel 1
 * the game's, the low reader fills the window there and then. Its next
 * visit gives the window up, and the request goes back whole. */
static void any_track_end(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45180u, 5u);
    m.chcr = 1u; /* the game uses DMA channel 1 */
    const uint32_t lba = 45185u, count = 45302u - lba;
    const int32_t token = any_begin(lba, count);
    const uint32_t had = R.engine.cursor.done;
    CHECK(had && had < count && w->other_windows == 1u);
    any_finish(token, lba, count, false);
    CHECK(!c->movie_ahead.variant && w->large_returned == 1u && !w->large_requests);
    /* (The stream was refused the channel, which is the one error on record.) */
    CHECK(w->declined && w->first_error == KUI_WIRE_E_CHANNEL && !c->stats.abandoned);
    CHECK(w->drops[KUI_TOY_WIRE_DROP_OTHER] == 1u && !w->drops[KUI_TOY_WIRE_DROP_WAITING] && !w->n.starts);
    m.chcr = 0u;
    printf("  a read of %u sectors over the track's end that opened a window for itself and had %u sectors of it: "
           "the window given up at its next visit, back to the low reader whole, data exact\n", (unsigned)count,
        (unsigned)had);
}

/* What a waiting request asks of the wire is what it is still owed. A
 * request of 61 sectors at a window that holds 61: its own visit hands
 * over two slices, the window has the 29 that are owed, and nothing is
 * asked of the wire. (Reckoned by the whole request, 16 sectors more than
 * the window holds would be asked for.) And off the low reader's movie
 * path every call of the game's visits the reader: the rest comes by CHECK
 * alone, with no EXEC behind the request's own and no interrupt. */
static void any_owed(void) {
    any_filled();
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    m.delay = 100000u; /* no run lands, so no interrupt comes */
    const unsigned long heard = eris + timers;
    const int32_t token = any_begin(45020u, 61u);
    CHECK(R.engine.cursor.done == 2u * ANY_SLICE && R.shared.service.pending);
    /* (The visit also read what the ring still held into the room the
     * slices made: the window has the 29 owed and more.) */
    CHECK(c->movie_ahead.verified - c->movie_ahead.first >= 61u - 2u * ANY_SLICE && !wire_state()->want);
    int32_t status = KUI_GD_PROCESSING;
    unsigned asked = 0u; /* (not `checks`: the CHECK macro counts in a variable of that name) */
    while(status == KUI_GD_PROCESSING) {
        ++asked;
        CHECK(asked <= 2u);
        status = gd(KUI_GD_CHECK, (uint32_t)token, STATUS);
    }
    CHECK(status == KUI_GD_COMPLETED && get(STATUS + 8u) == 61u * KUI_GAME_DATA_BYTES && eris + timers == heard);
    any_compare(45020u, 61u);
    CHECK(w->large_requests == 1u && !w->large_returned && c->stats.hit_requests == 2u && !w->first_error);
    m.delay = 0u;
    printf("  a request of 61 sectors at a window that holds them: two slices by its own visit, nothing asked of "
           "the wire, the rest by %u CHECK call%s alone\n", asked, asked == 1u ? "" : "s");
}
#endif

#if KUI_TOY_WIRE_TITLES && KUI_TOY_WIRE_STAY
/* ---- Test 79: the window for every title ---- */
/* Another title. The stage found no Toy Commander: no file is known, no
 * hook is in, no call of the game's is classified, and the worker says so
 * in the report. Its disc has a raw track on the SCI card, which is all
 * the stage asked for. Evolution 2's refills, 73 and 50 sectors by turns:
 * the first the low reader's, the rest from one window. Nothing is counted
 * as a movie's, and the low reader's movie policy is never on. And the
 * same title on a disc the stage did not call a source: no window.
 *
 * Another title's SDK is not Toy Commander's: the table words of the two
 * spare timers hold that title's own default callback (Evolution 2's
 * here), so the stream takes no timer, writes nothing into that table,
 * and a run cut short waits its time by the game's clock before a call
 * of the game's begins it again. On a quiet bus, and on two that cut
 * runs short. */
#define OTHER_DEFAULT UINT32_C(0x8c1add0c)
/* Test 81: the entries of the two spare timers in another title's table,
 * and a handler of the title's own for them. */
enum { OTHER_TMU1 = 17, OTHER_TMU2 = 18 };
#define OTHER_HANDLER UINT32_C(0x8c123456)
#define OTHER_VBR UINT32_C(0x8c1ad400)
#define OTHER_RELOAD UINT32_C(0x33330000)
#define OTHER_COUNT UINT32_C(0x00004444)
#define OTHER_CONTROL 0x0001u
#if KUI_TOY_WIRE_TIMER
/* That title with its table as the low resident admitted it: the stream
 * has a timer there, unless the title uses both spare timers itself. */
static void other_table(bool used) {
    for(unsigned i = 0; i < 64u; ++i) wx.slots[i] = OTHER_DEFAULT;
    R.vector600[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX] = OTHER_DEFAULT;
    /* Its table lies at its own vector base, which is the live one and
     * the one the low resident admitted. */
    wire_vbr = OTHER_VBR;
    hw.vbr = OTHER_VBR;
    R.vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX] = OTHER_VBR;
    if(used) wx.slots[OTHER_TMU1] = wx.slots[OTHER_TMU2] = OTHER_HANDLER;
    other_timers_used = used;
    /* What the title has in its spare timers, stopped. */
    for(unsigned k = 1u; k < 3u; ++k) {
        wx.tcor[k] = OTHER_RELOAD + k; wx.tcnt[k] = OTHER_COUNT + k; wx.tcr[k] = (uint16_t)(OTHER_CONTROL + k);
    }
}
#define OTHER_TIMER (!other_timers_used)
#else
#define OTHER_TIMER false
#endif
/* The title's table as the stream may leave it: every entry the title's
 * own word, but for the timer's while a stream that took one runs. */
static void other_table_kept(const struct kui_toy_wire_report *w) {
    for(unsigned i = 0; i < 64u; ++i) {
        uint32_t word = OTHER_DEFAULT;
#if KUI_TOY_WIRE_TIMER
        if(other_timers_used && (i == OTHER_TMU1 || i == OTHER_TMU2)) word = OTHER_HANDLER;
        if(w->timer_mode && stream_on() && i == OTHER_TMU1 - 1u + w->timer_mode)
            word = (uint32_t)(uintptr_t)kui_toy_wire_timer_callback;
#else
        (void)w;
#endif
        CHECK(wx.slots[i] == word);
    }
#if KUI_TOY_WIRE_TIMER
    /* And its spare timers' reload, count and control, but for the one a
     * running stream has. */
    for(unsigned k = 1u; k < 3u; ++k)
        if(!(stream_on() && w->timer_mode == k))
            CHECK(wx.tcor[k] == OTHER_RELOAD + k && wx.tcnt[k] == OTHER_COUNT + k && wx.tcr[k] == OTHER_CONTROL + k);
#endif
}
static void titles_other(bool source, uint32_t mean, unsigned dense) {
    open_card(2000u, false, true, file_amid, false);
#if KUI_TOY_WIRE_TIMER
    other_table(other_title_busy);
#else
    for(unsigned i = 0; i < 64u; ++i) wx.slots[i] = OTHER_DEFAULT;
#endif
    stall_mean = mean; stall_dense = dense;
    wire_clock_shared = true;
    struct kui_native_ra_config config = movie_configuration();
    memset(config.movie_ranges, 0, sizeof(config.movie_ranges));
    config.toy_poll_owner = 0u;
    if(!source) config.suite_source = 0u;
    bind_movie_configuration(&config);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    const uint32_t *trace = c->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    CHECK(!c->demand.owner_verified);
    uint32_t lba = 45002u;
    static const uint32_t sizes[5] = {73u, 50u, 73u, 50u, 52u};
    for(unsigned i = 0u; i < 5u; lba += sizes[i++]) {
        any_read(lba, sizes[i], source && i);
        CHECK(!R.shared.stream.toy_suite_active && !R.shared.stream.toy_suite_poll);
        CHECK(!trace[0] && !trace[1] && !trace[2] && !trace[3]);
    }
    CHECK(lba == 45300u && !w->first_error && !c->stats.abandoned);
    if(OTHER_TIMER && source) {
        /* Test 81: the stream took the title's second spare timer by the
         * title's own table, and its waits were ended by it. */
        CHECK(w->timer_mode == 2u && (!mean || timers > 8u));
    } else {
        /* No timer was taken and none ever ran for the stream. */
        CHECK(!w->timer_mode && !timers);
    }
    /* The title's table is as it was, but for a running stream's timer. */
    other_table_kept(w);
    if(source && mean && OTHER_TIMER) {
        CHECK(w->n.waits > 8u && w->other_windows && c->stats.hit_requests && !w->drops[KUI_TOY_WIRE_DROP_ERROR]);
        printf("  another title, the stream's timer by the title's own table, stalls %u/%u: %u waits, %lu answers "
               "of the timer; %u requests of five from %u windows, %u streams begun; data exact\n",
            (unsigned)mean, dense, (unsigned)w->n.waits, timers, (unsigned)c->stats.hit_requests,
            (unsigned)w->other_windows, (unsigned)w->n.starts);
        return;
    }
    if(source && mean) {
        /* A bus that cuts runs short. Every wait but one that may still
         * stand lasted its time at least: none was ended by the look
         * that followed its beginning. */
        CHECK(w->n.waits > 8u && w->n.wait_ticks >= (w->n.waits - 1u) * KUI_WIRE_BACKOFF_MIN);
        if(getenv("WIRE_VERBOSE")) fprintf(stderr, "other title stalls %u/%u: windows %u hits %u drops %u %u %u %u %u %u waits %u wticks %u starts %u back %u big %u\n",
            (unsigned)mean, dense, (unsigned)w->other_windows, (unsigned)c->stats.hit_requests, (unsigned)w->drops[0], (unsigned)w->drops[1],
            (unsigned)w->drops[2], (unsigned)w->drops[3], (unsigned)w->drops[4], (unsigned)w->drops[5], (unsigned)w->n.waits,
            (unsigned)w->n.wait_ticks, (unsigned)w->n.starts, (unsigned)w->large_returned, (unsigned)w->large_requests);
        CHECK(w->other_windows && c->stats.hit_requests && !w->drops[KUI_TOY_WIRE_DROP_ERROR]);
        printf("  another title, no timer, stalls %u/%u: %u waits of %u ticks in all, none shorter than its time; "
               "%u requests of five from %u windows, %u streams begun; data exact\n", (unsigned)mean, dense,
            (unsigned)w->n.waits, (unsigned)w->n.wait_ticks, (unsigned)c->stats.hit_requests,
            (unsigned)w->other_windows, (unsigned)w->n.starts);
        return;
    }
    no_drops(w);
    if(source) {
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && c->stats.hit_requests == 4u);
        CHECK(w->other_sectors == 50u + 73u + 50u + 52u && w->large_requests == 1u && !w->large_returned);
        CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u);
        printf("  another title on a disc with a raw track, %s: Evolution 2's refills served "
               "from one window, four requests of five, nothing counted as a movie's, the movie policy never on; "
               "CMD18 %u, CMD12 %u\n", OTHER_TIMER ? "the stream's timer taken by the title's own table" :
            "no timer for the stream", card.cmd18, card.cmd12);
    } else {
        CHECK(!c->movie_ahead.variant && !stream_on() && !w->other_windows && !c->stats.hit_requests);
        CHECK(!w->n.starts && !w->large_requests && card.cmd18 == 1u && !card.cmd12);
        printf("  another title on a disc the stage did not call a source: no window, the low reader read all "
               "five requests in one card command\n");
    }
}
#endif
#endif

#if KUI_TOY_WIRE_QUERY && KUI_TOY_WIRE_STAY
/* A read by programmed transfer is a read as one by DMA is: the window is
 * opened by it and serves it, stands through it, and counts no command.
 * (The fixture's reads are by DMA everywhere else.) */
static void sequence_programmed(void) {
    open_card(2000u, false, true, file_amid, false);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    plain_read(45010u, 5u);
    for(unsigned i = 0u; i < 3u; ++i) {
        const uint32_t lba = 45015u + 5u * i;
        memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
        put(PARAM, lba + 150u); put(PARAM + 4u, 5u); put(PARAM + 8u, OUTPUT); put(PARAM + 12u, 0u);
        const int32_t token = gd(KUI_GD_REQUEST, KUI_GD_PIOREAD, PARAM);
        CHECK(token > 0 && R.shared.service.command == KUI_GD_PIOREAD);
        plain_finish(token, lba, 5u);
        CHECK(c->movie_ahead.variant == OTHER_WINDOW && stream_on() && w->other_windows == 1u &&
            c->stats.hit_requests == i + 1u);
    }
    no_drops(w);
    CHECK(card.cmd18 == 1u && !card.cmd12 && !w->queries && !w->command_seen[0] && !w->command_seen[1]);
    printf("  reads by programmed transfer: a window opened by one and three served from it, no command "
           "counted\n");
}

/* Test 80. Another title's reads with a command behind each, as Evolution
 * 2 has them on the console: its refills of 73 and 50 sectors, each
 * beginning where the last ended, and the drive asked for its subcode in
 * between. Asked, the window stands: every request behind the first comes
 * from the one window, the card has one command, and nothing is given up.
 * Told to stop instead, the window goes with every command, as Test 79's
 * did for either: a window to a request. On a quiet bus and on one that
 * cuts runs short. */
static void titles_asked(bool asks, uint32_t mean, unsigned dense) {
    open_card(2000u, false, true, file_amid, false);
#if KUI_TOY_WIRE_TIMER
    other_table(false);
#else
    for(unsigned i = 0; i < 64u; ++i) wx.slots[i] = OTHER_DEFAULT;
#endif
    stall_mean = mean; stall_dense = dense;
    wire_clock_shared = true;
    struct kui_native_ra_config config = movie_configuration();
    memset(config.movie_ranges, 0, sizeof(config.movie_ranges));
    config.toy_poll_owner = 0u;
    bind_movie_configuration(&config);
    const struct kui_toy_wire_report *w = report();
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    CHECK(!c->demand.owner_verified);
    uint32_t lba = 45002u;
    static const uint32_t sizes[5] = {73u, 50u, 73u, 50u, 52u};
    uint32_t longest[KUI_TOY_WIRE_VISITS + 1u] = {0u};
    for(unsigned i = 0u; i < 5u; lba += sizes[i++]) {
        any_read(lba, sizes[i], false);
        CHECK(R.shared.service.position_lba == lba + sizes[i] - 1u);
        if(asks) asked_command(KUI_RETAIL_GD_GETSCD, 1u, 14u, STATUS + 32u, 0u);
        else plain_command(KUI_GD_STOP, 0u);
        /* The drive is where the read left it either way. */
        CHECK(R.shared.service.position_lba == lba + sizes[i] - 1u);
        /* The game plays on for a few of its frames before it reads again. */
        for(unsigned n = 0u; n < 12u; ++n) if(!hardware()) CHECK(gd(KUI_GD_EXEC, 0, 0) == 0);
        /* What is kept as the longest visit never grows shorter. */
        for(unsigned who = 0u; who <= KUI_TOY_WIRE_VISITS; ++who) {
            const uint32_t now = who < KUI_TOY_WIRE_VISITS ? w->visit_longest[who] : w->deliver_longest;
            CHECK(now >= longest[who]);
            longest[who] = now;
        }
    }
    CHECK(lba == 45300u && !w->first_error && !c->stats.abandoned && !w->large_returned);
    /* The longest single pump of each caller is one of its pumps: no more
     * than their sum and no less than their mean. Up to Test 80 this
     * title's stream has no timer, and no pump of one. The longest
     * delivery visit likewise, of the game's disc calls. */
    for(unsigned who = 0u; who < KUI_TOY_WIRE_VISITS; ++who) {
        CHECK(w->visit_longest[who] <= w->visit_ticks[who] &&
            (uint64_t)w->visit_longest[who] * w->visit_calls[who] >= w->visit_ticks[who]);
        if(who != KUI_TOY_WIRE_VISIT_TIMER) CHECK(w->visit_longest[who]);
        else if(!KUI_TOY_WIRE_TIMER) CHECK(!w->visit_longest[who] && !w->visit_calls[who]);
        else CHECK(!w->visit_longest[who] == !w->visit_ticks[who] && (!mean || w->visit_calls[who]));
    }
    CHECK(w->deliver_longest && w->deliver_longest <= w->deliver_ticks &&
        (uint64_t)w->deliver_longest * w->deliver_calls >= w->deliver_ticks);
    CHECK(w->deliver_longest < w->deliver_ticks && w->visit_longest[KUI_TOY_WIRE_VISIT_GAME] <
        w->visit_ticks[KUI_TOY_WIRE_VISIT_GAME]);
    /* (On a bus that cuts runs short a read may wait longer than a window
     * is kept for it: that window is given up for the wait, as in any
     * build.) */
    CHECK(!w->drops[KUI_TOY_WIRE_DROP_ERROR] && !w->drops[KUI_TOY_WIRE_DROP_IDLE]);
    if(asks) {
        /* The first read is the low reader's; the first command finds no
         * window, and is none of the window's to count. (Nor is one that
         * comes when a window has been given up for a wait.) */
        CHECK(w->queries && w->queries <= 4u && !w->command_seen[0] &&
            w->command_seen[1] == UINT32_C(1) << (KUI_RETAIL_GD_GETSCD - 32u));
        CHECK(!w->drops[KUI_TOY_WIRE_DROP_COMMAND]);
        if(!mean) {
            no_drops(w);
            CHECK(w->queries == 4u);
            CHECK(c->movie_ahead.variant == OTHER_WINDOW && w->other_windows == 1u && c->stats.hit_requests == 4u);
            CHECK(w->other_sectors == 50u + 73u + 50u + 52u && card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u);
        } else CHECK(w->other_windows && c->stats.hit_requests);
        printf("  another title that asks the drive for its subcode behind every read, stalls %u/%u: %u commands "
               "a window stood through, none given up for one; %u requests of five from %u windows, CMD18 %u, "
               "CMD12 %u; data exact\n", (unsigned)mean, dense, (unsigned)w->queries,
            (unsigned)c->stats.hit_requests, (unsigned)w->other_windows, card.cmd18, card.cmd12);
    } else {
        /* A window opened by each read behind the first, and each given
         * up for the command behind it. */
        CHECK(!w->queries && !w->command_seen[0] && w->command_seen[1] == UINT32_C(1) << (KUI_GD_STOP - 32u));
        CHECK(w->drops[KUI_TOY_WIRE_DROP_COMMAND] == 4u && w->other_windows == 4u && !c->movie_ahead.variant &&
            !stream_on());
        CHECK(c->stats.hit_requests == 4u && card.cmd12 == 4u);
        printf("  the same with the drive told to stop behind every read: %u windows, each given up for the "
               "command, CMD18 %u, CMD12 %u\n", (unsigned)w->other_windows, card.cmd18, card.cmd12);
    }
}
#endif

/* The movie ends while a request waits on a running stream. The next SCI
 * interrupt visit stops the stream there and then, without a card command;
 * the CMD12 waits for the foreground, and the waiting request is then
 * completed by the low reader. */
static void ended_in_interrupt(void) {
    open_movie(2000u, false, true);
#if !KUI_TOY_WIRE_SEQ
    stall_mean = 900u;
#endif
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
#if KUI_TOY_WIRE_SEQ
    /* Test 77: nothing of the movie's end reaches the window. It serves
     * the request that waits, and stays. (On a quiet bus, so that the one
     * stream and the one card command can be told.) */
    uint32_t token = (uint32_t)request(FIRST, REQUEST, OUTPUT);
    CHECK(stream_on() && R.shared.service.pending && card.cmd18 == 1u && !card.cmd12);
    const struct kui_toy_wire_report *w = report();
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 20000u && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    CHECK(stream_on() && kui_native_readahead_aux_context.movie_ahead.variant == 4u && !card.cmd12);
    CHECK(kui_native_readahead_aux_context.stats.hit_requests == 1u);
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    compare(FIRST, REQUEST, OUTPUT);
    printf("  movie ended under a waiting request: the window served it and stayed, no card command\n");
    /* What an interrupt visit still stops there and then is a window in
     * error: as a pump leaves it that could not go on. */
    open_movie(2000u, false, true);
    stall_mean = 900u;
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    token = (uint32_t)request(FIRST, REQUEST, OUTPUT);
    CHECK(stream_on() && R.shared.service.pending && card.cmd18 == 1u && !card.cmd12);
    w = report();
    kui_native_readahead_aux_context.movie_ahead.error = true;
    const unsigned long before = eris;
    CHECK(hardware() && eris == before + 1u);
    CHECK(!stream_on() && R.shared.stream.state == 3u && !R.engine.hooked && R.shared.service.pending);
    /* The window is counted as given up when it is gone: by the call that
     * ends the card's command. */
    CHECK(!card.cmd12 && !w->drops[KUI_TOY_WIRE_DROP_ERROR] && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == 1u &&
        kui_native_readahead_aux_context.movie_ahead.variant);
    closed();
#else
    const uint32_t token = (uint32_t)request(FIRST, REQUEST, OUTPUT);
    CHECK(stream_on() && R.shared.service.pending && card.cmd18 == 1u && !card.cmd12);
    const struct kui_toy_wire_report *w = report();
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    const unsigned long before = eris;
    CHECK(hardware() && eris == before + 1u);
    CHECK(!stream_on() && R.shared.stream.state == 3u && !R.engine.hooked && R.shared.service.pending);
    CHECK(!card.cmd12 && w->drops[KUI_TOY_WIRE_DROP_ENDED] == 1u && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == 1u);
    closed();
#endif
    CHECK(!hardware()); /* nothing is left to interrupt */
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 20000u && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    CHECK(card.cmd12 == 1u && !kui_native_readahead_aux_context.movie_ahead.variant);
#if KUI_TOY_WIRE_SEQ
    CHECK(w->drops[KUI_TOY_WIRE_DROP_ERROR] == 1u);
#endif
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    compare(FIRST, REQUEST, OUTPUT);
#if KUI_TOY_WIRE_SEQ
    printf("  window in error under a waiting request: stream stopped in the interrupt visit, CMD12 later, read exact\n");
#else
    printf("  movie ended under a waiting request: stream stopped in the interrupt visit, CMD12 later, read exact\n");
#endif
}

#if KUI_TOY_WIRE_IDLE
/* A call of the game's that carries no request, or an interrupt visit
 * without one, finds the window's reason gone: the movie over, the last
 * request not completed, a command that is not a read, the bus lost, the low
 * reader mid-resume, the region lent out, or the stream beyond repair. The
 * window is given up, or left alone, exactly as Test 73 does it: each case is
 * one term of the condition under which Test 74's short way through the
 * dispatcher may be taken, and must not be. The line printed for each case
 * is the same in both builds. */
enum { GONE_ENDED, GONE_ENDED_INTERRUPT, GONE_FAILED, GONE_COMMAND, GONE_FAULT, GONE_CLOSED, GONE_RELEASED,
    GONE_RESUME, GONE_MARKED, GONE_LOAN, GONE_ERROR, GONE_CASES };
static void window_gone(unsigned what) {
    static const char *const names[GONE_CASES] = {"movie over, a call", "movie over, an interrupt",
        "last request failed", "a command that is not a read", "bus fault", "reader closed", "bus released",
        "low reader mid-resume", "window marked in error",
        "region lent out", "stream beyond repair"};
    open_movie(2000u, false, true);
    stall_mean = 900u; /* interrupts keep coming */
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    const struct kui_native_readahead_aux_context *c = &kui_native_readahead_aux_context;
    read_request(FIRST, REQUEST, 3u);
    read_request(FIRST + REQUEST, REQUEST, 3u);
    CHECK(stream_on() && c->movie_ahead.variant && !R.shared.service.pending && !c->stats.abandoned);
    for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
    const unsigned cmd12 = card.cmd12;
    uint32_t pumps = trace[4] + trace[5], next = FIRST + 2u * REQUEST;
    bool pumped = true;
    switch(what) {
    case GONE_ENDED:
        fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
        game_poll();
#if KUI_TOY_WIRE_SEQ
        /* Test 77: the movie's end is no reason. The window stays. */
        CHECK(stream_on() && c->movie_ahead.variant == 4u && card.cmd12 == cmd12);
        for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
#else
        CHECK(!stream_on() && !c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_ENDED] == 1u);
#endif
        break;
    case GONE_ENDED_INTERRUPT: {
        /* A run in flight, whose end will bring the interrupt. */
        m.delay = 100000u;
        for(unsigned n = 0u; n < 100000u && !m.rx; ++n) if(!hardware()) game_poll();
        CHECK(m.rx && stream_on() && c->movie_ahead.variant);
        pumps = trace[4] + trace[5];
        fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
        const unsigned long before = eris;
        for(unsigned n = 0u; n < 100000u && eris == before; ++n) CHECK(hardware());
        m.delay = 0u;
#if KUI_TOY_WIRE_SEQ
        /* Test 77: nor in an interrupt visit. */
        CHECK(stream_on() && card.cmd12 == cmd12);
        game_poll();
        CHECK(stream_on() && c->movie_ahead.variant == 4u && card.cmd12 == cmd12);
        for(unsigned i = 0; i < KUI_TOY_WIRE_DROPS; ++i) CHECK(!w->drops[i]);
#else
        /* Stopped there and then; the card command waits for the foreground. */
        CHECK(!stream_on() && card.cmd12 == cmd12 && w->drops[KUI_TOY_WIRE_DROP_ENDED] == 1u);
        game_poll();
        CHECK(!c->movie_ahead.variant);
#endif
        break;
    }
    case GONE_FAILED:
        R.shared.service.status = KUI_GD_FAILED;
        game_poll();
        CHECK(!stream_on() && !c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_IDLE] == 1u);
        break;
    case GONE_COMMAND:
        R.shared.service.command = GONE_BY_UNKNOWN;
        game_poll();
        CHECK(!stream_on() && !c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_COMMAND] == 1u);
#if KUI_TOY_WIRE_QUERY
        CHECK(!w->queries && !w->command_seen[0] && !w->command_seen[1]);
#endif
        R.shared.service.command = 0u;
        break;
    case GONE_FAULT:
        R.shared.port.fault = true;
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
        R.shared.port.fault = false;
        CHECK(!stream_on());
        break;
    case GONE_CLOSED:
        R.engine.opened = 0u;
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
        R.engine.opened = 1u;
        CHECK(!stream_on());
        break;
    case GONE_RELEASED:
        R.shared.port.acquired = false;
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
        R.shared.port.acquired = true;
        CHECK(!stream_on());
        break;
    case GONE_RESUME:
        R.shared.stream.state = 4u;
        CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
        CHECK(!stream_on() && w->drops[KUI_TOY_WIRE_DROP_ERROR] == 1u);
        break;
    case GONE_MARKED:
        /* As an interrupt visit leaves it that could not send the card
         * command to end with: the next call of the game's ends it. */
        kui_native_readahead_aux_context.movie_ahead.error = true;
        game_poll();
        CHECK(!stream_on() && !c->movie_ahead.variant && w->drops[KUI_TOY_WIRE_DROP_ERROR] == 1u);
        break;
    case GONE_LOAN:
        /* The resident makes no visit while the region is lent out; one
         * that came all the same must not touch the window. */
        R.engine.native_loan_state = KUI_VMU_NATIVE_ASYNC_LOAN_STOPPING;
        CHECK(kui_native_readahead_aux_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, 0u) == 1u);
        R.engine.native_loan_state = KUI_VMU_NATIVE_ASYNC_LOAN_IDLE;
        CHECK(trace[4] + trace[5] == pumps && stream_on() && c->movie_ahead.variant);
        pumped = false;
        break;
    default: {
        /* A block ahead that neither the stream nor the low reader can
         * read: every try ends at its CRC16, and after eight in a row with
         * nothing gained the window is given up on a visit that carries no
         * request. */
        /* Behind the next two requests, which make room for the stream to
         * read on into it while nothing is asked, and beyond what the card
         * has already sent into the ring. */
        const uint32_t unreadable = next + 2u * REQUEST + 12u;
        CHECK(c->movie_ahead.verified + 16u < unreadable);
        unsigned offset;
        card.corrupt_lba = raw_block(unreadable, 100u, &offset);
        card.corrupt_count = 100000u;
        /* A byte of the sector's user data, changed by more than the low
         * reader can put right from the CRC16. */
        card.corrupt_custom = true; card.corrupt_index = offset; card.corrupt_mask = 0xffu;
        for(unsigned i = 0; i < 2u; ++i, next += REQUEST) read_request(next, REQUEST, 3u);
        CHECK(!R.shared.service.pending && c->movie_ahead.variant);
        for(unsigned n = 0u; n < 200000u && c->movie_ahead.variant; ++n) if(!hardware()) game_poll();
        if(getenv("WIRE_VERBOSE")) fprintf(stderr, "beyond repair: variant %u on %d abandoned %u drops %u %u %u %u %u %u stops %u %u %u %u %u left %u verified %u first %u\n",
            (unsigned)c->movie_ahead.variant, stream_on(), (unsigned)c->stats.abandoned, (unsigned)w->drops[0], (unsigned)w->drops[1],
            (unsigned)w->drops[2], (unsigned)w->drops[3], (unsigned)w->drops[4], (unsigned)w->drops[5], (unsigned)w->stops[0],
            (unsigned)w->stops[1], (unsigned)w->stops[2], (unsigned)w->stops[3], (unsigned)w->stops[4], (unsigned)card.corrupt_count,
            (unsigned)c->movie_ahead.verified, (unsigned)c->movie_ahead.first);
        CHECK(!c->movie_ahead.variant && !stream_on() && c->stats.abandoned == 1u);
        card.corrupt_count = 0u;
        card.corrupt_mask = 0u; card.corrupt_custom = false;
        break;
    }
    }
    /* How many streams were tried before the last case gave up depends on
     * where sectors happened to complete: at least the eight in a row. */
    if(what == GONE_ERROR) CHECK(card.cmd12 - cmd12 >= 8u);
    printf("  the window's reason gone (%s): stream %s, window %s, given up as error/over/idle/command/waiting/other "
           "%u/%u/%u/%u/%u/%u and on a pump's error %u, CMD12 %s, %s\n", names[what], stream_on() ? "on" : "off",
        c->movie_ahead.variant ? "kept" : "gone", (unsigned)w->drops[0], (unsigned)w->drops[1], (unsigned)w->drops[2],
        (unsigned)w->drops[3], (unsigned)w->drops[4], (unsigned)w->drops[5], (unsigned)c->stats.abandoned,
        what == GONE_ERROR ? "at least 8" : card.cmd12 == cmd12 ? "none" : card.cmd12 == cmd12 + 1u ? "once" : "more than once",
        pumped && trace[4] + trace[5] == pumps ? "no pump" : pumped ? "pumped" : "not pumped");
    if(what == GONE_LOAN) return;
    /* The movie's reads go on, by whichever way. */
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
    R.shared.service.status = KUI_GD_COMPLETED;
    read_request(next, REQUEST, 3u);
    read_request(next + REQUEST, REQUEST, 3u);
}
#endif

#if KUI_TOY_WIRE_MSB
/* The bit order is asked of the receiver by looking at a stream's first
 * token. A stream that loses a byte ahead of that token is given up without
 * an answer, and after four of them the question is: the stream then runs as
 * Test 73's does, whatever the receiver could have done, and the bit is not
 * written again. */
static void order_given_up(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    /* Idle bytes between blocks: the byte lost is one of them and the
     * token itself arrives, behind it. */
    card.nac = 5u;
    first_cuts = 5u;
    uint32_t lba = FIRST;
    for(unsigned i = 0; i < 4u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    /* Four streams given up; the fifth no longer asks, and its lost byte
     * is one like any other. */
    if(getenv("WIRE_VERBOSE")) fprintf(stderr, "order given up: first_cuts %u undecided %u order %u lost_frame %u starts %u\n",
        first_cuts, (unsigned)w->n.undecided, (unsigned)w->n.order, (unsigned)w->n.lost_frame, (unsigned)w->n.starts);
    CHECK(!first_cuts && w->n.undecided == 4u && w->n.order == KUI_WIRE_ORDER_LSB && w->n.lost_frame == 4u);
    CHECK(w->n.starts == 5u && stream_on() && !m.scmr);
    const unsigned long asked = sdir_writes;
    for(unsigned i = 0; i < 4u; ++i, lba += REQUEST) read_request(lba, REQUEST, 3u);
    CHECK(sdir_writes == asked && w->n.order == KUI_WIRE_ORDER_LSB && (!KUI_TOY_WIRE_CLEAN || w->light));
    printf("  four streams in a row that lose a byte ahead of their first token: the bit order is no longer asked, "
           "data exact\n");
}
/* A token that came MSB first says so before any block has passed its
 * CRC16 that way. Until one has, a single lost byte is not rebuilt from a
 * CRC16 taken in that order: the block goes on with the byte listed, for
 * its sector's parity and EDC. Where the token came the old way the order
 * is known, and the byte is rebuilt as ever. */
static void order_unconfirmed(void) {
    open_movie(2000u, false, true);
    const struct kui_toy_wire_report *w = report();
    early_cut = true;
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(FIRST, REQUEST, OUTPUT);
    for(unsigned n = 0u; !w->n.blocks; ++n) {
        CHECK(n < 200000u && R.shared.service.pending && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    /* The first block of the first stream is the one with the lost byte. */
    CHECK(!early_cut && w->n.starts == 1u && w->n.holed == (sdir_answer ? 1u : 0u) && w->n.rebuilt >= (sdir_answer ? 0u : 1u));
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 200000u && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    compare(FIRST, REQUEST, OUTPUT);
    CHECK(w->n.order == (sdir_answer ? KUI_WIRE_ORDER_MSB : KUI_WIRE_ORDER_LSB) && !w->check_bad);
    printf("  a byte lost in the first stream's first block: %s\n", sdir_answer ?
        "not rebuilt while the bit order was unconfirmed, filled from the sector's parity, data exact" :
        "rebuilt, the bit order known from the token");
}
#endif

/* No free timer, no interrupt route, no free channel. */
static void degraded(void) {
    open_movie(2000u, false, true);
    stall_mean = 400u; stall_dense = 40u;
    wx.tstr = 7u; /* both spare timers run for the game */
    uint32_t lba = FIRST;
    for(unsigned i = 0; i < 6u; ++i, lba += REQUEST) read_request(lba, REQUEST, 2u);
    const struct kui_toy_wire_report *w = report();
    CHECK(!w->timer_mode && w->n.waits && !timers && !w->first_error);
#if KUI_TOY_WIRE_PACE
    /* A wait that no timer ends is over at the next visit, so an update
     * call that finds one is passed on at once, whatever the spacing. */
    {
        /* The stream's state lies behind the window's 62 raw sectors. */
        const struct kui_wire_stream *s = (const void *)(arena + 62u * 2352u);
        bool seen = false;
#if KUI_TOY_WIRE_TITLES
        bool held = false;
#endif
        for(unsigned n = 0u; n < 12u && !seen; ++n, lba += REQUEST) {
            read_request(lba, REQUEST, 2u); /* room in the window */
            /* A wait is left standing by a visit whose last look at the
             * stream began it: one of the request's own calls, or an
             * interrupt visit that read. (With Test 75's LATER an
             * interrupt visit that leaves the reading ends the wait it
             * began: later_waits().) */
            for(unsigned k = 0u; k < 4000u && !seen; ++k) {
                if(!stream_on() || s->state != KUI_WIRE_WAIT) {
                    if(!hardware()) break;
                    continue;
                }
#if KUI_TOY_WIRE_TITLES
                /* Test 79: the wait lasts its time by the game's clock. A
                 * look before that leaves it standing; after it the wait
                 * is due, and the next call ends it as before. */
                CHECK(!s->mode);
                if(!kui_wire_overdue(s)) {
                    kui_wire_service((struct kui_wire_stream *)(uintptr_t)s);
                    CHECK(s->state == KUI_WIRE_WAIT && stream_on());
                    held = true;
                    wx.tcnt0 -= 2u * KUI_WIRE_BACKOFF_MAX;
                }
                CHECK(kui_wire_overdue(s));
#else
                CHECK(!s->mode && kui_wire_overdue(s));
#endif
                const unsigned long gate = gate_visits;
                const uint32_t work = w->update_work, due = w->update_due;
                /* The run the visit starts stays in flight, so that the
                 * wait is seen to be over and not followed by another. */
                m.delay = 100000u;
                player_update();
                m.delay = 0u;
                CHECK(gate_visits == gate + 1u && w->update_work == work + 1u && w->update_due == due);
                CHECK(s->state != KUI_WIRE_WAIT);
                invariants();
                seen = true;
            }
        }
        CHECK(seen && !w->timer_mode && !timers && !w->first_error);
#if KUI_TOY_WIRE_TITLES
        CHECK(held);
#endif
    }
#endif

    open_movie(2000u, false, true);
    m.chcr = 1u; /* the game uses DMA channel 1 */
    lba = FIRST;
    for(unsigned i = 0; i < 3u; ++i, lba += REQUEST) read_request(lba, REQUEST, 1u);
    w = report();
    CHECK(w->declined && !w->n.starts && w->first_error == KUI_WIRE_E_CHANNEL && m.chcr == 1u);
    CHECK(w->seek_blocks >= 3u * REQUEST * 4u && !timers);
#if KUI_TOY_WIRE_PACE
    printf("  no free timer: waits end at the next visit%s, and an update call that finds one is passed on; "
           "channel 1 taken: programmed reads only; data exact\n",
        KUI_TOY_WIRE_TITLES ? " after their time, and a look before it leaves one standing" : "");
#else
    printf("  no free timer: waits end at the next visit; channel 1 taken: programmed reads only; data exact\n");
#endif
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    for(unsigned i = 0; i < 256u; ++i) {
        unsigned j = i << 1;
        if(j & 0x100u) j ^= 0x11du;
        ecc_f[i] = (uint8_t)j;
        ecc_b[i ^ j] = (uint8_t)i;
    }
#if KUI_TOY_WIRE_STEADY
    /* Test 75 prints nothing of the chunk observer and has it do nothing:
     * no read of the game's memory, no count, whatever the hook hands it. */
    {
        setup_movie_card(1u, 48u);
        struct kui_native_ra_config configuration = movie_configuration();
        configuration.toy_poll_owner = 1u;
        bind_movie_configuration(&configuration);
        chunk_name("60hz_Intro_LRTF.avi");
        chunk_word_reads = 0u;
        kui_toy_movie_chunk_observe(PLAYER, 0);
        kui_toy_movie_chunk_observe(PLAYER, -2);
        (void)movie_chunk_classification; /* Test 66's own check of what the observer no longer does */
        const struct kui_toy_movie_chunk_trace *trace = kui_native_readahead_aux_context.movie_chunk;
        CHECK(!chunk_word_reads && !trace[1].ready && !trace[1].wait_gd && !trace[1].wait_no_gd && !trace[1].other);
    }
#else
    /* Test 66's own chunk classification is untouched by the switch. */
    movie_chunk_classification();
#endif
    printf("Test %u reader, real low reader + auxiliary + stream on the card model:\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_TIMER ? 81u : KUI_TOY_WIRE_QUERY ? 80u : KUI_TOY_WIRE_TITLES ? 79u : KUI_TOY_WIRE_ANY ? 78u : KUI_TOY_WIRE_SEQ ? 77u : KUI_TOY_WIRE_FLUID ? 76u : KUI_TOY_WIRE_STEADY ? 75u : KUI_TOY_WIRE_LEAN ? 74u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u);
#if KUI_TOY_WIRE_MSB
    /* Everything twice: with a receiver that ignores the bit order asked
     * of it, and with one that delivers bytes MSB first. */
    for(sdir_answer = 0u; sdir_answer < 2u; ++sdir_answer) {
    printf(" the receiver %s SCSCMR1.SDIR:\n", sdir_answer ? "honours" : "ignores");
#endif
    steady(2000u, false, 0u, 0u, 3u);
    steady(2000u, false, 0u, 0u, 0u);
    steady(2000u, false, 6000u, 0u, 3u);
    steady(2000u, false, 1500u, 20u, 2u);
    steady(2000u, false, 500u, 50u, 1u);
    steady(2000u, false, 150u, 80u, 4u);
    steady(28u, true, 0u, 0u, 3u);
    steady(28u, true, 6000u, 5u, 2u);
    /* The same with the reader at the console's pace. */
    reader_pace(70u);
    steady(2000u, false, 0u, 0u, 3u);
    steady(2000u, false, 1500u, 20u, 2u);
    steady(2000u, false, 150u, 80u, 4u);
    steady(28u, true, 6000u, 5u, 2u);
    reader_pace(0u);
    const unsigned at_cost = playing(0u, 0u, 70u);
    (void)playing(6000u, 0u, 70u);
    (void)playing(1500u, 20u, 70u);
#if !KUI_TOY_WIRE_TICK
    /* What a cheaper reader does to the same run, with nothing else
     * changed: at half the cost an interrupt visit reads eleven of a full
     * run's sixteen blocks instead of six, at a third all of them. This is
     * the model's arithmetic, not a measurement of any build. */
    const unsigned at_half = playing(0u, 0u, 35u), at_third = playing(0u, 0u, 23u);
    CHECK(at_half >= at_cost && at_third >= at_half && at_third >= 19u);
#else
    (void)at_cost;
#endif
#if KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_ANY
    course_pieces();
#endif
    updating(0u, 0u);
    updating(6000u, 0u);
    updating(1500u, 20u);
#if KUI_TOY_WIRE_PACE
    polling(0u, 0u);
    polling(6000u, 0u);
    polling(1500u, 20u);
    overdue();
#if KUI_TOY_WIRE_LATER
    later_visits();
    later_standstill();
    later_waits(true);
    later_waits(false);
    later_fault();
#endif
    waited_for();
#if KUI_TOY_WIRE_STAY
    waited_in_vain();
    waited_slowly();
    waited_elsewhere(true);
    waited_elsewhere(false);
#endif
#endif
#if KUI_TOY_WIRE_IDLE
    waiting();
    player_measured();
#if KUI_TOY_WIRE_STEADY
    first_skip_apart();
#endif
#if KUI_TOY_WIRE_FLUID && !KUI_TOY_WIRE_ANY
    buffer_seen();
    pause_placed();
#endif
#if KUI_TOY_WIRE_COPY
    misaligned();
#endif
#if KUI_TOY_WIRE_QUIT
    quit_foreign(false);
    quit_foreign(true);
#endif
#endif
    damaged_block();
    wrong_blocks();
#if KUI_TOY_WIRE_CLEAN
    stored_wrong();
#endif
    no_parity();
    other_read();
#if KUI_TOY_WIRE_SEQ
    sequence_plain();
    sequence_track_end(false);
    sequence_waited();
    for(unsigned polls = 0u; polls < 3u; ++polls) sequence_pending(polls);
#if KUI_TOY_WIRE_STAY
    /* These have ordinary reads that wait for the stream, by a caller the
     * reader does not know. Without STAY such a read gives its window up
     * with its second call, as sequence_waited() shows, and what they say
     * of windows kept does not hold. */
    sequence_scattered(0u, 0u);
    sequence_scattered(1500u, 20u);
    sequence_scattered(150u, 80u);
    sequence_file();
    sequence_commands();
    sequence_interrupt();
    sequence_lent(false);
    sequence_lent(true);
    sequence_stop_failed();
    sequence_track_end(true);
#endif
    /* (Test 79 has a window in another title: titles_other() below.) */
    for(unsigned what = 0u; what < 4u; ++what) if(!KUI_TOY_WIRE_TITLES || what != 1u) sequence_nowhere(what);
    sequence_movie();
#if KUI_TOY_WIRE_ANY && KUI_TOY_WIRE_STAY
    any_evolution();
    any_long(0u, 0u, 0u);
    any_long(1500u, 20u, 0u);
    any_long(150u, 80u, 0u);
    any_long(0u, 0u, 70u);
    any_long(1500u, 20u, 70u);
    any_returned(false);
    any_returned(true);
    any_waited();
    any_in_vain();
    any_track_end();
    any_owed();
#endif
#if KUI_TOY_WIRE_TITLES && KUI_TOY_WIRE_STAY
    titles_other(true, 0u, 0u);
    titles_other(true, 1500u, 20u);
    titles_other(true, 150u, 80u);
    titles_other(false, 0u, 0u);
#if KUI_TOY_WIRE_TIMER
    /* Test 81: the same title using both spare timers itself. The stream
     * has none there, and what Test 79 says of a stream without a timer
     * holds again. */
    other_title_busy = true;
    titles_other(true, 0u, 0u);
    titles_other(true, 1500u, 20u);
    titles_other(true, 150u, 80u);
    other_title_busy = false;
#endif
#endif
#if KUI_TOY_WIRE_QUERY && KUI_TOY_WIRE_STAY
    sequence_programmed();
    titles_asked(true, 0u, 0u);
    titles_asked(true, 1500u, 20u);
    titles_asked(true, 150u, 80u);
    titles_asked(false, 0u, 0u);
#endif
#endif
    ended_in_interrupt();
#if KUI_TOY_WIRE_IDLE
    for(unsigned what = 0; what < GONE_CASES; ++what) window_gone(what);
#endif
#if KUI_TOY_WIRE_MSB
    order_given_up();
    order_unconfirmed();
#endif
    degraded();
#if KUI_TOY_WIRE_MSB
    }
#endif
#if KUI_TOY_WIRE_RING
    CHECK(ring_drops > 100000u);
    printf("  the ring read through the cache in all of the above: %lu lines dropped and fetched afresh\n", ring_drops);
#endif
#if KUI_TOY_WIRE_COPY
    /* Everywhere else every piece went through it. */
    CHECK(copies > 100000u);
    printf("  blocks copied into the window by the word copy's stand-in in all of the above: %lu pieces, %lu bytes\n",
        copies, copied);
#endif
#if KUI_TOY_WIRE_FLUID && !KUI_TOY_WIRE_ANY
    /* The wire's standstill was placed in the course wherever a run had waits. */
    CHECK(pause_seen > 100000u);
    printf("  the wire's standstill for timer waits, placed in the course's pieces in all of the above: %llu ticks\n",
        (unsigned long long)pause_seen);
#endif
    printf("PASS Test %u reader integration: %u checks\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_TIMER ? 81u : KUI_TOY_WIRE_QUERY ? 80u : KUI_TOY_WIRE_TITLES ? 79u : KUI_TOY_WIRE_ANY ? 78u : KUI_TOY_WIRE_SEQ ? 77u : KUI_TOY_WIRE_FLUID ? 76u : KUI_TOY_WIRE_STEADY ? 75u : KUI_TOY_WIRE_LEAN ? 74u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u, checks);
    return 0;
}
