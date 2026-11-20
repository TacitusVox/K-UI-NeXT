/* SPDX-License-Identifier: GPL-3.0-only */
/* The Windows CE data probe, stage side (include/kui/wce_probe.h). Included
 * by retail_stage.c only in the probe build (KUI_WCE_PROBE), which installs
 * as a test ce-boot.kui.
 *
 * Before Windows CE starts: find 0:/KUI/WCEDATA/WCEDATA.BIN, claim its first
 * EMPTY slot (src/core/wce_probe_fat.c), write the stage report and the
 * CLAIMED header into it, and, after the reader's init, hand the slot's card
 * runs to the reader through its seed entry word. Every write goes into the
 * claimed slot: its header, read back as this file's EMPTY header for that
 * slot, and runs whose first sectors were read back blank. Any failure leaves
 * the launch unchanged, with no probe armed.
 * KUI_WCE_PROBE_ZERO (default on) also clears the RAM the launcher left from
 * the end of the loaded kernel up to this stage's prefix, and from this
 * stage's BSS up to its stack reservation, so in those ranges the end-of-game
 * dump holds only what Windows CE and the game wrote. */
#include "kui/wce_probe.h"
#include "../core/wce_probe_fat.c"
#include "../dreamcast/sci_sd_storage.c"
__asm__(".global __kui_wce_probe_stage\n.set __kui_wce_probe_stage,1\n");

#ifndef KUI_WCE_PROBE_ZERO
#define KUI_WCE_PROBE_ZERO 1
#endif
#define WCE_PROBE_ZERO_CEILING 0x8ce00000u /* this stage's prefix and image follow */
/* The stage's BSS end (retail_stage_ce.ld); its 64 KiB stack reservation
 * starts at KUI_RETAIL_STAGE_MEMORY_END. */
extern const uint8_t probe_stage_bss_end[] __asm__("__retail_stage_bss_end");

static struct kui_wce_probe_seed probe_seed;
static bool probe_armed;
static uint8_t probe_sector[512] __attribute__((aligned(4)));
static struct kui_wce_probe_layout probe_layout;
static struct kui_wce_probe_claim probe_claim;
static union {
    struct kui_wce_probe_stage report;
    uint8_t bytes[KUI_WCE_PROBE_STAGE_SECTORS*KUI_WCE_PROBE_SECTOR];
} probe_stage __attribute__((aligned(4)));
/* Recorded by ce_load. */
static uint32_t probe_body,probe_body_bytes,probe_prefix,probe_prefix_bytes;

