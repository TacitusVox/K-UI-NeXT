/* SPDX-License-Identifier: GPL-3.0-only */
/* A movie request remains owned by the original GD cursor. The
 * separately received wire bytes cannot become guest bytes until checked. */
enum {
    TOY_BURST_WIRE = 8192u,
    TOY_BURST_FRAMES = 15u,
    TOY_BURST_HISTORY = 32u,
    TOY_BURST_LEAD = 312u,
    TOY_BURST_EMPTY_LIMIT = 3u,
    TOY_BURST_MAX_GAP = 64u,
    TOY_BURST_IRQ_CHECKS = 4u,
    TOY_BURST_CALL_CHECKS = 8u,
    TOY_BURST_TIMEOUT_TICKS = 39063u,
    TOY_BURST_MAX_POLLS = 16384u,
#if KUI_TOY_SUITE_MODE == 28
    TOY_BURST_RECOVERY_ATTEMPTS = 2u,
#endif
};
_Static_assert(KUI_NATIVE_RA_CACHE_BYTES + 2u * TOY_BURST_WIRE == KUI_NATIVE_RA_DATA_BYTES,
    "burst staging uses the existing data lease");
_Static_assert(TOY_BURST_LEAD + TOY_BURST_HISTORY == KUI_NATIVE_RA_CACHE_BLOCKS,
    "burst ring lead and history fit");
#define B C.burst
#define METRIC C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE]
static uint8_t *burst_slab(void) { return C.config.data + KUI_NATIVE_RA_CACHE_BYTES; }

#if KUI_TOY_SUITE_MODE == 28
static struct kui_toy_burst_rate *burst_rate(void) {
    return B.slow_rx ? &C.burst_slow : &C.burst_fast;
}
#endif

#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
static struct kui_toy_burst_metrics *burst_metrics(void) {
    return B.calibration ? &C.burst_quiet : &C.burst_live;
}

#define BURST_METRIC_ADD(field, value) do { \
    const uint32_t increment = (value); \
    burst_metrics()->field += increment; \
    if(B.matched) C.burst_match.field += increment; \
} while(0)

static bool burst_in_window(uint32_t first, uint32_t end) {
    return B.lba >= first && B.lba < end && B.frames <= end - B.lba;
}

static void burst_record_arm(uint32_t wanted) {
    B.wanted = wanted;
    /* A comparable live receive must lie entirely in the physical span
     * covered by the quiet requests, including its last programmed frame. */
    B.matched = !B.calibration && (burst_in_window(C.burst_quiet_lba, C.burst_quiet_end) ||
        burst_in_window(C.burst_quiet_late_lba, C.burst_quiet_late_end));
    BURST_METRIC_ADD(arms, 1u);
    BURST_METRIC_ADD(requested, wanted);
#if KUI_TOY_SUITE_MODE == 28
    if(!B.calibration) ++burst_rate()->arms;
#endif
    if(!B.calibration) ++C.suite[0];
}

static void burst_record_start_error(enum kui_sci_stream_result result) {
    /* These attempts never armed a burst and are not paired retirements. */
    struct kui_toy_burst_metrics *m = burst_metrics();
    if(result == KUI_SCI_STREAM_TOKEN) ++m->bad_token;
    else if(result == KUI_SCI_STREAM_COMMAND) ++m->command_error;
    else if(result == KUI_SCI_STREAM_RESET) ++m->reset;
}

static void burst_record_short(struct kui_toy_burst_metrics *m,
    const struct kui_toy_burst_cut *cut) {
    if(!m->first_short.tcr) m->first_short = *cut;
    m->last_short = *cut;
}

static void burst_record_received(uint32_t bytes) {
    B.bytes = bytes;
    B.offset = B.parsed = 0;
    B.pending = true;
    BURST_METRIC_ADD(received, bytes);
    if(!B.calibration) METRIC[3] += bytes;
    if(bytes == B.wanted) {
        BURST_METRIC_ADD(dma_full, 1u);
#if KUI_TOY_SUITE_MODE == 28
        if(!B.calibration) ++burst_rate()->dma_full;
#endif
    } else {
        BURST_METRIC_ADD(dma_short, 1u);
        if(!B.calibration) {
#if KUI_TOY_SUITE_MODE == 28
            /* Retire and parse the stable prefix before the next arm.
             * A short fast receive gets two lower-rate attempts followed
             * by a fast probe. Only a new short fast receive renews them. */
            if(!B.slow_rx) B.slow_attempts = TOY_BURST_RECOVERY_ATTEMPTS;
#endif
            const struct kui_toy_burst_cut cut = {
                .tcr = B.wanted - bytes,
                .chcr2 = read_hw(UINT32_C(0xffa0002c), 4u),
                .c2dst = read_hw(UINT32_C(0xa05f6808), 4u),
                .g2adst = read_hw(UINT32_C(0xa05f7818), 4u),
            };
            burst_record_short(&C.burst_live, &cut);
            if(B.matched) burst_record_short(&C.burst_match, &cut);
        }
    }
}
#endif

