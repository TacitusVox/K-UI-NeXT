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

#define KUI_TOY_SDK_LIVENESS_MAIN toy_retained_sdk_tests
#include "test_toy_sdk_liveness.c"

#if !KUI_TOY_WIRE_STREAM
#error "This fixture is the Test 68 reader's"
#endif

#define RING_PHYSICAL 0x0c300000u
enum { RING_BYTES = 32768u, FIRST = MOVIE_LBA, REQUEST = 13u, WINDOW_END = 45291u };
void kui_toy_wire_timer_callback(uint32_t event);
void kui_toy_movie_present_boundary(uint32_t ctx, uintptr_t witness, unsigned phase);
#define PLAYER UINT32_C(0x8c00dc90)
static unsigned long gate_visits;
#if KUI_TOY_WIRE_IDLE
void kui_toy_movie_ahead_update(uint32_t ctx, uint32_t entered);
/* The player's memory as Test 73's hook reads it: its output and skip
 * counters, its lag counter, and its buffer behind three pointers. */
static struct { uint32_t out, skip, lag, capacity, released, filled, stream; } player;
#define PLAYER_STREAM UINT32_C(0x8c200000)
#define PLAYER_CHANNEL UINT32_C(0x8c200100)
#define PLAYER_AVI UINT32_C(0x8c200200)
#define PLAYER_LAG UINT32_C(0x8c1138c4)
static bool player_word(uint32_t address, uint32_t *value) {
    switch(address) {
    case PLAYER + 0xacu: *value = player.out; return true;
    case PLAYER + 0x90u: *value = player.skip; return true;
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
/* The reader looks at its clock once for every block it reads. With no
 * extra cost a visit reads all there is; 70 ticks a look is the console's
 * measured pace of about six blocks to an interrupt visit's 390 ticks. */
static void reader_pace(uint32_t ticks) {
    refill_clock_extra_ticks = ticks;
    clock_step = ticks ? ticks : 5u;
}
static const struct kui_toy_wire_report *report(void) {
    return (const void *)(arena + KUI_TOY_WIRE_REPORT_OFFSET);
}
/* The first word behind the report is the stream's "on" flag. */
static bool stream_on(void) {
    uint32_t on;
    memcpy(&on, arena + KUI_TOY_WIRE_REPORT_OFFSET + sizeof(struct kui_toy_wire_report), sizeof(on));
    return on != 0u;
}
uint32_t kui_wire_test_read(uint32_t address, unsigned width) {
    if(address == 0xffd00004u) return wx.ipra;
    if(address == 0xffd80004u) return wx.tstr;
    if(address == 0xffd8000cu) return wx.tcnt0 -= clock_step;
    if(address >= 0xffd80008u && address < 0xffd8002cu) {
        const unsigned k = (address - 0xffd80008u) / 12u, field = (address - 0xffd80008u) % 12u;
        return field == 0u ? wx.tcor[k] : field == 4u ? wx.tcnt[k] : wx.tcr[k];
    }
    if(address >= 0x8c00f600u && address < 0x8c00f700u) return wx.slots[(address - 0x8c00f600u) / 4u];
    return kui_sci_stream_test_read(address, width);
}
void kui_wire_test_write(uint32_t address, uint32_t value, unsigned width) {
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
    if(address >= 0x8c00f600u && address < 0x8c00f700u) {wx.slots[(address - 0x8c00f600u) / 4u] = value; return;}
    kui_sci_stream_test_write(address, value, width);
}
void kui_wire_test_settle(unsigned count) { kui_sci_stream_test_settle(count); }
void kui_wire_test_fence(void) { kui_sci_stream_test_fence(); }
uint32_t kui_wire_test_physical(const void *area) {
    CHECK(area == arena + sizeof(arena) - RING_BYTES);
    return RING_PHYSICAL;
}
uint32_t kui_wire_test_vbr(void) { return KUI_NATIVE_SDK_SCI_OWNER_VBR; }
void kui_wire_test_purge(const void *area, unsigned bytes) {
    CHECK(area == arena + sizeof(arena) - RING_BYTES && bytes == RING_BYTES);
}
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
static bool stall(void) {
    static bool held;
    /* Only the stream's runs: the low reader's own block receptions keep
     * the shared model's own overrun injection. */
    if(!stall_mean || m.dar < RING_PHYSICAL || m.dar > RING_PHYSICAL + RING_BYTES) return false;
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
/* No programmed bus byte may be clocked while the stream owns the SCI. */
static void bus_check(void) { CHECK(!stream_on()); }

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
static void install_raw_sector(uint32_t lba, bool with_parity) {
    uint8_t raw[KUI_GAME_RAW_BYTES];
    const struct kui_retail_track *track = &manifest.slots[2].track;
    const uint32_t first = (lba - track->start_lba) * KUI_GAME_RAW_BYTES;
    for(unsigned i = 0u; i < sizeof(raw); ++i) raw[i] = source(2u, first + i);
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
            const struct kui_retail_extent *x = &manifest.slots[track->first_extent + e].extent;
            if(file_block >= x->file_block && file_block - x->file_block < x->blocks) extent = x;
        }
        CHECK(extent);
        image_card[(extent->card_lba + file_block - extent->file_block) * 512u + (first + i) % 512u] = raw[i];
    }
}

/* ---- the console's side ---- */
static unsigned long eris, timers, polls;
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
static void open_movie(unsigned extent_blocks, bool scattered, bool with_parity) {
    setup_movie_card_extents(1u, extent_blocks, scattered);
    for(uint32_t lba = manifest.slots[2].track.start_lba; lba < manifest.slots[2].track.end_lba; ++lba)
        install_raw_sector(lba, with_parity);
    struct kui_native_ra_config config = movie_configuration();
    config.movie_ranges[3] = (struct kui_toy_movie_range){FIRST - 386u, WINDOW_END - (FIRST - 386u)};
    bind_movie_configuration(&config);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
    /* The player has started the second movie: its presentation clock runs
     * from here, and with it the report's course of that movie. */
    kui_toy_movie_present_boundary(PLAYER,
        (uintptr_t)&kui_native_readahead_aux_context.movie_exit[1], KUI_TOY_MOVIE_EXIT_STARTED);
    /* The intro's quiet opening phase is over: the window may serve. */
    kui_native_readahead_aux_context.onset[1].quiet_ctx = UINT32_MAX;
    memset(&wx, 0, sizeof(wx));
    for(unsigned i = 0; i < 64u; ++i) wx.slots[i] = UINT32_C(0x8c0be408);
    wx.tcnt0 = UINT32_C(0x60000000);
    m.extra = arena + sizeof(arena) - RING_BYTES;
    m.extra_base = RING_PHYSICAL;
    m.extra_bytes = RING_BYTES;
    m.stall_hook = stall;
    m.bus_check = bus_check;
    m.delay = 0u;
    stall_mean = 0u; stall_dense = 0u; stall_left = 0u; burst_left = 0u; stall_seed = 4242u;
    eris = timers = polls = 0;
#if KUI_TOY_WIRE_IDLE
    /* A megabyte of buffer, three quarters full. */
    memset(&player, 0, sizeof(player));
    player.stream = PLAYER_STREAM;
    player.capacity = 0x100000u; player.released = 0x4000u; player.filled = 0x4000u + 0xc0000u;
    chunk_word_other = player_word;
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
#if KUI_TOY_WIRE_MEASURE
/* The report's account of the reader's visits agrees with the console's side. */
static void visits_accounted(void) {
    const struct kui_toy_wire_report *w = report();
    const uint32_t *trace = kui_native_readahead_aux_context.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_SCI] == trace[5] && w->visit_calls[KUI_TOY_WIRE_VISIT_GAME] == trace[4]);
    CHECK(w->visit_calls[KUI_TOY_WIRE_VISIT_TIMER] == timers);
    for(unsigned i = 0; i < KUI_TOY_WIRE_VISITS; ++i) {
        CHECK(w->visit_max[i] <= w->visit_ticks[i]);
        CHECK(!w->visit_calls[i] == !w->visit_ticks[i]);
    }
    /* A timer call was asked for only by a visit that ran out of time, and
     * only Test 69 asks. */
    CHECK(w->n.ticks <= w->budget_ends && (KUI_TOY_WIRE_TICK || !w->n.ticks));
    for(unsigned i = 0; i < 2u; ++i) CHECK(w->wait_max[i] <= w->wait_ticks[i] && !w->wait_calls[i] == !w->wait_ticks[i]);
    CHECK(!w->wait_calls[0]);
    const uint32_t restarts = w->stops[KUI_TOY_WIRE_STOP_SECTOR] + w->stops[KUI_TOY_WIRE_STOP_FRAME] +
        w->stops[KUI_TOY_WIRE_STOP_FAULT];
#if KUI_TOY_WIRE_IDLE
    /* Test 73 does not time restarts or place them in the course. */
    CHECK(!w->restart_ticks && !w->restart_max);
#else
    CHECK(w->restart_max <= w->restart_ticks && (restarts || !w->restart_ticks));
#endif
#if KUI_TOY_WIRE_WORDS
    /* The second movie's course adds up to the same totals: everything in
     * these runs happens while it plays. */
    uint32_t wait = 0, interrupt = 0, game = 0, sectors = 0, stops = 0;
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) {
        wait += w->course_wait[i]; interrupt += w->course_interrupt[i]; game += w->course_game[i];
        sectors += w->course_sectors[i]; stops += w->course_stops[i];
    }
    CHECK(wait == w->wait_ticks[1] && game == w->visit_ticks[KUI_TOY_WIRE_VISIT_GAME]);
    CHECK(interrupt == w->visit_ticks[KUI_TOY_WIRE_VISIT_SCI] + w->visit_ticks[KUI_TOY_WIRE_VISIT_TIMER]);
    CHECK(sectors == trace[1] && stops == (KUI_TOY_WIRE_IDLE ? 0u : restarts));
