/* SPDX-License-Identifier: GPL-3.0-only */
/* The Windows CE data probe, reader side (include/kui/wce_probe.h). Included
 * by retail_async.c only in the probe build (KUI_WCE_PROBE), which installs
 * as a test ce-boot.kui; the accepted generic module never compiles this.
 *
 * While the game runs: each GD call is counted by function and command, CD
 * audio commands are logged, and each call reads the Maple DMA command list
 * that MDSTAR names, to count which port and slot each command addresses.
 * Sampling only sees lists present during GD calls: a frame counted is
 * evidence, a frame not counted is not evidence of absence. These are reads
 * only: the Maple list through P2 (uncached), MDSTAR, and the AICA clock at a
 * few marked moments.
 *
 * The timing session (KUI_WCE_PROBE=2) leaves Maple sampling, the AICA
 * clock and the CD audio log out and keeps a ring of the latest read
 * requests instead (include/kui/wce_probe.h): when each began and ended, by
 * CE's millisecond count and TMU0's count, and the TMU0 ticks the reader
 * spent in GD calls and in its interrupt meanwhile. Reads only: CE's count,
 * TMU0's count, reload, prescaler and underflow flag, MMUCR, VBR, the
 * caller's stack pointer and the reader's own counters.
 *
 * At the end (the game's BIOS menu call on A+B+X+Y+Start, or a reader fault
 * that stops it): with interrupts masked and the game no longer running, the
 * card stream is stopped and the terminal report and every non-zero 4 KiB
 * page of RAM are written into the slot the stage claimed, through the card
 * runs the stage checked, then the slot header the stage prepared. The
 * console then restarts into K-UI. */
#include "kui/wce_probe.h"
#include "retail_display.h"
#ifndef KUI_WCE_PROBE_HOST_TEST
#include "../dreamcast/sci_sd_storage.c"
#endif
/* Hardware access. tests/test_wce_probe_end_to_end.c maps these onto a host
 * model of the console's RAM and registers. */
#ifndef WCE_PROBE_REG
#define WCE_PROBE_REG(address) (*(volatile const uint32_t *)(uintptr_t)(address))
#endif
#ifndef WCE_PROBE_CACHE_PURGE
#define WCE_PROBE_CACHE_PURGE(ccr) do { \
    for(uint32_t entry=0;entry<512u;entry++) \
        if(!((ccr)&0x20u) || !(entry&0x80u)) \
            *(volatile uint32_t *)(uintptr_t)(UINT32_C(0xf4000000)|entry<<5)=0u; \
} while(0)
#endif
#ifndef WCE_PROBE_RAM
#define WCE_PROBE_RAM(address) ((const void *)(uintptr_t)(address))
#endif
#ifndef WCE_PROBE_REBOOT
#define WCE_PROBE_REBOOT() ((void (*)(void))(uintptr_t)0xa0000000u)()
#endif
#if KUI_WCE_PROBE == 2
#ifndef WCE_PROBE_REG16
#define WCE_PROBE_REG16(address) (*(volatile const uint16_t *)(uintptr_t)(address))
#endif
#ifndef WCE_PROBE_TIMING
#define WCE_PROBE_TIMING ((struct kui_wce_probe_timing *)(uintptr_t)KUI_WCE_PROBE_TIMING_ADDRESS)
#endif
#ifndef WCE_PROBE_VBR
#define WCE_PROBE_VBR() probe_vbr()
static inline uint32_t probe_vbr(void) {uint32_t v;__asm__ __volatile__("stc vbr,%0":"=r"(v));return v;}
#endif
#endif

extern volatile uint32_t kui_retail_hook_fault;

/* The stage writes this through the entry word after the resident's init
 * (which follows _start's BSS clear). It is BSS, outside the image bytes the
 * stage checks again after bootstrap 2. */
struct kui_wce_probe_seed kui_wce_probe_seed;
#if KUI_WCE_PROBE == 1
static struct kui_wce_probe_counters probe;
#endif

