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
    uint32_t handler_ticks, handler_calls, handler_max; /* SCI interrupt visits while streaming */
};
#endif
