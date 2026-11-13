/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_NATIVE_READAHEAD_H
#define KUI_NATIVE_READAHEAD_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "kui/retail_cursor.h"
#include "sci_stream.h"
#include "retail_async.h"
#include "kui/native_readahead_layout.h"
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
#include "kui/toy_movie_trace.h"
#endif
#if KUI_TOY_FMV_COMPARE
#include "kui/toy_fmv_compare.h"
#endif
enum kui_native_ra_event {
    KUI_NATIVE_RA_DELIVER=1u,
    KUI_NATIVE_RA_TICK=2u,
    KUI_NATIVE_RA_QUIESCE=3u,
    KUI_NATIVE_RA_RESUME=4u,
    KUI_NATIVE_RA_RESET=5u,
    KUI_NATIVE_RA_CAPTURE_START=6u,
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    KUI_NATIVE_RA_MOVIE_COMPLETE=7u,
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    KUI_NATIVE_RA_CALIBRATE=8u,
#endif
#if KUI_TOY_FMV_COMPARE
    KUI_NATIVE_RA_FMV_REQUEST=9u,
    KUI_NATIVE_RA_FMV_ACK=10u,
#endif
};
/* TICK's argument limits optional work without changing the visit ABI or
 * stopping a partial block. Zero retains the historical two-block policy;
 * ONE permits at most one complete PIO/CRC block. Other TICK arguments refuse.
 * DELIVER keeps its separate wait argument; other events require zero. */
#define KUI_NATIVE_RA_TICK_DEFAULT 0u
#define KUI_NATIVE_RA_TICK_ONE_BLOCK 1u
struct kui_retail_cursor;
/* Every pointer is proved against the immutable low resident by the stage.
 * The auxiliary code invokes these standard C entries on its own stack. */
struct kui_native_ra_low_ops {
    uint32_t magic,version,bytes;
    kui_retail_async_region_type **region_storage;
    void (*deliver)(uint32_t wait);
    void (*hook)(void);
    void (*unhook)(void);
    enum kui_sci_stream_result (*fetch)(uint32_t lba,uint32_t token_limit,bool polled);
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    bool (*cold_bus)(bool acquire);
#else
    enum kui_sci_stream_result (*poll)(bool interrupt);
#endif
    const uint8_t *(*take)(uint32_t lba,enum kui_sci_stream_result *result);
    enum kui_sci_stream_result (*stop)(void);
    bool (*busy)(void);
    enum kui_game_result (*feed)(struct kui_retail_cursor *,const uint8_t block[512]);
};
typedef uint32_t (*kui_native_ra_visit)(uint32_t event,uint32_t arg,
    kui_retail_async_region_type *region,uint32_t reserved);
struct kui_native_ra_config {
    uint32_t version,bytes;
    kui_retail_async_region_type *region;
    const struct kui_native_ra_low_ops *ops;
    uint8_t *data;
    uint32_t data_bytes,lease,lease_bytes;
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    struct kui_toy_movie_range movie_ranges[4];
#endif
#if defined(KUI_TOY_SUITE_MODE)
    struct kui_toy_suite_seed suite_seed;
    uint32_t suite_source;
#endif
};
/* The relocation domain includes exports, context, code and private stack;
 * the separately owned data arena is passed by the cold stage, never relocated. */
struct kui_native_ra_exports {
    uint32_t magic,version,bytes;
    kui_native_ra_visit visit;
    int (*init)(const struct kui_native_ra_config *);
    void *context;
    uint32_t image_end,stack_bottom,stack_top,memory_end;
    uint32_t reserved[6];
};
struct kui_native_ra_stats {
    uint32_t hits,misses,hit_requests,producer_blocks,producer_visits;
    uint32_t crc_errors,token_errors,abandoned,seeks,quiesces,resumes;
    uint32_t full,extent_ends,heartbeat_visits,irq_visits,guard_faults;
    uint32_t max_visit_ticks,clock_bad;
};
/* Appended diagnostic prefix: the existing statistics and guard word keep
 * their original offsets. These observations never authorize recovery. */