static void probe_sat16(uint16_t *c) {if(*c!=UINT16_MAX)++*c;}
static void probe_sat32(uint32_t *c) {if(*c!=UINT32_MAX)++*c;}

/* The AICA clock, in seconds, read only as K-UI's other masked readers do
 * (src/core/smash_phase_clock.c): with no G2 FIFO entry queued and no G2
 * DMA channel armed or running. Otherwise 0: unknown. */
#if KUI_WCE_PROBE == 1
static uint32_t __attribute__((noinline)) probe_rtc(void) {
    if(WCE_PROBE_REG(0xa05f688cu)&0x31u) return 0u;
    for(unsigned i=0;i<4u;i++) {
        uint32_t base=0xa05f7800u+32u*i;
        if((WCE_PROBE_REG(base+20u)|WCE_PROBE_REG(base+24u))&1u) return 0u;
    }
    uint32_t high=WCE_PROBE_REG(0xa0710000u)&0xffffu,low=WCE_PROBE_REG(0xa0710004u)&0xffffu;
    return high==(WCE_PROBE_REG(0xa0710000u)&0xffffu)?high<<16|low:0u;
}
#else
/* The timing session times by CE's millisecond count (its ring) instead. */
#define probe_rtc() 0u
#endif

#if KUI_WCE_PROBE == 1
static void probe_maple(void) {
    struct kui_wce_probe_counters *p=&probe;
    probe_sat32(&p->maple_samples);
    uint32_t start=WCE_PROBE_REG(0xa05f6c04u); /* MDSTAR */
    uint32_t at=start&UINT32_C(0x1fffffe0);
    if(at<UINT32_C(0x0c000000) || at>=UINT32_C(0x0d000000)) {probe_sat32(&p->maple_bad);return;}
    if(start!=p->maple_last_list) {p->maple_last_list=start;probe_sat32(&p->maple_lists);}
    for(unsigned n=0;n<32u;n++) {
        if(at>UINT32_C(0x0d000000)-16u) {probe_sat32(&p->maple_bad);return;}
        /* P2: uncached, so the probe leaves no cache lines over game memory
         * and sees what the Maple DMA reads. */
        const volatile uint32_t *w=(const volatile uint32_t *)WCE_PROBE_RAM(at|UINT32_C(0xa0000000));
        uint32_t instruction=w[0],header=w[2];
        uint32_t length=instruction&0xffu,port=instruction>>16&3u;
        /* A normal transfer whose frame names the same port and length. */
        if((instruction>>8&7u) || header>>24!=length || (header>>14&3u)!=port) {
            probe_sat32(&p->maple_bad);return;
        }
        uint32_t unit_bits=header>>8&0x3fu,command=header&0xffu;
        unsigned unit=(unit_bits&0x20u)?0u:unit_bits==1u?1u:unit_bits==2u?2u:3u;
        unsigned kind=command==1u?1u:command>=9u && command<=14u?command-7u:0u;
        if(unit<3u) {
            unsigned u=port*3u+unit;
            if(kind>=2u && length) p->maple_functions[u]|=w[3];
            /* Block read, write and completion count for storage only: an LCD
             * or clock block write is not a save. */
            if(kind>=4u && kind<=6u && (!length || w[3]!=UINT32_C(0x02000000))) kind=0u;
            if(p->maple[u][kind]!=UINT8_MAX) ++p->maple[u][kind];
        }
        probe_sat32(&p->maple_frames);
        if(instruction&UINT32_C(0x80000000)) return;
        at+=12u+4u*length;
    }
}
#endif

#if KUI_WCE_PROBE == 2
extern volatile uint32_t kui_retail_hook_caller[2]; /* the GD caller's PR and stack pointer */
static uint32_t probe_timing_on; /* the ring is the reader's from here on */
#define PROBE_TCOR0 0xffd80008u
#define PROBE_TCNT0 0xffd8000cu
#define PROBE_TCR0 0xffd80010u
#define PROBE_TCR0_UNF 0x100u
#define PROBE_IN_RING_AREA(address) \
    (((address)&0x1fffffffu)-(KUI_WCE_PROBE_TIMING_ADDRESS&0x1fffffffu)<KUI_WCE_PROBE_TIMING_BYTES)
