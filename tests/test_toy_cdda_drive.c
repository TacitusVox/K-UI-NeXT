/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 84 (DRIVE, KEEP): the drive plays. The real disc service
 * (src/core/retail_gd.c) and the real player (src/loader/toy_cdda_player.c),
 * called in the low resident's order (src/loader/retail_async.c,
 * kui_retail_async_call and kui_retail_async_after): the auxiliary's delivery
 * visit, which hands the player the call first, delivers what the background
 * reader's engine has, and hands the player the visit's end; then report(),
 * which publishes the engine's progress of the read pending; then the
 * service's answer. A read the service accepts starts at once, with a visit
 * and a report() of its own. TMU0 is a scripted count. The call sequences are
 * the games' own, as their gdFs library makes them (docs/cd-audio-corpus.md):
 * REQUEST and EXEC at once, CHECK later, the library's REQ_STAT after PLAY and
 * PAUSE, and its resume by RELEASE or by the saved PLAY.
 *
 * A twin service without a player answers every call too and is published the
 * same progress: where no play stands, every answer and every word of the
 * service must be the twin's; through KEEP's reads, every word but the drive
 * status. Host model only; nothing here ran on a console. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/vmu_native_background.h"
#ifndef KUI_RETAIL_ASYNC
#define KUI_TOY_CDDA_LAYOUT_VIEW 1
#define KUI_RETAIL_ASYNC 1
#define KUI_RETAIL_GD_ASYNC 1
#define KUI_RETAIL_TRANSPORT 1
#define KUI_SCI_SD_NO_BLOCK 1
#define KUI_GENERAL_SCI_RESUME_FIX 1
#endif
#include "retail_async.h"
#include "kui/native_readahead.h"
#ifdef KUI_TOY_CDDA_LAYOUT_VIEW
#undef KUI_RETAIL_ASYNC
#undef KUI_RETAIL_GD_ASYNC
#undef KUI_RETAIL_TRANSPORT
#undef KUI_SCI_SD_NO_BLOCK
#undef KUI_GENERAL_SCI_RESUME_FIX
#undef KUI_TOY_CDDA_LAYOUT_VIEW
#endif
#include "kui/retail_gd.h"
#include "kui/toy_cdda_player.h"

#if !KUI_TOY_CDDA_DRIVE
#error "This is DRIVE's test"
#endif
#ifndef KUI_TOY_CDDA_KEEP
#define KUI_TOY_CDDA_KEEP 0
#endif

