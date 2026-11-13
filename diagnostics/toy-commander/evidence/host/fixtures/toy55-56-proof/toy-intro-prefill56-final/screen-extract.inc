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
#if KUI_TOY_SUITE_MOVIE_POLL
    (void)region;
#else
    const struct kui_sci_stream_stats *sd=&region->shared.stream.stats;
#endif
#if KUI_TOY_SUITE_MODE == 1
    if(ra->suite_source==2u)retail_display_line("TOY TEST 5 2/2 EVIDENCE / HEX");
    else
#endif
#if KUI_TOY_SUITE_MOVIE_POLL
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
#elif KUI_TOY_SUITE_MOVIE_POLL
    for(unsigned i=0;i<2u;i++) {
#if !KUI_TOY_SUITE_MOVIE_CHUNK || (KUI_TOY_SUITE_SEQUENTIAL_PIO && !KUI_TOY_SUITE_DMA_ROUND_ROBIN)
        const struct kui_toy_demand_movie_trace *m=&ra->demand.movie[i];
#endif
#if !KUI_TOY_SUITE_MOVIE_EXIT
        retail_display_values(i?"INTRO POLL OTHER 900 MOVIE BLOCK":"BMP POLL OTHER 900 MOVIE BLOCK",
            m->polls,4u);
        retail_display_values(i?"INTRO EXEC SUM MAX CHECK SUM MAX":"BMP EXEC SUM MAX CHECK SUM MAX",
            (uint32_t[4]){m->exec.ticks,m->exec.max,m->check.ticks,m->check.max},4u);
#endif
#if KUI_TOY_SUITE_MODE == 34
        retail_display_values(i?"INTRO OVR START PIO WAKE ARM":"BMP OVR START PIO WAKE ARM",
            (uint32_t[5]){m->overruns,m->starts,m->polled,m->wakes,m->wake_arms},5u);
#elif KUI_TOY_SUITE_MODE == 36
        retail_display_values(i?"INTRO DMA PR0 PR1 PR2 PR3 CUT":"BMP DMA PR0 PR1 PR2 PR3 CUT",
            (uint32_t[5]){m->dma_priority[0],m->dma_priority[1],m->dma_priority[2],
                m->dma_priority[3],m->overruns},5u);
#elif KUI_TOY_SUITE_REFILL_SLICE
#if KUI_TOY_SUITE_MOVIE_CHUNK
#if KUI_TOY_SUITE_MOVIE_TIMING
        const struct kui_toy_movie_onset_trace *onset=&ra->onset[i];
        retail_display_values(i?"INTRO WAIT USED NEED SKIP LAST":"BMP WAIT USED NEED SKIP LAST",
            (uint32_t[5]){onset->wait_frame,onset->wait_used,onset->wait_missing,
                onset->skip_frame,onset->last_out_frame},5u);
#else
        const struct kui_toy_movie_chunk_trace *chunk=&ra->movie_chunk[i];
#if KUI_TOY_SUITE_RESUME_WAKE
        retail_display_values(i?"INTRO WK WGD RSM RET EXT":"BMP WK WGD RSM RET EXT",
            (uint32_t[5]){ra->demand.movie[i].wake_resumes,chunk->wait_gd,ra->suite[i],
                ra->suite[2u+i],ra->demand.movie[i].slice_blocks},5u);
#elif KUI_TOY_SUITE_RESUME_SLICE
        retail_display_values(i?"INTRO RDY WGD RSM RET EXT":"BMP RDY WGD RSM RET EXT",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,ra->suite[i],ra->suite[2u+i],
                ra->demand.movie[i].slice_blocks},5u);
#elif KUI_TOY_SUITE_VERIFIED_RESUME
        retail_display_values(i?"INTRO RDY WGD NOGD RSM RET":"BMP RDY WGD NOGD RSM RET",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[i],ra->suite[2u+i]},5u);
#elif KUI_TOY_SUITE_DMA_ROUND_ROBIN
        retail_display_values(i?"INTRO RDY WGD NOGD DMA RR":"BMP RDY WGD NOGD DMA RR",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[2u*i],ra->suite[2u*i+1u]},5u);
