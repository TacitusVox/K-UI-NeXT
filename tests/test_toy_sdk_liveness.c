/* SPDX-License-Identifier: GPL-3.0-only */
/* Real SDK SCI routing, read-ahead auxiliary, reader and SD model. */
#include "native_readahead_aux_internal.h"
#include "kui/toy_pilot_clock.h"

#define KUI_NATIVE_SDK_SCI_MAIN sdk_route_suite_main
#define kui_sci_sd_acquire model_sci_sd_acquire
#include "test_retail_async_sdk_sci_route.c"
#undef kui_sci_sd_acquire

extern const struct kui_native_ra_low_ops kui_native_ra_reader_ops;
static _Alignas(32) uint8_t arena[KUI_NATIVE_RA_DATA_BYTES];

enum { MOVIE_LBA = 45001u, MOVIE_SECTORS = 14u };

#if KUI_TOY_SUITE_PIPELINED_PIO
static unsigned payload_calls, payload_bytes, payload_fail_after;

/* Keep the card model shared with the ordinary recovery fixtures. The
 * production SCI helper's hardware credit is tested separately at MMIO. */
bool kui_sci_sd_pio_payload(uint8_t *area) {
    CHECK(!m.cs_high && !m.rx && m.healthy);
    ++payload_calls;
    for(unsigned i = 0u; i < KUI_SCI_SD_PIO_PAYLOAD_BYTES; ++i) {
        if(payload_fail_after && i == payload_fail_after) {
            m.healthy = false;
            m.scr = 0u;
            return false;
        }
        area[i] = bus_transfer(NULL, 0xffu, false);
        ++payload_bytes;
    }
    return true;
}
#endif

#if KUI_TOY_SUITE_MOVIE_POLL
enum { POLL_SR = 0x60u };
static const uint32_t poll_frame = END - 0x100u;
static const uint32_t event900_poll = UINT32_C(0x8c0b3b88);
static const uint32_t movie_async_poll = UINT32_C(0x8c097144);
#if !KUI_TOY_SUITE_DIRECT_PIO
static const uint32_t movie_blocking_poll = UINT32_C(0x8c09712a);
#endif
#if KUI_TOY_SUITE_REFILL_SLICE
/* Exercise the existing one-pass cadence in the recovery fixtures. The
 * scheduled caller has separate refill-slice cases below. */
#define SINGLE_SERVICE_POLL movie_async_poll
static bool refill_clock_valid = true;
static uint32_t refill_clock_extra_ticks;
#else
#define SINGLE_SERVICE_POLL event900_poll
#endif
volatile uint32_t kui_toy_poll_frame;
static unsigned poll_word_reads;

uint32_t kui_toy_poll_test_word(uint32_t address) {
    CHECK(address >= poll_frame && address < END && !(address & 3u));
    ++poll_word_reads;
    return get(address);
}

static void prepare_sdk_poll_frame(uint32_t function, uint32_t outer_pr) {
    CHECK(function == KUI_GD_EXEC || function == KUI_GD_CHECK);
    memset(ram + (poll_frame - BEGIN), 0, END - poll_frame);
    put(poll_frame + 8u * 4u, POLL_SR);
    if(function == KUI_GD_EXEC) {
        put(poll_frame + 7u * 4u, UINT32_C(0x8c0bd55a));
        put(poll_frame + 9u * 4u, UINT32_C(0x8c0b1fe6));
        put(poll_frame + 10u * 4u, outer_pr);
    } else {
        put(poll_frame + 7u * 4u, UINT32_C(0x8c0bd374));
        put(poll_frame + 13u * 4u, UINT32_C(0x8c0b145c));
        put(poll_frame + 25u * 4u, outer_pr);
    }
}

static int32_t sdk_poll_saved_frame(uint32_t function, uint32_t token, uint32_t frame) {
    uint32_t original_sr = hw.sr;
    kui_toy_poll_frame = frame;
    CHECK(!kui_retail_hook_active);
    kui_retail_hook_active = 1u;
    kui_retail_hook_sr = POLL_SR;
    hw.sr = POLL_SR | UINT32_C(0x100000f0);
    kui_retail_async_trace(0u, function);
    int32_t result = gd(function, function == KUI_GD_CHECK ? token : 0u,
        function == KUI_GD_CHECK ? STATUS : 0u);
    kui_retail_async_trace(1u, function);
    CHECK(!R.shared.stream.toy_suite_poll);
    kui_retail_hook_active = 0u;
    kui_toy_poll_frame = 0u;
    kui_retail_hook_sr = hw.sr = original_sr;
    return result;
}

static int32_t sdk_poll(uint32_t function, uint32_t token, uint32_t outer_pr) {
    prepare_sdk_poll_frame(function, outer_pr);
    return sdk_poll_saved_frame(function, token, poll_frame);
}
#endif

uint32_t kui_retail_async_vmu_test_sr(void) { return hw.sr; }
enum kui_loader_sd_result kui_sci_sd_acquire(void) {
    enum kui_loader_sd_result result = model_sci_sd_acquire();
    if(result == KUI_LOADER_SD_OK) R.shared.port.acquired = true;
    return result;
}
void kui_sci_sd_release(void) { R.shared.port.acquired = false; }

uint32_t kui_native_readahead_aux_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): return 0xe0au;
    case UINT32_C(0xffd80008): return UINT32_MAX;
    case UINT32_C(0xffd80010): return 2u;
#if KUI_TOY_SUITE_REFILL_SLICE
    case UINT32_C(0xffd80004): return refill_clock_valid ? 1u : 0u;
    case UINT32_C(0xffd8000c):
        clock_counter -= refill_clock_extra_ticks;
        return kui_retail_async_test_read32(address);
#else
    case UINT32_C(0xffd80004): return 1u;
    case UINT32_C(0xffd8000c): return kui_retail_async_test_read32(address);
#endif
    default:
        CHECK(address != RDR);
        return kui_sci_stream_test_read(address, width);
    }
}

static void setup_movie_card_extents(unsigned first_token_delay, unsigned extent_blocks, bool scattered) {
    cooked_fixture = false;
    setup(KUI_NATIVE_SDK_SCI_OWNER_VBR, X, extent_blocks, scattered);
    mode(2048u);
    card.nac_first = first_token_delay;
    kui_retail_hook_active = kui_retail_hook_fault = 0u;
    R.vector600[KUI_NATIVE_SDK_SCI_DEFAULT_INDEX] = UINT32_C(0x8c1add0c);
    R.vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX] = KUI_NATIVE_SDK_SCI_OWNER_VBR;
    R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = KUI_NATIVE_SDK_SCI_COOKIE;
    eri_slot = rxi_slot = callback();
#if KUI_TOY_SUITE_PIPELINED_PIO
    payload_calls = payload_bytes = payload_fail_after = 0u;
#endif
}

static void setup_movie_card(unsigned first_token_delay, unsigned extent_blocks) {
    setup_movie_card_extents(first_token_delay, extent_blocks, false);
}

static struct kui_native_ra_config movie_configuration(void) {
    struct kui_native_ra_config configuration = {
        .version = KUI_NATIVE_RA_VERSION,
        .bytes = sizeof(configuration),
        .region = &R,
        .ops = &kui_native_ra_reader_ops,
        .data = arena,
        .data_bytes = sizeof(arena),
        .lease_bytes = KUI_NATIVE_RA_LEASE_BYTES,
        .suite_source = 1u,
#if KUI_TOY_SUITE_MOVIE_POLL
        .toy_poll_owner = 1u,
#endif
    };
    return configuration;
}

static void bind_movie_configuration(const struct kui_native_ra_config *configuration) {
    memset(&kui_native_readahead_aux_context, 0, sizeof(kui_native_readahead_aux_context));
    CHECK(kui_native_readahead_aux_init(configuration) == 0);
    R.vector600[KUI_NATIVE_RA_ENTRY_INDEX] = (uint32_t)(uintptr_t)kui_native_readahead_aux_dispatch;
}

static void setup_movie(unsigned first_token_delay, bool scope_movie) {
    setup_movie_card(first_token_delay, 48u);
    struct kui_native_ra_config configuration = movie_configuration();
    if(scope_movie) {
        /* Start in the steady-state section of the movie and model only the
         * raw track segment that this test actually requests. */
        configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    }
    bind_movie_configuration(&configuration);
}

#if KUI_TOY_SUITE_MOVIE_CHUNK
#if KUI_TOY_SUITE_AFTER_START
/* The retail wrapper passes a local context on the inherited IP stack. */
#define CHUNK_CONTEXT UINT32_C(0x8c00dc90)
#define CHUNK_BEFORE_RAM UINT32_C(0x8c007ffc)
#else
#define CHUNK_CONTEXT (BEGIN + 0x40000u)
#define CHUNK_BEFORE_RAM (BEGIN - 4u)
#endif
#define CHUNK_NAME (CHUNK_CONTEXT + 0xbcu)
static unsigned chunk_word_reads;
static uint8_t chunk_filename[24];

uint32_t kui_toy_movie_chunk_test_word(uint32_t address) {
    address &= ~UINT32_C(0x20000000);
    CHECK(address >= CHUNK_NAME && address <= CHUNK_NAME + 20u && !(address & 3u));
    ++chunk_word_reads;
    const uint8_t *word = chunk_filename + (address - CHUNK_NAME);
    return (uint32_t)word[0] | (uint32_t)word[1] << 8 | (uint32_t)word[2] << 16 |
        (uint32_t)word[3] << 24;
}

static void chunk_name(const char *name) {
    size_t bytes = strlen(name) + 1u;
    CHECK(bytes <= sizeof(chunk_filename));
    memset(chunk_filename, 0xa5, sizeof(chunk_filename));
    memcpy(chunk_filename, name, bytes);
}

