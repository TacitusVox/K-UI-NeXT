/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 83: the cold stage's own text for the survey's report file
 * (toy_movie_scan in src/loader/toy_movie_iso_stage.inc, toy_survey_prepare
 * in src/loader/toy_survey_stage.inc and toy_suite_report_prepare in
 * src/loader/toy_suite_report_stage.inc), with the stage's globals and its
 * card supplied here: a RAM disk FatFs formats, behind the storage calls the
 * stage makes. The model holds the stage to the card's rules: every read and
 * write inside one acquisition, no stream left open, the block path off for
 * a write, and every launch that is refused leaves the card as it was.
 * Compiled with a private FatFs config that enables f_mkfs
 * (tests/test_toy_survey.py). The stage itself runs on the console only. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/retail_image.h"
#include "kui/storage.h"
#include "kui/vmu_native_mode.h"
#include "kui/vmu_backing.h"
#include "retail_storage.h"

/* The stage's text is built as the module's is. */
#define KUI_TOY_SUITE_MODE 66
#define KUI_NATIVE_BACKGROUND_COMPACT 1
#define KUI_NATIVE_GENERAL_LEAN_LAYOUT 1
#define KUI_NATIVE_SPLIT_CONTROLLER_GENERAL 1
#define KUI_NATIVE_READAHEAD 1
#define KUI_TOY_WIRE_STREAM 1
#define KUI_TOY_WIRE_VMU 0
#define KUI_TOY_WIRE_TITLES 1
#define KUI_TOY_WIRE_SAVE 1

#define BLOCKS (96u*1024u*1024u/512u)
#define SLOTS 4u
#define FILE_BYTES ((1u+3u*SLOTS)*512u)
#define PATH "0:/KUI/SURVEY/STREAM.TXT"
static uint8_t *disk,*before;

static struct kui_retail_manifest manifest;
static struct kui_retail_image image;
static struct kui_retail_storage card;
static uint8_t raw_boot[2u*KUI_GAME_RAW_BYTES];
static uint32_t boot_crc,exec_bytes;
static uint32_t mode;
static uint32_t native_runtime_mode(void) {return mode;}

