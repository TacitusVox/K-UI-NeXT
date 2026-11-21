/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARMADA_CE_PLAY_TIMING_H
#define KUI_ARMADA_CE_PLAY_TIMING_H
#include "kui/armada_ce_movie_timing.h"

/* Diagnostic only: what CD music On costs Armada's gameplay. The movie
 * timing's window still runs, only to find the movie's end: this window
 * opens at the first GD poll outside MOVIE2.SFD after the movie was seen
 * (the movie timing's LEFT state) and stays open until the report. The logic
 * reads no hardware or memory. Three sources feed it:
 * - the game's GD EXEC/CHECK polls (CE milliseconds): K-UI's time in its GD
 *   calls and interrupt, the game's sectors, the card stream's counters and
 *   the music transport's;
 * - the low reader's bus events (microseconds, from CE's count and TMU0):
 *   each game read from its REQUEST to the event after its last card block,
 *   and whether the music was granted the card while it ran;
 * - the music worker's steps (microseconds): each step's elapsed time less
 *   K-UI's GD calls and interrupts inside it, the music's urgency and its
 *   reserve of fresh PCM. */
#define KUI_ARMADA_PLAY_READ_SECTORS 32u   /* Armada's streamed 64 KiB reads */
#define KUI_ARMADA_PLAY_LONG_STEP_US 8000u
#define KUI_ARMADA_PLAY_PCM_BYTES_PER_S 176400u
#define KUI_ARMADA_PLAY_NO_RESERVE UINT32_MAX /* A step while the music is not playing. */

/* The music transport's free-running totals: card blocks for the music and
 * for the game, music grants while a game read was active, and game reads
 * that took the card from the music. Only window differences are reported. */
enum {
    KUI_ARMADA_PLAY_MUSIC_BLOCKS, KUI_ARMADA_PLAY_GAME_BLOCKS, KUI_ARMADA_PLAY_GRANTS,
    KUI_ARMADA_PLAY_PREEMPTIONS, KUI_ARMADA_PLAY_TRANSPORT
};
enum {
    KUI_ARMADA_PLAY_OPEN = 1u,   /* The window is open. */
    KUI_ARMADA_PLAY_WORKER = 2u  /* A worker step was timed in it. */
};

struct kui_armada_play_timing {
    struct kui_armada_movie_timing movie;
    uint32_t flags, start_ms, end_ms, last_ms, prev_pending, pend_ms;
    uint32_t base[4], end_acc[4], sectors0, sectors_end;
    uint32_t stream0[KUI_ARMADA_MOVIE_STREAM], stream_end[KUI_ARMADA_MOVIE_STREAM];
    uint32_t tr0[KUI_ARMADA_PLAY_TRANSPORT], tr_end[KUI_ARMADA_PLAY_TRANSPORT];
    /* One-second windows of K-UI's time: calls, interrupts and worker. */
    uint32_t work_us, win_ms, win_acc[4], win_work_us, windows, peak_pct;
    /* The game read being timed: its token, sectors, start and the music's
     * grants then; and the 32-sector reads it timed, without and with music
     * grants while they ran. reads counts every read begun in the window. */
    uint32_t read_open, read_token, read_sectors, read_begin_us, read_grants, reads;
    uint32_t solo, solo_us, shared, shared_us, read_max_us;
    /* Worker steps: count, longest, those of LONG_STEP_US or more, those
     * while the music was urgent, and the least fresh PCM a playing step saw. */
    uint32_t steps, step_max_us, long_steps, urgent_steps, low_bytes;
};

/* CE's millisecond count and TMU0's count as microseconds. TMU0 counts down
 * from tcor; underflow is TCR0's UNF, set while CE has not yet counted the
 * tick (the count is then the one after the reload). Only differences are
 * used: they hold for windows under 71 minutes. */
static inline uint32_t kui_armada_play_us(uint32_t ms, uint32_t count, bool underflow,
                                          uint32_t tcor, uint32_t ticks_per_ms) {
    if(!ticks_per_ms) return ms * 1000u;
    uint32_t phase = (count <= tcor ? tcor - count : 0u) + (underflow ? tcor + 1u : 0u);
    uint32_t whole = phase / ticks_per_ms, part = phase % ticks_per_ms;
    return ms * 1000u + whole * 1000u + part * 1000u / ticks_per_ms;
}

/* A GD EXEC/CHECK poll: the movie timing's input, with the transport's
 * totals. Opens the window at the first poll after the game left the movie. */
