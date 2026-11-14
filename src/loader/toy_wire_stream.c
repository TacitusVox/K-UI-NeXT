/* SPDX-License-Identifier: GPL-3.0-only
 * Toy Commander Test 68: the card's CMD18 stream received continuously in
 * the background (see toy_wire_stream.h).
 *
 * The register sequences are those of sci_stream.c as the Test 67 probe
 * (toy_wire_probe.c) ran them 33,000 times on the console without a fault:
 * finish() to stop and count, handoff() with its MSTP0 module reset after an
 * overrun, and select + start_dma() to receive on from the byte after the
 * lost one. New against the probe: the destination is a ring whose bytes are
 * read, so a run's last two bytes are kept track of exactly (the one the
 * receiver still held is stored, the one that overran is recorded as lost),
 * and a run is sized to the room the reader has left. */
#include "toy_wire_stream.h"
#include <stddef.h>
#include <string.h>
#if defined(KUI_WIRE_STREAM_TEST) && KUI_TOY_WIRE_WORDS
#include <assert.h>
#endif

#define SMR UINT32_C(0xffe00000)
#define BRR UINT32_C(0xffe00004)
#define SCR UINT32_C(0xffe00008)
#define SSR UINT32_C(0xffe00010)
#define RDR UINT32_C(0xffe00014)
#define SCMR UINT32_C(0xffe00018)
#define SPTR UINT32_C(0xffe0001c)
#define STBCR UINT32_C(0xffc00004)
#define PDTR UINT32_C(0xff800030)
#define SAR1 UINT32_C(0xffa00010)
#define DAR1 UINT32_C(0xffa00014)
#define TCR1 UINT32_C(0xffa00018)
#define CHCR1 UINT32_C(0xffa0001c)
#define DMAOR UINT32_C(0xffa00040)
#define IPRA UINT32_C(0xffd00004)
#define TSTR UINT32_C(0xffd80004)
#define TCOR0 UINT32_C(0xffd80008)
#define TCNT0 UINT32_C(0xffd8000c)
/* Channel 1: SCI receive request, byte units, destination incrementing, no
 * completion interrupt. RIE+RE clock the card without a transmitter; with
 * SPTR.EIO only the receiver's error reaches the CPU. */
#define RX_DMA UINT32_C(0x4911)
#define RDR_PHYSICAL (RDR & UINT32_C(0x1fffffff))
#define RDRF 0x40u
#define ORER 0x20u
#define FLAGS 0x38u
#define TEND 0x04u
#define CS UINT16_C(0x0080)
/* The exact Toy Commander executable (SHA-256 ee68a326...9bbd, checked by
 * the stage before any reader is installed) keeps its SDK interrupt table
 * here: one RTS callback per event, 4 bytes per 0x20 of INTEVT. */
#define SDK_VBR UINT32_C(0x8c00f400)
#define SDK_ERI_SLOT UINT32_C(0x8c00f65c)
#define SDK_DEFAULT UINT32_C(0x8c0be408)
#define EVENT_ERI 0x4e0u
#define MASK (KUI_WIRE_RING_BYTES - 1u)
#define HOLE_MASK (KUI_WIRE_HOLES - 1u)
#define NONE UINT32_C(0xffffffff)
#define IDLE_WIRE 0xffu
#define TOKEN_WIRE 0x7fu /* 0xfe as the LSB-first receiver delivers it */
#if KUI_TOY_WIRE_MSB
/* Test 74. SCSCMR1.SDIR: serial/parallel conversion most significant bit
 * first. The SH7091's manual describes the bit for the smart card interface;
 * whether the SCI honours it in clocked synchronous mode is something only
 * the console can say, so the stream asks and looks at the first token it
 * receives: 0xfe as sent means it did, 0x7f that it did not. In the first
 * case no byte needs its bits reversed here; in the second nothing changes
 * against Test 73 and the bit is not written again. The register is written
 * with the SCI stopped, at each run's start, and is back at zero before the
 * SCI is used for a command byte or handed to anyone else. */
#define SDIR 0x08u
#define TOKEN_TRUE 0xfeu
/* Streams that may be given up for an answer before the question is. */
#define KUI_WIRE_ASKS 4u
#define TOKEN_OF(w) ((w)->token)
#else
#define TOKEN_OF(w) TOKEN_WIRE
#endif
#define FRAME 514u       /* a block's data and its two CRC bytes */
#define WANT_MIN 64u
/* Every run ends in a lost byte, so none is started for less than a full
 * one: a reader that has fallen behind is waited for, not fed in scraps. */
#define RUN_ROOM KUI_WIRE_SEGMENT_MAX
#define INLINE static inline __attribute__((__always_inline__))
_Static_assert(!(KUI_WIRE_RING_BYTES & MASK) && !(KUI_WIRE_HOLES & HOLE_MASK), "powers of two");
_Static_assert(RUN_ROOM >= KUI_WIRE_SEGMENT_MAX && RUN_ROOM + 2u <= KUI_WIRE_RING_BYTES,
    "A run admitted by its room never reaches unread bytes");
enum { SEEK, DATA };

#ifdef KUI_WIRE_STREAM_TEST
extern uint32_t kui_wire_test_read(uint32_t address, unsigned width);
extern void kui_wire_test_write(uint32_t address, uint32_t value, unsigned width);
extern void kui_wire_test_settle(unsigned count);
extern void kui_wire_test_fence(void);
extern uint32_t kui_wire_test_physical(const void *area);
extern uint32_t kui_wire_test_vbr(void);
extern void kui_wire_test_purge(const void *area, unsigned bytes);
#define rd8(a) ((uint8_t)kui_wire_test_read(a, 1))
#define rd16(a) ((uint16_t)kui_wire_test_read(a, 2))
#define rd32(a) kui_wire_test_read(a, 4)
#define wr8(a,v) kui_wire_test_write(a, (uint8_t)(v), 1)
#define wr16(a,v) kui_wire_test_write(a, (uint16_t)(v), 2)
#define wr32(a,v) kui_wire_test_write(a, v, 4)
#define settle kui_wire_test_settle
#define fence(w) kui_wire_test_fence()
#define physical kui_wire_test_physical
#define vector_base kui_wire_test_vbr
#define purge kui_wire_test_purge
#define word_read(a) kui_wire_test_read(a, 4)
#define word_publish(a,v) kui_wire_test_write(a, v, 4)
#else
#define rd8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define rd16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define rd32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define wr8(a,v) (rd8(a) = (uint8_t)(v))
#define wr16(a,v) (rd16(a) = (uint16_t)(v))
#define wr32(a,v) (rd32(a) = (uint32_t)(v))
INLINE void settle(unsigned n) {
    __asm__ __volatile__("1: dt %0\n\tbf 1b" : "+r"(n) : : "t", "memory");
}
INLINE uint32_t physical(const void *area) {
    return (uint32_t)(uintptr_t)area & UINT32_C(0x1fffffff);
}
INLINE uint32_t vector_base(void) {
    uint32_t vbr;
    __asm__ __volatile__("stc vbr,%0" : "=r"(vbr));
    return vbr;
}
/* An uncached read of main memory waits for the external bus, and the DMAC
 * goes before the CPU there: a transfer the channel had begun has landed
 * when it returns (sci_stream.c). */