/* CE's millisecond count: the word after the reschedule flag in KData. */
static uint32_t probe_ms(void) {
    uint32_t at=kui_retail_ce_kernel[2];
    return at?*(volatile const uint32_t *)WCE_PROBE_RAM(at+4u):0u;
}
/* With the flag set, a reload between the two reads leaves the count after
 * it, which agrees with the flag. */
uint32_t kui_wce_probe_tmu(uint32_t count) {
    if(!(WCE_PROBE_REG16(PROBE_TCR0)&PROBE_TCR0_UNF)) return count;
    uint32_t again=WCE_PROBE_REG(PROBE_TCNT0);
    return KUI_WCE_PROBE_TMU_PENDING|(again>count?again:count);
}
/* CE's count and TMU0's, with no tick of CE's between them. */
static uint32_t probe_now(uint32_t *tmu) {
    uint32_t ms;
    do {ms=probe_ms();*tmu=kui_wce_probe_tmu(WCE_PROBE_REG(PROBE_TCNT0));} while(ms!=probe_ms());
    return ms;
}
/* The ring, once CE runs, for a session the stage prepared (nothing else is
 * saved): with CE's MMU on and neither its VBR nor the calling stack in
 * this area. Bootstrap 2 and its VBR use the area until then, and some
 * kernels make their first GD calls on a boot stack in it. */
static struct kui_wce_probe_timing *probe_timing(void) {
    struct kui_wce_probe_timing *t=WCE_PROBE_TIMING;
    if(probe_timing_on) return t;
    if(kui_wce_probe_seed.magic!=KUI_WCE_PROBE_SEED_MAGIC || !(WCE_PROBE_REG(0xff000010u)&1u) ||
       PROBE_IN_RING_AREA(WCE_PROBE_VBR()) || PROBE_IN_RING_AREA(kui_retail_hook_caller[1])) return NULL;
    memset(t,0,sizeof(*t));
    t->magic=KUI_WCE_PROBE_TIMING_MAGIC;t->capacity=KUI_WCE_PROBE_TIMING_REQUESTS;
    t->tcr0=WCE_PROBE_REG16(PROBE_TCR0);t->tcor0=WCE_PROBE_REG(PROBE_TCOR0);t->first_ms=probe_ms();
    probe_timing_on=1u;
    return t;
}
/* TMU0 ticks since `started` (both counts in KUI_WCE_PROBE_TMU_PENDING
 * form). TMU0 counts down and reloads from TCOR0: a later count, or an
 * underflow pending now but not then, is one reload (CE's tick may have
 * counted it meanwhile). A call or interrupt is far shorter than two
 * periods. */
static uint32_t probe_ticks(const struct kui_wce_probe_timing *t,uint32_t started) {
    uint32_t ended=kui_wce_probe_tmu(WCE_PROBE_REG(PROBE_TCNT0));
    uint32_t from=started&~KUI_WCE_PROBE_TMU_PENDING,to=ended&~KUI_WCE_PROBE_TMU_PENDING;
    return to>from || (ended&~started&KUI_WCE_PROBE_TMU_PENDING)?from+t->tcor0+1u-to:from-to;
}
static void probe_add(uint32_t *total,uint32_t *wraps,uint32_t ticks) {
    *total+=ticks;
    if(*total<ticks) ++*wraps;
}
static void probe_time_end(struct kui_wce_probe_timing *t,uint32_t flags) {
    struct kui_wce_probe_request_time *q=&t->ring[t->current];
    uint32_t ms=probe_now(&q->ended_tmu)-q->issued_ms;
    q->latency_ms=(uint16_t)(ms>UINT16_MAX?UINT16_MAX:ms);
    q->call_ticks=t->call_ticks-t->snap[0];q->irq_ticks=t->irq_ticks-t->snap[1];
    q->overruns=(uint16_t)(R.shared.stream.stats.overruns-t->snap[2]);
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
    q->slow_blocks=(uint16_t)(R.shared.stream.stats.slow_blocks-t->snap[3]);
#endif
    q->flags|=(uint8_t)flags;t->open=0u;
}
/* The open request has ended once the service no longer runs it; it failed
 * unless it completed (an abort included). */
