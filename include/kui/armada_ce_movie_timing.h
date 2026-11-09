/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARMADA_CE_MOVIE_TIMING_H
#define KUI_ARMADA_CE_MOVIE_TIMING_H
#include <stdbool.h>
#include <stdint.h>

/* Diagnostic only: where the time goes while CE streams Armada's opening
 * movie. Samples arrive at the game's own GD EXEC/CHECK polls; this logic
 * reads no hardware or memory. Times are CE milliseconds; K-UI's own time
 * (in its GD calls and its interrupt) arrives as free-running TMU0 tick
 * totals, converted with ticks_per_ms. Only differences are used, so the
 * totals need no reset. The window opens at the first sample whose last
 * request lies in MOVIE2.SFD and closes at the last such sample. */
#define KUI_ARMADA_MOVIE_FIRST_LBA 434063u /* MOVIE2.SFD, 60,860,416 bytes */
#define KUI_ARMADA_MOVIE_END_LBA   463780u
#define KUI_ARMADA_MOVIE_GAP_MS 90u        /* About one 16 KiB stereo ring. */
#define KUI_ARMADA_MOVIE_WINDOW_MS 1000u
#define KUI_ARMADA_MOVIE_HOT_PCT 50u

/* The low reader's stream counters, sampled with each poll: completed DMA
 * receptions, programmed receptions, unrecovered overruns, CMD18 starts,
 * CRC failures, token-search bytes, token searches paused at a slice,
 * in-place resumes, and (slow-after-overrun builds) 6.25 MHz receptions
 * that completed or overran. Only movie-window differences are reported. */
enum {
    KUI_ARMADA_MOVIE_DMA, KUI_ARMADA_MOVIE_POLLED, KUI_ARMADA_MOVIE_OVERRUNS,
    KUI_ARMADA_MOVIE_STARTS, KUI_ARMADA_MOVIE_CRC, KUI_ARMADA_MOVIE_TOKEN,
    KUI_ARMADA_MOVIE_YIELDS, KUI_ARMADA_MOVIE_REPAIRED, KUI_ARMADA_MOVIE_SLOW,
    KUI_ARMADA_MOVIE_SLOW_OVERRUNS, KUI_ARMADA_MOVIE_STREAM
};
#define KUI_ARMADA_MOVIE_TICK_MS 25u /* CE's clock step on this kernel */

enum {
    KUI_ARMADA_MOVIE_STARTED = 1u,  /* A movie request was seen. */
    KUI_ARMADA_MOVIE_LEFT = 2u,     /* A later sample's request was outside it. */
    KUI_ARMADA_MOVIE_CLOCK = 4u     /* TMU0 has a Pphi prescaler (report time). */
};

struct kui_armada_movie_input {
    uint32_t now_ms, lba, token, pending, sectors;
    uint32_t acc[4];          /* Call ticks, calls, interrupt ticks, interrupts. */
    uint32_t ticks_per_ms;
    uint32_t stream[KUI_ARMADA_MOVIE_STREAM];
};
struct kui_armada_movie_timing {
    uint32_t flags, start_ms, end_ms, last_ms, prev_pending, last_token;
    uint32_t base[4], end_acc[4], sectors0, sectors_end;
    uint32_t reads, samples, pend_ms, gap_max, gaps, pend_gap_max;
    uint32_t win_ms, win_acc[4], win_sectors, windows, peak_pct, hot, low_kbps;
    uint32_t stream0[KUI_ARMADA_MOVIE_STREAM], stream_end[KUI_ARMADA_MOVIE_STREAM];
    /* Overruns by CE clock step: steps that had any, the most in one step,
     * and the longest run of consecutive steps that had any (ms). */
    uint32_t prev_overruns, ovr_tick, ovr_tick_count, ovr_ticks, ovr_tick_max;
    uint32_t run_first, run_longest;
};

