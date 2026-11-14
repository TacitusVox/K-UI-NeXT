/* SPDX-License-Identifier: GPL-3.0-only */

/* Test 68: the verified movie window's producer is the continuous card
 * stream of toy_wire_stream.c instead of the low reader's block-by-block
 * delivery. Everything the game sees is unchanged from Test 66: it is served
 * whole requests from sectors whose address and EDC passed.
 *
 * The low reader still opens the card: it seeks to the window's next block
 * and reads that one block by programmed transfers, exactly as before. Then
 * the stream takes the SCI and receives on without stopping per block. A
 * sector that cannot be completed, or a stream that cannot be followed,
 * hands the card back the same way and the low reader seeks again to the
 * first sector not yet verified. */
#include "toy_wire_stream.h"
#include "kui/mode1_erasure.h"

enum { TOY_WIRE_STALL_LIMIT=8u, TOY_WIRE_SEEK_STEPS=64u };
enum { TOY_WIRE_TICKS_FOREGROUND=1172u, TOY_WIRE_TICKS_INTERRUPT=390u };
#if KUI_TOY_WIRE_TICK
/* Test 69. Test 68 on the console: an interrupt visit reads for half a
 * millisecond, a full run brings more than that reads, and a stream that
 * stood still for room was read again only at the game's next call, once a
 * frame while the movie played. Now a visit that runs out of time with
 * blocks left asks the stream's timer to call again this much later, so
 * reading goes on in the same short pieces with the game running between
 * them: at most two fifths of the time, which still outruns the wire. */
enum { TOY_WIRE_TICK_GAP=586u };
#endif
/* Kept across streams and window lifetimes: the report's counters first,
 * then the working state between visits. */
struct toy_wire_keep {
    struct kui_toy_wire_report report;
    uint32_t on, bad, stalls;
    uint32_t solid;               /* verified sectors to reach before the stream takes over again */
    uint32_t lost_count;          /* of the sector being assembled */
    uint32_t block_lost_count;    /* of the block being fed */
    const uint8_t *block;
    const uint16_t *block_lost;
    uint16_t lost[KUI_MODE1_ERASURE_MAX+2u];
#if KUI_TOY_WIRE_TICK
    uint32_t restarting, restart_start; /* a stream was given up at this clock value */
    uint32_t wait_token, wait_start;    /* a request waits for the window since then */
#endif
};
#define TOY_WIRE_STATE_OFFSET (AHEAD_SLOTS*KUI_GAME_RAW_BYTES)
#define TOY_WIRE_KEEP_OFFSET (KUI_RETAIL_IMAGE_MAX_SECTORS*KUI_GAME_RAW_BYTES)
#define TOY_WIRE_RING_OFFSET (KUI_NATIVE_RA_DATA_BYTES-KUI_WIRE_RING_BYTES)
#define TOY_WIRE_TABLE_OFFSET (TOY_WIRE_STATE_OFFSET+((sizeof(struct kui_wire_stream)+31u)&~31u))
/* The staging of a request the window does not serve uses the arena from its
 * start, up to 64 raw sectors. It may run over the stream's working state,
 * which every start sets up afresh, but never over what is kept or over the
 * ring the channel writes. */
_Static_assert(TOY_WIRE_KEEP_OFFSET==KUI_TOY_WIRE_REPORT_OFFSET,
    "The worker reads the kept counters at their published offset");
_Static_assert(TOY_WIRE_TABLE_OFFSET+KUI_WIRE_TABLE_BYTES<=TOY_WIRE_KEEP_OFFSET,
    "Stream state and tables fit below the kept counters");
_Static_assert(TOY_WIRE_KEEP_OFFSET+sizeof(struct toy_wire_keep)<=TOY_WIRE_RING_OFFSET,
    "Kept counters end before the ring");
_Static_assert(TOY_WIRE_RING_OFFSET%32u==0u && TOY_WIRE_KEEP_OFFSET%32u==0u &&
    TOY_WIRE_STATE_OFFSET%32u==0u,"Each part owns whole cache lines");