static void movie_chunk_classification(void) {
    setup_movie_card(1u, 48u);
    struct kui_native_ra_config configuration = movie_configuration();
    static const char *const names[] = {"50hz_Bumper_LRTF.avi", "60hz_Bumper_LRTF.avi",
        "50hz_Intro_LRTF.avi", "60hz_Intro_LRTF.avi"};
    for(unsigned i = 0u; i < 4u; ++i)
        configuration.movie_ranges[i] = (struct kui_toy_movie_range){0x62000u + i * 0x100u, 64u};
    const struct kui_toy_movie_chunk_trace *trace = kui_native_readahead_aux_context.movie_chunk;
    struct kui_retail_gd *service = &R.shared.service;
    const uint32_t intro = configuration.movie_ranges[3].lba;
    const uint32_t other_rate = configuration.movie_ranges[2].lba;
    const uint32_t bumper = configuration.movie_ranges[1].lba;
    chunk_name(names[0]);
    chunk_word_reads = 0u;
    configuration.toy_poll_owner = 0u;
    bind_movie_configuration(&configuration);
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, 0);
    CHECK(!chunk_word_reads && !trace[0].ready && !trace[1].ready);

    configuration.toy_poll_owner = 1u;
    bind_movie_configuration(&configuration);
    static const uint32_t invalid_contexts[] = {0u, CHUNK_BEFORE_RAM, CHUNK_CONTEXT + 2u,
        0x8d000000u - 0xd0u, 0xcc050000u};
    for(unsigned i = 0u; i < sizeof(invalid_contexts) / sizeof(invalid_contexts[0]); ++i)
        kui_toy_movie_chunk_observe(invalid_contexts[i], 0);
    CHECK(!chunk_word_reads);
    for(unsigned i = 0u; i < 4u; ++i) {
        chunk_name(names[i]);
        service->token = i + 1u;
        service->command = i % 2u ? KUI_GD_DMAREAD : KUI_GD_PIOREAD;
        service->lba = configuration.movie_ranges[i].lba + 60u;
        service->count = 4u;
        service->pending = 1u;
        uint32_t context = i % 2u ? CHUNK_CONTEXT | UINT32_C(0x20000000) : CHUNK_CONTEXT;
        kui_toy_movie_chunk_observe(context, (int32_t)i);
        kui_toy_movie_chunk_observe(context, -2);
        CHECK(trace[i / 2u].ready == i % 2u + 1u && trace[i / 2u].wait_gd == i % 2u + 1u);
    }
    /* Stale metadata, other reads and overlapping spans do not identify
     * the pending request for this exact AVI. */
    const struct { uint32_t pending, command, lba, count; } unrelated[] = {
        {0u, KUI_GD_DMAREAD, intro, 2u}, {1u, KUI_GD_DMAREAD, other_rate, 2u},
        {1u, KUI_GD_DMAREAD, bumper, 2u}, {1u, KUI_GD_PIOREAD, intro - 1u, 2u},
        {1u, KUI_GD_PIOREAD, intro + 63u, 2u}, {1u, KUI_GD_PIOREAD, intro, 0u},
        {1u, KUI_GD_DMAREAD, intro, UINT32_MAX}, {1u, KUI_GD_GETTOC2, intro, 2u},
    };
    for(unsigned i = 0u; i < sizeof(unrelated) / sizeof(unrelated[0]); ++i) {
        service->pending = unrelated[i].pending;
        service->command = unrelated[i].command;
        service->lba = unrelated[i].lba;
        service->count = unrelated[i].count;
        uint8_t snapshot[sizeof(*service)];
        memcpy(snapshot, service, sizeof(snapshot));
        kui_toy_movie_chunk_observe(CHUNK_CONTEXT, -2);
        CHECK(trace[1].wait_no_gd == i + 1u && !memcmp(snapshot, service, sizeof(snapshot)));
    }
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, -1);
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, INT32_MIN);
    chunk_name("60hz_Intro_LRTF.aviX");
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, 0);
    chunk_name("60hz_bumper_LRTF.avi");
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, -2);
    CHECK(trace[0].ready == 2u && trace[0].wait_gd == 2u && !trace[0].wait_no_gd && !trace[0].other);
    CHECK(trace[1].ready == 2u && trace[1].wait_gd == 2u && trace[1].wait_no_gd == 8u && trace[1].other == 2u);

    configuration.movie_ranges[3].count = 0u;
    bind_movie_configuration(&configuration);
    chunk_name(names[3]);
    kui_toy_movie_chunk_observe(CHUNK_CONTEXT, 0);
    CHECK(!trace[1].ready && !hw.writes);
    printf("Mode %u: exact AVI names, context %08x, clip spans, ready, pending, stale and other chunk results passed\n",
        KUI_TOY_SUITE_MODE, CHUNK_CONTEXT);
}
#undef CHUNK_NAME
#undef CHUNK_BEFORE_RAM
#undef CHUNK_CONTEXT
#endif

static void check_original_sdk_owner(void) {
    CHECK(hw.vbr == KUI_NATIVE_SDK_SCI_OWNER_VBR && !hw.vbr_sets);
    CHECK((hw.iprb & ~0xf0u) == 0x5a0fu);
}

#if KUI_TOY_SUITE_ASYNC_DEMAND
static bool independent_demand_wake(void) {
    hw.sr = 0xf0u;
    bool visited = kui_retail_native_demand_wake();
    hw.sr = 0u;
    check_original_sdk_owner();
    return visited;
}
#endif

#if KUI_TOY_SUITE_DIRECT_PIO
static void direct_pio_check_status(uint32_t token) {
    unsigned clocks = card.clocks, writes = hw.writes, blocks = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_PROCESSING);
    CHECK(card.clocks == clocks && hw.writes == writes && R.engine.stats.call_blocks == blocks);
}

static void direct_pio_finish(uint32_t token, uint32_t lba) {
    for(unsigned visits = 0u; R.shared.service.pending; ++visits) {
        CHECK(visits < 256u && !R.engine.failed && !kui_sci_stream_busy());
        CHECK(independent_demand_wake());
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * 2048u && !get(STATUS + 4u));
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_NOT_FOUND);
    compare(lba, MOVIE_SECTORS, OUTPUT);
    CHECK(!R.engine.hooked && !R.engine.release.armed && hw.iprb == 0x5a0fu);
}

static void direct_pio_run(bool fragmented, bool valid_clock, uint32_t extra_ticks,
    uint32_t caller, unsigned blocks_per_exec) {
    setup_movie_card_extents(1u, fragmented ? 28u : 128u, fragmented);
    struct kui_native_ra_config configuration = movie_configuration();
    configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    bind_movie_configuration(&configuration);
    m.chcr = 1u; /* Programmed reads leave even an occupied guest DMA channel alone. */
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.shared.service.pending && !kui_sci_stream_busy() && payload_calls == 1u);
    CHECK(!hw.writes && card.cmd18 == 1u && !card.cmd12);
    direct_pio_check_status(token);

    refill_clock_valid = valid_clock;
    refill_clock_extra_ticks = extra_ticks;
    unsigned before = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, caller) == 0);
    CHECK(R.engine.stats.call_blocks - before == blocks_per_exec);
    CHECK(payload_calls == blocks_per_exec + 1u && card.cmd18 == 1u && !card.cmd12);
    refill_clock_valid = true;
    refill_clock_extra_ticks = 0u;

    direct_pio_finish(token, MOVIE_LBA);
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    const struct kui_toy_demand_movie_trace *movie = &kui_native_readahead_aux_context.demand.movie[1];
    CHECK(!sd->overruns && !sd->crc_errors && !m.dma_starts && !m.module_resets);
    CHECK(m.chcr == 1u && m.sar == 0x11111111u && m.dar == 0x22222222u && m.tcr == 0x33u);
    CHECK(payload_calls == sd->polled && sd->toy_pio_window_ok == payload_calls);
    CHECK(movie->window_pio == payload_calls && movie->window_ok == payload_calls);
    CHECK(payload_bytes == payload_calls * KUI_SCI_SD_PIO_PAYLOAD_BYTES);
    CHECK(card.cmd18 == (fragmented ? 3u : 1u) && card.cmd12 + 1u == card.cmd18);
}

static void direct_pio_scope(bool verified, uint32_t lba) {
    setup_movie_card(1u, 128u);
    struct kui_native_ra_config configuration = movie_configuration();
    configuration.toy_poll_owner = verified;
    configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    bind_movie_configuration(&configuration);
    uint32_t token = (uint32_t)request(lba, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_sci_stream_busy() && m.dma_starts == 1u && !payload_calls);
    CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
    CHECK(!payload_calls && !kui_sci_stream_stats()->polled);
    compare(lba, MOVIE_SECTORS, OUTPUT);
}

static void direct_pio_crc_retry(void) {
    setup_movie(385u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(card.cmd18 == 1u && !payload_calls && !hw.writes);
    card.nac_first = 1u;
    card.corrupt_lba = damaged;
    card.corrupt_count = 1u;
    for(unsigned visits = 0u; !sd->crc_errors; ++visits) {
        CHECK(visits < 16u && !kui_sci_stream_busy() && independent_demand_wake());
        CHECK(!hw.writes && R.engine.cursor.block == damaged && !sd->toy_pio_window_ok);
    }
    CHECK(!card.corrupt_count && !m.dma_starts);
    direct_pio_finish(token, MOVIE_LBA);
    CHECK(sd->crc_errors == 1u && card.cmd18 == 2u && card.cmd12 == 1u && !m.dma_starts);
    CHECK(payload_calls > sd->toy_pio_window_ok);
}

static void direct_pio_partial_failure(void) {
    setup_movie(1u, true);
    payload_fail_after = 31u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    for(unsigned visits = 0u; R.shared.service.pending; ++visits) {
        CHECK(visits < 24u && !kui_sci_stream_busy() && !hw.writes);
        CHECK(independent_demand_wake());
    }
    CHECK(payload_calls == 1u && payload_bytes == payload_fail_after && !hw.writes);
    CHECK(!kui_sci_stream_stats()->toy_pio_window_ok && !kui_sci_stream_stats()->polled);
    CHECK(!m.dma_starts && !m.healthy && m.cs_high);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_IO && !get(STATUS + 8u));
}

static void direct_pio_cancel(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(payload_calls == 1u && !hw.writes);
    CHECK(gd(KUI_GD_ABORT, token, 0u) == 0 && !R.shared.service.pending);
    unsigned writes = hw.writes, clocks = card.clocks, payloads = payload_calls;
    hw.frozen = true;
    CHECK(!independent_demand_wake());
    CHECK(card.clocks == clocks && hw.writes == writes && payload_calls == payloads);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED);
    hw.frozen = false;
    read_and_compare(MOVIE_LBA - 1u, 1u, 0u, 1u);
    CHECK(payload_calls == payloads && m.dma_starts);
}
#else
static void independent_sdk_irq(void) {
    CHECK(kui_sci_stream_busy() && R.engine.hooked);
    CHECK(R.engine.release.armed == KUI_RETAIL_SCI_PRIORITY_FIELD);
    elapse();
    CHECK(kui_retail_sdk_sci_dispatch(0x4e0u) == 0u);
    check_original_sdk_owner();
}

