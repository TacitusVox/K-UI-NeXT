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
 *          straight to the pump's own rule (native_readahead_aux.c);
 *   SWIFT  the block loop every received byte goes through is written by
 *          hand, for either bit order (toy_wire_words.inc). */
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
#ifndef KUI_TOY_WIRE_SWIFT
#define KUI_TOY_WIRE_SWIFT KUI_TOY_WIRE_LEAN
#endif
#if (KUI_TOY_WIRE_OPEN || KUI_TOY_WIRE_CLEAN || KUI_TOY_WIRE_MSB || KUI_TOY_WIRE_QUICK || KUI_TOY_WIRE_SWIFT) && \
    !KUI_TOY_WIRE_LEAN
#error "OPEN, CLEAN, MSB, QUICK and SWIFT are parts of Test 74: use KUI_TOY_WIRE_LEAN"
#endif
/* Test 75: Test 74 with what its console run showed. Nearly twice the
 * frames, and the reader's time down by a seventh where the instruction
 * counts had promised a third: most of what a block costs is not in the
 * loops that were counted. Its changes come each under a switch of its
 * own. What they share is here: the report's layout, with the reader's
 * time taken apart by the clock where Test 74 estimated it (reading the
 * ring, feeding the window, opening a stream, copying a request out), and
 * the reasons streams and windows were given up printed again. The parts:
 *   STAY   a request the game waits for in a loop of its own no longer
 *          costs the window: the stream serves it (toy_wire_ahead.inc);
 *   LATER  an interrupt visit answers the stream and leaves the reading to
 *          the game's own calls, unless a request waits for it or the ring
 *          is filling up (toy_wire_ahead.inc);
 *   RING   the block loop reads the ring through the cache, a line at a
 *          time and each line fetched afresh, where it took a bus read
 *          for every word (toy_wire_stream.c). */
#ifndef KUI_TOY_WIRE_STEADY
#define KUI_TOY_WIRE_STEADY 0
#endif
#if KUI_TOY_WIRE_STEADY && !KUI_TOY_WIRE_LEAN
#error "Test 75 builds on Test 74"
#endif
#ifndef KUI_TOY_WIRE_STAY
#define KUI_TOY_WIRE_STAY KUI_TOY_WIRE_STEADY
#endif
/* Test 76: Test 75 with what its console run showed. RING was the gain.
 * LATER was not one: half the interrupt visits left the reading, the
 * reader's time in the player's frame calls did not fall, and the stream's
 * losses came closer together, because a visit that leaves gives the
 * processor back to the code that stalls the bus (the document has the
 * counts). So LATER is off unless a build asks for it, and Test 75's own
 * recipe with LATER left out is the comparison. The clock put a block at 86
 * microseconds, and an independent reading of that account found a wait no
 * timer covered. Its parts come each under a switch of its own, below.
 * The report gives up the rows that have served (the game's GD calls
 * timed whole, the reasons windows were given up) for what Test 75's
 * could not say about its two worst places: what the player's own buffer
 * held, and how long the wire stood still, by piece of the course. */
#ifndef KUI_TOY_WIRE_FLUID
#define KUI_TOY_WIRE_FLUID 0
#endif
#if KUI_TOY_WIRE_FLUID && !KUI_TOY_WIRE_STEADY
#error "Test 76 builds on Test 75"
#endif
#ifndef KUI_TOY_WIRE_LATER
#define KUI_TOY_WIRE_LATER (KUI_TOY_WIRE_STEADY && !KUI_TOY_WIRE_FLUID)
#endif
#ifndef KUI_TOY_WIRE_RING
#define KUI_TOY_WIRE_RING KUI_TOY_WIRE_STEADY
#endif
#if (KUI_TOY_WIRE_STAY || KUI_TOY_WIRE_LATER || KUI_TOY_WIRE_RING) && !KUI_TOY_WIRE_STEADY
#error "STAY, LATER and RING are parts of Test 75: use KUI_TOY_WIRE_STEADY"
#endif
/* Test 77 (SEQ): Test 76 as its second console run had it, without GAP,
 * and with one change. Up to here a request used the window only inside
 * one of Toy Commander's four movie files, 386 sectors in, while the
 * game's player said that movie was playing. Now the reader decides by
 * what it sees itself: a read of 2,048-byte sectors on a raw data track
 * that begins where the read before it ended opens the window, which
 * reaches to that track's end, and a read elsewhere gives it up. The
 * files and the player's word no longer decide whether a request has a
 * window. They still say which reads take the low reader's movie path
 * (the residents are Test 66's), and with it whether a waiting request's
 * calls reach the window as calls the game waits in; they name the
 * report's rows; the player's update call still pumps the reader while a
 * movie plays; and a build without OPEN still holds the window back from
 * the second movie's file at that movie's start. A window that an
 * ordinary sequential load opens serves that load as well, which is the
 * point of asking the console: the report counts such windows and their
 * sectors apart, and why windows were given up. */
