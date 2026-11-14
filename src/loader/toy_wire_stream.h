/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_WIRE_STREAM_H
#define KUI_TOY_WIRE_STREAM_H
#include <stdbool.h>
#include <stdint.h>
#include "kui/toy_wire_report.h"

/* Toy Commander Test 68: the card's open CMD18 stream received continuously
 * in the background, as Test 67 measured it beside the running decoder.
 *
 * The SCI holds one byte. A bus stall longer than a byte time overruns it:
 * the byte in the shifter is lost and the SCI stops clocking the card. Test
 * 67 showed that between those stalls the wire carries more than both
 * opening movies need. So nothing here stops per block. One DMA run takes
 * whatever comes, tokens, data, CRC and gap bytes alike, into a ring; an
 * overrun is answered by receiving on from the byte after the lost one, at
 * once after a long run and after a short timer wait after a short one.
 * Every lost byte's position in the stream is known and kept.
 *
 * The foreground then reads the ring: finds each block's token, checks the
 * block's CRC16 when no byte of it was lost, rebuilds a single lost byte
 * from that CRC, and hands blocks with more lost bytes on with their
 * positions, for the sector's own parity to fill. What leaves here is never
 * trusted by itself: the caller verifies every sector's address and EDC.
 *
 * Every register sequence is the one sci_stream.c and the Test 67 probe ran
 * on the console. All calls are made with interrupts masked. */

#define KUI_WIRE_RING_BYTES 32768u
#define KUI_WIRE_SEGMENT_MAX 8192u /* at most this long without an interrupt */
#define KUI_WIRE_HOLES 256u
#define KUI_WIRE_BLOCK_LOST 24u /* lost bytes of one block handed on, at most */
/* A run this long before the overrun is an isolated bus stall: receive on at
 * once. A shorter one means the bus is busy for a while: wait first. */
#define KUI_WIRE_GOOD_RUN 128u
/* TMU ticks at 781250 Hz: 82 us doubling to 1.3 ms. */
#define KUI_WIRE_BACKOFF_MIN 64u
#define KUI_WIRE_BACKOFF_MAX 1024u
/* Room the caller provides for the tables: bit reversal, the reflected
 * CRC16 step and its inverse. Each start fills them again. */
#define KUI_WIRE_TABLE_BYTES 1024u

enum kui_wire_state {
    KUI_WIRE_OFF,
    KUI_WIRE_RUN,   /* a DMA run is receiving */
    KUI_WIRE_WAIT,  /* stopped by an overrun after a short run; a timer resumes it */
    KUI_WIRE_FULL,  /* stopped at a segment's end with no room in the ring */
    KUI_WIRE_FAULT  /* the SCI or the channel is not as left; see error */
};
enum kui_wire_error {
    KUI_WIRE_OK,
    KUI_WIRE_E_CHANNEL = 1, /* DMAC channel 1 or DMAOR not free; detail = CHCR | DMAOR << 16 */
    KUI_WIRE_E_BUS,         /* SCI not in its fast command state */
    KUI_WIRE_E_START,       /* first receive could not start; detail = SPTR */
    KUI_WIRE_E_FOREIGN = 16,/* channel 1 no longer ours; detail = DAR */
    KUI_WIRE_E_HANDOFF,     /* SCI did not return to its command state; detail = step | SSR << 8 */
    KUI_WIRE_E_END,         /* segment end without the overrun behind it; detail = SSR */
    KUI_WIRE_E_STOP,        /* stopped without an overrun; detail = SSR | count << 16 */
    KUI_WIRE_E_ARM,         /* resume could not start; detail = SPTR */
    KUI_WIRE_E_HOLES        /* more lost bytes outstanding than the log holds */
};
/* Why the reader of the ring could not go on. */
enum kui_wire_parse {
    KUI_WIRE_NEED = 0,   /* the next block has not fully arrived */
    KUI_WIRE_BLOCK = 1,  /* one block is ready */
    KUI_WIRE_LOST_FRAME = -1, /* a byte that is neither idle nor a token between blocks */
    KUI_WIRE_BAD_CRC = -2,    /* a block with no lost byte failed its CRC16 */
    KUI_WIRE_AMBIGUOUS = -3   /* lost bytes between blocks left the next token unknown */
};