#if KUI_TOY_SUITE_MODE == 1
static void baseline_idle_after_overrun(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    m.overrun_after = 200u;
    independent_sdk_irq();
    CHECK(R.engine.active && R.shared.service.pending);
    CHECK(R.shared.service.token == token && !R.engine.cursor.done && !hw.writes);
    CHECK(!kui_sci_stream_busy() && !R.engine.hooked);
    unsigned clocks = card.clocks;
    for(unsigned n = 0u; n < 8u; ++n) {
        elapse();
        CHECK(kui_retail_sdk_sci_dispatch(0x4e0u) == UINT32_C(0x8c1add0c));
    }
    CHECK(card.clocks == clocks && !hw.writes && !R.engine.cursor.done);
    CHECK(R.engine.active && !R.engine.failed && R.shared.service.pending);
    printf("Mode 1 SDK route: 14 raw-image sectors accepted; overrun left DMA idle and request pending\n");
}
#endif

#if KUI_TOY_SUITE_ASYNC_DEMAND
static void finish_without_polls(uint32_t token) {
    unsigned gd_blocks = R.engine.stats.call_blocks;
    unsigned irqs = 0u, wakes = 0u;
    for(unsigned n = 0u; R.engine.active; ++n) {
        CHECK(n < 512u);
        if(kui_sci_stream_busy()) {
            independent_sdk_irq();
            ++irqs;
        } else {
            (void)independent_demand_wake();
            ++wakes;
        }
        CHECK(!R.engine.failed);
    }
    CHECK(irqs > 0u && wakes > 0u);
    CHECK(R.engine.stats.call_blocks == gd_blocks);
    CHECK(R.engine.cursor.done == MOVIE_SECTORS);
    CHECK(R.shared.service.token == token && !R.shared.service.pending);
    CHECK(R.shared.service.status == KUI_GD_COMPLETED);
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    unsigned clocks = card.clocks, writes = hw.writes;
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * 2048u && !get(STATUS + 4u));
    CHECK(card.clocks == clocks && hw.writes == writes);
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_NOT_FOUND);
    CHECK(!R.engine.hooked && !R.engine.release.armed && hw.iprb == 0x5a0fu);
    printf("Mode %u: exact 28,672 bytes from %u independent wake visits and %u SCI interrupts\n",
        KUI_TOY_SUITE_MODE, wakes, irqs);
}

static void recover_overrun_without_polls(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && kui_sci_stream_busy());
    m.overrun_after = 200u;
    card.nac_first = 385u;
    independent_sdk_irq();
    CHECK(R.engine.active && R.shared.service.pending && !hw.writes);
    CHECK(!kui_sci_stream_busy());
    finish_without_polls(token);
}

static void rearm_changed_sci_priority(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_sci_stream_busy() && R.engine.hooked);
    unsigned clocks = card.clocks, starts = m.dma_starts;
    /* The game changes IPRB while retaining the installed SDK callbacks. */
    hw.iprb = 0x5a0fu;
    (void)independent_demand_wake();
    CHECK(hw.iprb == READER_IPRB && R.engine.release.sci == 0u);
    CHECK(card.clocks == clocks && m.dma_starts == starts && !hw.writes);
    m.overrun_after = 200u;
    card.nac_first = 385u;
    independent_sdk_irq();
    finish_without_polls(token);
}

static void sliced_token_without_polls(void) {
    setup_movie(385u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && R.shared.service.pending && !hw.writes);
    CHECK(!kui_sci_stream_busy());
    CHECK(card.cmd18 == 1u && card.clocks < 385u);
    finish_without_polls(token);
}

static unsigned busy_for_stop, extra_busy_bytes;
static uint8_t slow_stop_transfer(void *context, uint8_t value, bool slow) {
    /* The common fixture's queue holds only 64 bytes. Extend the physical
     * card's CMD12 busy response after its queued stuff, R1 and busy bytes. */
    if(!m.cs_high && value == 0xffu && card.cmd12 == busy_for_stop &&
       !card.qlen && !card.cmd_len && !card.streaming && !card.start_after_queue && extra_busy_bytes) {
        CHECK(!slow && m.healthy && !m.rx && !m.brr);
        --extra_busy_bytes;
        ++bus_bytes;
        ++card.clocks;
        return 0u;
    }
    return bus_transfer(context, value, slow);
}

static void sliced_stop_busy_without_polls(void) {
    setup_movie(1u, true);
    R.shared.card.device.sd.bus.transfer = slow_stop_transfer;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && kui_sci_stream_busy());
    unsigned starts = card.cmd18;
    busy_for_stop = card.cmd12 + 1u;
    extra_busy_bytes = 191u;
    card.nac_first = 385u;
    m.overrun_after = 200u;
    independent_sdk_irq();
    unsigned busy_visits = 0u;
    while(extra_busy_bytes) {
        CHECK(busy_visits++ < 8u && !kui_sci_stream_busy());
        unsigned before = extra_busy_bytes;
        (void)independent_demand_wake();
        CHECK(before - extra_busy_bytes <= KUI_SCI_STREAM_TOKEN_SLICE);
        CHECK(card.cmd12 <= busy_for_stop && !hw.writes);
        if(extra_busy_bytes) CHECK(card.cmd18 == starts && !kui_sci_stream_busy());
    }
    CHECK(busy_visits >= 2u && card.cmd12 == busy_for_stop);
    finish_without_polls(token);
}

static void check_is_status_only(void) {
    setup_movie(385u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && !kui_sci_stream_busy());
    unsigned clocks = card.clocks;
    for(unsigned n = 0u; n < 8u; ++n) {
        CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_PROCESSING);
        CHECK(card.clocks == clocks && !hw.writes);
        CHECK(!get(STATUS + 4u) && !get(STATUS + 8u));
    }
    finish_without_polls(token);
}

static void abort_stopped_receive(void) {
    setup_movie(385u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && !kui_sci_stream_busy());
    CHECK(gd(KUI_GD_ABORT, token, 0u) == 0);
    CHECK(!R.shared.service.pending && !R.engine.active);
    hw.frozen = true;
    unsigned writes = hw.writes, clocks = card.clocks;
    for(unsigned n = 0u; n < 8u; ++n) CHECK(!independent_demand_wake());
    CHECK(hw.writes == writes && card.clocks == clocks);
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED);
    CHECK(!kui_sci_stream_busy() && !R.engine.hooked && hw.iprb == 0x5a0fu);
    hw.frozen = false;
    card.nac_first = 385u;
    token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    finish_without_polls(token);
}

static uint32_t initial_masked_submit(unsigned sr) {
    setup_movie(385u, true);
    kui_retail_hook_sr = hw.sr = sr;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && !kui_sci_stream_busy());
    CHECK(card.clocks <= KUI_SCI_STREAM_TOKEN_SLICE + 26u);
    CHECK(!hw.writes && card.cmd18 == 1u);
    unsigned clocks = card.clocks;
    CHECK(gd(KUI_GD_EXEC, 0u, 0u) == 0);
    CHECK(card.clocks > clocks && card.clocks - clocks <= KUI_SCI_STREAM_TOKEN_SLICE);
    CHECK(!hw.writes && !kui_sci_stream_busy() && card.cmd18 == 1u);
    return token;
}

static void masked_caller_finishes(unsigned sr) {
    uint32_t token = initial_masked_submit(sr);
    unsigned gd_blocks = R.engine.stats.call_blocks;
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_PROCESSING);
    CHECK(R.engine.stats.call_blocks > gd_blocks);
    CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(!R.engine.hooked && hw.iprb == 0x5a0fu);
    check_original_sdk_owner();
    kui_retail_hook_sr = hw.sr = 0u;
}

static void masked_sdk_submit_then_independent(void) {
    uint32_t token = initial_masked_submit(0xf0u);
    /* The SDK restores its mask after the immediate submit EXEC. Later
     * hardware and worker events must finish without another GD call. */
    kui_retail_hook_sr = hw.sr = 0u;
    finish_without_polls(token);
}

static void outside_movie_scope_keeps_catchup(void) {
    setup_movie(385u, false);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.active && kui_sci_stream_busy() && card.clocks > 385u);
    unsigned gd_blocks = R.engine.stats.call_blocks;
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_PROCESSING);
    CHECK(R.engine.stats.call_blocks > gd_blocks);
    CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(!R.engine.hooked && hw.iprb == 0x5a0fu);
    check_original_sdk_owner();
}

static void check_terminal_io_error(uint32_t token) {
    CHECK(!R.shared.service.pending && R.shared.service.token == token);
    CHECK(R.shared.service.status == KUI_GD_FAILED);
    CHECK(R.shared.service.error == KUI_GD_ERROR_IO);
    CHECK(!hw.writes && !R.engine.cursor.done);
    /* The pipelined reader can have armed the next SD block before learning
     * the current CRC failed. Its final IRQ must only retire scratch DMA. */
    if(kui_sci_stream_busy()) independent_sdk_irq();
    CHECK(!hw.writes && !kui_sci_stream_busy());
    CHECK(R.shared.service.status == KUI_GD_FAILED && !R.shared.service.pending);
    CHECK(!R.engine.hooked && hw.iprb == 0x5a0fu);
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_IO && !get(STATUS + 8u));
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_NOT_FOUND);
}

static void fatal_irq_failure_without_polls(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    m.healthy = false;
    independent_sdk_irq();
    check_terminal_io_error(token);
}

static void repeated_damage_fails_without_polls(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    m.overrun_every = 200u;
    card.corrupt_lba = R.engine.cursor.block;
    card.corrupt_count = 20u;
    for(unsigned n = 0; R.shared.service.pending; ++n) {
        CHECK(n < 32u);
        if(kui_sci_stream_busy()) independent_sdk_irq();
        else (void)independent_demand_wake();
        CHECK(!hw.writes);
    }
    CHECK(kui_sci_stream_stats()->crc_errors > 0u);
    check_terminal_io_error(token);
}
#endif

#if KUI_TOY_SUITE_MOVIE_POLL
static void status_only_poll(uint32_t token, uint32_t outer_pr) {
    unsigned clocks = card.clocks, writes = hw.writes, blocks = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_CHECK, token, outer_pr) == KUI_GD_PROCESSING);
    CHECK(card.clocks == clocks && hw.writes == writes && R.engine.stats.call_blocks == blocks);
    CHECK(!get(STATUS + 4u) && get(STATUS + 8u) <= MOVIE_SECTORS * 2048u);
}

