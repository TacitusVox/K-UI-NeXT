/* SPDX-License-Identifier: GPL-3.0-only */

/* This private cursor reads forward from an accepted movie request. A raw
 * sector becomes eligible only after its FAD and EDC pass. The game keeps
 * its original one-command queue, cursor, destination and completion. */
#define A KUI_TOY_AHEAD
#ifndef KUI_TOY_WIRE_STREAM
#define KUI_TOY_WIRE_STREAM 0
#endif
/* Test 66's producer: the low reader's own delivery with a borrowed cursor. */
#define AHEAD_LOW_PIPE (KUI_TOY_SUITE_MODE == 66 && !KUI_TOY_WIRE_STREAM)
#if KUI_TOY_WIRE_STREAM
/* The arena also holds the card stream's ring and state (toy_wire_ahead.inc). */
enum { AHEAD_SLOTS=62u, AHEAD_TOKEN_WAIT=5u, AHEAD_TOKEN_LIMIT=65536u };
#else
enum { AHEAD_SLOTS=80u, AHEAD_TOKEN_WAIT=5u, AHEAD_TOKEN_LIMIT=65536u };
#endif
/* The window serves whole requests only: one must fit beside the sector
 * still being received. */
enum { AHEAD_REQUEST_MAX=AHEAD_SLOTS-2u<KUI_RETAIL_IMAGE_MAX_SECTORS?
    AHEAD_SLOTS-2u:KUI_RETAIL_IMAGE_MAX_SECTORS };
#ifndef KUI_TOY_WIRE_SEQ
#define KUI_TOY_WIRE_SEQ 0
#endif
/* Test 77: the window is opened by consecutive reads (toy_movie_ahead). */
#define AHEAD_SEQ (KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_SEQ)
#if AHEAD_SEQ
/* What a window opened outside the four movie files has for its variant:
 * the files' own are 1 to 4 and still name the report's two movies. */
enum { AHEAD_OTHER=5u };
#endif
#ifndef KUI_TOY_WIRE_ANY
#define KUI_TOY_WIRE_ANY 0
#endif
/* Test 78: a window serves a request of any size (toy_ahead_publish). */
#define AHEAD_ANY (AHEAD_SEQ && KUI_TOY_WIRE_ANY)
#ifndef KUI_TOY_WIRE_QUERY
#define KUI_TOY_WIRE_QUERY 0
#endif
/* Test 80: a window stands through a command that only asks
 * (toy_ahead_foreign). */
#define AHEAD_QUERY (AHEAD_ANY && KUI_TOY_WIRE_QUERY)
#if AHEAD_ANY
/* The most sectors one publication of a large request hands over. A visit
 * publishes twice at most, and an interrupt visit keeps other interrupts
 * waiting for as long as its copy takes: 41 microseconds a sector on the
 * console. */
enum { AHEAD_SLICE=16u };
#endif
_Static_assert(AHEAD_SLOTS*KUI_GAME_RAW_BYTES<=KUI_NATIVE_RA_DATA_BYTES,
    "Verified movie window must fit the private reader arena");
#if KUI_TOY_SUITE_MODE == 65
enum { AHEAD_RETRY_LIMIT=3u };
#define AHEAD_TRACE C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE]

static void toy_ahead_fault(unsigned stage,enum kui_sci_stream_result result,uint32_t block) {
    struct kui_native_ra_failure_snapshot *first=&C.diagnostics.first;
    if(!first->stage) {
        first->stage=stage;
        first->result=result;
        first->operation_lba=block;
    }
    ++C.diagnostics.stage_result[stage][result];
    if(stage==KUI_NATIVE_RA_STAGE_APPEND || result==KUI_SCI_STREAM_RESET ||
       ++A.retry>AHEAD_RETRY_LIMIT) A.error=true;
    else A.receiving=true;
}
#endif

static uint8_t *toy_ahead_sector(uint32_t lba) {
    uint32_t slot=A.head+lba-A.first;
    if(slot>=AHEAD_SLOTS)slot-=AHEAD_SLOTS;
    return toy_resume_data()+slot*KUI_GAME_RAW_BYTES;
}

#if KUI_TOY_WIRE_STREAM
#include "toy_wire_ahead.inc"
#endif

#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_COPY
/* Test 76: a piece of a block into its sector's place, by the hand-written
 * copy (toy_wire_copy.inc) when everything about it is a multiple of four.
 * That routine loads and stores whole words wherever it is told to: a
 * place or a length that is not one would stop the processor, and inside a
 * visit that is a reset. The blocks of a raw image that begins on a
 * multiple of four in its file always qualify; anything else is copied as
 * before. */
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
extern void kui_wire_test_copy(uint8_t *out,const uint8_t *in,uint32_t bytes);
#define kui_wire_copy kui_wire_test_copy
#else
extern void kui_wire_copy(uint8_t *out,const uint8_t *in,uint32_t bytes);
#endif
static void toy_wire_copy(uint8_t *out,const uint8_t *in,uint32_t bytes) {
    if(((uintptr_t)out|(uintptr_t)in|bytes)&3u)memcpy(out,in,bytes);
    else kui_wire_copy(out,in,bytes);
}
#endif