static struct toy_wire_keep *toy_wire_keep(void) {
    return (void *)(toy_resume_data()+TOY_WIRE_KEEP_OFFSET);
}
static struct kui_wire_stream *toy_wire_stream(void) {
    return (void *)(toy_resume_data()+TOY_WIRE_STATE_OFFSET);
}
static void toy_ahead_write(void *,uint32_t,const uint8_t *,uint32_t);
void kui_toy_wire_timer_callback(uint32_t event);
extern uint32_t kui_native_readahead_aux_visit(uint32_t,uint32_t,
    kui_retail_async_region_type *,uint32_t);

/* Zero the counters; all of it when no stream can be running. */
static void toy_wire_reset(bool all) {
    struct toy_wire_keep *k=toy_wire_keep();
    memset(k,0,all?sizeof(*k):sizeof(k->report));
    k->report.magic=KUI_TOY_WIRE_REPORT_MAGIC;
}

/* Stop the stream wherever it stands. No card command is sent, so this is
 * allowed in the SCI interrupt: the low reader is left a card inside its
 * CMD18 at an unknown byte, which it ends before its next read. */
static void toy_wire_halt(kui_retail_async_region_type *r,unsigned why) {
    struct toy_wire_keep *k=toy_wire_keep();
    if(!k->on)return;
    struct kui_wire_stream *w=toy_wire_stream();
    struct kui_sci_stream_state *stream=&r->shared.stream;
    if(!k->report.first_error && w->error) {k->report.first_error=w->error;k->report.first_detail=w->detail;}
    if(!kui_wire_stop(w)) {
        /* The SCI did not return to command framing. Only a new
         * acquisition clears this; no further card byte is attempted. */
        if(!k->report.first_error) {k->report.first_error=KUI_WIRE_E_HANDOFF;k->report.first_detail=0u;}
        r->shared.port.fault=true;
        A.error=true;
    }
    stream->state=RA_LOST;
    stream->ready[0]=stream->ready[1]=0u;
    stream->kept=0u;
    stream->arrived=0u;
    k->on=0u;
    ++k->report.stops[why];
}

#if KUI_TOY_WIRE_TICK
/* The reader's own time in this visit, by who called. */
static void toy_wire_visit_time(struct toy_wire_keep *k,unsigned who,struct ra_clock entered) {
    if(!entered.valid)return;
    const uint32_t spent=entered.counter-clock_read().counter;
    ++k->report.visit_calls[who];
    k->report.visit_ticks[who]+=spent;
    if(spent>k->report.visit_max[who])k->report.visit_max[who]=spent;
}

/* A request the window cannot serve yet, and the moment it is served. */
static void toy_wire_wait_begin(uint32_t token) {
    struct toy_wire_keep *k=toy_wire_keep();
    if(!token || k->wait_token==token)return;
    const struct ra_clock now=clock_read();
    if(!now.valid)return;
    k->wait_token=token;
    k->wait_start=now.counter;
}
static void toy_wire_wait_end(uint32_t token) {
    struct toy_wire_keep *k=toy_wire_keep();
    if(!token || k->wait_token!=token)return;
    k->wait_token=0u;
    const struct ra_clock now=clock_read();
    if(!now.valid || !A.variant)return;
    const unsigned movie=(A.variant-1u)/2u;
    const uint32_t spent=k->wait_start-now.counter;
    ++k->report.wait_calls[movie];
    k->report.wait_ticks[movie]+=spent;
    if(spent>k->report.wait_max[movie])k->report.wait_max[movie]=spent;
}
#endif

/* Stop, and have the low reader seek again to the first sector not yet
 * verified. */
static void toy_wire_rewind(kui_retail_async_region_type *r,unsigned why) {
    struct toy_wire_keep *k=toy_wire_keep();
#if KUI_TOY_WIRE_TICK
    if(k->on && !k->restarting) {
        const struct ra_clock now=clock_read();
        k->restarting=now.valid;
        k->restart_start=now.counter;
    }
#endif
    toy_wire_halt(r,why);
    k->bad=0u;
    k->lost_count=0u;
    /* A sector the stream could not deliver is read by programmed
     * transfers, which cannot overrun, before the stream takes over again:
     * however busy the bus is just now, every rewind gains a sector. */
    k->solid=why==KUI_TOY_WIRE_STOP_SECTOR?A.verified+1u:0u;
    if(A.error)return;
    const uint32_t end=A.cursor.lba+A.cursor.count;
    if(++k->stalls>TOY_WIRE_STALL_LIMIT || A.verified>=end ||
       kui_retail_cursor_begin(&A.cursor,A.cursor.manifest,A.verified,end-A.verified,
           KUI_GAME_SECTOR_RAW,toy_ahead_write,NULL)!=KUI_GAME_OK)A.error=true;
}