static void probe_time_check(struct kui_wce_probe_timing *t) {
    if(!t->open || (R.shared.service.pending && R.shared.service.token==t->token)) return;
    probe_time_end(t,KUI_WCE_PROBE_TIME_ENDED|(R.shared.service.token!=t->token?KUI_WCE_PROBE_TIME_REPLACED:
        R.shared.service.status!=KUI_GD_COMPLETED?KUI_WCE_PROBE_TIME_FAILED:0u));
}
static void probe_time_begin(uint32_t command) {
    struct kui_wce_probe_timing *t=probe_timing();
    if(!t) return;
    if(t->open) probe_time_end(t,KUI_WCE_PROBE_TIME_REPLACED);
    uint32_t i=t->count?t->current+1u:0u;
    if(i>=KUI_WCE_PROBE_TIMING_REQUESTS) i=0u;
    struct kui_wce_probe_request_time *q=&t->ring[i];
    memset(q,0,sizeof(*q));
    uint32_t sectors=R.shared.service.count;
    q->issued_ms=probe_now(&q->issued_tmu);q->lba=R.shared.service.lba;
    q->sectors=(uint16_t)(sectors>UINT16_MAX?UINT16_MAX:sectors);q->command=(uint8_t)command;
    t->snap[0]=t->call_ticks;t->snap[1]=t->irq_ticks;t->snap[2]=R.shared.stream.stats.overruns;
#if KUI_CE_SCI_SLOW_AFTER_OVERRUN
    t->snap[3]=R.shared.stream.stats.slow_blocks;
#endif
    t->token=R.shared.service.token;t->current=i;t->open=1u;probe_sat32(&t->count);
}
void kui_wce_probe_time_call(uint32_t started) {
    struct kui_wce_probe_timing *t=probe_timing();
    if(!t) return;
    uint32_t ticks=probe_ticks(t,started);
    probe_add(&t->call_ticks,&t->call_wraps,ticks);
    if(t->open) {
        struct kui_wce_probe_request_time *q=&t->ring[t->current];
        probe_sat16(&q->calls);
        if(ticks>q->longest_call) q->longest_call=ticks;
    }
    probe_time_check(t);
}
/* In the interrupt: only after a GD call has taken the ring. */
void kui_wce_probe_time_irq(uint32_t started) {
    if(!probe_timing_on) return;
    struct kui_wce_probe_timing *t=WCE_PROBE_TIMING;
    probe_add(&t->irq_ticks,&t->irq_wraps,probe_ticks(t,started));
    probe_time_check(t);
}
#endif