static void toy_ahead_write(void *unused,uint32_t offset,const uint8_t *data,uint32_t bytes) {
#if AHEAD_LOW_PIPE
    struct kui_retail_async *e=unused;
    const struct kui_retail_cursor *cursor=&e->cursor;
    if(A.error || e->failed)return;
#else
    (void)unused;
    const struct kui_retail_cursor *cursor=&A.cursor;
    if(A.error)return;
#endif
#if KUI_TOY_WIRE_STREAM
    struct toy_wire_keep *k=toy_wire_keep();
    if(k->bad)return;
#endif
    const uint32_t lba=cursor->lba+cursor->done;
    const uint32_t part=cursor->offset;
    if(lba!=A.verified || lba-A.first>=AHEAD_SLOTS ||
       offset!=cursor->done*KUI_GAME_RAW_BYTES+part || !bytes || bytes>KUI_GAME_RAW_BYTES-part) {
        A.error=true;
#if AHEAD_LOW_PIPE
        e->failed=KUI_GD_ERROR_IO;
#endif
        return;
    }
    uint8_t *raw=toy_ahead_sector(lba);
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_COPY
    toy_wire_copy(raw+part,data,bytes);
#else
    memcpy(raw+part,data,bytes);
#endif
#if KUI_TOY_WIRE_STREAM
    toy_wire_note(k,part,data,bytes);
    if(k->lost_count>KUI_MODE1_ERASURE_MAX) {
        /* Beyond what its parity can fill: no need to receive the rest. */
        if(k->lost_count>k->report.most_lost)k->report.most_lost=k->lost_count;
        ++k->report.refused;
        k->lost_count=0u;
        k->bad=1u;
        return;
    }
    if(part+bytes!=KUI_GAME_RAW_BYTES)return;
    /* A sector that fails here is read again; nothing of it is served. */
#if KUI_TOY_WIRE_STEADY
    const uint32_t check_tick=toy_wire_phase();
#endif
    const bool good=toy_wire_sector(k,raw,lba);
#if KUI_TOY_WIRE_STEADY
    k->report.check_ticks+=check_tick-toy_wire_phase();
#endif
    if(good)++A.verified;
    else k->bad=1u;
#else
    if(part+bytes!=KUI_GAME_RAW_BYTES)return;
    if(raw[15]!=1u || !kui_sector_edc_valid_at(raw,lba+150u)) {
        A.error=true;
#if AHEAD_LOW_PIPE
        e->failed=KUI_GD_ERROR_IO;
#endif
    } else ++A.verified;
#endif
}

#if !AHEAD_SEQ
/* Up to Test 76 a window lives as long as its movie plays. */
static bool toy_ahead_live(void) {
    if(!A.variant)return false;
    const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[(A.variant-1u)/2u];
    return (w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))==
        KUI_TOY_MOVIE_EXIT_STARTED && w->start_status==0x07dbu;
}
#endif

static bool toy_ahead_clear(kui_retail_async_region_type *r) {
#if KUI_TOY_WIRE_STREAM
    /* The stream is stopped at once, in an interrupt too. The card's
     * CMD12 below then waits for the foreground as before. */
    toy_wire_halt(r,KUI_TOY_WIRE_STOP_CLOSE);
#endif
    if(A.receiving) {
        if(r->engine.in_irq)return false;
        if(C.config.ops->stop()!=KUI_SCI_STREAM_OK || C.config.ops->busy())return false;
        if(!r->engine.active)C.config.ops->unhook();
    }
    A.variant=0u;
    A.receiving=false;
    A.receive_token=0u;
    return true;
}

