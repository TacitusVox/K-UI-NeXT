/* SPDX-License-Identifier: GPL-3.0-only */
/* The real GD service, asynchronous DMA stream and raw-sector verifier
 * against the shared card and register model. */
#include "kui/hash.h"

#define KUI_TOY_SDK_LIVENESS_MAIN toy_retained_sdk_tests
#include "test_toy_sdk_liveness.c"

#if !KUI_TOY_SUITE_VERIFIED_RESUME
#error "Toy resume integration fixture requires the verified-resume mode"
#endif

static uint8_t bcd_field(uint32_t n) { return (uint8_t)((n / 10u) * 16u + n % 10u); }

static void install_raw_sector(uint32_t lba) {
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
    CHECK(kui_sector_edc_valid_at(raw, fad));
    for(unsigned i = 0u; i < sizeof(raw); ++i) {
        const uint32_t file_block = (first + i) / 512u;
        const struct kui_retail_extent *extent = &manifest.slots[track->first_extent].extent;
        CHECK(file_block >= extent->file_block && file_block - extent->file_block < extent->blocks);
        const uint32_t lba_on_card = extent->card_lba + file_block - extent->file_block;
        image_card[lba_on_card * 512u + (first + i) % 512u] = raw[i];
    }
}

static void verify_no_guest_data(void) {
    CHECK(!hw.writes && !R.engine.cursor.done && !R.shared.service.completed_bytes);
    for(unsigned i = 0u; i < MOVIE_SECTORS * KUI_GAME_DATA_BYTES; ++i)
        CHECK(ram[OUTPUT - BEGIN + i] == 0xa5u);
}

static uint32_t start_verified_movie(bool guest_dma) {
    setup_movie_card(1u, 128u);
    for(uint32_t i = 0u; i < MOVIE_SECTORS; ++i) install_raw_sector(MOVIE_LBA + i);
    struct kui_native_ra_config config = movie_configuration();
    config.movie_ranges[3] = (struct kui_toy_movie_range){MOVIE_LBA - 386u, 420u};
    bind_movie_configuration(&config);
    fixture_movie_started(KUI_TOY_MOVIE_EXIT_STARTED, 0x07dbu);
    if(guest_dma) m.chcr = 1u;
    uint32_t token = (uint32_t)request(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_native_readahead_aux_context.resume.token == token);
    CHECK(kui_sci_stream_busy() != guest_dma && m.dma_starts == (unsigned)!guest_dma);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    CHECK(!R.engine.cursor.raw);
    verify_no_guest_data();
    return token;
}

static void run_verified_resume(bool corrupt) {
    uint32_t token = start_verified_movie(false);

    m.overrun_after = 250u;
    independent_sdk_irq();
    CHECK(!kui_sci_stream_busy() && m.cs_high && m.dma_starts == 1u);
    CHECK(card.cmd18 == 1u && !card.cmd12);
#if KUI_TOY_SUITE_RESUME_WAKE
    const unsigned parked_clocks = card.clocks;
    kui_retail_hook_active = 1u;
    CHECK(!independent_demand_wake());
    kui_retail_hook_active = 0u;
    R.shared.service.token = token + 1u;
    CHECK(!independent_demand_wake());
    R.shared.service.token = token;

    /* DMA1 can change ownership while the card is parked. */
    m.chcr = 1u;
    (void)independent_demand_wake();
    CHECK(m.chcr == 1u && m.cs_high && card.clocks == parked_clocks);
    CHECK(!kui_sci_stream_busy() && m.dma_starts == 1u);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    verify_no_guest_data();
    m.chcr = 0u;

    /* A foreground-only polled retry also leaves the parked command alone. */
    R.engine.retries = 2u;
    (void)independent_demand_wake();
    CHECK(m.cs_high && card.clocks == parked_clocks);
    CHECK(!kui_sci_stream_busy() && m.dma_starts == 1u);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    verify_no_guest_data();
    R.engine.retries = 0u;

    const unsigned call_blocks = R.engine.stats.call_blocks;
    const unsigned irq_blocks = R.engine.stats.irq_blocks;
    CHECK(independent_demand_wake());
    CHECK(R.engine.stats.call_blocks == call_blocks && R.engine.stats.irq_blocks == irq_blocks);
    CHECK(kui_native_readahead_aux_context.demand.movie[1].wake_resumes == 1u);
    verify_no_guest_data();
#else
    CHECK(independent_demand_wake());
    CHECK(!kui_sci_stream_busy() && m.dma_starts == 1u);
    verify_no_guest_data();

#if KUI_TOY_SUITE_RESUME_SLICE
    /* Observe the exact suffix arm before a later visit can consume it. */
    refill_clock_valid = false;
#endif
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
#if KUI_TOY_SUITE_RESUME_SLICE
    refill_clock_valid = true;
#endif
#endif
    CHECK(kui_sci_stream_busy() && m.dma_starts == 2u && (m.dar & 0xfffu) == 252u);
    CHECK(card.cmd18 == 1u && !card.cmd12);
    if(corrupt) {
        /* This error pattern has zero SD CRC16 residue. Apply it only to
         * this DMA copy; the card's uncorrupted original remains readable. */
        const uint8_t delta[3] = {0x01u, 0x10u, 0x21u};
        CHECK(crc16(delta, sizeof(delta)) == 0u);
        elapse();
        CHECK(kui_sci_stream_test_read(SSR, 1u) & ORER);
        uint8_t *area = R.shared.stream.area[R.shared.stream.fill];
        for(unsigned i = 0u; i < sizeof(delta); ++i) area[400u + i] ^= rev8(delta[i]);
    }

    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 512u && !R.engine.failed);
        if(!kui_native_readahead_aux_context.suite[3]) verify_no_guest_data();
        if(kui_sci_stream_busy()) independent_sdk_irq();
        else CHECK(sdk_poll(KUI_GD_EXEC, 0u, movie_async_poll) == 0);
        CHECK(!R.engine.cursor.raw);
        if(!corrupt) CHECK(card.cmd18 == 1u && !card.cmd12);
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, movie_async_poll) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(kui_sci_stream_stats()->toy_resumed_blocks == 1u);
    if(corrupt) {
        CHECK(!kui_native_readahead_aux_context.suite[1]);
        CHECK(kui_native_readahead_aux_context.suite[3] == 1u);
        CHECK(card.cmd18 == 2u && card.cmd12 == 1u);
    } else {
        CHECK(kui_native_readahead_aux_context.suite[1] == 1u);
        CHECK(!kui_native_readahead_aux_context.suite[3]);
    }
}