static unsigned long checks;
#define CHECK(x) do { ++checks; if(!(x)) { \
    fprintf(stderr, "test_toy_cdda_drive.c:%d: %s\n", __LINE__, #x); exit(1); } } while(0)

/* Guest memory: one bank for the service the player answers through, one for its twin. */
#define BEGIN UINT32_C(0x8c010000)
#define PARAM (BEGIN + 0x000u)
#define OUT (BEGIN + 0x100u)
#define STAT (BEGIN + 0x200u)
#define BUFFER (BEGIN + 0x1000u)
static uint8_t bank[2][0x40000];
static int refuse_read;
static uint8_t *map(void *context, uint32_t address, uint32_t bytes, int writing) {
    (void)writing;
    uint8_t *b = context;
    if(address < BEGIN || address - BEGIN > sizeof(bank[0]) || bytes > sizeof(bank[0]) - (address - BEGIN))
        return NULL;
    return b + (address - BEGIN);
}
static int range(void *context, uint32_t lba, uint32_t count, uint32_t bytes) {
    (void)context; (void)bytes;
    return lba >= 45000u && lba + count <= 60000u ? 0 : -1;
}
static int reader(void *context, uint32_t lba, uint32_t count, uint32_t bytes, void *out) {
    (void)context;
    if(refuse_read) return -1;
    for(uint32_t i = 0; i < count; ++i) memset((uint8_t *)out + i * bytes, (int)((lba + i) & 255u), bytes);
    return 0;
}
static void put(int which, uint32_t address, uint32_t value) {
    uint8_t *p = map(bank[which], address, 4u, 1); CHECK(p); memcpy(p, &value, 4u);
}
static uint32_t get(int which, uint32_t address) {
    uint32_t v; uint8_t *p = map(bank[which], address, 4u, 0); CHECK(p); memcpy(&v, p, 4u); return v;
}
static uint8_t get8(int which, uint32_t address) { return *map(bank[which], address, 1u, 0); }

/* A GD disc like the corpus's: data, audio in the low-density area, data at
 * 45000, audio tracks (one behind a gap, as the map has no pregaps), data. */
enum { T1, T2, T3, T4, T5, T6, T7, TRACKS };
static const union kui_retail_slot tracks[TRACKS] = {
    {.track = {.start_lba = 0u, .end_lba = 600u, .control = 4u}},
    {.track = {.start_lba = 600u, .end_lba = 1200u, .control = 0u}},
    {.track = {.start_lba = 45000u, .end_lba = 60000u, .control = 4u}},
    {.track = {.start_lba = 60000u, .end_lba = 70000u, .control = 0u}},
    {.track = {.start_lba = 70150u, .end_lba = 71000u, .control = 0u}},
    {.track = {.start_lba = 71000u, .end_lba = 72500u, .control = 0u}},
    {.track = {.start_lba = 72500u, .end_lba = 80000u, .control = 4u}},
};
static kui_retail_async_region_type R;
static struct kui_retail_gd twin;
/* twin_but_status: the twin is compared in every word but the drive status. */
static bool twin_on, twin_but_status;

/* The background reader's engine (src/loader/retail_async.c): the read it
 * serves is the one whose token it took at its start (R.engine.token). A
 * delivery visit brings its complete sectors up to delivered, and its
 * failure to failure. started: the low reader starts a read accepted. */
static uint32_t delivered, failure;
static bool started;

/* TMU0 under the Toy profile, counting down. */
static uint32_t tcnt = UINT32_C(0x12345678), frqcr = 0x0e0au, tcor = UINT32_MAX, tcr = 0x0002u, tstr = 1u;
uint32_t kui_native_readahead_aux_test_read(uint32_t address, unsigned width) {
    switch(address) {
    case UINT32_C(0xffc00000): CHECK(width == 2u); return frqcr;
    case UINT32_C(0xffd80004): CHECK(width == 1u); return tstr;
    case UINT32_C(0xffd80008): CHECK(width == 4u); return tcor;
    case UINT32_C(0xffd8000c): CHECK(width == 4u); return tcnt;
    case UINT32_C(0xffd80010): CHECK(width == 2u); return tcr;
    default: CHECK(!"a register the player does not read"); return 0u;
    }
}
static void ticks(uint32_t n) { tcnt -= n; }
/* What the player's clock makes of n ticks. */
static uint32_t sectors_of(uint32_t n) { return n / 31250u * 3u + n % 31250u * 3u / 31250u; }
/* Ticks for exactly n sectors to have passed (at least n, fewer than n+1). */
static uint32_t ticks_for(uint32_t n) { return n / 3u * 31250u + (n % 3u * 31250u + 2u) / 3u; }

static void fresh(void) {
    memset(&R, 0, sizeof(R));
    memset(bank, 0, sizeof(bank));
    memset(&kui_toy_cdda_drive, 0, sizeof(kui_toy_cdda_drive));
    const struct kui_gd_ops ops = {bank[0], map, range, reader}, twin_ops = {bank[1], map, range, reader};
    CHECK(kui_retail_gd_init(&R.shared.service, tracks, TRACKS, &ops, UINT32_C(0x8c008000), UINT32_C(0x8d000000)) == 0);
    CHECK(kui_retail_gd_init(&twin, tracks, TRACKS, &twin_ops, UINT32_C(0x8c008000), UINT32_C(0x8d000000)) == 0);
    R.shared.service.position_lba = twin.position_lba = 45000u;
    twin_on = true; twin_but_status = false;
    delivered = failure = 0u; started = true;
    frqcr = 0x0e0au; tcor = UINT32_MAX; tcr = 0x0002u; tstr = 1u;
}
/* The twin keeps up while nothing has been played: every answer and every
 * word of the service is the same with the player as without it. */
static void same_as_twin(void) {
    struct kui_retail_gd a = R.shared.service, b = twin;
    a.ops.context = b.ops.context = NULL;
    if(twin_but_status) a.drive_status = b.drive_status;
    if(memcmp(&a, &b, sizeof(a))) {
        const uint8_t *x = (const void *)&a, *y = (const void *)&b;
        for(size_t i = 0; i < sizeof(a); ++i)
            if(x[i] != y[i]) fprintf(stderr, "service byte %zu: %02x against the twin's %02x\n", i, x[i], y[i]);
    }
    CHECK(!memcmp(&a, &b, sizeof(a)));
    /* Function 4's answer is the drive status: its word at OUT is left out. */
    const size_t out = OUT - BEGIN;
    CHECK(!memcmp(bank[0], bank[1], twin_but_status ? out : sizeof(bank[0])));
    if(twin_but_status) CHECK(!memcmp(bank[0] + out + 4u, bank[1] + out + 4u, sizeof(bank[0]) - out - 4u));
}
/* The low resident tells the auxiliary of a successful INIT or RESET, and of
 * an ABORT, after the service, and of a read report() completed
 * (retail_async.c, kui_retail_async_after and report()). */
static void movie_complete(void) {
    (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_MOVIE_COMPLETE, 0u, &R, NULL);
}
/* report(): the engine's progress, published to the service whose read it is. */
static void report(struct kui_retail_gd *s) {
    if(!s->pending || R.engine.token != s->token) return;
    kui_retail_gd_progress(s, R.engine.cursor.done, R.engine.failed);
    if(s == &R.shared.service && !s->pending) movie_complete();
}
/* A delivery visit from one of the game's calls: the player first, the
 * visit's deliveries, the player again at the visit's end. */
static void visit(void) {
    (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, NULL);
    R.engine.cursor.done = delivered; R.engine.failed = failure;
    (void)kui_toy_cdda_player_visit(KUI_TOY_CDDA_VISIT_END, 0u, &R, NULL);
}
static bool reading(const struct kui_retail_gd *s) {
    return s->pending && (s->command == KUI_GD_PIOREAD || s->command == KUI_GD_DMAREAD);
}
/* One of the game's GD calls, as the low resident makes it. */
static int32_t call(uint32_t function, uint32_t r4, uint32_t r5) {
    CHECK(!R.engine.in_irq);
    visit();
    report(&R.shared.service);
    const int32_t result = kui_retail_gd_dispatch(&R.shared.service, r4, r5, 0u, function);
    if(twin_on) {
        report(&twin);
        CHECK(kui_retail_gd_dispatch(&twin, r4, r5, 0u, function) == result);
    }
    /* A read accepted starts at once: the engine takes its token, delivers
     * what it has (an open window's sectors) in a visit, and report()s. */
    if(started && function == KUI_GD_REQUEST && result > 0 && reading(&R.shared.service)) {
        R.engine.token = R.shared.service.token;
        R.engine.cursor.done = 0u; R.engine.failed = failure = 0u;
        visit();
        report(&R.shared.service);
        if(twin_on) report(&twin);
    }
    if(twin_on) same_as_twin();
    return result;
}
/* gdFs: the request, and EXEC at once. */
static int32_t request(uint32_t command, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    for(int i = 0; i < 2; ++i) {
        put(i, PARAM, a); put(i, PARAM + 4u, b); put(i, PARAM + 8u, c); put(i, PARAM + 12u, d);
    }
    const int32_t token = call(KUI_GD_REQUEST, command, PARAM);
    CHECK(token > 0);
    (void)call(KUI_GD_EXEC, 0u, 0u);
    return token;
}
static int32_t check(int32_t token) { return call(KUI_GD_CHECK, (uint32_t)token, STAT); }
static void finish(int32_t token) { CHECK(check(token) == KUI_GD_COMPLETED); }
static uint32_t drive(void) {
    CHECK(call(KUI_GD_DRIVE, OUT, 0u) == 0);
    return get(0, OUT);
}
/* REQ_STAT: {status, track, 0x10000000|control<<24|FAD, 1}. */
static uint32_t req_stat(uint32_t *status, uint32_t *track) {
    const int32_t token = request(KUI_RETAIL_GD_REQ_STAT, OUT + 16u, OUT + 20u, OUT + 24u, OUT + 28u);
    finish(token);
    if(status) *status = get(0, OUT + 16u);
    if(track) *track = get(0, OUT + 20u);
    CHECK(get(0, OUT + 28u) == 1u);
    return get(0, OUT + 24u) & 0xffffffu;
}
/* GETSCD format 1: binary track, index, elapsed and FAD. */
static uint32_t subcode_fad(uint32_t *track, uint32_t *elapsed) {
    memset(map(bank[0], OUT + 64u, 14u, 1), 0xee, 14u);
    if(twin_on) memset(map(bank[1], OUT + 64u, 14u, 1), 0xee, 14u);
    const int32_t token = request(KUI_RETAIL_GD_GETSCD, 1u, 14u, OUT + 64u, 0u);
    finish(token);
    CHECK(get8(0, OUT + 65u) == 0x15u); /* the audio status is today's: gdFs never reads it */
    if(track) *track = get8(0, OUT + 69u);
    if(elapsed) *elapsed = (uint32_t)get8(0, OUT + 71u) << 16 | (uint32_t)get8(0, OUT + 72u) << 8 | get8(0, OUT + 73u);
    return (uint32_t)get8(0, OUT + 75u) << 16 | (uint32_t)get8(0, OUT + 76u) << 8 | get8(0, OUT + 77u);
}
/* GETSCD format 0, the raw Q the library decodes: FAD from its BCD MSF. */
static uint32_t bcd(uint8_t v) { return (v >> 4) * 10u + (v & 15u); }
static uint32_t raw_q_fad(void) {
    const int32_t token = request(KUI_RETAIL_GD_GETSCD, 0u, 100u, OUT + 128u, 0u);
    finish(token);
    uint8_t q[12] = {0};
    for(unsigned bit = 0; bit < 96u; ++bit)
        q[bit / 8u] |= (uint8_t)(((get8(0, OUT + 128u + 4u + bit) >> 6) & 1u) << (7u - bit % 8u));
    return (bcd(q[7]) * 60u + bcd(q[8])) * 75u + bcd(q[9]);
}
/* A data read of count sectors from lba, delivered a few sectors a call
 * while the game polls, to its end. */
static void data_read(uint32_t lba, uint32_t count) {
    delivered = 0u;
    const int32_t token = request(KUI_GD_PIOREAD, lba + 150u, count, BUFFER, 0u);
    int32_t status;
    unsigned n = 0;
    while((status = check(token)) == KUI_GD_PROCESSING) {
        delivered += 3u;
        (void)call(KUI_GD_EXEC, 0u, 0u);
        CHECK(++n < 1000u);
    }
    CHECK(status == KUI_GD_COMPLETED);
}
static unsigned scenarios;

/* No play: the player writes nothing. Every answer and every word of the
 * service is the twin's, through reads, positions, seeks and commands. */
static void no_play(void) {
    fresh();
    for(unsigned round = 0; round < 3u; ++round) {
        CHECK(drive() == (round ? 2u : 1u));
        data_read(45100u + round * 40u, 17u);
        uint32_t status, track;
        CHECK(req_stat(&status, &track) == 45100u + round * 40u + 16u + 150u && status == 1u && track == 3u);
        (void)subcode_fad(NULL, NULL);
        (void)raw_q_fad();
        finish(request(KUI_RETAIL_GD_SEEK, 60500u + 150u, 0u, 0u, 0u));
        finish(request(KUI_GD_NOP, 0u, 0u, 0u, 0u));
        finish(request(KUI_GD_STOP, 0u, 0u, 0u, 0u));
        CHECK(drive() == 2u);
        ticks(1000000u);
    }
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && !kui_toy_cdda_drive.projections);
    ++scenarios;
}