INLINE void fence(const struct kui_wire_stream *w) { (void)*(volatile uint32_t *)(void *)w->ring; }
/* Write back and drop every cache line of an area the channel will fill:
 * a line left from an earlier use must never be written over its bytes. */
static void purge(const void *area, unsigned bytes) {
    uintptr_t line = ((uintptr_t)area & UINT32_C(0x1fffffff)) | UINT32_C(0x80000000);
    for(const uintptr_t end = line + bytes; line < end; line += 32u)
        __asm__ __volatile__("ocbp @%0" : : "r"(line) : "memory");
}
/* The SDK's table through the cached view the SDK itself reads and writes;
 * a write is also flushed to RAM, as the stage's own hook writes are. */
INLINE uint32_t word_read(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}
INLINE void word_publish(uint32_t address, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)address = value;
    __asm__ __volatile__("ocbp @%0" : : "r"(address & ~31u) : "memory");
}
#endif
typedef uint32_t alias_word __attribute__((__may_alias__));

/* Four table bytes per 0x20 of event code. */
INLINE uint32_t slot_of(uint32_t event) {
    return SDK_ERI_SLOT - (EVENT_ERI >> 3) + (event >> 3);
}

INLINE void timer_cancel(const struct kui_wire_stream *w) {
    wr8(TSTR, rd8(TSTR) & ~w->timer_bit);
    wr16(w->timer_reg + 8u, 0x0002u); /* no interrupt, underflow cleared */
    (void)rd16(w->timer_reg + 8u);
}
/* One shot: whoever answers it stops the channel again. */
INLINE void timer_arm(const struct kui_wire_stream *w, uint32_t ticks) {
    wr8(TSTR, rd8(TSTR) & ~w->timer_bit);
    wr32(w->timer_reg, UINT32_C(0xffffffff));
    wr32(w->timer_reg + 4u, ticks);
    wr16(w->timer_reg + 8u, 0x0022u); /* underflow interrupt, Pphi/64 */
    wr8(TSTR, rd8(TSTR) | w->timer_bit);
}

static void wire_fault(struct kui_wire_stream *w, uint32_t error, uint32_t detail) {
    wr8(SCR, 0);
    if(w->mode) timer_cancel(w);
    w->state = KUI_WIRE_FAULT;
    if(!w->error) {w->error = error; w->detail = detail;}
}

/* sci_sd_bus.c select_card(false): the transmitter idle, then CS high. */
INLINE bool wire_deselect(void) {
    bool idle = false;
    for(unsigned n = 0; n < 10000u; ++n) {
        uint8_t status = rd8(SSR);
        if(status & FLAGS) break;
        if(status & TEND) {idle = true; break;}
    }
    wr16(PDTR, rd16(PDTR) | CS);
    return idle;
}
INLINE void wire_select(void) { wr16(PDTR, rd16(PDTR) & (uint16_t)~CS); }

/* sci_stream.c handoff() with module_reset(): deselect, reset the module
 * after an overrun, and restore the synchronous fast command settings.
 * Zero, or the step that failed. */
static uint32_t wire_handoff(const struct kui_wire_stream *w, bool overrun) {
#if KUI_TOY_WIRE_MSB
    /* The SCI is stopped (every caller has written SCR). Whatever the
     * module reset below does or does not do to the bit, the checks behind
     * it see the register as they always have. Also for the stream that
     * found the bit ignored: it had written it. */
    if(w->asked) wr8(SCMR, 0);
#endif
    if(!wire_deselect()) return 1u;
    if(!(rd16(PDTR) & CS)) return 2u;
    if(overrun) {
        uint8_t before = rd8(STBCR);
        if((before & 1u) || rd8(SCR) || (rd8(SSR) & (RDRF | FLAGS)) ||
           (rd8(SPTR) & 0x8au) != 0x82u) return 3u;
        wr8(STBCR, before | 1u);
        bool stopped = false, resumed = false;
        for(unsigned n = 0; n < 8u && !stopped; ++n) stopped = rd8(STBCR) & 1u;
        settle(64);
        wr8(STBCR, rd8(STBCR) & ~1u);
        for(unsigned n = 0; n < 8u && !resumed; ++n) resumed = !(rd8(STBCR) & 1u);
        if(!resumed || !stopped) return 4u;
        settle(64);
        if(rd8(SCR) || rd8(SMR) || rd8(BRR) != 0xffu || rd8(SCMR) ||
           (rd8(SSR) & 0xfcu) != 0x84u || (rd8(SPTR) & 0x8au) != 0x82u) {
            wr8(SCR, 0);
            return 5u;
        }
    }
    wr8(SCR, 0); wr8(SCMR, 0); wr8(SMR, 0x80u); wr8(BRR, 0);
    settle(64); /* more than one bit time at 12.5 MHz */
    wr8(SPTR, w->sptr); wr8(SCR, 0x30u);
    uint8_t status = rd8(SSR);
    if(rd8(SCR) != 0x30u || rd8(SMR) != 0x80u || rd8(BRR) != 0 ||
       (status & (RDRF | FLAGS)) || (status & 0x84u) != 0x84u) return 6u;
    return 0u;
}

