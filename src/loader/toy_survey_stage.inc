/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 83, the stream survey, stage side (include/kui/toy_survey.h). Included
 * by the cold stage only in that build (KUI_TOY_WIRE_SAVE).
 *
 * Card Tools found a report file's two sectors and patched their place into
 * the module; the Games app patches nothing, and from a Games launch Tests 79
 * to 81 showed their report on pages. Here the stage finds the place itself,
 * once the executable is loaded and before the reader is installed: it looks
 * 0:/KUI/SURVEY/STREAM.TXT up (src/core/toy_survey_fat.c, read-only), takes
 * the file's first slot whose three sectors all read back blank
 * (src/core/toy_survey.c), writes the launch record into the first of them
 * and hands the other two to the Toy worker as its report seed. The worker,
 * the low residents and the auxiliary are Test 81's, byte for byte.
 *
 * The one sector written here was read back as that slot's blank part 0 of
 * this file a moment before, lies in the partition and in no extent of the
 * game's, and is read back after the write. Any failure leaves the launch as
 * Test 81 runs it, with no report file: the pages again. The survey reads
 * sectors Test 81's stage never asks for and sends the card two commands it
 * never sends (its identity, a write); where one of them leaves the card
 * marked not ready, the card is asked for its status before the launch goes
 * on (toy_survey_settle), and stops the launch only if it does not answer. */
#include "kui/toy_survey.h"
#include "kui/vmu_backing.h"
#include "../dreamcast/sci_sd_storage.h"
#include "sci_sd_bus.h"
__asm__(".global __kui_toy_survey_stage\n.set __kui_toy_survey_stage,1\n");
_Static_assert(KUI_TOY_SURVEY_SEED_TEST==KUI_TOY_SUITE_MODE,"the survey's seed is this module's test entry");

static struct kui_toy_report_wire toy_survey_seed;
static struct kui_wce_probe_layout toy_survey_layout;
static struct kui_toy_survey_claim toy_survey_claim;
/* Whole cache lines of their own, as the image reader's sector: every read
 * into them takes the card bus's one block path, wherever the link puts them. */
static uint8_t toy_survey_work[KUI_TOY_SURVEY_SECTOR] __attribute__((aligned(32)));
static uint8_t toy_survey_text[KUI_TOY_SURVEY_SECTOR] __attribute__((aligned(32)));
/* A write to the card was begun: the stage's stop screen says so
 * (retail_stage.c, stopped). */
static bool toy_survey_wrote;

static int toy_survey_read(void *context,uint32_t lba,uint8_t out[512]) {
    (void)context;
    if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;
    enum kui_loader_sd_result read=kui_retail_storage_read_run(&card,lba,1u,out);
    enum kui_loader_sd_result stop=kui_retail_storage_stop(&card);
    kui_retail_storage_release(&card);
    return read==KUI_LOADER_SD_OK && stop==KUI_LOADER_SD_OK?0:-1;
}
/* A sector of the card that a survey sector may be: inside the partition and
 * the card, and in no extent of the game's image. */
static bool toy_survey_place(uint32_t lba) {
    if(!lba || lba<manifest.partition_start || lba>=manifest.partition_end ||
       lba>=manifest.card_sectors || lba>=card.device.sd.blocks) return false;
    for(uint32_t j=0u;j<manifest.extent_count;j++) {
        const struct kui_retail_extent *e=&manifest.slots[manifest.track_count+j].extent;
        if(lba>=e->card_lba && lba-e->card_lba<e->blocks) return false;
    }
    return true;
}
/* One sector, by the byte path the virtual VMU's stage writes by
 * (native_persist_pio_write): the stage's block path only reads. */
