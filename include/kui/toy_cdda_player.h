/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_PLAYER_H
#define KUI_TOY_CDDA_PLAYER_H
/* Test 84: CD music in the Background reader, the worker's player
 * (src/loader/toy_cdda_player.c, docs/cdda-background-test-84.md). */
#include <stdint.h>
#ifndef KUI_TOY_CDDA_CARD
#define KUI_TOY_CDDA_CARD 0
#endif
#if KUI_TOY_CDDA_CARD
union kui_retail_slot;
#endif

/* DRIVE: the drive plays. What the game's library reads back follows a play
 * clock: the drive status (3 while playing, 1 paused or ended) and, while a
 * REQ_STAT or a GETSCD is pending, the position. */
enum kui_toy_cdda_state {
    KUI_TOY_CDDA_IDLE = 0u,    /* no play: nothing is written, today's answers */
    KUI_TOY_CDDA_PLAYING = 1u,
    KUI_TOY_CDDA_PAUSED = 2u,
    KUI_TOY_CDDA_ENDED = 3u,   /* a play's last pass reached its range's end */
    KUI_TOY_CDDA_LOST = 4u     /* the clock's profile stopped holding: today's answers */
};
/* A sector of CD audio is 1/75 s: 781,250/75 = 31,250/3 TMU0 ticks by
 * TMU0's name. Where the music sounds (Test 89), the drive keeps the sound
 * chip's time, as the music does: TMU0 runs a quarter of a percent slow
 * against the chip (2,483 parts in a million on the console, give or take
 * 5, over 26 minutes of Test 87; the processor's clock is 199.5 MHz where
 * TMU0's 781,250 Hz reckons with 200), and three sectors of the chip's
 * time are 31,250 x 0.9975 = 31,172 of TMU0's ticks. With TMU0's own
 * measure the play's position, as the game reads it and as a play that
 * begins again takes the music up, fell 2.5 ms behind the music for every
 * second played. */
#if defined(KUI_TOY_CDDA_OUTPUT) && KUI_TOY_CDDA_OUTPUT
#define KUI_TOY_CDDA_TICKS_3_SECTORS 31172u
#else
#define KUI_TOY_CDDA_TICKS_3_SECTORS 31250u
#endif

struct kui_toy_cdda_drive {
    uint32_t state, generation, seen_token;
    /* The play: its range [first, end) in LBA, its passes (0: endless), and
     * the clock: at TMU0 count base_counter the play stood base_offset sectors
     * into its pass (down to -2 just after a lap). frozen is where it stood in
     * its pass when last seen (where it stands while paused), done the passes
     * completed. */
    uint32_t first, end, passes, base_counter;
    int32_t base_offset;
    uint32_t frozen, done;
    uint32_t play_token, repeat, track;
    /* A REQ_STAT or GETSCD is answered from the play's sector: the data
     * position is kept here until the command is no longer pending. */
    uint32_t projected, projected_token, saved_position;
    /* Counted for the report. */
    uint32_t plays, play2s, pauses, releases, ends, laps, refused, lost;
    uint32_t reads_in_play, projections, stops, keeps;
#if KUI_TOY_CDDA_CARD
    /* Test 85 (CARD): the disc's table as the service held it at the game's
     * last disc call, for the audio's segments (include/kui/toy_cdda_card.h). */
    const union kui_retail_slot *tracks;
    uint32_t track_count;
#endif
};
extern struct kui_toy_cdda_drive kui_toy_cdda_drive;
#if KUI_TOY_CDDA_CARD
/* Test 85 (CARD): the audio's consumer, called from the worker's heartbeat
 * behind the sound chip's survey and under its rule
 * (src/loader/vmu_native_passive.c). It follows the drive's play and asks for
 * its sectors (include/kui/toy_cdda_card.h); with no sound output compiled it
 * asks as an output would and drops the bytes (src/loader/toy_cdda_card.c). */
void kui_toy_cdda_consumer_heartbeat(void);
/* The worker's way to the auxiliary for it
 * (src/loader/vmu_native_reader_capture.inc): the soft window of one
 * heartbeat's visits in TMU0 ticks, half of it under the early SDK's drivers,
 * or 0 with no auxiliary bound; and one TICK visit, if one is bound. */
uint32_t kui_toy_cdda_card_window(void);
void kui_toy_cdda_card_tick(void);
#endif
#endif