/* ChuChu Rocket: a looping track, PLAY by track with repeat 15. The library's
 * REQ_STAT after PLAY; its pause, and its resume by RELEASE because the
 * position moved; the play's laps. Status 3 from the EXEC that saw PLAY. */
static void chuchu(void) {
    fresh();
    twin_on = false;
    const int32_t play = request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u);
    CHECK(R.shared.service.drive_status == 3u && kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING);
    finish(play);
    uint32_t status, track;
    const uint32_t played = req_stat(&status, &track);
    CHECK(status == 3u && track == 4u && played == 60000u + 150u);
    CHECK(R.shared.service.position_lba == 45000u); /* the data position, back */
    CHECK(drive() == 3u);
    /* 75 sectors a second. */
    ticks(ticks_for(750u));
    CHECK(req_stat(&status, NULL) == 60000u + 150u + 750u && status == 3u);
    ticks(ticks_for(10u) - 1u);
    CHECK(req_stat(NULL, NULL) == 60000u + 150u + 759u);
    ticks(1u);
    CHECK(req_stat(NULL, NULL) == 60000u + 150u + 760u);
    /* Pause: 1, and the position stands. */
    finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
    const uint32_t paused = req_stat(&status, NULL);
    CHECK(status == 1u && drive() == 1u && paused == 60000u + 150u + 760u);
    ticks(5u * 781250u);
    CHECK(req_stat(NULL, NULL) == paused && drive() == 1u);
    /* The library compares the two positions: they differ, so RELEASE. */
    CHECK(paused != played);
    finish(request(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u, 0u));
    CHECK(drive() == 3u);
    ticks(ticks_for(150u));
    CHECK(req_stat(NULL, NULL) == paused + 150u);
    /* Laps: the track is 10,000 sectors; it wraps to its start and plays on. */
    const uint32_t at = paused + 150u - 150u - 60000u;
    ticks(ticks_for(10000u - at) + 1u);
    uint32_t elapsed;
    const uint32_t lapped = subcode_fad(&track, &elapsed);
    CHECK(lapped == 60000u + 150u + 0u && track == 4u && elapsed == 0u);
    CHECK(drive() == 3u && kui_toy_cdda_drive.laps == 1u);
    ticks(ticks_for(3u * 10000u));
    CHECK(req_stat(&status, NULL) == 60000u + 150u && status == 3u && kui_toy_cdda_drive.laps == 4u);
    CHECK(kui_toy_cdda_drive.plays == 1u && kui_toy_cdda_drive.pauses == 1u && kui_toy_cdda_drive.releases == 1u);
    ++scenarios;
}

