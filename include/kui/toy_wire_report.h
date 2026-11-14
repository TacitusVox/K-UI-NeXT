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
};
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
#else
    uint32_t handler_ticks, handler_calls, handler_max; /* SCI interrupt visits while streaming */
#endif
};
#endif