#endif
}
#else
static void visits_accounted(void) {}
#endif
static void closed(void) {
    /* The SCI is the low reader's again, in its command framing. */
    CHECK(!stream_on() && m.scr == 0x30u && m.smr == 0x80u && m.brr == 0u && m.cs_high);
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
    CHECK(w->parity == 1u && w->timer_mode == 2u);
    CHECK(w->n.cuts + w->n.segments >= 1u && w->n.blocks >= 1u);
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
    if(!mean && pace >= 70u) CHECK(w->budget_ends && !w->n.ticks && w->wait_calls[1] >= requests / 2u);
    CHECK(w->wait_calls[1] >= requests - warm);
#endif
    /* The stall the console showed. On a quiet bus nearly all the data
     * comes in full runs, an interrupt visit reads a third of each, and
     * the stream stands still for room until the game's next call: no
     * frame finds its data ready. */
    if(!mean && pace >= 70u) CHECK(ready * 2u < requests && w->n.fulls >= requests);
    printf("  playing, stall every ~%u bytes, %u%% busy phases, %u ticks a block, without the timer's help: data "
           "ready for %u of %u frames when asked; fulls %u, SCI visits %lu, timer visits %lu\n", (unsigned)mean, dense,
        (unsigned)pace, ready, requests, (unsigned)w->n.fulls, eris, timers);
#endif
    (void)warm;
    return ready;
}

