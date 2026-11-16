/* SPDX-License-Identifier: GPL-3.0-only */
/* Hardware model for the auxiliary transport, not a timing emulation. */
#include "retail_async.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { RA_CLOSED, RA_PAUSED, RA_DMA, RA_LOST, RA_RESUME };
static uint32_t read_hw(uint32_t address,unsigned width);
#include "../src/loader/toy_sci_burst.inc"

static struct {
    uint32_t smr,brr,scr,ssr,rdr,scmr,sptr,stbcr,pdtr;
    uint32_t sar,dar,tcr,chcr,dmaor;
    unsigned fences,writes,starts,stops,clocked,phase,command,bytes,gap,late_at;
    bool bad_stop;
#if KUI_TOY_SUITE_MODE == 28
    bool refuse_slow_rate,refuse_scr_stop;
#endif
} hw;
static kui_retail_async_region_type region;
static uint8_t slab[TOY_BURST_SLAB_BYTES] __attribute__((aligned(32)));
static uint8_t other[TOY_BURST_SLAB_BYTES] __attribute__((aligned(32)));

static uint32_t *reg(uint32_t address) {
    switch(address) {
    case TOY_BURST_SMR: return &hw.smr;
    case TOY_BURST_BRR: return &hw.brr;
    case TOY_BURST_SCR: return &hw.scr;
    case TOY_BURST_SSR: return &hw.ssr;
    case TOY_BURST_RDR: return &hw.rdr;
    case TOY_BURST_SCMR: return &hw.scmr;
    case TOY_BURST_SPTR: return &hw.sptr;
    case TOY_BURST_STBCR: return &hw.stbcr;
    case TOY_BURST_PDTR: return &hw.pdtr;
    case TOY_BURST_SAR: return &hw.sar;
    case TOY_BURST_DAR: return &hw.dar;
    case TOY_BURST_TCR: return &hw.tcr;
    case TOY_BURST_CHCR: return &hw.chcr;
    case TOY_BURST_DMAOR: return &hw.dmaor;
    default: assert(false);return NULL;
    }
}
static uint32_t read_hw(uint32_t address,unsigned width) {
    (void)width;
    return *reg(address);
}
void kui_toy_sci_burst_test_write(uint32_t address,unsigned width,uint32_t value) {
    (void)width;
    ++hw.writes;
#if KUI_TOY_SUITE_MODE == 28
    if(address==TOY_BURST_BRR) {
        assert(!hw.scr);
        if(value==1u && hw.refuse_slow_rate)return;
    }
    if(address==TOY_BURST_SCR && !value && hw.refuse_scr_stop)return;
#endif
    *reg(address)=value;
    if(address==TOY_BURST_STBCR && (value&1u)) {
        hw.smr=hw.scr=hw.scmr=0;
        hw.brr=0xffu;
        hw.ssr=0x84u;
    }
}
uint32_t kui_toy_sci_burst_test_physical(const void *buffer) {
    return buffer==slab?0x0c100000u:buffer==other?0x0c102000u:0;
}
void kui_toy_sci_burst_test_fence(const void *buffer) {
    assert(buffer==slab || buffer==other);
    ++hw.fences;
    if(hw.fences==hw.late_at) {--hw.tcr;++hw.dar;}
}