/* A jingle (repeat 0) plays once, then reads 1 and stands at its last sector;
 * repeat 1 plays twice. A second PLAY while one plays starts the new one. */
static void once_and_twice(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 5u, 5u, 0u, 0u));
    CHECK(drive() == 3u);
    ticks(ticks_for(849u));
    CHECK(drive() == 3u && req_stat(NULL, NULL) == 70150u + 150u + 849u);
    ticks(ticks_for(1u));
    uint32_t status;
    CHECK(req_stat(&status, NULL) == 70999u + 150u && status == 1u && drive() == 1u);
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED && kui_toy_cdda_drive.ends == 1u && !kui_toy_cdda_drive.laps);
    ticks(ticks_for(5000u));
    CHECK(req_stat(NULL, NULL) == 70999u + 150u && drive() == 1u);
    /* Twice. */
    finish(request(KUI_RETAIL_GD_PLAY, 6u, 6u, 1u, 0u));
    ticks(ticks_for(1500u));
    CHECK(drive() == 3u && req_stat(NULL, NULL) == 71000u + 150u && kui_toy_cdda_drive.laps == 1u);
    ticks(ticks_for(1500u));
    CHECK(drive() == 1u && req_stat(NULL, NULL) == 72499u + 150u && kui_toy_cdda_drive.ends == 2u);
    /* A new play while one plays. */
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    ticks(ticks_for(30u));
    finish(request(KUI_RETAIL_GD_PLAY, 6u, 6u, 15u, 0u));
    CHECK(req_stat(NULL, NULL) == 71000u + 150u);
    ++scenarios;
}

/* A range of tracks: 5 and 6 follow on (5 ends where 6 begins); the map has no
 * pregap, so 4 to 5 has a gap of 150 sectors, which plays as the range does.
 * A range into a data track ends at the last audio track. */
