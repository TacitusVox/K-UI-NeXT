/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_WIRE_REPORT_H
#define KUI_TOY_WIRE_REPORT_H
#include <stdint.h>

/* Toy Commander Test 68: what the continuous card stream counted, kept in
 * the reader's arena across streams and movie windows. The auxiliary writes
 * it and the worker reads it for the terminal report, both through the
 * cached view. All words. */
#define KUI_TOY_WIRE_REPORT_MAGIC UINT32_C(0x4b573638)
/* Where it lies in the arena: behind the largest staged raw request
 * (64 sectors of 2352 bytes), which is the arena's only other user. */
#define KUI_TOY_WIRE_REPORT_OFFSET UINT32_C(150528)
/* Test 69: reading the stream goes on from the stream's spare timer between
 * the game's calls, and the report says where the reader's time went. */
#ifndef KUI_TOY_WIRE_TICK
#define KUI_TOY_WIRE_TICK 0
#endif
/* Test 70: Test 68's reader, without Test 69's timer, with the auxiliary's
 * private stack used through the cache (native_readahead_aux_entry.S). */
#ifndef KUI_TOY_WIRE_CACHED
#define KUI_TOY_WIRE_CACHED 0
#endif
/* Both keep the report's account of the reader's time. */
#define KUI_TOY_WIRE_MEASURE (KUI_TOY_WIRE_TICK || KUI_TOY_WIRE_CACHED)
/* Test 71: Test 70 with the ring reader's block loop taken a word at a time
 * (toy_wire_stream.c), and the second movie's course in the report. */
#ifndef KUI_TOY_WIRE_WORDS
#define KUI_TOY_WIRE_WORDS 0
#endif
#if KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_CACHED
#error "Test 71 builds on Test 70"
#endif
/* Test 72: Test 71 with the player's update calls passed on to the reader
 * only when it has work or has not been called for a while
 * (toy_wire_ahead.inc). The notes on unexpected bytes have served and are
 * left out. */
#ifndef KUI_TOY_WIRE_PACE
#define KUI_TOY_WIRE_PACE 0
#endif
#if KUI_TOY_WIRE_PACE && !KUI_TOY_WIRE_WORDS
#error "Test 72 builds on Test 71"
#endif
/* Test 73: Test 72 with the same rule for the game's GD calls that carry
 * no request, and the player's own calls, buffer and chunks in the report
 * (toy_wire_ahead.inc). */
#ifndef KUI_TOY_WIRE_IDLE
#define KUI_TOY_WIRE_IDLE 0
#endif
#if KUI_TOY_WIRE_IDLE && !KUI_TOY_WIRE_PACE
#error "Test 73 builds on Test 72"
#endif
/* Test 74: Test 73 with the reader's own work cut down. The measurements of
 * Test 73 put the reader at about a third of the CPU in the second movie's
 * dense part, nearly all of it work on every byte. Its changes come each
 * under a switch of its own, so that one can be left out of a build:
 *   OPEN   the second movie's refills are not held back at its start, so the
 *          reader begins beside a full player buffer, as in the first movie;
 *   CLEAN  a sector whose every block passed its CRC16 is not also passed
 *          through its EDC, and a single lost byte rebuilt from the CRC16 is
 *          checked against its sector's P parity instead (toy_wire_ahead.inc);
 *   MSB    the SCI is asked to deliver bytes most significant bit first, which
 *          saves reversing every byte; the first token received says whether
 *          it did (toy_wire_stream.c);
 *   QUICK  a delivery visit with no request and nothing to decide goes
 *          straight to the pump's own rule (native_readahead_aux.c). */