#ifndef KUI_TOY_WIRE_SEQ
#define KUI_TOY_WIRE_SEQ 0
#endif
#if KUI_TOY_WIRE_SEQ && !KUI_TOY_WIRE_FLUID
#error "Test 77 builds on Test 76"
#endif
/* Test 78: Test 77 with one change. A window served whole requests only,
 * of at most sixty sectors: one had to fit beside the sector still being
 * received. A larger read was the low reader's, and a window that stood
 * at its first sector was given up for it with all it had read ahead. A
 * game that refills in larger pieces (Evolution 2 reads 73 and 50 sectors
 * by turns, each where the last ended) would have lost its window at
 * every second request.
 *
 * Now a read of more than sixty sectors that begins at a window's first
 * sector is served through the window as the stream delivers it, at most
 * sixteen sectors a publication and two publications a visit; and such a
 * read, where it begins behind the read before it, opens a window like a
 * smaller one. The request's own cursor counts the sectors it has had. A
 * request of up to sixty sectors is served whole as before.
 *
 * A request the window has begun to serve and cannot finish (an error, a
 * wait that outlasts the stream) goes back to the low reader whole: its
 * cursor is begun again at its first sector, and the low reader delivers
 * every sector of it, over what the window wrote.
 *
 * Room: the auxiliary's image had 32 bytes free. The report's rows by
 * piece of the second movie's course (TF, TD, TW, TB, TR) are not kept in
 * this build; the owner has put that movie's remaining loss to rest. */
#ifndef KUI_TOY_WIRE_ANY
#define KUI_TOY_WIRE_ANY 0
#endif
#if KUI_TOY_WIRE_ANY && !KUI_TOY_WIRE_SEQ
#error "Test 78 builds on Test 77"
#endif
/* Test 79: Test 78 with one change. The window was offered to one title:
 * the stage looked for Toy Commander's executable and its four movie
 * files before it called a disc a source for the window, and the
 * auxiliary asked for that verified title with every request. Now every
 * title this launch family admits is offered the window, on an image with
 * a raw Mode 1 track on the SCI card, with the virtual VMU off.
 *
 * What stays Toy Commander's alone, by the same verification as before:
 * the hooks in its movie player, its movie files' ranges and the low
 * reader's policy for requests inside them, and the classification of its
 * disc calls by their return addresses. Another title has none of them:
 * its reads are a caller's the reader knows nothing of, as Toy
 * Commander's ordinary loading has been since Test 77.
 *
 * The module can be reached by a launch that passes the owner's VMU
 * setting on. Its worker has no virtual VMU: with the setting on, the
 * stage stops at once with a plain message, before any card step. And a
 * launch that brings no report file has the report on the screen, a page
 * at a time, where it could not be saved. Its text has the low reader's
 * own totals too (the two fixed pages are not shown then), and a card
 * stream still running is ended first, as a save ends it. */
#ifndef KUI_TOY_WIRE_TITLES
#define KUI_TOY_WIRE_TITLES 0
#endif
#if KUI_TOY_WIRE_TITLES && !KUI_TOY_WIRE_ANY
#error "Test 79 builds on Test 78"
#endif
/* Test 80: Test 79 with one change. A window was given up for every
 * command of the game's that is not a read. Evolution 2 sends such a
 * command between its reads: Test 79's run on the console opened 972
 * windows there and gave 969 of them up for one, 1.3 requests served to a
 * window, a fifth of the sectors read ahead thrown away.
 *
 * A command that only asks the drive something (its subcode, its status,
 * its mode, its version, a table of contents) or does nothing is answered
 * by the service from what it holds. It needs no card, and it leaves the
 * drive where it was. The window stands through those now. A command that
 * moves or stops the drive, sets its mode, plays audio or starts the drive
 * anew gives the window up as before, and so does every command the
 * service does not list.
 *
 * The report says which commands that are no reads were met while a
 * window stood, a bit to a command number, and how many the window stood
 * through: a game that sends another kind is named by the same run.
 *
 * Two things more in the report, neither of them the reader's behaviour.
 * It has the longest single visit by who called, where Test 79's had
 * totals alone: the owner saw a slight stutter in the Evolution games at
 * times while the card was read, and totals cannot say whether one visit
 * ever held the processor for a frame. And for a title that is not Toy
 * Commander the five rows of that game's movie player are left out: they
 * were zeros, and the report then fits three pages on the screen where
 * the owner photographed four. */
