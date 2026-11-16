/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real Test 41 helper with delayed SCI flags and strict credit. */
#include "sci_sd_bus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SMR 0xffe00000u
#define BRR 0xffe00004u
#define SCR 0xffe00008u
#define TDR 0xffe0000cu
#define SSR 0xffe00010u
#define RDR 0xffe00014u
#define PTR 0xffe00018u
#define STB 0xffc00004u
#define PCTR 0xff80002cu
#define PDTR 0xff800030u
enum { TDRE = 0x80u, RDRF = 0x40u, ORER = 0x20u, TEND = 0x04u, CARD_OFF = 0x80u };
enum { PAYLOAD = 512u, WIRE = PAYLOAD + 2u, GUARD = 16u };

static struct {
    uint8_t smr, brr, scr, tdr, ssr, rdr, ptr, stb;
    uint16_t pdtr;
    uint32_t pctr;
    unsigned reads, writes, scr_enables, tdr_writes, launched, drained, polls, delay;
    unsigned timeout_at, overrun_at;
    bool shifting, saw_tdre, saw_rdrf;
} hw;

static uint8_t expected(unsigned index) {
    if(index >= PAYLOAD) return index == PAYLOAD ? 0x96u : 0x3du;
    return (uint8_t)(11u + index * 37u);
}

static uint8_t reverse_reference(uint8_t value) {
    uint8_t reversed = 0u;
    for(unsigned i = 0u; i < 8u; ++i) {
        reversed = (uint8_t)((reversed << 1) | (value & 1u));
        value >>= 1;
    }
    return reversed;
}

static void progress(void) {
    if(!hw.shifting) return;
    /* The transmit holding register clears before the pending wire byte
     * reaches RDR. TDRE on its own must never grant another receive. */
    hw.ssr |= TDRE;
    if(hw.launched == hw.timeout_at) return;
    if(--hw.delay) return;
    hw.shifting = false;
    hw.ssr |= TEND;
    if(hw.launched == hw.overrun_at) {
        hw.ssr |= ORER;
        return;
    }
    assert(!(hw.ssr & RDRF));
    hw.rdr = reverse_reference(expected(hw.launched - 1u));
    hw.ssr |= RDRF;
}

uint32_t kui_sci_sd_test_read(uint32_t address, unsigned width) {
    ++hw.reads;
    switch(address) {
    case SMR: assert(width == 1u); return hw.smr;
    case BRR: assert(width == 1u); return hw.brr;
    case SCR: assert(width == 1u); return hw.scr;
    case SSR:
        assert(width == 1u);
        ++hw.polls;
        progress();
        if(hw.ssr & TDRE) hw.saw_tdre = true;
        if(hw.ssr & RDRF) hw.saw_rdrf = true;
        return hw.ssr;
    case RDR:
        assert(width == 1u && hw.saw_rdrf && (hw.ssr & RDRF));
        assert(hw.launched == hw.drained + 1u && !hw.shifting);
        ++hw.drained;
        return hw.rdr;
    case PTR: assert(width == 1u); return hw.ptr;
    case STB: assert(width == 1u); return hw.stb;
    case PDTR: assert(width == 2u); return hw.pdtr;
    case PCTR: assert(width == 4u); return hw.pctr;
    default: assert(!"unexpected SCI read (DMA and other MMIO are forbidden)"); return 0u;
    }
}