/* The lost bytes of the block being fed that fall into this piece of it. */
static void toy_wire_note(struct toy_wire_keep *k,uint32_t part,const uint8_t *data,uint32_t bytes) {
    if(!k->block_lost_count)return;
    const uint32_t from=(uint32_t)(data-k->block);
    for(uint32_t i=0u;i<k->block_lost_count;i++) {
        const uint32_t at=k->block_lost[i];
        if(at<from || at-from>=bytes)continue;
        if(k->lost_count<=KUI_MODE1_ERASURE_MAX)k->lost[k->lost_count]=(uint16_t)(part+at-from);
        ++k->lost_count;
    }
}

/* A sector has all its bytes: fill the lost ones from its own parity, then
 * the same address and EDC check every sector of the window passes. */
static bool toy_wire_sector(struct toy_wire_keep *k,uint8_t *raw,uint32_t lba) {
    const uint32_t lost=k->lost_count;
    k->lost_count=0u;
    if(lost) {
        if(lost>k->report.most_lost)k->report.most_lost=lost;
        if(lost>KUI_MODE1_ERASURE_MAX || !kui_mode1_fill_lost(raw,k->lost,lost,lba+150u)) {
            ++k->report.refused;
            return false;
        }
    }
    if(raw[15]!=1u || !kui_sector_edc_valid_at(raw,lba+150u)) {
        ++k->report.check_bad;
        return false;
    }
    if(lost) {
        ++k->report.filled;
        k->report.filled_bytes+=lost;
    } else if(!k->report.parity)k->report.parity=kui_mode1_parity_holds(raw)?1u:2u;
    return true;
}

static bool toy_wire_room(void) {
    /* One block can complete a sector and begin the next. */
    return A.cursor.done<A.cursor.count && A.verified-A.first<=AHEAD_SLOTS-2u;
}

/* The low reader has stopped behind the block before the window's next one:
 * receive on from there without it. */
static bool toy_wire_take_over(kui_retail_async_region_type *r) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    struct toy_wire_keep *k=toy_wire_keep();
    struct kui_wire_stream *w=toy_wire_stream();
    /* The SCI's interrupt reaches this reader only through the resident's
     * admitted callback, at its level. Without it a cut would never be
     * answered: stay with the low reader. */
    C.config.ops->hook();
    if(!r->engine.hooked || r->engine.release.armed!=0x10u ||
       !kui_wire_start(w,&k->report.n,(uint8_t *)C.config.data+TOY_WIRE_RING_OFFSET,
           toy_resume_data()+TOY_WIRE_TABLE_OFFSET,stream->sptr,
           (uint32_t)(uintptr_t)kui_toy_wire_timer_callback)) {
        if(!k->report.first_error && r->engine.hooked && r->engine.release.armed==0x10u) {
            k->report.first_error=w->error;
            k->report.first_detail=w->detail;
        }
        ++k->report.declined;
        return false;
    }
    /* The resident passes the SCI's interrupt on, and keeps its level,
     * exactly while its stream is in this state. */
    stream->state=RA_DMA;
    k->on=1u;
    k->report.timer_mode=w->mode;
#if KUI_TOY_WIRE_TICK
    if(k->restarting) {
        const struct ra_clock now=clock_read();
        k->restarting=0u;
        if(now.valid) {
            const uint32_t spent=k->restart_start-now.counter;
            k->report.restart_ticks+=spent;
            if(spent>k->report.restart_max)k->report.restart_max=spent;
        }
    }
#endif
    return true;
}

/* Bring the card to the window's next block with the low reader, one
 * checked block by programmed transfers, then let the stream take over. */
