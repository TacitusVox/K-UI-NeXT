/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_OUTPUT_H
#define KUI_TOY_CDDA_OUTPUT_H
/* Test 85 (OUTPUT, VOLUME): the CD music of Test 84's drive, heard
 * (src/loader/toy_cdda_output.c, docs/cdda-background-test-85.md). */
#include <stdint.h>

enum kui_toy_cdda_output_phase {
    KUI_TOY_CDDA_OUT_OFF = 0u,     /* no play, or between plays: nothing of ours sounds */
    KUI_TOY_CDDA_OUT_PRIME = 1u,   /* the ring is being filled from its first frame */
    KUI_TOY_CDDA_OUT_RUN = 2u,     /* both channels loop the ring */
    KUI_TOY_CDDA_OUT_SILENT = 3u   /* this play stays silent: why says why (NONE: it has ended) */
};
/* Why a play is silent, the first reason of the play. */
enum kui_toy_cdda_output_why {
    KUI_TOY_CDDA_OUT_WHY_NONE = 0u,
    KUI_TOY_CDDA_OUT_WHY_DRIVER = 1u,  /* no admitted sound driver runs */
    KUI_TOY_CDDA_OUT_WHY_ARM = 2u,     /* the sound CPU is held in reset */
    KUI_TOY_CDDA_OUT_WHY_ID = 3u,      /* the driver's identity changed under the play */
    KUI_TOY_CDDA_OUT_WHY_PROOF = 4u,   /* the driver's records do not prove the two channels */
    KUI_TOY_CDDA_OUT_WHY_RING = 5u,    /* no free place for the ring, or it was lost (detail) */
    KUI_TOY_CDDA_OUT_WHY_TAKE = 6u,    /* the driver took a channel (detail: which) */
    KUI_TOY_CDDA_OUT_WHY_FLAP = 7u,    /* keyed off behind our back too often in one play */
    KUI_TOY_CDDA_OUT_WHY_CLOCK = 8u    /* the timer left the Toy profile */
};
/* RING's detail: no place among the table's pairs; a pair the reading does
 * not explain (0x100 + its index); a pair came to lie in the ring; a guard
 * line was overwritten. */
#define KUI_TOY_CDDA_OUT_RING_NONE 1u
#define KUI_TOY_CDDA_OUT_RING_TABLE 2u
#define KUI_TOY_CDDA_OUT_RING_GUARD 3u
#define KUI_TOY_CDDA_OUT_RING_PAIR 0x100u

/* One plane's ADPCM decoder as the chip's stands behind the frames written. */
struct kui_toy_cdda_coder {
    int32_t signal;
    uint32_t step;
};
struct kui_toy_cdda_output {
    uint32_t phase, why, why_detail;
    /* The drive's play this output follows (started: it follows one), the
     * driver it was admitted for, and the version word and date hash of the
     * driver last looked at, admitted or not. */
    uint32_t generation, started, version, family, seen_version, seen_hash;
    /* The two channels (the left plane's, the right plane's), the ring's
     * place in sound memory (its first guard line), a plane's frames, and
     * the search for a place: its step, its candidate, the bytes cleared;
     * spare: the place is the driver's spare memory, not one of the table's. */
    uint32_t channel[2], ring, frames, placed, place_try, place_at, wiped, spare;
    /* The audio still to come: the next sector to ask the card for, the end
     * of the piece being read, passes left (0: for ever), whether the last
     * pass's end is written and at which frame. segment names the piece to
     * the card; track its track (0: a gap); got its bytes so far; card the
     * card's last answer for it; asked: the card holds a wish of ours;
     * broken: bytes were lost on the way into the ring. */
    uint32_t sector, piece_end, passes, finished, audio_end, segment, track, got, card, asked, broken;
    /* Frames written into the ring since its first frame and where in the
     * ring the next go, TMU0 at key-on, frames played when last looked, and
     * the bytes of a unit of frames not yet whole (16-bit samples: two
     * frames; ADPCM: eight, with each plane's coder). */
    uint32_t written, at, key_tick, played, carried;
    uint8_t carry[32];
    struct kui_toy_cdda_coder coder[2];
    /* What each channel's level and pan word should be now, what was last
     * written, the CD mixers as last read and as first read with something
     * in them, heartbeats they read nothing, and MANATEE 2's own level
     * bytes when they stood in for them. */
    uint32_t level[2], level_set[2], mixer[2], mixer_first[2], mixer_zero, shadow;
    uint32_t last_tick, keyed, rearms;
    /* Counted for the report: heartbeats that used the bus, the longest and
     * all of their TMU0 ticks (the card's time included), bytes written to
     * the ring, key-ons, starts over (after an underrun, a lost byte, the
     * clocks' margin or a key dropped), and the reasons one by one. */
    uint32_t heartbeats, longest, bus_ticks, bytes, keys, restarts;
    uint32_t takes, drops, fixes, underruns, anchors, rekeys, fallbacks;
    uint32_t skipped_bl, skipped_dma, skipped_stack, timeouts, places, silence_frames, refusals;
    uint32_t faults, lost_guard, lost_table, prime_ticks;
};
extern struct kui_toy_cdda_output kui_toy_cdda_output;
/* The completion that calls the audio's consumer admits as little as 256
 * bytes of the worker's private stack below it (stack_valid,
 * src/loader/vmu_native_passive.c). The output goes deeper: it asks the
 * worker how much of that stack lies below its caller, and a heartbeat with
 * less than this is counted and not made
 * (tests/test_toy_cdda_card_linked.py holds the output's depth to it). */
#define KUI_TOY_CDDA_OUTPUT_STACK 768u
uint32_t kui_vmu_native_passive_stack_room(void);
/* The worker's heartbeat, after the survey's. */
void kui_toy_cdda_output_heartbeat(void);
/* Before the terminal report's pages and loan: nothing of ours sounds on. */
void kui_toy_cdda_output_terminal(void);
#endif