static bool toy_ahead_begin(const struct kui_retail_async *e,unsigned variant) {
#if AHEAD_SEQ
    /* Test 77: to the end of the track the request begins on, where the
     * low reader's cursor stands before its first sector is delivered. */
    const uint32_t end=e->manifest->slots[e->cursor.track].track.end_lba;
    if(e->cursor.done || e->cursor.offset || !e->cursor.write ||
#else
    const struct kui_toy_movie_range *range=&C.config.movie_ranges[variant-1u];
    const struct kui_retail_track *track=&e->manifest->slots[e->cursor.track].track;
    const uint32_t end=range->lba+range->count;
    if(end>track->end_lba || e->cursor.done || e->cursor.offset || !e->cursor.write ||
#endif
       kui_retail_cursor_begin(&A.cursor,e->manifest,e->cursor.lba,end-e->cursor.lba,
           KUI_GAME_SECTOR_RAW,toy_ahead_write,
#if AHEAD_LOW_PIPE
           (void *)e
#else
           NULL
#endif
           )!=KUI_GAME_OK)return false;
#if KUI_TOY_WIRE_STREAM
    struct toy_wire_keep *k=toy_wire_keep();
    k->bad=k->stalls=k->solid=k->lost_count=k->block_lost_count=0u;
#if KUI_TOY_WIRE_CLEAN
    k->loose=k->mend_count=0u;
#endif
#if KUI_TOY_WIRE_MEASURE
#if KUI_TOY_WIRE_STEADY
    k->wait_token=0u;
#else
    k->restarting=k->wait_token=0u;
#endif
#endif
#if KUI_TOY_WIRE_CACHED && !KUI_TOY_WIRE_LEAN && !defined(KUI_NATIVE_READAHEAD_AUX_HOST_TEST)
    k->report.ccr=read_hw(UINT32_C(0xff00001c),4u);
    k->report.mmucr=read_hw(UINT32_C(0xff000010),4u);
#endif
#endif
#if AHEAD_SEQ
    A.variant=(uint8_t)(variant?variant:AHEAD_OTHER);
#else
    A.variant=(uint8_t)variant;
#endif
    A.first=A.verified=e->cursor.lba;
    A.head=0u;
    A.error=false;
#if KUI_TOY_SUITE_MODE >= 65
    A.retry=0u;
#endif
    return true;
}

#if KUI_TOY_SUITE_MODE != 66
static bool toy_ahead_dma_idle(void) {
    return !(read_hw(UINT32_C(0xffa0001c),4u)&7u) &&
        (read_hw(UINT32_C(0xffa00040),4u)&7u)==1u;
}

static enum kui_sci_stream_result toy_ahead_arm(kui_retail_async_region_type *r,uint32_t block,bool demand) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    /* Only foreground demand may seek/reopen. A wake or SCI callback can
     * continue a sequential token without a blocking CMD18/CMD12. */
    if((!demand || r->engine.in_irq) &&
       ((stream->state!=RA_PAUSED && stream->state!=AHEAD_TOKEN_WAIT &&
         !(stream->state==RA_RESUME && A.receive_token &&
           A.receive_token==r->engine.token)) ||
        stream->position!=block || !toy_ahead_dma_idle()))return KUI_SCI_STREAM_PENDING;
    if(r->engine.in_irq==1u && stream->state==RA_RESUME)return KUI_SCI_STREAM_PENDING;
    C.config.ops->hook();
    if(!r->engine.release.armed && !demand) {
        C.config.ops->unhook();
        return KUI_SCI_STREAM_PENDING;
    }
    const enum kui_sci_stream_result result=C.config.ops->fetch(block,AHEAD_TOKEN_LIMIT,false);
    A.receiving=true;
    A.receive_token=r->engine.token;
    return result;
}

#elif !KUI_TOY_WIRE_STREAM
/* Install the private RAW cursor only for the original native delivery
 * callback. Completion publication still sees the restored guest cursor. */
static void toy_ahead_pump(kui_retail_async_region_type *r,bool demand) {
    (void)demand;
    struct kui_retail_async *e=&r->engine;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    const struct kui_retail_cursor saved_cursor=e->cursor;
    const uint32_t saved_active=e->active,saved_failed=e->failed;
    const uint32_t saved_retries=e->retries,saved_since=e->since;
    const uint8_t saved_policy=stream->toy_suite_active,saved_poll=stream->toy_suite_poll;
    const uint32_t verified=A.verified;
    const bool room=A.cursor.done<A.cursor.count && A.verified-A.first<=AHEAD_SLOTS-2u;
    const bool was_busy=C.config.ops->busy();
    const bool owned=!A.receive_token || A.receive_token==e->token;
    const bool foreground=!e->in_irq;
    const bool slice=foreground && room && (saved_poll==1u || saved_poll==2u);

    if(!room && !was_busy)return;
    e->cursor=A.cursor;
    e->active=room;
    e->failed=0u;
    e->retries=A.retry;
    e->since=0u;
    /* Idle UPDATE has no GD save frame. Keep every scoped delivery on
     * the native one-pass branch, including when its SCI route is lost. */
    if(!saved_poll)stream->toy_suite_poll=2u;
    stream->toy_suite_active=saved_policy|1u;
    if(owned)stream->toy_suite_active|=KUI_TOY_SUITE_RESUME_ACTIVE|
        (foreground || e->in_irq==2u?KUI_TOY_SUITE_RESUME_FOREGROUND:0u);
    if(!was_busy)A.receive_token=e->token;
    C.config.ops->hook();
    uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    ++trace[foreground?4u:5u];
    if(slice)toy_resume_slice(r,(A.variant-1u)/2u);
    else C.config.ops->deliver(0u);
    A.cursor=e->cursor;
    A.retry=(uint8_t)e->retries;
    A.error=A.error || e->failed;
    C.stats.producer_blocks+=A.verified-verified;
    A.receiving=C.config.ops->busy() || stream->state>RA_PAUSED;
    if(!A.receiving)A.receive_token=0u;
    e->cursor=saved_cursor;
    e->active=saved_active;
    e->failed=saved_failed;
    e->retries=saved_retries;
    e->since=saved_since;
    stream->toy_suite_active=saved_policy;
    stream->toy_suite_poll=saved_poll;
}
#endif