/* sci_stream.c start_dma(): receive `length` bytes to the ring at `index`. */
INLINE bool wire_start_dma(const struct kui_wire_stream *w, uint32_t index, uint32_t length) {
    wr8(SPTR, 0x83u); /* TxD latched high as GPIO before the transmitter is off */
    if((rd8(SPTR) & 0x8au) != 0x82u) return false;
    wr8(SCR, 0);
#if KUI_TOY_WIRE_MSB
    if(w->msb) wr8(SCMR, SDIR);
#endif
    wr32(CHCR1, 0);
    wr32(SAR1, RDR_PHYSICAL);
    wr32(DAR1, w->ring_physical + index);
    wr32(TCR1, length);
    wr32(CHCR1, RX_DMA);
    wr8(SCR, 0x50u);
    return true;
}

/* Receive on from head, as far as the ring's end, the reader's room and the
 * segment limit allow; or hold for room. */
static void wire_arm(struct kui_wire_stream *w) {
    const uint32_t used = w->head - w->floor, index = w->head & MASK;
    /* Behind every run come the byte the receiver still held and the one
     * that overran: two more positions. */
    const uint32_t room = used + 2u < KUI_WIRE_RING_BYTES ? KUI_WIRE_RING_BYTES - 2u - used : 0u;
    /* Every run ends in one more lost position: the log must have a place
     * for it. A reader that has fallen that far behind is waited for like
     * a full ring. */
    if(room < RUN_ROOM || w->hole_put - w->hole_get >= KUI_WIRE_HOLES - 1u) {
        if(w->state != KUI_WIRE_FULL) ++w->n->fulls;
        w->state = KUI_WIRE_FULL;
        return;
    }
    /* The room is at least a full run here: only the ring's end and the
     * segment limit bound the length. */
    uint32_t length = KUI_WIRE_RING_BYTES - index;
    if(length > KUI_WIRE_SEGMENT_MAX) length = KUI_WIRE_SEGMENT_MAX;
    /* A caller that waits for a position is told as soon as it is in: the
     * run ends there. This costs the one byte every run's end costs, so it
     * is done only for a position still ahead, never to hurry a reader
     * along that already has its bytes. */
    const uint32_t ahead = w->want - w->head;
    if(w->want && ahead - 1u < length - 1u) {
        const uint32_t most = ahead < WANT_MIN ? WANT_MIN : ahead;
        if(most < length) length = most; /* never beyond the ring's end */
    }
    wire_select();
    if(!wire_start_dma(w, index, length)) {wire_fault(w, KUI_WIRE_E_ARM, rd8(SPTR)); return;}
    w->seg_begin = w->run_begin = w->head;
    w->seg_len = length;
    w->state = KUI_WIRE_RUN;
}

/* sci_stream.c finish(): stop the SCI and the channel, count what landed,
 * keep the RDR byte and hand the SCI back to command bytes. status carries
 * SSR, with bit 8 set if the channel had reached its count. */
static bool wire_capture(struct kui_wire_stream *w, uint32_t *count_out, uint32_t *status_out, uint8_t *tail_out) {
    const uint32_t start = w->ring_physical + (w->seg_begin & MASK);
    uint32_t control = 0, count = 1;
    if(rd32(CHCR1) & 2u)
        for(unsigned n = 0; n < 512u && !(rd8(SSR) & ORER); ++n) {}
    wr8(SCR, 0);
    fence(w);
    uint32_t address = rd32(DAR1);
    bool ours = rd32(SAR1) == RDR_PHYSICAL && address >= start && address <= start + w->seg_len;
    if(ours) {
        wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(5));
        (void)rd32(CHCR1);
        fence(w);
        control = rd32(CHCR1); count = rd32(TCR1);
        ours = rd32(DAR1) == start + w->seg_len - count;
        if(ours) wr32(CHCR1, 0);
    }
    uint8_t status = rd8(SSR), tail = 0;
    if(status & RDRF) tail = rd8(RDR);
    wr8(SSR, status & ~(RDRF | FLAGS));
    uint32_t step = wire_handoff(w, (status & ORER) != 0);
    if(step) {wire_fault(w, KUI_WIRE_E_HANDOFF, step | (uint32_t)status << 8); return false;}
    if(!ours) {wire_fault(w, KUI_WIRE_E_FOREIGN, address); return false;}
    *count_out = count;
    *status_out = status | ((control & 2u) << 7);
    *tail_out = tail;
    return true;
}

/* Account a stopped run. The channel stored `at` bytes of the stream. If
 * the receiver still held one, it is byte `at` and the byte lost is the one
 * after it; otherwise byte `at` itself is lost. Behind a run that reached
 * its count the same holds for the two bytes that followed it. True if the
 * run was cut short. */
static bool wire_account(struct kui_wire_stream *w, uint32_t count, uint32_t status, uint8_t tail) {
    if((status & FLAGS) != ORER) {
        wire_fault(w, KUI_WIRE_E_STOP, (status & 0x1ffu) | count << 16);
        return false;
    }
    if(!count && !(status & 0x100u)) {wire_fault(w, KUI_WIRE_E_END, status); return false;}
    if(w->hole_put - w->hole_get >= KUI_WIRE_HOLES) {wire_fault(w, KUI_WIRE_E_HOLES, w->hole_put); return false;}
    const uint32_t at = w->seg_begin + w->seg_len - count, held = status & RDRF ? 1u : 0u;
    if(held) w->ring[at & MASK] = tail;
    const uint32_t lost = at + held;
#if KUI_TOY_WIRE_WORDS && !KUI_TOY_WIRE_PACE
    {
        const uint32_t entry = w->hole_put & HOLE_MASK;
        if(held) w->held[entry >> 5] |= UINT32_C(1) << (entry & 31u);
        else w->held[entry >> 5] &= ~(UINT32_C(1) << (entry & 31u));
    }
#endif
    w->hole[w->hole_put++ & HOLE_MASK] = lost;
    w->head = lost + 1u;
    w->n->bytes += w->head - w->seg_begin;
    return count != 0u;
}

