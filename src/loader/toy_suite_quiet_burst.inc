/* SPDX-License-Identifier: GPL-3.0-only */
/* Run before the game continues, through the same transport and CRC parser.
 * Quiet data never reaches the guest or seeds the live cache. */
enum {
    TOY_QUIET_SAMPLED = 1u,
    TOY_QUIET_UNMAPPED = 2u,
    TOY_QUIET_UNAVAILABLE = 3u,
    TOY_QUIET_STOPPED = 4u,
    TOY_QUIET_UNSAFE = 5u,
    TOY_QUIET_BURSTS = 16u,
    TOY_QUIET_OPENING = 386u,
    TOY_QUIET_LAG_WINDOW = 638u,
    TOY_QUIET_TOKEN_POLLS = 32u,
    TOY_QUIET_DMA_POLLS = 16384u,
};

/* Use the same raw-sector first block as an admitted 2048-byte game read.
 * The cold stage already validated the map and the entire movie range. */
static bool burst_quiet_window(const struct kui_retail_manifest *m, uint32_t offset,
    uint32_t *first, uint32_t *end) {
    const struct kui_toy_movie_range *intro = &C.config.movie_ranges[3];
    if(!m || intro->count <= offset) return false;
    uint32_t sector = intro->lba + offset;
    for(unsigned track = 0; track < m->track_count; ++track) {
        const struct kui_retail_track *t = &m->slots[track].track;
        if(sector < t->start_lba || sector >= t->end_lba) continue;
        if(kui_retail_track_sector_bytes(t) != KUI_GAME_RAW_BYTES ||
           kui_retail_track_header_bytes(t) != 16u) return false;
        uint32_t file_block = (kui_retail_track_file_offset(t) +
            (sector - t->start_lba) * KUI_GAME_RAW_BYTES) >> 9;
        uint32_t extent = kui_retail_track_first_extent(t);
        for(unsigned i = 0; i < t->extent_count; ++i) {
            const struct kui_retail_extent *x = &m->slots[extent + i].extent;
            uint32_t delta = file_block - x->file_block;
            if(delta >= x->blocks) continue;
            struct kui_retail_cursor cursor = {
                .manifest = m,
                .track = track,
                .block = x->card_lba + delta,
            };
            uint32_t limit = burst_extent_end(&cursor, 4u);
            if(limit <= cursor.block) return false;
            *first = cursor.block;
            *end = cursor.block + (limit - cursor.block < TOY_QUIET_BURSTS * TOY_BURST_FRAMES ?
                limit - cursor.block : TOY_QUIET_BURSTS * TOY_BURST_FRAMES);
            return true;
        }
        return false;
    }
    return false;
}

static enum kui_sci_stream_result burst_quiet_one(kui_retail_async_region_type *r,
    uint32_t lba, unsigned frames, uint32_t *covered_end) {
    C.burst.lba = lba;
    C.burst.frontier = lba;
    C.burst.frames = frames;
    enum kui_sci_stream_result result = KUI_SCI_STREAM_PENDING;
    for(unsigned n = 0; n < TOY_QUIET_TOKEN_POLLS && result == KUI_SCI_STREAM_PENDING; ++n)
        result = toy_sci_burst_start(r, lba, frames, burst_slab());
    if(result != KUI_SCI_STREAM_OK) {
        burst_record_start_error(result);
        if(result == KUI_SCI_STREAM_PENDING) ++C.burst_quiet.timeout;
        return result;
    }
    burst_record_arm(r->shared.stream.ready_lba[1]);
    *covered_end = lba + frames;
    uint32_t bytes = 0;
    result = KUI_SCI_STREAM_PENDING;
    for(unsigned n = 0; n < TOY_QUIET_DMA_POLLS && result == KUI_SCI_STREAM_PENDING; ++n) {
#ifndef KUI_TOY_BURST_PRODUCER_TEST
        toy_burst_settle(128u);
#endif
        result = toy_sci_burst_poll(r, burst_slab(), frames, &bytes);
    }
    if(result == KUI_SCI_STREAM_OK && bytes <= TOY_BURST_WIRE) {
        burst_record_received(bytes);
        burst_parse(TOY_BURST_FRAMES);
    } else if(result == KUI_SCI_STREAM_PENDING) {
        ++C.burst_quiet.timeout;
    } else {
        ++C.burst_quiet.reset;
        result = KUI_SCI_STREAM_RESET;
    }
    return result;
}