void kui_wce_probe_record(uint32_t function,uint32_t command,int32_t result) {
#if KUI_WCE_PROBE == 1
    struct kui_wce_probe_counters *p=&probe;
#else
    /* The timing session's counters live in its ring area. */
    struct kui_wce_probe_timing *timing=probe_timing();
    if(!timing) return;
    struct kui_wce_probe_counters *p=&timing->counters;
#endif
    if(!p->calls) {
        p->magic=KUI_WCE_PROBE_COUNTERS_MAGIC;p->version=KUI_WCE_PROBE_VERSION;p->bytes=sizeof(*p);
        p->rtc_first=probe_rtc();
    }
    probe_sat32(&p->calls);
    probe_sat16(&p->functions[function&(KUI_WCE_PROBE_FUNCTIONS-1u)]);
    if(function==KUI_GD_REQUEST) {
        if(command-KUI_WCE_PROBE_COMMAND_FIRST>=KUI_WCE_PROBE_COMMANDS) probe_sat32(&p->unknown_commands);
        if(result>0) {
            if(command-KUI_WCE_PROBE_COMMAND_FIRST<KUI_WCE_PROBE_COMMANDS)
                probe_sat16(&p->commands[command-KUI_WCE_PROBE_COMMAND_FIRST]);
        } else {
            probe_sat32(&p->refused);p->refused_last=command;
            if(command<64u) p->refused_mask[command/32u]|=UINT32_C(1)<<(command%32u);
        }
        if(result>0 && (command==KUI_GD_PIOREAD || command==KUI_GD_DMAREAD ||
           command==KUI_RETAIL_GD_DMAREAD_STREAM || command==KUI_RETAIL_GD_PIOREAD_STREAM)) {
            uint32_t sectors=R.shared.service.count,total=p->read_sectors+sectors;
            probe_sat32(&p->read_requests);
            p->read_sectors=total<p->read_sectors?UINT32_MAX:total;
            if(sectors>p->largest_read) p->largest_read=sectors;
            if(!p->rtc_first_read) p->rtc_first_read=probe_rtc();
#if KUI_WCE_PROBE == 2
            probe_time_begin(command);
#endif
        }
#if KUI_WCE_PROBE == 1
        /* The CD audio log (the timing session counts the commands only). */
        if(command==KUI_RETAIL_GD_PLAY || command==KUI_RETAIL_GD_PLAY2 || command==KUI_RETAIL_GD_PAUSE ||
           command==KUI_RETAIL_GD_RELEASE || command==KUI_GD_STOP) {
            uint32_t n=p->audio_count;
            struct kui_wce_probe_audio *a=n<KUI_WCE_PROBE_AUDIO_FIRST?&p->audio_first[n]:
                &p->audio_ring[(n-KUI_WCE_PROBE_AUDIO_FIRST)%KUI_WCE_PROBE_AUDIO_RING];
            a->command=(uint8_t)command;a->result=result>0?1u:result==0?0u:2u;
            a->call=(uint16_t)p->calls;
            a->first=R.shared.service.outputs[0];a->last=R.shared.service.outputs[1];
            a->repeat=R.shared.service.outputs[2];
            if(!n) p->rtc_first_audio=probe_rtc();
            probe_sat32(&p->audio_count);
        }
#endif
    }
#if KUI_WCE_PROBE == 1
    probe_maple();
#endif
}

/* Sector `sector` counts from the slot's terminal report, where runs[0]
 * begins; the stage checked every run lies on the card. */
static enum kui_loader_sd_result probe_write(struct kui_loader_sd *sd,uint32_t sector,
    uint32_t count,const uint8_t *data) {
    const struct kui_wce_probe_seed *s=&kui_wce_probe_seed;
    for(uint32_t i=0;i<s->run_count && count;i++) {
        const struct kui_wce_probe_run *r=&s->runs[i];
        if(sector>=r->sectors) {sector-=r->sectors;continue;}
        uint32_t take=r->sectors-sector;
        if(take>count) take=count;
        enum kui_loader_sd_result result=kui_sci_sd_write(sd,r->lba+sector,take,data);
        /* One retry of the same sectors: a single transient error (a data CRC
         * reject) does not end the dump. */
        if(result!=KUI_LOADER_SD_OK) {sd->ready=true;result=kui_sci_sd_write(sd,r->lba+sector,take,data);}
        if(result!=KUI_LOADER_SD_OK) return result;
        data+=take*KUI_WCE_PROBE_SECTOR;count-=take;sector=0u;
    }
    return count?KUI_LOADER_SD_RANGE:KUI_LOADER_SD_OK;
}

/* Before any write: the seed the stage installed is intact (CRC-32 over
 * every field before it), describes exactly one slot's terminal area, and
 * every sector it names lies in the image's partition. Anything else (a
 * seed changed by bootstrap 2, the game or memory damage) writes nothing. */