/* The receiver overran: a cut, or the two bytes behind a full run. */
static void wire_stopped(struct kui_wire_stream *w) {
    uint32_t count, status;
    uint8_t tail;
    if(!wire_capture(w, &count, &status, &tail)) return;
    const bool cut = wire_account(w, count, status, tail);
    if(w->state == KUI_WIRE_FAULT) return;
    const uint32_t run = w->head - 1u - w->run_begin;
    if(!cut) ++w->n->segments;
    else ++w->n->cuts;
#if KUI_TOY_WIRE_LEAN
    if(cut && w->began && run < KUI_WIRE_GOOD_RUN)
        ++w->n->again[w->began - 1u][run < 8u ? 0u : run < 32u ? 1u : 2u];
    w->began = cut ? 1u : 0u;
#endif
    if(!cut || run >= KUI_WIRE_GOOD_RUN) {
        /* Only a long run shows the bus is quiet again. */
        if(run >= KUI_WIRE_GOOD_RUN) w->backoff = KUI_WIRE_BACKOFF_MIN;
        wire_arm(w);
        return;
    }
    w->state = KUI_WIRE_WAIT;
    w->wait_tick = rd32(TCNT0);
    w->wait_ticks = w->backoff;
    ++w->n->waits;
    if(w->mode) timer_arm(w, w->backoff);
    if(w->backoff < KUI_WIRE_BACKOFF_MAX) w->backoff <<= 1;
}

static void wire_resume(struct kui_wire_stream *w) {
    w->n->wait_ticks += w->wait_tick - rd32(TCNT0);
#if KUI_TOY_WIRE_LEAN
    w->began = 2u;
#endif
    wire_arm(w);
}

void kui_wire_timer(struct kui_wire_stream *w) {
    if(w->state == KUI_WIRE_OFF || w->state == KUI_WIRE_FAULT || !w->mode) return;
    timer_cancel(w);
    if(w->state == KUI_WIRE_WAIT) wire_resume(w);
}

#if KUI_TOY_WIRE_TICK
bool kui_wire_tick(struct kui_wire_stream *w, uint32_t ticks) {
    if(!w->mode || (w->state != KUI_WIRE_RUN && w->state != KUI_WIRE_FULL)) return false;
    /* The SDK may rewrite the priority register for its own devices. */
    if(!((rd16(IPRA) >> w->timer_shift) & 15u)) wr16(IPRA, rd16(IPRA) | 1u << w->timer_shift);
    timer_arm(w, ticks);
    ++w->n->ticks;
    return true;
}
#endif

#if KUI_TOY_WIRE_PACE
bool kui_wire_overdue(const struct kui_wire_stream *w) {
    if(w->state == KUI_WIRE_RUN) return (rd8(SSR) & ORER) != 0u;
    if(w->state == KUI_WIRE_WAIT) return !w->mode || (rd16(w->timer_reg + 8u) & 0x0100u) != 0u;
    return false;
}
#endif

void kui_wire_service(struct kui_wire_stream *w) {
    switch(w->state) {
    case KUI_WIRE_RUN:
        if((rd8(SSR) & ORER) || (rd32(CHCR1) & 2u)) wire_stopped(w);
        break;
    case KUI_WIRE_WAIT:
        /* Without a timer the next call that passes by ends the wait. With
         * one, a timer that ran out while interrupts were masked is
         * answered here; its interrupt request goes with its flag. */
        if(!w->mode) wire_resume(w);
        else if(rd16(w->timer_reg + 8u) & 0x0100u) kui_wire_timer(w);
        else if(!((rd16(IPRA) >> w->timer_shift) & 15u))
            /* The SDK may rewrite the priority register for its own devices. */
            wr16(IPRA, rd16(IPRA) | 1u << w->timer_shift);
        break;
    case KUI_WIRE_FULL:
#if KUI_TOY_WIRE_LEAN
        w->began = 0u;
#endif
        wire_arm(w);
        break;
    default:
        break;
    }
}

static void timer_select(struct kui_wire_stream *w, uint32_t callback) {
    w->mode = 0;
    if(!callback || vector_base() != SDK_VBR) return;
    const uint32_t running = rd8(TSTR), levels = rd16(IPRA);
    for(unsigned k = 2u; k; --k) {
        const uint32_t reg = TCOR0 + 12u * k, event = 0x400u + 0x20u * k, shift = 12u - 4u * k;
        if((running & (1u << k)) || (rd16(reg + 8u) & 0x0020u) || ((levels >> shift) & 15u) ||
           word_read(slot_of(event)) != SDK_DEFAULT) continue;
        w->timer_reg = reg; w->timer_bit = 1u << k; w->timer_event = event;
        w->timer_slot = slot_of(event); w->timer_shift = shift;
        w->timer_callback = callback;
        w->mode = k;
        return;
    }
}

static void wire_tables(struct kui_wire_stream *w, uint8_t *tables) {
    w->reverse = tables;
    w->inverse = tables + 256u;
    w->crc = (uint16_t *)(void *)(tables + 512u);
    for(unsigned i = 0; i < 256u; ++i) {
        unsigned r = i, c = i;
#if KUI_TOY_WIRE_MSB
        if(w->msb) {
            /* Bytes as the card sent them. CRC16-CCITT as it is defined,
             * one byte a step: c = (c << 8) ^ T[(c >> 8) ^ byte]. With the
             * remainder kept byte-swapped that is the very step the
             * reflected table takes below, c = (c >> 8) ^ T'[(c ^ byte) &
             * 0xff], for T' the same table byte-swapped: so every loop of
             * this file runs unchanged on either table, zero means zero in
             * both, and an entry's high byte still names it (it is the low
             * byte of T, which takes every value once). */
            c = i << 8;
            for(unsigned bit = 0; bit < 8u; ++bit) c = (c & 0x8000u ? c << 1 ^ 0x1021u : c << 1) & 0xffffu;
            c = (c >> 8 | c << 8) & 0xffffu;
        } else {
#endif
        r = (r >> 4 | r << 4) & 0xffu;
        r = ((r & 0xccu) >> 2 | (r & 0x33u) << 2) & 0xffu;
        r = ((r & 0xaau) >> 1 | (r & 0x55u) << 1) & 0xffu;
        /* CRC16-CCITT taken bit-reversed, as the receiver delivers bytes:
         * the remainder of a block and its CRC is zero either way. */
        for(unsigned bit = 0; bit < 8u; ++bit) c = c & 1u ? c >> 1 ^ 0x8408u : c >> 1;
#if KUI_TOY_WIRE_MSB
        }
#endif
        w->reverse[i] = (uint8_t)r;
        w->crc[i] = (uint16_t)c;
        /* A step's high byte comes from the table alone and names its entry. */
        w->inverse[c >> 8] = (uint8_t)i;
    }
#if KUI_TOY_WIRE_WORDS
    /* Two steps in one for a pair of bytes b0, b1 and t = crc ^ (b0 | b1 << 8):
     * the table is linear in its index, so the second step's entry splits
     * into crc[t >> 8] and what the first step's result adds to it. */
    /* The hand-written loops of Test 74 find both tables from the block's
     * start: this layout is theirs too. */
    w->crc2 = (uint16_t *)(void *)(tables + 1024u);
    for(unsigned i = 0; i < 256u; ++i) {
        const unsigned c = w->crc[i];
        w->crc2[i] = (uint16_t)(c >> 8 ^ w->crc[c & 0xffu]);
    }
#endif
}

