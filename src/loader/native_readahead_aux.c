/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional physical-card look-ahead. The normal reader remains the sole
 * owner of demand completion, guest destinations, retries and card adoption.
 * Reader policy runs through its separately guarded private-stack entry. */
#include "native_readahead_aux_internal.h"
#include "kui/vmu_native_background.h"
#include "kui/toy_pilot_clock.h"
#include <string.h>
#if KUI_TOY_SUITE_WIDE_EDC
#include "kui/hash.h"
#include "kui/retail_image.h"
#endif
#if KUI_TOY_SUITE_SLOW_RETRY || KUI_TOY_SUITE_VERIFIED_RESUME
#include "kui/retail_loader_layout.h"
#define RA_LOW_LIMIT KUI_RETAIL_LOW_ASYNC_LIMIT
#else
#define RA_LOW_LIMIT UINT32_C(0x8c007ba0)
#endif

#if !KUI_NATIVE_READAHEAD || defined(KUI_RETAIL_CE) || (KUI_SCI_STREAM_TOKEN_SLICED && !KUI_TOY_SUITE_ASYNC_DEMAND)
#error "Read-ahead auxiliary code requires its separate native unsliced SCI profile"
#endif
#if KUI_NATIVE_BACKGROUND_COMPACT && \
    (!KUI_NATIVE_SPLIT_CONTROLLER_ISOLATION || !KUI_NATIVE_SPLIT_CONTROLLER_GENERAL || \
     !KUI_VMU_NATIVE_CACHED_CODE || !KUI_VMU_NATIVE_PERSISTENT_PROBE)
#error "Compact Background auxiliary requires its complete persistent General reader profile"
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE && !KUI_NATIVE_TOY_MOVIE_CAPTURE
#error "Playback capture requires the stage's Toy movie ranges"
#endif
#if KUI_NATIVE_TOY_HEADER_REPLAY && !KUI_NATIVE_TOY_PLAYBACK_CAPTURE
#error "Header replay requires the Toy movie playback profile"
#endif
#if defined(KUI_TOY_SUITE_MODE) && (!KUI_NATIVE_TOY_PLAYBACK_CAPTURE || KUI_NATIVE_TOY_HEADER_REPLAY)
#error "Toy suite requires the playback profile with stock demand"
#endif

/* These are the existing unsliced sci_stream.c states. No new state or field
 * is inserted in the tightly bounded low reader region. */
enum { RA_CLOSED=0u,RA_PAUSED=1u,RA_DMA=KUI_SCI_STREAM_DMA_STATE,RA_LOST=3u,RA_RESUME=4u };
#define SPEC_TOKEN_LIMIT 256u
__asm__(".global __kui_native_readahead_diagnostic_auxiliary\n"
        ".set __kui_native_readahead_diagnostic_auxiliary,1\n"
        ".global __kui_native_readahead_repair_diagnostic_auxiliary\n"
        ".set __kui_native_readahead_repair_diagnostic_auxiliary,1\n"
        ".global __kui_native_readahead_pio_auxiliary\n"
        ".set __kui_native_readahead_pio_auxiliary,1\n"
        ".global __kui_native_readahead_pio_pair_auxiliary\n"
        ".set __kui_native_readahead_pio_pair_auxiliary,1\n"
        ".global __kui_native_readahead_tick_budget_auxiliary\n"
        ".set __kui_native_readahead_tick_budget_auxiliary,1\n"
        ".global __kui_native_readahead_max_context_auxiliary\n"
        ".set __kui_native_readahead_max_context_auxiliary,1\n");
#if KUI_NATIVE_GENERAL_LEAN_LAYOUT
__asm__(".global __kui_native_general_lean_auxiliary\n"
        ".set __kui_native_general_lean_auxiliary,1\n");
#endif
#if KUI_NATIVE_BACKGROUND_COMPACT
__asm__(".global __kui_native_background_compact_auxiliary\n"
        ".set __kui_native_background_compact_auxiliary,1\n"
        ".global __kui_native_isolation_lease_bytes\n"
        ".set __kui_native_isolation_lease_bytes,0x47000\n");
#endif
#define C kui_native_readahead_aux_context
struct kui_native_readahead_aux_context kui_native_readahead_aux_context
    __attribute__((section(".data.native_ra_context"),aligned(32)))={0};