static void bounded_poll(uint32_t outer_pr) {
    unsigned blocks = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, outer_pr) == 0);
    CHECK(R.engine.stats.call_blocks - blocks <= 1u);
}

static void cut_movie_dma(uint32_t token) {
    CHECK(kui_sci_stream_busy() && m.dma_starts == 1u && card.cmd18 == 1u);
    CHECK(!kui_sci_stream_stats()->polled && !hw.writes && !m.rx_brr && !m.brr);

    /* Keep the fresh CMD18 token pending long enough to observe the
     * recovering request before the replacement receive is armed. */
    card.nac_first = 385u;
    m.overrun_after = 200u;
    independent_sdk_irq();
    CHECK(kui_sci_stream_stats()->overruns == 1u && !kui_sci_stream_busy());
    CHECK(!hw.writes && R.shared.service.pending && !R.engine.cursor.done && !m.brr);
    status_only_poll(token, event900_poll);
}

static uint32_t cut_first_movie_dma(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    cut_movie_dma(token);
    return token;
}

static void finish_recovery_poll_at(uint32_t token, uint32_t lba) {
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 512u && !R.engine.failed);
        if(kui_sci_stream_busy()) independent_sdk_irq();
        else bounded_poll(SINGLE_SERVICE_POLL);
        if(R.shared.service.pending) status_only_poll(token, event900_poll);
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * 2048u && !get(STATUS + 4u));
    compare(lba, MOVIE_SECTORS, OUTPUT);
}

static void finish_recovery_in_movie_poll(uint32_t token) {
    finish_recovery_poll_at(token, MOVIE_LBA);
}

#if KUI_TOY_SUITE_REFILL_SLICE
static void refill_slice_poll(uint32_t outer_pr, bool valid_clock, uint32_t extra_ticks,
    unsigned expected_blocks) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_sci_stream_busy());
    const struct kui_toy_demand_trace *trace = &kui_native_readahead_aux_context.demand;
    const struct kui_toy_demand_movie_trace *movie = &trace->movie[1];
    CHECK(!movie->slice_visits && !movie->slice_blocks);
    m.delay = 0u;
    elapse();
    refill_clock_valid = valid_clock;
    refill_clock_extra_ticks = extra_ticks;
    unsigned before = R.engine.stats.call_blocks;
    status_only_poll(token, outer_pr);
    CHECK(!movie->slice_visits && !movie->slice_blocks);
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, outer_pr) == 0);
    CHECK(R.engine.stats.call_blocks - before == expected_blocks);
    CHECK(movie->slice_visits == (expected_blocks > 1u));
    CHECK(movie->slice_blocks == expected_blocks - 1u);
    CHECK(!trace->movie[0].slice_visits && !trace->movie[0].slice_blocks);
    CHECK(!R.engine.stats.irq_blocks && !R.engine.failed);
    status_only_poll(token, outer_pr);
    refill_clock_valid = true;
    refill_clock_extra_ticks = 0u;
    m.delay = RECEIVE_POLLS;
    finish_recovery_in_movie_poll(token);
}

static void refill_slice_keeps_one_pio_block(void) {
    setup_movie(1u, true);
    m.chcr = 1u; /* Guest-owned DMA1 forces programmed reads. */
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    const struct kui_toy_demand_movie_trace *movie = &kui_native_readahead_aux_context.demand.movie[1];
    CHECK(sd->polled == 1u && !m.dma_starts && !kui_sci_stream_busy());
    unsigned before = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, event900_poll) == 0);
    CHECK(sd->polled == 2u && R.engine.stats.call_blocks - before == 1u);
    CHECK(!movie->slice_visits && !movie->slice_blocks);
    CHECK(m.chcr == 1u && !m.dma_starts);
#if KUI_TOY_SUITE_PIO_WINDOW
    CHECK(!sd->toy_pio_window_blocks && !sd->toy_pio_window_ok);
#endif
#if KUI_TOY_SUITE_PIPELINED_PIO
    CHECK(!payload_calls && !payload_bytes);
#endif
    m.chcr = 0u;
    finish_recovery_in_movie_poll(token);
}
#endif

#if KUI_TOY_SUITE_AFTER_START
static void fixture_movie_started(uint32_t flags, uint32_t status) {
    struct kui_toy_movie_exit_witness *witness = &kui_native_readahead_aux_context.movie_exit[1];
    witness->flags = flags;
    witness->start_status = status;
}
#endif

#if KUI_TOY_SUITE_SEQUENTIAL_PIO
static void sequential_pio_poll(uint32_t caller, uint32_t flags, uint32_t status,
    bool verified, bool valid_clock, unsigned expected_payloads) {
    setup_movie_card(1u, 128u);
    struct kui_native_ra_config configuration = movie_configuration();
    configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    configuration.toy_poll_owner = verified;
    bind_movie_configuration(&configuration);
    fixture_movie_started(flags, status);

    /* The ordinary initial receive parks a real CMD18. The new route
     * must begin only on the eligible foreground callback. */
    m.chcr = 1u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(sd->polled == 1u && !payload_calls && !m.dma_starts);
    CHECK(card.cmd18 == 1u && !card.cmd12 && !kui_sci_stream_busy());
    m.chcr = 0u;
    if(verified) status_only_poll(token, caller);
    refill_clock_valid = valid_clock;
    unsigned before = R.engine.stats.call_blocks;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, caller) == 0);
    CHECK(payload_calls == expected_payloads);
    if(expected_payloads) {
        CHECK(R.engine.stats.call_blocks - before == expected_payloads);
        CHECK(sd->polled == expected_payloads + 1u && !m.dma_starts);
        CHECK(card.cmd18 == 1u && !card.cmd12 && !kui_sci_stream_busy());
    }
    refill_clock_valid = true;
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL, status);
    if(verified) finish_recovery_in_movie_poll(token);
    else {
        CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
        compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    }
    CHECK(payload_calls == expected_payloads && sd->toy_pio_window_ok == expected_payloads);
    CHECK(!sd->crc_errors && !sd->overruns);
}

static void sequential_pio_keeps_wake_baseline(void) {
    setup_movie(1u, true);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
    m.chcr = 1u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(!kui_sci_stream_busy() && !payload_calls && card.cmd18 == 1u);
    m.chcr = 0u;
    CHECK(independent_demand_wake());
    CHECK(kui_sci_stream_busy() && m.dma_starts == 1u && !payload_calls);
    fixture_movie_started(0u, 0u);
    finish_recovery_in_movie_poll(token);
    CHECK(!payload_calls);
}
#endif

#if KUI_TOY_SUITE_BOUNDED_DMA
static void bounded_dma_poll(uint32_t caller, uint32_t flags, uint32_t status,
    unsigned target, bool guest_dma, unsigned expected_blocks) {
    setup_movie(1u, true);
    fixture_movie_started(flags, status);
    if(guest_dma) m.chcr = 1u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(kui_sci_stream_busy() == !guest_dma && m.dma_starts == !guest_dma);
    R.engine.target = target;
    status_only_poll(token, caller);
    unsigned before = R.engine.stats.call_blocks;
    unsigned polled = sd->polled;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, caller) == 0);
    CHECK(R.engine.stats.call_blocks - before == expected_blocks);
    CHECK(R.engine.target == target && !R.shared.stream.toy_suite_poll);
    CHECK(sd->polled - polled == (unsigned)guest_dma && !payload_calls);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    if(guest_dma) CHECK(m.chcr == 1u && !m.dma_starts);
    m.chcr = guest_dma ? 0u : m.chcr;
    fixture_movie_started(0u, 0u);
    finish_recovery_in_movie_poll(token);
    CHECK(!sd->crc_errors && !sd->overruns);
}
#endif

static void first_cut_retry_route(void) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    for(unsigned n = 0u; !kui_sci_stream_busy() && !sd->polled; ++n) {
        CHECK(n < 16u);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && !card.idle_cmd12);
    CHECK(R.engine.cursor.block == damaged && !hw.writes);
#if KUI_TOY_SUITE_MODE == 34
    /* The complete retry is staged but still needs its own CRC check.
     * The card consumed precisely this physical block with no DMA rearm. */
    CHECK(sd->polled == 1u && m.dma_starts == 1u && !kui_sci_stream_busy());
    CHECK(card.lba == damaged + 1u && kui_sci_stream_ready(damaged));
    bounded_poll(SINGLE_SERVICE_POLL);
#else
    CHECK(!sd->polled && m.dma_starts == 2u && kui_sci_stream_busy());
    CHECK(card.lba == damaged);
    CHECK(m.rx_brr == (KUI_TOY_SUITE_SLOW_RETRY ? 1u : 0u));
    independent_sdk_irq();
#endif
    CHECK(R.engine.cursor.block == damaged + 1u && hw.writes);
    CHECK(card.lba == damaged + 1u && kui_sci_stream_busy() && !m.rx_brr && !m.brr);
    CHECK(m.dma_starts == (KUI_TOY_SUITE_MODE == 34 ? 2u : 3u));
    finish_recovery_in_movie_poll(token);
    CHECK(sd->overruns == 1u && !sd->crc_errors);
#if KUI_TOY_SUITE_SLOW_RETRY
    const struct kui_toy_demand_trace *trace = &kui_native_readahead_aux_context.demand;
    const struct kui_toy_demand_movie_trace *movie = &trace->movie[1];
    CHECK(sd->toy_retry_orer == 1u && sd->toy_retry_slow == 1u && sd->toy_retry_ok == 1u);
#if KUI_TOY_SUITE_PIO_WINDOW
    CHECK(movie->overruns == 1u && !movie->window_pio && !movie->window_ok);
    CHECK(!trace->movie[0].overruns && !trace->movie[0].window_pio && !trace->movie[0].window_ok);
#else
    CHECK(movie->overruns == 1u && movie->retry_orer == 1u &&
          movie->retry_slow == 1u && movie->retry_ok == 1u);
    CHECK(!trace->movie[0].overruns && !trace->movie[0].retry_orer &&
          !trace->movie[0].retry_slow && !trace->movie[0].retry_ok);
#endif
#endif
    printf("Mode %u: incomplete receive retries same LBA by %s, then resumes next-LBA DMA\n",
        KUI_TOY_SUITE_MODE, KUI_TOY_SUITE_MODE == 34 ? "checked PIO" :
        KUI_TOY_SUITE_SLOW_RETRY ? "checked 6.25 MHz DMA" : "DMA");
}