/* Pphi is 50 MHz; TCR TPSC 0..4 selects Pphi/4 .. Pphi/1024. */
static inline uint32_t kui_armada_movie_ticks_per_ms(uint32_t tcr) {
    uint32_t tpsc = tcr & 7u;
    return tpsc <= 4u ? 50000u >> (2u + 2u * tpsc) : 0u;
}

static inline bool kui_armada_movie_lba(uint32_t lba) {
    return lba >= KUI_ARMADA_MOVIE_FIRST_LBA && lba < KUI_ARMADA_MOVIE_END_LBA;
}

static inline void kui_armada_movie_sample(struct kui_armada_movie_timing *m,
                                           const struct kui_armada_movie_input *in) {
    bool movie = kui_armada_movie_lba(in->lba);
    if(!(m->flags & KUI_ARMADA_MOVIE_STARTED)) {
        if(!movie) return;
        m->flags |= KUI_ARMADA_MOVIE_STARTED;
        m->start_ms = m->end_ms = m->last_ms = m->win_ms = in->now_ms;
        for(unsigned i = 0; i < 4u; i++) m->base[i] = m->end_acc[i] = m->win_acc[i] = in->acc[i];
        m->sectors0 = m->sectors_end = m->win_sectors = in->sectors;
        for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++)
            m->stream0[i] = m->stream_end[i] = in->stream[i];
        m->prev_overruns = in->stream[KUI_ARMADA_MOVIE_OVERRUNS];
        m->last_token = in->token;
        m->reads = 1u;
        m->prev_pending = in->pending;
        m->low_kbps = UINT32_MAX;
        return;
    }
    uint32_t interval = in->now_ms - m->last_ms;
    if(movie) {
        m->samples++;
        if(interval > m->gap_max) m->gap_max = interval;
        if(interval >= KUI_ARMADA_MOVIE_GAP_MS) m->gaps++;
        /* The interval belongs to the state seen at its start. */
        if(m->prev_pending) {
            m->pend_ms += interval;
            if(interval > m->pend_gap_max) m->pend_gap_max = interval;
        }
        if(in->token != m->last_token) {m->reads++; m->last_token = in->token;}
        m->end_ms = in->now_ms;
        for(unsigned i = 0; i < 4u; i++) m->end_acc[i] = in->acc[i];
        m->sectors_end = in->sectors;
        for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++) m->stream_end[i] = in->stream[i];
        uint32_t overruns = in->stream[KUI_ARMADA_MOVIE_OVERRUNS] - m->prev_overruns;
        m->prev_overruns = in->stream[KUI_ARMADA_MOVIE_OVERRUNS];
        if(overruns) {
            uint32_t tick = in->now_ms;
            if(!m->ovr_ticks || tick != m->ovr_tick) {
                /* A new step; a run continues from the step just before. */
                if(!m->ovr_ticks || tick - m->ovr_tick > KUI_ARMADA_MOVIE_TICK_MS) m->run_first = tick;
                ++m->ovr_ticks;
                m->ovr_tick = tick;
                m->ovr_tick_count = 0;
            }
            m->ovr_tick_count += overruns;
            if(m->ovr_tick_count > m->ovr_tick_max) m->ovr_tick_max = m->ovr_tick_count;
            uint32_t run = tick - m->run_first + KUI_ARMADA_MOVIE_TICK_MS;
            if(run > m->run_longest) m->run_longest = run;
        }
        uint32_t span = in->now_ms - m->win_ms;
        if(span >= KUI_ARMADA_MOVIE_WINDOW_MS && in->ticks_per_ms) {
            uint32_t busy = ((in->acc[0] - m->win_acc[0]) + (in->acc[2] - m->win_acc[2])) /
                in->ticks_per_ms;
            uint32_t pct = busy * 100u / span;
            uint32_t kbps = (in->sectors - m->win_sectors) * 2000u / span;
            m->windows++;
            if(pct > m->peak_pct) m->peak_pct = pct;
            if(pct >= KUI_ARMADA_MOVIE_HOT_PCT) m->hot++;
            if(kbps < m->low_kbps) m->low_kbps = kbps;
            m->win_ms = in->now_ms;
            for(unsigned i = 0; i < 4u; i++) m->win_acc[i] = in->acc[i];
            m->win_sectors = in->sectors;
        }
    } else {
        m->flags |= KUI_ARMADA_MOVIE_LEFT;
    }
    m->last_ms = in->now_ms;
    m->prev_pending = in->pending;
}