bool kui_wire_start(struct kui_wire_stream *w, struct kui_wire_counters *counters,
    uint8_t *ring, uint8_t *tables, uint8_t sptr, uint32_t timer_callback) {
    /* Everything but the block buffer and the logs, whose live parts are
     * written before they are read. */
    memset(w, 0, offsetof(struct kui_wire_stream, hole));
    w->n = counters;
    w->ring = ring;
    w->ring_physical = physical(ring);
    w->sptr = sptr;
    w->backoff = KUI_WIRE_BACKOFF_MIN;
    w->phase = SEEK;
#if KUI_TOY_WIRE_MSB
    /* Ask for MSB first until a token has said the receiver does not do it
     * or a first block has failed that way. */
    w->asked = w->msb = counters->order != KUI_WIRE_ORDER_LSB && counters->order != KUI_WIRE_ORDER_BACK;
    w->token = w->msb ? TOKEN_TRUE : TOKEN_WIRE;
#endif
    const uint8_t status = rd8(SSR);
    if((sptr & 0x0au) || rd8(SCR) != 0x30u || rd8(SMR) != 0x80u || rd8(BRR) != 0 ||
       (status & (RDRF | FLAGS)) || !(rd16(PDTR) & CS) || (rd8(STBCR) & 1u)) {
        w->error = KUI_WIRE_E_BUS; w->detail = status | (uint32_t)rd8(SCR) << 8 | (uint32_t)sptr << 16;
        return false;
    }
    if((rd32(CHCR1) & 7u) || (rd32(DMAOR) & 7u) != 1u) {
        w->error = KUI_WIRE_E_CHANNEL; w->detail = (rd32(CHCR1) & 0xffffu) | rd32(DMAOR) << 16;
        return false;
    }
    wire_tables(w, tables);
    purge(ring, KUI_WIRE_RING_BYTES);
    for(unsigned i = 0; i < 4u; ++i) w->saved_dma[i] = rd32(SAR1 + 4u * i);
    timer_select(w, timer_callback);
    wire_select();
    if(!wire_start_dma(w, 0u, KUI_WIRE_SEGMENT_MAX)) {
        w->error = KUI_WIRE_E_START; w->detail = rd8(SPTR);
        wr16(PDTR, rd16(PDTR) | CS);
        wr8(SPTR, sptr); wr8(SCR, 0x30u);
        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);
        w->mode = 0;
        return false;
    }
    if(w->mode) {
        word_publish(w->timer_slot, w->timer_callback);
        timer_cancel(w);
        wr16(IPRA, rd16(IPRA) | 1u << w->timer_shift);
    }
    w->seg_len = KUI_WIRE_SEGMENT_MAX;
    w->state = KUI_WIRE_RUN;
    ++w->n->starts;
    return true;
}

bool kui_wire_stop(struct kui_wire_stream *w) {
    const uint32_t state = w->state;
    if(state == KUI_WIRE_OFF) return true;
    bool usable = true;
    if(w->mode) {
        timer_cancel(w);
        wr16(IPRA, rd16(IPRA) & ~(15u << w->timer_shift));
        if(word_read(w->timer_slot) == w->timer_callback) word_publish(w->timer_slot, SDK_DEFAULT);
        w->mode = 0;
    }
    if(state == KUI_WIRE_RUN) {
        /* The card must be left on a whole byte, and the receiver ends on
         * one only at its next overrun: a cut or the run's end, at most one
         * segment of wire time away (5 ms). What the run leaves behind is
         * not needed: whoever takes the card next ends this CMD18. */
        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}
        uint32_t count, status;
        uint8_t tail;
        if(!wire_capture(w, &count, &status, &tail)) usable = false;
        else w->n->bytes += w->seg_begin + w->seg_len - count - w->run_begin;
    } else if(state == KUI_WIRE_FAULT) {
        /* A fault left the SCI stopped at an unknown step: try the same
         * handoff once more from a known one. */
        const uint8_t status = rd8(SSR);
        wr8(SCR, 0);
        if(status & RDRF) (void)rd8(RDR);
        wr8(SSR, status & ~(RDRF | FLAGS));
        wr8(SPTR, 0x83u);
        if((rd32(SAR1) == RDR_PHYSICAL) && (rd32(CHCR1) & 0xffffu) == RX_DMA) wr32(CHCR1, 0);
        usable = !wire_handoff(w, true);
    }
    /* WAIT and FULL already stand in command framing with the card deselected. */
    if(usable && state != KUI_WIRE_FAULT)
        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);
    w->state = KUI_WIRE_OFF;
    return usable;
}

/* ---- the reader of the ring ---- */

/* How far the stream has landed in memory. */
static uint32_t wire_landed(const struct kui_wire_stream *w) {
    if(w->state != KUI_WIRE_RUN) return w->head;
    const uint32_t start = w->ring_physical + (w->seg_begin & MASK), address = rd32(DAR1);
    /* A transfer counted in this address had begun before the fence. */
    fence(w);
    if(rd32(SAR1) != RDR_PHYSICAL || address < start || address > start + w->seg_len) return w->seg_begin;
    return w->seg_begin + (address - start);
}

/* The first lost position at or behind `from`, or NONE. */
static uint32_t wire_hole(const struct kui_wire_stream *w, uint32_t from) {
    for(uint32_t i = w->hole_get; i != w->hole_put; ++i) {
        const uint32_t at = w->hole[i & HOLE_MASK];
        if(at >= from) return at;
    }
    return NONE;
}

#if KUI_TOY_WIRE_WORDS && KUI_TOY_WIRE_SWIFT && defined(__sh__) && !defined(KUI_WIRE_STREAM_TEST)
/* Test 74: the loops below written by hand for the SH-4, 41 instructions
 * for four bytes instead of 49, and 34 instead of 45 where no byte is
 * reversed (toy_wire_words.inc). They take the table block itself: bit
 * reversal first, the step table at 512 and the two-byte step table at 1024,
 * as wire_tables lays it out. The host tests run the C loops, held against
 * the byte steps; tests/test_toy_wire_linked_loops.py runs these as linked
 * against the same steps. */
