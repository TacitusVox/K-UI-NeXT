/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_WIRE_PROBE_H
#define KUI_TOY_WIRE_PROBE_H
#include <stdbool.h>
#include <stdint.h>

/* Toy Commander Test 67: while the original GD-ROM plays both opening movies
 * at full speed, the SCI pulls one open CMD18 stream from the microSD card in
 * the background and throws the data away. The game never reads a byte of
 * it. The question is how much wire time the SCI gets beside the running
 * decoder, how often its one-byte receiver overruns, and what servicing
 * that costs. Nothing here serves a game read. */
#define KUI_WIRE_SEGMENT_BYTES 16384u
#define KUI_WIRE_RUN_BUCKETS 6u
#define KUI_WIRE_PC_SAMPLES 128u
#define KUI_WIRE_CUT_LOG 64u
/* A run this long before the overrun is an isolated bus stall: receive on at
 * once. A shorter one means the bus is busy for a while: wait first. */
#define KUI_WIRE_GOOD_RUN 128u
/* TMU ticks at 781250 Hz: 82 us doubling to 1.3 ms. */
#define KUI_WIRE_BACKOFF_MIN 64u
#define KUI_WIRE_BACKOFF_MAX 1024u
/* First card block of the stream, and the room it needs behind it. */
#define KUI_WIRE_FIRST_BLOCK 0x20000u
#define KUI_WIRE_SPAN_BLOCKS 0x80000u

enum kui_wire_state {
    KUI_WIRE_OFF, KUI_WIRE_RUN, KUI_WIRE_PAUSED, KUI_WIRE_DONE, KUI_WIRE_FAULT
};
/* First refusal or fault; zero if none. 1-11 refuse before any reception. */
enum kui_wire_error {
    KUI_WIRE_OK,
    KUI_WIRE_E_CLOCK,    /* game timer is not the 781250 Hz profile */
    KUI_WIRE_E_ACQUIRE,  /* SCI port busy */
    KUI_WIRE_E_INIT,     /* card initialization; detail = reader result */
    KUI_WIRE_E_CARD,     /* slow card or too small for the stream span */
    KUI_WIRE_E_BUS,      /* SCI not in its fast command state */
    KUI_WIRE_E_IDLE,     /* card never idle before CMD18 */
    KUI_WIRE_E_COMMAND,  /* CMD18 response; detail = R1 */
    KUI_WIRE_E_TOKEN,    /* first data token; detail = byte seen */
    KUI_WIRE_E_VECTOR,   /* SDK interrupt table is not the admitted one */
    KUI_WIRE_E_CHANNEL,  /* DMAC channel 1 or DMAOR not free */
    KUI_WIRE_E_START,    /* first receive could not start */
    /* Faults of a running stream. */
    KUI_WIRE_E_FOREIGN = 16, /* channel 1 no longer ours; detail = DAR */
    KUI_WIRE_E_HANDOFF,  /* SCI did not return to its command state; detail = step */
    KUI_WIRE_E_END,      /* segment end without tail byte and overrun; detail = SSR */
    KUI_WIRE_E_STOP,     /* stopped without an overrun; detail = SSR | count << 8 */
    KUI_WIRE_E_ARM,      /* resume could not start; detail = SPTR */
    KUI_WIRE_E_CLOSE     /* CMD12 or port restoration; detail = step */
};

struct kui_wire_counters {
    uint32_t bytes;        /* wire bytes clocked out of the card */
    uint32_t cuts;         /* overruns inside a segment */
    uint32_t irqs;         /* our callbacks: SCI error and timer */
    uint32_t isr_ticks;    /* time inside them */
    uint32_t pauses;       /* overruns answered with a wait */
    uint32_t paused_ticks; /* time the wire stood still for those */
    uint32_t late_ticks;   /* time it stood still before an overrun was served */
    uint32_t run[KUI_WIRE_RUN_BUCKETS]; /* bytes before a cut: <16 <64 <256 <1024 <4096 more */
};
/* One movie, from its first submitted frame to the player's exit. */
struct kui_wire_window {
    uint32_t open, closed, begin_tick, span, pause_max;
    struct kui_wire_counters begin, total;
    /* The part of the window in which a movie read was in flight on the
     * GD-ROM drive: disc transfers an SD reader would never compete with. */
    uint32_t gd_ticks, gd_bytes, gd_cuts;
    uint32_t pc[KUI_WIRE_PC_SAMPLES], pc_count, pc_mask;
    uint32_t top_pc[2], top_count[2];
};
struct kui_wire_summary {
    uint32_t state, error, detail, mode; /* mode: 0 polls only, 1 TMU1, 2 TMU2 */
    uint32_t segments, restarts, spurious;
    uint32_t tail_bytes, tail_ok, tail_bad, tail_holed;
    struct kui_wire_window window[2];
};

/* Game-thread hooks; movie 0 is the bumper, 1 the intro. */
void kui_wire_movie_start(unsigned movie);
void kui_wire_frame(unsigned movie);
void kui_wire_movie_end(unsigned movie);
/* From hook code already running with every interrupt masked. gd: a movie
 * read was accepted by (true) or completed on (false) the GD-ROM drive. */
void kui_wire_poll(void);
void kui_wire_gd(bool busy);
/* Terminal report context, interrupts masked: stop whatever still runs and
 * reduce the samples. Call before the SCI port is used for the report. */
void kui_wire_shutdown(void);
const struct kui_wire_summary *kui_wire_summary(void);
/* The SDK's interrupt table calls this with the event code in R4. */
void kui_wire_isr(uint32_t event);

#endif