static inline void kui_armada_play_sample(struct kui_armada_play_timing *p,
                                          const struct kui_armada_movie_input *in,
                                          const uint32_t transport[KUI_ARMADA_PLAY_TRANSPORT]) {
    kui_armada_movie_sample(&p->movie, in);
    if(!(p->flags & KUI_ARMADA_PLAY_OPEN)) {
        if(!(p->movie.flags & KUI_ARMADA_MOVIE_LEFT)) return;
        p->flags |= KUI_ARMADA_PLAY_OPEN;
        p->start_ms = p->end_ms = p->last_ms = p->win_ms = in->now_ms;
        for(unsigned i = 0; i < 4u; i++) p->base[i] = p->end_acc[i] = p->win_acc[i] = in->acc[i];
        p->sectors0 = p->sectors_end = in->sectors;
        for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++)
            p->stream0[i] = p->stream_end[i] = in->stream[i];
        for(unsigned i = 0; i < KUI_ARMADA_PLAY_TRANSPORT; i++) p->tr0[i] = p->tr_end[i] = transport[i];
        p->win_work_us = p->work_us;
        p->prev_pending = in->pending;
        /* A read already running is not timed: its start was not seen. */
        p->read_token = in->token;
        p->low_bytes = KUI_ARMADA_PLAY_NO_RESERVE;
        return;
    }
    uint32_t interval = in->now_ms - p->last_ms;
    /* The interval belongs to the state seen at its start. */
    if(p->prev_pending) p->pend_ms += interval;
    p->end_ms = in->now_ms;
    for(unsigned i = 0; i < 4u; i++) p->end_acc[i] = in->acc[i];
    p->sectors_end = in->sectors;
    for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++) p->stream_end[i] = in->stream[i];
    for(unsigned i = 0; i < KUI_ARMADA_PLAY_TRANSPORT; i++) p->tr_end[i] = transport[i];
    uint32_t span = in->now_ms - p->win_ms;
    if(span >= KUI_ARMADA_MOVIE_WINDOW_MS && in->ticks_per_ms) {
        uint32_t busy = ((in->acc[0] - p->win_acc[0]) + (in->acc[2] - p->win_acc[2])) / in->ticks_per_ms +
            (p->work_us - p->win_work_us) / 1000u;
        uint32_t pct = busy * 100u / span;
        p->windows++;
        if(pct > p->peak_pct) p->peak_pct = pct;
        p->win_ms = in->now_ms;
        for(unsigned i = 0; i < 4u; i++) p->win_acc[i] = in->acc[i];
        p->win_work_us = p->work_us;
    }
    p->last_ms = in->now_ms;
    p->prev_pending = in->pending;
}

/* A bus event: the service's request (token, pending, whether it reads, its
 * sectors), produced when this event follows the feed of that request's
 * last card block, and the transport's grant total. read_due says whether
 * the event starts or ends a timed read, so the clock is read only then. */
static inline bool kui_armada_play_read_due(const struct kui_armada_play_timing *p, uint32_t token,
                                            bool pending, bool reading, bool produced) {
    if(!(p->flags & KUI_ARMADA_PLAY_OPEN)) return false;
    if(p->read_open) return produced || !pending || token != p->read_token;
    return pending && reading && !produced && token != p->read_token;
}
static inline void kui_armada_play_read(struct kui_armada_play_timing *p, uint32_t now_us,
                                        uint32_t token, bool pending, bool reading, uint32_t sectors,
                                        bool produced, uint32_t grants) {
    if(!(p->flags & KUI_ARMADA_PLAY_OPEN)) return;
    if(p->read_open && (produced || !pending || token != p->read_token)) {
        /* Only a read seen to its last block is timed; one aborted or
         * replaced is dropped. */
        if(produced && token == p->read_token && p->read_sectors == KUI_ARMADA_PLAY_READ_SECTORS) {
            uint32_t took = now_us - p->read_begin_us;
            if(grants != p->read_grants) {p->shared++; p->shared_us += took;}
            else {p->solo++; p->solo_us += took;}
            if(took > p->read_max_us) p->read_max_us = took;
        }
        p->read_open = 0;
    }
    if(!p->read_open && pending && reading && !produced && token != p->read_token) {
        p->read_open = 1u;
        p->read_token = token;
        p->read_sectors = sectors;
        p->read_begin_us = now_us;
        p->read_grants = grants;
        p->reads++;
    }
}

/* A worker step: its time less K-UI's GD calls and interrupts inside it,
 * whether the music was urgent after it, and its fresh PCM (NO_RESERVE
 * while the music is not playing). */