extern uint32_t kui_wire_words_lsb(const alias_word *p, uint32_t words, uint8_t *out,
    const uint8_t *tables, uint32_t crc);
#define wire_words(w, p, words, out, crc) kui_wire_words_lsb(p, words, out, (w)->reverse, crc)
#if KUI_TOY_WIRE_MSB
extern uint32_t kui_wire_words_true(const alias_word *p, uint32_t words, uint8_t *out,
    const uint8_t *tables, uint32_t crc);
#define wire_words_true(w, p, words, out, crc) kui_wire_words_true(p, words, out, (w)->reverse, crc)
#endif
#define WIRE_WORDS 1
#elif KUI_TOY_WIRE_WORDS && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
/* Test 71. `words` whole words at the ring's aligned address p: the same
 * CRC16 and the same true bytes as the byte steps of wire_run give, with
 * the CRC taken two bytes to a step and nothing kept in memory between
 * words. Test 70's loop spent 79 instructions and nine stack accesses on
 * four bytes; this is 49 and none (checked in the linked module). The
 * tables are read as signed: the bits a sign extension adds are masked
 * off where an index is taken and once at the end. */
static uint32_t __attribute__((__noinline__, __optimize__("O2")))
wire_words(const struct kui_wire_stream *w, const alias_word *p, uint32_t words, uint8_t *out, uint32_t crc) {
    const int16_t *const one = (const int16_t *)(const void *)w->crc;
    const int16_t *const two = (const int16_t *)(const void *)w->crc2;
    const uint8_t *const reverse = w->reverse;
    int32_t c = (int32_t)crc;
    do {
        const uint32_t x = *p++;
        uint32_t t = (uint32_t)c ^ x;
        c = two[t & 0xffu] ^ one[t >> 8 & 0xffu];
        t = (uint32_t)c ^ x >> 16;
        c = two[t & 0xffu] ^ one[t >> 8 & 0xffu];
        out[0] = reverse[x & 0xffu];
        out[1] = reverse[x >> 8 & 0xffu];
        out[2] = reverse[x >> 16 & 0xffu];
        out[3] = reverse[x >> 24];
        out += 4;
    } while(--words);
    return (uint32_t)c & 0xffffu;
}
#define WIRE_WORDS 1
#if KUI_TOY_WIRE_MSB
/* Test 74. The same for a receiver that delivers bytes as the card sent
 * them: the CRC16 steps on the other table, and the bytes stored as they
 * are. The destination is not aligned as the ring is (a block begins
 * anywhere in the stream), so they are stored a byte at a time. */
static uint32_t __attribute__((__noinline__, __optimize__("O2")))
wire_words_true(const struct kui_wire_stream *w, const alias_word *p, uint32_t words, uint8_t *out, uint32_t crc) {
    const int16_t *const step = (const int16_t *)(const void *)w->crc;
    const int16_t *const pair = (const int16_t *)(const void *)w->crc2;
    int32_t remainder = (int32_t)crc;
    do {
        const uint32_t word = *p++;
        uint32_t index = (uint32_t)remainder ^ word;
        remainder = pair[index & 0xffu] ^ step[index >> 8 & 0xffu];
        index = (uint32_t)remainder ^ word >> 16;
        remainder = pair[index & 0xffu] ^ step[index >> 8 & 0xffu];
        out[0] = (uint8_t)word;
        out[1] = (uint8_t)(word >> 8);
        out[2] = (uint8_t)(word >> 16);
        out[3] = (uint8_t)(word >> 24);
        out += 4;
    } while(--words);
    return (uint32_t)remainder & 0xffffu;
}
#endif
#else
#define WIRE_WORDS 0
#endif

/* `count` received bytes from position `from` on: the CRC advanced over
 * them as they came off the wire, and their true values written to out. */
static uint32_t wire_run(const struct kui_wire_stream *w, uint32_t from, uint32_t count,
                         uint8_t *out, uint32_t crc) {
    const uint16_t *table = w->crc;
    const uint8_t *reverse = w->reverse;
#define STEP(v) do { \
        const uint32_t byte_ = (v); \
        crc = (crc >> 8) ^ table[(crc ^ byte_) & 0xffu]; \
        if(out) *out++ = reverse[byte_]; \
    } while(0)
    while(count) {
        const uint32_t index = from & MASK;
        uint32_t piece = KUI_WIRE_RING_BYTES - index;
        if(piece > count) piece = count;
        const uint8_t *p = w->ring + index;
        from += piece; count -= piece;
        for(; piece && ((uintptr_t)p & 3u); --piece) STEP(*p++);
#if WIRE_WORDS
        if(out && piece >= 4u) {
            const uint32_t words = piece >> 2;
#ifdef KUI_WIRE_STREAM_TEST
            const uint32_t before = crc;
#endif
#if KUI_TOY_WIRE_MSB
            crc = w->msb ? wire_words_true(w, (const alias_word *)(const void *)p, words, out, crc) :
                wire_words(w, (const alias_word *)(const void *)p, words, out, crc);
#else
            crc = wire_words(w, (const alias_word *)(const void *)p, words, out, crc);
#endif
#ifdef KUI_WIRE_STREAM_TEST
            /* Host tests hold every word run against the byte steps. */
            uint32_t check = before;
            for(uint32_t i = 0; i < 4u * words; ++i) {
                check = (check >> 8) ^ table[(check ^ p[i]) & 0xffu];
                assert(out[i] == reverse[p[i]]);
            }
            assert(check == crc);
#endif
            p += 4u * words; out += 4u * words; piece &= 3u;
        }
#endif
#if !WIRE_WORDS
        for(; piece >= 4u; piece -= 4u, p += 4) {
            const uint32_t word = *(const alias_word *)(const void *)p;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            STEP(word & 0xffu); STEP(word >> 8 & 0xffu); STEP(word >> 16 & 0xffu); STEP(word >> 24);
#else
            STEP(word >> 24); STEP(word >> 16 & 0xffu); STEP(word >> 8 & 0xffu); STEP(word & 0xffu);
#endif
        }
#endif
        /* With the word loop above, what is left is a word's remainder, or
         * the two CRC bytes, which are not written out. */
        for(; piece; --piece) STEP(*p++);
    }