static void ranges(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 5u, 6u, 0u, 0u));
    ticks(ticks_for(850u));
    uint32_t track;
    CHECK(req_stat(NULL, &track) == 71000u + 150u && track == 6u);
    ticks(ticks_for(1500u));
    CHECK(drive() == 1u && kui_toy_cdda_drive.ends == 1u);
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 5u, 0u, 0u));
    ticks(ticks_for(10000u + 75u));
    CHECK(req_stat(NULL, &track) == 70075u + 150u && track == 4u && drive() == 3u); /* in the gap */
    ticks(ticks_for(75u + 850u));
    CHECK(drive() == 1u);
    finish(request(KUI_RETAIL_GD_PLAY, 6u, 7u, 15u, 0u));
    CHECK(kui_toy_cdda_drive.first == 71000u && kui_toy_cdda_drive.end == 72500u);
    finish(request(KUI_RETAIL_GD_PLAY, 6u, 99u, 15u, 0u));
    CHECK(kui_toy_cdda_drive.end == 72500u);
    ++scenarios;
}

/* PLAY2 by FAD: Ready 2 Rumble's stream (start, start + length, 0), the end
 * read as 1 once its length is played; KOF Dream Match 99's resume from the
 * FAD of the last raw subcode. A PLAY2 into a data track or before 150 is
 * refused, and a play that stood ends. */
static void by_fad(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY2, 60150u + 300u, 60150u + 300u + 450u, 0u, 0u));
    CHECK(drive() == 3u);
    ticks(ticks_for(449u));
    CHECK(drive() == 3u);
    ticks(ticks_for(1u));
    CHECK(drive() == 1u && req_stat(NULL, NULL) == 60150u + 300u + 449u);
    /* KOF: play, read the raw Q, pause, and play again from that FAD. */
    finish(request(KUI_RETAIL_GD_PLAY2, 60150u, 70000u + 150u, 15u, 0u));
    ticks(ticks_for(2000u));
    const uint32_t q = raw_q_fad();
    CHECK(q == 60150u + 2000u);
    finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_PLAY2, q, 70000u + 150u, 15u, 0u));
    CHECK(drive() == 3u && req_stat(NULL, NULL) == q);
    /* In the gap before track 5: the run goes on from there. */
    finish(request(KUI_RETAIL_GD_PLAY2, 70100u + 150u, 71500u + 150u, 0u, 0u));
    CHECK(drive() == 3u && kui_toy_cdda_drive.first == 70100u && kui_toy_cdda_drive.end == 71500u);
    /* Refused: data, below 150, an empty range. Each ends the play standing. */
    const uint32_t refused = kui_toy_cdda_drive.refused;
    finish(request(KUI_RETAIL_GD_PLAY2, 50000u, 50100u, 0u, 0u));
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && R.shared.service.drive_status == 1u);
    finish(request(KUI_RETAIL_GD_PLAY2, 100u, 60000u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_PLAY2, 61000u, 61000u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_PLAY, 3u, 3u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_PLAY, 8u, 9u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_PLAY, 5u, 4u, 0u, 0u));
    CHECK(kui_toy_cdda_drive.refused == refused + 6u && kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE);
    ++scenarios;
}

/* MDK2 and Bangai-O read the status while they load: during a play every
 * call answers 3, around the reads, and the window's rule keeps seeing the
 * data position: after a REQ_STAT or a GETSCD it is the last sector read. */
static void reads_during_play(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    for(unsigned n = 0; n < 4u; ++n) {
        data_read(45200u + 100u * n, 23u);
        CHECK(R.shared.service.position_lba == 45200u + 100u * n + 22u);
        CHECK(drive() == 3u);
        ticks(ticks_for(37u));
        uint32_t status;
        (void)req_stat(&status, NULL);
        CHECK(status == 3u && R.shared.service.position_lba == 45200u + 100u * n + 22u);
        (void)subcode_fad(NULL, NULL);
        CHECK(R.shared.service.position_lba == 45200u + 100u * n + 22u);
    }
    CHECK(kui_toy_cdda_drive.reads_in_play == 4u);
    ++scenarios;
}

/* SEEK, STOP and INIT end a play: the service's own answers from then on. The
 * functions INIT and RESET too, and an ABORT of the play before its EXEC. */
static void ends(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    finish(request(KUI_RETAIL_GD_SEEK, 61000u + 150u, 0u, 0u, 0u));
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && drive() == 1u);
    CHECK(R.shared.service.position_lba == 61000u);
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    finish(request(KUI_GD_STOP, 0u, 0u, 0u, 0u));
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && drive() == 2u);
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    finish(request(KUI_GD_COMMAND_INIT, 0u, 0u, 0u, 0u));
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && drive() == 1u);
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(call(KUI_GD_RESET, 0u, 0u) == 0);
    movie_complete();
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && drive() == 1u);
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(call(KUI_GD_INIT, 0u, 0u) == 0);
    movie_complete();
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE);
    /* ABORT: the request is seen at the ABORT call's visit, and cancelled. */
    for(int i = 0; i < 2; ++i) { put(i, PARAM, 4u); put(i, PARAM + 4u, 4u); put(i, PARAM + 8u, 15u); }
    const int32_t token = call(KUI_GD_REQUEST, KUI_RETAIL_GD_PLAY, PARAM);
    CHECK(token > 0 && call(KUI_GD_ABORT, (uint32_t)token, 0u) == 0);
    movie_complete();
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE && drive() == 1u);
    CHECK(check(token) == KUI_GD_FAILED);
    ++scenarios;
}

/* The library's own REQ_STAT is a REQUEST made in its CHECK pass, executed in
 * the next pass: the position is the play's at that EXEC, the data position
 * again at its CHECK. A second pause after a RELEASE-type resume sends no
 * REQ_STAT; the next resume is RELEASE again. */