enum kui_native_ra_failure_stage {
    KUI_NATIVE_RA_STAGE_NONE=0u,KUI_NATIVE_RA_STAGE_POLL,
    KUI_NATIVE_RA_STAGE_TAKE,KUI_NATIVE_RA_STAGE_APPEND,
    KUI_NATIVE_RA_STAGE_FETCH,KUI_NATIVE_RA_STAGE_RESUME,
    KUI_NATIVE_RA_STAGE_QUIESCE,KUI_NATIVE_RA_STAGE_COUNT
};
#define KUI_NATIVE_RA_RESULT_COUNT 8u
/* Under __kui_native_readahead_pio_auxiliary, the otherwise unused NONE
 * histogram row contains producer-only metrics. Failure rows1..6 retain
 * their original meaning; capture-start clears these metrics too. The
 * distinct pair profile rejects zero elapsed time for completed512B work
 * as an invalid per-attempt interval and never admits a second block then. */
enum kui_native_ra_pio_metric {
    KUI_NATIVE_RA_PIO_ATTEMPTS=0u,KUI_NATIVE_RA_PIO_MAX_TICKS,
    KUI_NATIVE_RA_PIO_CLOCK_BAD,KUI_NATIVE_RA_PIO_SECOND_ATTEMPTS,
    KUI_NATIVE_RA_PIO_SECOND_TIME_SKIPS,KUI_NATIVE_RA_PIO_MAX_SINGLE_TICKS,
    KUI_NATIVE_RA_PIO_WINDOW_OVERRUNS,
    /* Context of stats.max_visit_ticks, recorded only when that maximum
     * increases, using the same existing before/after samples. Bits0..7:
     * event; bits8..23: low16 bits of event argument; bits24..31: entry in_irq.
     * Equal/invalid intervals retain the previous context. This slot is
     * diagnostic only; it never grants work or changes the visit ABI. */
    KUI_NATIVE_RA_AUX_MAX_CONTEXT
};
#define KUI_NATIVE_RA_PIO_BLOCK_LIMIT 2u
#define KUI_NATIVE_RA_PIO_WINDOW_TICKS 2344u
#define KUI_NATIVE_RA_PIO_RESERVE_TICKS 1200u
#define KUI_NATIVE_RA_PIO_SECOND_LIMIT_TICKS \
    (KUI_NATIVE_RA_PIO_WINDOW_TICKS-KUI_NATIVE_RA_PIO_RESERVE_TICKS)