static uint32_t probe_word(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
/* The AICA clock, in seconds, read only as K-UI's other masked readers do
 * (src/core/smash_phase_clock.c): with no G2 FIFO entry queued and no G2
 * DMA channel armed or running. Otherwise 0: unknown. */
static uint32_t probe_rtc(void) {
    if(*(volatile const uint32_t *)(uintptr_t)0xa05f688cu&0x31u) return 0u;
    for(unsigned i=0;i<4u;i++) {
        uint32_t base=0xa05f7800u+32u*i;
        if((*(volatile const uint32_t *)(uintptr_t)(base+20u)|
            *(volatile const uint32_t *)(uintptr_t)(base+24u))&1u) return 0u;
    }
    volatile const uint32_t *rtc=(volatile const uint32_t *)(uintptr_t)0xa0710000u;
    uint32_t high=rtc[0]&0xffffu,low=rtc[1]&0xffffu;
    return high==(rtc[0]&0xffffu)?high<<16|low:0u;
}
static int probe_read(void *context,uint32_t lba,uint8_t out[512]) {
    (void)context;
    if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;
    enum kui_loader_sd_result read=kui_retail_storage_read_run(&card,lba,1u,out);
    enum kui_loader_sd_result stop=kui_retail_storage_stop(&card);
    kui_retail_storage_release(&card);
    return read==KUI_LOADER_SD_OK && stop==KUI_LOADER_SD_OK?0:-1;
}
static int probe_write(uint32_t lba,uint32_t count,const uint8_t *data) {
    if(lba<manifest.partition_start || lba>=manifest.partition_end ||
       count>manifest.partition_end-lba) return -1;
    if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;
    /* The stage's block path is read-only: write through the byte path. */
    struct kui_loader_sd sd=card.device.sd;
    sd.bus.transfer_block=NULL;
    enum kui_loader_sd_result result=kui_sci_sd_write(&sd,lba,count,data);
    /* A failed write must not stop a launch the installed module would boot:
     * keep the card while it still answers CMD13 (status). A write-error
     * token leaves its cause for the first CMD13, which reads and clears it,
     * so a second CMD13 decides. */
    if(result!=KUI_LOADER_SD_OK) {
        sd.ready=true;
        if(kui_sci_sd_sync(&sd)!=KUI_LOADER_SD_OK) {sd.ready=true;(void)kui_sci_sd_sync(&sd);}
    }
    card.device.sd.ready=sd.ready;
    card.device.sd.last_command=sd.last_command;card.device.sd.last_response=sd.last_response;
    kui_retail_storage_release(&card);
    return result==KUI_LOADER_SD_OK?0:-1;
}

/* "WCE DATA ..." on the launch screen says what the probe found. */
static void probe_stage_prepare(const uint8_t *ip) {
    probe_armed=false;
    memset(&probe_seed,0,sizeof(probe_seed));
    if(manifest.storage_transport!=KUI_STORAGE_SCI) {retail_display_line("WCE DATA OFF NOT SCI");return;}
    const struct kui_wce_probe_volume volume={manifest.partition_start,manifest.partition_end,probe_read,NULL};
    enum kui_wce_probe_fat_result found=kui_wce_probe_fat_find(&volume,&probe_layout,probe_sector);
    if(found!=KUI_WCE_PROBE_FAT_OK) {
        retail_display_hex(found==KUI_WCE_PROBE_FAT_MISSING?"WCE DATA OFF NO DATA FILE":
            "WCE DATA OFF FILE LOOKUP",(uint32_t)found);
        return;
    }
    enum kui_wce_probe_claim_result claimed=kui_wce_probe_claim_find(&volume,&probe_layout,probe_sector,&probe_claim);
    if(claimed!=KUI_WCE_PROBE_CLAIM_OK) {
        retail_display_hex(claimed==KUI_WCE_PROBE_CLAIM_FULL?"WCE DATA OFF ALL SLOTS USED":
            "WCE DATA OFF SLOT CHECK",(uint32_t)claimed);
        return;
    }
    const struct kui_wce_probe_claim *c=&probe_claim;
    /* Stage report. */
    struct kui_wce_probe_stage *r=&probe_stage.report;
    memset(&probe_stage,0,sizeof(probe_stage));
    memcpy(r->magic,"KUIWCEST",8u);
    r->version=KUI_WCE_PROBE_VERSION;r->bytes=sizeof(*r);
    memcpy(r->build,KUI_BUILD_ID,sizeof(KUI_BUILD_ID)<sizeof(r->build)?sizeof(KUI_BUILD_ID):sizeof(r->build));
    r->rtc=probe_rtc();r->transport=manifest.storage_transport;
    r->card_high_capacity=card.device.sd.high_capacity;r->card_blocks=(uint32_t)card.device.sd.blocks;
    r->partition_start=manifest.partition_start;r->partition_end=manifest.partition_end;
    r->track_count=manifest.track_count;r->extent_count=manifest.extent_count;
    r->map_slots=KUI_RETAIL_ASYNC_SLOTS;r->kernel_bytes=exec_bytes;r->kernel_crc32=boot_crc;
    r->kernel_head_crc32=kui_retail_crc32(0,ce_prefix,sizeof(ce_prefix));
    const uint8_t *body=(const uint8_t *)(uintptr_t)probe_body;
    uint32_t header=probe_body_bytes>=0x48u?probe_word(body+0x44u):0u;
    if(probe_body_bytes>=0x48u && probe_word(body+0x40u)==0x43454345u && !(header&3u) &&
       header>=probe_body && header-probe_body<=probe_body_bytes-84u) {
        const uint8_t *h=(const uint8_t *)(uintptr_t)header;
        r->rom_header=header;r->rom_header_method=1u;
        r->ram_start=probe_word(h+20u);r->ram_free=probe_word(h+24u);r->ram_end=probe_word(h+28u);
        memcpy(r->rom_header_bytes,h,sizeof(r->rom_header_bytes));
    }
    r->file_runs=probe_layout.run_count;r->slot=c->slot;r->slot_runs=c->run_count;r->sequence=c->used+1u;
    r->flags=(manifest.reader!=KUI_RETAIL_READER_STANDARD?1u:0u)|(KUI_WCE_PROBE==2?2u:0u);
    memcpy(r->ip,ip,sizeof(r->ip));
#if KUI_WCE_PROBE_ZERO
    /* Nothing the launch still needs lives here: firmware, IP.BIN and the
     * resident are below the body (0x8c010000), the prefix copy and this stage
     * (BSS, then the stack reservation) above 0x8ce00000. */
    uint32_t zero_begin=(probe_body+probe_body_bytes+0xfffu)&~0xfffu;
    uint32_t zero_end=WCE_PROBE_ZERO_CEILING;
    if(probe_prefix_bytes && probe_prefix<zero_end && probe_prefix+probe_prefix_bytes>zero_begin)
        zero_end=probe_prefix&~0xfffu;
    if(probe_body>=KUI_RETAIL_EXEC_ADDRESS && zero_end>zero_begin) {r->zero_begin=zero_begin;r->zero_end=zero_end;}
    uint32_t high=((uint32_t)(uintptr_t)probe_stage_bss_end+0xfffu)&~0xfffu;
    if(high<KUI_RETAIL_STAGE_MEMORY_END) {r->zero_high_begin=high;r->zero_high_end=KUI_RETAIL_STAGE_MEMORY_END;}
#endif
    /* The CLAIMED header first: an interrupted claim leaves a used slot, which
     * later launches skip, never an EMPTY slot over written sectors. Then the
     * stage report. */
    struct kui_wce_probe_slot claimed_header;
    kui_wce_probe_slot_header(&claimed_header,c->slot,KUI_WCE_PROBE_CLAIMED,c->used+1u,r->rtc,c->file.file_id);
    memset(probe_sector,0,sizeof(probe_sector));memcpy(probe_sector,&claimed_header,sizeof(claimed_header));
    if(probe_write(c->header_lba,1u,probe_sector)) {retail_display_line("WCE DATA OFF SLOT WRITE");return;}
    const uint8_t *from=probe_stage.bytes;
    for(uint32_t i=0;i<c->stage_count;i++) {
        if(probe_write(c->stage_runs[i].lba,c->stage_runs[i].sectors,from)) {retail_display_line("WCE DATA OFF STAGE WRITE");return;}
        from+=c->stage_runs[i].sectors*KUI_WCE_PROBE_SECTOR;
    }
    kui_wce_probe_seed_build(c,r->rtc,r->flags,&probe_seed);
#if KUI_WCE_PROBE_ZERO
    /* Word stores: the freestanding memset stores bytes (up to 14 MiB here).
     * Every bound is 4 KiB aligned. */
    const uint32_t ranges[2][2]={{r->zero_begin,r->zero_end},{r->zero_high_begin,r->zero_high_end}};
    for(unsigned k=0;k<2u;k++)
        for(uint32_t *w=(uint32_t *)(uintptr_t)ranges[k][0];(uintptr_t)w<ranges[k][1];w+=4) {
            w[0]=0u;w[1]=0u;w[2]=0u;w[3]=0u;
        }
#endif
    probe_armed=manifest.reader!=KUI_RETAIL_READER_STANDARD;
    retail_display_hex(probe_armed?"WCE DATA ON SLOT":"WCE DATA STAGE ONLY SLOT",c->slot);
}

/* After the resident's init: its seed entry word must name a seed in its
 * BSS (past the image bytes the relay checks again, below its stack). */
static void probe_stage_install(size_t resident_bytes_copied) {
    if(!probe_armed || resident_blob!=__retail_resident_scia_blob_start) return;
    uint32_t at=*(volatile const uint32_t *)(uintptr_t)KUI_RETAIL_CE_PROBE_SEED_ENTRY;
    if((at&3u) || at<KUI_RETAIL_RESIDENT_ADDRESS+resident_bytes_copied ||
       at+sizeof(probe_seed)>KUI_RETAIL_CE_ASYNC_LIMIT) {
        probe_armed=false;retail_display_hex("WCE DATA OFF RESIDENT SEED",at);return;
    }
    memcpy((void *)(uintptr_t)at,&probe_seed,sizeof(probe_seed));
}
