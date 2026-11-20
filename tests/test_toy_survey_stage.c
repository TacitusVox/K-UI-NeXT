/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 83: the cold stage's own text for the survey's report file
 * (toy_movie_scan in src/loader/toy_movie_iso_stage.inc, toy_survey_prepare
 * in src/loader/toy_survey_stage.inc and toy_suite_report_prepare in
 * src/loader/toy_suite_report_stage.inc), with the stage's globals and its
 * card supplied here: a RAM disk FatFs formats, behind the storage calls the
 * stage makes. The model holds the stage to the card's rules: every read and
 * write inside one acquisition and inside the map's partition, no stream left
 * open, the block path off for a write, and every launch that is refused
 * leaves the card as it was. Every case runs on a card without a partition
 * table and on one with (the volume at sector 63, as FatFs makes it).
 * Compiled with a private FatFs config that enables f_mkfs
 * (tests/test_toy_survey.py). The stage itself runs on the console only. */
#define _DEFAULT_SOURCE /* mprotect and sysconf, for the seed Card Tools patches */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
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
#define SLOTS 12u
#define FILE_BYTES ((1u+3u*SLOTS)*512u)
#define PATH "0:/KUI/SURVEY/STREAM.TXT"
static uint8_t *disk,*before;
/* The volume the Games map names: the whole card, or its partition. */
static uint32_t part_first,part_end;

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
    unsigned acquires,releases,reads,stops,writes,syncs,identities;
    unsigned fail_acquire_at,fail_read_at,fail_stop_at,sync_failures;
    enum kui_loader_sd_result write_result;
    bool drop_write,unhealthy,faulted,no_identity;
    /* A read that fails for good: the card is left not ready and its stream
     * cannot be confirmed stopped, as the real reader leaves them. Such a
     * card is still sending the read: it must not be asked anything. */
    bool read_kills,stop_failed,in_read;
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
    assert(s==&card && owned && !stream_open && available==1u && lba<BLOCKS);
    assert(lba>=manifest.partition_start && lba<manifest.partition_end && "a read outside the map's partition");
    /* A card that is not ready is sent nothing (kui_loader_sd_stream_start). */
    if(!card.device.sd.ready) return KUI_LOADER_SD_NOT_READY;
    assert(!io.in_read);
    stream_open=true;
    if(++io.reads==io.fail_read_at) {
        if(io.read_kills) {card.device.sd.ready=false;io.stop_failed=true;io.in_read=true;}
        return KUI_LOADER_SD_TIMEOUT;
    }
    memcpy(out,disk+(size_t)lba*512u,512u);return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_retail_storage_stop(struct kui_retail_storage *s) {
    assert(s==&card && owned);stream_open=false;
    if(io.stop_failed) {io.stop_failed=false;return KUI_LOADER_SD_TIMEOUT;}
    /* A stop the card does not take, after a read that was good: the card is
     * left not ready (stream_finish, src/loader/sd_reader.c) and may still be
     * sending. */
    if(++io.stops==io.fail_stop_at) {card.device.sd.ready=false;io.in_read=true;return KUI_LOADER_SD_TIMEOUT;}
    return KUI_LOADER_SD_OK;
}
void kui_retail_storage_release(struct kui_retail_storage *s) {
    assert(s==&card && owned && !stream_open);owned=false;++io.releases;
}
enum kui_loader_sd_result kui_sci_sd_write(struct kui_loader_sd *sd,uint32_t lba,uint32_t count,const uint8_t *data) {
    assert(sd==&card.device.sd && owned && !stream_open && count==1u && data && lba<BLOCKS);
    assert(sd->ready && !sd->bus.transfer_block && toy_survey_wrote_before_write() && !io.in_read);
    assert(lba>=manifest.partition_start && lba<manifest.partition_end && "a write outside the map's partition");
    assert(io.writes<4u);io.written[io.writes++]=lba;
    if(io.write_result!=KUI_LOADER_SD_OK) {sd->ready=false;sd->last_response=0x0du;return io.write_result;}
    if(!io.drop_write) memcpy(disk+(size_t)lba*512u,data,512u);
    return KUI_LOADER_SD_OK;
}
enum kui_loader_sd_result kui_sci_sd_sync(struct kui_loader_sd *sd) {
    assert(sd==&card.device.sd && owned && !stream_open && sd->ready);
    assert(!io.in_read && "a card inside a read is asked for its status");
    ++io.syncs;
    if(io.sync_failures) {--io.sync_failures;sd->ready=false;return KUI_LOADER_SD_COMMAND;}
    return KUI_LOADER_SD_OK;
}
bool kui_sci_sd_healthy(void) {return !io.unhealthy;}
bool kui_sci_sd_faulted(void) {return io.faulted;}
int kui_vmu_sd_identity_read(struct kui_loader_sd *sd,struct kui_vmu_sd_identity *out) {
    assert(sd==&card.device.sd && owned && !stream_open && !io.in_read);
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
static uint32_t le32(const uint8_t *at) {
    return (uint32_t)at[0]|((uint32_t)at[1]<<8)|((uint32_t)at[2]<<16)|((uint32_t)at[3]<<24);
}
static void card_make(bool exfat,bool partitioned,bool with_file) {
    static FATFS fs;static BYTE work[65536];
    FIL file;UINT done;
    memset(disk,0,(size_t)BLOCKS*512u);
    MKFS_PARM p={.fmt=(BYTE)((exfat?FM_EXFAT:FM_FAT32)|(partitioned?0:FM_SFD)),.n_fat=2,.align=1,.n_root=512,
        .au_size=exfat?4096u:512u};
    assert(f_mkfs("0:",&p,work,sizeof(work))==FR_OK);
    part_first=0u;part_end=BLOCKS;
    if(partitioned) {
        /* The first entry of the partition table: its first sector and its length. */
        part_first=le32(disk+446u+8u);part_end=part_first+le32(disk+446u+12u);
        assert(disk[510]==0x55u && disk[511]==0xaau && part_first && part_end>part_first && part_end<=BLOCKS);
    }
    assert(f_mount(&fs,"0:",1)==FR_OK);
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

enum {RAW_DATA=4u,COOKED_DATA=4u|KUI_RETAIL_TRACK_COOKED,MODE2_DATA=4u|KUI_RETAIL_TRACK_MODE2,
      SHORT_DATA=4u|KUI_RETAIL_TRACK_2336,LONG_DATA=4u|KUI_RETAIL_TRACK_2448,AUDIO=0u};
/* The game's image: extents side by side near the card's end, well away
 * from the file. */
#define EXTENTS_AT (BLOCKS-4096u)
static void extents(unsigned count,uint32_t blocks) {
    manifest.extent_count=count;
    for(unsigned j=0u;j<count;j++)
        manifest.slots[manifest.track_count+j].extent=(struct kui_retail_extent){j*blocks,EXTENTS_AT+j*blocks,blocks};
}
/* A launch: the disc's map, the card's state, then the stage's scan. */
static void launch(uint32_t reader,uint32_t transport,const uint8_t *controls,unsigned tracks) {
    memset(&manifest,0,sizeof(manifest));
    manifest.card_sectors=BLOCKS;manifest.partition_start=part_first;manifest.partition_end=part_end;
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
    extents(3u,1024u);
    mode=KUI_VMU_NATIVE_MODE_READER_ONLY;
    memset(&card,0,sizeof(card));
    card.transport=transport;card.device.sd.blocks=BLOCKS;card.device.sd.ready=true;
    card.device.sd.high_capacity=true;card.device.sd.bus.transfer_block=block_path;
    toy_suite_source=0u;lines=0u;memset(shown,0,sizeof(shown));
}
/* A fuller map, put over a launch's: text that fills its fields and has no
 * terminator, tracks of every kind, CD music from the session's first sector
 * on, seven extents. Every count of its launch record is another number. */
static void rich_map(void) {
    static const struct {uint32_t start,end;uint8_t control,files;} tracks[12]={
        {150u,1150u,AUDIO,1u},          /* below the session */
        {1150u,45149u,RAW_DATA,1u},
        {45149u,45150u,AUDIO,1u},       /* the last sector below the session */
        {45150u,46150u,AUDIO,2u},       /* the session's first sector: CD music */
        {46150u,47150u,MODE2_DATA,1u},
        {47150u,48150u,COOKED_DATA,1u},
        {48150u,49150u,AUDIO,0u},       /* CD music the map holds no file of */
        {49150u,50150u,SHORT_DATA,1u},
        {50150u,51150u,AUDIO,1u},
        {51150u,52150u,COOKED_DATA,1u},
        {52150u,53150u,LONG_DATA,1u},
        {53150u,54150u,AUDIO,3u},
    };
    for(unsigned i=0u;i<sizeof(manifest.title);i++) manifest.title[i]=(char)('A'+i%26u);
    memcpy(manifest.product,"T-1234567890123N",16u);
    memcpy(manifest.bootfile,"ABCDEFGHIJKLMNOPQRSTUVWX",24u);
    memcpy(manifest.region,"JUEJUEJUEJUEJUEJ",16u);
    manifest.flags=0x7u;manifest.ip_crc32=0xfedcba98u;manifest.gdi_crc32=1u;manifest.boot_crc32=0x80000000u;
    manifest.boot_bytes=0x00ffffffu;manifest.boot_lba=47150u;
    boot_crc=0u;exec_bytes=1u;
    manifest.track_count=12u;
    for(unsigned i=0u;i<12u;i++)
        manifest.slots[i].track=(struct kui_retail_track){tracks[i].start,tracks[i].end,0u,tracks[i].files,
            tracks[i].control};
    extents(7u,512u);
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
enum {MAP_RAW,MAP_COOKED,MAP_RICH};
static void map_launch(unsigned map) {
    if(map==MAP_COOKED) launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,cooked,3u);
    else launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);
    if(map==MAP_RICH) rich_map();
}
/* The whole sector of a slot's launch record, for each map. */
static void record_of(uint32_t slot,unsigned map,uint8_t out[512]) {
    static const char *const body[3]={
        "PRODUCT T-12345N\nTITLE EXAMPLE GAME\nBOOT 1ST_READ.BIN REGION JUE\n"
        "IPCRC 12345678 GDICRC 0D0D0D0D BOOTCRC 0B0B0B0B BOOTBYTES 0012D687\nEXECCRC 9ABCDEF0 EXECBYTES 003DE7E0\n"
        "READER 1 TRANSPORT 1 FLAGS 00000004 SOURCE 1\n"
        "TRACKS 5 RAW 1 COOKED 0 AUDIO 4 HDAUDIO 2 MAPPED 1 OTHER 0 EXTENTS 3\nSESSION 0000B05E BOOTLBA 0000B090\n",
        "PRODUCT T-12345N\nTITLE EXAMPLE GAME\nBOOT 1ST_READ.BIN REGION JUE\n"
        "IPCRC 12345678 GDICRC 0D0D0D0D BOOTCRC 0B0B0B0B BOOTBYTES 0012D687\nEXECCRC 9ABCDEF0 EXECBYTES 003DE7E0\n"
        "READER 1 TRANSPORT 1 FLAGS 00000004 SOURCE 0\n"
        "TRACKS 3 RAW 0 COOKED 1 AUDIO 2 HDAUDIO 0 MAPPED 0 OTHER 0 EXTENTS 3\nSESSION 0000B05E BOOTLBA 0000B090\n",
        /* The title is cut at 56 characters, the boot file's name at 16, the region at 8. */
        "PRODUCT T-1234567890123N\nTITLE ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCD\n"
        "BOOT ABCDEFGHIJKLMNOP REGION JUEJUEJU\n"
        "IPCRC FEDCBA98 GDICRC 00000001 BOOTCRC 80000000 BOOTBYTES 00FFFFFF\nEXECCRC 00000000 EXECBYTES 00000001\n"
        "READER 1 TRANSPORT 1 FLAGS 00000007 SOURCE 1\n"
        "TRACKS 12 RAW 1 COOKED 2 AUDIO 6 HDAUDIO 4 MAPPED 5 OTHER 3 EXTENTS 7\nSESSION 0000B05E BOOTLBA 0000B82E\n"};
    char text[513];
    int n=snprintf(text,sizeof(text),"==== SLOT %03u PART 0 ==== ID ",(unsigned)slot);
    for(unsigned i=0u;i<16u;i++) n+=snprintf(text+n,sizeof(text)-(size_t)n,"%02X",survey.id[i]);
    n+=snprintf(text+n,sizeof(text)-(size_t)n," ==== LAUNCH ====\nSURVEY 83 BUILD " KUI_BUILD_ID "\n%s",body[map]);
    assert(n>0 && n<512);
    memset(out,' ',511u);memcpy(out,text,(size_t)n);out[511]='\n';
}
static void good(uint32_t slot,unsigned map) {
    char line[80];
    struct kui_toy_report_wire out;
    memset(&io,0,sizeof(io));
    map_launch(map);
    uint32_t differ=changed();
    scan();
    snprintf(line,sizeof(line),"SURVEY ON / REPORT SAVES TO SLOT %03u",(unsigned)slot);
    if(strcmp(shown[0],line)) {fprintf(stderr,"FAIL shown \"%s\", not \"%s\"\n",shown[0],line);exit(1);}
    assert(lines==1u && toy_suite_source==(map!=MAP_COOKED));
    /* One sector written: the slot's part 0, the launch record of this game
     * and nothing else in it. */
    assert(io.writes==1u && io.written[0]==file_lba[kui_toy_survey_sector(slot,0u)] && changed()==differ+1u);
    assert(io.identities==1u && !io.syncs && toy_survey_wrote);
    const uint8_t *record=disk+(size_t)io.written[0]*512u;
    uint8_t expect[512];
    record_of(slot,map,expect);
    if(memcmp(record,expect,512u)) {
        fprintf(stderr,"FAIL the launch record of slot %u:\n%.512s\nnot\n%.512s\n",(unsigned)slot,(const char *)record,
            (const char *)expect);
        exit(1);
    }
    assert(launch_record(&survey,slot,record));
    /* The worker's seed: the slot's other two sectors, still blank, on this card. */
    memset(&out,0,sizeof(out));
    toy_suite_report_prepare(&out);
    assert(!memcmp(&out,&toy_survey_seed,sizeof(out)) && out.test==66u && out.bytes==64u && !out.reserved);
    assert(out.magic[0]==KUI_TOY_REPORT_MAGIC0 && out.magic[1]==KUI_TOY_REPORT_MAGIC1 && out.version==1u);
    assert(kui_retail_crc32(0u,&out,KUI_TOY_REPORT_CRC_OFFSET)==out.crc32);
    assert(!memcmp(out.cid,card_cid,16u) && out.high_capacity==1u);
    for(unsigned i=0u;i<2u;i++) {
        assert(out.lba[i]==file_lba[kui_toy_survey_sector(slot,1u+i)] && out.lba[i]>=part_first && out.lba[i]<part_end);
        assert(kui_retail_crc32(0u,disk+(size_t)out.lba[i]*512u,512u)==out.placeholder_crc[i]);
        assert(!memcmp(disk+(size_t)out.lba[i]*512u,blank+512u*kui_toy_survey_sector(slot,1u+i),512u));
    }
}

/* The largest blank file there is, written in pieces of eight sectors with
 * a spacer file's between them: more runs than the lookup maps. */
static void fragmented_file(void) {
    static FATFS fs;static uint8_t sector[512],fill[4096];
    FIL file,spacer;UINT done;
    struct kui_toy_survey_file largest=survey;
    largest.slots=KUI_TOY_SURVEY_SLOTS_MAX;
    uint32_t pieces=0u;
    memset(fill,0x77,sizeof(fill));
    assert(f_mount(&fs,"0:",1)==FR_OK && f_open(&file,PATH,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    assert(f_open(&spacer,"0:/SPACER.BIN",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    for(uint32_t at=0u;at<1u+3u*largest.slots;at++) {
        if(!at) kui_toy_survey_file_sector(&largest,sector);
        else kui_toy_survey_blank_sector(&largest,(at-1u)/3u,(at-1u)%3u,sector);
        assert(f_write(&file,sector,512u,&done)==FR_OK && done==512u);
        if(at%8u==7u) {
            assert(f_sync(&file)==FR_OK && f_write(&spacer,fill,sizeof(fill),&done)==FR_OK && done==sizeof(fill));
            assert(f_sync(&spacer)==FR_OK);
            ++pieces;
        }
    }
    assert(pieces>KUI_WCE_PROBE_FILE_RUNS);
    assert(f_close(&file)==FR_OK && f_close(&spacer)==FR_OK && f_unlink("0:/SPACER.BIN")==FR_OK);
    assert(f_mount(NULL,"0:",0)==FR_OK);
}
/* A launch with the file as it stands and the plain raw map. */
static void plain(void) {launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCI,raw,5u);}
static void place_line(char line[80],uint32_t slot,uint32_t part) {
    snprintf(line,80u,"SURVEY OFF / FILE PLACE %08X",(unsigned)file_lba[kui_toy_survey_sector(slot,part)]);
}

static void standing(bool exfat,bool partitioned,const char *leave) {
    char line[80];
    card_make(exfat,partitioned,true);
    /* A source for the window, then a disc that is none: both get a slot. */
    good(0u,MAP_RAW);
    good(1u,MAP_COOKED);
    /* Launches the survey is off for write nothing, whatever the file holds. */
    memcpy(before,disk,(size_t)BLOCKS*512u);
    memset(&io,0,sizeof(io));
    launch(KUI_RETAIL_READER_ASYNC_EAGER,KUI_STORAGE_SCI,raw,5u);
    off("SURVEY OFF / NOT THE NEW (X) READER",0u);assert(!io.acquires);
    launch(KUI_RETAIL_READER_ASYNC,KUI_STORAGE_SCIF,raw,5u);
    off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
    plain();card.transport=KUI_STORAGE_SCIF;
    off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
    plain();manifest.storage_transport=KUI_STORAGE_IDE;
    off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
    /* The virtual VMU on: the module's stage stops such a launch before
     * this; were it to come here, no survey. */
    plain();mode=KUI_VMU_NATIVE_MODE_NORMAL;
    off("SURVEY OFF / NOT THE NEW (X) READER",0u);assert(!io.acquires);
    plain();manifest.partition_end=UINT64_C(0x100000000);
    off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
    plain();manifest.partition_end=manifest.partition_start;
    off("SURVEY OFF / NOT THE SCI CARD",0u);assert(!io.acquires);
    /* A slot's sector in an extent of the game's (the first, the middle or
     * the last of them; the last of the fuller map's seven), past the map's
     * card, or past the card itself. */
    for(unsigned part=0u;part<3u;part++) {
        plain();
        manifest.slots[5u+part].extent=(struct kui_retail_extent){0u,
            file_lba[kui_toy_survey_sector(2u,part)]-(part?0u:3u),part?1u:4u};
        place_line(line,2u,part);off(line,0u);
    }
    plain();rich_map();
    manifest.slots[12u+6u].extent=(struct kui_retail_extent){0u,file_lba[kui_toy_survey_sector(2u,1u)],1u};
    place_line(line,2u,1u);off(line,0u);
    plain();manifest.card_sectors=file_lba[kui_toy_survey_sector(2u,1u)];
    place_line(line,2u,1u);off(line,0u);
    plain();card.device.sd.blocks=file_lba[kui_toy_survey_sector(2u,2u)];
    place_line(line,2u,2u);off(line,0u);
    /* The map's partition ends before the slot: the volume does not fit
     * it, and nothing behind its end is read. */
    plain();manifest.partition_end=file_lba[kui_toy_survey_sector(2u,1u)];
    off("SURVEY OFF / FILE LOOKUP 00000002",0u);
    if(partitioned) {
        /* A map that names the whole card: its first sector is the partition
         * table, and no volume. */
        plain();manifest.partition_start=0u;
        off("SURVEY OFF / FILE LOOKUP 00000002",0u);
    }
    /* The write's own guard, asked directly. A sector of the game's (the
     * first extent's, the last one's first and last), one past the card, the
     * card's first, the one before the partition: the card is not asked
     * for. The sector behind the last extent is no one's: the guard lets it
     * through, so it is the extents that refuse the others. */
    memset(&io,0,sizeof(io));
    plain();
    assert(toy_survey_write(EXTENTS_AT+7u,blank)==-1 && toy_survey_write(EXTENTS_AT+2048u,blank)==-1 &&
           toy_survey_write(EXTENTS_AT+3071u,blank)==-1 && toy_survey_write(BLOCKS,blank)==-1 &&
           toy_survey_write(0u,blank)==-1 && !io.acquires && !io.writes);
    if(part_first) assert(toy_survey_write(part_first-1u,blank)==-1 && !io.acquires);
    assert(!toy_survey_write(EXTENTS_AT+3072u,blank) && io.acquires==1u && io.writes==1u &&
           io.written[0]==EXTENTS_AT+3072u && changed()==1u);
    memcpy(disk,before,(size_t)BLOCKS*512u);
    /* A partition that begins later, or ends sooner, than the sector. */
    manifest.partition_start=2048u;
    assert(toy_survey_write(2047u,blank)==-1 && io.acquires==1u);
    manifest.partition_end=EXTENTS_AT-8u;
    assert(toy_survey_write(EXTENTS_AT-8u,blank)==-1 && toy_survey_write(EXTENTS_AT+3072u,blank)==-1 &&
           io.acquires==1u && io.writes==1u);
    /* The card does not answer: at any acquisition, at any read. */
    memset(&io,0,sizeof(io));
    plain();io.no_identity=true;
    off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);
    /* The identity command left the card not ready; it answered its status, and the launch goes on. */
    assert(io.syncs==1u);
    unsigned acquires=io.acquires-1u,reads=io.reads;
    assert(acquires>3u && reads>3u && io.stops==reads);
    /* ... at the second asking; or not at all, and then the launch stops as it must. */
    memset(&io,0,sizeof(io));io.no_identity=true;io.sync_failures=1u;
    plain();
    off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==2u);
    memset(&io,0,sizeof(io));io.no_identity=true;io.sync_failures=2u;dead=true;
    plain();
    off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==2u);
    /* It answers, and the port is not well: no. */
    for(unsigned which=0u;which<2u;which++) {
        memset(&io,0,sizeof(io));io.no_identity=true;io.unhealthy=!which;io.faulted=which;dead=true;
        plain();
        off("SURVEY OFF / CARD IDENTITY FFFFFFFB",0u);assert(io.syncs==1u);
    }
    /* A card that is not ready when the survey comes to it is not asked for anything. */
    memset(&io,0,sizeof(io));dead=true;
    plain();card.device.sd.ready=false;
    off("SURVEY OFF / CARD NOT READY",0u);assert(!io.acquires && !io.syncs);
    assert(acquires==reads+1u); /* one acquisition a read, and the identity's */
    for(unsigned at=1u;at<=acquires;at++) {
        memset(&io,0,sizeof(io));io.fail_acquire_at=at;
        plain();
        scan();
        if(at+5u>reads && at<=reads) {
            /* The card not to be had for a slot's sector: that slot is passed over, as below. */
            snprintf(line,sizeof(line),"SURVEY ON / REPORT SAVES TO SLOT %03u",at+3u>reads?3u:2u);
            assert(lines==1u && !strcmp(shown[0],line) && toy_survey_seed.test==66u && io.writes==1u && changed()==1u);
            memcpy(disk,before,(size_t)BLOCKS*512u);
        } else
        assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / ",13u) && !toy_survey_seed.test && !io.writes);
    }
    for(unsigned at=1u;at<=reads;at++) {
        memset(&io,0,sizeof(io));io.fail_read_at=at;
        plain();
        scan();
        if(at+5u>reads) {
            /* A sector of a slot that the card does not give, the card well: the slot is passed over,
             * used (slots 0 and 1) or blank (slot 2), and the launch has the next blank one. */
            snprintf(line,sizeof(line),"SURVEY ON / REPORT SAVES TO SLOT %03u",at+3u>reads?3u:2u);
            assert(lines==1u && !strcmp(shown[0],line) && toy_survey_seed.test==66u && io.writes==1u &&
                   io.written[0]==file_lba[kui_toy_survey_sector(at+3u>reads?3u:2u,0u)] && changed()==1u);
            memcpy(disk,before,(size_t)BLOCKS*512u);
        } else
        assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / FILE ",18u) && !toy_survey_seed.test && !io.writes);
        /* The same read leaving the card not ready, its stream not ended: the card is
         * asked nothing, and the launch stops as it does for a read of the stage's own. */
        memset(&io,0,sizeof(io));io.fail_read_at=at;io.read_kills=true;dead=true;
        plain();
        scan();
        assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / FILE ",18u) && !toy_survey_seed.test && !io.writes &&
               !io.syncs && !io.identities);
        /* The read good and its stop not taken: the sector does not count as
         * read, the card is asked nothing more, and the launch stops. */
        memset(&io,0,sizeof(io));io.fail_stop_at=at;dead=true;
        plain();
        scan();
        assert(lines==1u && !strncmp(shown[0],"SURVEY OFF / FILE ",18u) && !toy_survey_seed.test && !io.writes &&
               !io.syncs && !io.identities && io.reads==at);
    }
    assert(!changed());
    /* The write is refused by the card: it answers its status afterwards, and the launch goes on with it. */
    memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TOKEN;
    plain();
    off("SURVEY OFF / CARD WRITE 0000000D",1u);
    assert(io.syncs==1u && !changed());
    /* ... or answers at the second asking only, or not at all. */
    memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TIMEOUT;io.sync_failures=1u;
    plain();
    off("SURVEY OFF / CARD WRITE 0000000D",1u);
    assert(io.syncs==2u && !changed());
    memset(&io,0,sizeof(io));io.write_result=KUI_LOADER_SD_TIMEOUT;io.sync_failures=2u;
    plain();
    dead=true;
    off("SURVEY OFF / CARD WRITE 0000000D",1u);
    assert(io.syncs==2u && !changed());
    /* The port is left unwell by a write the card took: no seed. */
    for(unsigned which=0u;which<2u;which++) {
        memset(&io,0,sizeof(io));io.unhealthy=!which;io.faulted=which;io.drop_write=true;
        plain();
        off("SURVEY OFF / CARD WRITE 00000000",1u);
    }
    /* A write the card took and did not keep. */
    memset(&io,0,sizeof(io));io.drop_write=true;
    plain();
    off("SURVEY OFF / WRITE NOT READ BACK",1u);
    assert(io.written[0]==file_lba[kui_toy_survey_sector(2u,0u)] && !changed());
    /* The card is not to be had for the write: nothing written. */
    memset(&io,0,sizeof(io));io.fail_acquire_at=acquires+1u;
    plain();
    off("SURVEY OFF / CARD WRITE 00000000",0u);
    assert(!changed());
    /* The record is written and cannot be read back (the card not to be
     * had, the read failing for good, or the read good and its stop not
     * taken): the slot is spent, with its launch record and no seed, and the
     * next launch takes the next one. The card is asked nothing after a
     * read that was not ended. */
    for(unsigned how=0u;how<3u;how++) {
        memset(&io,0,sizeof(io));
        if(how==0u) io.fail_acquire_at=acquires+2u;
        else if(how==1u) {io.fail_read_at=reads+1u;io.read_kills=true;dead=true;}
        else {io.fail_stop_at=reads+1u;dead=true;}
        plain();
        off("SURVEY OFF / WRITE NOT READ BACK",1u);
        assert(!io.syncs && io.identities==1u);
        uint8_t expect[512];
        record_of(2u,MAP_RAW,expect);
        assert(changed()==1u && io.written[0]==file_lba[kui_toy_survey_sector(2u,0u)] &&
               !memcmp(disk+(size_t)io.written[0]*512u,expect,512u));
        if(how<2u) memcpy(disk,before,(size_t)BLOCKS*512u);
    }
    memcpy(before,disk,(size_t)BLOCKS*512u);
    /* The slots that are left, with a map of each kind in turn (slot 3 the
     * fuller one); then the file is full. */
    for(uint32_t slot=3u;slot<SLOTS;slot++) good(slot,slot%3u==0u?MAP_RICH:slot%3u==1u?MAP_RAW:MAP_COOKED);
    memcpy(before,disk,(size_t)BLOCKS*512u);
    memset(&io,0,sizeof(io));
    plain();
    off("SURVEY OFF / FILE FULL - COPY A BLANK ONE",0u);
    assert(!changed());
    if(leave) {
        /* The file as the stage left it, read by FatFs, for the tool to read. */
        static FATFS fs;static uint8_t back[FILE_BYTES];
        FIL file;UINT done;
        assert(f_mount(&fs,"0:",1)==FR_OK && f_open(&file,PATH,FA_READ)==FR_OK && f_size(&file)==FILE_BYTES);
        assert(f_read(&file,back,FILE_BYTES,&done)==FR_OK && done==FILE_BYTES && f_close(&file)==FR_OK);
        assert(f_mount(NULL,"0:",0)==FR_OK);
        FILE *out=fopen(leave,"wb");
        assert(out && fwrite(back,1u,FILE_BYTES,out)==FILE_BYTES && !fclose(out));
    }
    /* No file: off, and not a write. */
    card_make(exfat,partitioned,false);
    memset(&io,0,sizeof(io));
    plain();
    off("SURVEY OFF / NO KUI/SURVEY/STREAM.TXT",0u);
    assert(!changed());
    /* A file in more pieces than the lookup maps is refused whole, though
     * its first slots are blank and lie in the runs the lookup has. */
    fragmented_file();
    memcpy(before,disk,(size_t)BLOCKS*512u);
    memset(&io,0,sizeof(io));
    plain();
    off("SURVEY OFF / FILE LOOKUP 00000005",0u);
    assert(io.reads && !changed());
    printf("PASS the stage's survey text on %s, volume at sector %u: a slot for a source and for a disc that is "
           "none; one sector written per launch, read back; every refusal leaves the card as it was\n",
           exfat?"exFAT":"FAT32",(unsigned)part_first);
}

