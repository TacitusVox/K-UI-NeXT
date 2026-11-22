/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the Armada gameplay timing diagnostic: the window after the
 * intro movie, reader-side read timing with and without music grants, the
 * worker's steps, the one-second windows and the decimal report rows. */
#include "kui/armada_ce_play_timing.h"
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c) do { if(!(c)) {fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++;} } while(0)

static uint32_t from_decimal(uint32_t hex) {
    uint32_t n = 0, scale = 1;
    for(unsigned shift = 0; shift < 32u; shift += 4u) {n += ((hex >> shift) & 15u) * scale; scale *= 10u;}
    return n;
}

static const uint32_t TPM = 12500u;          /* Pphi/4 */
static const uint32_t TCOR = 312499u;        /* a 25 ms tick */

/* One poll: CE time, the request, sectors, K-UI's tick totals, the stream's
 * DMA and overrun counts, and the transport's totals. */
static void poll(struct kui_armada_play_timing *p, uint32_t ms, uint32_t lba, uint32_t token,
                 uint32_t pending, uint32_t sectors, uint32_t call_ticks, uint32_t irq_ticks,
                 uint32_t dma, uint32_t overruns, const uint32_t tr[KUI_ARMADA_PLAY_TRANSPORT]) {
    struct kui_armada_movie_input in = {ms, lba, token, pending, sectors,
        {call_ticks, 0u, irq_ticks, 0u}, TPM, {0}};
    in.stream[KUI_ARMADA_MOVIE_DMA] = dma;
    in.stream[KUI_ARMADA_MOVIE_OVERRUNS] = overruns;
    kui_armada_play_sample(p, &in, tr);
}

/* A bus event, reading the clock only when due (as the controller does). */
static unsigned clock_reads;
static void event(struct kui_armada_play_timing *p, uint32_t us, uint32_t token, bool pending,
                  bool reading, uint32_t sectors, bool produced, uint32_t grants) {
    if(!kui_armada_play_read_due(p, token, pending, reading, produced)) {
        /* Not due: the state must not change either way. */
        struct kui_armada_play_timing before = *p;
        kui_armada_play_read(p, us, token, pending, reading, sectors, produced, grants);
        CHECK(before.read_open == p->read_open && before.reads == p->reads &&
              before.solo == p->solo && before.shared == p->shared);
        return;
    }
    clock_reads++;
    kui_armada_play_read(p, us, token, pending, reading, sectors, produced, grants);
}