#ifndef KUI_TOY_WIRE_QUERY
#define KUI_TOY_WIRE_QUERY 0
#endif
#if KUI_TOY_WIRE_QUERY && !KUI_TOY_WIRE_TITLES
#error "Test 80 builds on Test 79"
#endif
/* Test 76's parts (the switch itself is above, with LATER's default):
 *   QUIT   a stream given up while it receives is ended at once, where it
 *          was waited out, with interrupts held off, until its run's next
 *          overrun: up to one segment, 5 ms (toy_wire_stream.c);
 *   HOT    the window's own state, and with it the cursor the low reader's
 *          feed works on, is reached through the cache: it lay at the
 *          auxiliary context's uncached address, a bus cycle for each of
 *          some fifty reads and writes a block
 *          (native_readahead_aux_internal.h);
 *   COPY   a block goes into the window a cache line at a time, each line
 *          claimed in the cache without being fetched first
 *          (toy_wire_copy.inc);
 *   GAP    the search for a block's token reads the ring through the
 *          cache, as the block loop has since Test 75: it took a bus read
 *          for every byte between two blocks (toy_wire_stream.c);
 *   APART  the buffer a block is taken out of the ring into no longer
 *          shares its places in the cache with the reader's own stack,
 *          nor with the bytes in the ring it is being filled from
 *          (toy_wire_stream.h). */
#ifndef KUI_TOY_WIRE_QUIT
#define KUI_TOY_WIRE_QUIT KUI_TOY_WIRE_FLUID
#endif
#ifndef KUI_TOY_WIRE_HOT
#define KUI_TOY_WIRE_HOT KUI_TOY_WIRE_FLUID
#endif
#ifndef KUI_TOY_WIRE_COPY
#define KUI_TOY_WIRE_COPY KUI_TOY_WIRE_FLUID
#endif
#ifndef KUI_TOY_WIRE_GAP
/* It reads as RING reads: a build without RING has no GAP either. Test 77
 * leaves it out unless a build asks for it: with and without it the
 * console's two runs of Test 76 cost the reader the same. */
#define KUI_TOY_WIRE_GAP (KUI_TOY_WIRE_FLUID && KUI_TOY_WIRE_RING && !KUI_TOY_WIRE_SEQ)
#endif
#ifndef KUI_TOY_WIRE_APART
#define KUI_TOY_WIRE_APART KUI_TOY_WIRE_FLUID
#endif
#if (KUI_TOY_WIRE_QUIT || KUI_TOY_WIRE_HOT || KUI_TOY_WIRE_COPY || KUI_TOY_WIRE_GAP || KUI_TOY_WIRE_APART) && \
    !KUI_TOY_WIRE_FLUID
#error "QUIT, HOT, COPY, GAP and APART are parts of Test 76: use KUI_TOY_WIRE_FLUID"
#endif
#if KUI_TOY_WIRE_GAP && !KUI_TOY_WIRE_RING
#error "GAP reads the ring through the cache as RING does: it needs KUI_TOY_WIRE_RING"
#endif
/* The parts a build has, as the report's PT value names them. */
#define KUI_TOY_WIRE_PARTS (KUI_TOY_WIRE_STAY | KUI_TOY_WIRE_LATER << 1 | KUI_TOY_WIRE_RING << 2 | \
    KUI_TOY_WIRE_QUIT << 3 | KUI_TOY_WIRE_HOT << 4 | KUI_TOY_WIRE_COPY << 5 | KUI_TOY_WIRE_GAP << 6 | \
    KUI_TOY_WIRE_APART << 7 | KUI_TOY_WIRE_SEQ << 8 | KUI_TOY_WIRE_ANY << 9 | \
    KUI_TOY_WIRE_TITLES << 10 | KUI_TOY_WIRE_QUERY << 11)
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
    KUI_TOY_WIRE_DROP_ENDED,   /* the movie is no longer playing (not from Test 77 on) */
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
#if !KUI_TOY_WIRE_FLUID
    /* Test 76 leaves out what no build since Test 73 has written. */
    uint32_t visit_max[KUI_TOY_WIRE_VISITS];