struct kui_wire_block {
    const uint8_t *data;   /* 512 bytes in card order */
    const uint16_t *lost;  /* ascending indices of bytes that are not real */
    uint32_t lost_count;
};
struct kui_wire_stream {
    uint32_t state, error, detail;
    uint32_t mode;         /* 0: no free timer, waits end at the next call; 1 TMU1; 2 TMU2 */
    /* Stream positions count wire bytes from the first one received. */
    uint32_t head;         /* next position to receive */
    uint32_t seg_begin, seg_len;
    uint32_t run_begin;    /* where the running or last run began */
    uint32_t tail;         /* next position to interpret */
    uint32_t want;         /* a position the caller waits for, or 0 */
    uint32_t backoff, wait_tick;
    uint32_t hole_put, hole_get;
    uint32_t phase, data_at, gap_hole, alternative;
    uint32_t sptr, saved_dma[4];
    uint32_t timer_reg, timer_bit, timer_event, timer_slot, timer_shift, timer_callback;
    uint32_t ring_physical;
    uint8_t *ring;         /* uncached view of KUI_WIRE_RING_BYTES, 32-byte aligned */
    uint8_t *reverse;      /* 256 */
    uint16_t *crc;         /* 256 */
    uint8_t *inverse;      /* 256 */
    struct kui_wire_counters *n; /* kept by the caller across starts */
    uint32_t floor;        /* oldest position the reader may still read */
    uint32_t wait_ticks;   /* the running wait's length */
#if KUI_TOY_WIRE_MEASURE
    uint32_t gap_begin;    /* where the space ahead of the next block began */
    uint32_t handed;       /* blocks this stream has handed on */
    uint32_t last_kind;    /* of the last one: 1 clean, 2 rebuilt, 3 with lost bytes */
#endif
    uint32_t hole[KUI_WIRE_HOLES];
    uint16_t lost[KUI_WIRE_BLOCK_LOST];
    _Alignas(4) uint8_t block[512];
};

/* Begin receiving. The card is in an open CMD18, stopped behind a block's
 * second CRC byte, deselected, and the SCI is in the bus's fast command
 * framing with sptr its pin register value. The stream's memory is set up
 * afresh; only *counters is added to. ring is the uncached view of
 * KUI_WIRE_RING_BYTES that nothing else touches, 32-byte aligned;
 * timer_callback is entered by the SDK's interrupt table for the chosen
 * timer and must lead to kui_wire_timer. False: no register was changed and
 * the stream is off; see error. */
bool kui_wire_start(struct kui_wire_stream *, struct kui_wire_counters *counters,
    uint8_t *ring, uint8_t *tables, uint8_t sptr, uint32_t timer_callback);
/* Serve the receiver: account a run an overrun ended and receive on, wait,
 * or hold for room; start again once there is room. From the SCI's error
 * interrupt and from any other call that passes by. */
void kui_wire_service(struct kui_wire_stream *);
/* The timer callback's work: a wait is over. */
void kui_wire_timer(struct kui_wire_stream *);
#if KUI_TOY_WIRE_TICK
/* Have the timer callback come once more, `ticks` from now, so that a
 * caller who ran out of time can go on reading without waiting for the
 * next run to end. Only while the stream receives or holds for room: a
 * wait uses the timer itself and brings the callback anyway. False if no
 * call was arranged. */
bool kui_wire_tick(struct kui_wire_stream *, uint32_t ticks);
#endif
/* The next block, if all of it has arrived. A negative result means this
 * stream cannot be followed any further and must be stopped. */
enum kui_wire_parse kui_wire_next(struct kui_wire_stream *, struct kui_wire_block *);
/* Stop receiving and give every borrowed register back: the SCI in command
 * framing, the card deselected and still inside its CMD18 at an unknown
 * byte. False if the SCI did not return to its command state. */
bool kui_wire_stop(struct kui_wire_stream *);
#endif