/* The seed Card Tools patches into a Toy test's module, for the cards made
 * here: two free sectors behind the extents of every map. */
#define TOOLS_SEED_AT (EXTENTS_AT+7u*512u+64u)
static struct kui_toy_report_wire tools_seed(void) {
    struct kui_toy_report_wire seed={.magic={KUI_TOY_REPORT_MAGIC0,KUI_TOY_REPORT_MAGIC1},
        .version=KUI_TOY_REPORT_VERSION,.bytes=sizeof(seed),.test=66u,.lba={TOOLS_SEED_AT,TOOLS_SEED_AT+1u},
        .placeholder_crc={0x11112222u,0x33334444u},.high_capacity=1u};
    memcpy(seed.cid,card_cid,16u);
    seed.crc32=kui_retail_crc32(0u,&seed,KUI_TOY_REPORT_CRC_OFFSET);
    return seed;
}
/* This module started from Card Tools, its seed patched: without the survey's
 * file it saves its report as Test 81 does, by Test 81's rules; with the
 * file, the survey's slot is the one that is used. */
static void card_tools(bool exfat,bool partitioned) {
    struct kui_toy_report_wire out;
    const struct kui_toy_report_wire wanted=tools_seed();
    card_make(exfat,partitioned,false);
    for(unsigned kind=0u;kind<5u;kind++) {
        memset(&io,0,sizeof(io));
        if(kind==1u) map_launch(MAP_COOKED);          /* a disc that is no source */
        else plain();
        if(kind==2u) manifest.slots[5u+2u].extent=(struct kui_retail_extent){0u,TOOLS_SEED_AT+1u,1u}; /* the game's sector */
        if(kind==3u) manifest.card_sectors=TOOLS_SEED_AT+1u;                                    /* past the map's card */
        if(kind==4u) rich_map();
        scan();
        assert(lines==1u && !strcmp(shown[0],"SURVEY OFF / NO KUI/SURVEY/STREAM.TXT") && !toy_survey_seed.test);
        memset(&out,0,sizeof(out));
        toy_suite_report_prepare(&out);
        if(kind==0u || kind==4u) assert(!memcmp(&out,&wanted,sizeof(out)));
        else assert(!out.test && !out.lba[0] && !out.lba[1]);
        assert(!io.writes && !changed());
    }
    /* With the file: the survey's slot, for a source and for a disc that is none. */
    card_make(exfat,partitioned,true);
    for(unsigned slot=0u;slot<2u;slot++) {
        memset(&io,0,sizeof(io));
        map_launch(slot?MAP_COOKED:MAP_RAW);
        scan();
        assert(lines==1u && !strncmp(shown[0],"SURVEY ON / REPORT SAVES TO SLOT 00",35u) && toy_survey_seed.test==66u);
        memset(&out,0,sizeof(out));
        toy_suite_report_prepare(&out);
        assert(!memcmp(&out,&toy_survey_seed,sizeof(out)) && memcmp(&out,&wanted,sizeof(out)) &&
               out.lba[0]==file_lba[kui_toy_survey_sector(slot,1u)] && out.lba[1]==file_lba[kui_toy_survey_sector(slot,2u)]);
    }
    printf("PASS started from Card Tools on %s, volume at sector %u: Test 81's seed without the survey's file, "
           "the survey's slot with it\n",exfat?"exFAT":"FAT32",(unsigned)part_first);
}