enum { CARD_IDLE,CARD_COMMAND,CARD_STUFF,CARD_REPLY,CARD_TOKEN,CARD_PAYLOAD };
static void card_select(void *unused,bool selected) {
    (void)unused;
    assert(hw.scr!=0x50u && region.engine.in_irq==0);
    hw.pdtr=selected?0u:0x80u;
}
static uint8_t card_byte(void *unused,uint8_t byte,bool slow) {
    (void)unused;
    assert(!slow && hw.scr==0x30u && hw.brr==0u && region.engine.in_irq==0);
    ++hw.clocked;
    if(hw.pdtr&0x80u) return 0xffu;
    if(hw.phase!=CARD_COMMAND && (byte==0x52u || byte==0x4cu)) {
        hw.command=byte;
        hw.phase=CARD_COMMAND;
        hw.bytes=1;
        return 0xffu;
    }
    switch(hw.phase) {
    case CARD_COMMAND:
        if(++hw.bytes==6u) {
            if(hw.command==0x4cu) {++hw.stops;hw.phase=CARD_STUFF;}
            else {++hw.starts;hw.phase=CARD_REPLY;}
        }
        return 0xffu;
    case CARD_STUFF:
        hw.phase=CARD_REPLY;
        return 0xffu;
    case CARD_REPLY:
        hw.phase=hw.command==0x52u?CARD_TOKEN:CARD_IDLE;
        return hw.command==0x4cu && hw.bad_stop?0x08u:0u;
    case CARD_TOKEN:
        if(hw.gap) {--hw.gap;return 0xffu;}
        hw.phase=CARD_PAYLOAD;
        return 0xfeu;
    default:
        return 0xffu;
    }
}
static void setup(void) {
    memset(&hw,0,sizeof(hw));
    memset(&region,0,sizeof(region));
    memset(slab,0,sizeof(slab));
    hw.smr=0x80u;hw.scr=0x30u;hw.ssr=0x84u;hw.pdtr=0x80u;hw.dmaor=1u;
    hw.sar=0x0c0a1120u;hw.dar=0x0c040020u;hw.tcr=23u;hw.chcr=0x4400u;
    struct kui_loader_sd *card=&region.shared.card.device.sd;
    card->blocks=1u<<19;
    card->ready=card->high_capacity=true;
    card->bus.select=card_select;
    card->bus.transfer=card_byte;
    region.shared.stream.card=card;
    region.shared.port.acquired=true;
    region.engine.opened=1u;
}
static void finished(unsigned received,unsigned planned) {
    assert(received<=planned);
    memset(slab,0x6c,received);
    hw.dar=kui_toy_sci_burst_test_physical(slab)+received;
    hw.tcr=planned-received;
    hw.chcr=TOY_BURST_RX_DMA|(received==planned?2u:0u);
    hw.ssr=0xe4u;
}

static void test_receive_cuts(bool slow) {
    const unsigned bytes=15u*516u-2u;
    uint32_t received;
    for(unsigned length=0;length<=bytes;++length) {
        setup();
#if KUI_TOY_SUITE_MODE == 28
        assert(toy_sci_burst_start_rate(&region,7123u,15u,slab,slow)==KUI_SCI_STREAM_OK);
#else
        assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
#endif
        assert(hw.starts==1u && hw.scr==0x50u && hw.tcr==bytes &&
            hw.brr==(uint32_t)slow &&
            region.shared.stream.state==KUI_TOY_SCI_BURST_DMA);
        unsigned writes=hw.writes,clocked=hw.clocked;
        region.engine.in_irq=1u;
        assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_PENDING);
        assert(!received && hw.writes==writes);
        assert(toy_sci_burst_stop(&region,slab,15u)==KUI_SCI_STREAM_BUSY);
        finished(length,bytes);
        assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_OK);
        assert(received==length && hw.fences==(length==bytes?2u:6u) && hw.clocked==clocked &&
            hw.sar==0x0c0a1120u && hw.dar==0x0c040020u && hw.tcr==23u &&
            hw.chcr==0x4400u && hw.scr==0x30u && hw.brr==0u && hw.pdtr==0x80u);
        assert(region.shared.stream.stats.overruns==(length!=bytes));
        region.engine.in_irq=0;
        assert(toy_sci_burst_stop(&region,slab,15u)==KUI_SCI_STREAM_OK);
        assert(hw.stops==1u && region.shared.stream.state==RA_CLOSED);
    }
}