#if KUI_TOY_SUITE_MODE != 66
/* One block per callback. Start the next DMA before CRC/EDC on the ready
 * block, reserving space for both and for a block that straddles sectors. */
static void toy_ahead_pump(kui_retail_async_region_type *r,bool demand) {
    const struct kui_native_ra_low_ops *ops=C.config.ops;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    if(A.cursor.done==A.cursor.count || A.verified-A.first>AHEAD_SLOTS-2u)return;
    const uint8_t saved_active=stream->toy_suite_active;
    const uint8_t saved_bounded=stream->token_bounded,saved_budget=stream->token_budget;
    /* An intact DMA block can cross a REQUEST boundary. A parked suffix
     * remains associated with the token that started that DMA. */
    if(A.receive_token && A.receive_token==r->engine.token)
        stream->toy_suite_active|=KUI_TOY_SUITE_RESUME_ACTIVE|
        (r->engine.in_irq!=1u?KUI_TOY_SUITE_RESUME_FOREGROUND:0u);
    stream->token_bounded=1u;
    stream->token_budget=KUI_SCI_STREAM_TOKEN_SLICE;
    enum kui_sci_stream_result result=KUI_SCI_STREAM_PENDING;
    const uint32_t block=A.cursor.block;
#if KUI_TOY_SUITE_MODE == 65
    if(A.retry) {
        /* Retire an already-started successor without appending it. Only
         * foreground may reopen at the failed block, using the usual
         * sliced command phases and CRC-checked programmed read. */
        if(ops->busy()) {
            result=ops->poll(r->engine.in_irq!=0u);
            if(result==KUI_SCI_STREAM_RESET)
                toy_ahead_fault(KUI_NATIVE_RA_STAGE_POLL,result,block);
            if(result==KUI_SCI_STREAM_PENDING || A.error)goto restore;
        }
        if(r->engine.in_irq)goto restore;
        stream->toy_suite_active&=(uint8_t)~(KUI_TOY_SUITE_RESUME_ACTIVE|KUI_TOY_SUITE_RESUME_FOREGROUND);
        result=ops->fetch(block,AHEAD_TOKEN_LIMIT,true);
        if(result>KUI_SCI_STREAM_PENDING)toy_ahead_fault(KUI_NATIVE_RA_STAGE_FETCH,result,block);
        if(result!=KUI_SCI_STREAM_OK)goto restore;
    } else
#endif
    if(ops->busy()) {
        result=ops->poll(r->engine.in_irq!=0u);
        if(result==KUI_SCI_STREAM_PENDING)goto restore;
        if(result!=KUI_SCI_STREAM_OK) {
#if KUI_TOY_SUITE_MODE == 65
            toy_ahead_fault(KUI_NATIVE_RA_STAGE_POLL,result,block);
#else
            A.error=true;
#endif
            goto restore;
        }
    }
    const bool ready=stream->arrived==block+1u;
    enum kui_sci_stream_result next=KUI_SCI_STREAM_PENDING;
    if(ready && A.cursor.run>1u && A.verified-A.first<=AHEAD_SLOTS-3u
#if KUI_TOY_SUITE_MODE == 65
       && !A.retry
#endif
       ) {
        /* A repair's ownership is tied to the currently accepted token.
         * The hardware of an ahead-only DMA is retired with repairs off. */
        next=toy_ahead_arm(r,block+1u,false);
    }
#if KUI_TOY_SUITE_MODE != 65
    if(next>KUI_SCI_STREAM_PENDING)A.error=true;
#endif
    const uint8_t *data=ops->take(block,&result);
    if(data) {
        const uint32_t verified=A.verified;
        if(ops->feed(&A.cursor,data)!=KUI_GAME_OK)A.error=true;
        C.stats.producer_blocks+=A.verified-verified;
#if KUI_TOY_SUITE_MODE == 65
        if(A.retry) {
            ++AHEAD_TRACE[3];
            A.retry=0u;
        }
        if(A.error)toy_ahead_fault(KUI_NATIVE_RA_STAGE_APPEND,KUI_SCI_STREAM_CRC,block);
        else if(next>KUI_SCI_STREAM_PENDING)
            toy_ahead_fault(KUI_NATIVE_RA_STAGE_FETCH,next,block+1u);
#endif
        if(!A.error && !ops->busy() && stream->state!=AHEAD_TOKEN_WAIT
#if KUI_TOY_SUITE_MODE == 65
           && !A.retry
#endif
           ) {
            A.receiving=false;
            A.receive_token=0u;
        }
    } else if(result!=KUI_SCI_STREAM_PENDING) {
#if KUI_TOY_SUITE_MODE == 65
        toy_ahead_fault(KUI_NATIVE_RA_STAGE_TAKE,result,block);
#else
        A.error=true;
#endif
    }
    if(!A.error && !ready && !data && !ops->busy()) {
        result=toy_ahead_arm(r,block,demand);
        if(result>KUI_SCI_STREAM_PENDING) {
#if KUI_TOY_SUITE_MODE == 65
            toy_ahead_fault(KUI_NATIVE_RA_STAGE_FETCH,result,block);
#else
            A.error=true;
#endif
        }
    }
restore:
    stream->toy_suite_active=saved_active;
    stream->token_bounded=saved_bounded;
    stream->token_budget=saved_budget;
}