static void library_order(void) {
    fresh();
    twin_on = false;
    for(int i = 0; i < 2; ++i) { put(i, PARAM, 4u); put(i, PARAM + 4u, 4u); put(i, PARAM + 8u, 15u); put(i, PARAM + 12u, 0u); }
    const int32_t play = call(KUI_GD_REQUEST, KUI_RETAIL_GD_PLAY, PARAM);
    (void)call(KUI_GD_EXEC, 0u, 0u);
    ticks(ticks_for(300u));
    CHECK(check(play) == KUI_GD_COMPLETED);
    /* The REQ_STAT in that CHECK pass. */
    put(0, PARAM, OUT + 16u); put(0, PARAM + 4u, OUT + 20u); put(0, PARAM + 8u, OUT + 24u); put(0, PARAM + 12u, OUT + 28u);
    const int32_t stat = call(KUI_GD_REQUEST, KUI_RETAIL_GD_REQ_STAT, PARAM);
    CHECK(stat > 0 && drive() == 3u);
    ticks(ticks_for(10u));
    (void)call(KUI_GD_EXEC, 0u, 0u);
    CHECK((get(0, OUT + 24u) & 0xffffffu) == 60150u + 310u && get(0, OUT + 16u) == 3u);
    CHECK(R.shared.service.position_lba == 60000u + 310u); /* still pending until its CHECK */
    CHECK(check(stat) == KUI_GD_COMPLETED && R.shared.service.position_lba == 45000u);
    /* Two visits found it pending: one projection. */
    CHECK(kui_toy_cdda_drive.projections == 1u);
    /* Two pauses, two RELEASEs. */
    finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
    finish(request(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u, 0u));
    ticks(ticks_for(75u));
    finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
    CHECK(drive() == 1u);
    finish(request(KUI_RETAIL_GD_RELEASE, 0u, 0u, 0u, 0u));
    CHECK(drive() == 3u && req_stat(NULL, NULL) == 60150u + 310u + 75u);
    ++scenarios;
}

/* The clock: a play kept for hours loses no sector against the arithmetic,
 * across the count's wrap; a play whose clock leaves the profile reads 1. */
static void clock(void) {
    fresh();
    tcnt = 1000u;
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    uint64_t total = 0;
    for(unsigned step = 0; step < 800u; ++step) {
        const uint32_t n = 6000000u + step * 7919u;
        ticks(n);
        total += n;
        CHECK(drive() == 3u);
    }
    const uint64_t sectors = total / 31250u * 3u + total % 31250u * 3u / 31250u;
    CHECK(req_stat(NULL, NULL) == 60150u + (uint32_t)(sectors % 10000u));
    CHECK(kui_toy_cdda_drive.laps == (uint32_t)(sectors / 10000u));
    (void)sectors_of;
    /* The profile goes: the play is lost, the status is 1, no position is the play's. */
    tcr = 0x0003u;
    CHECK(drive() == 1u && kui_toy_cdda_drive.state == KUI_TOY_CDDA_LOST && kui_toy_cdda_drive.lost == 1u);
    uint32_t status;
    CHECK(req_stat(&status, NULL) == 45000u + 150u && status == 1u);
    tcr = 0x0002u;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(drive() == 3u);
    tstr = 0u;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_LOST && drive() == 1u && kui_toy_cdda_drive.lost == 3u);
    ++scenarios;
}

/* An interrupt's delivery visit, and the cooperative pump's, answer no call
 * of the game's: the player writes nothing in them and sees no command. */
static void interrupt_visits(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(drive() == 3u);
    /* A REQ_STAT pending, a status the service wrote, and the clock moved. */
    for(int i = 0; i < 2; ++i) { put(i, PARAM, OUT + 16u); put(i, PARAM + 4u, OUT + 20u); put(i, PARAM + 8u, OUT + 24u); put(i, PARAM + 12u, OUT + 28u); }
    (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, NULL);
    const int32_t token = kui_retail_gd_dispatch(&R.shared.service, KUI_RETAIL_GD_REQ_STAT, PARAM, 0u, KUI_GD_REQUEST);
    CHECK(token > 0);
    R.shared.service.drive_status = 1u;
    ticks(ticks_for(600u));
    const struct kui_toy_cdda_drive before = kui_toy_cdda_drive;
    const struct kui_retail_gd service = R.shared.service;
    for(uint32_t context = 1u; context <= 2u; ++context) {
        R.engine.in_irq = (uint8_t)context;
        (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, NULL);
        (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_TICK, 0u, &R, NULL);
        (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_QUIESCE, 0u, &R, NULL);
        R.engine.in_irq = 0u;
        CHECK(!memcmp(&before, &kui_toy_cdda_drive, sizeof(before)));
        CHECK(!memcmp(&service, &R.shared.service, sizeof(service)));
    }
    /* The game's next call sees it all. */
    (void)call(KUI_GD_EXEC, 0u, 0u);
    CHECK(get(0, OUT + 16u) == 3u && (get(0, OUT + 24u) & 0xffffffu) == 60150u + 600u);
    CHECK(check(token) == KUI_GD_COMPLETED && R.shared.service.position_lba == 45000u);
    ++scenarios;
}

