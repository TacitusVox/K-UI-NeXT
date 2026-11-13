/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hash.h"
#if KUI_TOY_SUITE_FAST_COPY
#include "toy_resume_copy.inc"
#endif

enum { TOY_RESUME_IDLE, TOY_RESUME_STAGE, TOY_RESUME_RESTART, TOY_RESUME_STRICT };
_Static_assert(KUI_RETAIL_IMAGE_MAX_SECTORS*KUI_GAME_RAW_BYTES<=KUI_NATIVE_RA_DATA_BYTES,
    "All admitted raw request bytes fit the existing owned arena");

static uint8_t *toy_resume_data(void) {
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    return C.config.data;
#else
    /* Only the CPU touches this arena in the movie-resume profile. The
     * cold bind has already purged the cache lines of the owned lease. */
    return (void *)((uintptr_t)C.config.data&~UINT32_C(0x20000000));
#endif
}

static void toy_resume_invalidate(void) {
    C.resume.token=0u;
    C.resume.state=TOY_RESUME_IDLE;
}

static void toy_resume_write(void *context,uint32_t offset,const uint8_t *bytes,uint32_t count) {
    struct kui_retail_async *e=context;
    if(e->failed)return;
    const uint32_t begin=C.resume.checked*KUI_GAME_RAW_BYTES;
    const uint32_t end=begin+KUI_GAME_RAW_BYTES;
    if(C.resume.checked>=C.resume.cursor.count || offset!=C.resume.written ||
       offset<begin || offset>=end || !count || count>end-offset) {
        e->failed=KUI_GD_ERROR_IO;
        return;
    }
    uint8_t *data=toy_resume_data();
#if KUI_TOY_SUITE_FAST_COPY
    toy_resume_copy(data+offset,bytes,count);
#else
    memcpy(data+offset,bytes,count);
#endif
    C.resume.written=offset+count;
    if(C.resume.written==end) {
        const uint8_t *raw=data+begin;
        const uint32_t fad=C.resume.cursor.lba+C.resume.checked+150u;
#if KUI_TOY_SUITE_WIDE_EDC
        const bool valid=kui_mode1_edc_table_valid_at(raw,fad,toy_edc_table(&C.config));
#else
        const bool valid=raw[15]==1u && kui_sector_edc_valid_at(raw,fad);
#endif
        if(!valid) {
            e->failed=KUI_GD_ERROR_IO;
            return;
        }
        ++C.resume.checked;
    }
}

static bool toy_resume_begin(kui_retail_async_region_type *r,unsigned variant) {
    struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    const struct kui_retail_cursor *c=&e->cursor;
    if(!c->write || c->manifest!=e->manifest || c->done || c->offset ||
       c->lba!=s->lba || !c->count || c->count!=s->count || c->count>KUI_RETAIL_IMAGE_MAX_SECTORS ||
       c->lba>719851u-c->count || !r->shared.port.acquired || r->shared.port.fault)
        return false;
    if(kui_retail_cursor_begin(&C.resume.cursor,c->manifest,c->lba,c->count,
        KUI_GAME_SECTOR_RAW,toy_resume_write,e)!=KUI_GAME_OK)return false;
    C.resume.written=C.resume.checked=0u;
    C.resume.resumed_start=r->shared.stream.stats.toy_resumed_blocks;
    C.resume.variant=(uint8_t)variant;
    C.resume.state=TOY_RESUME_STAGE;
    return true;
}

static void toy_resume_retry(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    /* Every candidate write went to the private arena. Keep the same
     * original request pending. CMD12 can wait, so do it in foreground. */
    e->failed=0u;e->active=1u;
    if(e->in_irq) {
        /* The usual low prefetch may already own the next SD block.
         * Acknowledge its eventual receive interrupt without delivering
         * bytes or starting CMD12 from the interrupt handler. */
        if(C.config.ops->busy())(void)C.config.ops->poll(true);
        return;
    }
    if(C.config.ops->stop()!=KUI_SCI_STREAM_OK) {
        e->failed=KUI_GD_ERROR_IO;e->active=0u;
    }
    e->retries=e->since=0u;
    C.resume.state=TOY_RESUME_STRICT;
}