#undef STEP
    return crc;
}

/* The wire byte whose absence, standing as zero with `after` bytes behind
 * it, leaves this remainder; or -1 if no single byte explains it. The CRC
 * is linear: undo the `after` steps, each named by its high byte, and what
 * remains is the missing byte's own table entry. */
static int wire_solve(const struct kui_wire_stream *w, uint32_t crc, uint32_t after) {
    for(; after; --after) {
        const uint32_t index = w->inverse[crc >> 8];
        crc = ((crc ^ w->crc[index]) & 0xffu) << 8 | index;
    }
    const uint32_t value = w->inverse[crc >> 8];
    return w->crc[value] == crc ? (int)value : -1;
}

/* Take the block whose data begins at `first` out of the ring. The lost
 * data bytes stand as zero and are listed; *lost_crc counts lost CRC bytes.
 * The remainder over data and CRC as received, or NONE if a block has more
 * lost bytes than can be handed on. */
static uint32_t wire_block(struct kui_wire_stream *w, uint32_t first, uint32_t *lost_data, uint32_t *lost_crc) {
    uint32_t crc = 0, at = first, hole = wire_hole(w, first);
    const uint32_t end = first + FRAME;
    *lost_data = *lost_crc = 0;
    while(at < end) {
        if(at == hole) {
            if(at - first < 512u) {
                if(*lost_data == KUI_WIRE_BLOCK_LOST) return NONE;
                w->lost[(*lost_data)++] = (uint16_t)(at - first);
                w->block[at - first] = 0;
            } else ++*lost_crc;
            crc = (crc >> 8) ^ w->crc[crc & 0xffu];
            hole = wire_hole(w, ++at);
            continue;
        }
        uint32_t stop = hole < end ? hole : end;
        /* Data and CRC bytes apart: only data is written out. */
        if(at - first < 512u && stop - first > 512u) stop = first + 512u;
        crc = wire_run(w, at, stop - at, at - first < 512u ? w->block + (at - first) : 0, crc);
        at = stop;
    }
    return crc;
}

/* Whether the position behind a block looks like the space between blocks. */
static bool wire_gap_like(const struct kui_wire_stream *w, uint32_t at) {
    if(wire_hole(w, at) == at) return true;
    const uint8_t value = w->ring[at & MASK];
    return value == IDLE_WIRE || value == TOKEN_OF(w);
}

#if KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_PACE
/* Keep what lay around the byte at `at` that is neither idle nor a token. */
static void wire_frame_note(struct kui_wire_stream *w, uint32_t at, uint32_t landed) {
    struct kui_wire_counters *n = w->n;
    const uint8_t value = w->ring[at & MASK];
    unsigned bit = 0;
    while(bit < 7u && (value >> bit & 1u)) ++bit;
    uint32_t *by_bit = &n->frame_bit[bit >> 2];
    if((*by_bit >> 8u * (bit & 3u) & 0xffu) != 0xffu) *by_bit += 1u << 8u * (bit & 3u);
    if((n->frame_kind >> 8u * w->last_kind & 0xffu) != 0xffu) n->frame_kind += 1u << 8u * w->last_kind;
#if KUI_TOY_WIRE_WORDS
    if(at == w->gap_begin && w->handed) {
        /* The byte lies where the block before should have ended: that
         * block's positions are one too many somewhere. Look at its lost
         * positions for a byte taken from the receiver's register that
         * repeats the byte the channel stored before it. */
        bool held = false, twice = false;
        for(uint32_t i = w->hole_put, seen = 0; i && seen < KUI_WIRE_HOLES; --i, ++seen) {
            const uint32_t entry = (i - 1u) & HOLE_MASK, position = w->hole[entry];
            if(position >= at) continue;
            if(at - position > FRAME) break;
            if(!(w->held[entry >> 5] >> (entry & 31u) & 1u) || position < 2u) continue;
            held = true;
            if(w->ring[(position - 1u) & MASK] == w->ring[(position - 2u) & MASK]) twice = true;
        }
        ++n->frame_end;
        if(held) ++n->frame_held;
        if(twice) ++n->frame_twice;
    }
#endif
    if(n->lost_frame >= 2u) return;
    struct kui_wire_frame_note *note = &n->frame_note[n->lost_frame];
    uint32_t words[2] = {0, 0}, lost = 0, arrived = 0, since = NONE;
    for(unsigned i = 0; i < 8u; ++i) {
        const uint32_t position = at - 4u + i;
        uint32_t byte = 0;
        /* Positions ahead of the stream's first byte, or not yet in. */
        if(i < 4u ? at >= 4u - i : position < landed) {
            byte = w->ring[position & MASK];
            if(i >= 4u) ++arrived;
        }
        words[i >> 2] = words[i >> 2] << 8 | byte;
    }
    /* The log still holds positions the reader has passed, until they are
     * written over. */
    for(uint32_t i = w->hole_put, seen = 0; i && seen < KUI_WIRE_HOLES; --i, ++seen) {
        const uint32_t position = w->hole[(i - 1u) & HOLE_MASK];
        if(position + 4u >= at && position < at + 4u) lost |= 1u << (position + 4u - at);
        if(position < at && since == NONE) since = at - position;
        if(position + 4u < at) break;
    }
    const uint32_t gap = at - w->gap_begin;
    note->before = words[0];
    note->after = words[1];
    note->gap = (gap < 0xffffu ? gap : 0xffffu) | lost << 16 | arrived << 24;
    note->since_lost = since;
    note->blocks = (w->handed & UINT32_C(0x0fffffff)) | w->last_kind << 28;
}
#endif

static enum kui_wire_parse wire_refuse(struct kui_wire_stream *w, enum kui_wire_parse why) {
    if(why == KUI_WIRE_BAD_CRC) ++w->n->bad_crc;
    else if(why == KUI_WIRE_LOST_FRAME) ++w->n->lost_frame;
    else ++w->n->ambiguous;
    return why;
}

