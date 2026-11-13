/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise production burst policy with real GD cursor data and an owned
 * receiver that can return incomplete or corrupted wire snapshots. */
#include "native_readahead_aux_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { CLOSED, PAUSED, LOST = 3, DISC = 45000, CARD = 1000, MOVIE = DISC + 20,
       FIRST = MOVIE + 386, TRACK_SECTORS = 1000 };
static kui_retail_async_region_type region;
static kui_retail_async_region_type *region_cell = &region;
static struct kui_retail_manifest manifest;
static _Alignas(32) uint8_t arena[KUI_NATIVE_RA_DATA_BYTES];
static uint8_t output[8u * 2048u + 1u];
static uint32_t token, timer, wire_bytes, started_lba, started_frames;
static unsigned starts, stops, stock, corruption;
static bool ready, token_pending, started_slow;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
static unsigned cold_acquires, cold_releases;
#endif
#define C kui_native_readahead_aux_context
#define GD region.shared.service
#define E region.engine
#define S region.shared.stream

uint32_t kui_native_readahead_aux_test_read(uint32_t at, unsigned width) {
    (void)width;
    switch(at) {
    case 0xffc00000u: return 0xe0au;
    case 0xffd80008u: return UINT32_MAX;
    case 0xffd80010u: return 2u;
    case 0xffd80004u: return 1u;
    case 0xffd8000cu: return --timer;
    case 0xffa0001cu: return S.state == KUI_TOY_SCI_BURST_DMA ? 1u : 0u;
    case 0xffa00040u: return 1u;
    case 0xffd00008u: return 0x10u;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    case 0xffa0002cu: return 0x12c0u;
    case 0xa05f6808u: return 1u;
    case 0xa05f7818u: return 0u;
#endif
    default: assert(false); return 0;
    }
}

static uint8_t image_byte(uint32_t at) {
    uint32_t sector_byte = at % 2352u;
    if(sector_byte < 16u) {
        if(sector_byte && sector_byte < 11u) return 0xffu;
        return sector_byte == 15u;
    }
    return (uint8_t)((at * 13u) ^ (at >> 8) ^ 0x5au);
}

static void write_out(void *ignored, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    (void)ignored;
    assert(offset <= sizeof(output) && count <= sizeof(output) - offset);
    memcpy(output + offset, bytes, count);
}

static void hook(void) { E.hooked = 1u; E.release.armed = 0x10u; }
static void unhook(void) { E.hooked = 0u; E.release.armed = 0u; }
static bool busy(void) { return S.state == KUI_TOY_SCI_BURST_DMA; }
static enum kui_sci_stream_result low_stop(void) {
    assert(S.state == CLOSED || S.state == PAUSED);
    S.state = CLOSED;
    return KUI_SCI_STREAM_OK;
}

static enum kui_sci_stream_result low_fetch(uint32_t lba, uint32_t limit, bool polled) {
    (void)lba; (void)limit; (void)polled;
    assert(false);
    return KUI_SCI_STREAM_RESET;
}

static void legacy(uint32_t wait) {
    (void)wait;
    assert(S.state == CLOSED || S.state == PAUSED);
    ++stock;
    while(E.active && !E.failed) {
        uint8_t block[512];
        uint32_t file = (E.cursor.block - CARD) * 512u;
        for(unsigned i = 0; i < sizeof(block); ++i) block[i] = image_byte(file + i);
        assert(kui_retail_cursor_feed(&E.cursor, block) == KUI_GAME_OK);
        ++E.stats.call_blocks;
        if(E.cursor.done == E.cursor.count) E.active = 0u;
    }
    S.state = PAUSED;
}

#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
static bool cold_bus(bool acquire) {
    assert(!E.active && !E.in_irq && !busy());
    if(acquire) {
        assert(!E.opened && !region.shared.port.acquired);
        ++cold_acquires;
    } else {
        assert(E.opened && region.shared.port.acquired && S.state == CLOSED);
        ++cold_releases;
    }
    E.opened = region.shared.port.acquired = acquire;
    return true;
}
#endif

static const struct kui_native_ra_low_ops ops = {
    .magic = KUI_NATIVE_RA_OPS_MAGIC, .version = KUI_NATIVE_RA_VERSION, .bytes = sizeof(ops),
    .region_storage = &region_cell, .deliver = legacy, .hook = hook, .unhook = unhook,
    .fetch = low_fetch, .stop = low_stop, .busy = busy, .feed = kui_retail_cursor_feed,
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    .cold_bus = cold_bus,
#endif
};