#if KUI_TOY_SUITE_RESUME_SLICE
static void toy_resume_slice(kui_retail_async_region_type *r,unsigned clip) {
    const struct ra_clock start=clock_read();
    const uint32_t blocks=r->engine.stats.call_blocks;
    const uint32_t polled=r->shared.stream.stats.polled;
    /* The recognized foreground EXEC keeps each low call bounded. Keep
     * the private cursor and resume admission installed across the slice. */
    C.config.ops->deliver(0u);
    if(!start.valid)return;
    const uint32_t first=r->engine.stats.call_blocks;
    unsigned extra=0u;
    while(r->engine.active && !r->engine.failed &&
          r->engine.stats.call_blocks-blocks<TOY_REFILL_BLOCKS &&
          r->shared.stream.stats.polled==polled && extra<TOY_REFILL_EXTRA_LIMIT) {
        if(!toy_refill_time_left(start))break;
        ++extra;
        C.config.ops->deliver(0u);
    }
    if(extra) {
        struct kui_toy_demand_movie_trace *movie=&C.demand.movie[clip];
        ++movie->slice_visits;
        movie->slice_blocks+=r->engine.stats.call_blocks-first;
    }
}
#endif

static bool toy_resume_deliver(uint32_t wait,kui_retail_async_region_type *r,
    unsigned variant,bool selected) {
    struct kui_retail_async *e=&r->engine;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    const struct kui_retail_gd *s=&r->shared.service;
    if(!s->pending || !s->token || e->token!=s->token) {
        toy_resume_invalidate();
        return false;
    }
    if(C.resume.token!=s->token) {
        C.resume.token=s->token;
        C.resume.state=TOY_RESUME_IDLE;
    }
    if(C.resume.state==TOY_RESUME_STRICT)return false;
    if(C.resume.state==TOY_RESUME_RESTART) {
        toy_resume_retry(r);
        return true;
    }
    if(C.resume.state==TOY_RESUME_IDLE && (!selected || !toy_resume_begin(r,variant)))
        return false;

    /* The public cursor and guest write target remain intact at every
     * service boundary. Only the private, guarded low visit can take a
     * provisional block and its sole writer targets our arena. */
    const unsigned clip=(C.resume.variant-1u)/2u;
    if(selected && variant==C.resume.variant) {
        const struct kui_retail_cursor original=e->cursor;
        const uint8_t original_policy=stream->toy_suite_active;
        e->cursor=C.resume.cursor;
#if KUI_TOY_SUITE_RESUME_WAKE
        /* A verified controller reply is a separate wake, after the
         * SCI interrupt that parked the current block has returned. */
        const uint32_t parked_lba=stream->position;
        const bool wake_resume=e->in_irq==2u && stream->state==RA_RESUME &&
            parked_lba==e->cursor.block;
        const uint32_t resumed=stream->stats.toy_resumed_blocks;
#else
        const bool wake_resume=false;
#endif
        stream->toy_suite_active=original_policy|KUI_TOY_SUITE_RESUME_ACTIVE|
            (!e->in_irq || wake_resume?KUI_TOY_SUITE_RESUME_FOREGROUND:0u);
#if KUI_TOY_SUITE_RESUME_SLICE
        if(!e->in_irq && (stream->toy_suite_poll==1u || stream->toy_suite_poll==2u))
            toy_resume_slice(r,clip);
        else C.config.ops->deliver(wait);
#else
        C.config.ops->deliver(wait);
#endif
#if KUI_TOY_SUITE_RESUME_WAKE
        if(wake_resume && !e->failed &&
           ((stream->state==RA_DMA && stream->position==parked_lba) ||
            stream->stats.toy_resumed_blocks!=resumed))
            ++C.demand.movie[clip].wake_resumes;
#endif
        stream->toy_suite_active=original_policy;
        C.resume.cursor=e->cursor;
        e->cursor=original;
        if(!e->failed && e->active)return true;
        if(!e->failed && s->pending && s->token==C.resume.token && e->token==s->token &&
           C.resume.cursor.done==original.count && C.resume.checked==original.count &&
           C.resume.written==original.count*KUI_GAME_RAW_BYTES) {
            const uint8_t *raw=toy_resume_data();
#if KUI_TOY_SUITE_FAST_COPY
            if(!toy_resume_fast_publish(r,&original,raw))
#endif
            for(uint32_t sector=0u;sector<original.count && !e->failed;
                ++sector,raw+=KUI_GAME_RAW_BYTES)
                original.write(original.context,sector*KUI_GAME_DATA_BYTES,
                    raw+16u,KUI_GAME_DATA_BYTES);
            if(!e->failed) {
                e->cursor.done=original.count;
                e->cursor.block=C.resume.cursor.block;
                e->cursor.run=C.resume.cursor.run;
                C.suite[clip]+=stream->stats.toy_resumed_blocks-C.resume.resumed_start;
            }
            toy_resume_invalidate();
            return true;
        }
    }

    /* A failed raw candidate never contributes a cooked prefix. Retire
     * transport and buffers before the original strict cursor starts. */
    ++C.suite[2u+clip];
    C.resume.state=TOY_RESUME_RESTART;
    toy_resume_retry(r);
    return true;
}