enum kui_wire_parse kui_wire_next(struct kui_wire_stream *w, struct kui_wire_block *out) {
    if(w->state == KUI_WIRE_OFF) return KUI_WIRE_NEED;
    const uint32_t landed = wire_landed(w);
    while(w->hole_get != w->hole_put && w->hole[w->hole_get & HOLE_MASK] < w->floor) ++w->hole_get;
    if(w->phase == SEEK) {
        uint32_t at = w->tail, hole = wire_hole(w, at);
        for(; at < landed; ++at) {
            if(at == hole) {
                /* A byte lost between blocks was an idle byte or the token.
                 * Remember where data would begin had it been the token.
                 * Of two lost in one gap only the later is kept open: had
                 * the earlier been the token, nothing but idle-looking
                 * bytes lay between them, and the block's CRC, or failing
                 * that its sector's EDC, refuses the reading. */
                if(w->gap_hole) ++w->n->gap_idle;
                w->gap_hole = at + 1u;
                w->floor = at;
                hole = wire_hole(w, at + 1u);
                continue;
            }
            const uint8_t value = w->ring[at & MASK];
            if(value == IDLE_WIRE) {
                /* A full block of idle bytes cannot be data with its CRC. */
                if(w->gap_hole && at - w->gap_hole >= FRAME) {
                    w->gap_hole = 0;
                    w->floor = at;
                    ++w->n->gap_idle;
                }
                continue;
            }
#if KUI_TOY_WIRE_MSB
            if(w->n->order == KUI_WIRE_ORDER_TRY) {
                /* The stream began behind a block's CRC: the first byte
                 * that is not idle is the token, and its bits say which way
                 * round the receiver delivers them. A token that came the
                 * old way means SDIR does nothing here: this stream's bytes
                 * are read as before, and no later stream asks again. Any
                 * other byte, or a lost one ahead of it, decides nothing:
                 * the next stream asks anew. */
                if(w->gap_hole || (value != TOKEN_TRUE && value != TOKEN_WIRE)) {
                    /* Not asked for ever: the old order always works. */
                    if(++w->n->undecided >= KUI_WIRE_ASKS) w->n->order = KUI_WIRE_ORDER_LSB;
                    return wire_refuse(w, KUI_WIRE_LOST_FRAME);
                }
                if(value == TOKEN_TRUE) w->n->order = KUI_WIRE_ORDER_SEEN;
                else {
                    w->n->order = KUI_WIRE_ORDER_LSB;
                    w->msb = 0u;
                    w->token = TOKEN_WIRE;
                    wire_tables(w, w->reverse);
                }
            }
#endif
            if(value == TOKEN_OF(w)) {
                w->data_at = at + 1u;
                w->alternative = w->gap_hole;
            } else if(w->gap_hole) {
                w->data_at = w->gap_hole;
                w->alternative = 0;
                ++w->n->gap_token;
            } else {
#if KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_PACE
                wire_frame_note(w, at, landed);
#endif
                return wire_refuse(w, KUI_WIRE_LOST_FRAME);
            }
            w->phase = DATA;
            break;
        }
        if(w->phase == SEEK) {
            w->tail = at;
            if(!w->gap_hole) w->floor = at;
            return KUI_WIRE_NEED;
        }
        w->tail = w->data_at;
        w->floor = w->alternative && w->alternative < w->data_at ? w->alternative : w->data_at;
    }
    for(;;) {
        const uint32_t first = w->data_at, end = first + FRAME;
        /* With two readings of the gap open, one more byte may decide. */
        const uint32_t need = w->alternative ? end + 1u : end;
        if(landed < need) return KUI_WIRE_NEED;
        uint32_t lost_data, lost_crc;
        const uint32_t crc = wire_block(w, first, &lost_data, &lost_crc);
        bool accept = true, rebuilt = false;
        if(crc == NONE) accept = false;
        else if(!lost_data && !lost_crc) accept = !crc;
        else if(lost_data == 1u && !lost_crc
#if KUI_TOY_WIRE_MSB
            /* Until a block with no lost byte has passed its CRC16 in the
             * order the token suggested, nothing is rebuilt from a CRC16
             * taken in that order: such a block goes on with its lost byte
             * listed, for its sector's parity and EDC. */
            && w->n->order != KUI_WIRE_ORDER_SEEN
#endif
            ) {
            const int value = wire_solve(w, crc, FRAME - 1u - w->lost[0]);
            if(value >= 0) {
                w->block[w->lost[0]] = w->reverse[value];
                rebuilt = true;
            } else if(w->alternative) accept = false;
            else ++w->n->unsolved;
        } else if(w->alternative) accept = wire_gap_like(w, end);
        if(!accept) {
            if(w->alternative) {
                /* The other reading: the lost byte was the token. */
                w->data_at = w->alternative;
                w->alternative = 0;
                ++w->n->gap_token;
                continue;
            }
#if KUI_TOY_WIRE_MSB
            /* A token that looked MSB first, and then a block with no
             * lost byte fails its CRC16 before any has passed that way: do
             * not ask again. The caller's rewind starts the next stream the
             * old way. */
            if(w->n->order == KUI_WIRE_ORDER_SEEN && crc != NONE && !lost_data && !lost_crc)
                w->n->order = KUI_WIRE_ORDER_BACK;
#endif
            return wire_refuse(w, crc == NONE ? KUI_WIRE_AMBIGUOUS : KUI_WIRE_BAD_CRC);
        }
#if KUI_TOY_WIRE_MSB
        if(w->n->order == KUI_WIRE_ORDER_SEEN && !lost_data && !lost_crc) w->n->order = KUI_WIRE_ORDER_MSB;
#endif
#if KUI_TOY_WIRE_CLEAN
        out->kind = rebuilt ? KUI_WIRE_REBUILT : lost_data || lost_crc ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;
        out->rebuilt_at = w->lost[0];
#endif
        if(w->alternative) ++w->n->gap_idle;
        ++w->n->blocks;
#if KUI_TOY_WIRE_MEASURE && !KUI_TOY_WIRE_PACE
        ++w->handed;
        w->last_kind = rebuilt ? 2u : lost_data || lost_crc ? 3u : 1u;
        w->gap_begin = end;
#endif
        if(rebuilt) {++w->n->rebuilt; lost_data = 0;}
        else if(lost_data || lost_crc) {
            ++w->n->holed;
            if(!lost_data) ++w->n->unchecked;
        }
        out->data = w->block;
        out->lost = w->lost;
        out->lost_count = lost_data;
        w->tail = w->floor = end;
        w->phase = SEEK;
        w->gap_hole = w->alternative = 0;
        return KUI_WIRE_BLOCK;
    }
}