#if KUI_TOY_SUITE_MODE == 34
static void recovery_still_requires_crc(void) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    card.corrupt_lba = damaged;
    card.corrupt_count = 1u;
    for(unsigned n = 0u; !sd->crc_errors; ++n) {
        CHECK(n < 16u && !kui_sci_stream_busy());
        bounded_poll(SINGLE_SERVICE_POLL);
        CHECK(!hw.writes && R.engine.cursor.block == damaged);
    }
    CHECK(sd->polled == 1u && sd->crc_errors == 1u && !card.corrupt_count);
    finish_recovery_in_movie_poll(token);
    CHECK(sd->polled >= 2u && sd->overruns == 1u && sd->crc_errors == 1u);
}
#endif

#if KUI_TOY_SUITE_MODE == 34 || KUI_TOY_SUITE_SLOW_RETRY
static void cancelled_recovery_does_not_survive_new_token(void) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    for(unsigned n = 0u; card.cmd18 < 2u; ++n) {
        CHECK(n < 8u);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(!kui_sci_stream_busy() && card.lba == damaged);
    CHECK(!kui_sci_stream_stats()->polled && m.dma_starts == 1u);
    hw.frozen = true;
    CHECK(gd(KUI_GD_ABORT, token, 0u) == 0 && !R.shared.service.pending);
    unsigned clocks = card.clocks;
    CHECK(!independent_demand_wake() && card.clocks == clocks);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED);
    CHECK(!hw.writes && !kui_sci_stream_stats()->polled);
    hw.frozen = false;

    card.nac_first = 1u;
    uint32_t replacement = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(replacement != token);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && !kui_sci_stream_stats()->polled);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(m.dma_starts == 2u && card.lba == damaged && !kui_sci_stream_stats()->polled);
    CHECK(!m.rx_brr && !m.brr);
    finish_recovery_in_movie_poll(replacement);
}

static void nonmovie_cut_keeps_dma_retry(void) {
    setup_movie(1u, false);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    uint32_t damaged = R.engine.cursor.block;
    CHECK(kui_sci_stream_busy() && m.dma_starts == 1u);
    m.overrun_after = 200u;
    independent_sdk_irq();
    CHECK(kui_sci_stream_busy() && m.dma_starts == 2u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && card.lba == damaged);
    CHECK(!m.rx_brr && !m.brr);
    CHECK(kui_sci_stream_stats()->overruns == 1u && !kui_sci_stream_stats()->polled);
    CHECK(!hw.writes && R.engine.cursor.block == damaged);
    CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
}
#endif

#if KUI_TOY_SUITE_SLOW_RETRY
static void fail_slow_movie_retry(bool corrupt_crc) {
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && !sd->polled && !hw.writes);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(card.lba == damaged && m.rx_brr == 1u && m.brr == 1u && m.dma_starts == 2u);
    if(corrupt_crc) {
        card.corrupt_lba = damaged;
        card.corrupt_count = 1u;
    } else {
        m.overrun_after = 200u;
    }
    independent_sdk_irq();
    CHECK(!hw.writes && R.engine.cursor.block == damaged && !m.brr);
    CHECK(sd->crc_errors == (unsigned)corrupt_crc && sd->overruns == (corrupt_crc ? 1u : 2u));
    CHECK(sd->toy_retry_orer == sd->overruns && sd->toy_retry_slow == 1u && !sd->toy_retry_ok);
}

#if KUI_TOY_SUITE_PIO_WINDOW
static void advance_to_pio(unsigned count) {
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    for(unsigned n = 0u; sd->polled < count; ++n) {
        CHECK(n < 24u);
        if(kui_sci_stream_busy()) independent_sdk_irq();
        else bounded_poll(SINGLE_SERVICE_POLL);
        CHECK(!m.brr);
    }
    CHECK(sd->polled == count);
}
#endif

static void slow_retry_keeps_pio_fallback(bool corrupt_crc) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    fail_slow_movie_retry(corrupt_crc);
    for(unsigned n = 0u; !sd->polled; ++n) {
        CHECK(n < 24u && !hw.writes && R.engine.cursor.block == damaged);
        if(kui_sci_stream_busy()) independent_sdk_irq();
        else bounded_poll(SINGLE_SERVICE_POLL);
        CHECK(!m.brr);
    }
    CHECK(!hw.writes && kui_sci_stream_ready(damaged) && card.lba == damaged + 1u);
    CHECK(card.cmd18 >= 3u && !card.idle_cmd12);
#if KUI_TOY_SUITE_PIO_WINDOW
    if(!corrupt_crc) {
        unsigned starts = card.cmd18, stops = card.cmd12;
        for(unsigned offset = 0u; offset < 3u; ++offset) {
            advance_to_pio(offset + 1u);
            CHECK(kui_sci_stream_ready(damaged + offset));
            CHECK(R.engine.cursor.block == damaged + offset && !kui_sci_stream_busy());
            CHECK(card.cmd18 == starts && card.cmd12 == stops);
            CHECK(sd->toy_pio_window_blocks == offset + 1u && sd->toy_pio_window_ok == offset);
            bounded_poll(SINGLE_SERVICE_POLL);
            CHECK(R.engine.cursor.block == damaged + offset + 1u);
            CHECK(sd->toy_pio_window_ok == offset + 1u);
        }
        for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
            CHECK(n < 8u && sd->polled == 3u);
            bounded_poll(SINGLE_SERVICE_POLL);
        }
        CHECK(card.lba == damaged + 3u);
        CHECK(card.cmd18 == starts && card.cmd12 == stops);
        CHECK(m.dma_starts == 3u && !m.rx_brr && !m.brr);
    }
#endif
    finish_recovery_in_movie_poll(token);
    unsigned window_blocks = KUI_TOY_SUITE_PIO_WINDOW && !corrupt_crc ? 3u : 0u;
    CHECK(sd->polled == (window_blocks ? window_blocks : 1u));
#if KUI_TOY_SUITE_PIO_WINDOW
    CHECK(sd->toy_pio_window_blocks == window_blocks && sd->toy_pio_window_ok == window_blocks);
    const struct kui_toy_demand_trace *trace = &kui_native_readahead_aux_context.demand;
    CHECK(trace->movie[1].window_pio == window_blocks && trace->movie[1].window_ok == window_blocks);
    CHECK(!trace->movie[0].window_pio && !trace->movie[0].window_ok);
#endif
#if KUI_TOY_SUITE_PIPELINED_PIO
    CHECK(payload_calls == window_blocks);
    CHECK(payload_bytes == window_blocks * KUI_SCI_SD_PIO_PAYLOAD_BYTES);
#endif
    CHECK(sd->toy_retry_slow == 1u && !sd->toy_retry_ok);
    CHECK(!m.brr && !m.rx_brr);
}

static void nonorer_stop_keeps_fast_retry(void) {
    setup_movie(1u, true);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(kui_sci_stream_busy() && !m.rx_brr && !(m.ssr & ORER));
    /* Simulate an owned channel stopped before its receive could finish. */
    m.stall = true;
    m.chcr &= ~3u;
    card.nac_first = 385u;
    independent_sdk_irq();
    m.stall = false;
    CHECK(!kui_sci_stream_busy() && sd->overruns == 1u && !sd->toy_retry_orer);
    CHECK(!hw.writes && R.engine.cursor.block == damaged && !m.brr);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && !sd->polled && !hw.writes);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(card.lba == damaged && card.cmd18 == 2u && !m.rx_brr && !m.brr);
    finish_recovery_in_movie_poll(token);
    CHECK(!sd->toy_retry_slow && !sd->toy_retry_ok && !sd->crc_errors);
}

static void unverified_movie_keeps_fast_retry(void) {
    setup_movie_card(1u, 48u);
    struct kui_native_ra_config configuration = movie_configuration();
    configuration.toy_poll_owner = 0u;
    configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    bind_movie_configuration(&configuration);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(kui_sci_stream_busy() && !m.rx_brr);
    m.overrun_after = 200u;
    card.nac_first = 385u;
    independent_sdk_irq();
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && !sd->polled && !hw.writes);
        (void)independent_demand_wake();
    }
    CHECK(card.lba == damaged && card.cmd18 == 2u && !m.rx_brr && !m.brr);
    finish_without_polls(token);
    CHECK(sd->overruns == 1u && !sd->toy_retry_slow && !sd->toy_retry_ok && !sd->crc_errors);
}

#if KUI_TOY_SUITE_PIO_WINDOW
static void pio_window_crc_failure_restarts_same_block(void) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    fail_slow_movie_retry(false);
    card.corrupt_lba = damaged;
    card.corrupt_count = 1u;
    advance_to_pio(1u);
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(!hw.writes && kui_sci_stream_ready(damaged));
    CHECK(sd->toy_pio_window_blocks == 1u && !sd->toy_pio_window_ok);
    CHECK(!sd->crc_errors);
    unsigned starts = card.cmd18;
    bounded_poll(SINGLE_SERVICE_POLL);
    CHECK(!hw.writes && R.engine.cursor.block == damaged);
    CHECK(sd->crc_errors == 1u && !sd->toy_pio_window_ok);
    unsigned attempted = sd->toy_pio_window_blocks;
    CHECK(attempted >= 1u && attempted <= 2u);
    for(unsigned n = 0u; !kui_sci_stream_ready(damaged); ++n) {
        CHECK(n < 24u && !hw.writes && R.engine.cursor.block == damaged);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(card.cmd18 == starts + 1u);
    CHECK(sd->toy_pio_window_blocks == attempted);
    finish_recovery_in_movie_poll(token);
    CHECK(sd->toy_pio_window_blocks == attempted && sd->toy_pio_window_ok < attempted);
}

static void pio_window_cancel_replaces_request(bool seek) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    fail_slow_movie_retry(false);
    advance_to_pio(1u);
    CHECK(!hw.writes && kui_sci_stream_ready(damaged));
    CHECK(gd(KUI_GD_ABORT, token, 0u) == 0 && !R.shared.service.pending);
    /* Existing dispatch can deliver ready data before processing ABORT.
     * No visit after it accepts cancellation may deliver another byte. */
    unsigned written = hw.writes, polled = kui_sci_stream_stats()->polled;
    hw.frozen = true;
    CHECK(!independent_demand_wake() && hw.writes == written);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED);
    hw.frozen = false;
    card.nac_first = 1u;
    uint32_t lba = MOVIE_LBA + (seek ? 6u : 0u);
    uint32_t replacement = (uint32_t)request(lba, MOVIE_SECTORS, OUTPUT);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && kui_sci_stream_stats()->polled == polled);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(replacement != token && !m.rx_brr);
    if(seek) CHECK(card.lba == R.engine.cursor.block && card.lba != damaged);
    finish_recovery_poll_at(replacement, lba);
    CHECK(kui_sci_stream_stats()->polled == polled);
}