#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
extern uint32_t kui_native_readahead_aux_test_read(uint32_t,unsigned);
#define read_hw kui_native_readahead_aux_test_read
#else
static uint32_t read_hw(uint32_t address,unsigned width) {
    if(width==1u)return *(volatile const uint8_t *)(uintptr_t)address;
    if(width==2u)return *(volatile const uint16_t *)(uintptr_t)address;
    return *(volatile const uint32_t *)(uintptr_t)address;
}
#endif
struct ra_clock {uint32_t counter,valid;};
static struct ra_clock clock_read(void) {
    uint32_t frqcr=read_hw(UINT32_C(0xffc00000),2u);
    uint32_t tcor=read_hw(UINT32_C(0xffd80008),4u);
    uint32_t tcr=read_hw(UINT32_C(0xffd80010),2u);
    uint32_t tstr=read_hw(UINT32_C(0xffd80004),1u);
    struct ra_clock result={read_hw(UINT32_C(0xffd8000c),4u),
        kui_toy_pilot_clock_profile(frqcr,tcor,tcr,tstr)?1u:0u};
    return result;
}
#if KUI_TOY_SUITE_FRAME_TIMING
#include "toy_movie_present_trace.inc"
#endif
#if !KUI_TOY_SUITE_ASYNC_DEMAND
static bool dma_idle(void) {
    return !(read_hw(UINT32_C(0xffa0001c),4u)&7u) &&
        (read_hw(UINT32_C(0xffa00040),4u)&7u)==1u;
}
#endif
static bool no_overlap(const void *a,size_t an,const void *b,size_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=UINTPTR_MAX-an && y<=UINTPTR_MAX-bn && (x+an<=y || y+bn<=x);
}
#ifndef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
static bool low_code(uintptr_t address) {
    return !(address&1u) && address>=UINT32_C(0x8c004028) &&
        address<RA_LOW_LIMIT;
}
#endif
static bool config_valid(const struct kui_native_ra_config *c) {
    if(!c || c->version!=KUI_NATIVE_RA_VERSION || c->bytes!=sizeof(*c) ||
       !c->region || !c->ops || !c->data || c->data_bytes!=KUI_NATIVE_RA_DATA_BYTES ||
       c->lease_bytes!=KUI_NATIVE_RA_LEASE_BYTES || ((uintptr_t)c->data&31u) ||
       !no_overlap(c,sizeof(*c),&C,sizeof(C)) ||
       !no_overlap(c->data,c->data_bytes,&C,sizeof(C)) ||
       !no_overlap(c->data,c->data_bytes,c->region,sizeof(*c->region)) ||
       !no_overlap(c->data,c->data_bytes,c->ops,sizeof(*c->ops)))return false;
#ifndef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    uint32_t area=c->lease&UINT32_C(0xe0000000),p=c->lease&UINT32_C(0x1fffffff);
    uintptr_t ops=(uintptr_t)c->ops;
    if((area && area!=UINT32_C(0x80000000) && area!=UINT32_C(0xa0000000)) ||
       (p&31u) || p<UINT32_C(0x0c010020) ||
       p>UINT32_C(0x0d000000)-KUI_NATIVE_RA_LEASE_BYTES ||
       (uintptr_t)&C<(p|UINT32_C(0xa0000000))+KUI_NATIVE_RA_AUX_OFFSET+64u ||
       (uintptr_t)&C>(p|UINT32_C(0xa0000000))+KUI_NATIVE_RA_AUX_OFFSET+4096u-sizeof(C) ||
       (uintptr_t)c->region!=(p|UINT32_C(0x80000000))+KUI_VMU_NATIVE_BACKGROUND_REGION_OFFSET ||
       (uintptr_t)c->data!=(p|UINT32_C(0xa0000000))+KUI_NATIVE_RA_DATA_OFFSET ||
       (ops&3u) || ops<UINT32_C(0x8c004028) ||
       ops>RA_LOW_LIMIT-sizeof(*c->ops))return false;
    uintptr_t cell=(uintptr_t)c->ops->region_storage;
    if(
       (cell&3u) || cell<UINT32_C(0x8c004000) || cell>RA_LOW_LIMIT-4u ||
       !low_code((uintptr_t)c->ops->deliver) || !low_code((uintptr_t)c->ops->hook) ||
       !low_code((uintptr_t)c->ops->unhook) || !low_code((uintptr_t)c->ops->fetch) ||
#if KUI_TOY_SUITE_MODE != 26 && KUI_TOY_SUITE_MODE != 27 && KUI_TOY_SUITE_MODE != 28
       !low_code((uintptr_t)c->ops->poll) || !low_code((uintptr_t)c->ops->take) ||
#elif KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
       !low_code((uintptr_t)c->ops->cold_bus) ||
#endif
       !low_code((uintptr_t)c->ops->stop) || !low_code((uintptr_t)c->ops->busy) ||
       !low_code((uintptr_t)c->ops->feed))return false;
#endif
    const struct kui_native_ra_low_ops *o=c->ops;
    return o->magic==KUI_NATIVE_RA_OPS_MAGIC && o->version==KUI_NATIVE_RA_VERSION &&
        o->bytes==sizeof(*o) && o->region_storage && *o->region_storage==c->region &&
        o->deliver && o->hook && o->unhook && o->fetch &&
#if KUI_TOY_SUITE_MODE != 26 && KUI_TOY_SUITE_MODE != 27 && KUI_TOY_SUITE_MODE != 28
        o->poll && o->take &&
#elif KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        o->cold_bus &&
#endif
        o->stop && o->busy && o->feed;
}
#if KUI_TOY_SUITE_WIDE_EDC
static struct kui_cd_edc_table *toy_edc_table(const struct kui_native_ra_config *config) {
    /* This demand profile has no speculative cache producer. Reserve the
     * table after the largest staged raw request in the same owned arena. */
    _Static_assert(KUI_RETAIL_IMAGE_MAX_SECTORS*KUI_GAME_RAW_BYTES <=
        KUI_NATIVE_RA_DATA_BYTES-sizeof(struct kui_cd_edc_table),
        "Raw staging and the EDC table must be disjoint");
    _Static_assert((KUI_NATIVE_RA_DATA_BYTES-sizeof(struct kui_cd_edc_table))%32u==0u,
        "The EDC table has private cache lines");
    _Static_assert(KUI_GAME_RAW_BYTES%4u==0u,
        "Every staged raw sector is word aligned");
    uintptr_t data=(uintptr_t)config->data;
#ifndef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    data&=~(uintptr_t)UINT32_C(0x20000000);
#endif
    return (void *)(data+KUI_NATIVE_RA_DATA_BYTES-sizeof(struct kui_cd_edc_table));
}
#endif
#if KUI_TOY_WIRE_STREAM
static void toy_wire_reset(bool all);
#endif
int kui_native_readahead_aux_init(const struct kui_native_ra_config *c) {
    if(C.busy || C.initialized || !config_valid(c))return -1;
    struct kui_native_ra_cache cache;
    if(!kui_native_ra_cache_init(&cache,c->data,KUI_NATIVE_RA_CACHE_BYTES))return -1;
#if defined(KUI_TOY_SUITE_MODE)
    const struct kui_toy_suite_seed *seed=&c->suite_seed;
    uint32_t max_seed=KUI_NATIVE_RA_DATA_BYTES/(KUI_TOY_SUITE_MODE==1?KUI_GAME_DATA_BYTES:512u);
    if(c->suite_source>2u || seed->count[0]>max_seed || seed->count[1]>max_seed-seed->count[0] ||
       (seed->count[1] && !seed->count[0]) ||
       seed->base[0]>UINT32_MAX-seed->count[0] || seed->base[1]>UINT32_MAX-seed->count[1])return -1;
#endif
    memset(&C,0,sizeof(C));
    C.config=*c;C.cache=cache;C.initialized=1u;
#if KUI_TOY_SUITE_WIDE_EDC
    if(c->toy_poll_owner)kui_cd_edc_table_init(toy_edc_table(c));
#endif
#if KUI_TOY_SUITE_MOVIE_POLL
    C.demand.owner_verified=c->toy_poll_owner;
#endif
#if KUI_TOY_SUITE_MODE == 35
    if(c->toy_poll_owner) C.demand.window.seeded_blocks=seed->count[0]+seed->count[1];
#endif
#if defined(KUI_TOY_SUITE_MODE)
    C.suite_source=c->suite_source;
#if KUI_TOY_SUITE_MODE == 1 || KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
    C.suite[0]=seed->count[0]+seed->count[1];
#endif
#endif
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    for(unsigned i=0;i<4u;i++)
        if(c->movie_ranges[i].count)C.movie_mask|=1u<<i;
#endif
#if KUI_TOY_WIRE_STREAM
    toy_wire_reset(true);
#endif
    return 0;
}
#if KUI_TOY_SUITE_MODE != 26 && KUI_TOY_SUITE_MODE != 27 && KUI_TOY_SUITE_MODE != 28 && !KUI_TOY_SUITE_ASYNC_DEMAND
static void snapshot_hw(struct kui_native_ra_hw_snapshot *s) {
    /* Sequential, read-only observations: DMA can advance between reads.
     * Never read RDR, fence/purge memory, or change transport registers. */
    s->sar=read_hw(UINT32_C(0xffa00010),4u);
    s->dar=read_hw(UINT32_C(0xffa00014),4u);
    s->tcr=read_hw(UINT32_C(0xffa00018),4u);
    s->chcr=read_hw(UINT32_C(0xffa0001c),4u);
    s->dmaor=read_hw(UINT32_C(0xffa00040),4u);
    s->sci=read_hw(UINT32_C(0xffe00000),1u) |
        read_hw(UINT32_C(0xffe00004),1u)<<8 |
        read_hw(UINT32_C(0xffe00008),1u)<<16 |
        read_hw(UINT32_C(0xffe00010),1u)<<24;
    s->pins=read_hw(UINT32_C(0xffe0001c),1u) |
        read_hw(UINT32_C(0xffc00004),1u)<<8 |
        read_hw(UINT32_C(0xff800030),2u)<<16;
    s->tmu=read_hw(UINT32_C(0xffd8000c),4u);
}
static void snapshot_pre(enum kui_native_ra_failure_stage stage,
    const kui_retail_async_region_type *r,uint32_t operation_lba) {
    struct kui_native_ra_failure_snapshot *s=&C.observation;
    const struct kui_retail_async *e=&r->engine;
    const struct kui_sci_stream_state *stream=&r->shared.stream;
    s->stage=stage;s->event=C.current_event;
    s->flags=(e->active?KUI_NATIVE_RA_SNAP_ACTIVE:0u) |
        (r->shared.service.pending?KUI_NATIVE_RA_SNAP_PENDING:0u) |
        (r->shared.service.executing?KUI_NATIVE_RA_SNAP_EXECUTING:0u) |
        (e->failed?KUI_NATIVE_RA_SNAP_FAILED:0u) |
        (C.spec_owned?KUI_NATIVE_RA_SNAP_SPEC_OWNED:0u) |
        (C.deferred?KUI_NATIVE_RA_SNAP_DEFERRED:0u) |
        (C.quiesced?KUI_NATIVE_RA_SNAP_QUIESCED:0u) |
        (C.abandoned?KUI_NATIVE_RA_SNAP_ABANDONED:0u) |
        (C.have_seed?KUI_NATIVE_RA_SNAP_HAVE_SEED:0u) |
        (r->shared.port.acquired?KUI_NATIVE_RA_SNAP_ACQUIRED:0u) |
        (r->shared.port.fault?KUI_NATIVE_RA_SNAP_PORT_FAULT:0u) |
        (e->opened?KUI_NATIVE_RA_SNAP_OPENED:0u) |
        (e->in_irq?KUI_NATIVE_RA_SNAP_IRQ:0u);
    s->token=e->token;s->cursor_block=e->cursor.block;
    s->stream_state=stream->state;s->stream_position=stream->position;
    s->cache_base=C.cache.base;s->cache_count=C.cache.count;s->cache_end=C.cache.end;
    s->operation_lba=operation_lba;
    s->loan_state=(e->native_loan_state&UINT32_C(0xffff)) |
        (uint32_t)stream->unrepaired<<16;
    s->rx_flags=(uint32_t)stream->ready[0] | (uint32_t)stream->ready[1]<<8 |
        (uint32_t)stream->kept<<16 | stream->fill<<24;
    s->ready_lba[0]=stream->ready_lba[0];s->ready_lba[1]=stream->ready_lba[1];
    s->kept_lba=stream->kept_lba;s->arrived=stream->arrived;
    s->lost=(uint32_t)stream->lost[0] | (uint32_t)stream->lost[1]<<16;
    snapshot_hw(&s->pre);
}
static void snapshot_failure(const kui_retail_async_region_type *r,
    enum kui_sci_stream_result result) {
    struct kui_native_ra_failure_snapshot *s=&C.observation;
    s->result=result;
    s->post_stream_state=r->shared.stream.state;
    s->post_stream_position=r->shared.stream.position;
    snapshot_hw(&s->post);
    if(s->stage<KUI_NATIVE_RA_STAGE_COUNT && (uint32_t)result<KUI_NATIVE_RA_RESULT_COUNT)
        ++C.diagnostics.stage_result[s->stage][result];
    /* Word copies avoid an implicit large structure-copy runtime import. */
    const uint32_t *from=(const uint32_t *)(const void *)s;
    uint32_t *last=(uint32_t *)(void *)&C.diagnostics.last;
    uint32_t *first=(uint32_t *)(void *)&C.diagnostics.first;
    for(unsigned i=0u;i<sizeof(*s)/sizeof(uint32_t);++i) {
        last[i]=from[i];
        if(!C.diagnostics.failures)first[i]=from[i];
    }
    ++C.diagnostics.failures;
}
#endif
static bool skipped(enum kui_native_ra_skip reason) {
    ++C.diagnostics.skipped[reason];return true;
}
#if KUI_TOY_SUITE_MODE != 26 && KUI_TOY_SUITE_MODE != 27 && KUI_TOY_SUITE_MODE != 28 && !KUI_TOY_SUITE_ASYNC_DEMAND
static void abandon(kui_retail_async_region_type *r,enum kui_sci_stream_result result) {
    snapshot_failure(r,result);
    if(result==KUI_SCI_STREAM_CRC)++C.stats.crc_errors;
    if(result==KUI_SCI_STREAM_TOKEN)++C.stats.token_errors;
    ++C.stats.abandoned;C.abandoned=1u;C.spec_owned=C.deferred=0u;
    /* poll/take/fetch have already stopped an owned reception. No optional
     * error may invoke CMD12's long busy wait from inside an interrupt. */
    if(!C.config.ops->busy())r->shared.stream.state=RA_LOST;
    C.config.ops->unhook();
}
/* Retire at most one speculative block. A mid-block repair remains stopped
 * in RESUME until a later foreground/heartbeat visit, never the same IRQ. */