static bool probe_seed_ok(const struct kui_wce_probe_seed *s) {
    if(s->magic!=KUI_WCE_PROBE_SEED_MAGIC || s->version!=KUI_WCE_PROBE_VERSION ||
       !s->run_count || s->run_count>KUI_WCE_PROBE_SLOT_RUNS || !e.manifest) return false;
    uint32_t crc=UINT32_MAX;
    const uint8_t *p=(const uint8_t *)s;
    for(unsigned i=0;i<offsetof(struct kui_wce_probe_seed,crc32);i++) {
        crc^=p[i];
        for(unsigned bit=0;bit<8u;bit++) crc=(crc>>1)^(UINT32_C(0xedb88320)&(0u-(crc&1u)));
    }
    uint32_t start=(uint32_t)e.manifest->partition_start,span=(uint32_t)e.manifest->partition_end-start,total=0u;
    if(~crc!=s->crc32 || s->slot_header_lba-start>=span) return false;
    for(uint32_t i=0;i<s->run_count;i++) {
        const struct kui_wce_probe_run *r=&s->runs[i];
        if(r->lba-start>=span || r->sectors>span-(r->lba-start)) return false;
        total+=r->sectors;
    }
    return total==KUI_WCE_PROBE_SLOT_SECTORS-KUI_WCE_PROBE_TERMINAL_SECTOR;
}