#endif

#if AHEAD_ANY
/* Test 78. A request of up to sixty sectors is handed over whole, as
 * before. A larger one a slice at a time, from where its own cursor says
 * it has got to: what the window has of it, sixteen sectors at most. True
 * once the request is complete, or has failed in the game's memory. */
static bool toy_ahead_publish(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    struct kui_toy_wire_report *w=&toy_wire_keep()->report;
    const bool large=s->count>AHEAD_REQUEST_MAX;
    const uint32_t had=e->cursor.done,have=A.verified-A.first;
    uint32_t give=s->count-had;
    if(give>have) {
        if(!large)return false;
        give=have;
    }
    if(large && give>AHEAD_SLICE)give=AHEAD_SLICE;
    if(!give)return false;
#if KUI_TOY_WIRE_STEADY
    const uint32_t publish_tick=toy_wire_phase();
#endif
    for(uint32_t i=0u;i<give && !e->failed;i++)
        e->cursor.write(e->cursor.context,(had+i)*KUI_GAME_DATA_BYTES,
            toy_ahead_sector(A.first+i)+16u,KUI_GAME_DATA_BYTES);
#if KUI_TOY_WIRE_STEADY
    w->publish_ticks+=publish_tick-toy_wire_phase();
#endif
    if(!e->failed) {
        A.first+=give;
        A.head+=give;
        if(A.head>=AHEAD_SLOTS)A.head-=AHEAD_SLOTS;
        e->cursor.done=had+give;
        if(A.variant==AHEAD_OTHER)w->other_sectors+=give;
        else C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][(A.variant-1u)/2u]+=give;
        /* More of it is owed: the request stays the window's. */
        if(e->cursor.done<s->count)return false;
        e->since=e->target;
        ++C.stats.hit_requests;
        if(large) {
            ++w->large_requests;
            w->large_sectors+=s->count;
        }
    }
    e->active=0u;
    return true;
}

/* A request the window had begun to serve goes back to the low reader
 * whole. Its cursor counts the sectors the window delivered, and for the
 * card it still stands where the request began: the low reader, which
 * reads by that cursor, would put the request's first blocks where its
 * later sectors belong. Begun again, the cursor has the low reader
 * deliver every sector from the first, over what the window wrote, which
 * is the same bytes. */
static void toy_ahead_restart(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    const kui_retail_cursor_write write=e->cursor.write;
    void *const context=e->cursor.context;
    ++toy_wire_keep()->report.large_returned;
    if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba,s->count,
           KUI_GAME_SECTOR_MODE1,write,context)!=KUI_GAME_OK) {
        e->failed=KUI_GD_ERROR_IO;
        e->active=0u;
    }
}
#else
static bool toy_ahead_publish(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    /* The caller has admitted the whole unchanged native request. */
    if(s->count>A.verified-A.first)return false;
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_STEADY
    const uint32_t publish_tick=toy_wire_phase();
#endif
    for(uint32_t i=0u;i<s->count && !e->failed;i++)
        e->cursor.write(e->cursor.context,i*KUI_GAME_DATA_BYTES,
            toy_ahead_sector(A.first+i)+16u,KUI_GAME_DATA_BYTES);
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_STEADY
    toy_wire_keep()->report.publish_ticks+=publish_tick-toy_wire_phase();
#endif
    if(!e->failed) {
        A.first+=s->count;
        A.head+=s->count;
        if(A.head>=AHEAD_SLOTS)A.head-=AHEAD_SLOTS;
        e->cursor.done=s->count;
        e->since=e->target;
#if AHEAD_SEQ
        if(A.variant==AHEAD_OTHER)toy_wire_keep()->report.other_sectors+=s->count;
        else C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][(A.variant-1u)/2u]+=s->count;
        ++C.stats.hit_requests;
#elif KUI_TOY_SUITE_MODE == 66
        uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
        trace[(A.variant-1u)/2u]+=s->count;
        ++C.stats.hit_requests;
#endif
    }
    e->active=0u;
    return true;
}
#endif