enum kui_native_ra_skip {
    KUI_NATIVE_RA_SKIP_QUIESCED=0u,KUI_NATIVE_RA_SKIP_ABANDONED,
    KUI_NATIVE_RA_SKIP_NO_SEED,KUI_NATIVE_RA_SKIP_LOAN,
    KUI_NATIVE_RA_SKIP_ACTIVE,KUI_NATIVE_RA_SKIP_PENDING,
    KUI_NATIVE_RA_SKIP_FAILED,KUI_NATIVE_RA_SKIP_NOT_OPENED,
    KUI_NATIVE_RA_SKIP_PORT_FAULT,KUI_NATIVE_RA_SKIP_BUSY,
    KUI_NATIVE_RA_SKIP_GUEST_DMA,KUI_NATIVE_RA_SKIP_STREAM_POSITION,
    KUI_NATIVE_RA_SKIP_DEFERRED,KUI_NATIVE_RA_SKIP_SPEC_OWNED,
    KUI_NATIVE_RA_SKIP_COUNT
};
enum kui_native_ra_snapshot_flag {
    KUI_NATIVE_RA_SNAP_ACTIVE=1u,KUI_NATIVE_RA_SNAP_PENDING=2u,
    KUI_NATIVE_RA_SNAP_EXECUTING=4u,KUI_NATIVE_RA_SNAP_FAILED=8u,
    KUI_NATIVE_RA_SNAP_SPEC_OWNED=16u,KUI_NATIVE_RA_SNAP_DEFERRED=32u,
    KUI_NATIVE_RA_SNAP_QUIESCED=64u,KUI_NATIVE_RA_SNAP_ABANDONED=128u,
    KUI_NATIVE_RA_SNAP_HAVE_SEED=256u,KUI_NATIVE_RA_SNAP_ACQUIRED=512u,
    KUI_NATIVE_RA_SNAP_PORT_FAULT=1024u,KUI_NATIVE_RA_SNAP_OPENED=2048u,
    KUI_NATIVE_RA_SNAP_IRQ=4096u
};
struct kui_native_ra_hw_snapshot {
    uint32_t sar,dar,tcr,chcr,dmaor;
    /* SMR | BRR<<8 | SCR<<16 | SSR<<24. No RDR read occurs. */
    uint32_t sci;
    /* SPTR | STBCR<<8 | PDTR<<16. */
    uint32_t pins,tmu;
};
struct kui_native_ra_failure_snapshot {
    uint32_t stage,result,event,flags,token,cursor_block;
    uint32_t stream_state,stream_position,cache_base,cache_count,cache_end;
    /* Physical block attempted by the recorded operation. FETCH records
     * its next LBA before that LBA becomes the owned speculative block. */
    uint32_t operation_lba;
    /* Native loan state in bits0..15; SCI stream.unrepaired in bits16..23.
     * This is the transport's existing failed single-byte repair count,
     * not the number of repairs attempted during this capture. */
    uint32_t loan_state;
    /* ready[0] | ready[1]<<8 | kept<<16 | fill<<24. */
    uint32_t rx_flags,ready_lba[2],kept_lba,arrived;
    /* lost[0] | lost[1]<<16; indexes remain meaningful if fill is invalid. */
    uint32_t lost;
    struct kui_native_ra_hw_snapshot pre,post;
    uint32_t post_stream_state,post_stream_position;
};
struct kui_native_ra_diagnostics {
    uint32_t failures;
    uint32_t stage_result[KUI_NATIVE_RA_STAGE_COUNT][KUI_NATIVE_RA_RESULT_COUNT];
    uint32_t skipped[KUI_NATIVE_RA_SKIP_COUNT];
    struct kui_native_ra_failure_snapshot first,last;
};
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
struct kui_toy_burst_cut {
    uint32_t tcr,chcr2,c2dst,g2adst;
};
struct kui_toy_burst_metrics {
    /* Arms and byte totals refer to owned DMA starts and stable retires. */
    uint32_t arms,requested,received,dma_short,dma_full;
    uint32_t checked,parse_full_cut,bad_token,bad_crc,timeout,reset,usable,command_error;
    /* Stable receive remainder and concurrent register samples after retire.
     * The latter are observations only; they cannot establish bus ownership. */
    struct kui_toy_burst_cut first_short,last_short;
};
#endif
#if KUI_TOY_SUITE_MODE == 28
struct kui_toy_burst_rate {
    uint32_t arms, dma_full, checked;
};
#endif
struct kui_native_ra_public {
    uint32_t busy;
    struct kui_native_ra_stats stats;
    struct kui_native_ra_diagnostics diagnostics;
#if KUI_NATIVE_TOY_MOVIE_CAPTURE
    struct kui_toy_movie_stats movies[2];
    uint32_t movie_mask;
#endif
#if KUI_NATIVE_TOY_PLAYBACK_CAPTURE
    struct kui_toy_playback_stats playback[2];
#endif
#if defined(KUI_TOY_SUITE_MODE)
    uint32_t suite[5], suite_source;
#endif
#if KUI_TOY_SUITE_MODE == 27 || KUI_TOY_SUITE_MODE == 28
    uint32_t burst_quiet_status,burst_quiet_lba,burst_quiet_end;
    uint32_t burst_quiet_late_lba,burst_quiet_late_end;
    struct kui_toy_burst_metrics burst_quiet,burst_live,burst_match;
#endif
#if KUI_TOY_SUITE_MODE == 28
    struct kui_toy_burst_rate burst_fast,burst_slow;
#endif
#if KUI_TOY_FMV_COMPARE
    struct kui_toy_fmv_trace fmv_trace;
#endif
};
_Static_assert(sizeof(struct kui_native_ra_hw_snapshot)==32u,"read-ahead hardware snapshot ABI");
_Static_assert(sizeof(struct kui_native_ra_failure_snapshot)==148u,"read-ahead failure snapshot ABI");
_Static_assert(offsetof(struct kui_native_ra_public,diagnostics)==76u,"read-ahead diagnostic prefix ABI");
#if !KUI_NATIVE_TOY_MOVIE_CAPTURE
_Static_assert(sizeof(struct kui_native_ra_public)==656u,"read-ahead diagnostic prefix size");
#endif
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct kui_native_ra_low_ops)==52u,"read-ahead low operations ABI");
_Static_assert(sizeof(struct kui_native_ra_exports)==64u,"read-ahead auxiliary exports ABI");
#if defined(KUI_TOY_SUITE_MODE)
_Static_assert(sizeof(struct kui_native_ra_config)==84u,"Toy suite configuration ABI");
#elif KUI_NATIVE_TOY_MOVIE_CAPTURE
_Static_assert(sizeof(struct kui_native_ra_config)==64u,"movie trace configuration ABI");
#else
_Static_assert(sizeof(struct kui_native_ra_config)==32u,"read-ahead configuration ABI");
#endif
#endif
#endif