static int toy_survey_write(uint32_t lba,const uint8_t *data) {
    if(!toy_survey_place(lba) || kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return -1;
    struct kui_loader_sd *sd=&card.device.sd;
    bool (*block)(void *,const uint8_t *,uint8_t *,size_t,bool,uint16_t *)=sd->bus.transfer_block;
    sd->bus.transfer_block=NULL;
    toy_survey_wrote=true;
    enum kui_loader_sd_result result=kui_sci_sd_write(sd,lba,1u,data);
    sd->bus.transfer_block=block;
    bool healthy=kui_sci_sd_healthy() && !kui_sci_sd_faulted();
    kui_retail_storage_release(&card);
    return result==KUI_LOADER_SD_OK && healthy?0:-1;
}
/* A read, the identity command or the write can leave the card marked not
 * ready, and the low resident refuses such a card: a launch Test 81 would
 * boot must not stop for the survey. The card is kept while it still answers
 * its status command with the port well. A write error stays with the card
 * for the first status read, which clears it, so a second one decides (as
 * the Windows CE data probe's stage does after a failed write). */
static void toy_survey_settle(void) {
    struct kui_loader_sd *sd=&card.device.sd;
    if(sd->ready || kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) return;
    sd->ready=true;
    if(kui_sci_sd_sync(sd)!=KUI_LOADER_SD_OK) {sd->ready=true;(void)kui_sci_sd_sync(sd);}
    if(!kui_sci_sd_healthy() || kui_sci_sd_faulted()) sd->ready=false;
    kui_retail_storage_release(&card);
}
static void toy_survey_line(const char *text,uint32_t slot) {
    char line[40];unsigned n=0u;
    while(*text && n<sizeof(line)-5u) line[n++]=*text++;
    line[n++]=' ';line[n++]=(char)('0'+slot/100u%10u);line[n++]=(char)('0'+slot/10u%10u);
    line[n++]=(char)('0'+slot%10u);line[n]=0;
    retail_display_line(line);
}

/* "SURVEY ..." on the launch screen says what was found. */
static void toy_survey_look(void) {
    if(manifest.reader!=KUI_RETAIL_READER_ASYNC || native_runtime_mode()!=KUI_VMU_NATIVE_MODE_READER_ONLY) {
        retail_display_line("SURVEY OFF / NOT THE NEW (X) READER");return;
    }
    if(manifest.storage_transport!=KUI_STORAGE_SCI || card.transport!=KUI_STORAGE_SCI ||
       manifest.partition_end>UINT32_MAX || manifest.partition_start>=manifest.partition_end) {
        retail_display_line("SURVEY OFF / NOT THE SCI CARD");return;
    }
    const struct kui_wce_probe_volume volume={(uint32_t)manifest.partition_start,
        (uint32_t)manifest.partition_end,toy_survey_read,NULL};
    enum kui_wce_probe_fat_result found=kui_wce_probe_fat_find(&volume,&toy_survey_layout,toy_survey_work);
    if(found!=KUI_WCE_PROBE_FAT_OK) {
        if(found==KUI_WCE_PROBE_FAT_MISSING) retail_display_line("SURVEY OFF / NO KUI/SURVEY/STREAM.TXT");
        else retail_display_hex("SURVEY OFF / FILE LOOKUP",(uint32_t)found);
        return;
    }
    const struct kui_toy_survey_claim *c=&toy_survey_claim;
    enum kui_toy_survey_claim_result claimed=kui_toy_survey_claim_find(&volume,&toy_survey_layout,
        toy_survey_work,toy_survey_text,&toy_survey_claim);
    if(claimed!=KUI_TOY_SURVEY_CLAIM_OK) {
        if(claimed==KUI_TOY_SURVEY_CLAIM_FULL) retail_display_line("SURVEY OFF / FILE FULL - COPY A BLANK ONE");
        else retail_display_hex("SURVEY OFF / FILE CHECK",(uint32_t)claimed);
        return;
    }
    for(unsigned i=0u;i<KUI_TOY_SURVEY_PARTS;i++)
        if(!toy_survey_place(c->lba[i])) {retail_display_hex("SURVEY OFF / FILE PLACE",c->lba[i]);return;}
    struct kui_vmu_sd_identity identity;
    if(kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK) {retail_display_line("SURVEY OFF / CARD BUSY");return;}
    int known=kui_vmu_sd_identity_read(&card.device.sd,&identity);
    kui_retail_storage_release(&card);
    if(known!=KUI_VMU_BACKING_OK) {retail_display_hex("SURVEY OFF / CARD IDENTITY",(uint32_t)known);return;}

    struct kui_toy_survey_launch launch={.build=KUI_BUILD_ID,
        .title=manifest.title,.title_bytes=sizeof(manifest.title),
        .product=manifest.product,.product_bytes=sizeof(manifest.product),
        .bootfile=manifest.bootfile,.bootfile_bytes=sizeof(manifest.bootfile),
        .region=manifest.region,.region_bytes=sizeof(manifest.region),
        .ip_crc32=manifest.ip_crc32,.gdi_crc32=manifest.gdi_crc32,
        .boot_crc32=manifest.boot_crc32,.boot_bytes=manifest.boot_bytes,
        .exec_crc32=boot_crc,.exec_bytes=exec_bytes,
        .reader=manifest.reader,.transport=manifest.storage_transport,.flags=manifest.flags,
        .source=toy_suite_source,.tracks=manifest.track_count,.extents=manifest.extent_count,
        .session_lba=manifest.session_lba,.boot_lba=manifest.boot_lba};
    for(uint32_t i=0u;i<manifest.track_count;i++) {
        const struct kui_retail_track *track=&manifest.slots[i].track;
        if(!kui_retail_track_control(track)) {
            ++launch.audio;
            if(track->start_lba>=manifest.session_lba) ++launch.audio_high;
            if(track->extent_count) ++launch.audio_mapped;
        } else if(kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&
                  kui_retail_track_header_bytes(track)==16u) ++launch.raw;
        else if(kui_retail_track_sector_bytes(track)==KUI_GAME_DATA_BYTES) ++launch.cooked;
        else ++launch.other;
    }
    /* The launch record first, and read back: only a slot that names its
     * game is given to the worker. A write cut off leaves a slot that is no
     * longer blank, which later launches pass over. */
    kui_toy_survey_launch_sector(&c->file,c->slot,&launch,toy_survey_text);
    if(toy_survey_write(c->lba[0],toy_survey_text)) {
        retail_display_hex("SURVEY OFF / CARD WRITE",card.device.sd.last_response);return;
    }
    if(toy_survey_read(NULL,c->lba[0],toy_survey_work) ||
       memcmp(toy_survey_work,toy_survey_text,KUI_TOY_SURVEY_SECTOR)) {
        retail_display_line("SURVEY OFF / WRITE NOT READ BACK");return;
    }
    kui_toy_survey_seed_build(c,identity.cid,identity.high_capacity,&toy_survey_seed);
    toy_survey_line("SURVEY ON / REPORT SAVES TO SLOT",c->slot);
}
static void toy_survey_prepare(void) {
    memset(&toy_survey_seed,0,sizeof(toy_survey_seed));
    toy_survey_wrote=false;
    if(!card.device.sd.ready) {retail_display_line("SURVEY OFF / CARD NOT READY");return;}
    toy_survey_look();
    toy_survey_settle();
}