static inline void kui_armada_play_step(struct kui_armada_play_timing *p, uint32_t net_us,
                                        bool urgent, uint32_t reserve_bytes) {
    if(!(p->flags & KUI_ARMADA_PLAY_OPEN)) return;
    p->flags |= KUI_ARMADA_PLAY_WORKER;
    p->steps++;
    p->work_us += net_us;
    if(net_us > p->step_max_us) p->step_max_us = net_us;
    if(net_us >= KUI_ARMADA_PLAY_LONG_STEP_US) p->long_steps++;
    if(urgent) p->urgent_steps++;
    if(reserve_bytes < p->low_bytes) p->low_bytes = reserve_bytes;
}

/* Six report rows of five values, in decimal digits:
 * PLAY-MS SECTORS KB/S READS STATUS / CALL-MS IRQ-MS WORK-MS BUSY% PEAK% /
 * SOLO SOLO-MS SHARED SHARED-MS MAX-MS / MUSICBLK GAMEBLK GRANTS PREEMPT
 * PEND% / DMA-OK POLLED OVERRUN RESTART SLOW-OK / STEPS MAX-US OVER8MS
 * URGENT LOW-MS. STATUS: 1 movie seen, 2 window open, 4 clock valid, 8
 * worker timed (expect 15). */
#define KUI_ARMADA_PLAY_ROWS 6u
static inline void kui_armada_play_rows(const struct kui_armada_play_timing *p, uint32_t ticks_per_ms,
                                        uint32_t rows[KUI_ARMADA_PLAY_ROWS][5]) {
    uint32_t elapsed = p->end_ms - p->start_ms;
    uint32_t sectors = p->sectors_end - p->sectors0;
    uint32_t call_ms = ticks_per_ms ? (p->end_acc[0] - p->base[0]) / ticks_per_ms : 0u;
    uint32_t irq_ms = ticks_per_ms ? (p->end_acc[2] - p->base[2]) / ticks_per_ms : 0u;
    uint32_t work_ms = p->work_us / 1000u;
    uint32_t d[KUI_ARMADA_MOVIE_STREAM], tr[KUI_ARMADA_PLAY_TRANSPORT];
    for(unsigned i = 0; i < KUI_ARMADA_MOVIE_STREAM; i++) d[i] = p->stream_end[i] - p->stream0[i];
    for(unsigned i = 0; i < KUI_ARMADA_PLAY_TRANSPORT; i++) tr[i] = p->tr_end[i] - p->tr0[i];
    uint32_t status = (p->movie.flags & (KUI_ARMADA_MOVIE_STARTED | KUI_ARMADA_MOVIE_LEFT)) |
        (ticks_per_ms ? KUI_ARMADA_MOVIE_CLOCK : 0u) | ((p->flags & KUI_ARMADA_PLAY_WORKER) ? 8u : 0u);
    uint32_t values[KUI_ARMADA_PLAY_ROWS][5] = {
        {elapsed, sectors, elapsed ? sectors * 2000u / elapsed : 0u, p->reads, status},
        {call_ms, irq_ms, work_ms, elapsed ? (call_ms + irq_ms + work_ms) * 100u / elapsed : 0u, p->peak_pct},
        {p->solo, p->solo ? p->solo_us / p->solo / 1000u : 0u,
         p->shared, p->shared ? p->shared_us / p->shared / 1000u : 0u, p->read_max_us / 1000u},
        {tr[KUI_ARMADA_PLAY_MUSIC_BLOCKS], tr[KUI_ARMADA_PLAY_GAME_BLOCKS], tr[KUI_ARMADA_PLAY_GRANTS],
         tr[KUI_ARMADA_PLAY_PREEMPTIONS], elapsed ? p->pend_ms * 100u / elapsed : 0u},
        {d[KUI_ARMADA_MOVIE_DMA], d[KUI_ARMADA_MOVIE_POLLED], d[KUI_ARMADA_MOVIE_OVERRUNS],
         d[KUI_ARMADA_MOVIE_STARTS], d[KUI_ARMADA_MOVIE_SLOW]},
        {p->steps, p->step_max_us, p->long_steps, p->urgent_steps,
         p->low_bytes == KUI_ARMADA_PLAY_NO_RESERVE ? 0u :
             p->low_bytes / KUI_ARMADA_PLAY_PCM_BYTES_PER_S * 1000u +
             p->low_bytes % KUI_ARMADA_PLAY_PCM_BYTES_PER_S * 1000u / KUI_ARMADA_PLAY_PCM_BYTES_PER_S}};
    for(unsigned r = 0; r < KUI_ARMADA_PLAY_ROWS; r++)
        for(unsigned c = 0; c < 5u; c++) rows[r][c] = kui_armada_movie_decimal(values[r][c]);
}
#endif
