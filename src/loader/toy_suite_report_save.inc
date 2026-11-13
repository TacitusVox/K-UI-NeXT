/* SPDX-License-Identifier: GPL-3.0-only */
/* Terminal-only: the private issued Maple chain is retired before this
 * code borrows its first three sectors. Gameplay has no SD write path. */
#if !KUI_TOY_FMV_COMPARE
static void toy_report_string(char **at,const char *text) {
    while(*text)*(*at)++=*text++;
}

static void toy_report_row(char **at,const char *label,const uint32_t *values,unsigned count) {
    toy_report_string(at,label);
    *(*at)++='\n';
    for(unsigned i=0;i<count;i++) {
        if(i)*(*at)++=' ';
        for(unsigned shift=32u;shift;shift-=4u)
            *(*at)++="0123456789ABCDEF"[(values[i]>>(shift-4u))&15u];
    }
    *(*at)++='\n';
}

#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
static void toy_report_burst(char **at,const char *label,const struct kui_toy_burst_metrics *m) {
    toy_report_row(at,label,(uint32_t[]){m->arms,m->requested,m->received,m->dma_short,
        m->dma_full,m->checked,m->parse_full_cut,m->bad_token,m->bad_crc,m->timeout,
        m->reset,m->usable,m->command_error},13u);
}

static void toy_report_short(char **at,const char *label,const struct kui_toy_burst_cut *cut) {
    toy_report_row(at,label,(uint32_t[]){cut->tcr,cut->chcr2,cut->c2dst,cut->g2adst},4u);
}

#if KUI_TOY_SUITE_MODE == 28
static void toy_report_rate(char **at,const char *label,const struct kui_toy_burst_rate *rate) {
    toy_report_row(at,label,(uint32_t[]){rate->arms,rate->dma_full,rate->checked},3u);
}
#endif

static void toy_report_compare(char **at,const struct kui_native_ra_public *ra) {
    toy_report_row(at,"MAPPED SOURCE",(uint32_t[]){ra->movie_mask,ra->suite_source},2u);
    toy_report_row(at,"BUMPER SECT SPAN END FAIL CLK",ra->playback[0].progress,5u);
    const struct kui_toy_playback_stats *intro=&ra->playback[1];
    toy_report_row(at,"INTRO SECT SPAN END FAIL CLK",intro->progress,5u);
    toy_report_row(at,"INTRO GD WAIT SCI MAXREQ",(uint32_t[]){intro->costs[0],
        intro->costs[1],intro->costs[2],intro->reads[4]},4u);
    toy_report_row(at,"QUIET STATUS START END LATESTART LATEEND",(uint32_t[]){ra->burst_quiet_status,
        ra->burst_quiet_lba,ra->burst_quiet_end,ra->burst_quiet_late_lba,ra->burst_quiet_late_end},5u);
    toy_report_string(at,"ARM REQB RXB SHORT FULL OK PCUT TOK CRC TIMEOUT RESET USE CMD\n");
    toy_report_burst(at,"QUIET",&ra->burst_quiet);
#if KUI_TOY_SUITE_MODE == 27
    toy_report_burst(at,"MATCH",&ra->burst_match);
#endif
    toy_report_burst(at,"LIVE",&ra->burst_live);
#if KUI_TOY_SUITE_MODE == 28
    toy_report_rate(at,"FAST ARM FULL CHECKED",&ra->burst_fast);
    toy_report_rate(at,"SLOW ARM FULL CHECKED",&ra->burst_slow);
#endif
    toy_report_short(at,"LAST LIVE SHORT TCR CHCR2 C2DST G2ADST",&ra->burst_live.last_short);
    const uint32_t *burst=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    toy_report_row(at,"LIVE REQ FIRST READY FALLBACK LEAD",(uint32_t[]){burst[0],burst[1],
        ra->suite[4],ra->suite[3],burst[2]},5u);
}
#endif