static bool burst_release(kui_retail_async_region_type *r) {
    if(B.owned) {
        if(r->engine.in_irq) return false;
        enum kui_sci_stream_result result = toy_sci_burst_stop(r, burst_slab(), B.frames);
        if(result != KUI_SCI_STREAM_OK || C.config.ops->busy()) {
            r->engine.failed = KUI_GD_ERROR_IO;
            return false;
        }
        B.owned = false;
        if(!r->engine.active) C.config.ops->unhook();
    }
    B.pending = false;
    B.variant = 0u;
    return true;
}

/* The cold stage has authenticated the retained map. Limit read-ahead to
 * the current track's file span, physical extent, and mapped movie end. */
static uint32_t burst_extent_end(const struct kui_retail_cursor *c, unsigned variant) {
    const struct kui_retail_manifest *m = c->manifest;
    const struct kui_retail_track *t = &m->slots[c->track].track;
    const struct kui_toy_movie_range *movie = &C.config.movie_ranges[variant - 1u];
    uint32_t movie_end = movie->lba + movie->count;
    if(movie_end > t->end_lba) movie_end = t->end_lba;
    if(movie_end <= t->start_lba) return 0;
    uint32_t file_end = (kui_retail_track_file_offset(t) +
        (movie_end - t->start_lba) * KUI_GAME_RAW_BYTES + 511u) >> 9;
    uint32_t first = kui_retail_track_first_extent(t);
    for(unsigned i = 0; i < t->extent_count; ++i) {
        const struct kui_retail_extent *x = &m->slots[first + i].extent;
        if(c->block - x->card_lba >= x->blocks || file_end <= x->file_block) continue;
        uint32_t blocks = file_end - x->file_block;
        if(blocks > x->blocks) blocks = x->blocks;
        if(c->block - x->card_lba < blocks && blocks <= UINT32_MAX - x->card_lba)
            return x->card_lba + blocks;
    }
    return 0;
}

static bool burst_anchor(kui_retail_async_region_type *r, unsigned variant) {
    const struct kui_retail_cursor *c = &r->engine.cursor;
    uint32_t end = burst_extent_end(c, variant);
    if(!end || !burst_release(r)) return false;
    const struct kui_native_ra_low_ops *o = C.config.ops;
    /* Opening and nonmovie reads used the ordinary transport. It may have
     * left a checked-ready block or an open command; release that before
     * admitting the separate slab receiver. No DMA is drained from IRQ. */
    if(o->busy() || o->stop() != KUI_SCI_STREAM_OK) return false;
    if(!kui_native_ra_cache_rebase(&C.cache, c->block, end)) return false;
    B.variant = variant;
    B.frontier = c->block;
    B.empty = 0;
#if KUI_TOY_SUITE_MODE == 28
    /* A seek or stock fallback in this movie does not erase pending
     * recovery. A different movie starts with its own fast probe. */
    if(B.rate_variant != variant) {
        B.rate_variant = (uint8_t)variant;
        B.slow_attempts = 0u;
    }
    B.slow_tried = false;
#endif
    return true;
}

static void burst_trim(uint32_t block) {
    if(block >= B.frontier) B.frontier = block + 1u;
    uint32_t used = B.frontier - C.cache.base;
    if(used <= TOY_BURST_HISTORY) return;
    uint32_t drop = used - TOY_BURST_HISTORY;
    if(drop > C.cache.count) drop = C.cache.count;
    C.cache.base += drop;
    C.cache.count -= drop;
    C.cache.head += drop;
    if(C.cache.head >= KUI_NATIVE_RA_CACHE_BLOCKS)
        C.cache.head -= KUI_NATIVE_RA_CACHE_BLOCKS;
}

static bool burst_ready(uint32_t block) { return block - C.cache.base < C.cache.count; }