static uint32_t __attribute__((noipa)) burst_quiet_calibrate(kui_retail_async_region_type *r) {
    /* The cold stage invokes this once, even for a skipped sample. No
     * subsequent game call can start calibration via the published entry. */
    if(C.burst_quiet_status) return 0u;
    const struct kui_native_ra_low_ops *o = C.config.ops;
    struct kui_retail_async *e = &r->engine;
    struct kui_sci_stream_state *stream = &r->shared.stream;
    C.burst_quiet_status = TOY_QUIET_UNMAPPED;
    if(C.suite_source != 1u) return 1u;
    uint32_t first[2] = {0u, 0u}, end[2] = {0u, 0u};
    bool early = burst_quiet_window(e->manifest, TOY_QUIET_OPENING, &first[0], &end[0]);
    bool late = burst_quiet_window(e->manifest, TOY_QUIET_LAG_WINDOW, &first[1], &end[1]);
    if(!early && !late) return 1u;
    C.burst_quiet_status = TOY_QUIET_UNAVAILABLE;
    if(!r->shared.service.initialized || r->shared.service.pending ||
       r->shared.service.executing || e->active || e->in_irq || e->hooked ||
       e->release.armed || e->release.depth || e->native_loan_state ||
       stream->state > RA_PAUSED || o->busy() ||
       r->shared.card.transport != KUI_STORAGE_SCI || !r->shared.card.device.sd.ready ||
       r->shared.card.device.sd.slow || r->shared.card.stream.active || !dma_idle())
        return 1u;
    bool opened = e->opened != 0u;
    if(e->opened != (uint32_t)r->shared.port.acquired || r->shared.port.fault) goto unsafe;
    struct kui_sci_stream_state prior_stream = *stream;
    struct kui_sci_sd_port prior_port = r->shared.port;
    uint32_t guard_faults = C.stats.guard_faults, crc_errors = C.stats.crc_errors;
    if(opened) {
        if(o->stop() != KUI_SCI_STREAM_OK) goto unsafe;
    } else if(!o->cold_bus(true)) {
        if(e->opened || r->shared.port.acquired || r->shared.port.fault) goto unsafe;
        return 1u;
    }
    C.burst_quiet_lba = first[0];
    C.burst_quiet_end = first[0];
    C.burst_quiet_late_lba = first[1];
    C.burst_quiet_late_end = first[1];
    C.burst.calibration = true;
    C.burst_quiet_status = TOY_QUIET_SAMPLED;
    for(unsigned band = 0; band < 2u && C.burst_quiet_status == TOY_QUIET_SAMPLED; ++band) {
        uint32_t *covered_end = band ? &C.burst_quiet_late_end : &C.burst_quiet_end;
        for(uint32_t lba = first[band]; lba < end[band];) {
            uint32_t frames = end[band] - lba;
            if(frames > TOY_BURST_FRAMES) frames = TOY_BURST_FRAMES;
            if(!kui_native_ra_cache_rebase(&C.cache, lba, lba + frames)) goto unsafe;
            enum kui_sci_stream_result result = burst_quiet_one(r, lba, frames, covered_end);
            if(toy_sci_burst_stop(r, burst_slab(), frames) != KUI_SCI_STREAM_OK ||
               o->busy() || stream->state || r->shared.port.fault ||
               result == KUI_SCI_STREAM_RESET || C.stats.guard_faults != guard_faults)
                goto unsafe;
            if(result != KUI_SCI_STREAM_OK) {
                C.burst_quiet_status = TOY_QUIET_STOPPED;
                break;
            }
            lba += frames;
        }
    }
    if(!opened && !o->cold_bus(false)) goto unsafe;
    /* CMD12 retired the pre-existing paused command as well. Preserve its
     * statistics and context, but invalidate its now-retired ready data. */
    if(prior_stream.state) {
        prior_stream.state = RA_CLOSED;
        prior_stream.ready[0] = prior_stream.ready[1] = prior_stream.kept = 0u;
        prior_stream.arrived = 0u;
    }
    *stream = prior_stream;
    r->shared.port = prior_port;
    C.stats.crc_errors = crc_errors;
    memset(&C.burst, 0, sizeof(C.burst));
    (void)kui_native_ra_cache_init(&C.cache, C.config.data, KUI_NATIVE_RA_CACHE_BYTES);
    return 1u;
unsafe:
    C.burst_quiet_status = TOY_QUIET_UNSAFE;
    return 0u;
}