#ifndef KUI_TOY_WIRE_LEAN
#define KUI_TOY_WIRE_LEAN 0
#endif
#if KUI_TOY_WIRE_LEAN && !KUI_TOY_WIRE_IDLE
#error "Test 74 builds on Test 73"
#endif
#ifndef KUI_TOY_WIRE_OPEN
#define KUI_TOY_WIRE_OPEN KUI_TOY_WIRE_LEAN
#endif
#ifndef KUI_TOY_WIRE_CLEAN
#define KUI_TOY_WIRE_CLEAN KUI_TOY_WIRE_LEAN
#endif
#ifndef KUI_TOY_WIRE_MSB
#define KUI_TOY_WIRE_MSB KUI_TOY_WIRE_LEAN
#endif
#ifndef KUI_TOY_WIRE_QUICK
#define KUI_TOY_WIRE_QUICK KUI_TOY_WIRE_LEAN
#endif
#if (KUI_TOY_WIRE_OPEN || KUI_TOY_WIRE_CLEAN || KUI_TOY_WIRE_MSB || KUI_TOY_WIRE_QUICK) && !KUI_TOY_WIRE_LEAN
#error "OPEN, CLEAN, MSB and QUICK are parts of Test 74: use KUI_TOY_WIRE_LEAN"
#endif
/* The second movie in pieces of 2^22 ticks, 5.4 s: twelve cover it. */
#define KUI_TOY_WIRE_BUCKETS 12u
#define KUI_TOY_WIRE_BUCKET_SHIFT 22u
enum {
    KUI_TOY_WIRE_STOP_CLOSE,   /* window closed or its range complete */
    KUI_TOY_WIRE_STOP_JUMP,    /* the file continues elsewhere on the card */
    KUI_TOY_WIRE_STOP_SECTOR,  /* a sector could not be completed or failed its check */
    KUI_TOY_WIRE_STOP_FRAME,   /* the stream could not be followed */
    KUI_TOY_WIRE_STOP_FAULT,   /* the SCI or the channel was not as left */
    KUI_TOY_WIRE_STOPS
};
enum {
    KUI_TOY_WIRE_DROP_ERROR,   /* producer error */
    KUI_TOY_WIRE_DROP_ENDED,   /* the movie is no longer playing */
    KUI_TOY_WIRE_DROP_IDLE,    /* no request, and the last one did not complete */
    KUI_TOY_WIRE_DROP_COMMAND, /* a command that is not a read */
    KUI_TOY_WIRE_DROP_WAITING, /* a read the game waits for with interrupts held off */
    KUI_TOY_WIRE_DROP_OTHER,   /* a read this window does not serve */
    KUI_TOY_WIRE_DROPS
};
#if KUI_TOY_WIRE_MEASURE
enum {
    KUI_TOY_WIRE_VISIT_SCI,    /* the SCI's error interrupt: a run was cut or reached its end */
    KUI_TOY_WIRE_VISIT_TIMER,  /* the stream's timer: a wait is over, or reading goes on */
    KUI_TOY_WIRE_VISIT_GAME,   /* a call of the game's own: a request, a poll, a frame */
    KUI_TOY_WIRE_VISITS
};
/* What lay around a byte between blocks that was neither idle nor a token.
 * Bytes are as the receiver delivered them, bit-reversed, in stream order
 * from the highest byte of a word down. */
