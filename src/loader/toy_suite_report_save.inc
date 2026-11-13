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
    toy_report_string(&at,"\nBUILD " KUI_BUILD_ID "\nVALUES HEX / TIMER 781250 HZ\n");
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    (void)sd;
    toy_report_compare(&at,ra);
#else
    toy_report_row(&at,"MAPPED SEEN",&ra->movie_mask,1u);
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_playback_stats *p=&ra->playback[i];
        toy_report_row(&at,i?"INTRO SECT SPAN END FAIL CLK":"BUMPER SECT SPAN END FAIL CLK",p->progress,5u);
        toy_report_row(&at,"GD WAIT SCI MAX-GD MAX-SCI",p->costs,5u);
        toy_report_row(&at,"GAP LBA GD-BLOCKS SCI-BLOCKS MAX-REQ",p->reads,5u);
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
#elif KUI_TOY_SUITE_MODE == 26
    toy_report_row(&at,"BURST ARM CHECKED CUT FALLBACK READY",ra->suite,5u);
    const uint32_t *burst=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    toy_report_row(&at,"BURST REQUEST FIRST LEAD WIRE",burst,4u);
    toy_report_row(&at,"BURST TICK SUM MAX",burst+4u,2u);
#endif
    toy_report_row(&at,"SD-BLOCKS PIO START OVR CRC",(uint32_t[5]){
        sd->blocks,sd->polled,sd->starts,sd->overruns,sd->crc_errors},5u);
    toy_report_row(&at,"SOURCE TOKEN REPAIR AHEAD DEFER",(uint32_t[5]){
        ra->suite_source,sd->token_errors,sd->repaired,sd->ahead,sd->deferred},5u);
    toy_report_string(&at,"SOURCE 1=RAW2352 2=COOKED2048\n");
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