static uint8_t reverse(uint8_t b) {
    uint8_t out = 0;
    for(unsigned i = 0; i < 8u; ++i) {
        out = (uint8_t)(out * 2u + (b & 1u));
        b >>= 1;
    }
    return out;
}

static enum kui_sci_stream_result start_fixture(kui_retail_async_region_type *r,
    uint32_t lba, unsigned frames, uint8_t *slab, bool slow) {
    assert(r == &region && !E.in_irq && !busy());
    assert(slab == arena + KUI_NATIVE_RA_CACHE_BYTES && frames && frames <= 15u);
    if(S.state == KUI_TOY_SCI_BURST_TOKEN) {
        assert(lba == started_lba && frames == started_frames && slow == started_slow);
    }
    started_lba = lba;
    started_frames = frames;
    started_slow = slow;
    if(token_pending) {
        S.state = KUI_TOY_SCI_BURST_TOKEN;
        return KUI_SCI_STREAM_PENDING;
    }
    unsigned at = 0;
    for(unsigned n = 0; n < frames; ++n) {
        if(n) { slab[at++] = 0xffu; slab[at++] = 0x7fu; }
        uint16_t crc = 0;
        uint32_t file = (lba + n - CARD) * 512u;
        for(unsigned i = 0; i < 512u; ++i) {
            uint8_t byte = image_byte(file + i);
            slab[at++] = reverse(byte);
            crc ^= (uint16_t)byte << 8;
            for(unsigned bit = 0; bit < 8u; ++bit)
                crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
        }
        if(corruption == n + 1u) crc ^= 1u;
        slab[at++] = reverse((uint8_t)(crc >> 8));
        slab[at++] = reverse((uint8_t)crc);
    }
    wire_bytes = at;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    S.ready_lba[1] = at;
#endif
    ++starts;
    S.state = KUI_TOY_SCI_BURST_DMA;
    return KUI_SCI_STREAM_OK;
}

enum kui_sci_stream_result toy_sci_burst_start(kui_retail_async_region_type *r,
    uint32_t lba, unsigned frames, uint8_t *slab) {
    return start_fixture(r, lba, frames, slab, false);
}

#if KUI_TOY_SUITE_MODE == 28
enum kui_sci_stream_result toy_sci_burst_start_rate(kui_retail_async_region_type *r,
    uint32_t lba, unsigned frames, uint8_t *slab, bool slow) {
    return start_fixture(r, lba, frames, slab, slow);
}
#endif

enum kui_sci_stream_result toy_sci_burst_poll(kui_retail_async_region_type *r,
    uint8_t *slab, unsigned frames, uint32_t *received) {
    assert(r == &region && busy());
    assert(slab == arena + KUI_NATIVE_RA_CACHE_BYTES && frames == started_frames);
    if(!ready
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
       && !C.burst.calibration
#endif
      ) return KUI_SCI_STREAM_PENDING;
    ready = false;
    S.state = LOST;
    *received = wire_bytes;
    return KUI_SCI_STREAM_OK;
}

enum kui_sci_stream_result toy_sci_burst_stop(kui_retail_async_region_type *r,
    uint8_t *slab, unsigned frames) {
    assert(r == &region && !E.in_irq);
    assert(S.state == LOST || busy() || S.state == KUI_TOY_SCI_BURST_TOKEN);
    assert(slab == arena + KUI_NATIVE_RA_CACHE_BYTES && frames == started_frames);
    ++stops;
    S.state = CLOSED;
    ready = false;
    return KUI_SCI_STREAM_OK;
}

