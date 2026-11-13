/* SPDX-License-Identifier: GPL-3.0-only */
#include "disc_toy_trace.h"
#include "kui/gd_service.h"
#include "kui/toy_fmv_compare.h"
#include "kui/toy_pilot_clock.h"
#include "kui/vmu_backing.h"
#include "sci_sd_bus.h"
#include "stage_fast_crc32.h"
#include "../dreamcast/sci_sd_storage.h"
#include <string.h>

struct kui_disc_toy_trace_config kui_disc_toy_config;
uint32_t kui_disc_toy_gd_active;
static struct kui_toy_fmv_trace trace;
static struct {
    uint32_t pr, function, argument, output, fad, sectors;
} gd;
static _Alignas(32) char report_text[KUI_TOY_REPORT_FILE_BYTES];
static _Alignas(32) uint8_t verify[KUI_TOY_REPORT_SECTOR_BYTES];

void kui_disc_toy_gd_proxy(void);
void kui_disc_toy_gd_return(void);

static uint32_t word(uint32_t address) {
    return *(volatile const uint32_t *)(uintptr_t)address;
}

static const volatile uint32_t *guest_words(uint32_t address, uint32_t bytes) {
    uint32_t area=address&0xff000000u;
    uint32_t physical=address&0x00ffffffu;
    if((area!=0x0c000000u && area!=0x8c000000u && area!=0xac000000u) ||
       (physical&3u) || physical<0x8000u || physical>0xfd0000u-bytes) return NULL;
    if(area==0x0c000000u) address|=0x80000000u;
    return (const volatile uint32_t *)(uintptr_t)address;
}

static bool clock_read(uint32_t *tick) {
    if(!kui_toy_pilot_clock_profile(*(volatile const uint16_t *)0xffc00000u,
       word(0xffd80008u),*(volatile const uint16_t *)0xffd80010u,
       *(volatile const uint8_t *)0xffd80004u)) return false;
    *tick=word(0xffd8000cu);
    return true;
}

static bool movie_request(uint32_t fad, uint32_t count) {
    if(fad<KUI_GD_FAD_OFFSET || !count) return false;
    uint32_t lba=fad-KUI_GD_FAD_OFFSET;
    for(unsigned i=0;i<4u;i++) {
        const struct kui_toy_movie_range *movie=&kui_disc_toy_config.movie[i];
        if(lba>=movie->lba && lba-movie->lba>=386u && lba-movie->lba<movie->count) return true;
    }
    return false;
}

static bool tracked_token(uint32_t token) {
    if(!token) return false;
    for(unsigned i=0;i<KUI_TOY_FMV_PENDING;i++)
        if(trace.pending[i].token==token) return true;
    return false;
}

void kui_disc_toy_gd_before(uint32_t frame[21]) {
    if(frame[14]!=0u) return;
    uint32_t function=frame[13], argument=frame[16], output=frame[15];
    gd.fad=0u;
    if(function==KUI_GD_REQUEST) {
        if(argument!=KUI_GD_PIOREAD && argument!=KUI_GD_DMAREAD) return;
        const volatile uint32_t *parameters=guest_words(output,16u);
        if(!parameters) return;
        uint32_t fad=parameters[0], sectors=parameters[1];
        if(!movie_request(fad,sectors)) return;
        gd.fad=fad;
        gd.sectors=sectors;
    } else if(function!=KUI_GD_CHECK || !tracked_token(argument)) return;
    gd.pr=frame[5];
    gd.function=function;
    gd.argument=argument;
    gd.output=output;
    kui_disc_toy_gd_active=1u;
    frame[5]=(uint32_t)(uintptr_t)kui_disc_toy_gd_return;
}

void kui_disc_toy_gd_after(uint32_t frame[21]) {
    frame[5]=gd.pr;
    kui_disc_toy_gd_active=0u;
    int32_t result=(int32_t)frame[20];
    uint32_t tick;
    if(gd.function==KUI_GD_REQUEST && result>0 && gd.fad) {
        if(!clock_read(&tick)) {++trace.clock_lost;return;}
        kui_toy_fmv_request(&trace,kui_disc_toy_config.movie,(uint32_t)result,
            gd.fad-KUI_GD_FAD_OFFSET,gd.sectors,tick);
    } else if(gd.function==KUI_GD_CHECK && gd.argument &&
              (result==KUI_GD_COMPLETED || result==KUI_GD_FAILED)) {
        const volatile uint32_t *status=guest_words(gd.output,16u);
        if(!status) return;
        if(!clock_read(&tick)) {++trace.clock_lost;return;}
        kui_toy_fmv_status(&trace,gd.argument,tick,result,status[2]);
    }
}

