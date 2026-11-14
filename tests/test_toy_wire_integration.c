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

/* ---- what the stream needs beyond the shared model ---- */
static struct {
    uint8_t tstr;
    uint16_t ipra, tcr[3];
    uint32_t tcor[3], tcnt[3], slots[64], tcnt0;
} wx;
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
    if(address == 0xffd8000cu) return wx.tcnt0 -= 5u;
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
static void open_movie(unsigned extent_blocks, bool scattered, bool with_parity) {
    setup_movie_card_extents(1u, extent_blocks, scattered);
    for(uint32_t lba = manifest.slots[2].track.start_lba; lba < manifest.slots[2].track.end_lba; ++lba)
        install_raw_sector(lba, with_parity);
    struct kui_native_ra_config config = movie_configuration();
    config.movie_ranges[3] = (struct kui_toy_movie_range){FIRST - 386u, WINDOW_END - (FIRST - 386u)};
    bind_movie_configuration(&config);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
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
/* One moment of console time: whatever interrupt is due, else nothing. */
static bool hardware(void) {
    unsigned channel;
    if(stream_on()) {
        elapse();
        /* A run in flight ends at its overrun; the SCI then asks for its
         * error interrupt at the resident's level. */
        if(m.rx) (void)kui_sci_stream_test_read(SSR, 1u);
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
    } else if(kui_sci_stream_busy()) {
        independent_sdk_irq();
        return true;
    }
    return false;
}
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
        if(!scattered) CHECK(card.cmd18 == 1u && !card.cmd12 && w->n.starts == 1u && warm >= requests - 3u);
        else CHECK(w->stops[KUI_TOY_WIRE_STOP_JUMP] >= 2u && card.cmd18 >= 3u);
    }
    if(w->stops[KUI_TOY_WIRE_STOP_FAULT] || w->n.unsolved)
        fprintf(stderr, "fault stops %u, unsolved %u, first error %u detail %x, mean %u dense %u\n",
            (unsigned)w->stops[KUI_TOY_WIRE_STOP_FAULT], (unsigned)w->n.unsolved,
            (unsigned)w->first_error, (unsigned)w->first_detail, (unsigned)mean, dense);
    CHECK(!w->stops[KUI_TOY_WIRE_STOP_FAULT] && !w->n.unsolved);
    printf("  %s extents, stall every ~%u bytes, %u%% busy phases, poll every %u: %u requests, %u served on arrival; "
           "streams %u, cuts %u, waits %u, fulls %u, blocks %u (rebuilt %u, holed %u), sectors filled %u (most lost %u), "
           "re-read %u+%u, stream ends close/jump/sector/frame %u/%u/%u/%u, CMD18 %u, CMD12 %u, ERI %lu, timer %lu\n",
        scattered ? "scattered" : "contiguous", (unsigned)mean, dense, poll_every, requests, warm,
        (unsigned)w->n.starts, (unsigned)w->n.cuts, (unsigned)w->n.waits, (unsigned)w->n.fulls, (unsigned)w->n.blocks,
        (unsigned)w->n.rebuilt, (unsigned)w->n.holed, (unsigned)w->filled, (unsigned)w->most_lost,
        (unsigned)w->refused, (unsigned)w->check_bad, (unsigned)w->stops[0], (unsigned)w->stops[1],
        (unsigned)w->stops[2], (unsigned)w->stops[3], card.cmd18, card.cmd12, eris, timers);
}

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

    open_movie(2000u, false, true);
    m.chcr = 1u; /* the game uses DMA channel 1 */
    lba = FIRST;
    for(unsigned i = 0; i < 3u; ++i, lba += REQUEST) read_request(lba, REQUEST, 1u);
    w = report();
    CHECK(w->declined && !w->n.starts && w->first_error == KUI_WIRE_E_CHANNEL && m.chcr == 1u);
    CHECK(w->seek_blocks >= 3u * REQUEST * 4u && !timers);
    printf("  no free timer: waits end at the next visit; channel 1 taken: programmed reads only; data exact\n");
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
    printf("Test 68 reader, real low reader + auxiliary + stream on the card model:\n");
    steady(2000u, false, 0u, 0u, 3u);
    steady(2000u, false, 0u, 0u, 0u);
    steady(2000u, false, 6000u, 0u, 3u);
    steady(2000u, false, 1500u, 20u, 2u);
    steady(2000u, false, 500u, 50u, 1u);
    steady(2000u, false, 150u, 80u, 4u);
    steady(28u, true, 0u, 0u, 3u);
    steady(28u, true, 6000u, 5u, 2u);
    damaged_block();
    no_parity();
    other_read();
    ended_in_interrupt();
    degraded();
    printf("PASS Test 68 reader integration: %u checks\n", checks);
    return 0;
}