/* The catalogue (GETSCD format 2) carries no position: it is today's. A lap
 * that falls inside a group of three sectors is counted once: track 5 is 850
 * sectors, one more than a multiple of three. */
static void catalogue_and_partial_lap(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 5u, 5u, 15u, 0u));
    ticks(ticks_for(100u));
    const uint32_t projections = kui_toy_cdda_drive.projections;
    memset(map(bank[0], OUT + 64u, 24u, 1), 0xee, 24u);
    const int32_t token = request(KUI_RETAIL_GD_GETSCD, 2u, 24u, OUT + 64u, 0u);
    CHECK(R.shared.service.position_lba == 45000u);
    finish(token);
    CHECK(kui_toy_cdda_drive.projections == projections && get8(0, OUT + 68u) == 2u);
    /* To 851 sectors: the 850th crossed inside the group of 849 to 852. */
    ticks(ticks_for(851u) - ticks_for(100u));
    CHECK(req_stat(NULL, NULL) == 70150u + 150u + 1u && kui_toy_cdda_drive.laps == 1u);
    ticks(ticks_for(852u) - ticks_for(851u));
    CHECK(req_stat(NULL, NULL) == 70150u + 150u + 2u && kui_toy_cdda_drive.laps == 1u);
    ticks(ticks_for(855u) - ticks_for(852u));
    CHECK(req_stat(NULL, NULL) == 70150u + 150u + 5u && kui_toy_cdda_drive.laps == 1u);
    /* Many laps later, still exact. */
    ticks(ticks_for(850u * 41u + 5u) - ticks_for(855u));
    CHECK(req_stat(NULL, NULL) == 70150u + 150u + 5u && kui_toy_cdda_drive.laps == 41u);
    ++scenarios;
}

/* The card's interrupt between the game's calls: its visit answers no call of
 * the game's. The engine delivers up to sectors; in the Toy suite, report()
 * follows once the engine has finished (retail_async.c, kui_retail_async_irq).
 * The auxiliary hands no interrupt visit's end to the player: were it handed
 * one, the player would keep nothing in it. */
static void interrupt_delivers(uint32_t sectors, bool reports) {
    const struct kui_retail_gd before = R.shared.service;
    const struct kui_toy_cdda_drive drive_before = kui_toy_cdda_drive;
    R.engine.in_irq = 1u;
    (void)kui_toy_cdda_player_visit(KUI_NATIVE_RA_DELIVER, 0u, &R, NULL);
    delivered = sectors; R.engine.cursor.done = delivered;
    (void)kui_toy_cdda_player_visit(KUI_TOY_CDDA_VISIT_END, 0u, &R, NULL);
    R.engine.in_irq = 0u;
    CHECK(!memcmp(&before, &R.shared.service, sizeof(before)));
    CHECK(!memcmp(&drive_before, &kui_toy_cdda_drive, sizeof(drive_before)));
    if(reports) {
        report(&R.shared.service);
        if(twin_on) report(&twin);
    }
}

#if KUI_TOY_CDDA_KEEP
/* KEEP: MDK2 and Bangai-O ask for the drive's status between the pieces of a
 * load. The engine delivers in the visit (an open window's sectors at the
 * read's start, then the low reader's as it reads) and from the card's
 * interrupt between the game's calls. While the drive plays, every call
 * answers 3, and the service's words are the twin's but for the status, which
 * the twin's report() sets to 1. Raw sectors (2,352 bytes) are kept as well.
 * Paused or ended, the player keeps nothing. A failed read and an engine that
 * serves another read are report()'s: the failure's call answers 1, the next 3
 * again. */
