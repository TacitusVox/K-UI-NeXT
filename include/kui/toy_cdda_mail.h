/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_MAIL_H
#define KUI_TOY_CDDA_MAIL_H
/* Test 85 (CARD): the block the worker's heartbeat and the auxiliary share
 * for a CD-music track's audio (include/kui/toy_cdda_card.h).
 *
 * It lies in the worker's state, which the cold stage clears, and both parts
 * reach it at its uncached address only: nothing of it is ever in the cache,
 * so neither part has to write a line back for the other. The worker names
 * it in its player's descriptor (src/loader/vmu_native_trace_hooks.S); the
 * stage proves it lies in the worker's cleared state on whole cache lines and
 * hands its address to the auxiliary with the player's
 * (include/kui/native_readahead.h).
 *
 * The worker writes what it wants and makes a TICK visit; the auxiliary does
 * one visit's card work and answers. Nothing here is read by one part while
 * the other writes it: the worker's heartbeat runs with interrupts masked,
 * and a visit of the game's or of an interrupt reads none of what is asked. */
#include <stdbool.h>
#include <stdint.h>
#include "kui/toy_wire_report.h"

#define KUI_TOY_CDDA_MAIL_MAGIC UINT32_C(0x4b43444d) /* 'KCDM' */
/* What one visit hands on at most: two card blocks. */
#define KUI_TOY_CDDA_SINK_BYTES 1024u
/* Why a segment was refused (why). */
enum kui_toy_cdda_why {
    KUI_TOY_CDDA_WHY_NONE = 0u,
    KUI_TOY_CDDA_WHY_TRACK = 1u,    /* no such track in the map */
    KUI_TOY_CDDA_WHY_KIND = 2u,     /* a data track, or sectors that are not 2,352 or 2,448 bytes */
    KUI_TOY_CDDA_WHY_UNMAPPED = 3u, /* an audio track without its file's pieces in the map */
    KUI_TOY_CDDA_WHY_PIECES = 4u,   /* its pieces lie outside the map's own */
    KUI_TOY_CDDA_WHY_RANGE = 5u,    /* sectors outside the track, or none */
    KUI_TOY_CDDA_WHY_BEGIN = 6u,    /* the map does not hold the first sector's block */
    KUI_TOY_CDDA_WHY_FEED = 7u,     /* the map does not hold a later block, or bytes out of their order */
    KUI_TOY_CDDA_WHY_CARD = 8u      /* the block could not be read by programmed transfers, eight times over */
};

