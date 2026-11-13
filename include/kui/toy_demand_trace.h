/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_DEMAND_TRACE_H
#define KUI_TOY_DEMAND_TRACE_H
#include <stdint.h>

struct kui_toy_demand_timing {
    uint32_t count,ticks,max;
};

struct kui_toy_demand_movie_trace {
    struct kui_toy_demand_timing exec,check,no_arm;
    uint32_t wakes,wake_arms;
#if KUI_TOY_SUITE_MOVIE_POLL
    uint32_t polls[4]; /* Other, event 0x900, explicit movie async, blocking. */
#endif
#if KUI_TOY_SUITE_MODE == 34
    uint32_t overruns, starts, polled;
#elif KUI_TOY_SUITE_MODE == 36
    uint32_t dma_priority[4], overruns; /* DMAOR.PR samples while armed, then scoped cuts. */
#elif KUI_TOY_SUITE_SLOW_RETRY
#if KUI_TOY_SUITE_PIO_WINDOW
    uint32_t overruns, window_pio, window_ok;
#else
    uint32_t overruns, retry_orer, retry_slow, retry_ok;
#endif
#if KUI_TOY_SUITE_REFILL_SLICE
    uint32_t slice_visits, slice_blocks;
#endif
#elif KUI_TOY_SUITE_VERIFIED_RESUME
    uint32_t slice_visits, slice_blocks;
#if KUI_TOY_SUITE_RESUME_WAKE
    uint32_t wake_resumes;
#endif
#endif
};

#if KUI_TOY_SUITE_MODE == 35
struct kui_toy_window_trace {
    uint32_t requests, sectors, first_lba, end_lba, max_request_ticks;
    uint32_t seeded_blocks, target_ram, target_request_ticks, target_gap_ticks;
};
#endif

/* This state is diagnostic only. Playback never consults it for scheduling.
 * A zero variant invalidates a timestamp; a running TMU may read zero. */
struct kui_toy_demand_trace {
    struct kui_toy_demand_movie_trace movie[2];
#if KUI_TOY_SUITE_MOVIE_POLL
    uint32_t owner_verified;
#endif
#if KUI_TOY_SUITE_MODE == 35
    struct kui_toy_window_trace window;
#endif
    uint32_t gd_variant,gd_function,gd_tick;
    uint32_t idle_variant,idle_token,idle_tick;
};

#endif
