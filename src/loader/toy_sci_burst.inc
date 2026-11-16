/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Bounded receive-only CMD18 bursts for the native Toy trial. This code
 * runs on the guarded auxiliary stack. The existing low SCI interrupt
 * route dispatches it while the burst marker is live. The diagnostic
 * established a contiguous DMA prefix; only the caller's CRC parser
 * can publish complete blocks from that prefix.
 */
#define TOY_BURST_SMR UINT32_C(0xffe00000)
#define TOY_BURST_BRR UINT32_C(0xffe00004)
#define TOY_BURST_SCR UINT32_C(0xffe00008)
#define TOY_BURST_SSR UINT32_C(0xffe00010)
#define TOY_BURST_RDR UINT32_C(0xffe00014)
#define TOY_BURST_SCMR UINT32_C(0xffe00018)
#define TOY_BURST_SPTR UINT32_C(0xffe0001c)
#define TOY_BURST_STBCR UINT32_C(0xffc00004)
#define TOY_BURST_PDTR UINT32_C(0xff800030)
#define TOY_BURST_SAR UINT32_C(0xffa00010)
#define TOY_BURST_DAR UINT32_C(0xffa00014)
#define TOY_BURST_TCR UINT32_C(0xffa00018)
#define TOY_BURST_CHCR UINT32_C(0xffa0001c)
#define TOY_BURST_DMAOR UINT32_C(0xffa00040)
#define TOY_BURST_RX_DMA UINT32_C(0x4911)
#define TOY_BURST_RDRF 0x40u
#define TOY_BURST_ORER 0x20u
#define TOY_BURST_FLAGS 0x38u
#define TOY_BURST_TEND 0x04u
enum { TOY_BURST_MAX_FRAMES=15u, TOY_BURST_SLAB_BYTES=8192u,
       TOY_BURST_TOKEN_SLICE=256u, TOY_BURST_TOKEN_LIMIT=8192u,
       TOY_BURST_STOP_LIMIT=1024u };

#ifdef KUI_TOY_SCI_BURST_HOST_TEST
extern void kui_toy_sci_burst_test_write(uint32_t,unsigned,uint32_t);
extern uint32_t kui_toy_sci_burst_test_physical(const void *);
extern void kui_toy_sci_burst_test_fence(const void *);
#define toy_burst_write kui_toy_sci_burst_test_write
#define toy_burst_physical kui_toy_sci_burst_test_physical
#define toy_burst_fence kui_toy_sci_burst_test_fence
static void toy_burst_settle(unsigned count) { (void)count; }
#else
static inline void toy_burst_write(uint32_t reg,unsigned width,uint32_t value) {
    if(width==1u) *(volatile uint8_t *)(uintptr_t)reg=(uint8_t)value;
    else if(width==2u) *(volatile uint16_t *)(uintptr_t)reg=(uint16_t)value;
    else *(volatile uint32_t *)(uintptr_t)reg=value;
}
static uint32_t toy_burst_physical(const void *slab) {
    /* Only the auxiliary's exclusively owned, uncached receive slabs. */
    uintptr_t address=(uintptr_t)slab;
    if((address&31u) || address<UINT32_C(0xac010000) ||
       address>UINT32_C(0xad000000)-TOY_BURST_SLAB_BYTES) return 0;
    return (uint32_t)address&UINT32_C(0x1fffffff);
}
static void toy_burst_fence(const void *slab) {
    (void)*(volatile const uint32_t *)slab;
}
static void toy_burst_settle(unsigned count) {
    __asm__ __volatile__("1: dt %0\n\tbf 1b" : "+r"(count) : : "t", "memory");
}
#endif
#define toy_burst_read8(reg) read_hw(reg,1u)
#define toy_burst_read32(reg) read_hw(reg,4u)
#define toy_burst_write8(reg,value) toy_burst_write(reg,1u,value)
#define toy_burst_write32(reg,value) toy_burst_write(reg,4u,value)

