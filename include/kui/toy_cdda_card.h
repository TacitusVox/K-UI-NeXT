/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_CDDA_CARD_H
#define KUI_TOY_CDDA_CARD_H
/* Test 85 (CARD): a CD-music track's audio, read from the card between the
 * game's own reads (docs/cdda-background-test-85.md).
 *
 * The drive of Test 84 plays in silence: its position moves on a clock. The
 * sound needs the track's sectors. The worker's heartbeat asks for them one
 * segment at a time; the auxiliary reads them with the Background reader's
 * own stream while no read of the game is asked for, and gives the card back
 * at once when one is. Only blocks whose CRC was checked are handed on.
 *
 * This is the worker's side of it, which is all the consumer of the audio
 * (the sound output, or with no output a consumer that drops the bytes) sees. */
#include <stdbool.h>
#include <stdint.h>

/* The audio wanted: sectors [lba, end) of one audio track, 2,352 bytes each,
 * in play order. generation names the segment. A new generation starts a new
 * segment at its lba; the same generation goes on where the last heartbeat's
 * bytes ended. Generation 0 wants nothing: the audio leaves the card. */
struct kui_toy_cdda_want {
    uint32_t generation;
    uint32_t track;        /* the track's number, as PLAY names it (from 1) */
    uint32_t lba, end;
    bool yield_ok;         /* the consumer runs low: an idle read-ahead window may be given up (YIELD) */
};
enum kui_toy_cdda_card {
    KUI_TOY_CDDA_CARD_IDLE = 0u,      /* nothing wanted, or the card is not the audio's to take now */
    KUI_TOY_CDDA_CARD_SEEKING = 1u,   /* opening the stream at the segment's next block */
    KUI_TOY_CDDA_CARD_STREAMING = 2u,
    KUI_TOY_CDDA_CARD_DONE = 3u,      /* the segment's last sector has been handed on */
    KUI_TOY_CDDA_CARD_REFUSED = 4u    /* no mapped audio track of 2,352-byte sectors, or outside it */
};
/* The reader's whole soft window for optional work at a heartbeat, in TMU0
 * ticks: 3 ms (KUI_NATIVE_RA_PIO_WINDOW_TICKS, include/kui/native_readahead.h;
 * tests/test_toy_cdda_card_window.c holds the two to each other). A sound
 * output that runs low has all of it. */
#define KUI_TOY_CDDA_CARD_WINDOW_TICKS 2344u
/* The consumer takes count bytes that continue the segment at byte offset
 * from its first sector's first byte. Offsets follow on without a gap and
 * without a repeat within one generation, from 0. The bytes are the
 * worker's own and stand until the consumer returns. */
typedef void (*kui_toy_cdda_take)(void *context, const uint8_t *bytes, uint32_t count, uint32_t offset);
/* One heartbeat's card work, called from the worker's heartbeat only (SR.BL
 * clear, the interrupt mask at 15), at most once a heartbeat.
 * room: the bytes the consumer can take now; no more are handed to take().
 * Returns the card's state after the heartbeat's last visit to the
 * auxiliary, one of enum kui_toy_cdda_card. With no auxiliary bound, or the
 * timer out of the Toy profile, it returns IDLE and hands on nothing. */
uint32_t kui_toy_cdda_card_heartbeat(const struct kui_toy_cdda_want *want, uint32_t room,
    kui_toy_cdda_take take, void *context);
/* The audio track that holds sector lba, by the disc's table as the reader
 * holds it (the player keeps it from the game's last disc call): its number
 * and its sectors [*start, *end). 0 when lba lies in no audio track, or no
 * table has been seen yet. The gaps between tracks belong to no track. */
uint32_t kui_toy_cdda_track_of(uint32_t lba, uint32_t *start, uint32_t *end);
#endif