static unsigned burst_feed(kui_retail_async_region_type *r, unsigned limit) {
    struct kui_retail_async *e = &r->engine;
    unsigned fed = 0;
    while(fed < limit && e->active && !e->failed && burst_ready(e->cursor.block)) {
        uint32_t block = e->cursor.block, slot = C.cache.head + block - C.cache.base;
        if(slot >= KUI_NATIVE_RA_CACHE_BLOCKS) slot -= KUI_NATIVE_RA_CACHE_BLOCKS;
        if(C.config.ops->feed(&e->cursor, C.cache.data + (slot << 9)) != KUI_GAME_OK) {
            e->failed = KUI_GD_ERROR_IO;
            break;
        }
        e->retries = 0;
        ++e->since;
        ++fed;
        if(e->in_irq) ++e->stats.irq_blocks;
        else ++e->stats.call_blocks;
        burst_trim(block);
        demand_complete(r);
    }
    return fed;
}

static uint8_t burst_reverse(uint8_t x) {
    uint32_t y = x;
    y = ((y & 0x55u) << 1) | ((y >> 1) & 0x55u);
    y = ((y & 0x33u) << 2) | ((y >> 2) & 0x33u);
    return (uint8_t)((y << 4) | (y >> 4));
}

static bool burst_check_block(const uint8_t *wire, uint8_t *out) {
    uint16_t crc = 0;
    for(unsigned i = 0; i < 512u; ++i) {
        uint8_t b = burst_reverse(wire[i]);
        out[i] = b;
        uint32_t x = (crc >> 8) ^ b;
        x ^= x >> 4;
        crc = (uint16_t)((crc << 8) ^ (x << 12) ^ (x << 5) ^ x);
    }
    return crc == ((unsigned)burst_reverse(wire[512]) << 8 | burst_reverse(wire[513]));
}

static void burst_parse_done(void) {
    if(B.parsed < B.frames) {
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        if(B.bytes == B.wanted) BURST_METRIC_ADD(parse_full_cut, 1u);
        if(!B.calibration)
#endif
            ++C.suite[2];
    }
    if(B.parsed) {
        B.empty = 0;
#if KUI_TOY_SUITE_MODE == 28
        B.slow_tried = false;
#endif
    } else {
        ++B.empty;
#if KUI_TOY_SUITE_MODE == 28
        /* If the third empty was the first hardware cut since any good
         * data, give the first slower attempt a chance before stock. */
        if(!B.calibration && !B.slow_rx && B.bytes < B.wanted &&
           !B.slow_tried && B.empty == TOY_BURST_EMPTY_LIMIT) --B.empty;
#endif
    }
    B.pending = false;
}

/* Bound retirement work even in the SCI interrupt. Only whole contiguous
 * checked frames become visible. Token search cannot resync into payload. */
static void burst_parse(unsigned budget) {
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    uint32_t before = B.parsed;
#endif
    while(B.pending && budget--) {
        const uint8_t *slab = burst_slab();
        uint32_t at = B.offset;
        if(B.parsed) {
            unsigned gap = 0;
            while(at < B.bytes && gap < TOY_BURST_MAX_GAP && slab[at] == 0xffu) {
                ++at;
                ++gap;
            }
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
            bool token_available = at < B.bytes;
#endif
            if(at >= B.bytes || slab[at++] != 0x7fu) {
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
                /* Running out of bytes and seeing an invalid token are
                 * distinct. An overlong gap is an invalid token here. */
                if(token_available) BURST_METRIC_ADD(bad_token, 1u);
#endif
                burst_parse_done();
                break;
            }
        }
        if(B.bytes - at < 514u) {
            burst_parse_done();
            break;
        }
        uint32_t lba;
        if(!kui_native_ra_cache_next(&C.cache, &lba) || lba != B.lba + B.parsed) {
            ++C.stats.guard_faults;
            burst_parse_done();
            break;
        }
        uint32_t slot = C.cache.head + C.cache.count;
        if(slot >= KUI_NATIVE_RA_CACHE_BLOCKS) slot -= KUI_NATIVE_RA_CACHE_BLOCKS;
        /* Decode and check into the next unowned cache slot. Its bytes
         * remain unreachable on bad CRC because count has not advanced. */
        if(!burst_check_block(slab + at, C.cache.data + (slot << 9))) {
            ++C.stats.crc_errors;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
            BURST_METRIC_ADD(bad_crc, 1u);
#endif
            burst_parse_done();
            break;
        }
        ++C.cache.count;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        if(!B.calibration)
#endif
        {
            ++C.suite[1];
            uint32_t lead = C.cache.base + C.cache.count - B.frontier;
            if(lead > METRIC[2]) METRIC[2] = lead;
        }
        B.offset = at + 514u;
        if(++B.parsed == B.frames) burst_parse_done();
    }
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    if(B.parsed > before) {
        BURST_METRIC_ADD(checked, B.parsed - before);
#if KUI_TOY_SUITE_MODE == 28
        if(!B.calibration) burst_rate()->checked += B.parsed - before;
#endif
        if(!before) BURST_METRIC_ADD(usable, 1u);
    }
#endif
}