static void pio_window_stops_at_extent_break(void) {
    setup_movie_card_extents(1u, 28u, true);
    struct kui_native_ra_config configuration = movie_configuration();
    configuration.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    bind_movie_configuration(&configuration);
    const uint32_t lba = MOVIE_LBA + 5u; /* Last physical block of the first data extent. */
    uint32_t token = (uint32_t)request(lba, MOVIE_SECTORS, OUTPUT);
    CHECK(R.engine.cursor.run == 1u);
    cut_movie_dma(token);
    uint32_t damaged = R.engine.cursor.block;
    fail_slow_movie_retry(false);
    advance_to_pio(1u);
    CHECK(!hw.writes && kui_sci_stream_ready(damaged));
    unsigned starts = card.cmd18;
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 32u && kui_sci_stream_stats()->polled == 1u);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(card.cmd18 == starts + 1u && card.lba == R.engine.cursor.block);
    CHECK(card.lba != damaged && card.lba != damaged + 1u && !m.rx_brr);
    finish_recovery_poll_at(token, lba);
    CHECK(kui_sci_stream_stats()->polled == 1u);
}

#if KUI_TOY_SUITE_PIPELINED_PIO
static void failed_payload_never_reaches_guest(void) {
    uint32_t token = cut_first_movie_dma();
    uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    fail_slow_movie_retry(false);
    CHECK(!payload_calls && !hw.writes);
    payload_fail_after = 31u;
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 24u && !hw.writes);
        bounded_poll(SINGLE_SERVICE_POLL);
    }
    CHECK(payload_calls == 1u && payload_bytes == payload_fail_after);
    CHECK(!hw.writes && !sd->polled && !sd->toy_pio_window_ok);
    CHECK(!kui_sci_stream_ready(damaged) && m.cs_high && !m.healthy);
    CHECK(!R.shared.stream.toy_suite_slow_pending);
    check_terminal_io_error(token);
}
#endif
#if KUI_TOY_SUITE_FIRST_CUT_PIO
static uint32_t cut_started_movie(void) {
    setup_movie(1u, true);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(R.shared.stream.toy_suite_active & KUI_TOY_SUITE_FIRST_CUT_ACTIVE);
    cut_movie_dma(token);
    CHECK(R.shared.stream.toy_suite_slow_pending == KUI_TOY_SCI_PIO_WINDOW_FIRST);
    CHECK(!kui_sci_stream_stats()->toy_retry_slow && !payload_calls);
    return token;
}

static void wake_first_cut_pio(unsigned receives) {
    for(unsigned n = 0u; payload_calls < receives; ++n) {
        CHECK(n < 24u && !kui_sci_stream_busy());
        CHECK(independent_demand_wake());
        CHECK(!m.brr && !m.rx_brr);
    }
    CHECK(payload_calls == receives);
}

static void first_cut_pio_keeps_one_cmd18_window(void) {
    uint32_t token = cut_started_movie();
    const uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    wake_first_cut_pio(1u);
    CHECK(!hw.writes && kui_sci_stream_ready(damaged) && card.lba == damaged + 1u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && m.dma_starts == 1u);
    CHECK(sd->toy_pio_window_blocks == 1u && !sd->toy_pio_window_ok);
    wake_first_cut_pio(3u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && m.dma_starts == 1u);
    CHECK(card.lba == damaged + 3u && sd->toy_pio_window_blocks == 3u);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && payload_calls == 3u);
        CHECK(independent_demand_wake());
    }
    CHECK(card.lba == damaged + 3u && m.dma_starts == 2u && !m.rx_brr);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u && sd->toy_pio_window_ok == 3u);
    CHECK(!sd->toy_retry_slow && !sd->toy_retry_ok);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    finish_recovery_in_movie_poll(token);
    CHECK(!sd->crc_errors && sd->overruns == 1u && sd->polled == 3u);
    CHECK(card.cmd12 == 1u && card.cmd18 == 2u);
}

static void first_cut_pio_discards_bad_crc(void) {
    uint32_t token = cut_started_movie();
    const uint32_t damaged = R.engine.cursor.block;
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    card.corrupt_lba = damaged;
    card.corrupt_count = 1u;
    wake_first_cut_pio(1u);
    CHECK(!hw.writes && !sd->toy_pio_window_ok);
    for(unsigned n = 0u; !sd->crc_errors; ++n) {
        CHECK(n < 8u && !kui_sci_stream_busy() && independent_demand_wake());
        CHECK(!hw.writes && R.engine.cursor.block == damaged);
    }
    CHECK(sd->crc_errors == 1u && !sd->toy_pio_window_ok && !card.corrupt_count);
    for(unsigned n = 0u; card.cmd18 < 3u; ++n) {
        CHECK(n < 16u && !kui_sci_stream_busy() && independent_demand_wake());
        CHECK(!hw.writes && R.engine.cursor.block == damaged);
    }
    CHECK(card.lba == damaged && card.cmd12 == 2u && !sd->toy_retry_slow);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    finish_recovery_in_movie_poll(token);
    CHECK(sd->crc_errors == 1u && sd->overruns == 1u);
}

static void first_cut_pio_cancel_drops_window(void) {
    uint32_t token = cut_started_movie();
    wake_first_cut_pio(1u);
    CHECK(!hw.writes && R.shared.stream.toy_suite_slow_pending);
    CHECK(gd(KUI_GD_ABORT, token, 0u) == 0 && !R.shared.service.pending);
    unsigned payloads = payload_calls, clocks = card.clocks, writes = hw.writes;
    CHECK(!independent_demand_wake() && payload_calls == payloads &&
          card.clocks == clocks && hw.writes == writes);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED);
    CHECK(!R.shared.stream.toy_suite_slow_pending);

    card.nac_first = 1u;
    uint32_t replacement = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(replacement != token);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && payload_calls == payloads && independent_demand_wake());
    }
    CHECK(!m.rx_brr && payload_calls == payloads);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_TERMINAL, 0x07dbu);
    finish_recovery_in_movie_poll(replacement);
    CHECK(payload_calls == payloads);
}
#endif
#endif
#endif

static void masked_movie_poll_with_irqs(void) {
    uint32_t token = initial_masked_submit(POLL_SR);
    kui_retail_hook_sr = hw.sr = 0u;
    unsigned clocks = card.clocks;
    bounded_poll(SINGLE_SERVICE_POLL);
    status_only_poll(token, event900_poll);
    CHECK(card.clocks > clocks && card.clocks - clocks <= KUI_SCI_STREAM_TOKEN_SLICE);
    CHECK(!hw.writes && !kui_sci_stream_busy());
    unsigned irqs = 0u, overruns = 0u;
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 512u);
        bounded_poll(SINGLE_SERVICE_POLL);
        if(!R.shared.service.pending) break;
        status_only_poll(token, event900_poll);
        if(kui_sci_stream_busy()) {
            /* A second EXEC at IMASK=6 arrives before the hardware event. */
            clocks = card.clocks;
            bounded_poll(SINGLE_SERVICE_POLL);
            CHECK(card.clocks == clocks && m.rx_delay);
            status_only_poll(token, event900_poll);
            if(overruns < 3u && !(irqs % 4u)) {
                m.overrun_after = 200u;
                ++overruns;
            }
            CHECK(hw.sr == 0u);
            independent_sdk_irq();
            ++irqs;
        }
        CHECK(!R.engine.failed);
    }
    CHECK(overruns == 3u && irqs > overruns);
    CHECK(kui_sci_stream_stats()->overruns >= overruns);
    CHECK(sdk_poll(KUI_GD_CHECK, token, event900_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * 2048u && !get(STATUS + 4u));
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
}

static void masked_movie_poll_without_irqs(void) {
    uint32_t token = initial_masked_submit(POLL_SR);
    R.vector600[KUI_NATIVE_SDK_SCI_COOKIE_INDEX] = 0u;
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 512u);
        unsigned clocks = card.clocks;
        bool receiving = kui_sci_stream_busy();
        bounded_poll(movie_async_poll);
        if(!receiving && !hw.writes) CHECK(card.clocks - clocks <= KUI_SCI_STREAM_TOKEN_SLICE);
        CHECK(!R.engine.failed && !R.engine.release.armed);
        if(!R.shared.service.pending) break;
        status_only_poll(token, movie_async_poll);
        elapse();
    }
    CHECK(hw.sr == POLL_SR && !R.engine.stats.irq_blocks);
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * 2048u && !get(STATUS + 4u));
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    check_original_sdk_owner();
    kui_retail_hook_sr = hw.sr = 0u;
}

static void check_masked_poll_fallback(uint32_t function, uint32_t token, uint32_t frame,
    unsigned expected_word_reads) {
    unsigned blocks = R.engine.stats.call_blocks;
    unsigned reads = poll_word_reads;
    CHECK(sdk_poll_saved_frame(function, token, frame) == (function == KUI_GD_CHECK ? KUI_GD_PROCESSING : 0));
    CHECK(poll_word_reads - reads == expected_word_reads);
    CHECK(R.engine.stats.call_blocks - blocks > 1u);
#if KUI_TOY_SUITE_REFILL_SLICE
    CHECK(!kui_native_readahead_aux_context.demand.movie[1].slice_visits &&
          !kui_native_readahead_aux_context.demand.movie[1].slice_blocks);
#endif
    CHECK(finish((int32_t)token, 0u, 1u) == KUI_GD_COMPLETED);
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    kui_retail_hook_sr = hw.sr = 0u;
}

static void masked_poll_fallback(uint32_t function, uint32_t outer_pr) {
    uint32_t token = initial_masked_submit(POLL_SR);
    prepare_sdk_poll_frame(function, outer_pr);
    check_masked_poll_fallback(function, token, poll_frame, 3u);
}

static uint32_t failed_poll_gateway(uint32_t event, uint32_t arg,
    kui_retail_async_region_type *region, uint32_t reserved) {
    if(event == KUI_NATIVE_RA_DEMAND_GD_ENTER) return 1u;
    return kui_native_readahead_aux_dispatch(event, arg, region, reserved);
}