static void retire_spec(kui_retail_async_region_type *r,bool interrupt) {
    const struct kui_native_ra_low_ops *o=C.config.ops;
    if(!C.spec_owned || C.deferred)return;
    enum kui_sci_stream_result result=KUI_SCI_STREAM_OK;
    if(o->busy()) {snapshot_pre(KUI_NATIVE_RA_STAGE_POLL,r,C.spec_lba);result=o->poll(interrupt);}
    if(result==KUI_SCI_STREAM_PENDING) {
        if(!o->busy() && r->shared.stream.state==RA_RESUME) {
            C.deferred=1u;o->unhook();
        }
        return;
    }
    if(result!=KUI_SCI_STREAM_OK) {abandon(r,result);return;}
    if(o->busy())return;
    snapshot_pre(KUI_NATIVE_RA_STAGE_TAKE,r,C.spec_lba);
    const uint8_t *block=o->take(C.spec_lba,&result);
    if(!block || result!=KUI_SCI_STREAM_OK) {abandon(r,result);return;}
    snapshot_pre(KUI_NATIVE_RA_STAGE_APPEND,r,C.spec_lba);
    if(!kui_native_ra_cache_append(&C.cache,C.spec_lba,block)) {
        abandon(r,KUI_SCI_STREAM_BUSY);return;
    }
    C.spec_owned=0u;++C.stats.producer_blocks;
}
static bool producer_ready(kui_retail_async_region_type *r,bool interrupt,
    uint32_t *next,bool first) {
    const struct kui_native_ra_low_ops *o=C.config.ops;
    /* Optional CPU-clocked work belongs only to a later controller
     * heartbeat, never to GD entry or the SCI handler. */
    if(C.current_event!=KUI_NATIVE_RA_TICK || interrupt || r->engine.in_irq)return false;
    if(C.quiesced) {skipped(KUI_NATIVE_RA_SKIP_QUIESCED);return false;}
    if(C.abandoned) {skipped(KUI_NATIVE_RA_SKIP_ABANDONED);return false;}
    if(!C.have_seed) {skipped(KUI_NATIVE_RA_SKIP_NO_SEED);return false;}
    if(r->engine.native_loan_state!=KUI_VMU_NATIVE_ASYNC_LOAN_IDLE) {
        skipped(KUI_NATIVE_RA_SKIP_LOAN);return false;
    }
    if(first)++C.stats.producer_visits;
    if(r->engine.active) {skipped(KUI_NATIVE_RA_SKIP_ACTIVE);return false;}
    if(r->shared.service.pending || r->shared.service.executing) {
        skipped(KUI_NATIVE_RA_SKIP_PENDING);return false;
    }
    if(r->engine.failed) {skipped(KUI_NATIVE_RA_SKIP_FAILED);return false;}
    if(!r->engine.opened || !r->shared.port.acquired) {
        skipped(KUI_NATIVE_RA_SKIP_NOT_OPENED);return false;
    }
    if(r->shared.port.fault) {skipped(KUI_NATIVE_RA_SKIP_PORT_FAULT);return false;}
    if(o->busy()) {skipped(KUI_NATIVE_RA_SKIP_BUSY);return false;}
    if(C.deferred) {
        /* fetch(polled=true) still uses DMA in RESUME. Only the ordinary
         * demand path may adopt a stopped partial reception. */
        skipped(KUI_NATIVE_RA_SKIP_DEFERRED);return false;
    }
    if(C.spec_owned) {skipped(KUI_NATIVE_RA_SKIP_SPEC_OWNED);return false;}
    if(!kui_native_ra_cache_next(&C.cache,next)) {
        if(C.cache.count==KUI_NATIVE_RA_CACHE_BLOCKS)++C.stats.full;
        else ++C.stats.extent_ends;
        o->unhook();return false;
    }
    /* Continue a fresh block in the already open stream. Do not enter the
     * RESUME branch, seek, issue a command, or borrow a guest DMA channel. */
    if(r->shared.stream.state!=RA_PAUSED || r->shared.stream.position!=*next) {
        skipped(KUI_NATIVE_RA_SKIP_STREAM_POSITION);return false;
    }
    if(!dma_idle()) {skipped(KUI_NATIVE_RA_SKIP_GUEST_DMA);return false;}
    return true;
}
static void producer(kui_retail_async_region_type *r,bool interrupt,unsigned block_limit) {
    uint32_t next;
    if(!producer_ready(r,interrupt,&next,true))return;
    const struct kui_native_ra_low_ops *o=C.config.ops;
    uint32_t *metrics=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
    struct ra_clock before=clock_read(),block_before=before;
    bool valid=true;
    for(unsigned block=0u;block<block_limit;++block) {
        if(block) {
            /* Recheck every ordinary admission guard before deciding
             * whether another complete block fits the soft window. */
            if(!producer_ready(r,interrupt,&next,false))break;
            struct ra_clock admission=clock_read();
            uint32_t elapsed=before.counter-admission.counter;
            if(!valid || !before.valid || !admission.valid ||
               !elapsed || elapsed>=UINT32_C(0x80000000) || elapsed>KUI_NATIVE_RA_PIO_SECOND_LIMIT_TICKS) {
                ++metrics[KUI_NATIVE_RA_PIO_SECOND_TIME_SKIPS];
                valid=valid && admission.valid && elapsed && elapsed<UINT32_C(0x80000000);
                break;
            }
            block_before=admission;
            ++metrics[KUI_NATIVE_RA_PIO_SECOND_ATTEMPTS];
        }
        ++metrics[KUI_NATIVE_RA_PIO_ATTEMPTS];
        snapshot_pre(KUI_NATIVE_RA_STAGE_FETCH,r,next);
        enum kui_sci_stream_result result=o->fetch(next,SPEC_TOKEN_LIMIT,true);
        if(result!=KUI_SCI_STREAM_OK)abandon(r,result);
        else {
            C.spec_owned=1u;C.spec_lba=next;
            /* Finish this physical block, including CRC and publication,
             * before considering another; never stop a partial block. */
            retire_spec(r,false);
        }
        struct ra_clock after=clock_read();
        uint32_t elapsed=block_before.counter-after.counter;
        if(!block_before.valid || !after.valid || !elapsed || elapsed>=UINT32_C(0x80000000)) {
            ++metrics[KUI_NATIVE_RA_PIO_CLOCK_BAD];valid=false;
        } else if(elapsed>metrics[KUI_NATIVE_RA_PIO_MAX_SINGLE_TICKS])
            metrics[KUI_NATIVE_RA_PIO_MAX_SINGLE_TICKS]=elapsed;
        if(C.abandoned)break;
    }
    /* This is an observed complete-work duration, not a hard deadline:
     * an admitted block may take longer than its reserved allowance. */
    struct ra_clock after=clock_read();uint32_t elapsed=before.counter-after.counter;
    if(valid && before.valid && after.valid && elapsed && elapsed<UINT32_C(0x80000000)) {
        if(elapsed>metrics[KUI_NATIVE_RA_PIO_MAX_TICKS])metrics[KUI_NATIVE_RA_PIO_MAX_TICKS]=elapsed;
        if(elapsed>KUI_NATIVE_RA_PIO_WINDOW_TICKS)++metrics[KUI_NATIVE_RA_PIO_WINDOW_OVERRUNS];
    }
}
static void seed_completed(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    if(e->active || e->failed || !e->cursor.count || e->cursor.done!=e->cursor.count ||
       (C.have_completed && C.completed_token==e->token))return;
    C.have_completed=1u;C.completed_token=e->token;
    uint32_t block=e->cursor.block,end=kui_native_ra_extent_end(e->manifest,block);
    if(!end) {C.have_seed=0u;return;}
    if(!kui_native_ra_cache_lookup(&C.cache,block)) {
        enum kui_sci_stream_result result=KUI_SCI_STREAM_PENDING;
        const uint8_t *checked=C.config.ops->take(block,&result);
        if(!checked || result!=KUI_SCI_STREAM_OK ||
           !kui_native_ra_cache_rebase(&C.cache,block,end) ||
           !kui_native_ra_cache_append(&C.cache,block,checked)) {
            C.have_seed=0u;return;
        }
    }
    C.have_seed=1u;C.abandoned=0u;
}
#endif
#if !KUI_TOY_SUITE_ASYNC_DEMAND
static void demand_complete(kui_retail_async_region_type *r) {
    struct kui_retail_async *e=&r->engine;
    if(e->cursor.done!=e->cursor.count)return;
    e->active=0u;
#if KUI_NATIVE_REQUEST_TIMING
    uint32_t elapsed=e->stats.hooks-read_hw(UINT32_C(0xffd8000c),4u);
    if(elapsed>e->stats.releases) {
        e->stats.releases=elapsed;e->native_max_lba=r->shared.service.lba;
        e->native_max_count=r->shared.service.count;
    }
#endif
}
#endif
#if KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 3 || KUI_TOY_SUITE_MODE == 4 || \
    KUI_TOY_SUITE_MODE == 6 || KUI_TOY_SUITE_MODE == 7 || KUI_TOY_SUITE_MODE == 26 || \
    KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28 || KUI_TOY_SUITE_MODE == 31 || KUI_TOY_SUITE_ASYNC_DEMAND
static unsigned movie_variant(const struct kui_retail_gd *s);
static unsigned suite_movie(const kui_retail_async_region_type *r) {
    const struct kui_retail_async *e=&r->engine;
    const struct kui_retail_gd *s=&r->shared.service;
    if(C.suite_source!=1u || !e->active || e->failed || !s->pending || e->token!=s->token ||
       s->sector_bytes!=KUI_GAME_DATA_BYTES || !s->count)return 0u;
    unsigned variant=movie_variant(s);
    if(!variant)return 0u;
    const struct kui_toy_movie_range *movie=&C.config.movie_ranges[variant-1u];
#if KUI_TOY_SUITE_MODE >= 26
    if(!e->cursor.manifest || e->cursor.track>=e->cursor.manifest->track_count ||
       e->cursor.track>=KUI_RETAIL_MANIFEST_SLOTS || e->cursor.raw)return 0u;
#endif
    const struct kui_retail_track *track=&e->cursor.manifest->slots[e->cursor.track].track;
    if(s->lba<movie->lba || s->lba-movie->lba>=movie->count ||
       s->count>movie->count-(s->lba-movie->lba) ||
       kui_retail_track_sector_bytes(track)!=KUI_GAME_RAW_BYTES ||
       kui_retail_track_header_bytes(track)!=16u)return 0u;
    return variant;
}
#endif
#if KUI_TOY_SUITE_ASYNC_DEMAND
#if KUI_TOY_SUITE_REFILL_SLICE
enum { TOY_REFILL_TICKS=1172u, TOY_REFILL_BLOCKS=3u, TOY_REFILL_EXTRA_LIMIT=256u };

static inline bool toy_refill_time_left(struct ra_clock start) {
    const struct ra_clock now=clock_read();
    return now.valid && start.counter-now.counter<TOY_REFILL_TICKS;
}
#endif
#if KUI_TOY_SUITE_MODE == 35
#include "toy_ram_window.inc"
#endif
#if KUI_TOY_SUITE_VERIFIED_RESUME
#include "toy_resume_verify.inc"
#endif
#if KUI_TOY_SUITE_MODE >= 62
#include "toy_movie_ahead.inc"
extern uint32_t kui_native_readahead_aux_visit(uint32_t,uint32_t,
    kui_retail_async_region_type *,uint32_t);

void kui_toy_movie_ahead_update(uint32_t ctx) {
    if(C.initialized && toy_present.owner==ctx)
        (void)kui_native_readahead_aux_visit(KUI_NATIVE_RA_MOVIE_AHEAD,ctx,C.config.region,0u);
}
#endif
#if KUI_TOY_SUITE_SEQUENTIAL_PIO
static bool toy_sequential_pio_step(kui_retail_async_region_type *r,bool selected) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    const struct kui_retail_cursor *cursor=&r->engine.cursor;
    /* Reuse the low reader's checked window only at an already open card
     * boundary. A seek, incomplete block or changed owner stays on the
     * established fetch/retry path. */
    const bool offered=selected && stream->state==RA_PAUSED && !stream->toy_suite_slow_pending &&
       (stream->position==cursor->block ||
        (cursor->run>1u && stream->position-cursor->block==1u));
    const uint32_t reads=stream->stats.toy_pio_window_blocks;
    if(offered)stream->toy_suite_slow_pending=KUI_TOY_SCI_PIO_WINDOW_FIRST;
    C.config.ops->deliver(0u);
    if(offered && stream->toy_suite_slow_pending>=KUI_TOY_SCI_PIO_WINDOW_FIRST)
        stream->toy_suite_slow_pending=0u;
    return offered && stream->stats.toy_pio_window_blocks!=reads;
}
#endif
static void suite_demand(uint32_t wait,kui_retail_async_region_type *r) {
    unsigned variant=suite_movie(r);
#if KUI_TOY_SUITE_MODE == 35
    const bool first_visit=!r->shared.stream.toy_suite_active;
#endif
    /* Keep the original opening fill and nonmovie reads on the full
     * demand path. Retain the decision for the separate controller wake. */
    if(r->engine.active && !r->engine.failed) {
        unsigned active=1u;
#if KUI_TOY_SUITE_SLOW_RETRY
        if(C.config.toy_poll_owner)active|=KUI_TOY_SUITE_SLOW_MOVIE;
#endif
        r->shared.stream.toy_suite_active=variant &&
            r->shared.service.lba-C.config.movie_ranges[variant-1u].lba>=386u?active:0u;
    }
#if KUI_TOY_SUITE_MODE == 35
    if(first_visit && r->shared.stream.toy_suite_active && toy_window_deliver(r,variant))return;
#endif
#if KUI_TOY_SUITE_AFTER_START
    bool in_play=false;
    if(variant && r->engine.active && !r->engine.failed &&
       (KUI_TOY_SUITE_FIRST_CUT_PIO || KUI_TOY_SUITE_VERIFIED_RESUME || !r->engine.in_irq) &&
       (KUI_TOY_SUITE_VERIFIED_RESUME?
        C.config.toy_poll_owner && r->shared.stream.toy_suite_active:
        (r->shared.stream.toy_suite_active&KUI_TOY_SUITE_SLOW_MOVIE))) {
        const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[(variant-1u)/2u];
        in_play=(w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))==
            KUI_TOY_MOVIE_EXIT_STARTED && w->start_status==0x07dbu;
    }
