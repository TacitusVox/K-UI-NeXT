/* SPDX-License-Identifier: GPL-3.0-only */
/* Terminal-only: the private issued Maple chain is retired before this
 * code borrows its first three sectors. Gameplay has no SD write path. */
#if !KUI_TOY_FMV_COMPARE
static void toy_report_string(char **at,const char *text) {
    while(*text)*(*at)++=*text++;
}

#if !KUI_TOY_WIRE_STREAM
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
#endif

#if KUI_TOY_WIRE_STREAM
#include "kui/toy_wire_report.h"
/* Test 68 has more to say than fixed-width rows leave room for in the two
 * report sectors. A row whose values are written without leading zeros,
 * and only if all of it fits before `end`: a row is never cut short and
 * nothing is ever written past the end marker's place. */
static void toy_report_short(char **at,const char *end,const char *label,
    const uint32_t *values,unsigned count) {
    char row[104],*p=row;
    while(*label && p<row+40)*p++=*label++;
    *p++='\n';
    for(unsigned i=0;i<count && i<6u;i++) {
        if(i)*p++=' ';
        unsigned shift=32u;
        while(shift>4u && !(values[i]>>(shift-4u)))shift-=4u;
        for(;shift;shift-=4u)*p++="0123456789ABCDEF"[(values[i]>>(shift-4u))&15u];
    }
    *p++='\n';
    const uint32_t bytes=(uint32_t)(p-row);
    if(bytes>(uint32_t)(end-*at))return;
    memcpy(*at,row,bytes);
    *at+=bytes;
}

#ifndef KUI_TOY_WIRE_REPORT_HOST_TEST
/* The auxiliary keeps its stream counters in the reader arena and writes
 * them through the cached view; read them the same way. */
static const struct kui_toy_wire_report *toy_wire_report_block(void) {
    const uint32_t lease=(config.lease.payload&UINT32_C(0x1fffffff))|UINT32_C(0x80000000);
    return (const void *)(uintptr_t)(lease+KUI_NATIVE_RA_DATA_OFFSET+KUI_TOY_WIRE_REPORT_OFFSET);
}
#endif