#endif
    uint32_t budget_ends;         /* visits that stopped reading for time with blocks left to read */
#if !KUI_TOY_WIRE_FLUID
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
#endif
#if KUI_TOY_WIRE_WORDS
    /* The second movie's course, a piece of 5.4 s each from its start:
     * frames submitted, sectors served, streams given up; and ticks the
     * requests waited, ticks in interrupt visits (SCI and timer) and in the
     * game's calls. */
    uint16_t course_frames[KUI_TOY_WIRE_BUCKETS], course_sectors[KUI_TOY_WIRE_BUCKETS];
#if !KUI_TOY_WIRE_FLUID
    uint16_t course_stops[KUI_TOY_WIRE_BUCKETS];
#endif
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
     * showed. Test 76 keeps the buffer's two and has, in the lag's place,
     * the ticks the wire stood still for the stream's timer waits. */
#if KUI_TOY_WIRE_FLUID
    uint32_t buffer_capacity;
    uint32_t course_free[KUI_TOY_WIRE_BUCKETS], course_pause[KUI_TOY_WIRE_BUCKETS];
#else
    uint32_t update_ticks, buffer_capacity;
    uint32_t course_free[KUI_TOY_WIRE_BUCKETS], course_lag[KUI_TOY_WIRE_BUCKETS];
#endif
#endif
#if KUI_TOY_WIRE_LEAN
    /* Sectors accepted without their EDC because every block of theirs was
     * checked; sectors passed through the EDC; lost bytes rebuilt from a
     * CRC16 that their sector's P parity confirmed; such bytes it did not
     * confirm (the EDC then decided); and sectors the lighter check passed
     * that the EDC, taken as a sample, refused: this must stay zero, and one
     * such sector switches the lighter check off. */
    uint32_t light, edc, mends, mend_bad, light_bad;
#if !KUI_TOY_WIRE_FLUID
    /* The game's EXEC and CHECK calls while a movie request is the last
     * one, each timed whole from the resident's entry visit to its exit
     * visit: how many and their ticks. Tests 74 and 75 measured them (30
     * microseconds a call); Test 76 no longer does. */
    uint32_t gd_calls, gd_ticks;
#endif
#endif
#if KUI_TOY_WIRE_STEADY
    /* The reader's time by what it was doing, in ticks: taking blocks out
     * of the ring (token, CRC16, bytes), feeding them to the window (the
     * copy and the sector's check), the low reader bringing the card to a
     * stream's first block, and copying requests out to the game. */
    uint32_t parse_ticks, feed_ticks, seek_ticks, publish_ticks;
    /* And two parts of those: the stream's own service (a run that ended
     * answered, the next one begun), wherever it was called from, and the
     * sectors' checks inside the feeding. */
    uint32_t service_ticks, check_ticks;
    /* Interrupt visits that answered the stream and left the reading for
     * later. */
    uint32_t later;
    /* The second movie's frame at the player's first skip. */
    uint32_t skip_first;
    /* Requests the game waited for in a loop of its own that kept their
     * window (STAY). */
    uint32_t stays;
#endif
#if KUI_TOY_WIRE_FLUID
    /* Ticks spent ending streams (kui_wire_stop): with a stream that
     * receives, up to Test 75, the wait for its run's next overrun. */
    uint32_t halt_ticks;
#endif
#if KUI_TOY_WIRE_SEQ
    /* Windows opened by a read outside the four movie files, and the
     * sectors served from such windows. */
    uint32_t other_windows, other_sectors;
#endif
#if KUI_TOY_WIRE_ANY
    /* Requests of more than sixty sectors a window served to their end,
     * their sectors, and requests a window had begun to serve that went
     * back to the low reader. */
    uint32_t large_requests, large_sectors, large_returned;
#endif
#if KUI_TOY_WIRE_QUERY
    /* Commands that are no reads met while a window stood: bit n of the
     * first word for command n below 32, of the second for n from 32 on.
     * And how many of them the window stood through. */
    uint32_t command_seen[2], queries;
    /* The longest single pump by who called, and the longest whole
     * delivery visit from one of the game's disc calls, in the clock's
     * ticks: what the reader held the processor for in one go. (The
     * totals beside them are visit_ticks and deliver_ticks.) */
    uint32_t visit_longest[KUI_TOY_WIRE_VISITS], deliver_longest;
#endif
#else
    uint32_t handler_ticks, handler_calls, handler_max; /* SCI interrupt visits while streaming */
#endif
};
#endif