#endif
#if KUI_TOY_SUITE_VERIFIED_RESUME
#if KUI_TOY_SUITE_MODE >= 62
    if(toy_movie_ahead(r,variant,in_play,wait))return;
#endif
    if(toy_resume_deliver(wait,r,variant,in_play))return;
#endif
#if KUI_TOY_SUITE_FIRST_CUT_PIO
    if(in_play)r->shared.stream.toy_suite_active|=KUI_TOY_SUITE_FIRST_CUT_ACTIVE;
#endif
#if KUI_TOY_SUITE_BOUNDED_DMA
    if(in_play && r->shared.stream.toy_suite_poll==1u && r->engine.target) {
        const uint32_t target=r->engine.target;
        const uint32_t before=r->engine.stats.call_blocks;
        /* Lend the scheduled foreground caller the established full
         * receive/wait step, with its native foreground block cap lowered
         * for this visit. IRQs and one-pass polling keep their old path. */
        r->engine.target=target<6u?target:6u;
        r->shared.stream.toy_suite_poll=0u;
        C.config.ops->deliver(r->engine.target);
        r->shared.stream.toy_suite_poll=1u;
        r->engine.target=target;
        const unsigned clip=(variant-1u)/2u;
        /* This test's suite slots record visits and deliveries from the
         * capped wait alone, separate from the ordinary background slice. */
        ++C.suite[2u*clip];
        C.suite[2u*clip+1u]+=r->engine.stats.call_blocks-before;
        return;
    }
#endif
#if KUI_TOY_SUITE_REFILL_SLICE
    const bool direct_pio=
#if KUI_TOY_SUITE_SEQUENTIAL_PIO
        in_play;
#else
        KUI_TOY_SUITE_DIRECT_PIO &&
        (r->shared.stream.toy_suite_active&KUI_TOY_SUITE_SLOW_MOVIE);
#endif
    /* Borrow a bounded interval from the scheduled EXEC. Direct PIO has
     * no SCI wake, so also use the decoder's recognized one-pass EXEC.
     * Neither caller enters the resident's synchronous wait. */
    if(variant && r->engine.active && !r->engine.failed &&
       r->shared.stream.toy_suite_active && !r->engine.in_irq &&
       (r->shared.stream.toy_suite_poll==1u ||
        (direct_pio && r->shared.stream.toy_suite_poll==2u))) {
        const struct ra_clock start=clock_read();
        const uint32_t blocks=r->engine.stats.call_blocks;
        const uint32_t polled=r->shared.stream.stats.polled;
        /* Movie PIO leaves no SCI interrupt to advance a parked block.
         * The same scheduled slice can therefore step up to three PIO
         * blocks, including a buffered block's completion. Other traffic
         * retains the ordinary one-PIO allowance. */
        const uint32_t pio_limit=direct_pio?3u:1u;
#if KUI_TOY_SUITE_SEQUENTIAL_PIO
        const unsigned clip=(variant-1u)/2u;
        const bool first_pio=toy_sequential_pio_step(r,direct_pio && start.valid);
        if(!KUI_TOY_SUITE_DMA_ROUND_ROBIN) C.suite[clip]+=first_pio;
#else
        C.config.ops->deliver(0u);
#endif
        if(!start.valid)return;
        const uint32_t first=r->engine.stats.call_blocks;
        unsigned extra=0u;
        while(r->engine.active && !r->engine.failed &&
              r->engine.stats.call_blocks-blocks<TOY_REFILL_BLOCKS &&
              r->shared.stream.stats.polled-polled<pio_limit && extra<TOY_REFILL_EXTRA_LIMIT) {
            if(!toy_refill_time_left(start))break;
            ++extra;
#if KUI_TOY_SUITE_SEQUENTIAL_PIO
            const bool extra_pio=toy_sequential_pio_step(r,direct_pio);
            if(!KUI_TOY_SUITE_DMA_ROUND_ROBIN) C.suite[clip]+=extra_pio;
#else
            C.config.ops->deliver(0u);
#endif
        }
        if(extra && !KUI_TOY_SUITE_RESUME_SLICE) {
            struct kui_toy_demand_movie_trace *movie=&C.demand.movie[(variant-1u)/2u];
            ++movie->slice_visits;
            movie->slice_blocks+=r->engine.stats.call_blocks-first;
        }
        return;
    }
#endif
    C.config.ops->deliver(wait);
}
#elif KUI_TOY_SUITE_MODE == 31
static void suite_demand(uint32_t wait,kui_retail_async_region_type *r) {
    unsigned variant=suite_movie(r);
    if(variant && r->shared.service.lba-C.config.movie_ranges[variant-1u].lba<386u)
        variant=0u;
    struct kui_sci_stream_state *stream=&r->shared.stream;
    uint8_t before=stream->toy_suite_retries;
    stream->toy_suite_active=variant!=0u;
    C.config.ops->deliver(wait);
    stream->toy_suite_active=0u;
    if(variant) C.suite[(variant-1u)/2u]+=(uint8_t)(stream->toy_suite_retries-before);
}
#elif KUI_TOY_SUITE_MODE == 3 || KUI_TOY_SUITE_MODE == 4 || KUI_TOY_SUITE_MODE == 7
static void suite_demand(uint32_t wait,kui_retail_async_region_type *r) {
    unsigned variant=suite_movie(r);
#if KUI_TOY_SUITE_MODE == 3 || KUI_TOY_SUITE_MODE == 4
    if(r->engine.in_irq)variant=0u;
#endif
#if KUI_TOY_SUITE_MODE == 4
    if(variant && !clock_read().valid)variant=0u;
#endif
    uint16_t before=r->shared.stream.toy_suite_events;
    r->shared.stream.toy_suite_active=variant!=0u;
    C.config.ops->deliver(wait);
    r->shared.stream.toy_suite_active=0u;
    if(variant)C.suite[(variant-1u)/2u]+=(uint16_t)(r->shared.stream.toy_suite_events-before);
}
#else
static inline void suite_demand(uint32_t wait,kui_retail_async_region_type *r) {
    (void)r;
    C.config.ops->deliver(wait);
}
#endif

#if KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
static const uint8_t *suite_window_block(uint32_t lba) {
    const struct kui_toy_suite_seed *seed=&C.config.suite_seed;
    uint32_t first=lba-seed->base[0];
    if(first<seed->count[0])return C.config.data+first*512u;
    uint32_t second=lba-seed->base[1];
    return second<seed->count[1]?C.config.data+(seed->count[0]+second)*512u:NULL;
}
static void suite_window_deliver(uint32_t wait,kui_retail_async_region_type *r) {
    const struct kui_native_ra_low_ops *o=C.config.ops;
    struct kui_retail_async *e=&r->engine;
    if(suite_movie(r)!=4u) {
        o->deliver(wait);return;
    }
    uint32_t center=KUI_TOY_SUITE_MODE==2?469080u:468128u;
    unsigned limit=e->in_irq?KUI_RETAIL_NATIVE_IRQ_BATCH_LIMIT:e->target;
    if(limit>25u)limit=25u;
    bool idle=!o->busy();
    for(unsigned i=0u;i<limit;i++) {
        uint32_t sector=e->cursor.lba+e->cursor.done;
        if(sector<center-40u || sector>=center+40u) {
            if(!i)o->deliver(wait);
            return;
        }
        const uint8_t *block=idle?suite_window_block(e->cursor.block):NULL;
        if(!block) {
            if(!i) {++C.suite[4];o->deliver(wait);}
            return;
        }
        e->retries=0u;
        if(o->feed(&e->cursor,block)!=KUI_GAME_OK) {
            e->failed=KUI_GD_ERROR_IO;return;
        }
        if(!C.suite[1]++)C.suite[2]=sector;
        C.suite[3]=sector;
        ++e->since;
        if(e->in_irq)++e->stats.irq_blocks;else ++e->stats.call_blocks;
        demand_complete(r);
        if(!e->active)return;
    }
}
#endif
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
#ifdef KUI_TOY_BURST_PRODUCER_TEST
extern enum kui_sci_stream_result toy_sci_burst_start(kui_retail_async_region_type *,
    uint32_t, unsigned, uint8_t *);
#if KUI_TOY_SUITE_MODE == 28
extern enum kui_sci_stream_result toy_sci_burst_start_rate(kui_retail_async_region_type *,
    uint32_t, unsigned, uint8_t *, bool);
#endif
extern enum kui_sci_stream_result toy_sci_burst_poll(kui_retail_async_region_type *,
    const uint8_t *, unsigned, uint32_t *);
extern enum kui_sci_stream_result toy_sci_burst_stop(kui_retail_async_region_type *,
    const uint8_t *, unsigned);