static bool toy_ahead_fallback(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
#if AHEAD_ANY
    /* Test 78: whether the window has handed this request sectors. Only
     * a window that stands right behind them has. */
    const bool begun=A.variant && e->active && e->cursor.done &&
        r->shared.service.lba+e->cursor.done==A.first;
    if(toy_ahead_clear(r)) {
        if(begun)toy_ahead_restart(r);
        return false;
    }
#else
    if(toy_ahead_clear(r))return false;
#endif
    if(e->in_irq) {
        if(C.config.ops->busy())(void)C.config.ops->poll(true);
    } else if(e->active) {
#if AHEAD_SEQ
        /* Test 77: a window is given up at the end of every run of
         * consecutive reads, where up to Test 76 that was twice a console
         * run. A stop command that failed once is not the request's
         * failure. The stream is halted, and the card's command is the low
         * reader's to end before its own read, where it tries again as it
         * does for any read. Only a reception still under way is not its
         * to take over. */
        if(!C.config.ops->busy()) {
            A.variant=0u;
            A.receiving=false;
            A.receive_token=0u;
#if AHEAD_ANY
            if(begun)toy_ahead_restart(r);
#endif
            return false;
        }
#endif
        /* The bounded CMD12 failed. Preserve the original cursor and report
         * an I/O failure rather than leaving the request pending forever. */
        e->failed=KUI_GD_ERROR_IO;
        e->active=0u;
    }
    return true;
}

#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_QUICK
/* Test 74. A delivery visit that finds a window, no request, and nothing in
 * the request state, the movie or the bus for toy_movie_ahead to act on:
 * every reason it has to give the window up, to mark it in error or to
 * serve from it is absent, and all it would do is pump under the rule for
 * idle calls and return. The dispatcher does that directly. Each term is
 * one of toy_movie_ahead's own, in its order; a visit that fails any of
 * them takes the whole path as before. */
static bool toy_ahead_plain(const kui_retail_async_region_type *r) {
    const struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    return A.variant && !e->active &&
        e->native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE &&
        r->shared.stream.state!=RA_RESUME &&
#if AHEAD_SEQ
        !A.error && s->status==KUI_GD_COMPLETED &&
#else
        !A.error && toy_ahead_live() && s->status==KUI_GD_COMPLETED &&
#endif
        (!s->command || s->command==KUI_GD_PIOREAD || s->command==KUI_GD_DMAREAD) &&
        e->opened && !r->shared.port.fault && r->shared.port.acquired;
}
#endif

#if AHEAD_QUERY
/* Test 80. Whether the game's command, which is no read, gives the window
 * up. One that only asks the drive something, or does nothing, does not:
 * the service answers it from what it holds (src/core/retail_gd.c), without the
 * card, and the drive's position, which says whether the next read follows
 * on, is as it was. GETTOC 18, GETTOC2 19, NOP 29, REQ_MODE 30, GETSCD 34,
 * REQ_STAT 36, GET_VERS 40. Any other does: SEEK and STOP move the drive,
 * INIT sets what a sector is, SET_MODE changes the drive's mode words (and
 * nothing a window rests on: it is given up for out of caution), PLAY,
 * PAUSE and RELEASE are the audio's, and what the service does not list it
 * has refused.
 *
 * The report keeps which such commands a window met, a bit to a number,
 * and counts those it stood through, each once. */
static bool toy_ahead_foreign(const struct kui_retail_gd *s) {
    static const uint32_t asks[2]={
        UINT32_C(1)<<KUI_RETAIL_GD_GETTOC|UINT32_C(1)<<KUI_GD_GETTOC2|UINT32_C(1)<<KUI_GD_NOP|
            UINT32_C(1)<<KUI_RETAIL_GD_REQ_MODE,
        UINT32_C(1)<<(KUI_RETAIL_GD_GETSCD-32u)|UINT32_C(1)<<(KUI_RETAIL_GD_REQ_STAT-32u)|
            UINT32_C(1)<<(KUI_RETAIL_GD_GET_VERS-32u)};
    struct toy_wire_keep *k=toy_wire_keep();
    const uint32_t command=s->command;
    if(command>=64u)return true;
    const uint32_t bit=UINT32_C(1)<<(command&31u);
    k->report.command_seen[command>>5]|=bit;
    if(!(asks[command>>5]&bit))return true;
    if(k->query_token!=s->token) {
        k->query_token=s->token;
        ++k->report.queries;
    }
    return false;
}
#endif

static bool toy_movie_ahead(kui_retail_async_region_type *r,unsigned variant,bool selected,
    uint32_t wait) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    const bool demand=e->active && s->pending;
    if(demand && e->failed)return toy_ahead_fallback(r);
    if(A.variant && !demand && r->shared.stream.state==RA_RESUME &&
       A.receive_token!=e->token)A.error=true;