#elif KUI_TOY_SUITE_FIRST_CUT_PIO
        retail_display_values(i?"INTRO RDY WGD NOGD PIO OK":"BMP RDY WGD NOGD PIO OK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,m->window_pio,m->window_ok},5u);
#elif KUI_TOY_SUITE_SEQUENTIAL_PIO
        retail_display_values(i?"INTRO RDY WGD NOGD ARM OK":"BMP RDY WGD NOGD ARM OK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[i],m->window_ok},5u);
#elif KUI_TOY_SUITE_BOUNDED_DMA
        retail_display_values(i?"INTRO RDY WGD NOGD RUN BLK":"BMP RDY WGD NOGD RUN BLK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[2u*i],ra->suite[2u*i+1u]},5u);
#else
        retail_display_values(i?"INTRO CHUNK READY WAITGD NOGD OTHER":"BMP CHUNK READY WAITGD NOGD OTHER",
            (uint32_t[4]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,chunk->other},4u);
#endif
#endif
#elif KUI_TOY_SUITE_PIO_WINDOW
        retail_display_values(i?"INTRO CUT PIO OK SLICES BLKS":"BMP CUT PIO OK SLICES BLKS",
            (uint32_t[5]){m->overruns,m->window_pio,m->window_ok,m->slice_visits,m->slice_blocks},5u);
#else
        retail_display_values(i?"INTRO CUT SLOW OK SLICES BLKS":"BMP CUT SLOW OK SLICES BLKS",
            (uint32_t[5]){m->overruns,m->retry_slow,m->retry_ok,m->slice_visits,m->slice_blocks},5u);
#endif
#if KUI_TOY_SUITE_MOVIE_EXIT
        const struct kui_toy_movie_exit_witness *w=&ra->movie_exit[i];
#if KUI_TOY_SUITE_MODE == 49 || KUI_TOY_SUITE_MODE == 50 || KUI_TOY_SUITE_MODE == 51 || KUI_TOY_SUITE_MODE == 52 || KUI_TOY_SUITE_MODE == 53 || KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 55 || KUI_TOY_SUITE_MODE == 56
        retail_display_values(i?"INTRO FLG START OUT SKIP":"BMP FLG START OUT SKIP",
            (uint32_t[4]){w->flags,w->start_status,w->output_frames,w->skipped_frames},4u);
#else
        retail_display_values(i?"INTRO START FLG CODE FRAME SAMP":"BMP START FLG CODE FRAME SAMP",
            (uint32_t[4]){w->flags,w->start_status,w->start_frame,w->start_samples},4u);
#endif
        retail_display_values(i?"INTRO EXIT CODE FRAME SAMP LAG LIM":"BMP EXIT CODE FRAME SAMP LAG LIM",
            (uint32_t[5]){w->status,w->frame,w->samples,w->lag,w->limit},5u);
#endif
#elif KUI_TOY_SUITE_MODE == 37
        retail_display_values(i?"INTRO CUT ORER SLOW OK IDLEMAX":"BMP CUT ORER SLOW OK IDLEMAX",
            (uint32_t[5]){m->overruns,m->retry_orer,m->retry_slow,m->retry_ok,m->no_arm.max},5u);
#elif KUI_TOY_SUITE_MODE != 35
        retail_display_values(i?"INTRO NOARM N SUM MAX WAKE N ARM":"BMP NOARM N SUM MAX WAKE N ARM",
            (uint32_t[5]){m->no_arm.count,m->no_arm.ticks,m->no_arm.max,m->wakes,m->wake_arms},5u);
#endif
    }
#if KUI_TOY_SUITE_MOVIE_TIMING
    retail_display_values("INTRO SKIP USED NEED",
        (uint32_t[2]){ra->onset[1].skip_used,ra->onset[1].skip_missing},2u);
#endif
#if KUI_TOY_SUITE_MODE == 35
    const struct kui_toy_window_trace *window=&ra->demand.window;
    retail_display_values("RAM REQ SECT FIRST END MAXREQ",(uint32_t[5]){
        window->requests,window->sectors,window->first_lba,window->end_lba,window->max_request_ticks},5u);
    retail_display_values("TARGET 69263 SEED RAM REQ GAP",(uint32_t[4]){
        window->seeded_blocks,window->target_ram,window->target_request_ticks,window->target_gap_ticks},4u);
#endif
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
#if !KUI_TOY_SUITE_MOVIE_POLL
    retail_display_values("SD BLOCKS PIO START OVR CRC",(uint32_t[5]){
        sd->blocks,sd->polled,sd->starts,sd->overruns,sd->crc_errors},5u);
#endif
#if !KUI_TOY_SUITE_ASYNC_DEMAND
    retail_display_values("SOURCE TOKEN REPAIR AHEAD DEFER",(uint32_t[5]){
        ra->suite_source,sd->token_errors,sd->repaired,sd->ahead,sd->deferred},5u);
#endif
}

#endif