#else
#include "toy_sci_burst.inc"
#endif
#include "toy_suite_burst.inc"
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
#include "toy_suite_quiet_burst.inc"
#endif
#elif !KUI_TOY_SUITE_ASYNC_DEMAND
static void deliver(uint32_t wait,kui_retail_async_region_type *r) {
    const struct kui_native_ra_low_ops *o=C.config.ops;
    struct kui_retail_async *e=&r->engine;
    if(e->in_irq)++C.stats.irq_visits;
    retire_spec(r,e->in_irq!=0u);
    if(!e->active) {
        if(!C.spec_owned)suite_demand(wait,r);
        seed_completed(r);producer(r,e->in_irq!=0u,KUI_NATIVE_RA_PIO_BLOCK_LIMIT);return;
    }
    if(C.quiesced) {C.quiesced=0u;++C.stats.resumes;}
    uint32_t delivered=0u,limit=e->in_irq?KUI_RETAIL_NATIVE_IRQ_BATCH_LIMIT:e->target;
    if(limit>25u)limit=25u;
    while(delivered<limit && e->active && !e->failed) {
        const uint8_t *block=kui_native_ra_cache_lookup(&C.cache,e->cursor.block);
        if(!block)break;
        e->retries=0u;
        if(o->feed(&e->cursor,block)!=KUI_GAME_OK)e->failed=KUI_GD_ERROR_IO;
        ++delivered;++e->since;++C.stats.hits;
        if(e->in_irq)++e->stats.irq_blocks;else ++e->stats.call_blocks;
        if(!C.have_hit || C.hit_token!=e->token) {
            C.have_hit=1u;C.hit_token=e->token;++C.stats.hit_requests;
        }
        demand_complete(r);
    }
    /* Preserve the complete per-visit cap. After partial hits, a miss is
     * supplied by the next ordinary call rather than adding a fresh20/25. */
    if(!delivered && e->active && !e->failed) {
        ++C.stats.misses;
        if(C.spec_owned) {
            if(e->cursor.block==C.spec_lba) {
                /* Its received block is exactly the caller's next demand. */
                C.spec_owned=C.deferred=0u;
            } else {
                if(e->in_irq)return;
                ++C.stats.seeks;
                (void)o->stop();C.spec_owned=C.deferred=0u;o->unhook();
            }
        }
        suite_demand(wait,r);
    }
    seed_completed(r);producer(r,e->in_irq!=0u,KUI_NATIVE_RA_PIO_BLOCK_LIMIT);
}
#endif
static uint32_t quiesce(kui_retail_async_region_type *r) {
    if(r->engine.active || r->engine.in_irq || r->shared.service.pending ||
       r->shared.service.executing)return 0u;
    C.quiesced=1u;
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    return burst_release(r) && !C.config.ops->busy();
#elif KUI_TOY_SUITE_ASYNC_DEMAND
#if KUI_TOY_SUITE_MODE >= 62
    if(!toy_ahead_clear(r))return 0u;
#endif
    return !C.config.ops->busy();
#else
    ++C.stats.quiesces;
    if(C.spec_owned) {
        snapshot_pre(KUI_NATIVE_RA_STAGE_QUIESCE,r,C.spec_lba);
        enum kui_sci_stream_result result=C.config.ops->stop();
        bool failed=result!=KUI_SCI_STREAM_OK || C.config.ops->busy();
        if(failed)snapshot_failure(r,result);
        C.spec_owned=C.deferred=0u;C.config.ops->unhook();
        if(failed) {
            C.abandoned=1u;++C.stats.abandoned;return 0u;
        }
    }
    return C.config.ops->busy()?0u:1u;
#endif
}
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
/* Return the matched variant plus one; zero means no matching movie. */
static unsigned movie_variant(const struct kui_retail_gd *s) {
    if(!C.movie_mask || (s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD))return 0u;
    for(unsigned i=0;i<4u;i++) {
        const struct kui_toy_movie_range *range=&C.config.movie_ranges[i];
        if(range->count && s->lba<range->lba+range->count &&
           range->lba<s->lba+s->count)return i+1u;
    }
    return 0u;
}
#if KUI_TOY_SUITE_MOVIE_CHUNK
static uint32_t toy_chunk_word(uint32_t address) {
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    extern uint32_t kui_toy_movie_chunk_test_word(uint32_t);
    return kui_toy_movie_chunk_test_word(address);
#else
    return *(const volatile uint32_t *)(uintptr_t)address;
#endif
}

#define TOY_NAME_WORD(a,b,c,d) ((uint32_t)(a) | (uint32_t)(b)<<8 | (uint32_t)(c)<<16 | (uint32_t)(d)<<24)
/* Same exact game context and filenames as the movie exit witness. */
static unsigned toy_chunk_variant(uint32_t name) {
    unsigned rate;
    switch(toy_chunk_word(name)) {
    case TOY_NAME_WORD('5','0','h','z'): rate=1u;break;
    case TOY_NAME_WORD('6','0','h','z'): rate=2u;break;
    default: return 0u;
    }
    switch(toy_chunk_word(name+4u)) {
    case TOY_NAME_WORD('_','B','u','m'):
        return toy_chunk_word(name+8u)==TOY_NAME_WORD('p','e','r','_') &&
            toy_chunk_word(name+12u)==TOY_NAME_WORD('L','R','T','F') &&
            toy_chunk_word(name+16u)==TOY_NAME_WORD('.','a','v','i') &&
            !(toy_chunk_word(name+20u)&255u)?rate:0u;
    case TOY_NAME_WORD('_','I','n','t'):
        return toy_chunk_word(name+8u)==TOY_NAME_WORD('r','o','_','L') &&
            toy_chunk_word(name+12u)==TOY_NAME_WORD('R','T','F','.') &&
            toy_chunk_word(name+16u)==TOY_NAME_WORD('a','v','i','\0')?rate+2u:0u;
    default: return 0u;
    }
}
#undef TOY_NAME_WORD

static bool toy_chunk_pending(const struct kui_toy_movie_range *range) {
    const volatile struct kui_retail_gd *s=&C.config.region->shared.service;
    if(!s->pending)return false;
    const uint32_t token=s->token,command=s->command,lba=s->lba,count=s->count;
    const uint32_t offset=lba-range->lba;
    return (command==KUI_GD_PIOREAD || command==KUI_GD_DMAREAD) &&
        offset<range->count && count && count<=range->count-offset &&
        s->pending && s->token==token;
}

#if KUI_TOY_SUITE_MOVIE_TIMING
/* Follow only the exact owner's foreground AVI/stream objects in main RAM.
 * P1 and P2 are the two legitimate aliases for these objects. */
static bool toy_chunk_ram(uint32_t address,uint32_t bytes) {
    const uint32_t physical=address&UINT32_C(0x1fffffff);
    return (address&UINT32_C(0xdf000003))==UINT32_C(0x8c000000) &&
        physical>=UINT32_C(0x0c010000) && physical<=UINT32_C(0x0d000000)-bytes;
}

static bool toy_chunk_buffer(uint32_t ctx,uint32_t *ring_used,uint32_t *missing) {
    const uint32_t unknown=UINT32_MAX;
    *ring_used=*missing=unknown;
    const uint32_t stream=toy_chunk_word(ctx+8u);
    if(!toy_chunk_ram(stream,20u))return false;
    const uint32_t channel=toy_chunk_word(stream+4u);
    if(!toy_chunk_ram(channel,72u))return false;
    const uint32_t avi=toy_chunk_word(channel+68u);
    if(!toy_chunk_ram(avi,108u))return false;
    const uint32_t capacity=toy_chunk_word(avi+0x14u);
    const uint32_t released=toy_chunk_word(avi+0x64u);
    const uint32_t filled=toy_chunk_word(avi+0x68u);
    const uint32_t used=filled-released;
    /* Original 8c098a02 reports capacity+released-filled as free bytes. */
    if(!capacity || capacity>UINT32_C(0x400000) || used>capacity)return false;
    *ring_used=used;

    const uint32_t entries=toy_chunk_word(avi+0x38u);
    const uint32_t count=toy_chunk_word(avi+0x44u);
    const uint32_t next=toy_chunk_word(stream+0x10u)+1u;
    const uint32_t kind=toy_chunk_word(channel+4u);
    if(kind>255u || count>65536u || next>=count || !toy_chunk_ram(entries,count*12u))return false;
    const uint32_t base=toy_chunk_word(avi+0x20u);
    uint32_t limit=count-next;
    if(limit>32u)limit=32u;
    /* Same kind and next index as original acquisition at 8c08ee38.
     * Stop at the first matching video entry, with a bounded scan. */
    for(uint32_t entry=entries+next*12u;limit;--limit,entry+=12u) {
        const uint32_t metadata=toy_chunk_word(entry+8u);
        if((metadata&255u)!=kind)continue;
        const uint32_t offset=toy_chunk_word(entry);
        const uint32_t size=toy_chunk_word(entry+4u);
        if(offset>UINT32_MAX-base || size>UINT32_MAX-(offset+base))return false;
        const uint32_t end=offset+base+size;
        *missing=end>filled?end-filled:0u;
        /* Original acquisition at 8c08eeda requires ready bit 1 and
         * rejects released bit 2. */
        return (metadata&0x300u)==0x100u && size && end<=filled;
    }
    return false;
}

#if KUI_TOY_SUITE_DECODE_WINDOW
enum {
    TOY_QUIET_LIMIT=1u, TOY_QUIET_RING, TOY_QUIET_REQUEST, TOY_QUIET_CONTEXT,
    TOY_QUIET_MAX_FRAME=30u, TOY_QUIET_MAX_SAMPLES=44100u,
    TOY_QUIET_MAX_TICKS=976562u, TOY_QUIET_MIN_USED=384u*1024u
};

static bool toy_quiet_refuse(struct kui_toy_movie_onset_trace *onset,
    uint32_t frame,uint32_t reason,bool permanent) {
    onset->quiet_reason=reason;
    onset->quiet_end_frame=frame+1u;
    if(permanent || onset->quiet_defers)onset->quiet_ctx=UINT32_MAX;
    return false;
}

/* Called only at the original enqueue site, after the ordinary AVI parser
 * has processed the prior result. Never touch any guest or service state. */
bool kui_toy_movie_refill_defer(uint32_t avi,uint32_t file,uint32_t bytes) {
    struct kui_toy_movie_onset_trace *onset=&C.onset[1];
    const uint32_t ctx=onset->quiet_ctx;
    if(!ctx || ctx==UINT32_MAX)return false;
    const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[1];
    if((w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))!=
       KUI_TOY_MOVIE_EXIT_STARTED || w->start_status!=0x07dbu ||
       (ctx&UINT32_C(0xdf000003))!=UINT32_C(0x8c000000) ||
       (ctx&UINT32_C(0x1fffffff))<UINT32_C(0x0c008000) ||
       (ctx&UINT32_C(0x1fffffff))>UINT32_C(0x0d000000)-0x248u)return false;
    const uint32_t frame=toy_chunk_word(ctx+0x1c0u);
    const struct ra_clock now=clock_read();
    if(frame>=TOY_QUIET_MAX_FRAME || toy_chunk_word(ctx+0x244u)>=TOY_QUIET_MAX_SAMPLES ||
       (now.valid && onset->quiet_tick-now.counter>=TOY_QUIET_MAX_TICKS))
        return toy_quiet_refuse(onset,frame,TOY_QUIET_LIMIT,true);
    if(!now.valid || !bytes || (bytes&2047u) || bytes>1024u*1024u ||
       toy_chunk_word(ctx)!=avi || !toy_chunk_ram(avi,108u))
        return toy_quiet_refuse(onset,frame,TOY_QUIET_CONTEXT,true);
    const uint32_t owner=toy_chunk_word(avi);
    if(!toy_chunk_ram(owner,112u) || toy_chunk_word(owner+108u)!=file ||
       !toy_chunk_ram(file,4u))
        return toy_quiet_refuse(onset,frame,TOY_QUIET_CONTEXT,true);
    const volatile kui_retail_async_region_type *r=C.config.region;
    if(r->shared.service.pending || r->shared.service.executing || r->engine.active ||
       r->shared.stream.state>RA_PAUSED || toy_chunk_word(UINT32_C(0x8c0b0228)))
        return toy_quiet_refuse(onset,frame,TOY_QUIET_REQUEST,false);
    uint32_t used,missing;
    if(!toy_chunk_buffer(ctx,&used,&missing) || used<TOY_QUIET_MIN_USED)
        return toy_quiet_refuse(onset,frame,TOY_QUIET_RING,false);
    onset->quiet_reason=0u;
    onset->quiet_end_frame=frame+1u;
    if(!onset->quiet_defers)C.suite[4]=frame+1u;
    ++onset->quiet_defers;
    return true;
}
#if KUI_TOY_SUITE_MODE == 61
/* Leave the stock AVI parser and enqueue in control of file position and
 * completion. At 8c0989d8 the AVI records the exact byte count returned by
 * GDFS, so the following refill starts where this shorter one ends. */