static void rejected_poll_frames_keep_catchup(void) {
    uint32_t token = initial_masked_submit(POLL_SR);
    kui_native_readahead_aux_context.config.toy_poll_owner = 0u;
    prepare_sdk_poll_frame(KUI_GD_CHECK, event900_poll);
    check_masked_poll_fallback(KUI_GD_CHECK, token, poll_frame, 0u);

    const uint32_t invalid_frames[] = {poll_frame + 2u, UINT32_C(0x8c007ffc), UINT32_C(0x8cffff9c)};
    for(unsigned i = 0u; i < sizeof(invalid_frames) / sizeof(invalid_frames[0]); ++i) {
        token = initial_masked_submit(POLL_SR);
        prepare_sdk_poll_frame(KUI_GD_CHECK, event900_poll);
        check_masked_poll_fallback(KUI_GD_CHECK, token, invalid_frames[i], 0u);
    }

    token = initial_masked_submit(POLL_SR);
    prepare_sdk_poll_frame(KUI_GD_EXEC, event900_poll);
    put(poll_frame + 9u * 4u, UINT32_C(0x8c0b1fe8));
    check_masked_poll_fallback(KUI_GD_EXEC, token, poll_frame, 2u);

    token = initial_masked_submit(POLL_SR);
    R.vector600[KUI_NATIVE_RA_ENTRY_INDEX] = (uint32_t)(uintptr_t)failed_poll_gateway;
    prepare_sdk_poll_frame(KUI_GD_CHECK, event900_poll);
    check_masked_poll_fallback(KUI_GD_CHECK, token, poll_frame, 0u);

    setup_movie(385u, false);
    token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    prepare_sdk_poll_frame(KUI_GD_CHECK, event900_poll);
    check_masked_poll_fallback(KUI_GD_CHECK, token, poll_frame, 0u);
}
#endif

#if KUI_TOY_SUITE_MODE == 35
enum { BUMPER_CENTER = 0x69263u, BUMPER_FIRST = BUMPER_CENTER - 40u,
    BUMPER_LAST = BUMPER_CENTER + 40u, BUMPER_FILE_OFFSET = 17u };
enum bumper_seed_kind { BUMPER_SEED_FULL, BUMPER_SEED_FIRST_RUN, BUMPER_SEED_NONE };

static void setup_bumper_window(enum bumper_seed_kind kind) {
    setup_movie_card(1u, 256u);
    const uint32_t delta = BUMPER_FIRST - 29u - manifest.session_lba;
    manifest.session_lba += delta;
    manifest.boot_lba += delta;
    for(unsigned i = 2u; i < manifest.track_count; ++i) {
        manifest.slots[i].track.start_lba += delta;
        manifest.slots[i].track.end_lba += delta;
    }
    struct kui_retail_track *track = &manifest.slots[2].track;
    const uint32_t extent_slot = kui_retail_track_first_extent(track);
    track->first_extent |= BUMPER_FILE_OFFSET << 8u;
    /* Make the seed cross non-adjacent card extents and a nonzero file
     * offset, so the replay must honor the real cursor's mapped LBAs. */
    manifest.slots[extent_slot + 1u].extent.card_lba = 1650u;
    uint32_t track_bytes = (track->end_lba - track->start_lba) * KUI_GAME_RAW_BYTES;
    for(uint32_t i = 0u; i < track->extent_count; ++i) {
        const struct kui_retail_extent *e = &manifest.slots[extent_slot + i].extent;
        for(uint32_t j = 0u; j < e->blocks * 512u; ++j) {
            const uint32_t byte = e->file_block * 512u + j - BUMPER_FILE_OFFSET;
            image_card[e->card_lba * 512u + j] = byte < track_bytes ? source(2u, byte) : 0xf3u;
        }
    }
    manifest.storage_transport = KUI_STORAGE_SCI;
    CHECK(kui_retail_image_init(&reference, &manifest, read_block, NULL) == KUI_GAME_OK);

    struct kui_native_ra_config configuration = movie_configuration();
    /* This fixture maps only the relevant raw track segment. The original
     * title's initial read ends before the fixture's replay window. */
    configuration.movie_ranges[1] = (struct kui_toy_movie_range){BUMPER_FIRST - 386u, 500u};
    memset(arena, 0x93, sizeof(arena));
    if(kind != BUMPER_SEED_NONE) {
        uint32_t block = ((BUMPER_FIRST - track->start_lba) * KUI_GAME_RAW_BYTES +
            BUMPER_FILE_OFFSET) / 512u;
        uint32_t end = ((BUMPER_LAST - track->start_lba) * KUI_GAME_RAW_BYTES +
            BUMPER_FILE_OFFSET + 511u) / 512u;
        uint8_t *out = arena;
        unsigned runs = kind == BUMPER_SEED_FULL ? 2u : 1u;
        for(unsigned i = 0u; i < runs; ++i) {
            const struct kui_retail_extent *e = &manifest.slots[extent_slot + i].extent;
            CHECK(block >= e->file_block && block - e->file_block < e->blocks);
            uint32_t offset = block - e->file_block, count = e->blocks - offset;
            if(count > end - block) count = end - block;
            configuration.suite_seed.base[i] = e->card_lba + offset;
            configuration.suite_seed.count[i] = count;
            CHECK(count * 512u <= (size_t)(arena + sizeof(arena) - out));
            memcpy(out, image_card + (e->card_lba + offset) * 512u, count * 512u);
            out += count * 512u;
            block += count;
        }
        CHECK((block == end) == (kind == BUMPER_SEED_FULL));
    }
    bind_movie_configuration(&configuration);

    /* Warm the SCI port with a normal request, as the real game does before
     * reaching the bumper. Count only card clocks in the replay request. */
    read_and_compare(BUMPER_FIRST - 2u, 1u, 0u, 1u);
    CHECK(R.engine.opened && !kui_sci_stream_busy());
}

static void bumper_replay_copies_without_card(uint32_t lba, uint32_t sectors) {
    unsigned clocks = card.clocks, dma = m.dma_starts, acquires = hw.acquires;
    memset(ram + (OUTPUT - BEGIN), 0xa5, (size_t)sectors * 2048u + 1u);
    uint32_t token = (uint32_t)request(lba, sectors, OUTPUT);
    CHECK(R.shared.service.status == KUI_GD_COMPLETED && !R.shared.service.pending);
    CHECK(gd(KUI_GD_CHECK, token, STATUS) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == sectors * 2048u && !get(STATUS + 4u));
    CHECK(card.clocks == clocks && m.dma_starts == dma && hw.acquires == acquires);
    CHECK(!R.engine.active && !R.engine.hooked && !kui_sci_stream_busy());
    CHECK(ram[OUTPUT - BEGIN - 1u] == 0xa5u);
    compare(lba, sectors, OUTPUT);
}

static void bumper_replay_falls_through(uint32_t lba, uint32_t sectors) {
    unsigned clocks = card.clocks, dma = m.dma_starts;
    const struct kui_toy_window_trace *window = &kui_native_readahead_aux_context.demand.window;
    uint32_t hits = window->requests;
    /* Poison the synthetic seed. A partial replay followed by real reads
     * could otherwise pass a final comparison without proving fallback. */
    memset(arena, 0x93, sizeof(arena));
    memset(ram + (OUTPUT - BEGIN), 0xa5, (size_t)sectors * 2048u + 1u);
    int32_t token = request(lba, sectors, OUTPUT);
    CHECK(R.shared.service.pending && R.shared.service.status == KUI_GD_PROCESSING);
    CHECK(ram[OUTPUT - BEGIN] == 0xa5u);
    CHECK(finish(token, 0u, 1u) == KUI_GD_COMPLETED);
    CHECK(card.clocks > clocks && m.dma_starts > dma);
    CHECK(window->requests == hits);
    CHECK(get(STATUS + 8u) == sectors * 2048u && !get(STATUS + 4u));
    CHECK(ram[OUTPUT - BEGIN - 1u] == 0xa5u);
    compare(lba, sectors, OUTPUT);
}

static void bumper_ram_window(void) {
    setup_bumper_window(BUMPER_SEED_FULL);
    /* Starts before the break at file block 256, ends after it. */
    bumper_replay_copies_without_card(BUMPER_FIRST + 22u, 14u);
    bumper_replay_copies_without_card(BUMPER_CENTER, 14u);
    bumper_replay_copies_without_card(BUMPER_LAST - 1u, 1u);
    const struct kui_toy_window_trace *window = &kui_native_readahead_aux_context.demand.window;
    CHECK(window->seeded_blocks && window->requests == 3u && window->sectors == 29u);
    CHECK(window->first_lba == BUMPER_FIRST + 22u && window->end_lba == BUMPER_LAST);
    CHECK(window->target_ram == 1u && window->target_request_ticks && window->target_gap_ticks);
    CHECK(window->max_request_ticks >= window->target_request_ticks);
    bumper_replay_falls_through(BUMPER_LAST - 2u, 4u);
    bumper_replay_falls_through(BUMPER_FIRST - 1u, 2u);
    bumper_replay_falls_through(BUMPER_CENTER, 25u);

    setup_bumper_window(BUMPER_SEED_FIRST_RUN);
    bumper_replay_falls_through(BUMPER_FIRST + 22u, 14u);
    setup_bumper_window(BUMPER_SEED_NONE);
    bumper_replay_falls_through(BUMPER_CENTER, 14u);

    setup_bumper_window(BUMPER_SEED_FULL);
    kui_native_readahead_aux_context.config.toy_poll_owner = 0u;
    bumper_replay_falls_through(BUMPER_CENTER, 14u);
}

static void bumper_ram_window_with_receive_in_flight(void) {
    setup_bumper_window(BUMPER_SEED_FULL);
    uint32_t stale = (uint32_t)request(BUMPER_LAST + 2u, 1u, OUTPUT + 0x20000u);
    CHECK(kui_sci_stream_busy());
    CHECK(gd(KUI_GD_ABORT, stale, 0u) == 0);
    CHECK(gd(KUI_GD_CHECK, stale, STATUS) == KUI_GD_FAILED);
    CHECK(get(STATUS + 4u) == KUI_GD_ERROR_CANCELLED && kui_sci_stream_busy());

    memset(ram + (OUTPUT - BEGIN), 0xa5, 14u * 2048u + 1u);
    unsigned writes = hw.writes;
    int32_t token = request(BUMPER_CENTER, 14u, OUTPUT);
    CHECK(kui_sci_stream_busy() && R.shared.service.pending && hw.writes == writes);
    CHECK(finish(token, 0u, 1u) == KUI_GD_COMPLETED);
    CHECK(!kui_native_readahead_aux_context.demand.window.requests);
    CHECK(ram[OUTPUT + 0x20000u - BEGIN] == 0xa5u);
    compare(BUMPER_CENTER, 14u, OUTPUT);
}
#endif