static bool toy_burst_bus_valid(const kui_retail_async_region_type *r) {
    const struct kui_loader_sd *card=&r->shared.card.device.sd;
    return r->engine.opened && r->shared.port.acquired && !r->shared.port.fault &&
        r->shared.stream.card==card && card->ready && card->bus.select &&
        card->bus.transfer && !(toy_burst_read8(TOY_BURST_STBCR)&1u);
}
static bool toy_burst_framing(const struct kui_sci_stream_state *stream) {
    return toy_burst_read8(TOY_BURST_SCR)==0x30u &&
        toy_burst_read8(TOY_BURST_SMR)==0x80u && !toy_burst_read8(TOY_BURST_BRR) &&
        !toy_burst_read8(TOY_BURST_SCMR) &&
        (toy_burst_read8(TOY_BURST_SPTR)&0x8au)==(stream->sptr&0x8au) &&
        (toy_burst_read8(TOY_BURST_SSR)&0xfcu)==0x84u;
}
static uint8_t toy_burst_byte(const struct kui_loader_sd_bus *bus,uint8_t value) {
    return bus->transfer(bus->ctx,value,false);
}
static uint8_t toy_burst_command(const struct kui_loader_sd_bus *bus,
    const uint8_t packet[6],bool stop) {
    for(unsigned n=0;n<6u;++n) (void)toy_burst_byte(bus,packet[n]);
    if(stop) (void)toy_burst_byte(bus,0xffu);
    uint8_t response=0xffu;
    for(unsigned n=0;n<20u && (response&0x80u);++n)
        response=toy_burst_byte(bus,0xffu);
    return response;
}
static uint8_t toy_burst_crc7(const uint8_t packet[6]) {
    unsigned crc=0;
    for(unsigned n=0;n<5u;++n) for(unsigned bit=0;bit<8u;++bit) {
        crc<<=1;
        if(((packet[n]>>(7u-bit))^(crc>>7))&1u) crc^=9u;
    }
    return (uint8_t)((crc<<1)|1u);
}
/* Failed cleanup quarantines the lease. No retry may start at an unknown
 * card position, and no command is ever clocked over an active receive. */
static enum kui_sci_stream_result toy_burst_close(kui_retail_async_region_type *r) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    if(stream->state==RA_CLOSED) return KUI_SCI_STREAM_OK;
    if(r->engine.in_irq || stream->state==KUI_TOY_SCI_BURST_DMA ||
       stream->state==RA_DMA) return KUI_SCI_STREAM_BUSY;
    if(!toy_burst_bus_valid(r) || !toy_burst_framing(stream)) {
        r->shared.port.fault=true;
        return KUI_SCI_STREAM_RESET;
    }
    const struct kui_loader_sd_bus *bus=&stream->card->bus;
    const uint8_t stop[6]={0x4cu,0,0,0,0,0x61u};
    bus->select(bus->ctx,true);
    uint8_t response=toy_burst_command(bus,stop,true);
    unsigned busy=TOY_BURST_STOP_LIMIT;
    while(busy && toy_burst_byte(bus,0xffu)!=0xffu) --busy;
    bus->select(bus->ctx,false);
    (void)toy_burst_byte(bus,0xffu);
    if(!busy || (response&~0x04u) || r->shared.port.fault ||
       !(read_hw(TOY_BURST_PDTR,2u)&0x80u)) {
        r->shared.port.fault=true;
        return KUI_SCI_STREAM_RESET;
    }
    stream->state=RA_CLOSED;
    return KUI_SCI_STREAM_OK;
}
static enum kui_sci_stream_result toy_burst_handoff(
    kui_retail_async_region_type *r,uint32_t status) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    if(status&TOY_BURST_RDRF) (void)toy_burst_read8(TOY_BURST_RDR);
    toy_burst_write8(TOY_BURST_SSR,status&~(TOY_BURST_RDRF|TOY_BURST_FLAGS));
    (void)toy_burst_read8(TOY_BURST_SSR);
    /* RE is already clear and ORER ended receive clocking. Deselect on
     * our admitted PA7 lease without invoking the PIO TEND wait in IRQ. */
    toy_burst_write(TOY_BURST_PDTR,2u,read_hw(TOY_BURST_PDTR,2u)|0x80u);
    if(r->shared.port.fault || !(read_hw(TOY_BURST_PDTR,2u)&0x80u))
        return KUI_SCI_STREAM_RESET;
    if(status&TOY_BURST_ORER) {
        uint32_t before=toy_burst_read8(TOY_BURST_STBCR);
        if(before&1u || toy_burst_read8(TOY_BURST_SCR) ||
           toy_burst_read8(TOY_BURST_SSR)&(TOY_BURST_RDRF|TOY_BURST_FLAGS) ||
           (toy_burst_read8(TOY_BURST_SPTR)&0x8au)!=0x82u)
            return KUI_SCI_STREAM_RESET;
        toy_burst_write8(TOY_BURST_STBCR,before|1u);
        bool stopped=(toy_burst_read8(TOY_BURST_STBCR)&1u)!=0;
        toy_burst_settle(64u);
        toy_burst_write8(TOY_BURST_STBCR,toy_burst_read8(TOY_BURST_STBCR)&~1u);
        if(toy_burst_read8(TOY_BURST_STBCR)&1u) return KUI_SCI_STREAM_RESET;
        toy_burst_settle(64u);
        if(!stopped || toy_burst_read8(TOY_BURST_SCR) ||
           toy_burst_read8(TOY_BURST_SMR) || toy_burst_read8(TOY_BURST_BRR)!=0xffu ||
           toy_burst_read8(TOY_BURST_SCMR) ||
           (toy_burst_read8(TOY_BURST_SSR)&0xfcu)!=0x84u ||
           (toy_burst_read8(TOY_BURST_SPTR)&0x8au)!=0x82u) return KUI_SCI_STREAM_RESET;
    }
    toy_burst_write8(TOY_BURST_SCMR,0);
    toy_burst_write8(TOY_BURST_SMR,0x80u);
    toy_burst_write8(TOY_BURST_BRR,0);
    toy_burst_settle(64u);
    toy_burst_write8(TOY_BURST_SPTR,stream->sptr);
    toy_burst_write8(TOY_BURST_SCR,0x30u);
    return toy_burst_framing(stream)?KUI_SCI_STREAM_OK:KUI_SCI_STREAM_RESET;
}
/* Foreground only. Return PENDING for a sliced token search or a channel
 * owned by the game. A later visit resumes that token, not another CMD18. */