static void keep_through_reads(void) {
    fresh();
    twin_but_status = true;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    CHECK(drive() == 3u && twin.drive_status == 1u);
    const uint32_t first = kui_toy_cdda_drive.keeps;
    /* 24 sectors: 8 from an open window at the start, then 2 from an
     * interrupt and 4 from a poll's visit, in turn. */
    delivered = 8u;
    const int32_t token = request(KUI_GD_PIOREAD, 45300u + 150u, 24u, BUFFER, 0u);
    CHECK(kui_toy_cdda_drive.keeps == first + 1u && R.shared.service.completed_bytes == 8u * 2048u);
    CHECK(R.shared.service.drive_status == 3u && twin.drive_status == 1u && R.shared.service.position_lba == 45307u);
    interrupt_delivers(10u, false);
    CHECK(drive() == 3u);                     /* the interrupt's 2, kept */
    delivered = 14u;
    CHECK(check(token) == KUI_GD_PROCESSING); /* the poll's own 4 */
    CHECK(drive() == 3u);
    interrupt_delivers(16u, false);
    delivered = 20u;
    CHECK(check(token) == KUI_GD_PROCESSING); /* both at once */
    CHECK(drive() == 3u);
    interrupt_delivers(22u, false);
    CHECK(drive() == 3u && kui_toy_cdda_drive.keeps == first + 5u);
    /* The interrupt that delivers the last two, and its report(): 1 until the
     * game's next call, whose visit writes 3 before it is answered. */
    interrupt_delivers(24u, true);
    CHECK(!R.shared.service.pending && R.shared.service.drive_status == 1u);
    CHECK(drive() == 3u && check(token) == KUI_GD_COMPLETED);
    CHECK(R.shared.service.position_lba == 45300u + 23u && R.shared.service.diag.sectors_read == 24u);
    const uint32_t kept = kui_toy_cdda_drive.keeps;
    /* Raw sectors: 37 of 2,352 bytes. */
    for(int i = 0; i < 2; ++i) { put(i, PARAM, 0u); put(i, PARAM + 4u, 0x1000u); put(i, PARAM + 8u, 0u); put(i, PARAM + 12u, 2352u); }
    CHECK(call(KUI_GD_DATATYPE, PARAM, 0u) == 0 && R.shared.service.sector_bytes == 2352u);
    delivered = 5u;
    const int32_t raw = request(KUI_GD_PIOREAD, 45400u + 150u, 37u, BUFFER, 0u);
    while(delivered < 37u) {
        CHECK(drive() == 3u);
        delivered = delivered + 7u < 37u ? delivered + 7u : 37u;
        (void)call(KUI_GD_EXEC, 0u, 0u);
    }
    CHECK(drive() == 3u && check(raw) == KUI_GD_COMPLETED && R.shared.service.completed_bytes == 37u * 2352u);
    CHECK(kui_toy_cdda_drive.keeps == kept + 6u);
    for(int i = 0; i < 2; ++i) { put(i, PARAM, 0u); put(i, PARAM + 4u, 0x2000u); put(i, PARAM + 8u, 0u); put(i, PARAM + 12u, 2048u); }
    CHECK(call(KUI_GD_DATATYPE, PARAM, 0u) == 0 && R.shared.service.sector_bytes == 2048u);
    /* A failed read is report()'s. */
    delivered = 0u;
    const int32_t bad = request(KUI_GD_PIOREAD, 45500u + 150u, 10u, BUFFER, 0u);
    delivered = 4u;
    CHECK(drive() == 3u && R.shared.service.completed_bytes == 4u * 2048u);
    delivered = 6u; failure = KUI_GD_ERROR_IO;
    CHECK(drive() == 1u && R.shared.service.completed_bytes == 6u * 2048u && !R.shared.service.pending);
    CHECK(drive() == 3u && check(bad) == KUI_GD_FAILED);
    /* An engine serving another read (the low reader did not start this one)
     * is report()'s too: nothing of it is published. */
    started = false;
    delivered = 9u; failure = 0u;
    const int32_t stale = request(KUI_GD_PIOREAD, 45600u + 150u, 10u, BUFFER, 0u);
    CHECK(drive() == 3u && !R.shared.service.completed_bytes && R.shared.service.pending);
    CHECK(call(KUI_GD_ABORT, (uint32_t)stale, 0u) == 0);
    movie_complete();
    CHECK(check(stale) == KUI_GD_FAILED);
    started = true;
    CHECK(kui_toy_cdda_drive.state == KUI_TOY_CDDA_PLAYING && drive() == 3u);
    /* Paused: 1, and nothing kept. */
    finish(request(KUI_RETAIL_GD_PAUSE, 0u, 0u, 0u, 0u));
    const uint32_t paused = kui_toy_cdda_drive.keeps;
    data_read(45700u, 12u);
    CHECK(drive() == 1u && kui_toy_cdda_drive.keeps == paused);
    /* Ended: likewise. */
    finish(request(KUI_RETAIL_GD_PLAY, 5u, 5u, 0u, 0u));
    ticks(ticks_for(850u));
    CHECK(drive() == 1u && kui_toy_cdda_drive.state == KUI_TOY_CDDA_ENDED);
    data_read(45800u, 12u);
    CHECK(drive() == 1u && kui_toy_cdda_drive.keeps == paused);
    ++scenarios;
}
#else
/* Without KEEP the drive status is 1 in a call during a read in a play
 * whenever the call's visit delivered sectors: the low reader's report()
 * publishes them after the visit, and kui_retail_gd_progress writes 1. */
static void status_through_reads(void) {
    fresh();
    twin_on = false;
    finish(request(KUI_RETAIL_GD_PLAY, 4u, 4u, 15u, 0u));
    delivered = 0u;
    const int32_t token = request(KUI_GD_PIOREAD, 45300u + 150u, 24u, BUFFER, 0u);
    CHECK(drive() == 3u);
    delivered = 4u;
    CHECK(drive() == 1u);
    CHECK(drive() == 3u);
    interrupt_delivers(8u, false);
    CHECK(drive() == 1u);
    delivered = 24u;
    CHECK(check(token) == KUI_GD_COMPLETED && drive() == 3u);
    ++scenarios;
}
#endif

int main(void) {
    no_play();
    catalogue_and_partial_lap();
    interrupt_visits();
    chuchu();
    once_and_twice();
    ranges();
    by_fad();
    reads_during_play();
#if KUI_TOY_CDDA_KEEP
    keep_through_reads();
#else
    status_through_reads();
#endif
    ends();
    library_order();
    clock();
    printf("PASS Test 84 drive: %u scenarios, %lu checks; with no play every answer and every service word "
           "is the twin service's without a player%s\n", scenarios, checks,
           KUI_TOY_CDDA_KEEP ? "; through a play's reads (KEEP) status 3 and every other word the twin's" :
           "; without KEEP a play's reads answer 1");
    return 0;
}