static void setup(void) {
    memset(&C, 0, sizeof(C));
    memset(&region, 0, sizeof(region));
    memset(&manifest, 0, sizeof(manifest));
    token = wire_bytes = started_lba = started_frames = 0u;
    timer = 0x70000000u;
    starts = stops = stock = corruption = 0u;
    ready = token_pending = started_slow = false;
    manifest.card_sectors = 10000u;
    manifest.partition_start = 100u;
    manifest.partition_end = 9000u;
    manifest.track_count = manifest.extent_count = 1u;
    manifest.slots[0].track = (struct kui_retail_track){DISC, DISC + TRACK_SECTORS, 1u, 1u, 4u};
    manifest.slots[1].extent = (struct kui_retail_extent){0u, CARD, (TRACK_SECTORS * 2352u + 511u) / 512u};
    E.manifest = &manifest;
    E.target = 20u;
    E.opened = 1u;
    GD.initialized = 1u;
    region.shared.port.acquired = true;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    cold_acquires = cold_releases = 0u;
    region.shared.card.transport = KUI_STORAGE_SCI;
    region.shared.card.device.sd.ready = true;
#endif
    struct kui_native_ra_config cfg = {
        .version = KUI_NATIVE_RA_VERSION, .bytes = sizeof(cfg), .region = &region, .ops = &ops,
        .data = arena, .data_bytes = sizeof(arena), .lease = 0x0c400000u,
        .lease_bytes = KUI_NATIVE_RA_LEASE_BYTES,
        .movie_ranges = {[3] = {MOVIE, 900u}}, .suite_source = 1u,
    };
    assert(kui_native_readahead_aux_init(&cfg) == 0);
}

static void begin(uint32_t lba, uint32_t count) {
    memset(output, 0xa5, sizeof(output));
    E.token = ++token;
    E.active = 1u;
    E.failed = E.since = E.in_irq = 0u;
    GD.command = KUI_GD_DMAREAD;
    GD.token = token;
    GD.pending = 1u;
    GD.status = KUI_GD_PROCESSING;
    GD.error = GD.completed_bytes = 0u;
    GD.lba = lba;
    GD.count = count;
    GD.sector_bytes = 2048u;
    assert(kui_retail_cursor_begin(&E.cursor, &manifest, lba, count, KUI_GAME_SECTOR_MODE1,
        write_out, NULL) == KUI_GAME_OK);
}

static void visit(uint32_t event) {
    uint32_t pending = GD.pending, completed = GD.completed_bytes;
    int32_t status = GD.status;
    assert(kui_native_readahead_aux_dispatch(event, 0u, &region, 0u) == 1u);
    assert(GD.pending == pending && GD.completed_bytes == completed && GD.status == status);
}

static void check_output(uint32_t lba, uint32_t count) {
    assert(!E.active && !E.failed);
    for(unsigned n = 0; n < count; ++n) {
        for(unsigned i = 0; i < 2048u; ++i)
            assert(output[n * 2048u + i] == image_byte((lba - DISC + n) * 2352u + 16u + i));
    }
    assert(output[count * 2048u] == 0xa5u);
}

static void finish(uint32_t lba, uint32_t count) {
    for(unsigned n = 0; E.active && n < 20u; ++n) {
        ready = busy();
        visit(KUI_NATIVE_RA_DELIVER);
    }
    check_output(lba, count);
}