uint32_t kui_toy_movie_refill_length(uint32_t avi,uint32_t file,uint32_t bytes,
    uint32_t dest) {
    if(kui_toy_movie_refill_defer(avi,file,bytes))return UINT32_MAX;

    enum { CAP_SECTORS=8u, CAP_BYTES=CAP_SECTORS*KUI_GAME_DATA_BYTES,
        CAP_MIN_BYTES=13u*KUI_GAME_DATA_BYTES, CAP_MIN_USED=16u*1024u,
        CAP_MAX_USED=192u*1024u };
    struct kui_toy_movie_onset_trace *onset=&C.onset[0];
    const uint32_t ctx=onset->quiet_ctx;
    const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[0];
    if(!ctx || bytes<CAP_MIN_BYTES || bytes>KUI_RETAIL_IMAGE_MAX_SECTORS*KUI_GAME_DATA_BYTES ||
       (bytes&(KUI_GAME_DATA_BYTES-1u)) || (dest&31u) ||
       (w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))!=
           KUI_TOY_MOVIE_EXIT_STARTED || w->start_status!=0x07dbu ||
       toy_chunk_word(ctx)!=avi || !toy_chunk_ram(avi,108u) || toy_chunk_word(avi+16u)!=6u)
        return bytes;

    const uint32_t capacity=toy_chunk_word(avi+0x14u);
    const uint32_t ring=toy_chunk_word(avi+0x1cu),write=toy_chunk_word(avi+0x60u);
    const uint32_t used=toy_chunk_word(avi+0x68u)-toy_chunk_word(avi+0x64u);
    const uint32_t ring_end=toy_chunk_word(avi+0x18u);
    if(used<CAP_MIN_USED || used>CAP_MAX_USED || used>capacity || capacity>UINT32_C(0x400000) ||
       capacity-used<CAP_BYTES || write>=capacity || capacity-write<CAP_BYTES ||
       write>=ring_end || ring_end-write<CAP_BYTES ||
       !toy_chunk_ram(ring,capacity) || dest!=ring+write)
        return bytes;

    const uint32_t owner=toy_chunk_word(avi);
    if(!toy_chunk_ram(owner,112u) || toy_chunk_word(owner+108u)!=file ||
       !toy_chunk_ram(file,80u))return bytes;
    const uint32_t device=toy_chunk_word(file),file_mode=toy_chunk_word(file+76u);
    const uint32_t start=toy_chunk_word(file+8u),position=toy_chunk_word(file+20u);
    const uint32_t length=toy_chunk_word(file+16u);
    if(!(file_mode&0xffffu) || (file_mode>>16)!=1u || !toy_chunk_ram(device,32u) ||
       toy_chunk_word(device+12u)!=UINT32_C(0x8c0c6260) || toy_chunk_word(device+28u) ||
       position>=length || (bytes/KUI_GAME_DATA_BYTES)>length-position ||
       start<150u || start>=720000u || position>=720000u-start)
        return bytes;

    const unsigned variant=toy_chunk_variant(ctx+0xbcu);
    if(!variant || variant>2u)return bytes;
    const struct kui_toy_movie_range *range=&C.config.movie_ranges[variant-1u];
    const uint32_t offset=start+position-150u-range->lba;
    if(offset<386u || offset>=range->count || bytes/KUI_GAME_DATA_BYTES>range->count-offset)
        return bytes;

    const volatile kui_retail_async_region_type *r=C.config.region;
    if(r->shared.service.command || r->shared.service.pending || r->shared.service.executing ||
       r->shared.service.sector_bytes!=KUI_GAME_DATA_BYTES || r->engine.active || r->engine.in_irq ||
       r->shared.stream.state>RA_PAUSED || !r->shared.port.acquired || r->shared.port.fault ||
       toy_chunk_word(UINT32_C(0x8c0b0228)) ||
       toy_chunk_word(UINT32_C(0x8c11546c))!=toy_chunk_word(UINT32_C(0x8c115470)))
        return bytes;

    const uint32_t frame=toy_chunk_word(ctx+0x1c0u)+1u;
    if(!onset->quiet_defers)onset->quiet_tick=frame;
    ++onset->quiet_defers;
    onset->quiet_end_frame=frame;
    onset->quiet_reason+=bytes/KUI_GAME_DATA_BYTES-CAP_SECTORS;
    return CAP_BYTES;
}
#endif
#endif

static void toy_chunk_onset(uint32_t ctx,unsigned movie,bool pending) {
    const volatile struct kui_toy_movie_exit_witness *w=&C.movie_exit[movie];
    if((w->flags&(KUI_TOY_MOVIE_EXIT_STARTED|KUI_TOY_MOVIE_EXIT_TERMINAL))!=KUI_TOY_MOVIE_EXIT_STARTED ||
       w->start_status!=0x07dbu)return;
    struct kui_toy_movie_onset_trace *onset=&C.onset[movie];
    const uint32_t outputs=toy_chunk_word(ctx+0xacu);
    const uint32_t frame=toy_chunk_word(ctx+0x1c0u)+1u;
#if KUI_TOY_SUITE_DECODE_WINDOW
#if KUI_TOY_SUITE_MODE == 61
    if(movie==0u && !onset->quiet_ctx)onset->quiet_ctx=ctx;
#endif
    if(movie==1u && !onset->quiet_ctx) {
        const struct ra_clock now=clock_read();
        if(frame<=TOY_QUIET_MAX_FRAME && now.valid) {
            onset->quiet_tick=now.counter;
            onset->quiet_ctx=ctx;
        } else {
            toy_quiet_refuse(onset,frame-1u,now.valid?TOY_QUIET_LIMIT:TOY_QUIET_CONTEXT,true);
        }
    }
#endif
    if(outputs!=onset->last_out_count) {
        onset->last_out_frame=frame;
        onset->last_out_count=outputs;
    }
    const bool skip=!onset->skip_frame && toy_chunk_word(ctx+0x90u);
    const bool wait=pending && outputs && !onset->wait_frame;
    if(skip || wait) {
        uint32_t used,missing;
        (void)toy_chunk_buffer(ctx,&used,&missing);
        if(skip) {
            onset->skip_frame=frame;
            onset->skip_used=used;
            onset->skip_missing=missing;
        }
        if(!wait)return;
        onset->wait_frame=frame;
        onset->wait_used=used;
        onset->wait_missing=missing;
#if KUI_TOY_SUITE_FRAME_TIMING
        const volatile struct kui_retail_gd *service=&C.config.region->shared.service;
        struct kui_toy_movie_wait_trace *reading=&C.wait[movie];
        const uint32_t token=service->token;
        reading->lba=service->lba;
        reading->sectors=service->count;
        reading->verified=UINT32_MAX;
        if(C.resume.state==TOY_RESUME_STAGE && C.resume.token==token)
            reading->verified=C.resume.checked;
#if KUI_TOY_SUITE_MODE >= 62
        if(C.movie_ahead.variant && !C.movie_ahead.error && C.movie_ahead.first==reading->lba &&
           C.movie_ahead.verified-C.movie_ahead.first<reading->sectors)
            reading->verified=C.movie_ahead.verified-C.movie_ahead.first;
#endif
        reading->elapsed=UINT32_MAX;
        if(toy_present.owner==ctx && toy_present.start_valid) {
            const struct ra_clock now=clock_read();
            const uint32_t elapsed=toy_present.start_tick-now.counter;
            if(now.valid && elapsed<=INT32_MAX)reading->elapsed=elapsed;
        }
#if KUI_TOY_SUITE_MODE >= 61
        const volatile struct kui_retail_async *engine=&C.config.region->engine;
        reading->age=reading->to_complete=UINT32_MAX;
        if(token && service->pending && service->token==token && engine->token==token) {
            const uint32_t started=engine->stats.hooks;
            const struct ra_clock now=clock_read();
            const uint32_t age=started-now.counter;
            if(now.valid && age<=INT32_MAX) {
                reading->age=age;
                C.wait_token[movie]=token;
            }
        }
#endif
#endif
    }
}
#endif

/* Foreground game-stack observer. Service metadata is sampled read-only. */
void kui_toy_movie_chunk_observe(uint32_t ctx,int32_t result) {
    if(!C.initialized || !C.config.toy_poll_owner ||
       (ctx&UINT32_C(0xdf000003))!=UINT32_C(0x8c000000))return;
    const uint32_t physical=ctx&UINT32_C(0x1fffffff);
    const uint32_t begin=KUI_TOY_SUITE_AFTER_START?UINT32_C(0x0c008000):UINT32_C(0x0c010000);
#if KUI_TOY_SUITE_MOVIE_TIMING
    const uint32_t bytes=0x1c4u;
#else
    const uint32_t bytes=0xd4u;
#endif
    if(physical<begin || physical>UINT32_C(0x0d000000)-bytes)return;
    const unsigned variant=toy_chunk_variant(ctx+0xbcu);
    if(!variant || !(C.movie_mask&(1u<<(variant-1u))))return;
    const unsigned movie=(variant-1u)/2u;
    struct kui_toy_movie_chunk_trace *trace=&C.movie_chunk[movie];
    const bool pending=result==-2 && toy_chunk_pending(&C.config.movie_ranges[variant-1u]);
    if(result>=0)++trace->ready;
    else if(result!=-2)++trace->other;
    else if(pending)++trace->wait_gd;
    else ++trace->wait_no_gd;
#if KUI_TOY_SUITE_MOVIE_TIMING
    toy_chunk_onset(ctx,movie,pending);
#endif
}
#endif
#if KUI_NATIVE_TOY_HEADER_REPLAY
#include "toy_header_replay.inc"
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
static unsigned playback_variant(const struct kui_retail_gd *s) {
    unsigned variant=movie_variant(s);
    /* Both observed opening reads start two sectors in and request 384.
     * Include only requests beginning beyond those initial AVI sectors. */
    return variant && s->lba>=C.config.movie_ranges[variant-1u].lba+386u?variant:0u;
}
static unsigned playback_delivery(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    if(!(C.movie_mask&15u) || !r->engine.active || !s->pending || r->engine.token!=s->token)return 0u;
    if(s->token!=C.playback_token) {
        C.playback_token=s->token;
        C.playback_variant=playback_variant(s);
    }
    return C.playback_variant;
}
static void playback_complete(const struct kui_retail_gd *s,unsigned variant,
    struct ra_clock now,uint32_t elapsed) {
    if(s->lba<C.config.movie_ranges[variant-1u].lba+386u) {C.last_play_variant=0u;return;}
    unsigned movie=(variant-1u)/2u;
    struct kui_toy_playback_stats *p=&C.playback[movie];
    p->sectors+=s->count;
    p->end_lba=s->lba+s->count;
#if KUI_TOY_SUITE_MODE == 35
    struct kui_toy_window_trace *window=&C.demand.window;
    const bool from_ram=C.window_token && C.window_token==s->token;
    if(from_ram) {
        C.window_token=0u;
        if(!window->requests++)window->first_lba=s->lba;
        window->sectors+=s->count;
        window->end_lba=s->lba+s->count;
    }
    const bool target=variant==KUI_TOY_RAM_WINDOW_VARIANT+1u && s->lba==KUI_TOY_RAM_WINDOW_TARGET;
    if(target && from_ram)++window->target_ram;
#endif
    if(!now.valid || elapsed>=UINT32_C(0x80000000)) {
        ++p->bad_clock;C.last_play_variant=0u;return;
    }
    uint32_t bit=1u<<movie;
    if(!(C.playback_valid&bit)) {
        C.playback_valid|=bit;
        C.playback_started[movie]=now.counter+elapsed;
    }
    p->span=C.playback_started[movie]-now.counter;
#if !KUI_NATIVE_TOY_HEADER_REPLAY
    if(elapsed>p->longest)p->longest=elapsed;
#endif
    uint32_t gap=C.last_play_tick-now.counter;
#if KUI_TOY_SUITE_MODE == 35
    if(from_ram && elapsed>window->max_request_ticks)window->max_request_ticks=elapsed;
    if(target && (from_ram || !window->target_ram)) {
        window->target_request_ticks=elapsed;
        window->target_gap_ticks=C.last_play_variant==variant && gap<UINT32_C(0x80000000)?gap:0u;
    }
#endif
    if(C.last_play_variant==variant && gap<UINT32_C(0x80000000) && gap>p->gap) {
        p->gap=gap;p->gap_lba=s->lba;
    }
    C.last_play_variant=variant;C.last_play_tick=now.counter;
}
static void playback_visit(unsigned variant,bool irq,uint32_t wait,uint32_t blocks,
    uint32_t elapsed,bool clock_valid) {
    struct kui_toy_playback_stats *p=&C.playback[(variant-1u)/2u];
    if(irq)p->sci_irq_blocks+=blocks;
    else p->foreground_blocks+=blocks;
    if(!clock_valid) {++p->bad_clock;return;}
    if(irq) {
        p->sci_irq+=elapsed;
        if(elapsed>p->sci_irq_max)p->sci_irq_max=elapsed;
    } else {
        p->foreground+=elapsed;
        if(wait)p->foreground_wait+=elapsed;
        if(elapsed>p->foreground_max)p->foreground_max=elapsed;
    }
}
#endif
/* GD progress has already published terminal status. Ordinary completions
 * and cache-only completions share this exact observer. */