#if KUI_TOY_SUITE_RESUME_SLICE
static void run_resume_slice(uint32_t caller, bool valid_clock, uint32_t extra_ticks,
    bool guest_dma, unsigned expected_blocks) {
    uint32_t token = start_verified_movie(guest_dma);
    const struct kui_toy_demand_movie_trace *movie = &kui_native_readahead_aux_context.demand.movie[1];
    const struct kui_sci_stream_stats *sd = kui_sci_stream_stats();
    CHECK(!movie->slice_visits && !movie->slice_blocks);
    m.delay = 0u;
    elapse();
    refill_clock_valid = valid_clock;
    refill_clock_extra_ticks = extra_ticks;
    status_only_poll(token, caller);
    verify_no_guest_data();
    const unsigned blocks = R.engine.stats.call_blocks;
    const unsigned polled = sd->polled;
    CHECK(sdk_poll(KUI_GD_EXEC, 0u, caller) == 0);
    CHECK(R.engine.stats.call_blocks - blocks == expected_blocks);
    CHECK(movie->slice_blocks == expected_blocks - 1u);
    CHECK(sd->polled - polled == (unsigned)guest_dma);
    if(guest_dma) CHECK(m.chcr == 1u && !m.dma_starts);
    CHECK(!R.engine.cursor.raw && card.cmd18 == 1u && !card.cmd12);
    verify_no_guest_data();
    status_only_poll(token, caller);
    verify_no_guest_data();
    refill_clock_valid = true;
    refill_clock_extra_ticks = 0u;
    if(guest_dma) m.chcr = 0u;
    m.delay = RECEIVE_POLLS;

    /* IRQ work remains a single checked block per dispatch. */
    for(unsigned n = 0u; R.shared.service.pending; ++n) {
        CHECK(n < 512u && !R.engine.failed);
        verify_no_guest_data();
        if(kui_sci_stream_busy()) {
            const unsigned before = R.engine.stats.irq_blocks;
            independent_sdk_irq();
            CHECK(R.engine.stats.irq_blocks - before <= 1u);
        } else CHECK(sdk_poll(KUI_GD_EXEC, 0u, caller) == 0);
        CHECK(!R.engine.cursor.raw && card.cmd18 == 1u && !card.cmd12);
    }
    CHECK(sdk_poll(KUI_GD_CHECK, token, caller) == KUI_GD_COMPLETED);
    CHECK(get(STATUS + 8u) == MOVIE_SECTORS * KUI_GAME_DATA_BYTES && !get(STATUS + 4u));
    compare(MOVIE_LBA, MOVIE_SECTORS, OUTPUT);
    CHECK(!sd->toy_resumed_blocks && !kui_native_readahead_aux_context.suite[3]);
}
#endif

int main(void) {
    movie_chunk_classification();
    run_verified_resume(false);
    run_verified_resume(true);
#if KUI_TOY_SUITE_RESUME_SLICE
    run_resume_slice(event900_poll, true, 0u, false, 3u);
    run_resume_slice(movie_async_poll, true, 0u, false, 3u);
    run_resume_slice(movie_async_poll, false, 0u, false, 1u);
    run_resume_slice(event900_poll, true, 1172u, false, 1u);
    run_resume_slice(event900_poll, true, 0u, true, 1u);
    printf("Mode %u: staged SDK refill caps, status-only checks, and IRQ/PIO bounds passed\n",
        KUI_TOY_SUITE_MODE);
#endif
    printf("Mode %u: real GD/SCI same-CMD18 resume, EDC quarantine, and clean strict fallback passed\n",
        KUI_TOY_SUITE_MODE);
    return 0;
}
