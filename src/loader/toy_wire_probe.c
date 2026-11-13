/* SPDX-License-Identifier: GPL-3.0-only
 * Toy Commander Test 67, the SCI wire probe (see toy_wire_probe.h).
 *
 * One DMA run receives a whole 16 KiB segment of the card's CMD18 stream:
 * tokens, data, CRC and gap bytes alike, across block boundaries. The
 * receiver holds one byte, so a bus stall longer than a byte time (0.64 us
 * at 12.5 MHz) overruns it; the byte in the shifter is lost and the SCI
 * stops clocking the card. The SCI's error interrupt then ends the run.
 *
 * Every register sequence below is the one sci_stream.c already runs on the
 * console: finish() to stop and count, handoff() with its MSTP0 module
 * reset after an overrun, and select + start_dma() to receive on from the
 * byte after the lost one (the in-place resume of Tests 48-51). New here is
 * only the policy: no per-block stop, and a wait before resuming when the
 * previous run was short. The SDK's interrupt table delivers the events, as
 * it did for the SD reader in Tests 32-66. */
#include "toy_wire_probe.h"
#include "sci_sd_bus.h"
#include "kui/toy_pilot_clock.h"
#include <string.h>

#define SMR UINT32_C(0xffe00000)
#define BRR UINT32_C(0xffe00004)
#define SCR UINT32_C(0xffe00008)
#define SSR UINT32_C(0xffe00010)
#define RDR UINT32_C(0xffe00014)
#define SCMR UINT32_C(0xffe00018)
#define SPTR UINT32_C(0xffe0001c)
#define STBCR UINT32_C(0xffc00004)
#define FRQCR UINT32_C(0xffc00000)
#define PDTR UINT32_C(0xff800030)
#define SAR1 UINT32_C(0xffa00010)
#define DAR1 UINT32_C(0xffa00014)
#define TCR1 UINT32_C(0xffa00018)
#define CHCR1 UINT32_C(0xffa0001c)
#define DMAOR UINT32_C(0xffa00040)
#define IPRA UINT32_C(0xffd00004)
#define IPRB UINT32_C(0xffd00008)
#define TSTR UINT32_C(0xffd80004)
#define TCOR0 UINT32_C(0xffd80008)
#define TCNT0 UINT32_C(0xffd8000c)
#define TCR0 UINT32_C(0xffd80010)
/* Channel 1: SCI receive request, byte units, destination incrementing, no
 * completion interrupt. RIE+RE clock the card without a transmitter; with
 * SPTR.EIO only the receiver's error reaches the CPU. */
#define RX_DMA UINT32_C(0x4911)
#define RDRF 0x40u
#define ORER 0x20u
#define FLAGS 0x38u
#define TEND 0x04u
#define CS UINT16_C(0x0080)
#define SEGMENT KUI_WIRE_SEGMENT_BYTES
#define SCI_FIELD 0x00f0u
#define SCI_LEVEL 0x0010u /* level 1, the SD reader's proven level */
/* The exact Toy Commander executable (SHA-256 ee68a326...9bbd, checked by
 * the stage before this module is installed) keeps its SDK interrupt table
 * here: one RTS callback per event, 4 bytes per 0x20 of INTEVT. Recorded in
 * the handoff evidence toy-graphics-priority-final-original-sdk-callback. */
#define SDK_VBR UINT32_C(0x8c00f400)
#define SDK_ERI_SLOT UINT32_C(0x8c00f65c)
#define SDK_DEFAULT UINT32_C(0x8c0be408)
#define EVENT_ERI 0x4e0u
#define EVENT_RXI 0x500u
#define INLINE static inline __attribute__((__always_inline__))