static void toy_report_wire(char **at,const char *end,const struct kui_native_ra_public *ra,
    const struct kui_sci_stream_stats *sd) {
    static const struct kui_toy_wire_report none;
    const struct kui_toy_wire_report *w=toy_wire_report_block();
    if(w->magic!=KUI_TOY_WIRE_REPORT_MAGIC)w=&none;
    const uint32_t *ahead=ra->diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
#if KUI_TOY_WIRE_MEASURE
    /* Tests 69 and 70, most telling first: a row that no longer fits is left out
     * whole. What the game showed; where the reader's time went and how
     * long requests waited; the stream; then what stopped it. */
    toy_report_short(at,end,"PIPE B I CALLS IRQ",
        (uint32_t[]){ahead[2],ahead[3],ahead[4],ahead[5]},4u);
    toy_report_short(at,end,"AHEAD B I HIT EDC STOP",
        (uint32_t[]){ahead[0],ahead[1],ra->stats.hit_requests,
            ra->stats.producer_blocks,ra->stats.abandoned},5u);
    for(unsigned n=0;n<2u;n++) {
        const unsigned i=1u-n;
        const struct kui_toy_movie_present_trace *present=&ra->present[i];
        const struct kui_toy_movie_exit_witness *x=&ra->movie_exit[i];
        const struct kui_toy_movie_onset_trace *onset=&ra->onset[i];
        toy_report_short(at,end,i?"I SUBMIT CADENCE MAX AT":"B SUBMIT CADENCE MAX AT",
            (uint32_t[]){present->submitted,present->on_time,present->max_gap,
                present->max_gap_frame},4u);
        toy_report_short(at,end,i?"I FIRST LAST SKIP EXIT WF SF":"B FIRST LAST SKIP EXIT WF SF",
            (uint32_t[]){present->first_gap,present->last_frame,x->skipped_frames,x->status,
                onset->wait_frame,onset->skip_frame},6u);
    }
    toy_report_short(at,end,"V SCI TMR GAME N TICKS",
        (uint32_t[]){w->visit_calls[0],w->visit_calls[1],w->visit_calls[2],
            w->visit_ticks[0],w->visit_ticks[1],w->visit_ticks[2]},6u);
    toy_report_short(at,end,"V MAX SCI TMR GAME",w->visit_max,KUI_TOY_WIRE_VISITS);
    toy_report_short(at,end,"Q B N T MAX I N T MAX",
        (uint32_t[]){w->wait_calls[0],w->wait_ticks[0],w->wait_max[0],
            w->wait_calls[1],w->wait_ticks[1],w->wait_max[1]},6u);
#if KUI_TOY_WIRE_CACHED
    toy_report_short(at,end,"P CCR MMU BUDGET RT RMAX",
        (uint32_t[]){w->ccr,w->mmucr,w->budget_ends,w->restart_ticks,w->restart_max},5u);
#else
    toy_report_short(at,end,"P TICK BUDGET RT RMAX",
        (uint32_t[]){w->n.ticks,w->budget_ends,w->restart_ticks,w->restart_max},4u);
#endif
    toy_report_short(at,end,"W START SEG CUT WAIT FULL",
        (uint32_t[]){w->n.starts,w->n.segments,w->n.cuts,w->n.waits,w->n.fulls},5u);
    toy_report_short(at,end,"W BYTES WTICK TMR ERR DET",
        (uint32_t[]){w->n.bytes,w->n.wait_ticks,w->timer_mode,w->first_error,w->first_detail},5u);
    toy_report_short(at,end,"W BLK FIX UNFIX HOLED NOCRC SEEK",
        (uint32_t[]){w->n.blocks,w->n.rebuilt,w->n.unsolved,w->n.holed,w->n.unchecked,
            w->seek_blocks},6u);
    toy_report_short(at,end,"W GAPI GAPT BADCRC FRAME AMBIG",
        (uint32_t[]){w->n.gap_idle,w->n.gap_token,w->n.bad_crc,w->n.lost_frame,w->n.ambiguous},5u);
    toy_report_short(at,end,"W FILL BYTES MOST REF BAD DECL",
        (uint32_t[]){w->filled,w->filled_bytes,w->most_lost,w->refused,w->check_bad,w->declined},6u);
    toy_report_short(at,end,"W STOP C J S F X",w->stops,KUI_TOY_WIRE_STOPS);
    toy_report_short(at,end,"W DROP E N I C W O",w->drops,KUI_TOY_WIRE_DROPS);
    toy_report_short(at,end,"F BIT03 BIT47 KIND",
        (uint32_t[]){w->n.frame_bit[0],w->n.frame_bit[1],w->n.frame_kind},3u);
    for(unsigned i=0;i<2u;i++) {
        const struct kui_wire_frame_note *note=&w->n.frame_note[i];
        toy_report_short(at,end,i?"F2":"F1",
            (uint32_t[]){note->before,note->after,note->gap,note->since_lost,note->blocks},5u);
    }
    toy_report_short(at,end,"SD BLK PIO START CRC",
        (uint32_t[]){sd->blocks,sd->polled,sd->starts,sd->crc_errors},4u);
}
#else
    toy_report_short(at,end,"PIPE B I CALLS IRQ FAIL",
        (uint32_t[]){ahead[2],ahead[3],ahead[4],ahead[5],
            ra->playback[0].failed+ra->playback[1].failed},5u);
    toy_report_short(at,end,"AHEAD B I HIT EDC STOP",
        (uint32_t[]){ahead[0],ahead[1],ra->stats.hit_requests,
            ra->stats.producer_blocks,ra->stats.abandoned},5u);
    toy_report_short(at,end,"SD BLOCKS PIO START CRC",
        (uint32_t[]){sd->blocks,sd->polled,sd->starts,sd->crc_errors},4u);
    toy_report_short(at,end,"W START SEG CUT WAIT FULL",
        (uint32_t[]){w->n.starts,w->n.segments,w->n.cuts,w->n.waits,w->n.fulls},5u);
    toy_report_short(at,end,"W BYTES WTICK TMR ERR DET",
        (uint32_t[]){w->n.bytes,w->n.wait_ticks,w->timer_mode,w->first_error,w->first_detail},5u);
    toy_report_short(at,end,"W BLK FIX UNFIX HOLED NOCRC SEEK",
        (uint32_t[]){w->n.blocks,w->n.rebuilt,w->n.unsolved,w->n.holed,w->n.unchecked,
            w->seek_blocks},6u);
    toy_report_short(at,end,"W GAPI GAPT BADCRC FRAME AMBIG",
        (uint32_t[]){w->n.gap_idle,w->n.gap_token,w->n.bad_crc,w->n.lost_frame,w->n.ambiguous},5u);
    toy_report_short(at,end,"W FILL BYTES MOST REFUSE BAD",
        (uint32_t[]){w->filled,w->filled_bytes,w->most_lost,w->refused,w->check_bad},5u);
    toy_report_short(at,end,"W STOP CLOSE JUMP SECT FRAME FAULT",w->stops,KUI_TOY_WIRE_STOPS);
    toy_report_short(at,end,"W DROP ERR END IDLE CMD WAIT OTHER",w->drops,KUI_TOY_WIRE_DROPS);
    toy_report_short(at,end,"W IRQ TICKS MAX PARITY DECL",
        (uint32_t[]){w->handler_calls,w->handler_ticks,w->handler_max,w->parity,w->declined},5u);
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_movie_present_trace *present=&ra->present[i];
        const struct kui_toy_movie_exit_witness *x=&ra->movie_exit[i];
        toy_report_short(at,end,i?"I SUBMIT CADENCE MAX AT TAIL":"B SUBMIT CADENCE MAX AT TAIL",
            (uint32_t[]){present->submitted,present->on_time,present->max_gap,
                present->max_gap_frame,present->tail_gap},5u);
        toy_report_short(at,end,i?"I FIRST LAST CLK OUT SKIP":"B FIRST LAST CLK OUT SKIP",
            (uint32_t[]){present->first_gap,present->last_frame,present->clock_lost,
                x->output_frames,x->skipped_frames},5u);
        toy_report_short(at,end,i?"I ENTER SCENE EXIT FRAME":"B ENTER SCENE EXIT FRAME",
            (uint32_t[]){present->render_entries,present->scene_hits,x->status,x->frame},4u);
    }
    /* Test 66's first-wait rows, as far as room is left. */
    for(unsigned i=0;i<2u;i++) {
        const struct kui_toy_movie_onset_trace *onset=&ra->onset[i];
        const struct kui_toy_movie_wait_trace *wait=&ra->wait[i];
        toy_report_short(at,end,i?"I WAIT USED NEED SKIP LBA":"B WAIT USED NEED SKIP LBA",
            (uint32_t[]){onset->wait_frame,onset->wait_used,onset->wait_missing,
                onset->skip_frame,wait->lba},5u);
        toy_report_short(at,end,i?"I READ N VERIFIED TICK AGE DONE":"B READ N VERIFIED TICK AGE DONE",
            (uint32_t[]){wait->sectors,wait->verified,wait->elapsed,wait->age,wait->to_complete},5u);
    }
    toy_report_short(at,end,"I LAG LIMIT",
        (uint32_t[]){ra->movie_exit[1].lag,ra->movie_exit[1].limit},2u);
}
#endif
#endif

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
#if KUI_TOY_WIRE_STREAM
    /* Card Tools starts this reader from its Test 66 entry. */
    test=KUI_TOY_WIRE_TICK?69u:KUI_TOY_WIRE_CACHED?70u:68u;
#endif
    if(test>=10u)*at++=(char)('0'+test/10u);
    *at++=(char)('0'+test%10u);
#if KUI_TOY_SUITE_MOVIE_POLL
    toy_report_string(&at,"\nBUILD " KUI_BUILD_ID "\nHEX / 781250 HZ\n");
#else
    toy_report_string(&at,"\nBUILD " KUI_BUILD_ID "\nVALUES HEX / TIMER 781250 HZ\n");
#endif
#if KUI_TOY_WIRE_STREAM
    toy_report_wire(&at,text+KUI_TOY_REPORT_FILE_BYTES-12u,ra,sd);
#elif KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
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