struct kui_toy_cdda_mail {
    uint32_t magic;            /* KUI_TOY_CDDA_MAIL_MAGIC once the worker has asked */
    /* Asked by the worker before each visit: the segment (generation 0:
     * nothing, the audio leaves the card), the card blocks this visit may
     * hand on at most (512 bytes each), whether it may read one block by
     * programmed transfers (the auxiliary clears it when it has tried), and
     * whether an idle read-ahead window may be given up for the audio. */
    uint32_t want_generation, want_track, want_lba, want_end;
    uint8_t want_blocks, seek_ok, yield_ok, reserved;
    /* Answered by the auxiliary: the segment it has taken up, the card's
     * state after the visit (enum kui_toy_cdda_card), why a segment was
     * refused, and the sink: sink_bytes bytes that continue the segment at
     * sink_offset. The auxiliary only adds to the sink; the worker hands the
     * bytes on, moves sink_offset behind them and clears sink_bytes. */
    uint32_t have_generation, card, why;
    uint32_t sink_offset, sink_bytes;
    /* Counted for the report, by the auxiliary:
     * blocks        card blocks of the stream handed on;
     * rebuilt       of those, with one lost byte rebuilt from the block's CRC16;
     * rereads       stream blocks not handed on (lost bytes left in them, an
     *               unchecked CRC16, or a stream that could not be followed):
     *               the stream ended there and the block was read again;
     * seek_blocks   blocks read by programmed transfers, each checked by the
     *               low reader;
     * seek_waits    visits that left such a read waiting for the card;
     * yields        idle read-ahead windows given up for the audio (YIELD);
     * declined      streams that could not be begun: no interrupt route, or
     *               no free channel;
     * data_halts    streams and waiting reads ended for a read of the game's;
     * other_halts   streams ended by the audio itself: a segment's or a
     *               piece's end, a block to read again, another segment
     *               wanted, a fault;
     * external_halts streams found ended by the reader (a loan's quiesce);
     * refused       segments refused;
     * seek_ticks    TMU0 ticks spent in those programmed reads;
     * first_error, first_detail  the first stream that could not be begun
     *               or followed: the stream's own error and detail. */
    uint32_t blocks, rebuilt, rereads, seek_blocks, seek_waits, yields, declined;
    uint32_t data_halts, other_halts, external_halts, refused, seek_ticks;
    uint32_t first_error, first_detail;
    /* And by the worker: visits made for the audio and their TMU0 ticks,
     * heartbeats that made one and the longest of them in ticks, and the
     * longest single visit that only read the stream (the heartbeat's rule
     * for one more visit reckons with it). */
    uint32_t visits, visit_ticks, heartbeats, longest, round_longest;
    /* The audio stream's own counters, as the window's stream keeps its. */
    struct kui_wire_counters wire;
    uint8_t sink[KUI_TOY_CDDA_SINK_BYTES];
};
#if KUI_TOY_WIRE_WORDS && KUI_TOY_WIRE_LEAN
_Static_assert(sizeof(struct kui_toy_cdda_mail) == 1312u && sizeof(struct kui_toy_cdda_mail) % 32u == 0u,
    "The shared block is whole cache lines, and the stage, the worker and the auxiliary agree on it");
#endif

/* Whether a block of this size at `address` lies wholly in [begin, end) on
 * whole cache lines: the stage asks it of the worker's cleared state, the
 * auxiliary of the worker's span of its own lease. */
static inline bool kui_toy_cdda_mail_address(uint32_t address, uint32_t begin, uint32_t end) {
    return !(address & 31u) && address >= begin && address <= end &&
        end - address >= (uint32_t)sizeof(struct kui_toy_cdda_mail);
}

/* The worker's own (src/loader/toy_cdda_card.c). */
extern struct kui_toy_cdda_mail kui_toy_cdda_mail;
#ifndef KUI_TOY_CDDA_OUTPUT
#define KUI_TOY_CDDA_OUTPUT 0
#endif
#if !KUI_TOY_CDDA_OUTPUT
#include "kui/toy_cdda_card.h"
/* The consumer with no sound output (include/kui/toy_cdda_player.h,
 * kui_toy_cdda_consumer_heartbeat): it follows the drive's play from the
 * sector the play stands at, one segment at a time (a track's sectors, to
 * the play's end at most), and takes bytes while it is less than this far
 * ahead of the play's clock: 16,384 frames, the most a sound output would
 * hold. Under a third of that it says it runs low. */
#define KUI_TOY_CDDA_LISTENER_LEAD 65536u
#define KUI_TOY_CDDA_LISTENER_LOW (KUI_TOY_CDDA_LISTENER_LEAD / 3u)
struct kui_toy_cdda_listener {
    /* The play followed: its generation and its count of releases when the
     * consumer began at its sector, and whether one is followed. */
    uint32_t play, releases, following;
    /* The segment asked for (generation 0: none), the last generation given
     * out, and the segment's bytes taken. */
    struct kui_toy_cdda_want want;
    uint32_t generation, taken;
    /* Where the next segment begins, the pass of the play it lies in, the
     * play's sector count where the consumer began, and the bytes taken or
     * passed over in silence since. */
    uint32_t sector, done, sync, ahead;
    /* Counted: times begun at a play's sector, segments asked for, bytes
     * taken, sectors passed in silence (no track holds them), bytes that
     * did not follow on (never), heartbeats not made with SR.BL set. */
    uint32_t starts, segments, bytes, silent, wrong, skipped;
};
extern struct kui_toy_cdda_listener kui_toy_cdda_listener;
#endif
#endif
