/* SPDX-License-Identifier: GPL-3.0-only */
/* The Windows CE data probe, reader side (include/kui/wce_probe.h). Included
 * by retail_async.c only in the probe build (KUI_WCE_PROBE), which installs
 * as a test ce-boot.kui; the accepted generic module never compiles this.
 *
 * While the game runs: each GD call is counted by function and command, CD
 * audio commands are logged, and every fourth call reads the Maple DMA
 * command list that MDSTAR names, to count which port and slot each command
 * addresses. These are reads only: bounded P1 views of main RAM, MDSTAR,
 * and the AICA clock at a few marked moments.
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
#ifndef WCE_PROBE_RAM
#define WCE_PROBE_RAM(address) ((const void *)(uintptr_t)(address))
#endif
#ifndef WCE_PROBE_REBOOT
#define WCE_PROBE_REBOOT() ((void (*)(void))(uintptr_t)0xa0000000u)()
#endif

extern volatile uint32_t kui_retail_hook_fault;

/* The stage writes this through the entry word after the resident's init
 * (which follows _start's BSS clear). It is BSS, outside the image bytes the
 * stage checks again after bootstrap 2. */
struct kui_wce_probe_seed kui_wce_probe_seed;
static struct kui_wce_probe_counters probe;

static void probe_sat16(uint16_t *c) {if(*c!=UINT16_MAX)++*c;}
static void probe_sat32(uint32_t *c) {if(*c!=UINT32_MAX)++*c;}

/* The AICA clock, in seconds, read only as K-UI's other masked readers do
 * (src/core/smash_phase_clock.c): with no G2 FIFO entry queued and no G2
 * DMA channel armed or running. Otherwise 0: unknown. */
static uint32_t __attribute__((noinline)) probe_rtc(void) {
    if(WCE_PROBE_REG(0xa05f688cu)&0x31u) return 0u;
    for(unsigned i=0;i<4u;i++) {
        uint32_t base=0xa05f7800u+32u*i;
        if((WCE_PROBE_REG(base+20u)|WCE_PROBE_REG(base+24u))&1u) return 0u;
    }
    uint32_t high=WCE_PROBE_REG(0xa0710000u)&0xffffu,low=WCE_PROBE_REG(0xa0710004u)&0xffffu;
    return high==(WCE_PROBE_REG(0xa0710000u)&0xffffu)?high<<16|low:0u;
}

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
            if(p->maple[u][kind]!=UINT8_MAX) ++p->maple[u][kind];
            if(kind>=2u && length) p->maple_functions[u]|=w[3];
        }
        probe_sat32(&p->maple_frames);
        if(instruction&UINT32_C(0x80000000)) return;
        at+=12u+4u*length;
    }
}

void kui_wce_probe_record(uint32_t function,uint32_t command,int32_t result) {
    struct kui_wce_probe_counters *p=&probe;
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
        }
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
    }
    if(!(p->calls&3u)) probe_maple();
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
    /* Registers first, before the display or the card changes any of them. */
    static const uint32_t registers[KUI_WCE_PROBE_REGISTERS-2u]={
        0xff00001cu,0xff000010u,0xff000000u,0xff000008u,0xff00000cu,0xff000024u,0xff000028u,
        0xa05f6c04u,0xa05f6c14u,0xa05f6c18u,0xa05f6900u,0xa05f6904u,0xa05f6908u,
        0xa05f80d0u,0xa05f80d8u,0xa05f80d4u,0xa05f80dcu,0xa05f80e0u,0xa05f80e8u,0xa05f80ecu,
        0xa05f80f0u,0xa05f8044u,0xa05f805cu,0xa05f8050u,0xff80002cu,0xa05f6910u,0xa05f6920u,
        0xa05f6930u,0xffd8000cu,0xffd80008u}; /* all 32-bit registers, read only */
    uint32_t values[KUI_WCE_PROBE_REGISTERS];
#ifdef KUI_WCE_PROBE_HOST_TEST
    values[0]=values[1]=0u;
#else
    __asm__ __volatile__("stc sr,%0":"=r"(values[0]));
    __asm__ __volatile__("stc vbr,%0":"=r"(values[1]));
#endif
    for(unsigned i=0;i<KUI_WCE_PROBE_REGISTERS-2u;i++) values[i+2u]=WCE_PROBE_REG(registers[i]);
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
    t->seed=*s;t->counters=probe;t->counters.rtc_last=t->rtc;
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
