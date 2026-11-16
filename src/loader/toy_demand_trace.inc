/* SPDX-License-Identifier: GPL-3.0-only */
/* Async demand uses the ordinary SCI state and scopes observation to post-header
 * movie requests. This observes the existing delivery path without touching
 * receive registers, acknowledging completion or granting transport work. */
static void toy_demand_timing_add(struct kui_toy_demand_timing *timing,uint32_t elapsed) {
    if(timing->count!=UINT32_MAX)++timing->count;
    if(elapsed>UINT32_MAX-timing->ticks)timing->ticks=UINT32_MAX;
    else timing->ticks+=elapsed;
    if(elapsed>timing->max)timing->max=elapsed;
}

static uint32_t toy_demand_gd_trace(uint32_t event,uint32_t function,
    const kui_retail_async_region_type *r
#if KUI_TOY_SUITE_MOVIE_POLL
    ,uint32_t poll
#endif
    ) {
    struct kui_toy_demand_trace *trace=&C.demand;
    if(function!=KUI_GD_EXEC && function!=KUI_GD_CHECK)return 0u;
    if(event==KUI_NATIVE_RA_DEMAND_GD_ENTER) {
        /* A completed request can still own its command before CHECK.
         * Attribute the entire dispatch by its entry command, even if
         * CHECK releases it before the leave observation. */
        const unsigned variant=playback_variant(&r->shared.service);
        trace->gd_variant=0u;
        if(!variant)return 1u;
#if KUI_TOY_SUITE_MOVIE_POLL
        uint32_t *polls=trace->movie[(variant-1u)/2u].polls;
        const unsigned kind=poll<4u?poll:0u;
        if(polls[kind]!=UINT32_MAX)++polls[kind];
#endif
        const struct ra_clock now=clock_read();
        if(!now.valid) {
            ++C.playback[(variant-1u)/2u].bad_clock;
            return 1u;
        }
        trace->gd_variant=variant;
        trace->gd_function=function;
        trace->gd_tick=now.counter;
        return 1u;
    }
    const unsigned variant=trace->gd_variant;
    trace->gd_variant=0u;
    if(!variant || trace->gd_function!=function)return 1u;
    const struct ra_clock now=clock_read();
    const uint32_t elapsed=trace->gd_tick-now.counter;
    const unsigned movie=(variant-1u)/2u;
    if(!now.valid || elapsed>=UINT32_C(0x80000000)) {
        ++C.playback[movie].bad_clock;
        return 1u;
    }
    struct kui_toy_demand_movie_trace *m=&trace->movie[movie];
    toy_demand_timing_add(function==KUI_GD_EXEC?&m->exec:&m->check,elapsed);
    return 1u;
}

/* Count only closed, same-token intervals with no receive armed at both
 * delivery boundaries. This is a transport liveness metric, not a claim
 * that the game's decoder or a separate verified-data queue is empty. */
static bool toy_demand_before(unsigned variant,const kui_retail_async_region_type *r,
    struct ra_clock now) {
    struct kui_toy_demand_trace *trace=&C.demand;
    const bool armed=r->shared.stream.state==RA_DMA;
    if(variant && variant==trace->idle_variant && !armed &&
       r->engine.token==trace->idle_token) {
        const uint32_t elapsed=trace->idle_tick-now.counter;
        const unsigned movie=(variant-1u)/2u;
        if(now.valid && elapsed<UINT32_C(0x80000000))
            toy_demand_timing_add(&trace->movie[movie].no_arm,elapsed);
        else ++C.playback[movie].bad_clock;
    }
    trace->idle_variant=0u;
    return armed;
}

static void toy_demand_after(unsigned variant,bool wake,bool previously_armed,
    const kui_retail_async_region_type *r,struct ra_clock now) {
    if(!variant)return;
    struct kui_toy_demand_trace *trace=&C.demand;
    struct kui_toy_demand_movie_trace *m=&trace->movie[(variant-1u)/2u];
    const bool armed=r->shared.stream.state==RA_DMA;
    if(wake && !previously_armed) {
        if(m->wakes!=UINT32_MAX)++m->wakes;
        if(armed && m->wake_arms!=UINT32_MAX)++m->wake_arms;
    }
    if(!armed && r->engine.active && !r->engine.failed && now.valid) {
        trace->idle_variant=variant;
        trace->idle_token=r->engine.token;
        trace->idle_tick=now.counter;
    }
}