void kui_sci_sd_test_write(uint32_t address, uint32_t value, unsigned width) {
    ++hw.writes;
    switch(address) {
    case SMR: assert(width == 1u); hw.smr = (uint8_t)value; return;
    case BRR: assert(width == 1u && !hw.shifting); hw.brr = (uint8_t)value; return;
    case SCR:
        assert(width == 1u && (value == 0u || value == 0x30u));
        hw.scr = (uint8_t)value;
        if(value) ++hw.scr_enables;
        else if(hw.shifting) {
            assert(hw.launched == hw.timeout_at);
            hw.shifting = false;
            hw.ssr |= TDRE | TEND;
        }
        return;
    case TDR:
        assert(width == 1u && value == 0xffu && (hw.ssr & TDRE) && hw.saw_tdre);
        ++hw.tdr_writes;
        hw.tdr = (uint8_t)value;
        return;
    case SSR: {
        assert(width == 1u);
        bool launch = (hw.scr & 0x20u) && (hw.ssr & TDRE) && !(value & TDRE);
        if((hw.ssr & RDRF) && !(value & RDRF)) {
            assert(hw.saw_rdrf && hw.launched == hw.drained);
            hw.saw_rdrf = false;
        }
        hw.ssr = (uint8_t)((hw.ssr & value & 0xf8u) | (hw.ssr & 7u));
        if(launch) {
            assert(hw.saw_tdre && hw.tdr == 0xffu && hw.scr == 0x30u && !hw.brr);
            assert(!(hw.pdtr & CARD_OFF));
            assert(!hw.shifting && hw.launched == hw.drained && !(hw.ssr & RDRF));
            ++hw.launched;
            hw.saw_tdre = false;
            hw.shifting = true;
            hw.delay = 4u + hw.launched % 5u;
            hw.ssr &= (uint8_t)~TEND;
        }
        return;
    }
    case PTR: assert(width == 1u); hw.ptr = (uint8_t)value; return;
    case STB: assert(width == 1u); hw.stb = (uint8_t)value; return;
    case PDTR:
        assert(width == 2u && (!(value & CARD_OFF) || !hw.shifting));
        hw.pdtr = (uint16_t)value;
        return;
    case PCTR: assert(width == 4u); hw.pctr = value; return;
    default: assert(!"unexpected SCI write (DMA and other MMIO are forbidden)");
    }
}

void kui_sci_sd_test_delay(uint32_t count) { assert(count >= 64u); }

static void run_payload(unsigned timeout_at, unsigned overrun_at) {
    uint8_t guarded[GUARD + WIRE + GUARD];
    memset(guarded, 0xa5, sizeof(guarded));
    memset(&hw, 0, sizeof(hw));
    hw.ssr = TDRE | TEND;
    hw.pdtr = 0x1256u;
    assert(!kui_sci_sd_pio_payload(guarded + GUARD) && !hw.reads && !hw.writes);
    assert(kui_sci_sd_acquire() == KUI_LOADER_SD_OK);
    const struct kui_loader_sd_bus *bus = kui_sci_sd_bus();
    assert(bus && !bus->transfer_block);
    bus->select(bus->ctx, true);
    unsigned reads = hw.reads, writes = hw.writes;
    assert(!kui_sci_sd_pio_payload(guarded + GUARD));
    assert(hw.reads == reads && hw.writes == writes && !hw.launched);
    /* The real caller has already read its data token through fast PIO. */
    assert(bus->transfer(bus->ctx, 0xffu, false) == expected(0u));
    hw.tdr_writes = hw.scr_enables = hw.launched = hw.drained = hw.polls = 0u;
    hw.timeout_at = timeout_at;
    hw.overrun_at = overrun_at;
    bool fault = timeout_at || overrun_at;
    assert(kui_sci_sd_pio_payload(guarded + GUARD) == !fault);
    assert(hw.tdr_writes == 1u && hw.scr_enables == 1u);
    assert(hw.launched == (timeout_at ? timeout_at : overrun_at ? overrun_at : WIRE));
    assert(hw.drained == hw.launched - (unsigned)fault);
    assert(kui_sci_sd_healthy() == !fault && !hw.shifting);
    for(unsigned i = 0u; i < sizeof(guarded); ++i) {
        unsigned at = i - GUARD;
        assert(guarded[i] == (at < hw.drained ? expected(at) : 0xa5u));
    }
    if(fault) {
        assert(!hw.scr && hw.polls < 14000u);
        unsigned clocks = hw.launched;
        assert(!kui_sci_sd_pio_payload(guarded + GUARD) && hw.launched == clocks);
    } else {
        assert(!(hw.ssr & (RDRF | ORER)) && (hw.ssr & TEND));
        assert(guarded[GUARD + PAYLOAD] == 0x96u && guarded[GUARD + PAYLOAD + 1u] == 0x3du);
    }
    bus->select(bus->ctx, false);
    kui_sci_sd_release();
    assert(hw.pdtr == 0x1256u && !hw.scr);
}

int main(void) {
    run_payload(0u, 0u);
    run_payload(77u, 0u);
    run_payload(0u, WIRE);
    puts("Test 41 real SCI PIO helper: one-byte credit, 514 exact bytes, CRC tail and bounded faults passed");
    return 0;
}
