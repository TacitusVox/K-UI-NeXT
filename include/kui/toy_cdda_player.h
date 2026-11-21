/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_PLAYER_H
#define KUI_TOY_CDDA_PLAYER_H
/* Test 84: CD music in the Background reader, the worker's player
 * (src/loader/toy_cdda_player.c, docs/cdda-background-test-84.md). */
#include <stdint.h>

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
/* A sector of CD audio is 1/75 s: 781,250/75 = 31,250/3 TMU0 ticks. */
#define KUI_TOY_CDDA_TICKS_3_SECTORS 31250u

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
};
extern struct kui_toy_cdda_drive kui_toy_cdda_drive;
#endif