static bool write_verified(struct kui_loader_sd *card) {
    const struct kui_toy_report_wire *seed=&kui_disc_toy_config.report;
    struct kui_vmu_sd_identity identity;
    if(!card->ready || card->slow || card->high_capacity!=!!seed->high_capacity ||
       seed->lba[0]>=card->blocks || seed->lba[1]>=card->blocks ||
       kui_vmu_sd_identity_read(card,&identity)!=KUI_VMU_BACKING_OK ||
       memcmp(identity.cid,seed->cid,sizeof(identity.cid))) return false;
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(card,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK ||
           kui_stage_crc32(0u,verify,sizeof(verify))!=seed->placeholder_crc[i]) return false;
    }
    for(unsigned i=0;i<2u;i++) {
        if(kui_sci_sd_write(card,seed->lba[i],1u,
           (const uint8_t *)report_text+i*sizeof(verify))!=KUI_LOADER_SD_OK) return false;
    }
    if(kui_sci_sd_sync(card)!=KUI_LOADER_SD_OK) return false;
    for(unsigned i=0;i<2u;i++) {
        if(kui_loader_sd_read(card,seed->lba[i],1u,verify)!=KUI_LOADER_SD_OK ||
           memcmp(verify,report_text+i*sizeof(verify),sizeof(verify))) return false;
    }
    return true;
}

static bool save_report(void) {
    if(kui_sci_sd_acquire()!=KUI_LOADER_SD_OK) return false;
    struct kui_loader_sd card;
    bool ok=kui_loader_sd_init_bus(&card,kui_sci_sd_bus())==KUI_LOADER_SD_OK &&
        write_verified(&card) && kui_sci_sd_healthy();
    kui_sci_sd_release();
    return ok && !kui_sci_sd_faulted();
}

void kui_disc_toy_terminal(void) __attribute__((noreturn));
void kui_disc_toy_terminal(void) {
    volatile uint32_t *vector=(volatile uint32_t *)KUI_DISC_TOY_TRACE_GD_VECTOR;
    if(*vector==(uint32_t)(uintptr_t)kui_disc_toy_gd_proxy)
        *vector=kui_disc_toy_config.firmware;
    kui_toy_fmv_format(report_text,&trace,"PHYSICAL GD-ROM",KUI_BUILD_ID);
    bool saved=save_report();
    retail_display_restore(&kui_disc_toy_config.display);
    retail_display_line("TOY COMMANDER PHYSICAL GD-ROM");
    retail_display_line(saved?"REPORT SAVED TO SD - 30.TXT":"REPORT SAVE FAILED - PHOTO THIS SCREEN");
    if(!saved) {
        retail_display_line("REQUESTS ACKS FAILURES SHORT BYTES");
        for(unsigned i=0;i<2u;i++) {
            const struct kui_toy_fmv_movie *movie=&trace.movie[i];
            retail_display_values(i?"INTRO":"BUMPER",(uint32_t[]){movie->requests,
                movie->acks,movie->failed,movie->short_bytes},4u);
            uint32_t requested=0u, completed=0u, longest=0u, gap=0u;
            for(unsigned j=0;j<KUI_TOY_FMV_BINS;j++) {
                const struct kui_toy_fmv_bin *bin=&movie->bins[j];
                requested+=bin->requested;completed+=bin->completed;
                if(bin->longest>longest) longest=bin->longest;
                if(bin->gap>gap) gap=bin->gap;
            }
            retail_display_values("REQ ACK MAXREQ MAXGAP",(uint32_t[]){requested,completed,longest,gap},4u);
            retail_display_hex("LAST ACK END LBA",movie->end_lba);
        }
        retail_display_hex("CLOCK LOST",trace.clock_lost);
        retail_display_line("HEX / 781250 TICKS PER SECOND");
    }
    retail_display_line("POWER OFF AFTER CAPTURE");
    for(;;) __asm__ volatile("nop");
}
