/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_READAHEAD_AUX_INTERNAL_H
#define KUI_NATIVE_READAHEAD_AUX_INTERNAL_H
#include "kui/native_readahead.h"
#include "kui/native_readahead_cache.h"
#include <stddef.h>

/* The SH entry owns busy. The public report prefix is deliberately small
 * and stable; all policy state is private to this separately owned module. */
struct kui_native_readahead_aux_context {
    uint32_t busy;
    struct kui_native_ra_stats stats;
    struct kui_native_ra_diagnostics diagnostics;
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    struct kui_toy_movie_stats movies[2];
    uint32_t movie_mask;
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    struct kui_toy_playback_stats playback[2];
#endif
#if defined(KUI_TOY_SUITE_MODE)
    uint32_t suite[5], suite_source;
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    uint32_t burst_quiet_status,burst_quiet_lba,burst_quiet_end;
    uint32_t burst_quiet_late_lba,burst_quiet_late_end;
    struct kui_toy_burst_metrics burst_quiet,burst_live,burst_match;
#endif
#if KUI_TOY_SUITE_MODE == 28
    struct kui_toy_burst_rate burst_fast,burst_slow;
#endif
#if KUI_TOY_FMV_COMPARE
    struct kui_toy_fmv_trace fmv_trace;
#endif
    struct kui_native_ra_config config;
    struct kui_native_ra_cache cache;
    uint32_t initialized,quiesced,abandoned,have_seed;
    uint32_t completed_token,have_completed,hit_token,have_hit;
    uint32_t spec_owned,spec_lba,deferred;
    /* Serialized entry owns this staging record. Keeping it here avoids
     * growing the private stack for every transport operation. */
    struct kui_native_ra_failure_snapshot observation;
    uint32_t current_event;
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    struct {
        uint32_t variant, frontier, lba, frames, bytes, offset, parsed;
        uint32_t token, fallback_token, empty, started, polls;
        bool owned, pending, have_token, fallback;
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        uint32_t wanted;
        bool calibration, matched;
#endif
#if KUI_TOY_SUITE_MODE == 28
        uint8_t rate_variant, slow_attempts;
        bool slow_rx, slow_tried;
#endif
    } burst;
#endif
#if KUI_NATIVE_TOY_MOVIE_CAPTURE && !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
    uint32_t last_movie,last_movie_tick;
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    uint32_t playback_started[2], playback_valid;
    uint32_t playback_token, playback_variant;
    uint32_t last_play_variant, last_play_tick;
#endif
#if KUI_NATIVE_TOY_HEADER_REPLAY
    struct {
        const struct kui_retail_manifest *manifest;
        kui_retail_cursor_write write;
        void *context;
        uint32_t token,lba,last_block,stream_position;
        uint32_t sector_part,track_type;
#if KUI_NATIVE_SESSION_CONTROLS
        uint32_t disc_generation;
#endif
        uint16_t written,first_chunk;
        bool valid;
        _Alignas(4) uint8_t data[KUI_GAME_DATA_BYTES];
    } header;
#endif
};
_Static_assert(offsetof(struct kui_native_readahead_aux_context,busy)==0u,
    "auxiliary assembly busy word");
_Static_assert(offsetof(struct kui_native_readahead_aux_context,stats)==4u,
    "auxiliary public statistics prefix");
_Static_assert(offsetof(struct kui_native_readahead_aux_context,diagnostics)==76u,
    "auxiliary appended diagnostic prefix");
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
_Static_assert(offsetof(struct kui_native_readahead_aux_context,playback)==
    offsetof(struct kui_native_ra_public,playback),"playback report prefix");
#endif
#if defined(KUI_TOY_SUITE_MODE)
_Static_assert(offsetof(struct kui_native_readahead_aux_context,suite)==
    offsetof(struct kui_native_ra_public,suite),"Toy suite report prefix");
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
_Static_assert(offsetof(struct kui_native_readahead_aux_context,burst_match)==
    offsetof(struct kui_native_ra_public,burst_match),"Toy burst measurement prefix");
#endif
#if KUI_TOY_SUITE_MODE == 28
_Static_assert(offsetof(struct kui_native_readahead_aux_context,burst_slow)==
    offsetof(struct kui_native_ra_public,burst_slow),"Toy burst rate prefix");
#endif
#if KUI_TOY_FMV_COMPARE
_Static_assert(offsetof(struct kui_native_readahead_aux_context,fmv_trace)==
    offsetof(struct kui_native_ra_public,fmv_trace),"Toy FMV compare report prefix");
#endif
_Static_assert(offsetof(struct kui_native_readahead_aux_context,stats)+
    offsetof(struct kui_native_ra_stats,guard_faults)==64u,
    "auxiliary assembly guard fault counter");
_Static_assert(sizeof(struct kui_native_readahead_aux_context)<=4096u-64u,
    "auxiliary context fits its owned first page with export/guard margin");
extern struct kui_native_readahead_aux_context kui_native_readahead_aux_context;
int kui_native_readahead_aux_init(const struct kui_native_ra_config *);
uint32_t kui_native_readahead_aux_dispatch(uint32_t,uint32_t,
    kui_retail_async_region_type *,uint32_t);
#endif