static void opening_and_inter_request_lead(void) {
    setup();
    begin(FIRST - 1u, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST - 1u, 1u);
    assert(stock == 1u && starts == 0u);

    begin(FIRST, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    assert(E.active && starts == 1u && busy() && C.cache.count == 0u);
    visit(KUI_NATIVE_RA_DELIVER);
    assert(E.active && starts == 1u && stock == 1u && output[0] == 0xa5u);
    E.in_irq = 1u;
    ready = true;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(C.cache.count == 4u && starts == 1u);
    E.in_irq = 0u;
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST, 1u);
    assert(GD.pending && starts == 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    assert(busy() && starts == 2u && stops == 0u);

    GD.pending = 0u;
    GD.status = KUI_GD_COMPLETED;
    GD.command = 0u; /* Real CHECK clears command after reporting completion. */
    visit(KUI_NATIVE_RA_TICK);
    assert(C.burst.owned && busy() && !stops);
    begin(FIRST + 1u, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST + 1u, 1u);
    assert(C.suite[4] == 1u && busy() && starts == 2u && stock == 1u);

    GD.pending = 0u;
    GD.status = KUI_GD_NOT_FOUND; /* Real RESET/INIT. */
    GD.command = 0u;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(!C.burst.owned && !busy() && stops == 1u);
}

static void checked_prefix(bool bad_crc, bool bad_token) {
    setup();
    corruption = bad_crc ? 3u : 0u;
    begin(FIRST, 2u);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    C.burst_quiet_lba = E.cursor.block;
    C.burst_quiet_end = C.burst_quiet_lba + 15u;
#endif
    visit(KUI_NATIVE_RA_DELIVER);
    uint32_t first = started_lba;
    bool full_dma = bad_crc || bad_token;
    if(!full_dma) wire_bytes = 2u * 516u - 2u + 100u;
    if(bad_token) arena[KUI_NATIVE_RA_CACHE_BYTES + 2u * 516u - 1u] = 0x12u;
    E.in_irq = 1u;
    ready = true;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(C.cache.count == 2u && C.suite[2] == 1u && starts == 1u &&
        C.stats.crc_errors == (unsigned)bad_crc && E.active);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    const struct kui_toy_burst_metrics *m = &C.burst_match;
    assert(m->arms == 1u && m->requested == 15u * 516u - 2u && m->received == wire_bytes);
    assert(m->checked == 2u && m->usable == 1u && !m->timeout && !m->reset);
    assert(m->dma_full == (unsigned)full_dma && m->dma_short == !full_dma);
    assert(m->parse_full_cut == (unsigned)full_dma && m->bad_crc == (unsigned)bad_crc &&
        m->bad_token == (unsigned)bad_token);
    if(!full_dma) {
        assert(m->first_short.tcr == m->requested - wire_bytes &&
            m->first_short.chcr2 == 0x12c0u && m->first_short.c2dst == 1u &&
            !m->first_short.g2adst);
        assert(!memcmp(&m->first_short, &m->last_short, sizeof(m->first_short)));
    }
    assert(!memcmp(m, &C.burst_live, sizeof(*m)) && !C.burst_quiet.arms);
#endif
    assert(!kui_native_ra_cache_lookup(&C.cache, first + 2u));
    E.in_irq = 0u;
    corruption = 0u;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(busy() && starts == 2u && started_lba == first + 2u);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    assert(C.burst_live.arms == 2u && m->arms == 1u); /* Extends beyond the matched band. */
#endif
#if KUI_TOY_SUITE_MODE == 28
    assert(started_slow == !full_dma);
#endif
    finish(FIRST, 2u);
    assert(stock == 0u);
}

static void handoff_and_timeout(void) {
    setup();
    token_pending = true;
    begin(FIRST, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    assert(S.state == KUI_TOY_SCI_BURST_TOKEN && C.burst.owned);
    begin(DISC, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(DISC, 1u);
    assert(stops == 1u && stock == 1u && !C.burst.owned);

    setup();
    begin(FIRST, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    GD.pending = E.active = 0u;
    GD.error = KUI_GD_ERROR_CANCELLED;
    GD.status = KUI_GD_FAILED;
    visit(KUI_NATIVE_RA_MOVIE_COMPLETE);
    assert(stops == 1u && !C.burst.owned && !stock);

    setup();
    begin(FIRST, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    timer -= 39064u;
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST, 1u);
    assert(stops == 1u && stock == 1u && !C.burst.owned && C.suite[3] == 1u);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    assert(C.burst_live.arms == 1u && C.burst_live.timeout == 1u &&
        !C.burst_live.dma_short && !C.burst_live.dma_full);
#endif
}

static void ring_wrap_and_quiesce(void) {
    setup();
    uint32_t first = CARD + (FIRST - DISC) * 2352u / 512u;
    bool wrapped = false;
    uint32_t last_head = 0;
    for(unsigned n = 0; n < 100u; ++n) {
        begin(FIRST + n, 1u);
        finish(FIRST + n, 1u);
        GD.pending = 0u;
        GD.status = KUI_GD_COMPLETED;
        GD.command = 0u;
        visit(KUI_NATIVE_RA_TICK);
        if(C.cache.head < last_head) wrapped = true;
        last_head = C.cache.head;
    }
    assert(wrapped && C.cache.base > first && C.cache.count <= KUI_NATIVE_RA_CACHE_BLOCKS && !stock);
    visit(KUI_NATIVE_RA_QUIESCE);
    assert(!C.burst.owned && !busy() && !E.hooked && stops == 1u);
}

#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
static void quiet_comparison(void) {
    setup();
    E.opened = region.shared.port.acquired = false;
    S.stats.overruns = 13u;
    visit(KUI_NATIVE_RA_CALIBRATE);
    assert(C.burst_quiet_status == 1u && cold_acquires == 1u && cold_releases == 1u);
    assert(!E.opened && !region.shared.port.acquired && S.stats.overruns == 13u);
    assert(C.burst_quiet_lba == CARD + (FIRST - DISC) * 2352u / 512u);
    assert(C.burst_quiet_end - C.burst_quiet_lba == 240u);
    assert(C.burst_quiet_late_lba == CARD + (MOVIE + 638u - DISC) * 2352u / 512u);
    assert(C.burst_quiet_late_end - C.burst_quiet_late_lba == 240u);
    const struct kui_toy_burst_metrics *m = &C.burst_quiet;
    assert(m->arms == 32u && m->dma_full == 32u && m->usable == 32u && m->checked == 480u);
    assert(m->requested == 32u * (15u * 516u - 2u) && m->received == m->requested);
    assert(!m->dma_short && !m->bad_token && !m->bad_crc && !m->parse_full_cut);
    assert(!C.burst_live.arms && !C.burst_match.arms && !C.suite[0] && !C.cache.count);
#if KUI_TOY_SUITE_MODE == 28
    assert(!started_slow && !C.burst_fast.arms && !C.burst_slow.arms);
#endif
    assert(kui_native_readahead_aux_dispatch(KUI_NATIVE_RA_CALIBRATE, 0u, &region, 0u) == 0u);
    assert(starts == 32u && cold_acquires == 1u && cold_releases == 1u);
}
#endif

#if KUI_TOY_SUITE_MODE == 28
static void receive_and_rearm(uint32_t bytes, bool slow_next, uint32_t next_lba) {
    unsigned prior_starts = starts;
    wire_bytes = bytes;
    ready = true;
    for(unsigned n = 0; n < 4u && starts == prior_starts; ++n) visit(KUI_NATIVE_RA_DELIVER);
    assert(starts == prior_starts + 1u && busy() && started_slow == slow_next &&
        started_lba == next_lba && !stock);
}

static void adaptive_rate(void) {
    setup();
    begin(FIRST, 8u);
    visit(KUI_NATIVE_RA_DELIVER);
    uint32_t first = started_lba;
    assert(!started_slow && C.burst_fast.arms == 1u);
    receive_and_rearm(514u, true, first + 1u);
    receive_and_rearm(wire_bytes, true, first + 16u);
    receive_and_rearm(wire_bytes, false, first + 31u);
    assert(C.burst_fast.arms == 2u && !C.burst_fast.dma_full && C.burst_fast.checked == 1u);
    assert(C.burst_slow.arms == 2u && C.burst_slow.dma_full == 2u && C.burst_slow.checked == 30u);
    finish(FIRST, 8u);

    /* Recovery selected at a sliced token, then retained after a stock
     * fallback if another slower attempt is still owed in this movie. */
    setup();
    begin(FIRST, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    first = started_lba;
    token_pending = true;
    wire_bytes = 0;
    ready = true;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(S.state == KUI_TOY_SCI_BURST_TOKEN && started_slow && started_lba == first &&
        !C.burst_slow.arms);
    token_pending = false;
    visit(KUI_NATIVE_RA_DELIVER);
    assert(busy() && C.burst_slow.arms == 1u && started_slow);
    timer -= 39064u;
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST, 1u);
    assert(stock == 1u && !busy());
    begin(FIRST + 1u, 1u);
    visit(KUI_NATIVE_RA_DELIVER);
    assert(busy() && C.burst_slow.arms == 2u && started_slow);

    /* Two parser cuts alone do not select a slower clock. An empty third
     * fast receive may add one slow recovery, then ordinary stock wins. */
    setup();
    begin(FIRST, 1u);
    corruption = 1u;
    visit(KUI_NATIVE_RA_DELIVER);
    first = started_lba;
    receive_and_rearm(wire_bytes, false, first);
    receive_and_rearm(wire_bytes, false, first);
    receive_and_rearm(0u, true, first);
    wire_bytes = 0u;
    ready = true;
    visit(KUI_NATIVE_RA_DELIVER);
    check_output(FIRST, 1u);
    assert(stock == 1u && !busy() && C.burst_slow.arms == 1u);
}
#endif

int main(void) {
    opening_and_inter_request_lead();
    checked_prefix(false, false);
    checked_prefix(true, false);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    checked_prefix(false, true);
    quiet_comparison();
#endif
#if KUI_TOY_SUITE_MODE == 28
    adaptive_rate();
#endif
    handoff_and_timeout();
    ring_wrap_and_quiesce();
    puts("Toy burst auxiliary: stock opening, verified partial/CRC prefixes, IRQ bounds, "
         "cache-ready next request, ACK/reset, token/abort closure, DMA timeout and ring wrap passed");
    return 0;
}
