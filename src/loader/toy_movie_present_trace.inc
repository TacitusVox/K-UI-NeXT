/* SPDX-License-Identifier: GPL-3.0-only */

/* Both hooks run on the foreground game thread. The scene hook can run
 * during UPDATE, but cannot overlap a START or terminal boundary record. */
static struct {
    uint32_t owner, start_tick, on_time_limit;
    struct kui_toy_movie_present_trace *trace;
    bool start_valid, last_valid;
#if KUI_TOY_SUITE_MODE >= 61
    bool render_active;
#endif
} toy_present;

static uint32_t toy_present_word(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}

void kui_toy_movie_present_boundary(uint32_t ctx,uintptr_t witness,unsigned phase) {
    const bool intro=witness==(uintptr_t)&C.movie_exit[1];
    struct kui_toy_movie_present_trace *trace=&C.present[intro];
    if(phase==KUI_TOY_MOVIE_EXIT_STARTED) {
        if(C.movie_exit[intro].start_status!=0x07dbu)return;
#if KUI_TOY_SUITE_MODE < 62
        const uint32_t fps=toy_present_word(ctx+0xa0u);
#endif
        const struct ra_clock now=clock_read();
        toy_present.owner=ctx;
        toy_present.start_tick=now.counter;
#if KUI_TOY_SUITE_MODE >= 62
        toy_present.on_time_limit=0u;
        trace->on_time=UINT32_MAX;
#else
        toy_present.on_time_limit=fps>=10u && fps<=60u ? 1171875u/fps : 0u;
#endif
        toy_present.trace=trace;
        toy_present.start_valid=now.valid;
        toy_present.last_valid=false;
#if KUI_TOY_SUITE_MODE >= 61
        toy_present.render_active=false;
#endif
        if(!now.valid)++trace->clock_lost;
        return;
    }
    if(ctx!=toy_present.owner || trace!=toy_present.trace)return;
    const struct ra_clock now=clock_read();
    const uint32_t gap=trace->last_tick-now.counter;
    if(!now.valid || (toy_present.last_valid && gap>INT32_MAX))++trace->clock_lost;
    else if(toy_present.last_valid)trace->tail_gap=gap;
    toy_present.owner=0u;
}

#if defined(KUI_TOY_WIRE_WORDS) && KUI_TOY_WIRE_WORDS
static void toy_wire_course_frame(uint32_t now);
#endif
#if KUI_TOY_SUITE_MODE >= 61
static void kui_toy_movie_present_observe(uint32_t ctx) {
#else
void kui_toy_movie_present_observe(uint32_t ctx) {
#endif
    if(!ctx || ctx!=toy_present.owner)return;
    const struct ra_clock now=clock_read();
    const uint32_t frame=toy_present_word(ctx+0x1c0u)+1u;
    struct kui_toy_movie_present_trace *trace=toy_present.trace;
#if KUI_TOY_SUITE_MODE >= 62
    /* Original AVI timing setup fills the rate during the first UPDATE.
     * START only opens the movie, so read the rate at scene submission. */
    if(!toy_present.on_time_limit) {
        const uint32_t fps=toy_present_word(ctx+0xa0u);
        if(fps>=10u && fps<=60u) {
            toy_present.on_time_limit=1171875u/fps;
            trace->on_time=0u;
        }
    }
#endif
    if(!now.valid)++trace->clock_lost;
    else if(!trace->submitted && toy_present.start_valid) {
        const uint32_t gap=toy_present.start_tick-now.counter;
        if(gap<=INT32_MAX)trace->first_gap=gap;
        else ++trace->clock_lost;
    } else if(toy_present.last_valid) {
        const uint32_t gap=trace->last_tick-now.counter;
        if(gap>INT32_MAX)++trace->clock_lost;
        else {
            if(gap>trace->max_gap) {
                trace->max_gap=gap;
                trace->max_gap_frame=trace->last_frame;
            }
            if(toy_present.on_time_limit && gap<=toy_present.on_time_limit &&
               frame==trace->last_frame+1u)++trace->on_time;
        }
    }
#if defined(KUI_TOY_WIRE_WORDS) && KUI_TOY_WIRE_WORDS
    if(now.valid)toy_wire_course_frame(now.counter);
#endif
    ++trace->submitted;
    trace->last_frame=frame;
    trace->last_tick=now.counter;
    toy_present.last_valid=now.valid;
}

#if KUI_TOY_SUITE_MODE >= 61
/* Use the actual callback argument instead of inferring owner or the outer
 * caller's return address from the renderer's live stack. Keep counts of
 * both arrivals, including when the exact owner test declines a sample. */
void kui_toy_movie_render_enter(uint32_t ctx) {
    if(!toy_present.owner)return;
    ++toy_present.trace->render_entries;
    if(ctx==toy_present.owner)toy_present.render_active=true;
}

void kui_toy_movie_render_leave(uint32_t ctx) {
    if(ctx==toy_present.owner)toy_present.render_active=false;
}

void kui_toy_movie_scene_observe(void) {
    if(!toy_present.owner)return;
    ++toy_present.trace->scene_hits;
    if(toy_present.render_active)kui_toy_movie_present_observe(toy_present.owner);
}
#endif