#if KUI_TOY_WIRE_WORDS
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
    CHECK(w->deliver_calls >= idle + requests && w->deliver_ticks);
    CHECK(w->deliver_calls <= polls + 4u * (requests + 1u));
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
    CHECK(!intro->out && !intro->skip && w->update_ticks == 100u + step);
    CHECK(w->buffer_capacity == 0x100000u && w->course_free[0] == 0x40000u && w->course_lag[0] == 3u);
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
    /* A frame skipped in a short one. */
    ++player.skip;
    player_update_for(300u);
    CHECK(intro->skip == 1u && intro->skip_ticks == 300u + step && intro->out == 1u);
    /* Neither: a call that only waited. */
    player_update_for(50u);
    CHECK(intro->out == 1u && intro->skip == 1u);
    /* Both counters moved: the call put a frame out. */
    ++player.out; ++player.skip;
    player_update_for(700u);
    CHECK(intro->out == 2u && intro->out_ticks == 20700u + 2u * step && intro->skip == 1u);
    CHECK(intro->out_interrupt == between && intro->skip_ticks == 300u + step);
    /* From the call before: its own 700 ticks and the few looks at the
     * clock since. */
    CHECK(intro->out_cycle - cycle >= 700u && intro->out_cycle - cycle < 900u);
    CHECK(w->update_ticks == 100u + 20000u + 300u + 50u + 700u + 5u * step);
    /* The buffer's emptiest moment in the piece is kept, and the lag's
     * highest; a fuller buffer and a lower lag afterwards change nothing. */
    player.filled -= 0x80000u; player.lag = 9u;
    player_update_for(10u);
    CHECK(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
    player.filled += 0xa0000u; player.lag = 2u;
    player_update_for(10u);
    CHECK(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
    /* A buffer that cannot be one is left out: no capacity, more in it
     * than it holds, a pointer outside the game's memory. */
    const uint32_t ticks = w->update_ticks;
    player.capacity = 0u; player.filled = player.released;
    player_update_for(10u);
    player.capacity = 0x100000u; player.filled = player.released + 0x100001u;
    player_update_for(10u);
    player.filled = player.released; player.stream = UINT32_C(0x8c000004);
    player_update_for(10u);
    CHECK(w->course_free[0] == 0xc0000u && w->buffer_capacity == 0x100000u);
    CHECK(w->update_ticks == ticks + 3u * (10u + step));
    /* Later pieces of the course are their own. */
    player.stream = PLAYER_STREAM; player.filled = player.released + 0xf0000u; player.lag = 1u;
    clock_counter -= UINT32_C(3) << KUI_TOY_WIRE_BUCKET_SHIFT;
    player_update_for(10u);
    CHECK(w->course_free[3] == 0x10000u && w->course_lag[3] == 1u && !w->course_free[1] && !w->course_lag[2]);
    CHECK(w->course_free[0] == 0xc0000u && w->course_lag[0] == 9u);
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
    const uint32_t all = w->update_ticks;
    player.filled = player.released; player.lag = 50u;
    ++player.out;
    player_update_for(900u); /* its first call: the counters as they stand */
    CHECK(!bumper->out && !bumper->skip);
    ++player.out;
    player_update_for(15000u);
    ++player.skip;
    player_update_for(40u);
    CHECK(bumper->out == 1u && bumper->out_ticks == 15000u + step && bumper->skip == 1u && bumper->skip_ticks == 40u + step);
    CHECK(intro->out == 2u && intro->skip == 1u && w->update_ticks == all);
    for(unsigned i = 0; i < KUI_TOY_WIRE_BUCKETS; ++i) CHECK(w->course_free[i] <= 0xc0000u && w->course_lag[i] <= 9u);
    reader_pace(0u);
    m.delay = 0u;
    printf("  the player itself: frames put out and skipped with their update's time, the reader's interrupt time "
           "around them, its buffer's emptiest and its lag's highest in each piece; each movie apart\n");
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
    CHECK(w->parity == 2u && w->check_bad >= 1u && w->stops[KUI_TOY_WIRE_STOP_SECTOR] >= 1u);
    CHECK(w->check_bad + w->refused == w->stops[KUI_TOY_WIRE_STOP_SECTOR] && w->seek_blocks >= 5u);
    printf("  image without parity: %u sectors re-read by programmed transfers, data exact\n",
        (unsigned)w->stops[KUI_TOY_WIRE_STOP_SECTOR]);
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
    /* The movie goes on: a new window, a new stream. */
    read_request(FIRST + REQUEST, REQUEST, 3u);
    read_request(FIRST + 2u * REQUEST, REQUEST, 3u);
    CHECK(w->n.starts == 2u && kui_native_readahead_aux_context.stats.hit_requests == 3u);
    printf("  another read in the middle of the movie: stream stopped, CMD12 from the foreground, read exact\n");
}

/* The movie ends while a request waits on a running stream. The next SCI
 * interrupt visit stops the stream there and then, without a card command;
 * the CMD12 waits for the foreground, and the waiting request is then
 * completed by the low reader. */
static void ended_in_interrupt(void) {
    open_movie(2000u, false, true);
    stall_mean = 900u;
    memset(ram + (OUTPUT - BEGIN), 0xa5, 64u * 2352u + 16u);
    const uint32_t token = (uint32_t)request(FIRST, REQUEST, OUTPUT);
    CHECK(stream_on() && R.shared.service.pending && card.cmd18 == 1u && !card.cmd12);
    const struct kui_toy_wire_report *w = report();
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    const unsigned long before = eris;
    CHECK(hardware() && eris == before + 1u);
    CHECK(!stream_on() && R.shared.stream.state == 3u && !R.engine.hooked && R.shared.service.pending);
    CHECK(!card.cmd12 && w->drops[KUI_TOY_WIRE_DROP_ENDED] == 1u && w->stops[KUI_TOY_WIRE_STOP_CLOSE] == 1u);
    closed();
    CHECK(!hardware()); /* nothing is left to interrupt */
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 20000u && !R.engine.failed);
        if(!hardware()) game_poll();
    }
    CHECK(card.cmd12 == 1u && !kui_native_readahead_aux_context.movie_ahead.variant);
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    compare(FIRST, REQUEST, OUTPUT);
    printf("  movie ended under a waiting request: stream stopped in the interrupt visit, CMD12 later, read exact\n");
}