static void toy_wire_seek(kui_retail_async_region_type *r) {
    const struct kui_native_ra_low_ops *ops=C.config.ops;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    struct toy_wire_keep *k=toy_wire_keep();
    /* Card commands come from the foreground only. */
    if(r->engine.in_irq || ops->busy())return;
    const uint8_t saved_active=stream->toy_suite_active;
    const uint8_t saved_bounded=stream->token_bounded,saved_budget=stream->token_budget;
    stream->toy_suite_active&=(uint8_t)~(KUI_TOY_SUITE_RESUME_ACTIVE|KUI_TOY_SUITE_RESUME_FOREGROUND);
    const struct ra_clock start=clock_read();
    bool declined=false;
    for(unsigned step=0u;step<TOY_WIRE_SEEK_STEPS && toy_wire_room() && !A.error;step++) {
        const uint32_t block=A.cursor.block;
        if(!declined && stream->state==RA_PAUSED && stream->position==block &&
           (!k->solid || (int32_t)(A.verified-k->solid)>=0)) {
            if(toy_wire_take_over(r))break;
            declined=true;
        }
        if(step && (!start.valid || !toy_refill_time_left(start)))break;
        /* A block the low reader still holds from the request before (two
         * requests often share one) is taken without touching the card. */
        enum kui_sci_stream_result result=KUI_SCI_STREAM_OK;
        const uint8_t *data=ops->take(block,&result);
        if(!data && result==KUI_SCI_STREAM_PENDING) {
            stream->token_bounded=1u;
            stream->token_budget=KUI_SCI_STREAM_TOKEN_SLICE;
            result=ops->fetch(block,AHEAD_TOKEN_LIMIT,true);
            if(result==KUI_SCI_STREAM_PENDING)continue;
            if(result==KUI_SCI_STREAM_OK)data=ops->take(block,&result);
        }
        if(!data) {
            /* The low reader starts the block afresh; give up only when
             * nothing gets through. */
            if(result==KUI_SCI_STREAM_RESET || ++k->stalls>TOY_WIRE_STALL_LIMIT)A.error=true;
            break;
        }
        k->block_lost_count=0u;
        const uint32_t verified=A.verified;
        if(ops->feed(&A.cursor,data)!=KUI_GAME_OK)A.error=true;
        ++k->report.seek_blocks;
        if(k->bad) {
            /* A sector read whole failed its check: read it again. */
            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_SECTOR);
            break;
        }
        if(A.verified!=verified)k->stalls=0u;
    }
    stream->toy_suite_active=saved_active;
    stream->token_bounded=saved_bounded;
    stream->token_budget=saved_budget;
}

/* Read what the stream has brought, for at most `ticks`. True if it stopped
 * for time, with more to read as far as it knows. */
static bool toy_wire_drain(kui_retail_async_region_type *r,uint32_t ticks) {
    struct kui_wire_stream *w=toy_wire_stream();
    struct toy_wire_keep *k=toy_wire_keep();
    const struct ra_clock start=clock_read();
    bool more=false;
    for(unsigned blocks=0u;;blocks++) {
        /* Between blocks: an overrun that came meanwhile is answered here,
         * since no interrupt gets through during a visit. */
        kui_wire_service(w);
        if(w->state==KUI_WIRE_FAULT) {
            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_FAULT);
            return false;
        }
        if(!toy_wire_room())break;
        if(blocks) {
            if(!start.valid) {
                if(blocks>=4u) {more=true;break;}
            } else if(start.counter-clock_read().counter>=ticks) {more=true;break;}
        }
        struct kui_wire_block block;
        const enum kui_wire_parse parse=kui_wire_next(w,&block);
        if(parse==KUI_WIRE_NEED)break;
        if(parse!=KUI_WIRE_BLOCK) {
            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_FRAME);
            return false;
        }
        const uint32_t next=A.cursor.block+1u,verified=A.verified;
        k->block=block.data;
        k->block_lost=block.lost;
        k->block_lost_count=block.lost_count;
        const enum kui_game_result fed=C.config.ops->feed(&A.cursor,block.data);
        k->block_lost_count=0u;
        if(k->bad) {
            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_SECTOR);
            return false;
        }
        if(fed!=KUI_GAME_OK || A.error) {
            A.error=true;
            return false;
        }
        if(A.verified!=verified)k->stalls=0u;
        if(A.cursor.done==A.cursor.count) {
            toy_wire_halt(r,KUI_TOY_WIRE_STOP_CLOSE);
            return false;
        }
        if(A.cursor.block!=next) {
            toy_wire_halt(r,KUI_TOY_WIRE_STOP_JUMP);
            return false;
        }
    }
    /* Room made above lets a stream that stood still for it start again. */
    kui_wire_service(w);
    return more;
}

#if KUI_TOY_WIRE_TICK
/* Read for one visit's time and, if that ran out first, have the stream's
 * timer call again. */