/* The hexadecimal printer shows these as decimal digits. */
static inline uint32_t kui_armada_movie_decimal(uint32_t n) {
    if(n > 99999999u) n = 99999999u;
    uint32_t out = 0;
    for(unsigned shift = 0; shift < 32u; shift += 4u) {out |= (n % 10u) << shift; n /= 10u;}
    return out;
}

/* Six report rows of five values, in decimal digits:
 * MOVIE-MS SECTORS KB/S READS STATUS / CALL-MS IRQ-MS BUSY% PEND% PEAK% /
 * DMA-OK POLLED OVERRUN RESTART CRC-ERR / SLOW-OK SLOWOVR TOKBYTES
 * TOKYIELD REPAIRED / OVR-TICKS MAX/TICK RUN-MS GAPMAX LOW-KB/S /
 * CALLS IRQS SAMPLES HOT>50 1S-WINS */
#define KUI_ARMADA_MOVIE_ROWS 6u
static inline void kui_armada_movie_rows(const struct kui_armada_movie_timing *m,
                                         uint32_t ticks_per_ms, uint32_t rows[KUI_ARMADA_MOVIE_ROWS][5]) {
    uint32_t elapsed = m->end_ms - m->start_ms;
    uint32_t sectors = m->sectors_end - m->sectors0;
    uint32_t call_ms = ticks_per_ms ? (m->end_acc[0] - m->base[0]) / ticks_per_ms : 0u;
    uint32_t irq_ms = ticks_per_ms ? (m->end_acc[2] - m->base[2]) / ticks_per_ms : 0u;
    uint32_t d[KUI_ARMADA_MOVIE_STREAM];
    for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++) d[i] = m->stream_end[i] - m->stream0[i];
    uint32_t values[KUI_ARMADA_MOVIE_ROWS][5] = {
        {elapsed, sectors, elapsed ? sectors * 2000u / elapsed : 0u, m->reads,
         m->flags | (ticks_per_ms ? KUI_ARMADA_MOVIE_CLOCK : 0u)},
        {call_ms, irq_ms, elapsed ? (call_ms + irq_ms) * 100u / elapsed : 0u,
         elapsed ? m->pend_ms * 100u / elapsed : 0u, m->peak_pct},
        {d[KUI_ARMADA_MOVIE_DMA], d[KUI_ARMADA_MOVIE_POLLED], d[KUI_ARMADA_MOVIE_OVERRUNS],
         d[KUI_ARMADA_MOVIE_STARTS], d[KUI_ARMADA_MOVIE_CRC]},
        {d[KUI_ARMADA_MOVIE_SLOW], d[KUI_ARMADA_MOVIE_SLOW_OVERRUNS], d[KUI_ARMADA_MOVIE_TOKEN],
         d[KUI_ARMADA_MOVIE_YIELDS], d[KUI_ARMADA_MOVIE_REPAIRED]},
        {m->ovr_ticks, m->ovr_tick_max, m->run_longest, m->gap_max, m->windows ? m->low_kbps : 0u},
        {m->end_acc[1] - m->base[1], m->end_acc[3] - m->base[3], m->samples, m->hot, m->windows}};
    for(unsigned r = 0; r < KUI_ARMADA_MOVIE_ROWS; r++)
        for(unsigned c = 0; c < 5u; c++) rows[r][c] = kui_armada_movie_decimal(values[r][c]);
}
#endif