#if AHEAD_ANY
    /* Test 78: where the request has got to. A window that serves a large
     * request in slices stands at its next sector, and the request's own
     * cursor counts the sectors it has had. */
    const uint32_t had=demand?e->cursor.done:0u;
    const uint32_t at=s->lba+had,left=s->count-had;
    const bool warm=A.variant && at==A.first && left<=A.verified-A.first;
#else
    const bool warm=A.variant && s->lba==A.first && s->count<=A.verified-A.first;
#endif
#if KUI_TOY_SUITE_MODE >= 64
    /* Katana's aligned GDFS submit immediately calls EXEC at 0x8c0b2b4a
     * before restoring its mask. The resident already keeps this one
     * kickoff bounded when its SCI callback is armed. Preserve its FIFO. */
    const bool kick=r->shared.stream.toy_suite_kick && e->release.armed;
#else
    const bool kick=false;
#endif
#if AHEAD_ANY
    /* A request is the window's untouched, of any size, or as far as a
     * window standing at its next sector has served it: a large one. */
    selected=selected && s->token && !s->executing &&
        s->completed_bytes<=had*KUI_GAME_DATA_BYTES &&
        e->cursor.manifest==e->manifest && e->cursor.lba==s->lba && e->cursor.count==s->count &&
        (!had || (left && s->count>AHEAD_REQUEST_MAX && A.variant && at==A.first)) &&
        !e->cursor.offset && e->cursor.write &&
#else
    selected=selected && s->token && !s->executing && !s->completed_bytes &&
        e->cursor.manifest==e->manifest && e->cursor.lba==s->lba && e->cursor.count==s->count &&
        !e->cursor.done && !e->cursor.offset && e->cursor.write &&
        s->count<=AHEAD_REQUEST_MAX &&
#endif
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_STAY
        /* Test 75: a request the game waits for keeps the window that
         * stands at its first sector (toy_wire_stay). */
        (!wait || kick || warm ||
#if AHEAD_ANY
         (A.variant && at==A.first && !A.error && toy_wire_stay(s->token)))
#elif AHEAD_SEQ
         (A.variant && s->lba==A.first && !A.error && toy_wire_stay(s->token)))
#else
         (A.variant==variant && s->lba==A.first && !A.error && toy_wire_stay(s->token)))
#endif
#else
        (!wait || kick || warm)
#endif
#if !(KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_OPEN)
        /* Test 57's hold on the second movie's refills at its start: the
         * window serves that movie only once the hold is over. Test 74
         * has no hold (kui_toy_movie_refill_defer). */
        && (variant<=2u || C.onset[1].quiet_ctx==UINT32_MAX)
#endif
        ;
#if KUI_TOY_SUITE_MODE == 64
    uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    if(demand && A.variant && wait && !warm) {
        if(kick && selected)++trace[4];
        if(!kick)++trace[5];
    }
#endif
#if AHEAD_SEQ
    /* Test 77: a window ends for a read elsewhere, not with a movie. That
     * is a read that does not begin at the window's first sector, and one
     * that reaches beyond the window's end, the end of its track: such a
     * read would wait for sectors this window never receives. (A window
     * that has served its track's last sector stands at the first sector
     * of the next track.) And a request the window has served in an
     * interrupt visit, which the resident has yet to report as complete,
     * is no failed one: off its movie path, which is a read outside the
     * movie files or in a file's first 386 sectors, the resident reports
     * only from the game's next call. */
    const bool idle=!demand && !s->pending && s->status!=KUI_GD_COMPLETED;
#if AHEAD_ANY
    /* Test 78: by what is still owed of the request, from its next
     * sector. */
    const bool beyond=left>A.cursor.lba+A.cursor.count-A.first;