/* No free timer, no interrupt route, no free channel. */
static void degraded(void) {
    open_movie(2000u, false, true);
    stall_mean = 400u; stall_dense = 40u;
    wx.tstr = 6u; /* both spare timers run for the game */
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
        for(unsigned n = 0u; n < 12u && !seen; ++n, lba += REQUEST) {
            read_request(lba, REQUEST, 2u); /* room in the window */
            for(unsigned k = 0u; k < 4000u && !seen && hardware(); ++k) {
                if(!stream_on() || s->state != KUI_WIRE_WAIT) continue;
                CHECK(!s->mode && kui_wire_overdue(s));
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
    printf("  no free timer: waits end at the next visit, and an update call that finds one is passed on; "
           "channel 1 taken: programmed reads only; data exact\n");
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
    /* Test 66's own chunk classification is untouched by the switch. */
    movie_chunk_classification();
    printf("Test %u reader, real low reader + auxiliary + stream on the card model:\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u);
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
#if KUI_TOY_WIRE_WORDS
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
#endif
#if KUI_TOY_WIRE_IDLE
    waiting();
    player_measured();
#endif
    damaged_block();
    no_parity();
    other_read();
    ended_in_interrupt();
    degraded();
    printf("PASS Test %u reader integration: %u checks\n", KUI_TOY_WIRE_TICK ? 69u : KUI_TOY_WIRE_IDLE ? 73u : KUI_TOY_WIRE_PACE ? 72u : KUI_TOY_WIRE_WORDS ? 71u : KUI_TOY_WIRE_CACHED ? 70u : 68u, checks);
    return 0;
}