/* test-toy-survey-stage [DIRECTORY]   the standing cases; the last card's file is left in DIRECTORY
 * test-toy-survey-stage --seed        the report seed as this program holds it, then the one Card Tools
 *                                     would patch in (tests/test_toy_survey.py patches a copy of the program)
 * test-toy-survey-stage --card-tools  the cases of a module whose seed is patched */
int main(int argc,char **argv) {
    const struct kui_toy_report_wire wanted=tools_seed();
    if(argc>1 && !strcmp(argv[1],"--seed"))
        return fwrite(&toy_report_seed,1u,sizeof(toy_report_seed),stdout)==sizeof(toy_report_seed) &&
               fwrite(&wanted,1u,sizeof(wanted),stdout)==sizeof(wanted)?0:2;
    disk=calloc(BLOCKS,512u);before=calloc(BLOCKS,512u);
    if(!disk || !before) return 2;
    if(argc>1 && !strcmp(argv[1],"--card-tools")) {
        assert(!memcmp(&toy_report_seed,&wanted,sizeof(wanted)) && "this program's seed is not patched");
        for(unsigned partitioned=0u;partitioned<2u;partitioned++)
            for(unsigned exfat=0u;exfat<2u;exfat++) card_tools(exfat,partitioned);
        printf("PASS the stage's survey text, started from Card Tools\n");
        free(disk);free(before);
        return 0;
    }
    /* From the Games menu the seed is as built: blank. */
    assert(!toy_report_seed.test && !toy_report_seed.lba[0] && !toy_report_seed.lba[1]);
    /* The launch line's slot number: three digits. */
    static const struct {uint32_t slot;const char *text;} numbers[]={{7u,"SURVEY ON / REPORT SAVES TO SLOT 007"},
        {10u,"SURVEY ON / REPORT SAVES TO SLOT 010"},{99u,"SURVEY ON / REPORT SAVES TO SLOT 099"},
        {100u,"SURVEY ON / REPORT SAVES TO SLOT 100"},{998u,"SURVEY ON / REPORT SAVES TO SLOT 998"}};
    for(unsigned i=0u;i<sizeof(numbers)/sizeof(numbers[0]);i++) {
        lines=0u;memset(shown,0,sizeof(shown));
        toy_survey_line("SURVEY ON / REPORT SAVES TO SLOT",numbers[i].slot);
        assert(lines==1u && !strcmp(shown[0],numbers[i].text));
    }
    char leave[1024]="";
    if(argc>1) snprintf(leave,sizeof(leave),"%s/stage-used.txt",argv[1]);
    for(unsigned partitioned=0u;partitioned<2u;partitioned++)
        for(unsigned exfat=0u;exfat<2u;exfat++)
            standing(exfat,partitioned,argc>1 && partitioned && exfat?leave:NULL);
    /* No card that is a volume at all. */
    part_first=0u;part_end=BLOCKS;
    memset(disk,0,(size_t)BLOCKS*512u);memcpy(before,disk,(size_t)BLOCKS*512u);
    memset(&io,0,sizeof(io));
    plain();
    off("SURVEY OFF / FILE LOOKUP 00000002",0u);
    assert(!changed());
    printf("PASS the stage's survey text\n");
    free(disk);free(before);
    return 0;
}