#ifdef KUI_WIRE_TEST
extern uint32_t kui_wire_test_read(uint32_t address, unsigned width);
extern void kui_wire_test_write(uint32_t address, uint32_t value, unsigned width);
extern void kui_wire_test_settle(unsigned count);
extern void kui_wire_test_fence(void);
extern uint32_t kui_wire_test_physical(const void *area);
extern uint32_t kui_wire_test_spc(void);
extern uint32_t kui_wire_test_vbr(void);
#define rd8(a) ((uint8_t)kui_wire_test_read(a, 1))
#define rd16(a) ((uint16_t)kui_wire_test_read(a, 2))
#define rd32(a) kui_wire_test_read(a, 4)
#define wr8(a,v) kui_wire_test_write(a, (uint8_t)(v), 1)
#define wr16(a,v) kui_wire_test_write(a, (uint16_t)(v), 2)
#define wr32(a,v) kui_wire_test_write(a, v, 4)
#define settle kui_wire_test_settle
#define fence kui_wire_test_fence
#define physical kui_wire_test_physical
#define interrupted_pc kui_wire_test_spc
#define vector_base kui_wire_test_vbr
#define word_read(a) kui_wire_test_read(a, 4)
#define word_publish(a,v) kui_wire_test_write(a, v, 4)
static uint32_t irq_mask(void) { return 0u; }
static void irq_restore(uint32_t sr) { (void)sr; }
#define CALLBACK UINT32_C(0x8cfd1234)
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
INLINE uint32_t interrupted_pc(void) {
    uint32_t pc;
    __asm__ __volatile__("stc spc,%0" : "=r"(pc));
    return pc;
}
INLINE uint32_t vector_base(void) {
    uint32_t vbr;
    __asm__ __volatile__("stc vbr,%0" : "=r"(vbr));
    return vbr;
}
INLINE uint32_t irq_mask(void) {
    uint32_t sr;
    __asm__ __volatile__("stc sr,%0" : "=r"(sr));
    uint32_t masked = sr | 0xf0u;
    __asm__ __volatile__("ldc %0,sr" : : "r"(masked) : "memory");
    return sr;
}
INLINE void irq_restore(uint32_t sr) {
    __asm__ __volatile__("ldc %0,sr" : : "r"(sr) : "t", "memory");
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
/* This module is linked and installed in the uncached mirror. The callback
 * alone is entered through its cached alias: the stage invalidated both
 * caches before the game started, nothing has executed in this lease from
 * the cached side since, and this code is never modified. Its literals are
 * read-only; all its data stays at the linked uncached addresses. */
#define CALLBACK (((uint32_t)(uintptr_t)kui_wire_isr & UINT32_C(0x1fffffff)) | UINT32_C(0x80000000))
#endif

struct wire {
    struct kui_wire_summary s;
    struct kui_wire_counters now;
    uint32_t run_off;    /* segment offset where the current run began */
    uint32_t resume_off; /* where a paused stream receives on */
    uint32_t backoff, pause_tick, run_tick;
    uint32_t gd_busy, gd_tick, gd_bytes;
    uint32_t sptr, saved_dma[4], saved_iprb, saved_ipra;
    uint32_t timer_reg, timer_bit, timer_event, timer_slot, timer_shift;
    uint32_t hooked;
    struct kui_wire_window *sample; /* the movie whose cuts are sampled */
    uint32_t started[2];
    uint32_t cut_count, tail_filled;
    uint16_t cut_log[KUI_WIRE_CUT_LOG];
    struct kui_loader_sd card;
};
static struct wire wire;
static _Alignas(32) uint8_t wire_buffer[SEGMENT];
#ifdef KUI_WIRE_TEST
uint8_t *kui_wire_test_buffer(void) { return wire_buffer; }
#endif

#ifndef KUI_WIRE_TEST
/* An uncached read of main memory waits for the external bus, and the DMAC
 * goes before the CPU there: a transfer the channel had begun has landed
 * when it returns (sci_stream.c). */
INLINE void fence(void) { (void)*(volatile uint32_t *)(void *)wire_buffer; }
#endif

/* Four table bytes per 0x20 of event code. */
INLINE uint32_t slot_of(uint32_t event) {
    return SDK_ERI_SLOT - (EVENT_ERI >> 3) + (event >> 3);
}

INLINE void timer_cancel(const struct wire *w) {
    wr8(TSTR, rd8(TSTR) & ~w->timer_bit);
    wr16(w->timer_reg + 8u, 0x0002u); /* no interrupt, underflow cleared */
    (void)rd16(w->timer_reg + 8u);
}
/* One shot: the callback stops the channel again. */
INLINE void timer_arm(const struct wire *w, uint32_t ticks) {
    wr8(TSTR, rd8(TSTR) & ~w->timer_bit);
    wr32(w->timer_reg, UINT32_C(0xffffffff));
    wr32(w->timer_reg + 4u, ticks);
    wr16(w->timer_reg + 8u, 0x0022u); /* underflow interrupt, Pphi/64 */
    wr8(TSTR, rd8(TSTR) | w->timer_bit);
}

INLINE void wire_fault(struct wire *w, uint32_t error, uint32_t detail) {
    wr8(SCR, 0);
    if(w->s.mode) timer_cancel(w);
    w->s.state = KUI_WIRE_FAULT;
    if(!w->s.error) {w->s.error = error; w->s.detail = detail;}
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
INLINE uint32_t wire_handoff(const struct wire *w, bool overrun) {
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

/* sci_stream.c start_dma(): receive the segment's bytes from offset on. */
INLINE bool wire_start_dma(uint32_t offset) {
    wr8(SPTR, 0x83u); /* TxD latched high as GPIO before the transmitter is off */
    if((rd8(SPTR) & 0x8au) != 0x82u) return false;
    wr8(SCR, 0);
    wr32(CHCR1, 0);
    wr32(SAR1, RDR & UINT32_C(0x1fffffff));
    wr32(DAR1, physical(wire_buffer) + offset);
    wr32(TCR1, SEGMENT - offset);
    wr32(CHCR1, RX_DMA);
    wr8(SCR, 0x50u);
    return true;
}

INLINE void wire_arm(struct wire *w, uint32_t offset) {
    wire_select();
    if(!wire_start_dma(offset)) {wire_fault(w, KUI_WIRE_E_ARM, rd8(SPTR)); return;}
    w->run_off = offset;
    w->run_tick = rd32(TCNT0);
    w->s.state = KUI_WIRE_RUN;
}

/* sci_stream.c finish(): stop the SCI and the channel, count what landed,
 * keep the RDR byte and hand the SCI back to command bytes. status carries
 * SSR, with bit 8 set if the channel had reached its count. */
INLINE bool wire_capture(struct wire *w, uint32_t *count_out, uint32_t *status_out, uint8_t *tail_out) {
    const uint32_t start = physical(wire_buffer);
    uint32_t control = 0, count = 1;
    if(rd32(CHCR1) & 2u)
        for(unsigned n = 0; n < 512u && !(rd8(SSR) & ORER); ++n) {}
    wr8(SCR, 0);
    fence();
    uint32_t address = rd32(DAR1);
    bool ours = rd32(SAR1) == (RDR & UINT32_C(0x1fffffff)) && address >= start &&
        address <= start + SEGMENT;
    if(ours) {
        wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(5));
        (void)rd32(CHCR1);
        fence();
        control = rd32(CHCR1); count = rd32(TCR1);
        ours = rd32(DAR1) == start + SEGMENT - count;
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

INLINE void wire_segment_done(struct wire *w) {
    ++w->s.segments;
    w->cut_count = 0;
}

/* Account a run the receiver's overrun ended. next is the segment offset to
 * receive on from; cut is set for a byte lost inside the segment. False
 * after a fault. */
INLINE bool wire_account(struct wire *w, uint32_t count, uint32_t status, uint8_t tail,
                         bool *cut, uint32_t *run_out, uint32_t *next) {
    const uint32_t at = SEGMENT - count, run = at - w->run_off, held = status & RDRF ? 1u : 0u;
    *cut = false;
    *run_out = run;
    if((status & FLAGS) != ORER) {
        wire_fault(w, KUI_WIRE_E_STOP, (status & 0x1ffu) | count << 16);
        return false;
    }
    if(!count) {
        /* Behind a full segment: the byte waiting in RDR, unless a stall
         * made the channel's last transfer late, and the one that overran.
         * Neither belongs to the segment. */
        if(!(status & 0x100u)) {wire_fault(w, KUI_WIRE_E_END, status); return false;}
        w->now.bytes += run + held + 1u;
        w->tail_filled = SEGMENT;
        *next = 0u;
        return true;
    }
    /* The channel has `at` bytes. If RDR still held one, it is byte `at`
     * and the lost byte follows it; otherwise byte `at` itself is lost. */
    const uint32_t lost = at + held;
    if(held) wire_buffer[at] = tail;
    w->now.bytes += run + held + 1u;
    w->tail_filled = lost < SEGMENT ? lost : SEGMENT;
    *cut = true;
    if(w->cut_count < KUI_WIRE_CUT_LOG) w->cut_log[w->cut_count] = (uint16_t)lost;
    ++w->cut_count;
    *next = lost + 1u;
    return true;
}

INLINE void wire_resume(struct wire *w, uint32_t now) {
    const uint32_t waited = w->pause_tick - now;
    w->now.paused_ticks += waited;
    if(w->sample && waited > w->sample->pause_max) w->sample->pause_max = waited;
    wire_arm(w, w->resume_off);
}

/* The receiver overran: a cut, or the two bytes behind a full segment. */
INLINE void wire_overrun(struct wire *w, uint32_t now) {
    uint32_t count, status, run, next;
    uint8_t tail;
    bool cut;
    if(!wire_capture(w, &count, &status, &tail)) return;
    const uint32_t before = w->now.bytes;
    if(!wire_account(w, count, status, tail, &cut, &run, &next)) return;
    /* Two wire bytes take one timer tick. Whatever else passed since this
     * run began, the stopped receiver spent waiting for this call. */
    const uint32_t elapsed = w->run_tick - now, wired = (w->now.bytes - before + 1u) >> 1;
    if(elapsed > wired) w->now.late_ticks += elapsed - wired;
    if(cut) {
        ++w->now.cuts;
        if(w->gd_busy && w->sample) ++w->sample->gd_cuts;
        ++w->now.run[run < 16u ? 0u : run < 64u ? 1u : run < 256u ? 2u :
                     run < 1024u ? 3u : run < 4096u ? 4u : 5u];
        struct kui_wire_window *win = w->sample;
        if(win) {
            /* An even sample of this movie's cuts whatever their number:
             * when the table fills, keep every other entry and halve the rate. */
            const uint32_t index = w->now.cuts - win->begin.cuts - 1u;
            if(!(index & win->pc_mask)) {
                if(win->pc_count == KUI_WIRE_PC_SAMPLES) {
                    for(unsigned i = 0; i < KUI_WIRE_PC_SAMPLES / 2u; ++i) win->pc[i] = win->pc[2u * i];
                    win->pc_count = KUI_WIRE_PC_SAMPLES / 2u;
                    win->pc_mask = win->pc_mask << 1 | 1u;
                }
                if(!(index & win->pc_mask)) win->pc[win->pc_count++] = interrupted_pc();
            }
        }
    }
    /* A byte lost at the segment's very end leaves nothing to resume into. */
    if(next >= SEGMENT) {next = 0u; cut = false;}
    if(!next) wire_segment_done(w);
    if(!cut || run >= KUI_WIRE_GOOD_RUN) {
        /* Only a long run shows the bus is quiet again. */
        if(run >= KUI_WIRE_GOOD_RUN) w->backoff = KUI_WIRE_BACKOFF_MIN;
        wire_arm(w, next);
        return;
    }
    w->resume_off = next;
    w->pause_tick = now;
    w->s.state = KUI_WIRE_PAUSED;
    ++w->now.pauses;
    if(w->s.mode) timer_arm(w, w->backoff);
    if(w->backoff < KUI_WIRE_BACKOFF_MAX) w->backoff <<= 1;
}

void kui_wire_isr(uint32_t event) {
    struct wire *w = &wire;
    const uint32_t entered = rd32(TCNT0);
    ++w->now.irqs;
    if(w->s.mode && event == w->timer_event) {
        timer_cancel(w);
        if(w->s.state == KUI_WIRE_PAUSED) wire_resume(w, entered);
    } else if(event == EVENT_ERI && w->s.state == KUI_WIRE_RUN) wire_overrun(w, entered);
    else ++w->s.spurious;
    w->now.isr_ticks += entered - rd32(TCNT0);
}

void kui_wire_poll(void) {
    struct wire *w = &wire;
    if(!w->hooked) return;
    /* Without a free timer channel, hook visits are the only later chance. */
    if(w->s.state == KUI_WIRE_PAUSED && !w->s.mode) wire_resume(w, rd32(TCNT0));
    /* The SDK may rewrite the priority register for its own devices. */
    uint16_t levels = rd16(IPRB);
    if((levels & SCI_FIELD) != SCI_LEVEL) wr16(IPRB, (levels & ~SCI_FIELD) | SCI_LEVEL);
}

static uint8_t wire_byte(struct wire *w, uint8_t value) {
    return w->card.bus.transfer(w->card.bus.ctx, value, false);
}
static void wire_card_select(struct wire *w, bool selected) {
    w->card.bus.select(w->card.bus.ctx, selected);
}
static uint8_t crc7(const uint8_t *p) {
    unsigned c = 0;
    for(unsigned i = 0; i < 5; ++i) for(unsigned j = 0; j < 8; ++j) {
        c <<= 1;
        if(((p[i] >> (7u - j)) ^ (c >> 7)) & 1u) c ^= 9u;
    }
    return (uint8_t)((c << 1) | 1u);
}
/* R1, or 0xff if none came within the response window (sci_stream.c). */
static uint8_t wire_command(struct wire *w, const uint8_t packet[6], bool stop) {
    for(unsigned i = 0; i < 6; ++i) (void)wire_byte(w, packet[i]);
    if(stop) (void)wire_byte(w, 0xff); /* CMD12's stuff byte */
    uint8_t value = 0xff;
    for(unsigned i = 0; i < 20u && (value & 0x80u); ++i) value = wire_byte(w, 0xff);
    return value;
}
/* CMD12 into the stream, its busy, deselection and an idle byte. */
static bool wire_stop_card(struct wire *w) {
    static const uint8_t stop[6] = {0x4c, 0, 0, 0, 0, 0x61};
    wire_card_select(w, true);
    bool stopped = !(wire_command(w, stop, true) & ~0x04u);
    for(uint32_t n = 0; wire_byte(w, 0xff) != 0xffu; ++n)
        if(n >= 200000u || !kui_sci_sd_healthy()) {stopped = false; break;}
    wire_card_select(w, false);
    (void)wire_byte(w, 0xff);
    return stopped && kui_sci_sd_healthy();
}

static void wire_refuse(struct wire *w, uint32_t error, uint32_t detail) {
    if(!w->s.error) {w->s.error = error; w->s.detail = detail;}
}

static void timer_select(struct wire *w) {
    const uint32_t running = rd8(TSTR), levels = rd16(IPRA);
    w->s.mode = 0;
    for(unsigned k = 2u; k; --k) {
        const uint32_t reg = TCOR0 + 12u * k, event = 0x400u + 0x20u * k, shift = 12u - 4u * k;
        if((running & (1u << k)) || (rd16(reg + 8u) & 0x0020u) || ((levels >> shift) & 15u) ||
           word_read(slot_of(event)) != SDK_DEFAULT) continue;
        w->timer_reg = reg; w->timer_bit = 1u << k; w->timer_event = event;
        w->timer_slot = slot_of(event); w->timer_shift = shift;
        w->s.mode = k;
        return;
    }
}

static void wire_unhook(struct wire *w) {
    if(!w->hooked) return;
    if(w->s.mode) {
        timer_cancel(w);
        wr16(IPRA, rd16(IPRA) & ~(15u << w->timer_shift));
        if(word_read(w->timer_slot) == CALLBACK) word_publish(w->timer_slot, SDK_DEFAULT);
    }
    uint16_t levels = rd16(IPRB);
    if((levels & SCI_FIELD) == SCI_LEVEL)
        wr16(IPRB, (levels & ~SCI_FIELD) | (w->saved_iprb & SCI_FIELD));
    for(unsigned i = 0; i < 2u; ++i)
        if(word_read(SDK_ERI_SLOT + 4u * i) == CALLBACK) word_publish(SDK_ERI_SLOT + 4u * i, SDK_DEFAULT);
    w->hooked = 0;
}

static bool clock_valid(void) {
    return kui_toy_pilot_clock_profile(rd16(FRQCR), rd32(TCOR0), rd16(TCR0), rd8(TSTR));
}

static void wire_open(struct wire *w) {
    if(!clock_valid()) {wire_refuse(w, KUI_WIRE_E_CLOCK, rd16(TCR0)); return;}
    uint32_t sr = irq_mask();
    enum kui_loader_sd_result result = kui_sci_sd_acquire();
    irq_restore(sr);
    if(result != KUI_LOADER_SD_OK) {wire_refuse(w, KUI_WIRE_E_ACQUIRE, result); return;}
    result = kui_loader_sd_init_bus(&w->card, kui_sci_sd_bus());
    if(result != KUI_LOADER_SD_OK) {
        kui_sci_sd_release(); /* the reader's failure path already has */
        wire_refuse(w, KUI_WIRE_E_INIT, result);
        return;
    }
    uint32_t error = KUI_WIRE_OK, detail = 0;
    bool streaming = false;
    if(!w->card.ready || w->card.slow ||
       w->card.blocks < (uint64_t)KUI_WIRE_FIRST_BLOCK + KUI_WIRE_SPAN_BLOCKS) {
        error = KUI_WIRE_E_CARD; detail = (uint32_t)w->card.blocks;
        goto refuse;
    }
    /* sci_stream.c open: a deselected idle byte at the fast rate. */
    w->sptr = rd8(SPTR);
    wire_card_select(w, false);
    (void)wire_byte(w, 0xff);
    if(!kui_sci_sd_healthy() || (w->sptr & 0x0au) || rd8(SCR) != 0x30u || rd8(BRR) != 0) {
        error = KUI_WIRE_E_BUS; detail = w->sptr | (uint32_t)rd8(SCR) << 8 | (uint32_t)rd8(BRR) << 16;
        goto refuse;
    }
    /* sci_stream.c start(): idle, CMD18, R1, then the first data token. */
    const uint32_t address = w->card.high_capacity ? KUI_WIRE_FIRST_BLOCK : KUI_WIRE_FIRST_BLOCK << 9;
    uint8_t packet[6] = {0x52, (uint8_t)(address >> 24), (uint8_t)(address >> 16),
        (uint8_t)(address >> 8), (uint8_t)address, 0};
    packet[5] = crc7(packet);
    wire_card_select(w, true);
    for(uint32_t n = 0; wire_byte(w, 0xff) != 0xffu;)
        if(++n >= 50000u || !kui_sci_sd_healthy()) {
            wire_card_select(w, false);
            error = KUI_WIRE_E_IDLE;
            goto refuse;
        }
    streaming = true; /* from here on, CMD18 may be running */
    uint8_t value = wire_command(w, packet, false);
    if(!kui_sci_sd_healthy() || value) {error = KUI_WIRE_E_COMMAND; detail = value; goto refuse;}
    value = 0xff;
    for(uint32_t n = 0; n < 65536u && value == 0xffu; ++n) value = wire_byte(w, 0xff);
    if(value != 0xfeu || !kui_sci_sd_healthy()) {error = KUI_WIRE_E_TOKEN; detail = value; goto refuse;}
    for(unsigned k = 0; k < 64u && !(rd8(SSR) & TEND); ++k) {}

    sr = irq_mask();
    if(vector_base() != SDK_VBR || word_read(SDK_ERI_SLOT) != SDK_DEFAULT ||
       word_read(SDK_ERI_SLOT + 4u) != SDK_DEFAULT) {
        error = KUI_WIRE_E_VECTOR; detail = word_read(SDK_ERI_SLOT);
    } else if((rd32(CHCR1) & 7u) || (rd32(DMAOR) & 7u) != 1u) {
        error = KUI_WIRE_E_CHANNEL; detail = (rd32(CHCR1) & 0xffffu) | rd32(DMAOR) << 16;
    } else {
        for(unsigned i = 0; i < 4u; ++i) w->saved_dma[i] = rd32(SAR1 + 4u * i);
        timer_select(w);
        w->saved_iprb = rd16(IPRB);
        w->run_off = 0; w->backoff = KUI_WIRE_BACKOFF_MIN;
        w->cut_count = 0; w->tail_filled = 0;
        for(unsigned i = 0; i < 2u; ++i) word_publish(SDK_ERI_SLOT + 4u * i, CALLBACK);
        if(w->s.mode) {
            word_publish(w->timer_slot, CALLBACK);
            timer_cancel(w);
            wr16(IPRA, rd16(IPRA) | 1u << w->timer_shift);
        }
        wr16(IPRB, (w->saved_iprb & ~SCI_FIELD) | SCI_LEVEL);
        w->hooked = 1u;
        settle(64); /* the token byte's last edge */
        if(wire_start_dma(0)) {
            w->run_tick = rd32(TCNT0);
            w->s.state = KUI_WIRE_RUN;
        } else {
            error = KUI_WIRE_E_START; detail = rd8(SPTR);
            wr8(SPTR, w->sptr); wr8(SCR, 0x30u);
            wire_unhook(w);
            for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);
        }
    }
    irq_restore(sr);
    if(!error) return;
refuse:
    if(streaming) (void)wire_stop_card(w);
    kui_sci_sd_release();
    wire_refuse(w, error, detail);
}

/* Stop the stream, give every borrowed register back and close the card. */
static void wire_close(struct wire *w) {
    uint32_t sr = irq_mask();
    const uint32_t state = w->s.state;
    if(state != KUI_WIRE_RUN && state != KUI_WIRE_PAUSED && state != KUI_WIRE_FAULT) {
        irq_restore(sr);
        return;
    }
    bool usable = true;
    if(w->s.mode) timer_cancel(w);
    if(state == KUI_WIRE_RUN) {
        /* A whole byte ends at the next overrun, a cut or the segment's end:
         * at most one segment of wire time away. */
        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}
        uint32_t count, status, run, next;
        uint8_t tail;
        bool cut;
        if(!wire_capture(w, &count, &status, &tail)) usable = false;
        else if(!(status & ORER)) {
            /* The wait ran out and the receiver was stopped as it stood:
             * what the channel stored still counts. */
            w->now.bytes += SEGMENT - count - w->run_off;
            w->tail_filled = SEGMENT - count;
        } else if(!wire_account(w, count, status, tail, &cut, &run, &next)) usable = false;
        else if(cut) ++w->now.cuts;
    } else if(state == KUI_WIRE_PAUSED) {
        w->now.paused_ticks += w->pause_tick - rd32(TCNT0);
    } else {
        /* A fault left the SCI stopped at an unknown step: try the same
         * handoff once more so CMD12 can still reach the card. */
        wr8(SCR, 0);
        if(rd32(SAR1) == (RDR & UINT32_C(0x1fffffff))) wr32(CHCR1, 0);
        uint8_t status = rd8(SSR);
        if(status & RDRF) (void)rd8(RDR);
        wr8(SSR, status & ~(RDRF | FLAGS));
        wr8(SPTR, 0x83u); /* the pin state the module reset expects */
        usable = !wire_handoff(w, true);
    }
    if(rd32(SAR1) == (RDR & UINT32_C(0x1fffffff)) && !(rd32(CHCR1) & 1u))
        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);
    wire_unhook(w);
    w->s.state = state == KUI_WIRE_FAULT || !usable ? KUI_WIRE_FAULT : KUI_WIRE_DONE;
    irq_restore(sr);
    if(usable && kui_sci_sd_healthy()) {
        if(!wire_stop_card(w)) wire_refuse(w, KUI_WIRE_E_CLOSE, 1u);
    } else wire_refuse(w, KUI_WIRE_E_CLOSE, 2u);
    wr8(SPTR, w->sptr);
    kui_sci_sd_release();
    if(kui_sci_sd_faulted()) wire_refuse(w, KUI_WIRE_E_CLOSE, 3u);
}

/* Counters with the bytes of the run still in flight. Interrupts masked. */
static struct kui_wire_counters wire_snapshot(const struct wire *w) {
    struct kui_wire_counters c = w->now;
    if(w->s.state == KUI_WIRE_RUN) {
        const uint32_t stored = SEGMENT - rd32(TCR1);
        if(stored >= w->run_off && stored <= SEGMENT) c.bytes += stored - w->run_off;
    }
    return c;
}

/* Close the disc-read interval that is open, into the window that is. */
static void gd_settle(struct wire *w, uint32_t now, uint32_t bytes) {
    if(w->gd_busy && w->sample) {
        w->sample->gd_ticks += w->gd_tick - now;
        w->sample->gd_bytes += bytes - w->gd_bytes;
    }
    w->gd_tick = now;
    w->gd_bytes = bytes;
}

void kui_wire_gd(bool busy) {
    struct wire *w = &wire;
    gd_settle(w, rd32(TCNT0), wire_snapshot(w).bytes);
    w->gd_busy = busy;
}

static void window_close(struct wire *w, unsigned movie) {
    struct kui_wire_window *win = &w->s.window[movie];
    if(!win->open || win->closed) return;
    uint32_t sr = irq_mask();
    const struct kui_wire_counters c = wire_snapshot(w);
    const uint32_t now = rd32(TCNT0);
    gd_settle(w, now, c.bytes);
    win->span = win->begin_tick - now;
    win->total.bytes = c.bytes - win->begin.bytes;
    win->total.cuts = c.cuts - win->begin.cuts;
    win->total.irqs = c.irqs - win->begin.irqs;
    win->total.isr_ticks = c.isr_ticks - win->begin.isr_ticks;
    win->total.pauses = c.pauses - win->begin.pauses;
    win->total.paused_ticks = c.paused_ticks - win->begin.paused_ticks;
    win->total.late_ticks = c.late_ticks - win->begin.late_ticks;
    for(unsigned i = 0; i < KUI_WIRE_RUN_BUCKETS; ++i) win->total.run[i] = c.run[i] - win->begin.run[i];
    win->closed = 1u;
    if(w->sample == win) w->sample = NULL;
    irq_restore(sr);
}

void kui_wire_movie_start(unsigned movie) {
    struct wire *w = &wire;
    if(movie > 1u || w->started[movie]) return;
    w->started[movie] = 1u;
    if(w->s.state == KUI_WIRE_RUN || w->s.state == KUI_WIRE_PAUSED) return;
    if(w->s.state == KUI_WIRE_FAULT) wire_close(w);
    if(movie) ++w->s.restarts;
    wire_open(w);
}

void kui_wire_frame(unsigned movie) {
    struct wire *w = &wire;
    if(movie > 1u) return;
    struct kui_wire_window *win = &w->s.window[movie];
    if(win->open) return;
    uint32_t sr = irq_mask();
    kui_wire_poll();
    win->begin = wire_snapshot(w);
    win->begin_tick = rd32(TCNT0);
    win->pc_count = 0; win->pc_mask = 0; win->pause_max = 0;
    win->gd_ticks = win->gd_bytes = win->gd_cuts = 0;
    win->open = 1u;
    /* A disc read already in flight counts from here. */
    w->gd_tick = win->begin_tick; w->gd_bytes = win->begin.bytes;
    w->sample = win;
    irq_restore(sr);
}

void kui_wire_movie_end(unsigned movie) {
    struct wire *w = &wire;
    if(movie > 1u) return;
    window_close(w, movie);
    if(movie) wire_close(w);
}

static uint8_t rev8(uint8_t x) {
    x = (uint8_t)(x >> 4 | x << 4);
    x = (uint8_t)((x & 0xccu) >> 2 | (x & 0x33u) << 2);
    return (uint8_t)((x & 0xaau) >> 1 | (x & 0x55u) << 1);
}
static uint16_t crc16(uint16_t crc, uint8_t data) {
    uint32_t x = (crc >> 8) ^ data;
    x ^= x >> 4;
    return (uint16_t)((crc << 8) ^ (x << 12) ^ (x << 5) ^ x);
}
static bool holed(const struct wire *w, uint32_t first, uint32_t last) {
    for(uint32_t i = 0; i < w->cut_count && i < KUI_WIRE_CUT_LOG; ++i)
        if(w->cut_log[i] >= first && w->cut_log[i] <= last) return true;
    return false;
}
/* What the last segment holds, checked at leisure: SD blocks framed by
 * their token whose CRC16 agrees. Buffer positions are wire positions, a
 * lost byte keeps its slot, so framing survives a cut. ok counts intact
 * blocks; holed counts blocks a logged cut fell into; bad counts a framed
 * block without a cut whose CRC disagrees, which must stay zero. The SCI
 * shifts LSB first, so wire bytes are bit-reversed. */
static void wire_tail(struct wire *w) {
    if(w->s.state != KUI_WIRE_DONE) return; /* a faulted stream's last stop is not known */
    const uint32_t end = w->tail_filled <= SEGMENT ? w->tail_filled : 0u;
    w->s.tail_bytes = end;
    if(w->cut_count > KUI_WIRE_CUT_LOG) {w->s.tail_holed = UINT32_MAX; return;}
    bool synced = false;
    for(uint32_t p = 0; p + 515u <= end;) {
        const uint8_t byte = wire_buffer[p];
        if(byte != 0x7fu) {
            /* Between framed blocks only the card's 0xff gap may stand. */
            if(byte != 0xffu) synced = false;
            ++p;
            continue;
        }
        const bool hole = holed(w, p, p + 514u);
        uint16_t crc = 0;
        for(unsigned i = 0; i < 512u; ++i) crc = crc16(crc, rev8(wire_buffer[p + 1u + i]));
        const uint16_t sent = (uint16_t)(rev8(wire_buffer[p + 513u]) << 8 | rev8(wire_buffer[p + 514u]));
        if(crc == sent && !hole) {++w->s.tail_ok; synced = true; p += 515u; continue;}
        if(synced && hole) {++w->s.tail_holed; p += 515u; continue;}
        if(synced) {++w->s.tail_bad; synced = false;}
        ++p;
    }
}

/* The two 64-byte code windows most often interrupted by a cut. */
static void window_reduce(struct kui_wire_window *win) {
    for(unsigned rank = 0; rank < 2u; ++rank) {
        uint32_t best = 0, best_count = 0;
        for(uint32_t i = 0; i < win->pc_count; ++i) {
            const uint32_t bucket = win->pc[i] & ~UINT32_C(63);
            bool taken = false;
            for(unsigned j = 0; j < rank; ++j) if(win->top_count[j] && win->top_pc[j] == bucket) taken = true;
            if(taken) continue;
            uint32_t count = 0;
            for(uint32_t j = 0; j < win->pc_count; ++j) if((win->pc[j] & ~UINT32_C(63)) == bucket) ++count;
            if(count > best_count) {best = bucket; best_count = count;}
        }
        win->top_pc[rank] = best;
        /* Scale the sample back to cuts. */
        win->top_count[rank] = best_count * (win->pc_mask + 1u);
    }
}

void kui_wire_shutdown(void) {
    struct wire *w = &wire;
    for(unsigned i = 0; i < 2u; ++i) window_close(w, i);
    wire_close(w);
    for(unsigned i = 0; i < 2u; ++i) window_reduce(&w->s.window[i]);
    wire_tail(w);
}

const struct kui_wire_summary *kui_wire_summary(void) { return &wire.s; }