#if AHEAD_QUERY
    /* Test 80: of the commands that are no reads, those that only ask
     * leave the window. */
    const bool foreign=A.variant && s->command && s->command!=KUI_GD_PIOREAD &&
        s->command!=KUI_GD_DMAREAD && toy_ahead_foreign(s);
    if(A.variant && (A.error || idle || foreign ||
#else
    if(A.variant && (A.error || idle ||
       (s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
#endif
       (demand && (!selected || at!=A.first || beyond)))) {
#else
    if(A.variant && (A.error || idle ||
       (s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       (demand && (!selected || s->lba!=A.first ||
           s->count>A.cursor.lba+A.cursor.count-A.first)))) {
#endif
        if(demand)A.failed_token=s->token;
        /* Why, for the report: the first that holds. A read the game
         * waits for counts as waited only where the window had the sectors
         * to give it: off the low reader's movie path every later call of
         * a request is one the game waits in, whatever the window lacks.
         * A window is counted when it is gone: an interrupt visit cannot
         * end the card's command, and leaves the window to the game's
         * next call. */
#if AHEAD_ANY
        const unsigned why=
            A.error?KUI_TOY_WIRE_DROP_ERROR:idle?KUI_TOY_WIRE_DROP_IDLE:
#if AHEAD_QUERY
            foreign?KUI_TOY_WIRE_DROP_COMMAND:
#else
            s->command && s->command!=KUI_GD_PIOREAD &&
                s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:
#endif
            wait && !kick && !warm && !beyond?KUI_TOY_WIRE_DROP_WAITING:
            KUI_TOY_WIRE_DROP_OTHER;
#else
        const unsigned why=A.error?KUI_TOY_WIRE_DROP_ERROR:
            idle?KUI_TOY_WIRE_DROP_IDLE:
            s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:
            wait && !kick && !warm && s->count<=A.cursor.lba+A.cursor.count-A.first?
            KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_OTHER;
#endif
        if(toy_ahead_fallback(r))return true;
        ++toy_wire_keep()->report.drops[why];
    }
#else
    if(A.variant && (A.error || !toy_ahead_live() ||
       (!demand && s->status!=KUI_GD_COMPLETED) ||
       (s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       (demand && (!selected || variant!=A.variant || s->lba!=A.first)))) {
        if(demand)A.failed_token=s->token;
#if KUI_TOY_WIRE_STREAM
        /* Why the window is given up, for the report: the first that holds.
         * (Test 76 prints other things in that row's room; the counts are
         * kept, and the host fixtures read them.) */
        ++toy_wire_keep()->report.drops[A.error?KUI_TOY_WIRE_DROP_ERROR:
            !toy_ahead_live()?KUI_TOY_WIRE_DROP_ENDED:
            !demand && s->status!=KUI_GD_COMPLETED?KUI_TOY_WIRE_DROP_IDLE:
            s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:
            wait && !kick && !warm?KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_OTHER];
#endif
        if(toy_ahead_fallback(r))return true;
    }
#endif
    if(!e->opened || r->shared.port.fault || !r->shared.port.acquired) {
        return toy_ahead_fallback(r);
    }
    if(!A.variant) {
        if(!selected || !demand || s->token==A.failed_token ||
           (C.resume.token==s->token && C.resume.state!=TOY_RESUME_IDLE))return false;
#if AHEAD_SEQ
        /* Test 77: a window is opened for a read that begins where the
         * read before it ended, by the service's own drive position (the
         * last sector a read delivered), and where the stream can take the
         * card over: with the resident's SCI route selected, and the low
         * reader not in the middle of a wait of its own. (A read the game
         * ended while the low reader looked for its block's token leaves
         * that search standing. The window's seek would carry it on as
         * the low reader's own reception, which nothing then ends.) Any
         * other read is the low reader's as it always was. */
        if(s->lba!=s->position_lba+1u || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;
#endif
        if(C.config.ops->busy() || !toy_ahead_begin(e,variant)) {
            A.failed_token=s->token;
            return false;
        }
        toy_resume_invalidate();
#if AHEAD_SEQ
        if(variant)++C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][2u+(variant-1u)/2u];
        else ++toy_wire_keep()->report.other_windows;
#elif KUI_TOY_SUITE_MODE == 64
        ++trace[2u+(variant-1u)/2u];
#elif KUI_TOY_SUITE_MODE == 66
        ++C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][2u+(variant-1u)/2u];
#endif
    }
    if(selected && !A.error && toy_ahead_publish(r)) {
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_ANY
        toy_wire_wait_end(s->token);
#endif
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_ANY
        if(!e->failed)toy_wire_course_served(s->count);
#endif
        if(!e->failed) {
#if KUI_TOY_SUITE_MODE != 66
            uint32_t *stats=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
            stats[(variant-1u)/2u]+=s->count;
            ++C.stats.hit_requests;
#endif
            toy_ahead_pump(r,false);
        }
        return true;
    }
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_ANY
    if(demand && selected)toy_wire_wait_begin(s->token);
#endif
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_IDLE
    /* One of the game's GD calls with no request to serve or to wait for
     * pumps only when the reader needs the foreground or has not been read
     * for a while. An interrupt visit, a waiting request and an update
     * call the hook has already passed on pump as before. */
    if(demand || e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())
#endif
    toy_ahead_pump(r,demand);
    if(A.error) {
        if(demand)A.failed_token=s->token;
        ++C.stats.abandoned;
        return toy_ahead_fallback(r);
    }
    if(selected && toy_ahead_publish(r)) {
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_ANY
        toy_wire_wait_end(s->token);
#endif
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_ANY
        if(!e->failed)toy_wire_course_served(s->count);
#endif
#if KUI_TOY_SUITE_MODE == 65
        if(!e->failed)++AHEAD_TRACE[4];
#endif
    }
    return true;
}
#if KUI_TOY_SUITE_MODE == 65
#undef AHEAD_TRACE
#endif
#undef AHEAD_ANY
#undef AHEAD_SEQ
#undef AHEAD_QUERY
#undef A