static void toy_wire_read(kui_retail_async_region_type *r,uint32_t ticks) {
    struct toy_wire_keep *k=toy_wire_keep();
    if(!toy_wire_drain(r,ticks) || !k->on)return;
    ++k->report.budget_ends;
    (void)kui_wire_tick(toy_wire_stream(),TOY_WIRE_TICK_GAP);
}

/* The stream's timer: a wait is over, or the last visit ran out of time.
 * The stream's registers, and the window as an SCI interrupt visit fills
 * it. No request state is read and no card command is sent from here. */
static void toy_wire_tick_visit(kui_retail_async_region_type *r) {
    struct toy_wire_keep *k=toy_wire_keep();
    if(!k->on)return;
    const struct ra_clock entered=clock_read();
    kui_wire_timer(toy_wire_stream());
    /* Only while the region is this reader's to use. */
    if(A.variant && !A.error && !C.quiesced &&
       r->engine.native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE) {
        const uint32_t verified=A.verified;
        toy_wire_read(r,TOY_WIRE_TICKS_INTERRUPT);
        C.stats.producer_blocks+=A.verified-verified;
        A.receiving=k->on || C.config.ops->busy() || r->shared.stream.state>RA_PAUSED;
        if(!A.receiving)A.receive_token=0u;
    }
    toy_wire_visit_time(k,KUI_TOY_WIRE_VISIT_TIMER,entered);
}
#endif

static void toy_ahead_pump(kui_retail_async_region_type *r,bool demand) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    struct toy_wire_keep *k=toy_wire_keep();
    const uint32_t verified=A.verified;
    const bool interrupt=r->engine.in_irq==1u;
    uint32_t *trace=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    ++trace[r->engine.in_irq?5u:4u];
#if KUI_TOY_WIRE_TICK
    const struct ra_clock entered=clock_read();
#endif
    if(k->on) {
#if !KUI_TOY_WIRE_TICK
        const struct ra_clock entered=clock_read();
#endif
        struct kui_wire_stream *w=toy_wire_stream();
        /* A request the window cannot serve yet: have the stream tell as
         * soon as the bytes it lacks are in. */
        const struct kui_retail_gd *s=&r->shared.service;
        const uint32_t have=A.verified-A.first;
        const bool waiting=demand && s->count>have;
        w->want=waiting?w->tail+(s->count-have)*(KUI_GAME_RAW_BYTES+KUI_GAME_RAW_BYTES/16u):0u;
        if(!w->want && waiting)w->want=1u;
        /* The SDK may rewrite the priority register for its own devices. */
        if(!interrupt)C.config.ops->hook();
        /* While the game waits for this request nothing else of it runs:
         * the interrupt visit may then work as long as a foreground one. */
#if KUI_TOY_WIRE_TICK
        toy_wire_read(r,interrupt && !waiting?TOY_WIRE_TICKS_INTERRUPT:TOY_WIRE_TICKS_FOREGROUND);
#else
        (void)toy_wire_drain(r,interrupt && !waiting?TOY_WIRE_TICKS_INTERRUPT:TOY_WIRE_TICKS_FOREGROUND);
        if(interrupt && entered.valid) {
            const uint32_t spent=entered.counter-clock_read().counter;
            k->report.handler_ticks+=spent;
            ++k->report.handler_calls;
            if(spent>k->report.handler_max)k->report.handler_max=spent;
        }
#endif
    }
    if(!k->on && !A.error && toy_wire_room())toy_wire_seek(r);
    C.stats.producer_blocks+=A.verified-verified;
    A.receiving=k->on || C.config.ops->busy() || stream->state>RA_PAUSED;
    if(!A.receiving)A.receive_token=0u;
#if KUI_TOY_WIRE_TICK
    toy_wire_visit_time(k,r->engine.in_irq?KUI_TOY_WIRE_VISIT_SCI:KUI_TOY_WIRE_VISIT_GAME,entered);
#endif
}

/* The SDK's interrupt table enters here for the stream's timer, by JSR with
 * interrupts masked. The visit gate gives it this module's own stack. */
void kui_toy_wire_timer_callback(uint32_t event) {
    (void)event;
    if(C.initialized)
        (void)kui_native_readahead_aux_visit(KUI_NATIVE_RA_WIRE_TIMER,0u,C.config.region,0u);
}