static void toy_report_format(char *text,const struct kui_native_ra_public *ra,
    const struct kui_sci_stream_stats *sd,unsigned test) {
    /* The terminal format tests also exercise every field at full width. */
    memset(text,' ',KUI_TOY_REPORT_FILE_BYTES);
    char *at=text;
    toy_report_string(&at,"K-UI TOY COMMANDER REPORT\nTEST ");
    if(test>=10u)*at++=(char)('0'+test/10u);
    *at++=(char)('0'+test%10u);
#if KUI_TOY_SUITE_MOVIE_POLL
    toy_report_string(&at,"\nBUILD " KUI_BUILD_ID "\nHEX / 781250 HZ\n");
#else
    toy_report_string(&at,"\nBUILD " KUI_BUILD_ID "\nVALUES HEX / TIMER 781250 HZ\n");
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    (void)sd;
    toy_report_compare(&at,ra);
#elif KUI_TOY_SUITE_FRAME_TIMING
    toy_report_string(&at,ra->demand.owner_verified?"RAW ID1\n":"RAW ID0\n");
#if KUI_TOY_SUITE_MODE == 65
    uint32_t recovery[4],first[4];
    (void)sd;
    toy_ahead_recovery_report(ra,recovery,first);
    toy_report_row(&at,"FIX POLL TAKE FETCH",recovery,4u);
    toy_report_row(&at,"FIRST CARD WHY RAW SERVED",first,4u);
#elif KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 66
    const uint32_t *admission=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    (void)sd;
#if KUI_TOY_SUITE_MODE == 66
    toy_report_row(&at,"PIPE B I CALLS IRQ FAIL",
#else
    toy_report_row(&at,"OPEN B I KICK WAIT FAIL",
#endif
        (uint32_t[]){admission[2],admission[3],admission[4],admission[5],
            ra->playback[0].failed+ra->playback[1].failed},5u);
#elif KUI_TOY_SUITE_MODE >= 61
    toy_report_row(&at,"B I SECT FAIL CRC",
        (uint32_t[]){ra->playback[0].sectors,ra->playback[1].sectors,
            ra->playback[0].failed,ra->playback[1].failed,sd->crc_errors},5u);
#else
    toy_report_row(&at,"SD BLOCKS CRC",(uint32_t[]){sd->blocks,sd->crc_errors},2u);
#endif
#if KUI_TOY_SUITE_MODE == 66
    toy_report_row(&at,"SD BLOCKS PIO OVR CRC",
        (uint32_t[]){sd->blocks,sd->polled,sd->overruns,sd->crc_errors},4u);
#elif KUI_TOY_SUITE_MODE != 65
#if KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 66
    const char *intro_defer_label="I DEFER WHY FIRST END";
#else
    const char *intro_defer_label="INTRO DEFER WHY FIRST END";
#endif
    toy_report_row(&at,intro_defer_label,
        (uint32_t[]){ra->onset[1].quiet_defers,ra->onset[1].quiet_reason,
            ra->suite[4],ra->onset[1].quiet_end_frame},4u);
#endif
#if KUI_TOY_SUITE_MODE >= 62
    const uint32_t *ahead=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    toy_report_row(&at,"AHEAD B I HIT EDC STOP",
        (uint32_t[]){ahead[0],ahead[1],ra->stats.hit_requests,
            ra->stats.producer_blocks,ra->stats.abandoned},5u);
#elif KUI_TOY_SUITE_MODE == 61
    toy_report_row(&at,"B CAP FIRST LAST SECT",
        (uint32_t[]){ra->onset[0].quiet_defers,ra->onset[0].quiet_tick,
            ra->onset[0].quiet_end_frame,ra->onset[0].quiet_reason},4u);
#endif
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_movie_onset_trace *onset=&ra->onset[i];
        const struct kui_toy_movie_wait_trace *wait=&ra->wait[i];
        const struct kui_toy_movie_present_trace *present=&ra->present[i];
        const struct kui_toy_movie_exit_witness *w=&ra->movie_exit[i];
#if KUI_TOY_SUITE_MODE == 60
        const struct kui_toy_playback_stats *p=&ra->playback[i];
        toy_report_row(&at,i?"I SECT SPAN END FAIL":"B SECT SPAN END FAIL",p->progress,4u);
        toy_report_row(&at,i?"I READ GAP LBA REQMAX":"B READ GAP LBA REQMAX",
            (uint32_t[]){p->gap,p->gap_lba,p->longest},3u);
#endif
        toy_report_row(&at,i?"I WAIT USED NEED SKIP LBA":"B WAIT USED NEED SKIP LBA",
            (uint32_t[]){onset->wait_frame,onset->wait_used,onset->wait_missing,
                onset->skip_frame,wait->lba},5u);
#if KUI_TOY_SUITE_MODE >= 61
        toy_report_row(&at,i?"I READ N VERIFIED TICK AGE DONE":"B READ N VERIFIED TICK AGE DONE",
            (uint32_t[]){wait->sectors,wait->verified,wait->elapsed,wait->age,wait->to_complete},5u);
#else
        toy_report_row(&at,i?"I READ N VERIFIED TICK":"B READ N VERIFIED TICK",
            (uint32_t[]){wait->sectors,wait->verified,wait->elapsed},3u);
#endif
        toy_report_row(&at,i?"I SUBMIT CADENCE MAX AT TAIL":"B SUBMIT CADENCE MAX AT TAIL",
            (uint32_t[]){present->submitted,present->on_time,present->max_gap,
                present->max_gap_frame,present->tail_gap},5u);
#if KUI_TOY_SUITE_MODE >= 61
        toy_report_row(&at,i?"I FIRST LAST CLK OUT SKIP":"B FIRST LAST CLK OUT SKIP",
            (uint32_t[]){present->first_gap,present->last_frame,present->clock_lost,
                w->output_frames,w->skipped_frames},5u);
        toy_report_row(&at,i?"I ENTER SCENE EXIT FRAME":"B ENTER SCENE EXIT FRAME",
            (uint32_t[]){present->render_entries,present->scene_hits,w->status,w->frame},4u);
#else
        toy_report_row(&at,i?"I FIRST LAST CLOCK LOST":"B FIRST LAST CLOCK LOST",
            (uint32_t[]){present->first_gap,present->last_frame,present->clock_lost},3u);
        toy_report_row(&at,i?"I EXIT FRAME LAG LIMIT":"B EXIT FRAME LAG LIMIT",
            (uint32_t[]){w->status,w->frame,w->lag,w->limit},4u);
#endif
    }
#if KUI_TOY_SUITE_MODE >= 61
    toy_report_row(&at,"I LAG LIMIT",(uint32_t[]){ra->movie_exit[1].lag,ra->movie_exit[1].limit},2u);
#endif
#else
#if !KUI_TOY_SUITE_MOVIE_POLL
    toy_report_row(&at,"MAPPED SEEN",&ra->movie_mask,1u);
#endif
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_playback_stats *p=&ra->playback[i];
        toy_report_row(&at,i?"INTRO SECT SPAN END FAIL CLK":"BUMPER SECT SPAN END FAIL CLK",p->progress,5u);
#if KUI_TOY_SUITE_MODE == 34
        toy_report_row(&at,"GD BG",(uint32_t[2]){p->foreground,p->sci_irq},2u);
        toy_report_row(&at,"GAP LBA GDBLK BGBLK REQMAX",p->reads,5u);
#elif KUI_TOY_SUITE_MOVIE_POLL
        toy_report_row(&at,"GD BG MAXGD MAXBG",(uint32_t[4]){
            p->foreground,p->sci_irq,p->foreground_max,p->sci_irq_max},4u);
        toy_report_row(&at,"GAP LBA GDBLK BGBLK REQMAX",p->reads,5u);
#elif KUI_TOY_SUITE_ASYNC_DEMAND
        toy_report_row(&at,"GD WAITARG BG MAX-GD MAX-BG",p->costs,5u);
        toy_report_row(&at,"GAP LBA GD-BLOCKS BG-BLOCKS MAX-REQ",p->reads,5u);
#else
        toy_report_row(&at,"GD WAIT SCI MAX-GD MAX-SCI",p->costs,5u);
        toy_report_row(&at,"GAP LBA GD-BLOCKS SCI-BLOCKS MAX-REQ",p->reads,5u);
#endif
    }