static void movie_complete(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    if(!C.movie_mask || (s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       (s->status!=KUI_GD_FAILED && s->status!=KUI_GD_COMPLETED))return;
    unsigned variant=movie_variant(s);
    if(!variant) {
#if !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
        C.last_movie=0u;
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
        C.last_play_variant=0u;
#endif
        return;
    }
    /* Playback reports retain the ABI without repeating whole-movie totals. */
#if !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
    struct kui_toy_movie_stats *m=&C.movies[(variant-1u)/2u];
#endif
    C.movie_mask|=1u<<(variant+3u);
    if(s->status==KUI_GD_FAILED) {
#if !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
        if(!m->failed++) {
            m->first_error=s->error;m->first_error_lba=s->lba;
            m->first_error_count=s->count;m->first_error_bytes=s->completed_bytes;
        }
        C.last_movie=0u;
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
        ++C.playback[(variant-1u)/2u].failed;
        C.last_play_variant=0u;
#endif
        return;
    }
#if !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
    ++m->completed;m->sectors+=s->count;
#endif
    struct ra_clock now=clock_read();
    uint32_t elapsed=r->engine.stats.hooks-now.counter;
#if KUI_TOY_SUITE_MODE >= 61
    const unsigned movie=(variant-1u)/2u;
    struct kui_toy_movie_wait_trace *reading=&C.wait[movie];
    if(C.wait_token[movie] && s->token==C.wait_token[movie] && s->lba==reading->lba &&
       s->count==reading->sectors && r->engine.token==s->token && now.valid &&
       elapsed<=INT32_MAX && elapsed>=reading->age) {
        reading->to_complete=elapsed-reading->age;
        C.wait_token[movie]=0u;
    }
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    playback_complete(s,variant,now,elapsed);
#endif
#if !KUI_NATIVE_TOY_HEADER_REPLAY && !defined(KUI_TOY_SUITE_MODE)
    if(!now.valid || elapsed>=UINT32_C(0x80000000)) {
        ++m->bad_clock;C.last_movie=0u;return;
    }
    if(elapsed>m->longest) {
        m->longest=elapsed;m->longest_lba=s->lba;m->longest_count=s->count;
    }
    uint32_t gap=C.last_movie_tick-now.counter;
    if(C.last_movie==variant && gap<UINT32_C(0x80000000) && gap>m->gap) {
        m->gap=gap;m->gap_lba=s->lba;
    }
    C.last_movie=variant;C.last_movie_tick=now.counter;
#endif
}
#endif
#if KUI_TOY_SUITE_MODE == 1
#include "toy_suite_witness.inc"
#endif
#if KUI_TOY_SUITE_ASYNC_DEMAND && KUI_TOY_SUITE_MODE < 62
#include "toy_demand_trace.inc"
#endif
#if KUI_TOY_SUITE_MOVIE_POLL
static uint32_t toy_poll_word(uint32_t address) {
#ifdef KUI_NATIVE_READAHEAD_AUX_HOST_TEST
    extern uint32_t kui_toy_poll_test_word(uint32_t);
    return kui_toy_poll_test_word(address);
#else
    return *(const volatile uint32_t *)(uintptr_t)address;
#endif
}

/* The BIOS jump stub is shared. Its saved PR plus the two original SDK
 * frames distinguish a scheduled or one-pass poll from a blocking wait.
 * Read only a checked slice of the game stack, under the GD entry lock. */
static uint32_t toy_poll_context(uint32_t function,uint32_t frame) {
    if(!C.config.toy_poll_owner || (frame&UINT32_C(0xdf000003))!=UINT32_C(0x8c000000))return 0u;
    const uint32_t physical=frame&UINT32_C(0x1fffffff);
    if(physical<UINT32_C(0x0c008000) || physical>UINT32_C(0x0d000000)-104u)return 0u;
    const uint32_t pr=toy_poll_word(frame+28u);
    uint32_t outer;
    if(function==KUI_GD_EXEC && pr==UINT32_C(0x8c0bd55a) &&
       toy_poll_word(frame+36u)==UINT32_C(0x8c0b1fe6))
        outer=toy_poll_word(frame+40u);
    else if(function==KUI_GD_CHECK && pr==UINT32_C(0x8c0bd374) &&
            toy_poll_word(frame+52u)==UINT32_C(0x8c0b145c))
        outer=toy_poll_word(frame+100u);
    else return 0u;
    switch(outer) {
    case UINT32_C(0x8c0b3b88): return 1u;
    case UINT32_C(0x8c097144): return 2u;
    case UINT32_C(0x8c09712a): return 3u;
    default: return 0u;
    }
}
#endif
uint32_t kui_native_readahead_aux_dispatch(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *r,uint32_t reserved) {
    if(!C.initialized || r!=C.config.region ||
#if KUI_TOY_SUITE_MOVIE_POLL
       (reserved && event!=KUI_NATIVE_RA_DEMAND_GD_ENTER) ||
#else
       reserved ||
#endif
       *C.config.ops->region_storage!=r) {
        ++C.stats.guard_faults;
#if KUI_TOY_SUITE_MOVIE_POLL
        if(event==KUI_NATIVE_RA_DEMAND_GD_ENTER || event==KUI_NATIVE_RA_DEMAND_GD_LEAVE)return 0u;
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        if(event==KUI_NATIVE_RA_CALIBRATE) return 0u;
#endif
        return event==KUI_NATIVE_RA_QUIESCE?0u:1u;
    }
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    if(event==KUI_NATIVE_RA_CALIBRATE) return arg ? 0u : burst_quiet_calibrate(r);
#endif
#if KUI_TOY_SUITE_ASYNC_DEMAND
    if(event==KUI_NATIVE_RA_DEMAND_GD_ENTER || event==KUI_NATIVE_RA_DEMAND_GD_LEAVE) {
#if KUI_TOY_SUITE_MOVIE_POLL
        uint32_t poll=event==KUI_NATIVE_RA_DEMAND_GD_ENTER?toy_poll_context(arg,reserved):0u;
#if KUI_TOY_SUITE_MODE < 62
        (void)toy_demand_gd_trace(event,arg,r,poll);
#endif
        /* The gateway can return 1 on a guard fault. Only 2 and 3 admit. */
        return poll==1u || poll==2u ? poll+1u : 0u;
#else
        return toy_demand_gd_trace(event,arg,r);
#endif
    }
#endif
#if KUI_TOY_FMV_COMPARE
    if(event==KUI_NATIVE_RA_FMV_REQUEST || event==KUI_NATIVE_RA_FMV_ACK) {
        const struct kui_retail_gd *service=&r->shared.service;
        if(event==KUI_NATIVE_RA_FMV_REQUEST) {
            unsigned variant=playback_variant(service);
            if(!variant) return 1u;
            const struct kui_toy_movie_range *range=&C.config.movie_ranges[variant-1u];
            if(service->lba-range->lba>=range->count) return 1u;
        } else {
            unsigned i=0u;
            while(i<KUI_TOY_FMV_PENDING && C.fmv_trace.pending[i].token!=service->token) ++i;
            if(i==KUI_TOY_FMV_PENDING) return 1u;
        }
        struct ra_clock now=clock_read();
        if(!now.valid) ++C.fmv_trace.clock_lost;
        else if(event==KUI_NATIVE_RA_FMV_REQUEST)
            kui_toy_fmv_request(&C.fmv_trace,C.config.movie_ranges,service->token,
                service->lba,service->count,now.counter);
        else
            kui_toy_fmv_status(&C.fmv_trace,service->token,now.counter,(int32_t)arg,
                service->completed_bytes);
        return 1u;
    }
#endif
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    if(event==KUI_NATIVE_RA_MOVIE_COMPLETE && !arg) {
#if KUI_TOY_SUITE_MODE >= 62
        if(C.movie_ahead.variant && (r->shared.service.error==KUI_GD_ERROR_CANCELLED ||
           r->shared.service.status==KUI_GD_NOT_FOUND)) {
            C.movie_ahead.error=true;
            (void)toy_ahead_clear(r);
        }
#endif
#if KUI_TOY_SUITE_VERIFIED_RESUME
        if(!r->shared.service.pending)toy_resume_invalidate();
#endif
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        if(r->shared.service.error==KUI_GD_ERROR_CANCELLED) (void)burst_release(r);
#endif
#if KUI_NATIVE_TOY_HEADER_REPLAY
        header_publish(r);
#endif
        movie_complete(r);
#if KUI_TOY_SUITE_MODE == 1
        toy_suite_witness_complete(r);
#endif
        return 1u;
    }
#endif
#if KUI_TOY_SUITE_MODE >= 62
    if(event==KUI_NATIVE_RA_MOVIE_AHEAD) {
        /* The game UPDATE owns the foreground. It may resume an idle
         * sequential token search without pretending a GD request exists. */
        if(arg==toy_present.owner && C.movie_ahead.variant &&
           r->engine.native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE &&
           !r->engine.active && !r->shared.service.pending && !r->shared.service.executing &&
           !r->engine.in_irq && !C.quiesced)
            (void)toy_movie_ahead(r,0u,false,0u);
        return 1u;
    }
#endif
#if KUI_TOY_WIRE_STREAM
    if(event==KUI_NATIVE_RA_WIRE_TIMER) {
#if KUI_TOY_WIRE_TICK
        if(!arg)toy_wire_tick_visit(r);
#else
        /* Nothing but the stream's own registers: no request state is read
         * and no low reader entry is called from this timer interrupt. */
        if(!arg && toy_wire_keep()->on)kui_wire_timer(toy_wire_stream());
#endif
        return 1u;
    }
#endif
    /* Admit the event-specific argument before clocks, seeding or any low
     * operation. DELIVER's wait is unchanged; an invalid optional budget must
     * never silently turn into the larger historical PIO window. */
    if(event<KUI_NATIVE_RA_DELIVER || event>KUI_NATIVE_RA_CAPTURE_START ||
       (event==KUI_NATIVE_RA_TICK?arg>KUI_NATIVE_RA_TICK_ONE_BLOCK:
        event!=KUI_NATIVE_RA_DELIVER && arg)) {
        ++C.stats.guard_faults;return 0u;
    }
#if KUI_NATIVE_TOY_HEADER_REPLAY
    if(event==KUI_NATIVE_RA_QUIESCE || event==KUI_NATIVE_RA_RESET || event==KUI_NATIVE_RA_RESUME ||
       r->engine.native_loan_state!=KUI_VMU_NATIVE_ASYNC_LOAN_IDLE)header_invalidate();
#endif
    if(r->engine.native_loan_state!=KUI_VMU_NATIVE_ASYNC_LOAN_IDLE) {
        skipped(KUI_NATIVE_RA_SKIP_LOAN);
        return event==KUI_NATIVE_RA_QUIESCE?0u:1u;
    }
    if(event==KUI_NATIVE_RA_CAPTURE_START) {
        /* A gesture opens a statistics window, not a new cache lifetime. */
        memset(&C.stats,0,sizeof(C.stats));
#if KUI_TOY_WIRE_STREAM
        toy_wire_reset(false);
#endif
        memset(&C.diagnostics,0,sizeof(C.diagnostics));return 1u;
    }
    C.current_event=event;
    /* Preserve entry context across low callbacks which may update the
     * mutable engine. This uses no additional clock sample or public state. */
    uint32_t visit_context=event|((arg&UINT32_C(0xffff))<<8)|((r->engine.in_irq&255u)<<24);
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    unsigned playing=event==KUI_NATIVE_RA_DELIVER?playback_delivery(r):0u;
#if KUI_TOY_SUITE_ASYNC_DEMAND
    bool irq=(visit_context>>24)!=0u;
#else
    bool irq=(visit_context>>24)==1u;
#endif
    uint32_t blocks=playing?(irq?r->engine.stats.irq_blocks:r->engine.stats.call_blocks):0u;
#endif
    struct ra_clock before=clock_read();uint32_t result=1u;
#if KUI_TOY_SUITE_ASYNC_DEMAND && KUI_TOY_SUITE_MODE < 62
    bool was_armed=event==KUI_NATIVE_RA_DELIVER && toy_demand_before(playing,r,before);
#endif
#if KUI_TOY_SUITE_MODE == 34
    const struct kui_sci_stream_stats *sd=&r->shared.stream.stats;
    uint32_t overruns=sd->overruns,starts=sd->starts,polled=sd->polled;
#elif KUI_TOY_SUITE_PIO_WINDOW
    const struct kui_sci_stream_stats *sd=&r->shared.stream.stats;
    const uint32_t overruns=sd->overruns,polled=sd->toy_pio_window_blocks,ok=sd->toy_pio_window_ok;
#elif KUI_TOY_SUITE_SLOW_RETRY
    const struct kui_sci_stream_stats *sd=&r->shared.stream.stats;
    const uint32_t overruns=sd->overruns,orer=sd->toy_retry_orer;
    const uint32_t slow=sd->toy_retry_slow,ok=sd->toy_retry_ok;
#endif
#if KUI_TOY_SUITE_MODE == 36
    const uint32_t overruns=r->shared.stream.stats.overruns;
    struct kui_toy_demand_movie_trace *dma_movie=playing?&C.demand.movie[(playing-1u)/2u]:NULL;
    if(dma_movie && was_armed) {
        const unsigned priority=(read_hw(UINT32_C(0xffa00040),4u)>>8)&3u;
        ++dma_movie->dma_priority[priority];
    }
#endif
#if KUI_TOY_SUITE_DMA_ROUND_ROBIN
    if(playing && was_armed && C.config.toy_poll_owner) {
        const unsigned clip=(playing-1u)/2u;
        const uint32_t mode=read_hw(UINT32_C(0xffa00040),4u)&UINT32_C(0x8307);
        ++C.suite[2u*clip];
        if(mode==UINT32_C(0x8301)) ++C.suite[2u*clip+1u];
    }
#endif
    switch(event) {
    case KUI_NATIVE_RA_DELIVER:
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        burst_service(arg,r);
#elif KUI_TOY_SUITE_ASYNC_DEMAND
        suite_demand(arg,r);
#elif KUI_NATIVE_TOY_HEADER_REPLAY
        header_deliver(arg,r);
#else
#if KUI_TOY_SUITE_MODE == 1 || KUI_TOY_SUITE_MODE == 31
        if(C.suite_source==1u)suite_demand(arg,r);
        else
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
        if(C.config.suite_seed.count[0])suite_window_deliver(arg,r);
        else
#endif
        deliver(arg,r);
#endif
        break;
    case KUI_NATIVE_RA_TICK:
        ++C.stats.heartbeat_visits;
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
        burst_service(0,r);break;
#elif KUI_TOY_SUITE_ASYNC_DEMAND
        break;
#elif KUI_TOY_SUITE_MODE == 1 || KUI_TOY_SUITE_MODE == 31
        if(C.suite_source==1u)break;
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6
        if(C.config.suite_seed.count[0])break;
#endif
#if KUI_TOY_SUITE_MODE != 26 && KUI_TOY_SUITE_MODE != 27 && KUI_TOY_SUITE_MODE != 28 && !KUI_TOY_SUITE_ASYNC_DEMAND
        if(C.quiesced && r->engine.opened && r->shared.port.acquired) {
            C.quiesced=0u;++C.stats.resumes;
        }
        seed_completed(r);producer(r,false,arg==KUI_NATIVE_RA_TICK_ONE_BLOCK?
            1u:KUI_NATIVE_RA_PIO_BLOCK_LIMIT);break;
#endif
    case KUI_NATIVE_RA_QUIESCE:
        result=quiesce(r);
#if KUI_TOY_SUITE_VERIFIED_RESUME
        if(result)toy_resume_invalidate();
#endif
        break;
    case KUI_NATIVE_RA_RESUME:
        C.quiesced=0u;++C.stats.resumes;break;
    case KUI_NATIVE_RA_RESET:
        result=quiesce(r);
        if(result) {
#if KUI_TOY_SUITE_VERIFIED_RESUME
            toy_resume_invalidate();
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
            C.playback_token=C.last_play_variant=0u;
#endif
            (void)kui_native_ra_cache_init(&C.cache,C.config.data,KUI_NATIVE_RA_CACHE_BYTES);
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
            C.burst.have_token=false;
#endif
            C.have_seed=C.have_hit=C.abandoned=0u;
#if defined(KUI_TOY_SUITE_MODE)
            C.config.suite_seed.count[0]=C.config.suite_seed.count[1]=0u;
#endif
            /* A later heartbeat must not republish the old completed
             * cursor after explicit validity reset. A new token may seed. */
            C.have_completed=1u;C.completed_token=r->engine.token;
        }
        break;
    default:++C.stats.guard_faults;result=0u;break;
    }
    struct ra_clock after=clock_read();uint32_t elapsed=before.counter-after.counter;
#if KUI_TOY_SUITE_ASYNC_DEMAND && KUI_TOY_SUITE_MODE < 62
    if(event==KUI_NATIVE_RA_DELIVER) toy_demand_after(playing,(visit_context>>24)==2u,was_armed,r,after);
#endif
#if KUI_TOY_SUITE_MODE == 34
    if(playing) {
        struct kui_toy_demand_movie_trace *movie=&C.demand.movie[(playing-1u)/2u];
        movie->overruns+=sd->overruns-overruns;
        movie->starts+=sd->starts-starts;
        movie->polled+=sd->polled-polled;
    }
#endif
#if KUI_TOY_SUITE_MODE == 36
    if(dma_movie)dma_movie->overruns+=r->shared.stream.stats.overruns-overruns;
#elif KUI_TOY_SUITE_PIO_WINDOW
    if(playing) {
        struct kui_toy_demand_movie_trace *movie=&C.demand.movie[(playing-1u)/2u];
        movie->overruns+=sd->overruns-overruns;
        movie->window_pio+=sd->toy_pio_window_blocks-polled;
        movie->window_ok+=sd->toy_pio_window_ok-ok;
    }
#elif KUI_TOY_SUITE_SLOW_RETRY
    if(playing) {
        struct kui_toy_demand_movie_trace *movie=&C.demand.movie[(playing-1u)/2u];
        movie->overruns+=sd->overruns-overruns;
        movie->retry_orer+=sd->toy_retry_orer-orer;
        movie->retry_slow+=sd->toy_retry_slow-slow;
        movie->retry_ok+=sd->toy_retry_ok-ok;
    }
#endif
#if KUI_TOY_SUITE_MODE == 26 || KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    if(event==KUI_NATIVE_RA_TICK && C.burst.variant>=3u && before.valid && after.valid &&
       elapsed<UINT32_C(0x80000000)) {
        uint32_t *metrics=C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE];
        metrics[4]+=elapsed;
        if(elapsed>metrics[5])metrics[5]=elapsed;
    }
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    if(playing)playback_visit(playing,irq,arg,
        (irq?r->engine.stats.irq_blocks:r->engine.stats.call_blocks)-blocks,
        elapsed,before.valid && after.valid && elapsed<UINT32_C(0x80000000));
#endif
    if(!before.valid || !after.valid || elapsed>=UINT32_C(0x80000000))++C.stats.clock_bad;
    else if(elapsed>C.stats.max_visit_ticks) {
        C.stats.max_visit_ticks=elapsed;
        C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][KUI_NATIVE_RA_AUX_MAX_CONTEXT]=
            visit_context;
    }
    return result;
}