#define PROBE_BLUE 0x001fu
#define PROBE_GREEN 0x07e0u
#define PROBE_RED 0xf800u
#define PROBE_YELLOW 0xffe0u
void kui_wce_probe_terminal(const struct retail_display_state *display,uint32_t reason,
    uint32_t command,uint32_t caller,uint32_t stack) {
    /* Registers first, before the display or the card changes any of them.
     * All 32-bit, read without side effects (include/kui/wce_probe.h). */
    static const uint32_t registers[KUI_WCE_PROBE_REGISTERS_READ-2u]={
        0xff00001cu,0xff000010u,0xff000000u,0xff000008u,0xa05f6c04u,0xa05f6900u,
        0xa05f80d0u,0xa05f80d8u,0xa05f80e8u,0xa05f8044u,0xa05f8050u,
        0xa05f6910u,0xa05f6920u,0xa05f6930u};
    uint32_t values[KUI_WCE_PROBE_REGISTERS_READ];
#ifdef KUI_WCE_PROBE_HOST_TEST
    values[0]=values[1]=0u;
#else
    __asm__ __volatile__("stc sr,%0":"=r"(values[0]));
    __asm__ __volatile__("stc vbr,%0":"=r"(values[1]));
#endif
    for(unsigned i=0;i<KUI_WCE_PROBE_REGISTERS_READ-2u;i++) values[i+2u]=WCE_PROBE_REG(registers[i]);
#if KUI_WCE_PROBE == 2
    /* The ring's end, before the cache write-back below saves it. */
    if(probe_timing_on) WCE_PROBE_TIMING->end_ms=probe_ms();
#endif
    const struct kui_wce_probe_seed *s=&kui_wce_probe_seed;
    uint16_t color=PROBE_YELLOW; /* no data file or free slot at launch, or no read yet */
    if(!probe_seed_ok(s) || !e.opened) goto done;
    color=PROBE_RED;
    retail_display_color(display,PROBE_BLUE);
    /* Stop the card stream; the port stays ours until the restart. */
    if(kui_sci_stream_stop()!=KUI_SCI_STREAM_OK) goto done;
    kui_sci_stream_discard();
    struct kui_loader_sd sd=R.shared.card.device.sd;
    sd.bus.transfer_block=NULL; /* the resident's block path is read-only */
    if(!sd.ready) goto done;
    /* The stopped stream's buffers are scratch now: the report in spill,
     * the page map in area0, the slot header in area1. */
    uint8_t *report=R.spill,*map=R.area0,*slot=R.area1;
    memset(report,0,KUI_RETAIL_ASYNC_SPILL_BYTES);memset(map,0,KUI_WCE_PROBE_SECTOR);
    struct kui_wce_probe_terminal *t=(struct kui_wce_probe_terminal *)(void *)report;
    memcpy(t->magic,"KUIWCETR",8u);
    t->version=KUI_WCE_PROBE_VERSION;t->bytes=sizeof(*t);t->reason=reason;
    t->menu_command=command;t->caller=caller;t->stack=stack;t->rtc=probe_rtc();
    t->hook_fault=kui_retail_hook_fault;
    memcpy(t->registers,values,sizeof(values));
    t->seed=*s;
#if KUI_WCE_PROBE == 1
    t->counters=probe;
#else
    if(probe_timing_on) t->counters=WCE_PROBE_TIMING->counters;
#endif
    t->counters.rtc_last=t->rtc;
    /* CE runs the operand cache copy-back and maps the game's pages at other
     * cache indexes than their P1 alias: write back every dirty line before
     * reading RAM through P1. A non-associative write of 0 to each entry of
     * the OC address array writes a dirty line back, then invalidates it.
     * With CCR.ORA set, entries 128-255 and 384-511 are on-chip RAM. */
    WCE_PROBE_CACHE_PURGE(values[2]);
    uint32_t pages=0u;
    for(uint32_t page=0;page<KUI_WCE_PROBE_PAGES;page++) {
        const uint32_t *w=(const uint32_t *)WCE_PROBE_RAM(KUI_WCE_PROBE_RAM_BEGIN+page*KUI_WCE_PROBE_PAGE_BYTES);
        for(unsigned i=0;i<KUI_WCE_PROBE_PAGE_BYTES/4u;i++)
            if(w[i]) {map[page/8u]|=(uint8_t)(1u<<(page%8u));++pages;break;}
    }
    t->pages=pages;t->dump_sectors=pages*(KUI_WCE_PROBE_PAGE_BYTES/KUI_WCE_PROBE_SECTOR);
    _Static_assert(KUI_RETAIL_ASYNC_SPILL_BYTES==4u*KUI_WCE_PROBE_SECTOR &&
        sizeof(struct kui_wce_probe_terminal)<=KUI_RETAIL_ASYNC_SPILL_BYTES &&
        KUI_WCE_PROBE_PAGE_MAP_OFFSET==7u*KUI_WCE_PROBE_SECTOR &&
        KUI_WCE_PROBE_TERMINAL_SECTORS==8u,"WCE probe terminal layout");
    /* The report (4 sectors, 3 zero sectors, the page map), the pages, then
     * the COMPLETE slot header: a cut-off dump still decodes. */
    if(probe_write(&sd,0u,4u,report)!=KUI_LOADER_SD_OK) goto done;
    memset(report,0,KUI_RETAIL_ASYNC_SPILL_BYTES);
    if(probe_write(&sd,4u,3u,report)!=KUI_LOADER_SD_OK ||
       probe_write(&sd,7u,1u,map)!=KUI_LOADER_SD_OK) goto done;
    uint32_t sector=KUI_WCE_PROBE_DUMP_SECTOR-KUI_WCE_PROBE_TERMINAL_SECTOR,written=0u;
    for(uint32_t page=0;page<KUI_WCE_PROBE_PAGES;) {
        if(!(map[page/8u]>>(page%8u)&1u)) {++page;continue;}
        uint32_t end=page;
        while(end<KUI_WCE_PROBE_PAGES && end-page<128u && (map[end/8u]>>(end%8u)&1u)) ++end;
        uint32_t count=(end-page)*(KUI_WCE_PROBE_PAGE_BYTES/KUI_WCE_PROBE_SECTOR);
        if(probe_write(&sd,sector,count,(const uint8_t *)WCE_PROBE_RAM(
               KUI_WCE_PROBE_RAM_BEGIN+page*KUI_WCE_PROBE_PAGE_BYTES))!=KUI_LOADER_SD_OK) goto done;
        sector+=count;written+=end-page;page=end;
    }
    memset(slot,0,KUI_WCE_PROBE_SECTOR);
    memcpy(slot,&s->complete,sizeof(s->complete));
    if(kui_sci_sd_write(&sd,s->slot_header_lba,1u,slot)==KUI_LOADER_SD_OK) color=PROBE_GREEN;
done:
    retail_display_color(display,color);
    retail_display_pause(color==PROBE_GREEN?180u:600u);
    WCE_PROBE_REBOOT();
    for(;;) __asm__ volatile("");
}