#if KUI_TOY_SUITE_MODE == 28
static enum kui_sci_stream_result toy_sci_burst_start_rate(kui_retail_async_region_type *r,
    uint32_t lba,unsigned frames,uint8_t *slab,bool slow) {
#else
static enum kui_sci_stream_result toy_sci_burst_start(kui_retail_async_region_type *r,
    uint32_t lba,unsigned frames,uint8_t *slab) {
#endif
    struct kui_sci_stream_state *stream=&r->shared.stream;
    const struct kui_loader_sd *card=&r->shared.card.device.sd;
    uint32_t address=toy_burst_physical(slab);
    if(r->engine.in_irq || stream->state==KUI_TOY_SCI_BURST_DMA ||
       stream->state==RA_DMA) return KUI_SCI_STREAM_BUSY;
    if(!address || !frames || frames>TOY_BURST_MAX_FRAMES ||
       lba>UINT32_MAX-(frames-1u) || (uint64_t)lba+frames>card->blocks ||
       (!card->high_capacity && lba>UINT32_MAX/512u) ||
       !toy_burst_bus_valid(r) || !toy_burst_framing(stream)) return KUI_SCI_STREAM_RESET;
    if((toy_burst_read32(TOY_BURST_CHCR)&7u) ||
       (toy_burst_read32(TOY_BURST_DMAOR)&7u)!=1u) return KUI_SCI_STREAM_PENDING;
    const struct kui_loader_sd_bus *bus=&card->bus;
    if(stream->state!=KUI_TOY_SCI_BURST_TOKEN || stream->position!=lba) {
        enum kui_sci_stream_result stop=toy_burst_close(r);
        if(stop!=KUI_SCI_STREAM_OK) return stop;
        uint32_t argument=card->high_capacity?lba:lba<<9;
        uint8_t packet[6]={0x52u,(uint8_t)(argument>>24),(uint8_t)(argument>>16),
            (uint8_t)(argument>>8),(uint8_t)argument,0};
        packet[5]=toy_burst_crc7(packet);
        bus->select(bus->ctx,true);
        unsigned idle=TOY_BURST_TOKEN_SLICE;
        while(idle && toy_burst_byte(bus,0xffu)!=0xffu) --idle;
        if(!idle || r->shared.port.fault) {
            bus->select(bus->ctx,false);
            return r->shared.port.fault?KUI_SCI_STREAM_RESET:KUI_SCI_STREAM_COMMAND;
        }
        stream->state=RA_LOST;
        ++stream->stats.starts;
        if(toy_burst_command(bus,packet,false) || r->shared.port.fault) {
            bus->select(bus->ctx,false);
            return r->shared.port.fault?KUI_SCI_STREAM_RESET:KUI_SCI_STREAM_COMMAND;
        }
        stream->state=KUI_TOY_SCI_BURST_TOKEN;
        stream->position=lba;
        stream->ready_lba[0]=TOY_BURST_TOKEN_LIMIT;
        stream->ready[0]=stream->ready[1]=stream->kept=0;
        stream->arrived=0;
    } else bus->select(bus->ctx,true);
    unsigned slice=TOY_BURST_TOKEN_SLICE;
    uint8_t token=0xffu;
    while(slice && stream->ready_lba[0] && token==0xffu) {
        token=toy_burst_byte(bus,0xffu);
        --stream->ready_lba[0];
        --slice;
    }
    if(token!=0xfeu || r->shared.port.fault) {
        bus->select(bus->ctx,false);
        if(r->shared.port.fault) return KUI_SCI_STREAM_RESET;
        if(token==0xffu && stream->ready_lba[0]) return KUI_SCI_STREAM_PENDING;
        ++stream->stats.token_errors;
        stream->state=RA_LOST;
        return KUI_SCI_STREAM_TOKEN;
    }
    /* The first token was clocked by PIO. The remaining bytes are raw
     * LSB-first receive bytes. Leave room for every frame's two CRC bytes. */
    uint32_t wire_bytes=frames*516u-2u;
    toy_burst_settle(64u);
    toy_burst_write8(TOY_BURST_SPTR,0x83u);
    if((toy_burst_read8(TOY_BURST_SPTR)&0x8au)!=0x82u) {
        bus->select(bus->ctx,false);
        r->shared.port.fault=true;
        stream->state=RA_LOST;
        return KUI_SCI_STREAM_RESET;
    }
    for(unsigned n=0;n<4u;++n)
        stream->saved[n]=toy_burst_read32(TOY_BURST_SAR+4u*n);
    stream->ready_lba[0]=address;
    stream->ready_lba[1]=wire_bytes;
    toy_burst_write8(TOY_BURST_SCR,0);
#if KUI_TOY_SUITE_MODE == 28
    if(slow) {
        /* CMD18 and token search used the fast command clock. Switch only
         * this payload, with both SCI directions disabled. As in the CE
         * slow receive, settle for more than one 6.25 MHz bit time. */
        if(toy_burst_read8(TOY_BURST_SCR)) {
            stream->state=RA_LOST;
            r->shared.port.fault=true;
            return KUI_SCI_STREAM_RESET;
        }
        toy_burst_write8(TOY_BURST_BRR,1u);
        toy_burst_settle(64u);
        if(toy_burst_read8(TOY_BURST_BRR)!=1u) {
            /* DMA was never armed. Restore fast framing if SCI is still
             * ours, then quarantine the failed rate transition. */
            (void)toy_burst_handoff(r,toy_burst_read8(TOY_BURST_SSR));
            stream->state=RA_LOST;
            r->shared.port.fault=true;
            return KUI_SCI_STREAM_RESET;
        }
    }
#endif
    toy_burst_write32(TOY_BURST_CHCR,0);
    toy_burst_write32(TOY_BURST_SAR,TOY_BURST_RDR&UINT32_C(0x1fffffff));
    toy_burst_write32(TOY_BURST_DAR,address);
    toy_burst_write32(TOY_BURST_TCR,wire_bytes);
    toy_burst_write32(TOY_BURST_CHCR,TOY_BURST_RX_DMA);
    stream->state=KUI_TOY_SCI_BURST_DMA;
    toy_burst_write8(TOY_BURST_SCR,0x50u);
    return KUI_SCI_STREAM_OK;
}
#if KUI_TOY_SUITE_MODE == 28
static inline enum kui_sci_stream_result toy_sci_burst_start(kui_retail_async_region_type *r,
    uint32_t lba,unsigned frames,uint8_t *slab) {
    return toy_sci_burst_start_rate(r,lba,frames,slab,false);
}
#endif
/* Wait only for a hardware event during ordinary visits. Retire a bounded
 * burst prefix after the same trailing or premature ORER that the single
 * block reader uses. Never treat RDR or a byte clocked after ORER as part
 * of the prefix, and never touch a channel taken over by the game. */
static enum kui_sci_stream_result toy_burst_retire(kui_retail_async_region_type *r,
    const uint8_t *slab,unsigned frames,uint32_t *received,bool cancel) {
    struct kui_sci_stream_state *stream=&r->shared.stream;
    *received=0;
    uint32_t start=toy_burst_physical(slab);
    uint32_t wire_bytes=frames*516u-2u;
    if(stream->state!=KUI_TOY_SCI_BURST_DMA || !start || !frames ||
       frames>TOY_BURST_MAX_FRAMES || start!=stream->ready_lba[0] ||
       wire_bytes!=stream->ready_lba[1]) return KUI_SCI_STREAM_RESET;
    bool bus_ok=toy_burst_bus_valid(r);
    uint32_t control=toy_burst_read32(TOY_BURST_CHCR);
    uint32_t next=toy_burst_read32(TOY_BURST_DAR);
    bool channel_owned=(control&~2u)==TOY_BURST_RX_DMA &&
        toy_burst_read32(TOY_BURST_SAR)==(TOY_BURST_RDR&UINT32_C(0x1fffffff)) &&
        next>=start && next<=start+wire_bytes;
    bool sci_owned=bus_ok && toy_burst_read8(TOY_BURST_SCR)==0x50u &&
        (toy_burst_read8(TOY_BURST_SPTR)&0x8au)==0x82u &&
        !(read_hw(TOY_BURST_PDTR,2u)&0x80u);
    bool dma_ok=(toy_burst_read32(TOY_BURST_DMAOR)&7u)==1u;
    uint32_t status=sci_owned?toy_burst_read8(TOY_BURST_SSR):0;
    if(!cancel && channel_owned && sci_owned && dma_ok && !(status&TOY_BURST_FLAGS))
        return KUI_SCI_STREAM_PENDING;
    if(sci_owned) toy_burst_write8(TOY_BURST_SCR,0);
    uint32_t remaining=wire_bytes;
    if(channel_owned) {
        toy_burst_fence(slab);
        toy_burst_write32(TOY_BURST_CHCR,control&~5u);
        (void)toy_burst_read32(TOY_BURST_CHCR);
        toy_burst_fence(slab);
        remaining=toy_burst_read32(TOY_BURST_TCR);
        /* An incomplete receive can still have a byte accepted on the bus
         * after DE is clear. Use the diagnostic's bounded stability check
         * before restoring the guest tuple or exposing the stopped prefix. */
        uint32_t first=remaining, stable=remaining?0u:4u;
        for(unsigned n=0;n<16u && stable<4u;++n) {
            toy_burst_fence(slab);
            toy_burst_settle(256u);
            uint32_t count=toy_burst_read32(TOY_BURST_TCR);
            if(count>first || first-count>2u || (toy_burst_read32(TOY_BURST_CHCR)&5u)) break;
            if(count==remaining && toy_burst_read32(TOY_BURST_DAR)==start+wire_bytes-count)
                ++stable;
            else stable=0;
            remaining=count;
        }
        control=toy_burst_read32(TOY_BURST_CHCR);
        channel_owned=stable==4u && remaining<=wire_bytes &&
            toy_burst_read32(TOY_BURST_DAR)==start+wire_bytes-remaining &&
            (control&7u)==(remaining?0u:2u);
        if(channel_owned) {
            toy_burst_write32(TOY_BURST_CHCR,0);
            for(unsigned n=0;n<4u;++n)
                toy_burst_write32(TOY_BURST_SAR+4u*n,stream->saved[n]);
        }
    }
    stream->state=RA_LOST;
    enum kui_sci_stream_result result=KUI_SCI_STREAM_RESET;
    if(sci_owned) result=toy_burst_handoff(r,toy_burst_read8(TOY_BURST_SSR));
    if(!channel_owned || !dma_ok || !(status&TOY_BURST_ORER) || result!=KUI_SCI_STREAM_OK) {
        r->shared.port.fault=true;
        return KUI_SCI_STREAM_RESET;
    }
    if(remaining) ++stream->stats.overruns;
    *received=wire_bytes-remaining;
    return KUI_SCI_STREAM_OK;
}
static enum kui_sci_stream_result toy_sci_burst_poll(kui_retail_async_region_type *r,
    const uint8_t *slab,unsigned frames,uint32_t *received) {
    return toy_burst_retire(r,slab,frames,received,false);
}
static enum kui_sci_stream_result toy_sci_burst_stop(kui_retail_async_region_type *r,
    const uint8_t *slab,unsigned frames) {
    if(r->engine.in_irq) return KUI_SCI_STREAM_BUSY;
    if(r->shared.stream.state==KUI_TOY_SCI_BURST_DMA) {
        uint32_t ignored;
        enum kui_sci_stream_result result=KUI_SCI_STREAM_PENDING;
        /* Quiesce only, never a movie poll. Prefer a whole-byte stop. A
         * nonresponsive receiver is fenced and quarantined without CMD12. */
        for(unsigned n=0;n<200000u && result==KUI_SCI_STREAM_PENDING;++n) {
            if(!toy_burst_bus_valid(r) || (toy_burst_read8(TOY_BURST_SSR)&TOY_BURST_FLAGS) ||
               (toy_burst_read32(TOY_BURST_DMAOR)&7u)!=1u ||
               !(toy_burst_read32(TOY_BURST_CHCR)&1u))
                result=toy_sci_burst_poll(r,slab,frames,&ignored);
        }
        if(result==KUI_SCI_STREAM_PENDING) {
            (void)toy_burst_retire(r,slab,frames,&ignored,true);
            r->shared.port.fault=true;
            return KUI_SCI_STREAM_RESET;
        }
        if(result!=KUI_SCI_STREAM_OK) return result;
    }
    return toy_burst_close(r);
}
