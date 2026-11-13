static void toy_playback_page(const struct kui_native_ra_public *ra) {
#if KUI_TOY_SUITE_MODE
#if KUI_TOY_SUITE_MODE == 1
    if(ra->suite_source==2u)retail_display_line("TOY TEST 5 1/2 PLAYBACK / 781250 HZ");
    else
#endif
    retail_display_line(KUI_TOY_TEST_TITLE " 1/2 PLAYBACK / 781250 HZ");
#elif KUI_NATIVE_TOY_HEADER_REPLAY
    retail_display_line("TOY 1/2 PLAYBACK / HEX / 781250 HZ");
#else
    retail_display_line("TOY PLAYBACK / HEX");
#endif
    retail_display_hex("MAPPED / SEEN",ra->movie_mask);
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_playback_stats *p=&ra->playback[i];
        retail_display_values(i?"INTRO SECT SPAN END FAIL CLK":"BUMPER SECT SPAN END FAIL CLK",
            p->progress,5u);
#if KUI_TOY_SUITE_ASYNC_DEMAND
        retail_display_values("GD WAIT ARG SCI WAKE MAX GD MAX BG",p->costs,5u);
        retail_display_values("GAP LBA GD BLKS BG BLKS MAX REQ",p->reads,5u);
#else
        retail_display_values("GD WAIT SCI MAX-GD MAX-SCI",p->costs,5u);
#if KUI_NATIVE_TOY_HEADER_REPLAY
        retail_display_values("GAP LBA GD SCI REPLAY",p->reads,5u);
#else
        retail_display_values("GAP LBA GD BLKS SCI BLKS MAX REQ",p->reads,5u);
#endif
#endif
    }
}
#if KUI_TOY_SUITE_MODE
static void toy_storage_page(const struct kui_native_ra_public *ra,
    const kui_retail_async_region_type *region) {
#if KUI_TOY_SUITE_MODE == 33
    (void)region;
#else
    const struct kui_sci_stream_stats *sd=&region->shared.stream.stats;
#endif
#if KUI_TOY_SUITE_MODE == 1
    if(ra->suite_source==2u)retail_display_line("TOY TEST 5 2/2 EVIDENCE / HEX");
    else
#endif
#if KUI_TOY_SUITE_MODE == 33
    retail_display_line(ra->demand.owner_verified?KUI_TOY_TEST_TITLE " 2/2 / ID1 HEX":
        KUI_TOY_TEST_TITLE " 2/2 / ID0 HEX");
#else
    retail_display_line(KUI_TOY_TEST_TITLE " 2/2 EVIDENCE / HEX");
#endif
#if KUI_TOY_SUITE_MODE == 1
    retail_display_values("LOADED CHECKED BAD LBA BYTE DATA",ra->suite,5u);
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
    retail_display_values("RAM BLOCKS HITS FIRST LAST MISS",ra->suite,5u);
#elif KUI_TOY_SUITE_MODE == 3
    retail_display_values("GUARDED PIO BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 4
    retail_display_values("BUDGET YIELDS BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 7
    retail_display_values("SERIAL BLOCKS BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 31
    retail_display_values("SLOW BLOCK RETRIES BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 33
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_demand_movie_trace *m=&ra->demand.movie[i];
        retail_display_values(i?"INTRO POLL OTHER 900 MOVIE BLOCK":"BMP POLL OTHER 900 MOVIE BLOCK",
            m->polls,4u);
        retail_display_values(i?"INTRO EXEC SUM MAX CHECK SUM MAX":"BMP EXEC SUM MAX CHECK SUM MAX",
            (uint32_t[4]){m->exec.ticks,m->exec.max,m->check.ticks,m->check.max},4u);
        retail_display_values(i?"INTRO NOARM N SUM MAX WAKE N ARM":"BMP NOARM N SUM MAX WAKE N ARM",
            (uint32_t[5]){m->no_arm.count,m->no_arm.ticks,m->no_arm.max,m->wakes,m->wake_arms},5u);
    }
#elif KUI_TOY_SUITE_ASYNC_DEMAND
    const struct kui_toy_demand_movie_trace *bumper=&ra->demand.movie[0],*intro=&ra->demand.movie[1];
    retail_display_values("BMP EXEC SUM MAX CHECK SUM MAX",(uint32_t[4]){
        bumper->exec.ticks,bumper->exec.max,bumper->check.ticks,bumper->check.max},4u);
    retail_display_values("BMP NOARM N SUM MAX WAKE N ARM",(uint32_t[5]){
        bumper->no_arm.count,bumper->no_arm.ticks,bumper->no_arm.max,bumper->wakes,bumper->wake_arms},5u);
    retail_display_values("INTRO EXEC N SUM MAX",(uint32_t[3]){
        intro->exec.count,intro->exec.ticks,intro->exec.max},3u);
    retail_display_values("INTRO CHECK N SUM MAX",(uint32_t[3]){
        intro->check.count,intro->check.ticks,intro->check.max},3u);
    retail_display_values("INTRO NOARM N SUM MAX WAKE N ARM",(uint32_t[5]){
        intro->no_arm.count,intro->no_arm.ticks,intro->no_arm.max,intro->wakes,intro->wake_arms},5u);
#elif KUI_TOY_SUITE_MODE == 26
    retail_display_values("BURST ARM CHECKED CUT FALLBACK READY",ra->suite,5u);
    const uint32_t *burst=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    retail_display_values("BURST REQUEST FIRST LEAD WIRE",burst,4u);
    retail_display_values("BURST TICK SUM MAX",burst+4u,2u);
#elif KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    const struct kui_toy_burst_metrics *q=&ra->burst_quiet;
#if KUI_TOY_SUITE_MODE == 27
    const struct kui_toy_burst_metrics *m=&ra->burst_match;
#endif
    const struct kui_toy_burst_metrics *l=&ra->burst_live;
    retail_display_values("QUIET STATUS START END",(uint32_t[]){ra->burst_quiet_status,
        ra->burst_quiet_lba,ra->burst_quiet_end},3u);
    retail_display_values("LATE START END",(uint32_t[]){ra->burst_quiet_late_lba,
        ra->burst_quiet_late_end},2u);
    retail_display_values("QUIET ARM SHORT FULL OK PCUT",(uint32_t[]){q->arms,
        q->dma_short,q->dma_full,q->checked,q->parse_full_cut},5u);
#if KUI_TOY_SUITE_MODE == 27
    retail_display_values("MATCH ARM SHORT FULL OK PCUT",(uint32_t[]){m->arms,
        m->dma_short,m->dma_full,m->checked,m->parse_full_cut},5u);
#else
    retail_display_values("FAST ARM FULL CHECKED",(uint32_t[]){ra->burst_fast.arms,
        ra->burst_fast.dma_full,ra->burst_fast.checked},3u);
    retail_display_values("SLOW ARM FULL CHECKED",(uint32_t[]){ra->burst_slow.arms,
        ra->burst_slow.dma_full,ra->burst_slow.checked},3u);
#endif
    retail_display_values("LIVE ARM SHORT FULL OK PCUT",(uint32_t[]){l->arms,
        l->dma_short,l->dma_full,l->checked,l->parse_full_cut},5u);
#endif
#if KUI_TOY_SUITE_MODE != 33
    retail_display_values("SD BLOCKS PIO START OVR CRC",(uint32_t[5]){
        sd->blocks,sd->polled,sd->starts,sd->overruns,sd->crc_errors},5u);
#endif
#if !KUI_TOY_SUITE_ASYNC_DEMAND
    retail_display_values("SOURCE TOKEN REPAIR AHEAD DEFER",(uint32_t[5]){
        ra->suite_source,sd->token_errors,sd->repaired,sd->ahead,sd->deferred},5u);
#endif
}

#endif