static void burst_retire(kui_retail_async_region_type *r) {
    if(r->shared.stream.state == KUI_TOY_SCI_BURST_DMA) {
        uint32_t bytes = 0;
        enum kui_sci_stream_result result = toy_sci_burst_poll(r, burst_slab(), B.frames, &bytes);
        if(result == KUI_SCI_STREAM_PENDING) {
            if(r->engine.in_irq) return;
            struct ra_clock now = clock_read();
            uint32_t elapsed = B.started - now.counter;
            if(++B.polls < TOY_BURST_MAX_POLLS && (!now.valid ||
               elapsed < TOY_BURST_TIMEOUT_TICKS || elapsed >= UINT32_C(0x80000000))) return;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
            BURST_METRIC_ADD(timeout, 1u);
#endif
            ++C.suite[2];
            if(burst_release(r)) B.empty = TOY_BURST_EMPTY_LIMIT;
            return;
        }
        if(result != KUI_SCI_STREAM_OK || bytes > TOY_BURST_WIRE
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
           || bytes > B.wanted
#endif
          ) {
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
            BURST_METRIC_ADD(reset, 1u);
#endif
            ++C.suite[2];
            r->engine.failed = KUI_GD_ERROR_IO;
            return;
        }
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        burst_record_received(bytes);
#else
        B.bytes = bytes;
        B.offset = B.parsed = 0;
        B.pending = true;
        METRIC[3] += bytes;
#endif
    }
    if(B.pending) burst_parse(r->engine.in_irq ? TOY_BURST_IRQ_CHECKS : TOY_BURST_CALL_CHECKS);
}

static void burst_arm(kui_retail_async_region_type *r) {
    struct kui_retail_async *e = &r->engine;
    if(!B.variant || B.pending || e->failed || e->in_irq || C.quiesced ||
       C.config.ops->busy() || !dma_idle()) return;
    uint32_t lba;
    if(!kui_native_ra_cache_next(&C.cache, &lba)) return;
    uint32_t lead = lba - B.frontier;
    if(lead >= TOY_BURST_LEAD) return;
    unsigned frames = TOY_BURST_LEAD - lead;
    if(frames > C.cache.end - lba) frames = C.cache.end - lba;
    if(frames > KUI_NATIVE_RA_CACHE_BLOCKS - C.cache.count)
        frames = KUI_NATIVE_RA_CACHE_BLOCKS - C.cache.count;
    if(frames > TOY_BURST_FRAMES) frames = TOY_BURST_FRAMES;
    if(r->shared.stream.state == KUI_TOY_SCI_BURST_TOKEN) {
        if(!B.owned || B.lba != lba) {e->failed=KUI_GD_ERROR_IO;return;}
        frames = B.frames;
    }
    C.config.ops->hook();
    if(!e->hooked || e->release.armed != 0x10u ||
       (read_hw(UINT32_C(0xffd00008), 2u) & 0xf0u) != 0x10u) {
        ++B.empty;
        return;
    }
#if KUI_TOY_SUITE_MODE == 28
    bool slow = B.slow_attempts != 0u;
    enum kui_sci_stream_result result = toy_sci_burst_start_rate(r, lba, frames, burst_slab(), slow);
    /* A sliced token search keeps the selected rate until it finishes. */
    if(slow && result != KUI_SCI_STREAM_BUSY && result != KUI_SCI_STREAM_PENDING) {
        --B.slow_attempts;
        B.slow_tried = true;
    }
#else
    enum kui_sci_stream_result result = toy_sci_burst_start(r, lba, frames, burst_slab());
#endif
    uint32_t state = r->shared.stream.state;
    if(state == KUI_TOY_SCI_BURST_DMA || state == KUI_TOY_SCI_BURST_TOKEN || state == RA_LOST) {
        B.lba = lba;
        B.frames = frames;
        B.owned = true;
    }
    if(result == KUI_SCI_STREAM_OK) {
#if KUI_TOY_SUITE_MODE == 28
        B.slow_rx = slow;
#endif
        B.started = read_hw(UINT32_C(0xffd8000c), 4u);
        B.polls = 0;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        burst_record_arm(r->shared.stream.ready_lba[1]);
#else
        ++C.suite[0];
#endif
    } else if(result != KUI_SCI_STREAM_BUSY && result != KUI_SCI_STREAM_PENDING) {
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        burst_record_start_error(result);
#endif
        ++B.empty;
        if(result == KUI_SCI_STREAM_RESET) e->failed = KUI_GD_ERROR_IO;
    }
}

