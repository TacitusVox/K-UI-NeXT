/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the Armada movie timing diagnostic's window, gap, pending,
 * busy and one-second accounting, with its decimal report rows. */
#include "kui/armada_ce_movie_timing.h"
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c) do { if(!(c)) {fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++;} } while(0)

static uint32_t from_decimal(uint32_t hex) {
    uint32_t n = 0, scale = 1;
    for(unsigned shift = 0; shift < 32u; shift += 4u) {n += ((hex >> shift) & 15u) * scale; scale *= 10u;}
    return n;
}

int main(void) {
    CHECK(kui_armada_movie_ticks_per_ms(0) == 12500u);
    CHECK(kui_armada_movie_ticks_per_ms(1) == 3125u);
    CHECK(kui_armada_movie_ticks_per_ms(5) == 0u);
    CHECK(kui_armada_movie_decimal(1234u) == 0x1234u);
    CHECK(kui_armada_movie_decimal(4000000000u) == 0x99999999u);
    CHECK(!kui_armada_movie_lba(KUI_ARMADA_MOVIE_FIRST_LBA - 1u));
    CHECK(kui_armada_movie_lba(KUI_ARMADA_MOVIE_FIRST_LBA));
    CHECK(!kui_armada_movie_lba(KUI_ARMADA_MOVIE_END_LBA));

    struct kui_armada_movie_timing m = {0};
    const uint32_t tpm = 12500u;
    /* Boot reads outside the movie leave the window closed. */
    struct kui_armada_movie_input in = {1000u, 548388u, 1u, 1u, 500u, {7u, 3u, 9u, 4u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.flags == 0u);
    /* First movie poll opens the window; free-running totals are baselines. */
    in = (struct kui_armada_movie_input){2000u, 434100u, 2u, 1u, 600u,
        {UINT32_MAX - 100u, 10u, 5000u, 20u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.flags == KUI_ARMADA_MOVIE_STARTED && m.reads == 1u);
    /* 40 ms later, same read, still pending: 40 ms pending, no new read.
     * Call ticks wrap through zero; only differences count. */
    in = (struct kui_armada_movie_input){2040u, 434100u, 2u, 0u, 618u,
        {UINT32_MAX - 100u + 12500u * 4u, 14u, 5000u + 12500u * 2u, 25u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.pend_ms == 40u && m.reads == 1u && m.gap_max == 40u && m.gaps == 0u);
    /* A 100 ms gap with no read outstanding, then a new request. */
    in = (struct kui_armada_movie_input){2140u, 434118u, 3u, 1u, 618u,
        {UINT32_MAX - 100u + 12500u * 5u, 16u, 5000u + 12500u * 3u, 27u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.pend_ms == 40u && m.reads == 2u && m.gap_max == 100u && m.gaps == 1u);
    /* One second after the window opened: a one-second window closes. */
    in = (struct kui_armada_movie_input){3000u, 434130u, 3u, 1u, 1100u,
        {UINT32_MAX - 100u + 12500u * 205u, 40u, 5000u + 12500u * 103u, 90u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.windows == 1u);
    CHECK(m.peak_pct == 30u); /* (205 + 103) ms over 1000 ms */
    CHECK(m.low_kbps == 1000u); /* 500 sectors in one second */
    CHECK(m.pend_ms == 900u && m.pend_gap_max == 860u);
    /* After the intro the game reads elsewhere: the window stops growing. */
    in = (struct kui_armada_movie_input){9000u, 548400u, 4u, 1u, 9000u,
        {0u, 99u, 0u, 99u}, tpm, {0}};
    kui_armada_movie_sample(&m, &in);
    CHECK(m.flags == (KUI_ARMADA_MOVIE_STARTED | KUI_ARMADA_MOVIE_LEFT) && m.end_ms == 3000u);

    uint32_t rows[KUI_ARMADA_MOVIE_ROWS][5];
    kui_armada_movie_rows(&m, tpm, rows);
    CHECK(from_decimal(rows[0][0]) == 1000u);  /* movie window ms */
    CHECK(from_decimal(rows[0][1]) == 500u);   /* sectors */
    CHECK(from_decimal(rows[0][2]) == 1000u);  /* KB/s */
    CHECK(from_decimal(rows[0][3]) == 2u);     /* reads */
    CHECK(from_decimal(rows[0][4]) == 7u);     /* started, left, clock */
    CHECK(from_decimal(rows[1][0]) == 205u && from_decimal(rows[1][1]) == 103u);
    CHECK(from_decimal(rows[1][2]) == 30u && from_decimal(rows[1][3]) == 90u &&
          from_decimal(rows[1][4]) == 30u);    /* busy%, pending%, peak% */
    for(unsigned c = 0; c < 5u; c++) CHECK(rows[2][c] == 0u && rows[3][c] == 0u);
    CHECK(from_decimal(rows[4][0]) == 0u && from_decimal(rows[4][3]) == 860u &&
          from_decimal(rows[4][4]) == 1000u);  /* no overruns; gap max; low KB/s */
    CHECK(from_decimal(rows[5][0]) == 30u && from_decimal(rows[5][1]) == 70u &&
          from_decimal(rows[5][2]) == 3u && from_decimal(rows[5][3]) == 0u &&
          from_decimal(rows[5][4]) == 1u);     /* calls, irqs, samples, hot, windows */
    /* An unreported clock leaves times zero instead of dividing by zero. */
    kui_armada_movie_rows(&m, 0u, rows);
    CHECK(rows[1][0] == 0u && from_decimal(rows[0][4]) == 3u);

    /* Stream counters: only movie-window differences, and overruns grouped
     * by CE clock step (25 ms): steps 5000 and 5025 form one 50 ms run, a
     * gap, then step 5100 alone. Counts wrap like the hardware's. */
    struct kui_armada_movie_timing c = {0};
    struct kui_armada_movie_input s = {4975u, 440000u, 1u, 1u, 0u, {0}, tpm,
        {UINT32_MAX - 1u, 10u, 7u, 9u, 1u, 5000u, 2u, 3u, 0u, 0u}};
    kui_armada_movie_sample(&c, &s);
    static const uint32_t at[] = {5000u, 5000u, 5025u, 5050u, 5100u, 5100u};
    static const uint32_t add[] = {2u, 1u, 4u, 0u, 1u, 2u};
    for(unsigned i = 0; i < 6u; i++) {
        s.now_ms = at[i]; s.token++;
        s.stream[KUI_ARMADA_MOVIE_OVERRUNS] += add[i];
        s.stream[KUI_ARMADA_MOVIE_DMA] += 40u;
        s.stream[KUI_ARMADA_MOVIE_POLLED] += add[i];
        s.stream[KUI_ARMADA_MOVIE_STARTS] += add[i];
        s.stream[KUI_ARMADA_MOVIE_TOKEN] += 345u * add[i];
        s.stream[KUI_ARMADA_MOVIE_SLOW] += 3u;
        kui_armada_movie_sample(&c, &s);
    }
    kui_armada_movie_rows(&c, tpm, rows);
    CHECK(from_decimal(rows[2][0]) == 240u && from_decimal(rows[2][1]) == 10u &&
          from_decimal(rows[2][2]) == 10u && from_decimal(rows[2][3]) == 10u &&
          from_decimal(rows[2][4]) == 0u);     /* DMA wrapped through zero */
    CHECK(from_decimal(rows[3][0]) == 18u && from_decimal(rows[3][1]) == 0u &&
          from_decimal(rows[3][2]) == 3450u && rows[3][3] == 0u && rows[3][4] == 0u);
    CHECK(from_decimal(rows[4][0]) == 3u);     /* steps 5000, 5025, 5100 */
    CHECK(from_decimal(rows[4][1]) == 4u);     /* most in one step */
    CHECK(from_decimal(rows[4][2]) == 50u);    /* 5000-5025 run */
    if(failures) return EXIT_FAILURE;
    puts("armada movie timing: ok");
    return 0;
}