/* The stage's own flag for its stop screen, set before the card is written. */
static bool toy_survey_wrote;
static bool toy_survey_wrote_before_write(void) {return toy_survey_wrote;}
/* The card as the stage reaches it. */
static struct {
    unsigned acquires,releases,reads,writes,syncs,identities;
    unsigned fail_acquire_at,fail_read_at,sync_failures;
    enum kui_loader_sd_result write_result;
    bool drop_write,unhealthy,faulted,no_identity;
    /* A read that fails for good: the card is left not ready and its stream
     * cannot be confirmed stopped, as the real reader leaves them. */
    bool read_kills,stop_failed;
    uint32_t written[4];
} io;
static bool owned,stream_open;
static const uint8_t card_cid[16]={0xa1,0xb2,0xc3,0xd4,1,2,3,4,5,6,7,8,9,10,11,12};
static bool block_path(void *ctx,const uint8_t *tx,uint8_t *rx,size_t count,bool slow,uint16_t *crc) {
    (void)ctx;(void)tx;(void)rx;(void)count;(void)slow;(void)crc;
    assert(!"the stage's block path only reads: a write must not use it");
    return false;
}
enum kui_loader_sd_result kui_retail_storage_acquire(struct kui_retail_storage *s) {
    assert(s==&card && !owned);
    if(++io.acquires==io.fail_acquire_at) return KUI_LOADER_SD_TIMEOUT;
    owned=true;return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_retail_storage_read_run(struct kui_retail_storage *s,uint32_t lba,
    uint32_t available,uint8_t out[512]) {
    assert(s==&card && owned && !stream_open && available==1u && lba<BLOCKS && card.device.sd.ready);
    stream_open=true;
    if(++io.reads==io.fail_read_at) {
        if(io.read_kills) {card.device.sd.ready=false;io.stop_failed=true;}
        return KUI_LOADER_SD_TIMEOUT;
    }
    memcpy(out,disk+(size_t)lba*512u,512u);return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_retail_storage_stop(struct kui_retail_storage *s) {
    assert(s==&card && owned);stream_open=false;
    if(io.stop_failed) {io.stop_failed=false;return KUI_LOADER_SD_TIMEOUT;}
    return KUI_LOADER_SD_OK;
}
void kui_retail_storage_release(struct kui_retail_storage *s) {
    assert(s==&card && owned && !stream_open);owned=false;++io.releases;
}
enum kui_loader_sd_result kui_sci_sd_write(struct kui_loader_sd *sd,uint32_t lba,uint32_t count,const uint8_t *data) {
    assert(sd==&card.device.sd && owned && !stream_open && count==1u && data && lba<BLOCKS);
    assert(sd->ready && !sd->bus.transfer_block && toy_survey_wrote_before_write());
    assert(io.writes<4u);io.written[io.writes++]=lba;
    if(io.write_result!=KUI_LOADER_SD_OK) {sd->ready=false;sd->last_response=0x0du;return io.write_result;}
    if(!io.drop_write) memcpy(disk+(size_t)lba*512u,data,512u);
    return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_sci_sd_sync(struct kui_loader_sd *sd) {
    assert(sd==&card.device.sd && owned && !stream_open && sd->ready);
    ++io.syncs;
    if(io.sync_failures) {--io.sync_failures;sd->ready=false;return KUI_LOADER_SD_COMMAND;}
    return KUI_LOADER_SD_OK;
}
bool kui_sci_sd_healthy(void) {return !io.unhealthy;}
bool kui_sci_sd_faulted(void) {return io.faulted;}
int kui_vmu_sd_identity_read(struct kui_loader_sd *sd,struct kui_vmu_sd_identity *out) {
    assert(sd==&card.device.sd && owned && !stream_open);
    ++io.identities;
    /* The real read marks the card not ready whenever it fails (src/loader/vmu_backing.c). */
    if(io.no_identity) {sd->ready=false;return KUI_VMU_BACKING_IO;}
    memcpy(out->cid,card_cid,16u);out->sectors=sd->blocks;out->high_capacity=sd->high_capacity;
    return KUI_VMU_BACKING_OK;
}
static char shown[4][80];
static unsigned lines;
void retail_display_line(const char *text) {
    if(lines<4u) snprintf(shown[lines],sizeof(shown[0]),"%s",text);
    ++lines;
}
void retail_display_hex(const char *text,uint32_t value) {
    if(lines<4u) snprintf(shown[lines],sizeof(shown[0]),"%s %08X",text,(unsigned)value);
    ++lines;
}

#include "toy_movie_iso_stage.inc"
#include "toy_survey_stage.inc"
#include "toy_suite_report_stage.inc"

/* FatFs, to format the card and to read the file back. */
DSTATUS disk_initialize(BYTE drive) {return drive || !disk?STA_NOINIT:0;}
DSTATUS disk_status(BYTE drive) {return disk_initialize(drive);}
DRESULT disk_read(BYTE drive,BYTE *out,LBA_t start,UINT count) {
    if(drive || start>=BLOCKS || count>BLOCKS-start) return RES_PARERR;
    memcpy(out,disk+(size_t)start*512u,(size_t)count*512u);return RES_OK;
}
DRESULT disk_write(BYTE drive,const BYTE *in,LBA_t start,UINT count) {
    if(drive || start>=BLOCKS || count>BLOCKS-start) return RES_PARERR;
    memcpy(disk+(size_t)start*512u,in,(size_t)count*512u);return RES_OK;
}
DRESULT disk_ioctl(BYTE drive,BYTE operation,void *out) {
    if(drive) return RES_NOTRDY;
    switch(operation) {
    case CTRL_SYNC:return RES_OK;
    case GET_SECTOR_COUNT:*(LBA_t *)out=BLOCKS;return RES_OK;
    case GET_SECTOR_SIZE:*(WORD *)out=512;return RES_OK;
    case GET_BLOCK_SIZE:*(DWORD *)out=1;return RES_OK;
    default:return RES_PARERR;
    }
}
DWORD get_fattime(void) {return ((2026u-1980u)<<25)|(11u<<21)|(20u<<16);}

/* Whether a sector is a launch record of this file's slot, by its first line. */
static bool launch_record(const struct kui_toy_survey_file *file,uint32_t slot,const uint8_t sector[512]) {
    char line[128];int n=snprintf(line,sizeof(line),"==== SLOT %03u PART 0 ==== ID ",(unsigned)slot);
    for(unsigned i=0u;i<16u;i++) n+=snprintf(line+n,sizeof(line)-(size_t)n,"%02X",file->id[i]);
    n+=snprintf(line+n,sizeof(line)-(size_t)n," ==== LAUNCH ====\n");
    return !memcmp(sector,line,(size_t)n) && sector[511]=='\n';
}
static const struct kui_toy_survey_file survey={
    {0x5a,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff},SLOTS};
static uint8_t blank[FILE_BYTES];
static uint32_t file_lba[1u+3u*SLOTS];
static void card_make(bool exfat,bool with_file) {
    static FATFS fs;static BYTE work[65536];
    FIL file;UINT done;
    memset(disk,0,(size_t)BLOCKS*512u);
    MKFS_PARM p={.fmt=(BYTE)((exfat?FM_EXFAT:FM_FAT32)|FM_SFD),.n_fat=2,.align=1,.n_root=512,.au_size=exfat?4096u:512u};
    assert(f_mkfs("0:",&p,work,sizeof(work))==FR_OK && f_mount(&fs,"0:",1)==FR_OK);
    assert(f_mkdir("0:/KUI")==FR_OK && f_mkdir("0:/KUI/apps")==FR_OK && f_mkdir("0:/KUI/SURVEY")==FR_OK);
    kui_toy_survey_file_sector(&survey,blank);
    for(uint32_t slot=0u;slot<SLOTS;slot++)
        for(uint32_t part=0u;part<3u;part++)
            kui_toy_survey_blank_sector(&survey,slot,part,blank+512u*kui_toy_survey_sector(slot,part));
    if(with_file) {
        assert(f_open(&file,PATH,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
        assert(f_write(&file,blank,FILE_BYTES,&done)==FR_OK && done==FILE_BYTES && f_close(&file)==FR_OK);
    }
    assert(f_mount(NULL,"0:",0)==FR_OK);
    /* Where the file's sectors are: FatFs wrote them, the test finds them. */
    memset(file_lba,0,sizeof(file_lba));
    for(uint32_t s=0u;with_file && s<1u+3u*SLOTS;s++) {
        for(uint32_t lba=0u;lba<BLOCKS && !file_lba[s];lba++)
            if(!memcmp(disk+(size_t)lba*512u,blank+512u*s,512u)) file_lba[s]=lba;
        assert(file_lba[s]);
    }
    memcpy(before,disk,(size_t)BLOCKS*512u);
}
static uint32_t changed(void) {
    uint32_t n=0u;
    for(uint32_t lba=0u;lba<BLOCKS;lba++) if(memcmp(disk+(size_t)lba*512u,before+(size_t)lba*512u,512u)) ++n;
    return n;
}

enum {RAW_DATA=4u,COOKED_DATA=4u|KUI_RETAIL_TRACK_COOKED,MODE2_DATA=4u|KUI_RETAIL_TRACK_MODE2,AUDIO=0u};
/* A launch: the disc's map, the card's state, then the stage's scan. */
static void launch(uint32_t reader,uint32_t transport,const uint8_t *controls,unsigned tracks) {
    memset(&manifest,0,sizeof(manifest));
    manifest.card_sectors=BLOCKS;manifest.partition_start=0u;manifest.partition_end=BLOCKS;
    manifest.reader=reader;manifest.storage_transport=transport;manifest.flags=0x4u;
    manifest.session_lba=45150u;manifest.boot_lba=45200u;manifest.boot_bytes=1234567u;
    manifest.boot_crc32=0x0b0b0b0bu;manifest.ip_crc32=0x12345678u;manifest.gdi_crc32=0x0d0d0d0du;
    memcpy(manifest.title,"EXAMPLE GAME                    ",32u);
    memcpy(manifest.product,"T-12345N  ",10u);memcpy(manifest.bootfile,"1ST_READ.BIN",12u);
    memcpy(manifest.region,"JUE",3u);
    boot_crc=0x9abcdef0u;exec_bytes=4057056u;
    manifest.track_count=tracks;
    for(unsigned i=0u;i<tracks;i++)
        manifest.slots[i].track=(struct kui_retail_track){i<2u?150u+1000u*i:45150u+1000u*i,
            i<2u?1150u+1000u*i:46150u+1000u*i,0u,(uint8_t)(i==3u),controls[i]};
    /* The game's image: one extent of the card, well away from the file. */
    manifest.extent_count=1u;
    manifest.slots[tracks].extent=(struct kui_retail_extent){0u,BLOCKS-4096u,2048u};
    mode=KUI_VMU_NATIVE_MODE_READER_ONLY;
    memset(&card,0,sizeof(card));
    card.transport=transport;card.device.sd.blocks=BLOCKS;card.device.sd.ready=true;
    card.device.sd.high_capacity=true;card.device.sd.bus.transfer_block=block_path;
    toy_suite_source=0u;lines=0u;memset(shown,0,sizeof(shown));
}
static bool dead; /* the case leaves a card that no longer answers */
static void scan(void) {
    toy_movie_scan();
    assert(!owned && !stream_open && io.acquires==io.releases+(io.fail_acquire_at && io.acquires>=io.fail_acquire_at));
    assert(card.device.sd.bus.transfer_block==block_path);
    /* A launch Test 81 would boot goes on: the low resident refuses a card
     * that is not ready. */
    assert(card.device.sd.ready==!dead);
    dead=false;
}
/* A launch the survey is off for: no seed, the card as it was (but for the
 * writes the case allows), and the report's preparation gives the worker
 * nothing. */
static void off(const char *line,unsigned writes) {
    struct kui_toy_report_wire out;
    scan();
    if(strcmp(shown[0],line)) {fprintf(stderr,"FAIL shown \"%s\", not \"%s\"\n",shown[0],line);exit(1);}
    assert(lines==1u && !toy_survey_seed.test && io.writes==writes && toy_survey_wrote==(writes!=0u));
    memset(&out,0,sizeof(out));
    toy_suite_report_prepare(&out);
    assert(!out.test && !out.lba[0] && !out.lba[1]);
}
static const uint8_t raw[5]={AUDIO,AUDIO,RAW_DATA,AUDIO,AUDIO},cooked[3]={AUDIO,AUDIO,COOKED_DATA};
static void good(uint32_t slot,const uint8_t *controls,unsigned tracks,uint32_t source) {
    char line[80];
    struct kui_toy_report_wire out;
    memset(&io,0,sizeof(io));
    launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,controls,tracks);
    uint32_t differ=changed();
    scan();
    snprintf(line,sizeof(line),"SURVEY ON / REPORT SAVES TO SLOT %03u",(unsigned)slot);
    if(strcmp(shown[0],line)) {fprintf(stderr,"FAIL shown \"%s\", not \"%s\"\n",shown[0],line);exit(1);}
    assert(lines==1u && toy_suite_source==source);
    /* One sector written: the slot's part 0, a launch record of this game. */
    assert(io.writes==1u && io.written[0]==file_lba[kui_toy_survey_sector(slot,0u)] && changed()==differ+1u);
    assert(io.identities==1u && !io.syncs && toy_survey_wrote);
    const uint8_t *record=disk+(size_t)io.written[0]*512u;
    assert(launch_record(&survey,slot,record));
    char text[513];memcpy(text,record,512u);text[512]=0;
    char expect[400];
    snprintf(expect,sizeof(expect),"\nSURVEY 83 BUILD " KUI_BUILD_ID "\nPRODUCT T-12345N\nTITLE EXAMPLE GAME\n"
        "BOOT 1ST_READ.BIN REGION JUE\nIPCRC 12345678 GDICRC 0D0D0D0D BOOTCRC 0B0B0B0B BOOTBYTES 0012D687\n"
        "EXECCRC 9ABCDEF0 EXECBYTES 003DE7E0\nREADER 1 TRANSPORT 1 FLAGS 00000004 SOURCE %u\n",(unsigned)source);
    assert(strstr(text,expect));
    assert(strstr(text,tracks==5u?"\nTRACKS 5 RAW 1 COOKED 0 AUDIO 4 HDAUDIO 2 MAPPED 1 OTHER 0 EXTENTS 1\nSESSION 0000B05E BOOTLBA 0000B090\n":
        "\nTRACKS 3 RAW 0 COOKED 1 AUDIO 2 HDAUDIO 0 MAPPED 0 OTHER 0 EXTENTS 1\nSESSION 0000B05E BOOTLBA 0000B090\n"));
    /* The worker's seed: the slot's other two sectors, still blank, on this card. */
    memset(&out,0,sizeof(out));
    toy_suite_report_prepare(&out);
    assert(!memcmp(&out,&toy_survey_seed,sizeof(out)) && out.test==66u && out.bytes==64u && !out.reserved);
    assert(out.magic[0]==KUI_TOY_REPORT_MAGIC0 && out.magic[1]==KUI_TOY_REPORT_MAGIC1 && out.version==1u);
    assert(kui_retail_crc32(0u,&out,KUI_TOY_REPORT_CRC_OFFSET)==out.crc32);
    assert(!memcmp(out.cid,card_cid,16u) && out.high_capacity==1u);
    for(unsigned i=0u;i<2u;i++) {
        assert(out.lba[i]==file_lba[kui_toy_survey_sector(slot,1u+i)] && out.lba[i]<BLOCKS);
        assert(kui_retail_crc32(0u,disk+(size_t)out.lba[i]*512u,512u)==out.placeholder_crc[i]);
        assert(!memcmp(disk+(size_t)out.lba[i]*512u,blank+512u*kui_toy_survey_sector(slot,1u+i),512u));
    }
}

int main(void) {
    disk=calloc(BLOCKS,512u);before=calloc(BLOCKS,512u);
    if(!disk || !before) return 2;
    for(unsigned exfat=0u;exfat<2u;exfat++) {
        card_make(exfat,true);
        /* A source for the window, then a disc that is none: both get a slot. */
        good(0u,raw,5u,1u);
        good(1u,cooked,3u,0u);
        /* Launches the survey is off for write nothing, whatever the file holds. */
        memcpy(before,disk,(size_t)BLOCKS*512u);
        memset(&io,0,sizeof(io));
        launch(KUI_RETAIL_READER_ASYNC_EAGER,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / NOT THE NEW (X) READER",0u);assert(!io.acquires);
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCIF,raw,5u);
        off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);card.transport=KUI_STORAGE_SCIF;
        off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);manifest.storage_transport=KUI_STORAGE_IDE;
        off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
        /* The virtual VMU on: the module's stage stops such a launch before
         * this; were it to come here, no survey. */
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);mode=KUI_VMU_NATIVE_MODE_NORMAL;
        off("SURVEY OFF / NOT THE NEW (X) READER",0u);assert(!io.acquires);
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);manifest.partition_end=UINT64_C(0x100000000);
        off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
        /* A slot's sector in an extent of the game's, past the map's card, or past the card itself. */
        for(unsigned part=0u;part<3u;part++) {
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            manifest.slots[5].extent=(struct kui_retail_extent){0u,file_lba[kui_toy_survey_sector(2u,part)]-(part?0u:3u),part?1u:4u};
            char line[80];snprintf(line,sizeof(line),"SURVEY OFF / FILE PLACE %08X",(unsigned)file_lba[kui_toy_survey_sector(2u,part)]);
            off(line,0u);
        }
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        manifest.card_sectors=file_lba[kui_toy_survey_sector(2u,1u)];
        {char line[80];snprintf(line,sizeof(line),"SURVEY OFF / FILE PLACE %08X",(unsigned)file_lba[kui_toy_survey_sector(2u,1u)]);off(line,0u);}
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        card.device.sd.blocks=file_lba[kui_toy_survey_sector(2u,2u)];
        {char line[80];snprintf(line,sizeof(line),"SURVEY OFF / FILE PLACE %08X",(unsigned)file_lba[kui_toy_survey_sector(2u,2u)]);off(line,0u);}
        /* The write's own guard, asked directly: a sector of the game's, one
         * past the card, the partition's first: the card is not asked for. */
        memset(&io,0,sizeof(io));
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        assert(toy_survey_write(BLOCKS-4096u+7u,blank)==-1 && toy_survey_write(BLOCKS,blank)==-1 &&
               toy_survey_write(0u,blank)==-1 && !io.acquires && !io.writes);
        manifest.partition_start=2048u;
        assert(toy_survey_write(2047u,blank)==-1 && !io.acquires && !io.writes);
        /* The card does not answer: at any acquisition, at any read. */
        memset(&io,0,sizeof(io));
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);io.no_identity=true;
        off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);
        /* The identity command left the card not ready; it answered its status, and the launch goes on. */
        assert(io.syncs==1u);
        unsigned acquires=io.acquires-1u,reads=io.reads;
        assert(acquires>3u && reads>3u);
        /* ... at the second asking; or not at all, and then the launch stops as it must. */
        memset(&io,0,sizeof(io));io.no_identity=true;io.sync_failures=1u;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==2u);
        memset(&io,0,sizeof(io));io.no_identity=true;io.sync_failures=2u;dead=true;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==2u);
        /* It answers, and the port is not well: no. */
        for(unsigned which=0u;which<2u;which++) {
            memset(&io,0,sizeof(io));io.no_identity=true;io.unhealthy=!which;io.faulted=which;dead=true;
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==1u);
        }
        /* A card that is not ready when the survey comes to it is not asked for anything. */
        memset(&io,0,sizeof(io));dead=true;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);card.device.sd.ready=false;
        off("SURVEY OFF / CARD NOT READY",0u);assert(!io.acquires && !io.syncs);
        for(unsigned at=1u;at<=acquires;at++) {
            memset(&io,0,sizeof(io));io.fail_acquire_at=at;
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            scan();
            assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / ",13u) && !toy_survey_seed.test && !io.writes);
        }
        for(unsigned at=1u;at<=reads;at++) {
            memset(&io,0,sizeof(io));io.fail_read_at=at;
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            scan();
            assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / FILE ",18u) && !toy_survey_seed.test && !io.writes);
            /* The same read leaving the card not ready, its stream not confirmed stopped: asked for its status once. */
            memset(&io,0,sizeof(io));io.fail_read_at=at;io.read_kills=true;
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            scan();
            assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / FILE ",18u) && !toy_survey_seed.test && !io.writes &&
                   io.syncs==1u);
        }
        assert(!changed());
        /* The write is refused by the card: it answers its status afterwards, and the launch goes on with it. */
        memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TOKEN;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / CARD WRITE 0000000D",1u);
        assert(io.syncs==1u && !changed());
        /* ... or answers at the second asking only, or not at all. */
        memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TIMEOUT;io.sync_failures=1u;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / CARD WRITE 0000000D",1u);
        assert(io.syncs==2u && !changed());
        memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TIMEOUT;io.sync_failures=2u;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        dead=true;
        off("SURVEY OFF / CARD WRITE 0000000D",1u);
        assert(io.syncs==2u && !changed());
        /* The port is left unwell by a write the card took: no seed. */
        for(unsigned which=0u;which<2u;which++) {
            memset(&io,0,sizeof(io));io.unhealthy=!which;io.faulted=which;io.drop_write=true;
            launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
            off("SURVEY OFF / CARD WRITE 00000000",1u);
        }
        /* A write the card took and did not keep. */
        memset(&io,0,sizeof(io));io.drop_write=true;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / WRITE NOT READ BACK",1u);
        assert(io.written[0]==file_lba[kui_toy_survey_sector(2u,0u)] && !changed());
        /* The card is not to be had for the write: nothing written. */
        memset(&io,0,sizeof(io));io.fail_acquire_at=acquires+1u;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / CARD WRITE 00000000",0u);
        assert(!changed());
        /* The record is written and cannot be read back: the slot is spent,
         * with its launch record and no seed, and the next launch takes the
         * next one. */
        memset(&io,0,sizeof(io));
        if(exfat) io.fail_read_at=reads+1u; else io.fail_acquire_at=acquires+2u;
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / WRITE NOT READ BACK",1u);
        assert(changed()==1u && io.written[0]==file_lba[kui_toy_survey_sector(2u,0u)] &&
               launch_record(&survey,2u,disk+(size_t)io.written[0]*512u));
        memcpy(before,disk,(size_t)BLOCKS*512u);
        /* The last slot; then the file is full. */
        good(3u,cooked,3u,0u);
        memcpy(before,disk,(size_t)BLOCKS*512u);
        memset(&io,0,sizeof(io));
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / FILE FULL - COPY A BLANK ONE",0u);
        assert(!changed());
        /* No file, no folder: off, and not a write. */
        card_make(exfat,false);
        memset(&io,0,sizeof(io));
        launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
        off("SURVEY OFF / NO KUI/SURVEY/STREAM.TXT",0u);
        assert(!changed());
        printf("PASS the stage's survey text on %s: a slot for a source and for a disc that is none; one sector "
               "written per launch, read back; every refusal leaves the card as it was\n",exfat?"exFAT":"FAT32");
    }
    /* No card that is a volume at all. */
    memset(disk,0,(size_t)BLOCKS*512u);memcpy(before,disk,(size_t)BLOCKS*512u);
    memset(&io,0,sizeof(io));
    launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
    off("SURVEY OFF / FILE LOOKUP 00000002",0u);
    assert(!changed());
    printf("PASS the stage's survey text\n");
    free(disk);free(before);
    return 0;
}