int main(void) {
    const unsigned bytes=15u*516u-2u;
    uint32_t received;
    test_receive_cuts(false);
#if KUI_TOY_SUITE_MODE == 28
    test_receive_cuts(true);
    setup();hw.gap=300u;
    assert(toy_sci_burst_start_rate(&region,7123u,15u,slab,true)==KUI_SCI_STREAM_PENDING);
    assert(hw.brr==0u && hw.scr==0x30u && hw.pdtr==0x80u);
    assert(toy_sci_burst_start_rate(&region,7123u,15u,slab,true)==KUI_SCI_STREAM_OK);
    assert(hw.starts==1u && hw.brr==1u);
    assert(toy_sci_burst_stop(&region,slab,15u)==KUI_SCI_STREAM_RESET);
    assert(region.shared.port.fault && hw.scr==0x30u && hw.brr==0u &&
        hw.chcr==0x4400u && !hw.stops);

    setup();hw.refuse_slow_rate=true;
    assert(toy_sci_burst_start_rate(&region,7123u,15u,slab,true)==KUI_SCI_STREAM_RESET);
    assert(region.shared.port.fault && hw.brr==0u && hw.scr==0x30u &&
        hw.chcr==0x4400u && hw.pdtr==0x80u);

    setup();hw.refuse_scr_stop=true;
    assert(toy_sci_burst_start_rate(&region,7123u,15u,slab,true)==KUI_SCI_STREAM_RESET);
    assert(region.shared.port.fault && hw.brr==0u && hw.chcr==0x4400u);
#endif
    setup();hw.gap=600u;
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_PENDING);
    assert(region.shared.stream.state==KUI_TOY_SCI_BURST_TOKEN && hw.pdtr==0x80u);
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_PENDING);
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    assert(hw.starts==1u);
    assert(toy_sci_burst_poll(&region,other,15u,&received)==KUI_SCI_STREAM_RESET);
    assert(!received && region.shared.stream.state==KUI_TOY_SCI_BURST_DMA);
    finished(1536u,bytes);
    assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_OK);
    assert(received==1536u);
    hw.bad_stop=true;
    assert(toy_sci_burst_stop(&region,slab,15u)==KUI_SCI_STREAM_RESET);
    assert(region.shared.port.fault && hw.pdtr==0x80u);
    assert(toy_sci_burst_start(&region,7126u,15u,slab)==KUI_SCI_STREAM_RESET);
    assert(hw.starts==1u);

    setup();
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    finished(1535u,bytes);hw.late_at=4u;
    assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_OK);
    assert(received==1536u && hw.fences==8u && hw.dar==0x0c040020u);
    setup();hw.chcr=0x4401u;
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_PENDING);
    assert(!hw.writes && !hw.clocked);
    setup();
    assert(toy_sci_burst_start(&region,7123u,16u,slab)==KUI_SCI_STREAM_RESET);
    assert(!hw.writes && !hw.clocked);
    setup();
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    hw.sar=0x0c0fedccu;hw.chcr=0x6401u;hw.ssr=0xe4u;
    assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_RESET);
    assert(!received && hw.sar==0x0c0fedccu && hw.chcr==0x6401u && region.shared.port.fault);
    setup();
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    finished(1024u,bytes);++hw.tcr;
    assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_RESET);
    assert(!received && region.shared.port.fault && !(hw.chcr&1u));
    setup();
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    finished(1024u,bytes);hw.ssr=0xd4u;
    assert(toy_sci_burst_poll(&region,slab,15u,&received)==KUI_SCI_STREAM_RESET);
    assert(!received && region.shared.port.fault && !(hw.chcr&1u) && !hw.stops);
    setup();
    assert(toy_sci_burst_start(&region,7123u,15u,slab)==KUI_SCI_STREAM_OK);
    assert(toy_sci_burst_stop(&region,slab,15u)==KUI_SCI_STREAM_RESET);
    assert(region.shared.port.fault && hw.scr==0x30u && !(hw.chcr&1u) && !hw.stops);
    puts("Toy SCI burst transport passed all 7,739 prefix cuts, PIO-free IRQ retirement, sliced token, owned restore, bad count/stop, foreign DMA and stalled cancellation");
#if KUI_TOY_SUITE_MODE == 28
    puts("6.25 MHz receive passed all 7,739 cuts, token wait, cancellation, and rate-transition refusals");
#endif
    return 0;
}