#if KUI_TOY_SUITE_MODE == 36
static void movie_dma_priority_trace(void) {
    setup_movie(1u, true);
    const struct kui_toy_demand_trace *trace = &kui_native_readahead_aux_context.demand;
    const struct kui_toy_demand_movie_trace *movie = &trace->movie[1];
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    m.chcr2 = 0x12c1u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_sci_stream_busy() && !movie->overruns);
    for(unsigned i = 0u; i < 4u; ++i) CHECK(!movie->dma_priority[i]);

    /* Model Toy Commander starting a graphics burst under its original
     * priority. The first card block cuts out and must not reach the guest. */
    m.dmaor = 0x8201u;
    m.overrun_after = 200u;
    card.nac_first = 385u;
    uint32_t samples = movie->dma_priority[2];
    independent_sdk_irq();
    CHECK(movie->dma_priority[2] == samples + 1u);
    CHECK(movie->overruns == 1u && sd->overruns == 1u);
    CHECK(m.dmaor == 0x8201u && m.chcr2 == 0x12c1u);
    CHECK(!hw.writes && !kui_sci_stream_busy() && R.shared.service.pending);

    /* A sliced visit waiting for the replacement token has no armed
     * receive and should not count any DMA priority observation. */
    m.dmaor = 0x8301u;
    CHECK(independent_demand_wake() && !kui_sci_stream_busy());
    CHECK(!movie->dma_priority[3] && movie->overruns == 1u);
    for(unsigned n = 0u; !kui_sci_stream_busy(); ++n) {
        CHECK(n < 16u && R.shared.service.pending && !hw.writes);
        (void)independent_demand_wake();
    }

    /* The report follows the register the owner set for each live receive. */
    static const unsigned priorities[] = {0u, 1u, 3u};
    for(unsigned i = 0u; i < sizeof(priorities) / sizeof(priorities[0]); ++i) {
        const unsigned priority = priorities[i];
        m.dmaor = 0x8001u | (priority << 8u);
        samples = movie->dma_priority[priority];
        independent_sdk_irq();
        CHECK(movie->dma_priority[priority] == samples + 1u);
        CHECK(m.dmaor == (0x8001u | (priority << 8u)) && m.chcr2 == 0x12c1u);
    }
    for(unsigned i = 0u; i < 4u; ++i) CHECK(movie->dma_priority[i] == 1u);
    finish_without_polls(token);
    CHECK(!sd->crc_errors && movie->overruns == 1u && sd->overruns == 1u);
    CHECK(!trace->movie[0].overruns);
    for(unsigned i = 0u; i < 4u; ++i) CHECK(!trace->movie[0].dma_priority[i]);

    /* The opening movie read still uses SCI but lies outside the report. */
    struct kui_toy_demand_movie_trace before = *movie;
    read_and_compare(MOVIE_LBA - 1u, 1u, 0u, 1u);
    CHECK(!memcmp(movie->dma_priority, before.dma_priority, sizeof(before.dma_priority)));
    CHECK(movie->overruns == before.overruns);
    puts("Mode 36: scoped DMAOR priority and one cut reported, owner registers and delivered data intact");
}
#endif
#endif

#ifndef KUI_TOY_SDK_LIVENESS_MAIN
#define KUI_TOY_SDK_LIVENESS_MAIN main
#endif
int KUI_TOY_SDK_LIVENESS_MAIN(void) {
#if KUI_TOY_SUITE_DIRECT_PIO
    direct_pio_run(false, true, 0u, event900_poll, 3u);
    direct_pio_run(true, true, 0u, event900_poll, 3u);
    direct_pio_run(false, true, 1172u, event900_poll, 1u);
    direct_pio_run(false, false, 0u, event900_poll, 1u);
    direct_pio_run(false, true, 0u, movie_async_poll, 3u);
    direct_pio_scope(false, MOVIE_LBA);
    direct_pio_scope(true, MOVIE_LBA - 1u);
    direct_pio_scope(true, MOVIE_LBA + 34u);
    direct_pio_crc_retry();
    direct_pio_partial_failure();
    direct_pio_cancel();
    puts("Mode 43: CRC-checked exact movie data without DMA or sequential CMD18 restarts; scope, cadence and fault handling passed");
#elif KUI_TOY_SUITE_MODE == 1
    baseline_idle_after_overrun();
#elif KUI_TOY_SUITE_ASYNC_DEMAND
    recover_overrun_without_polls();
    rearm_changed_sci_priority();
    sliced_token_without_polls();
    sliced_stop_busy_without_polls();
    check_is_status_only();
    abort_stopped_receive();
    masked_sdk_submit_then_independent();
    masked_caller_finishes(0x10u);
    masked_caller_finishes(UINT32_C(0x10000000));
    outside_movie_scope_keeps_catchup();
    fatal_irq_failure_without_polls();
    repeated_damage_fails_without_polls();
    printf("Mode %u: bounded busy, status-only CHECK, masked fallback, cancellation, and independent terminal errors passed\n",
        KUI_TOY_SUITE_MODE);
#if KUI_TOY_SUITE_MOVIE_POLL
    first_cut_retry_route();
#if KUI_TOY_SUITE_MODE == 34
    recovery_still_requires_crc();
#endif
#if KUI_TOY_SUITE_MODE == 34 || KUI_TOY_SUITE_SLOW_RETRY
    cancelled_recovery_does_not_survive_new_token();
    nonmovie_cut_keeps_dma_retry();
#endif
#if KUI_TOY_SUITE_SLOW_RETRY
    slow_retry_keeps_pio_fallback(false);
    slow_retry_keeps_pio_fallback(true);
    nonorer_stop_keeps_fast_retry();
    unverified_movie_keeps_fast_retry();
#if KUI_TOY_SUITE_PIO_WINDOW
    pio_window_crc_failure_restarts_same_block();
    pio_window_cancel_replaces_request(false);
    pio_window_cancel_replaces_request(true);
    pio_window_stops_at_extent_break();
    puts("Mode 40: checked PIO window is sequential, returns to DMA, and clears on cancellation or extent changes");
#endif
#if KUI_TOY_SUITE_PIPELINED_PIO
    failed_payload_never_reaches_guest();
    printf("Mode %u: only window reads use the 514-byte helper, and partial failure publishes no data\n",
        KUI_TOY_SUITE_MODE);
#endif
#if KUI_TOY_SUITE_MOVIE_CHUNK
    movie_chunk_classification();
#endif
    printf("Mode %u: cut/CRC fallback, hardware ORER and exact-owner gating, cancellation and rate restoration passed\n",
        KUI_TOY_SUITE_MODE);
#endif
    masked_movie_poll_with_irqs();
    masked_movie_poll_without_irqs();
    masked_poll_fallback(KUI_GD_CHECK, UINT32_C(0x8c097148));
    masked_poll_fallback(KUI_GD_EXEC, UINT32_C(0x8c097148));
    masked_poll_fallback(KUI_GD_CHECK, movie_blocking_poll);
    masked_poll_fallback(KUI_GD_EXEC, movie_blocking_poll);
    rejected_poll_frames_keep_catchup();
#if KUI_TOY_SUITE_REFILL_SLICE
    refill_slice_poll(event900_poll, true, 0u, 3u);
    refill_slice_poll(event900_poll, true, 1172u, 1u);
    refill_slice_poll(event900_poll, false, 0u, 1u);
    refill_slice_poll(movie_async_poll, true, 0u, 1u);
    refill_slice_keeps_one_pio_block();
    puts("Mode 38: scheduled refill honors time, block and PIO caps; CHECK and one-pass preserved");
#endif
#if KUI_TOY_SUITE_SEQUENTIAL_PIO
    sequential_pio_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, true, true, 3u);
    sequential_pio_poll(movie_async_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, true, true, 3u);
    sequential_pio_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, true, false, 0u);
    sequential_pio_poll(event900_poll, 0u, 0u, true, true, 0u);
    sequential_pio_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dau, true, true, 0u);
    sequential_pio_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL,
        0x07dbu, true, true, 0u);
    sequential_pio_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, false, true, 0u);
    sequential_pio_keeps_wake_baseline();
    puts("Mode 44: retained CMD18 PIO admission requires verified playback, foreground EXEC and a valid clock");
#endif
#if KUI_TOY_SUITE_FIRST_CUT_PIO
    first_cut_pio_keeps_one_cmd18_window();
    first_cut_pio_discards_bad_crc();
    first_cut_pio_cancel_drops_window();
    puts("Mode 46: admitted hardware ORER immediately retries by checked PIO, keeps CMD18 open, and fences CRC and cancellation");
#endif
#if KUI_TOY_SUITE_BOUNDED_DMA
    bounded_dma_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, 20u, false, 6u);
    bounded_dma_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, 4u, false, 4u);
    bounded_dma_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, 20u, true, 1u);
    bounded_dma_poll(movie_async_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu, 20u, false, 0u);
    bounded_dma_poll(event900_poll, 0u, 0u, 20u, false, 0u);
    bounded_dma_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED, 0x07dau, 20u, false, 0u);
    bounded_dma_poll(event900_poll, KUI_TOY_MOVIE_EXIT_STARTED | KUI_TOY_MOVIE_EXIT_TERMINAL,
        0x07dbu, 20u, false, 0u);
    puts("Mode 45: six-block DMA cap, one-PIO fallback and unchanged CHECK and one-pass cadence");
#endif
    printf("Mode %u: masked SDK movie polls deliver exact data with IRQs and with continuous IMASK6; other callers retain catch-up\n",
        KUI_TOY_SUITE_MODE);
#endif
#if KUI_TOY_SUITE_MODE == 35
    bumper_ram_window();
    bumper_ram_window_with_receive_in_flight();
    puts("Mode 35: exact zero-card-clock bumper replay across fragmented raw extents; incomplete or ineligible requests use SCI");
#endif
#if KUI_TOY_SUITE_MODE == 36
    movie_dma_priority_trace();
#endif
#endif
    return 0;
}