static void burst_request_stats(const struct kui_retail_cursor *c, unsigned variant) {
    ++METRIC[0];
    if(B.variant != variant || !burst_ready(c->block)) return;
    ++METRIC[1];
    /* A cursor run ending inside the movie extent covers the whole
     * request. Exclude runs at the boundary rather than count a partial
     * request that continues in a different physical extent. */
    if(c->run < C.cache.end - c->block &&
       C.cache.count - (c->block - C.cache.base) >= c->run)
        ++C.suite[4];
}

static void burst_service(uint32_t wait, kui_retail_async_region_type *r) {
    struct kui_retail_async *e = &r->engine;
    unsigned variant = suite_movie(r);
    if(variant && r->shared.service.lba - C.config.movie_ranges[variant - 1u].lba < 386u)
        variant = 0;
    if(!variant) {
        /* An SCI callback can finish the cursor before the next GD call
         * publishes status. Pending with the same token remains our film. */
        if(B.variant && (e->active || e->failed ||
           (r->shared.service.pending && r->shared.service.token != e->token) ||
           r->shared.service.status == KUI_GD_NOT_FOUND ||
           r->shared.service.command == KUI_RETAIL_GD_PAUSE)) {
            if(e->in_irq || C.current_event == KUI_NATIVE_RA_TICK) {
                burst_retire(r);
                return;
            }
            if(!burst_release(r)) return;
        }
        if(B.variant) {
            burst_retire(r);
            if(B.empty < TOY_BURST_EMPTY_LIMIT) burst_arm(r);
        } else if(C.current_event == KUI_NATIVE_RA_DELIVER) suite_demand(wait, r);
        return;
    }
    C.quiesced = 0;
    if(!B.have_token || B.token != e->token) {
        B.have_token = true;
        B.token = e->token;
        B.fallback = false;
        burst_request_stats(&e->cursor, variant);
    }
    if(B.fallback && B.fallback_token == e->token) {
        if(C.current_event == KUI_NATIVE_RA_DELIVER) suite_demand(wait ? 1u : 0u, r);
        return;
    }
    uint32_t block = e->cursor.block;
    uint32_t admitted = C.cache.count;
    if(B.pending || r->shared.stream.state == KUI_TOY_SCI_BURST_DMA ||
       r->shared.stream.state == KUI_TOY_SCI_BURST_TOKEN) admitted += B.frames;
    if(B.variant != variant || block < C.cache.base || block >= C.cache.end ||
       block - C.cache.base > admitted) {
        if(e->in_irq) {
            if(B.owned) burst_retire(r);
            else suite_demand(wait, r);
            return;
        }
        if(!burst_anchor(r, variant)) {
            if(!e->failed && !B.owned && C.current_event == KUI_NATIVE_RA_DELIVER)
                suite_demand(wait ? 1u : 0u, r);
            return;
        }
    }
    unsigned limit = e->in_irq ? KUI_RETAIL_NATIVE_IRQ_BATCH_LIMIT : e->target;
    if(limit > 25u) limit = 25u;
    /* Serve already-checked demand first, independently of any in-flight
     * speculative transfer. No movie visit polls in a wait loop. */
    unsigned fed = burst_feed(r, limit);
    burst_retire(r);
    if(fed < limit) fed += burst_feed(r, limit - fed);
    if(e->failed) return;
    if(!e->in_irq && e->active && !fed && !B.pending &&
       (B.empty >= TOY_BURST_EMPTY_LIMIT ||
        (!C.config.ops->busy() && !dma_idle()))) {
        if(!burst_release(r)) return;
        B.fallback = true;
        B.fallback_token = e->token;
        ++C.suite[3];
        if(C.current_event == KUI_NATIVE_RA_DELIVER) suite_demand(wait ? 1u : 0u, r);
        return;
    }
    if(B.empty < TOY_BURST_EMPTY_LIMIT) burst_arm(r);
}

#undef METRIC
#undef B
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
#undef BURST_METRIC_ADD
#endif