struct kui_wire_frame_note {
    uint32_t before;      /* the four bytes ahead of it */
    uint32_t after;       /* the byte and the three behind it */
    /* Bits 0..15: bytes from the last block's end, or from the stream's
     * start, to the byte. Bits 16..23: which of the eight bytes above were
     * lost positions, the earliest in bit 16. Bits 24..27: how many of
     * `after` had arrived. */
    uint32_t gap;
    uint32_t since_lost;  /* bytes back to the nearest lost position, all ones if none is known */
    /* Bits 0..27: blocks this stream had handed on. Bits 28..31: the last
     * one's kind: 0 none yet, 1 clean, 2 rebuilt, 3 handed on with lost bytes. */
    uint32_t blocks;
};
#endif
struct kui_wire_counters {
    uint32_t starts;       /* streams begun */
    uint32_t segments;     /* DMA runs that reached their count */
    uint32_t cuts;         /* overruns inside a run */
    uint32_t waits;        /* of those, answered with a timer wait */
    uint32_t fulls;        /* runs not restarted for lack of room */
    uint32_t bytes;        /* wire bytes clocked out of the card */
    uint32_t wait_ticks;   /* time the wire stood still for timer waits */
    uint32_t blocks;       /* blocks handed on, all kinds */
    uint32_t rebuilt;      /* of those, one lost byte rebuilt from the CRC16 */
    uint32_t unsolved;     /* one lost byte the CRC16 could not explain: handed on holed */
    uint32_t holed;        /* of those, handed on with lost bytes for the sector parity */
    uint32_t unchecked;    /* of those holed, no lost data byte: only a CRC byte was lost */
    uint32_t gap_idle;     /* lost bytes between blocks taken as idle bytes */
    uint32_t gap_token;    /* lost bytes between blocks taken as the token */
    uint32_t bad_crc, lost_frame, ambiguous; /* parse stops, by kind */
#if KUI_TOY_WIRE_MEASURE
    uint32_t ticks;        /* timer calls asked for to go on reading */
    /* lost_frame by the byte's lowest zero bit, a count of at most 255 in
     * a byte each: bits 0..3 in the first word, 4..7 in the second. */
    uint32_t frame_bit[2];
    uint32_t frame_kind;   /* lost_frame by the kind of the block before, a byte each */
    struct kui_wire_frame_note frame_note[2]; /* the first two */
#endif
#if KUI_TOY_WIRE_WORDS
    /* lost_frame directly behind a block's end, and of those: with a lost
     * position in that block whose byte before it was stored from the
     * receiver's register (not by the channel), and with such a byte equal
     * to the one before it, as a byte taken twice would be. */
    uint32_t frame_end, frame_held, frame_twice;
#endif
#if KUI_TOY_WIRE_LEAN
    /* Runs that an overrun cut short again within 8, 32 and 128 bytes of
     * their start: those begun at once behind a cut, then those begun after
     * a timer wait. They say whether receiving on at once is worth it. */
    uint32_t again[2][3];
    /* The bit order the receiver delivers bytes in (enum kui_wire_order),
     * and how many streams were given up without an answer. */
    uint32_t order, undecided;
#endif
};
#if KUI_TOY_WIRE_LEAN
enum kui_wire_order {
    KUI_WIRE_ORDER_TRY,    /* not known yet: the next stream asks for MSB first */
    KUI_WIRE_ORDER_LSB,    /* asked for MSB first and the token came LSB first: not asked again */
    KUI_WIRE_ORDER_SEEN,   /* the token came MSB first; no block has passed its CRC16 yet */
    KUI_WIRE_ORDER_MSB,    /* MSB first, and a block has passed its CRC16 that way */
    KUI_WIRE_ORDER_BACK    /* the token came MSB first but the first block check failed: LSB first again */
};
#endif
struct kui_toy_wire_report {
    uint32_t magic;
    struct kui_wire_counters n;
    uint32_t seek_blocks;         /* blocks the low reader read to open or reopen */
    uint32_t stops[KUI_TOY_WIRE_STOPS];
    uint32_t drops[KUI_TOY_WIRE_DROPS];
    uint32_t filled, filled_bytes;/* sectors completed from their parity; bytes filled */
    uint32_t most_lost;           /* most lost bytes one sector arrived with */
    uint32_t refused;             /* sectors the parity could not complete */
    uint32_t check_bad;           /* sectors that then failed address or EDC */
    uint32_t parity;              /* 1: a sector that arrived whole has consistent parity; 2: not */
    uint32_t first_error, first_detail, timer_mode;
    uint32_t declined;            /* takeovers refused: no interrupt route or no channel */
#if KUI_TOY_WIRE_MEASURE
    /* The reader's own time, by who called: visits, their ticks, the longest. */
    uint32_t visit_calls[KUI_TOY_WIRE_VISITS], visit_ticks[KUI_TOY_WIRE_VISITS];
    uint32_t visit_max[KUI_TOY_WIRE_VISITS];
    uint32_t budget_ends;         /* visits that stopped reading for time with blocks left to read */
    uint32_t restart_ticks, restart_max; /* from a stream given up to its successor receiving */
    /* Requests the window could not serve when first asked, per movie:
     * how many, their ticks until served, the longest. */
    uint32_t wait_calls[2], wait_ticks[2], wait_max[2];
#if KUI_TOY_WIRE_CACHED
    /* The game's cache and MMU control registers when a window opened: they
     * decide how the private stack is reached and whether its writes are
     * kept in the cache. */
    uint32_t ccr, mmucr;
#endif
#if KUI_TOY_WIRE_WORDS
    /* The second movie's course, a piece of 5.4 s each from its start:
     * frames submitted, sectors served, streams given up; and ticks the
     * requests waited, ticks in interrupt visits (SCI and timer) and in the
     * game's calls. */
    uint16_t course_frames[KUI_TOY_WIRE_BUCKETS], course_sectors[KUI_TOY_WIRE_BUCKETS];
    uint16_t course_stops[KUI_TOY_WIRE_BUCKETS];
    uint32_t course_wait[KUI_TOY_WIRE_BUCKETS], course_interrupt[KUI_TOY_WIRE_BUCKETS];
    uint32_t course_game[KUI_TOY_WIRE_BUCKETS];
#endif
#if KUI_TOY_WIRE_PACE
    /* The player's update calls while a movie plays: all of them, those
     * passed on because the reader had work only the foreground can do,
     * and those passed on because none had been for a while. */
    uint32_t updates, update_work, update_due;
#endif
#if KUI_TOY_WIRE_IDLE
    /* The game's GD calls that reach the window with no request to serve:
     * all of them, and those that pumped the reader for work or as due. */
    uint32_t idles, idle_work, idle_due;
    /* Delivery visits from the game's GD calls, whole, as the dispatcher
     * times them: how many and their ticks. */
    uint32_t deliver_calls, deliver_ticks;
    /* The player's update calls, for each movie, by what the player's own
     * counters say the call did. It put a frame out: how many such calls,
     * their ticks inside the update, their ticks since the update before
     * returned, and the reader's interrupt ticks in that span. It skipped
     * a frame and put none out: how many and their ticks inside. */
    struct kui_toy_wire_player {
        uint32_t out, out_ticks, out_cycle, out_interrupt, skip, skip_ticks;
    } player[2];
    /* The second movie: ticks inside all its update calls; the capacity of
     * the player's own buffer; and for each piece of the course the most
     * of that buffer that was free and the most the player's lag counter
     * showed. */
    uint32_t update_ticks, buffer_capacity;
    uint32_t course_free[KUI_TOY_WIRE_BUCKETS], course_lag[KUI_TOY_WIRE_BUCKETS];
#endif
#if KUI_TOY_WIRE_LEAN
    /* Sectors accepted without their EDC because every block of theirs was
     * checked; sectors passed through the EDC; lost bytes rebuilt from a
     * CRC16 that their sector's P parity confirmed; such bytes it did not
     * confirm (the EDC then decided); and sectors the lighter check passed
     * that the EDC, taken as a sample, refused: this must stay zero, and one
     * such sector switches the lighter check off. */
    uint32_t light, edc, mends, mend_bad, light_bad;
    /* The game's EXEC and CHECK calls while a movie request is the last
     * one, each timed whole from the resident's entry visit to its exit
     * visit: how many and their ticks. */
    uint32_t gd_calls, gd_ticks;
#endif
#else
    uint32_t handler_ticks, handler_calls, handler_max; /* SCI interrupt visits while streaming */
#endif
};
#endif