#if KUI_TOY_SUITE_MODE == 1
    toy_report_row(&at,"LOADED CHECKED BAD-LBA BYTE DATA",ra->suite,5u);
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
    toy_report_row(&at,"RAM-BLOCKS HITS FIRST LAST MISS",ra->suite,5u);
#elif KUI_TOY_SUITE_MODE == 3
    toy_report_row(&at,"GUARDED PIO BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 4
    toy_report_row(&at,"BUDGET YIELDS BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 7
    toy_report_row(&at,"SERIAL BLOCKS BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 31
    toy_report_row(&at,"SLOW BLOCK RETRIES BUMPER INTRO",ra->suite,2u);
#elif KUI_TOY_SUITE_MODE == 26
    toy_report_row(&at,"BURST ARM CHECKED CUT FALLBACK READY",ra->suite,5u);
    const uint32_t *burst=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    toy_report_row(&at,"BURST REQUEST FIRST LEAD WIRE",burst,4u);
    toy_report_row(&at,"BURST TICK SUM MAX",burst+4u,2u);
#endif
    toy_report_row(&at,"SD-BLOCKS PIO START OVR CRC",(uint32_t[5]){
        sd->blocks,sd->polled,sd->starts,sd->overruns,sd->crc_errors},5u);
#if KUI_TOY_SUITE_ASYNC_DEMAND
    /* Async demand has no erased-byte repair counters. Keep both movies' timing
     * in the existing two sectors by using that row's space for these data. */
#if KUI_TOY_SUITE_MOVIE_TIMING
    toy_report_string(&at,ra->demand.owner_verified?"RAW ID1\n":"RAW ID0\n");
#elif KUI_TOY_SUITE_MOVIE_POLL
    toy_report_string(&at,ra->demand.owner_verified?"BG=SCI+WAKE RAW2352 ID1\n":"BG=SCI+WAKE RAW2352 ID0\n");
#else
    toy_report_string(&at,"BG=SCI IRQ AND CONTROLLER WAKE / RAW2352\n");
#endif
    for(unsigned i=0;i<2u;i++) {
#if !KUI_TOY_SUITE_MOVIE_CHUNK || (KUI_TOY_SUITE_SEQUENTIAL_PIO && !KUI_TOY_SUITE_DMA_ROUND_ROBIN)
        const struct kui_toy_demand_movie_trace *m=&ra->demand.movie[i];
#endif
#if !KUI_TOY_SUITE_MOVIE_EXIT
#if KUI_TOY_SUITE_MODE == 34
        toy_report_row(&at,i?"INTRO EXEC CHECK N SUM MAX":"BMP EXEC CHECK N SUM MAX",
#elif KUI_TOY_SUITE_MODE == 38
        toy_report_row(&at,i?"INTRO EXEC / CHECK":"BMP EXEC / CHECK",
#else
        toy_report_row(&at,i?"INTRO EXEC N SUM MAX CHECK N SUM MAX":"BMP EXEC N SUM MAX CHECK N SUM MAX",
#endif
            (uint32_t[6]){m->exec.count,m->exec.ticks,m->exec.max,m->check.count,m->check.ticks,m->check.max},6u);
#endif
#if KUI_TOY_SUITE_MODE == 36
        toy_report_row(&at,i?"INTRO DMA PR0 PR1 PR2 PR3 CUT":"BMP DMA PR0 PR1 PR2 PR3 CUT",
            (uint32_t[5]){m->dma_priority[0],m->dma_priority[1],m->dma_priority[2],
                m->dma_priority[3],m->overruns},5u);
#elif KUI_TOY_SUITE_MODE == 38
        toy_report_row(&at,i?"INTRO CUT SLOW OK IDLE SLICES BLKS":"BMP CUT SLOW OK IDLE SLICES BLKS",
            (uint32_t[6]){m->overruns,m->retry_slow,m->retry_ok,m->no_arm.max,
                m->slice_visits,m->slice_blocks},6u);
#elif KUI_TOY_SUITE_MOVIE_EXIT
#if KUI_TOY_SUITE_MOVIE_CHUNK
#if KUI_TOY_SUITE_MOVIE_TIMING
        const struct kui_toy_movie_onset_trace *onset=&ra->onset[i];
#if KUI_TOY_SUITE_DECODE_WINDOW
        if(!i)toy_report_row(&at,"INTRO DEFER WHY FIRST END",
            (uint32_t[4]){ra->onset[1].quiet_defers,ra->onset[1].quiet_reason,
                ra->suite[4],ra->onset[1].quiet_end_frame},4u);
        else
#endif
        toy_report_row(&at,i?"I WAIT USED NEED SKIP LAST":"B WAIT USED NEED SKIP LAST",
            (uint32_t[5]){onset->wait_frame,onset->wait_used,onset->wait_missing,
                onset->skip_frame,onset->last_out_frame},5u);
#else
        const struct kui_toy_movie_chunk_trace *chunk=&ra->movie_chunk[i];
#if KUI_TOY_SUITE_RESUME_WAKE
        toy_report_row(&at,i?"INTRO WK WGD RSM RET EXT":"BMP WK WGD RSM RET EXT",
            (uint32_t[5]){ra->demand.movie[i].wake_resumes,chunk->wait_gd,ra->suite[i],
                ra->suite[2u+i],ra->demand.movie[i].slice_blocks},5u);
#elif KUI_TOY_SUITE_RESUME_SLICE
        toy_report_row(&at,i?"INTRO RDY WGD RSM RET EXT":"BMP RDY WGD RSM RET EXT",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,ra->suite[i],ra->suite[2u+i],
                ra->demand.movie[i].slice_blocks},5u);
#elif KUI_TOY_SUITE_VERIFIED_RESUME
        toy_report_row(&at,i?"INTRO RDY WGD NOGD RSM RET":"BMP RDY WGD NOGD RSM RET",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[i],ra->suite[2u+i]},5u);
#elif KUI_TOY_SUITE_DMA_ROUND_ROBIN
        toy_report_row(&at,i?"INTRO RDY WGD NOGD DMA RR":"BMP RDY WGD NOGD DMA RR",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[2u*i],ra->suite[2u*i+1u]},5u);
#elif KUI_TOY_SUITE_FIRST_CUT_PIO
        toy_report_row(&at,i?"INTRO RDY WGD NOGD PIO OK":"BMP RDY WGD NOGD PIO OK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,m->window_pio,m->window_ok},5u);
#elif KUI_TOY_SUITE_SEQUENTIAL_PIO
        toy_report_row(&at,i?"INTRO RDY WGD NOGD ARM OK":"BMP RDY WGD NOGD ARM OK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[i],m->window_ok},5u);
#elif KUI_TOY_SUITE_BOUNDED_DMA
        toy_report_row(&at,i?"INTRO RDY WGD NOGD RUN BLK":"BMP RDY WGD NOGD RUN BLK",
            (uint32_t[5]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,ra->suite[2u*i],ra->suite[2u*i+1u]},5u);
#else
        toy_report_row(&at,i?"INTRO CHUNK READY WAITGD NOGD OTHER":"BMP CHUNK READY WAITGD NOGD OTHER",
            (uint32_t[4]){chunk->ready,chunk->wait_gd,chunk->wait_no_gd,chunk->other},4u);
#endif
#endif
#elif KUI_TOY_SUITE_PIO_WINDOW
        toy_report_row(&at,i?"INTRO CUT PIO OK SLICES BLKS":"BMP CUT PIO OK SLICES BLKS",
            (uint32_t[5]){m->overruns,m->window_pio,m->window_ok,m->slice_visits,m->slice_blocks},5u);
#else
        toy_report_row(&at,i?"INTRO CUT SLOW OK SLICES BLKS":"BMP CUT SLOW OK SLICES BLKS",
            (uint32_t[5]){m->overruns,m->retry_slow,m->retry_ok,m->slice_visits,m->slice_blocks},5u);
#endif
        const struct kui_toy_movie_exit_witness *w=&ra->movie_exit[i];
#if KUI_TOY_SUITE_MODE == 49 || KUI_TOY_SUITE_MODE == 50 || KUI_TOY_SUITE_MODE == 51 || KUI_TOY_SUITE_MODE == 52 || KUI_TOY_SUITE_MODE == 53 || KUI_TOY_SUITE_MODE == 54 || KUI_TOY_SUITE_MODE == 55 || KUI_TOY_SUITE_MODE == 56 || KUI_TOY_SUITE_MODE == 57 || KUI_TOY_SUITE_MODE == 58 || KUI_TOY_SUITE_MODE == 59 || KUI_TOY_SUITE_MODE == 60 || KUI_TOY_SUITE_MODE == 61 || KUI_TOY_SUITE_MODE == 62 || KUI_TOY_SUITE_MODE == 64 || KUI_TOY_SUITE_MODE == 65 || KUI_TOY_SUITE_MODE == 66
        toy_report_row(&at,i?"INTRO FLG START OUT SKIP":"BMP FLG START OUT SKIP",
            (uint32_t[4]){w->flags,w->start_status,w->output_frames,w->skipped_frames},4u);
#else
        toy_report_row(&at,i?"INTRO START FLG CODE FRAME SAMP":"BMP START FLG CODE FRAME SAMP",
            (uint32_t[4]){w->flags,w->start_status,w->start_frame,w->start_samples},4u);
#endif
        toy_report_row(&at,i?"INTRO EXIT CODE FRAME SAMP LAG LIM":"BMP EXIT CODE FRAME SAMP LAG LIM",
            (uint32_t[5]){w->status,w->frame,w->samples,w->lag,w->limit},5u);
#elif KUI_TOY_SUITE_MODE == 37
        toy_report_row(&at,i?"INTRO CUT ORER SLOW OK IDLEMAX":"BMP CUT ORER SLOW OK IDLEMAX",
            (uint32_t[5]){m->overruns,m->retry_orer,m->retry_slow,m->retry_ok,m->no_arm.max},5u);
#elif KUI_TOY_SUITE_MODE != 35
        toy_report_row(&at,i?"INTRO NOARM N SUM MAX WAKE N ARM":"BMP NOARM N SUM MAX WAKE N ARM",
            (uint32_t[5]){m->no_arm.count,m->no_arm.ticks,m->no_arm.max,m->wakes,m->wake_arms},5u);
#endif
    }
#if KUI_TOY_SUITE_MOVIE_TIMING
    toy_report_row(&at,"I SKIP USED NEED",
        (uint32_t[2]){ra->onset[1].skip_used,ra->onset[1].skip_missing},2u);
#endif
#if KUI_TOY_SUITE_MOVIE_POLL && !KUI_TOY_SUITE_MOVIE_EXIT
    const struct kui_toy_demand_movie_trace *bumper=&ra->demand.movie[0],*opening=&ra->demand.movie[1];
    const uint32_t *bmp=bumper->polls,*intro=opening->polls;
    toy_report_row(&at,
#if KUI_TOY_SUITE_MODE == 34
        "POLL OTHER 900 MOVIE BLOCK BMP INTRO",
#else
        "POLL OTHER 900 MOVIE BLOCK / BMP INTRO",
#endif
        (uint32_t[8]){
        bmp[0],bmp[1],bmp[2],bmp[3],intro[0],intro[1],intro[2],intro[3]},8u);
#if KUI_TOY_SUITE_MODE == 34
    toy_report_row(&at,"REC OVR START PIO BMP INTRO",(uint32_t[6]){
        bumper->overruns,bumper->starts,bumper->polled,
        opening->overruns,opening->starts,opening->polled},6u);
#elif KUI_TOY_SUITE_MODE == 35
    const struct kui_toy_window_trace *window=&ra->demand.window;
    toy_report_row(&at,"RAM REQ SECT FIRST END MAXREQ",(uint32_t[5]){
        window->requests,window->sectors,window->first_lba,window->end_lba,window->max_request_ticks},5u);
    toy_report_row(&at,"TARGET 69263 SEED RAM REQ GAP",(uint32_t[4]){
        window->seeded_blocks,window->target_ram,window->target_request_ticks,window->target_gap_ticks},4u);
#endif
#endif
#else
    toy_report_row(&at,"SOURCE TOKEN REPAIR AHEAD DEFER",(uint32_t[5]){
        ra->suite_source,sd->token_errors,sd->repaired,sd->ahead,sd->deferred},5u);
    toy_report_string(&at,"SOURCE 1=RAW2352 2=COOKED2048\n");
#endif
#endif
    /* End marker follows the second physical block even for short reports. */
    memcpy(text+KUI_TOY_REPORT_FILE_BYTES-12u,"\nEND REPORT\n",12u);
}
#endif

static bool toy_suite_save_report(const struct kui_native_ra_public *ra,
    const kui_retail_async_region_type *r) {
    if(!config.toy_report.test || !state.armed || state.pending ||
       state.submitted!=state.completed || !reader_only() ||
       (current_sr()&0xf0u)!=0xf0u || !lease_valid() || !stack_valid())return false;
    _Static_assert(sizeof(issued_chain)>=3u*KUI_TOY_REPORT_SECTOR_BYTES,"terminal report workspace");
#if KUI_TOY_FMV_COMPARE
    (void)r;
    kui_toy_fmv_format((char *)issued_chain,&ra->fmv_trace,"SCI",KUI_BUILD_ID);
#else
    toy_report_format((char *)issued_chain,ra,&r->shared.stream.stats,config.toy_report.test);
#endif
    int (*lease)(kui_vmu_sd_transaction,void *)=(void *)(uintptr_t)config.sd_lease;
    return lease(kui_vmu_native_persistent_sd_transaction,&config.toy_report)==KUI_VMU_SD_LEASE_OK;
}