int main(void) {
    /* The clock: CE's count plus the ticks since its last tick. */
    CHECK(kui_armada_play_us(1000u, TCOR, false, TCOR, TPM) == 1000000u);
    CHECK(kui_armada_play_us(1000u, TCOR - 12500u, false, TCOR, TPM) == 1001000u);
    CHECK(kui_armada_play_us(1000u, 0u, false, TCOR, TPM) == 1024999u);
    /* An underflow CE has not counted yet: a whole tick more. */
    CHECK(kui_armada_play_us(1000u, TCOR - 125u, true, TCOR, TPM) == 1025010u);
    /* Across CE's tick the clock keeps rising. */
    CHECK(kui_armada_play_us(1025u, TCOR - 125u, false, TCOR, TPM) == 1025010u);
    CHECK(kui_armada_play_us(7u, 5u, false, TCOR, 0u) == 7000u);
    /* Wraps: differences only. */
    CHECK(kui_armada_play_us(4294967u + 10u, TCOR, false, TCOR, TPM) -
          kui_armada_play_us(4294967u, TCOR, false, TCOR, TPM) == 10000u);

    struct kui_armada_play_timing p = {0};
    uint32_t tr[KUI_ARMADA_PLAY_TRANSPORT] = {100u, 200u, 3u, 4u};
    /* Before and during the movie the window is closed; bus events and
     * worker steps are not counted. */
    poll(&p, 1000u, 548388u, 1u, 1u, 100u, 0u, 0u, 0u, 0u, tr);
    poll(&p, 2000u, 434100u, 2u, 1u, 200u, 0u, 0u, 0u, 0u, tr);
    CHECK(!(p.flags & KUI_ARMADA_PLAY_OPEN));
    CHECK(!kui_armada_play_read_due(&p, 3u, true, true, false));
    kui_armada_play_step(&p, 5000u, true, 1000u);
    CHECK(p.steps == 0u && p.work_us == 0u);
    poll(&p, 3000u, 434200u, 3u, 1u, 300u, 0u, 0u, 0u, 0u, tr);
    CHECK(!(p.flags & KUI_ARMADA_PLAY_OPEN));
    /* The first poll outside the movie opens it. A read running then (token
     * 4) is not timed: its start was not seen. */
    poll(&p, 10000u, 500000u, 4u, 1u, 1000u, 125000u, 250000u, 5000u, 50u, tr);
    CHECK(p.flags & KUI_ARMADA_PLAY_OPEN);
    CHECK(p.start_ms == 10000u && p.read_token == 4u && p.low_bytes == KUI_ARMADA_PLAY_NO_RESERVE);
    event(&p, 10010000u, 4u, true, true, 32u, true, 3u);
    CHECK(p.solo == 0u && p.shared == 0u && p.reads == 0u);

    /* Read 5: 32 sectors, no music grant while it ran: solo, 80 ms. Events
     * between its start and its last block are not due. */
    event(&p, 10100000u, 5u, true, true, 32u, false, 3u);
    CHECK(p.read_open && p.reads == 1u);
    event(&p, 10120000u, 5u, true, true, 32u, false, 3u);
    event(&p, 10180000u, 5u, true, true, 32u, true, 3u);
    CHECK(!p.read_open && p.solo == 1u && p.solo_us == 80000u && p.read_max_us == 80000u);
    /* Events after it, before the next REQUEST, are not due. */
    event(&p, 10190000u, 5u, true, true, 32u, false, 3u);
    event(&p, 10195000u, 5u, false, false, 0u, false, 3u);
    CHECK(p.reads == 1u);
    /* Read 6: 32 sectors with two music grants: shared, 130 ms. */
    event(&p, 10200000u, 6u, true, true, 32u, false, 3u);
    event(&p, 10250000u, 6u, true, true, 32u, false, 4u);
    event(&p, 10330000u, 6u, true, true, 32u, true, 5u);
    CHECK(p.shared == 1u && p.shared_us == 130000u && p.read_max_us == 130000u);
    /* Read 7: 16 sectors: counted, not timed. */
    event(&p, 10400000u, 7u, true, true, 16u, false, 5u);
    event(&p, 10440000u, 7u, true, true, 16u, true, 5u);
    CHECK(p.reads == 3u && p.solo == 1u && p.shared == 1u);
    /* Read 8 is aborted (no longer pending before its last block); read 9
     * replaces read 10 before its end. Neither is timed. */
    event(&p, 10500000u, 8u, true, true, 32u, false, 5u);
    event(&p, 10520000u, 8u, false, true, 32u, false, 5u);
    CHECK(!p.read_open);
    event(&p, 10600000u, 9u, true, true, 32u, false, 5u);
    event(&p, 10610000u, 10u, true, true, 32u, false, 5u);
    CHECK(p.read_open && p.read_token == 10u && p.reads == 6u);
    event(&p, 10690000u, 10u, true, true, 32u, true, 5u);
    CHECK(p.solo == 2u && p.solo_us == 160000u);
    /* A request that does not read (an audio control) is not a read. */
    event(&p, 10700000u, 11u, true, false, 0u, false, 5u);
    CHECK(!p.read_open && p.reads == 6u);
    CHECK(clock_reads == 11u);

    /* Worker steps in the window. */
    kui_armada_play_step(&p, 1200u, false, KUI_ARMADA_PLAY_NO_RESERVE);
    kui_armada_play_step(&p, 9000u, true, 150000u);
    kui_armada_play_step(&p, 300u, false, 300000u);
    CHECK(p.steps == 3u && p.work_us == 10500u && p.step_max_us == 9000u);
    CHECK(p.long_steps == 1u && p.urgent_steps == 1u && p.low_bytes == 150000u);
    CHECK(p.flags & KUI_ARMADA_PLAY_WORKER);

    /* A second later: a one-second window with 100 ms of calls, 150 ms of
     * interrupts and the worker's 10.5 ms. */
    uint32_t tr2[KUI_ARMADA_PLAY_TRANSPORT] = {100u + 344u, 200u + 600u, 3u + 2u, 4u + 3u};
    poll(&p, 11000u, 500100u, 11u, 0u, 1130u, 125000u + 100u * TPM, 250000u + 150u * TPM,
         5000u + 900u, 50u + 30u, tr2);
    CHECK(p.windows == 1u && p.peak_pct == 26u);
    /* A read was outstanding at the first poll: the whole second counts. */
    CHECK(p.pend_ms == 1000u);
    poll(&p, 12000u, 500100u, 11u, 0u, 1130u, 125000u + 100u * TPM, 250000u + 150u * TPM,
         5000u + 900u, 50u + 30u, tr2);
    CHECK(p.pend_ms == 1000u && p.windows == 2u);

    uint32_t rows[KUI_ARMADA_PLAY_ROWS][5];
    kui_armada_play_rows(&p, TPM, rows);
    CHECK(from_decimal(rows[0][0]) == 2000u);  /* window ms */
    CHECK(from_decimal(rows[0][1]) == 130u);   /* sectors */
    CHECK(from_decimal(rows[0][2]) == 130u);   /* KB/s */
    CHECK(from_decimal(rows[0][3]) == 6u);     /* reads begun */
    CHECK(from_decimal(rows[0][4]) == 15u);    /* movie, left, clock, worker */
    CHECK(from_decimal(rows[1][0]) == 100u && from_decimal(rows[1][1]) == 150u &&
          from_decimal(rows[1][2]) == 10u);    /* call, irq, worker ms */
    CHECK(from_decimal(rows[1][3]) == 13u && from_decimal(rows[1][4]) == 26u);
    CHECK(from_decimal(rows[2][0]) == 2u && from_decimal(rows[2][1]) == 80u &&
          from_decimal(rows[2][2]) == 1u && from_decimal(rows[2][3]) == 130u &&
          from_decimal(rows[2][4]) == 130u);   /* solo, solo ms, shared, shared ms, max */
    CHECK(from_decimal(rows[3][0]) == 344u && from_decimal(rows[3][1]) == 600u &&
          from_decimal(rows[3][2]) == 2u && from_decimal(rows[3][3]) == 3u &&
          from_decimal(rows[3][4]) == 50u);    /* transport deltas, pending % */
    CHECK(from_decimal(rows[4][0]) == 900u && from_decimal(rows[4][2]) == 30u);
    CHECK(from_decimal(rows[5][0]) == 3u && from_decimal(rows[5][1]) == 9000u &&
          from_decimal(rows[5][2]) == 1u && from_decimal(rows[5][3]) == 1u &&
          from_decimal(rows[5][4]) == 850u);   /* 150,000 bytes of PCM */
    /* No clock: times zero, no division by zero. */
    kui_armada_play_rows(&p, 0u, rows);
    CHECK(rows[1][0] == 0u && from_decimal(rows[0][4]) == 11u);

    /* A window that never opened reports zeros and no reserve. */
    struct kui_armada_play_timing q = {0};
    kui_armada_play_rows(&q, TPM, rows);
    for(unsigned r = 0; r < KUI_ARMADA_PLAY_ROWS; r++)
        for(unsigned c = 0; c < 5u; c++) CHECK(r == 0u && c == 4u ? from_decimal(rows[r][c]) == 4u : rows[r][c] == 0u);
    if(failures) return EXIT_FAILURE;
    puts("armada play timing: ok");
    return 0;
}
