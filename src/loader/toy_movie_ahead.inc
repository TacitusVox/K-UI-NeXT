/* SPDX-License-Identifier: GPL-3.0-only */

/* This private cursor reads forward from an accepted movie request. A raw
 * sector becomes eligible only after its FAD and EDC pass. The game keeps
 * its original one-command queue, cursor, destination and completion. */
#define A C.movie_ahead
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
    memcpy(raw+part,data,bytes);
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
    if(toy_wire_sector(k,raw,lba))++A.verified;
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

static bool toy_ahead_live(void) {
    if(!A.variant)return false;
    const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[(A.variant-1u)/2u];
    return (w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))==
        KUI_TOY_MOVIE_EXIT_STARTED && w->start_status==0x07dbu;
}

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
    const struct kui_toy_movie_range *range=&C.config.movie_ranges[variant-1u];
    const struct kui_retail_track *track=&e->manifest->slots[e->cursor.track].track;
    const uint32_t end=range->lba+range->count;
    if(end>track->end_lba || e->cursor.done || e->cursor.offset || !e->cursor.write ||
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
#if KUI_TOY_WIRE_MEASURE
    k->restarting=k->wait_token=0u;
#endif
#if KUI_TOY_WIRE_CACHED && !defined(KUI_NATIVE_READAHEAD_AUX_HOST_TEST)
    k->report.ccr=read_hw(UINT32_C(0xff00001c),4u);
    k->report.mmucr=read_hw(UINT32_C(0xff000010),4u);
#endif
#endif
    A.variant=(uint8_t)variant;
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

static bool toy_ahead_publish(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    /* The caller has admitted the whole unchanged native request. */
    if(s->count>A.verified-A.first)return false;
    for(uint32_t i=0u;i<s->count && !e->failed;i++)
        e->cursor.write(e->cursor.context,i*KUI_GAME_DATA_BYTES,
            toy_ahead_sector(A.first+i)+16u,KUI_GAME_DATA_BYTES);
    if(!e->failed) {
        A.first+=s->count;
        A.head+=s->count;
        if(A.head>=AHEAD_SLOTS)A.head-=AHEAD_SLOTS;
        e->cursor.done=s->count;
        e->since=e->target;
#if KUI_TOY_SUITE_MODE == 66
        uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
        trace[(A.variant-1u)/2u]+=s->count;
        ++C.stats.hit_requests;
#endif
    }
    e->active=0u;
    return true;
}

static bool toy_ahead_fallback(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    if(toy_ahead_clear(r))return false;
    if(e->in_irq) {
        if(C.config.ops->busy())(void)C.config.ops->poll(true);
    } else if(e->active) {
        /* The bounded CMD12 failed. Preserve the original cursor and report
         * an I/O failure rather than leaving the request pending forever. */
        e->failed=KUI_GD_ERROR_IO;
        e->active=0u;
    }
    return true;
}

static bool toy_movie_ahead(kui_retail_async_region_type *r,unsigned variant,bool selected,
    uint32_t wait) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    const bool demand=e->active && s->pending;
    if(demand && e->failed)return toy_ahead_fallback(r);
    if(A.variant && !demand && r->shared.stream.state==RA_RESUME &&
       A.receive_token!=e->token)A.error=true;
    const bool warm=A.variant && s->lba==A.first && s->count<=A.verified-A.first;
#if KUI_TOY_SUITE_MODE >= 64
    /* Katana's aligned GDFS submit immediately calls EXEC at 0x8c0b2b4a
     * before restoring its mask. The resident already keeps this one
     * kickoff bounded when its SCI callback is armed. Preserve its FIFO. */
    const bool kick=r->shared.stream.toy_suite_kick && e->release.armed;
#else
    const bool kick=false;
#endif
    selected=selected && s->token && !s->executing && !s->completed_bytes &&
        e->cursor.manifest==e->manifest && e->cursor.lba==s->lba && e->cursor.count==s->count &&
        !e->cursor.done && !e->cursor.offset && e->cursor.write &&
        s->count<=AHEAD_REQUEST_MAX &&
        (!wait || kick || warm) &&
        (variant<=2u || C.onset[1].quiet_ctx==UINT32_MAX);
#if KUI_TOY_SUITE_MODE == 64
    uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    if(demand && A.variant && wait && !warm) {
        if(kick && selected)++trace[4];
        if(!kick)++trace[5];
    }
#endif
    if(A.variant && (A.error || !toy_ahead_live() ||
       (!demand && s->status!=KUI_GD_COMPLETED) ||
       (s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       (demand && (!selected || variant!=A.variant || s->lba!=A.first)))) {
        if(demand)A.failed_token=s->token;
#if KUI_TOY_WIRE_STREAM
        /* Why the window is given up, for the report: the first that holds. */
        ++toy_wire_keep()->report.drops[A.error?KUI_TOY_WIRE_DROP_ERROR:
            !toy_ahead_live()?KUI_TOY_WIRE_DROP_ENDED:
            !demand && s->status!=KUI_GD_COMPLETED?KUI_TOY_WIRE_DROP_IDLE:
            s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:
            wait && !kick && !warm?KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_OTHER];
#endif
        if(toy_ahead_fallback(r))return true;
    }
    if(!e->opened || r->shared.port.fault || !r->shared.port.acquired) {
        return toy_ahead_fallback(r);
    }
    if(!A.variant) {
        if(!selected || !demand || s->token==A.failed_token ||
           (C.resume.token==s->token && C.resume.state!=TOY_RESUME_IDLE))return false;
        if(C.config.ops->busy() || !toy_ahead_begin(e,variant)) {
            A.failed_token=s->token;
            return false;
        }
        toy_resume_invalidate();
#if KUI_TOY_SUITE_MODE == 64
        ++trace[2u+(variant-1u)/2u];
#elif KUI_TOY_SUITE_MODE == 66
        ++C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][2u+(variant-1u)/2u];
#endif
    }
    if(selected && !A.error && toy_ahead_publish(r)) {
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE
        toy_wire_wait_end(s->token);
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
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE
    if(demand && selected)toy_wire_wait_begin(s->token);
#endif
    toy_ahead_pump(r,demand);
    if(A.error) {
        if(demand)A.failed_token=s->token;
        ++C.stats.abandoned;
        return toy_ahead_fallback(r);
    }
    if(selected && toy_ahead_publish(r)) {
#if KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_MEASURE
        toy_wire_wait_end(s->token);
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
#undef A
