/* SPDX-License-Identifier: GPL-3.0-only */
/* End to end, on the host, through the probe build's own code:
 *  1. a FAT32 or exFAT RAM disk holding a blank KUI/WCEDATA/WCEDATA.BIN made
 *     by tools/wce_probe_data.py (optionally fragmented);
 *  2. the stage's lookup and claim (src/core/wce_probe_fat.c) and its writes
 *     (stage report, CLAIMED header), as the stage makes them;
 *  3. the reader (src/loader/wce_probe_reader.inc, compiled here with its
 *     hardware hooks mapped onto a host model of RAM and registers): GD calls,
 *     Maple sampling, and the end-of-game writer;
 *  4. the file is copied back out for tools/wce_probe_data.py to decode
 *     (tests/test_wce_probe_end_to_end.py checks the decode and the dump).
 * Every card write must fall inside the claimed slot.
 * usage: test-wce-probe-end-to-end exfat|fat32 FRAGMENT BLANK OUT_DATA OUT_RAM OUT_JSON */
#define _POSIX_C_SOURCE 200809L
#include "ff.h"
#include "diskio.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the reader's environment ---- */
#define KUI_RETAIL_CE 1
#define KUI_RETAIL_ASYNC 1
#define KUI_WCE_PROBE 1
#define KUI_WCE_PROBE_HOST_TEST 1
#include "kui/wce_probe.h"
#include "kui/gd_service.h"
#include "sd_reader.h"
enum {KUI_RETAIL_GD_PLAY=20,KUI_RETAIL_GD_PLAY2=21,KUI_RETAIL_GD_PAUSE=22,KUI_RETAIL_GD_RELEASE=23};
#define KUI_RETAIL_GD_DMAREAD_STREAM 38u
#define KUI_RETAIL_GD_PIOREAD_STREAM 39u
#define KUI_RETAIL_ASYNC_SPILL_BYTES 2048u
enum kui_sci_stream_result {KUI_SCI_STREAM_OK=0};
#include "retail_display.h"
#define BLOCKS_FOR_MAP (128u*1024u*1024u/512u)
static struct {
    uint8_t area0[544] __attribute__((aligned(32)));
    uint8_t area1[544] __attribute__((aligned(32)));
    uint8_t spill[KUI_RETAIL_ASYNC_SPILL_BYTES];
    struct {
        struct {uint32_t count,outputs[4];} service;
        struct {struct {struct kui_loader_sd sd;} device;} card;
    } shared;
} R;
struct host_manifest {uint64_t card_sectors,partition_start,partition_end;};
static const struct host_manifest host_map={BLOCKS_FOR_MAP,0u,BLOCKS_FOR_MAP};
static struct {uint32_t opened;const struct host_manifest *manifest;} e;
volatile uint32_t kui_retail_hook_fault;

#define BLOCKS (128u*1024u*1024u/512u)
static uint8_t *disk;
static uint8_t *ram;           /* the console's 16 MiB, physical 0x0c000000.. */
static uint32_t mdstar,rtc_seconds=0x12345678u;
static jmp_buf rebooted;
static uint16_t colors[8];static unsigned color_count;
static uint32_t slot_first,slot_last;  /* the claimed slot's card sectors */
static unsigned writes,stream_stops;
static bool fail_writes,interrupt_claim;
static unsigned fail_after;    /* fail every write after this many (0: never) */
static unsigned fail_once;     /* fail only the write after this many (0: never) */
static unsigned write_attempts;

static void fail(const char *what) {fprintf(stderr,"FAIL %s\n",what);exit(1);}
static void check(bool ok,const char *what) {if(!ok) fail(what);}

static uint32_t host_reg(uint32_t address) {
    switch(address) {
    case 0xa05f6c04u: return mdstar;
    case 0xa0710000u: return rtc_seconds>>16;
    case 0xa0710004u: return rtc_seconds&0xffffu;
    case 0xa05f80d0u: return 0x00000150u;   /* a recognisable SPG_CONTROL */
    default: return 0u;                     /* G2 idle, everything else zero */
    }
}
static const void *host_ram(uint32_t address) {
    uint32_t physical=address&0x1fffffffu;
    if(physical<0x0c000000u || physical>=0x0d000000u) fail("RAM access outside main RAM");
    return ram+(physical-0x0c000000u);
}
#define WCE_PROBE_REG(address) host_reg(address)
#define WCE_PROBE_RAM(address) host_ram(address)
#define WCE_PROBE_REBOOT() longjmp(rebooted,1)
static unsigned cache_purges;
#define WCE_PROBE_CACHE_PURGE(ccr) ((void)(ccr),++cache_purges)

enum kui_sci_stream_result kui_sci_stream_stop(void) {++stream_stops;return KUI_SCI_STREAM_OK;}
void kui_sci_stream_discard(void) {}
void retail_display_color(const struct retail_display_state *s,uint16_t color) {
    (void)s;if(color_count<8u) colors[color_count++]=color;
}
void retail_display_pause(uint32_t frames) {(void)frames;}
enum kui_loader_sd_result kui_sci_sd_write(struct kui_loader_sd *card,uint32_t lba,uint32_t count,const uint8_t *data) {
    check(card && card->ready && card->bus.transfer_block==NULL,"reader writes through the byte path");
    ++write_attempts;
    if(fail_writes || (fail_after && writes>=fail_after)) return KUI_LOADER_SD_TIMEOUT;
    if(fail_once && writes==fail_once) {fail_once=0u;return KUI_LOADER_SD_CRC;}
    if(lba<slot_first || lba>=slot_last || count>slot_last-lba) fail("a reader write left the claimed slot");
    memcpy(disk+(size_t)lba*512u,data,(size_t)count*512u);
    ++writes;return KUI_LOADER_SD_OK;
}
#include "../src/loader/wce_probe_reader.inc"

/* ---- FatFs RAM disk ---- */
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
DWORD get_fattime(void) {return ((2026u-1980u)<<25)|(11u<<21)|(17u<<16);}
static int card_read(void *context,uint32_t lba,uint8_t sector[512]) {
    (void)context;if(lba>=BLOCKS) return -1;
    memcpy(sector,disk+(size_t)lba*512u,512u);return 0;
}
static void stage_write(uint32_t lba,uint32_t count,const void *data) {
    if(lba<slot_first || lba>=slot_last || count>slot_last-lba) fail("a stage write left the claimed slot");
    memcpy(disk+(size_t)lba*512u,data,(size_t)count*512u);
}

/* The stage's part, as wce_probe_stage.inc does it, without the console. */
static void stage(struct kui_wce_probe_seed *seed,uint32_t expected_slot,const char *product) {
    static uint8_t work[512];
    static struct kui_wce_probe_layout layout;
    struct kui_wce_probe_volume v={0u,BLOCKS,card_read,NULL};
    check(kui_wce_probe_fat_find(&v,&layout,work)==KUI_WCE_PROBE_FAT_OK,"lookup");
    struct kui_wce_probe_claim c;
    check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_OK,"claim");
    check(c.slot==expected_slot,"the first EMPTY slot is claimed");
    uint32_t base=c.file.first_slot+c.slot*c.file.slot_sectors;
    slot_first=kui_wce_probe_lba(&layout,base);
    slot_last=kui_wce_probe_lba(&layout,base+c.file.slot_sectors-1u)+1u;
    check(slot_last>slot_first,"slot bounds");
    /* A fragmented slot: the bound above covers its runs only if they rise. */
    for(uint32_t i=0;i<c.run_count;i++) check(c.runs[i].lba>=slot_first && c.runs[i].lba+c.runs[i].sectors<=slot_last,"run in slot");
    static union {struct kui_wce_probe_stage r;uint8_t b[KUI_WCE_PROBE_STAGE_SECTORS*512u];} report;
    memset(&report,0,sizeof(report));
    memcpy(report.r.magic,"KUIWCEST",8u);report.r.version=1u;report.r.bytes=sizeof(report.r);
    memcpy(report.r.build,"0000000000aa",12u);report.r.rtc=rtc_seconds;report.r.slot=c.slot;
    report.r.kernel_bytes=1253376u;report.r.kernel_crc32=0xa5a5a5a5u;report.r.track_count=3u;
    memcpy(report.r.ip,"SEGA SEGAKATANA SEGA ENTERPRISES",32u);
    memcpy(report.r.ip+64,product,strlen(product));
    memcpy(report.r.ip+128,"HOST TEST GAME",14u);
    /* The CLAIMED header first, then the stage report (wce_probe_stage.inc). */
    struct kui_wce_probe_slot claimed;
    kui_wce_probe_slot_header(&claimed,c.slot,KUI_WCE_PROBE_CLAIMED,c.used+1u,rtc_seconds,c.file.file_id);
    memset(work,0,512u);memcpy(work,&claimed,sizeof(claimed));
    stage_write(c.header_lba,1u,work);
    if(interrupt_claim) return; /* power off between the two writes */
    const uint8_t *from=report.b;
    for(uint32_t i=0;i<c.stage_count;i++) {
        stage_write(c.stage_runs[i].lba,c.stage_runs[i].sectors,from);from+=c.stage_runs[i].sectors*512u;
    }
    kui_wce_probe_seed_build(&c,rtc_seconds,1u,seed);
}

static void session(void) {
    /* A GD session: init, reads, CD audio, a refused tray command, and enough
     * calls for the Maple sampler. */
    kui_wce_probe_record(KUI_GD_INIT,0u,0);
    for(unsigned i=0;i<40u;i++) {
        R.shared.service.count=32u+i;
        kui_wce_probe_record(KUI_GD_REQUEST,KUI_GD_DMAREAD,(int32_t)(i+1u));
        kui_wce_probe_record(KUI_GD_CHECK,0u,2);
    }
    for(unsigned i=0;i<14u;i++) {
        R.shared.service.outputs[0]=2u+i;R.shared.service.outputs[1]=3u+i;R.shared.service.outputs[2]=15u;
        kui_wce_probe_record(KUI_GD_REQUEST,i%2u?KUI_RETAIL_GD_PAUSE:KUI_RETAIL_GD_PLAY,(int32_t)(100u+i));
    }
    kui_wce_probe_record(KUI_GD_REQUEST,26u,0); /* OPEN_TRAY, refused */
    for(unsigned i=0;i<8u;i++) kui_wce_probe_record(KUI_GD_DRIVE,0u,0);
}

static void write_file(const char *path,const void *data,size_t bytes) {
    FILE *f=fopen(path,"wb");
    if(!f || fwrite(data,1,bytes,f)!=bytes || fclose(f)) fail("host write");
}

int main(int argc,char **argv) {
    if(argc!=7) return 2;
    bool exfat=!strcmp(argv[1],"exfat");unsigned fragment=(unsigned)atoi(argv[2]);
    disk=calloc(BLOCKS,512u);ram=calloc(1u,0x01000000u);
    if(!disk || !ram) return 2;
    static BYTE work[65536];
    MKFS_PARM p={.fmt=(BYTE)((exfat?FM_EXFAT:FM_FAT32)|FM_SFD),.n_fat=2,.align=1,.n_root=512,.au_size=exfat?4096u:512u};
    check(f_mkfs("0:",&p,work,sizeof(work))==FR_OK,"mkfs");
    static FATFS fs;FIL out,spacer;UINT done;
    check(f_mount(&fs,"0:",1)==FR_OK,"mount");
    check(f_mkdir("0:/KUI")==FR_OK && f_mkdir("0:/KUI/WCEDATA")==FR_OK,"mkdir");
    FILE *blank=fopen(argv[3],"rb");if(!blank) fail("blank file");
    check(f_open(&out,"0:/KUI/WCEDATA/WCEDATA.BIN",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"open data");
    if(fragment) check(f_open(&spacer,"0:/SPACER.BIN",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"open spacer");
    static uint8_t chunk[1u<<20];size_t n;unsigned pieces=0;
    while((n=fread(chunk,1,sizeof(chunk),blank))>0) {
        check(f_write(&out,chunk,(UINT)n,&done)==FR_OK && done==n,"copy data");
        check(f_sync(&out)==FR_OK,"sync data");
        /* Spacers after the first few MiB only: a slot then spans a few
         * runs, within the reader's eight. */
        if(fragment && pieces++<fragment) {
            check(f_write(&spacer,chunk,4096u,&done)==FR_OK && done==4096u && f_sync(&spacer)==FR_OK,"spacer");
        }
    }
    fclose(blank);
    check(f_close(&out)==FR_OK,"close data");
    if(fragment) {check(f_close(&spacer)==FR_OK,"close spacer");check(f_unlink("0:/SPACER.BIN")==FR_OK,"unlink");}
    check(f_mount(NULL,"0:",0)==FR_OK,"unmount");

    /* RAM: non-zero pages with a pattern, zero pages in between (free RAM),
     * and a Maple DMA list at physical 0x0c100000. */
    for(uint32_t page=0;page<4096u;page++) {
        if(page%7u==3u || (page>=2048u && page<3000u)) continue;
        for(uint32_t i=0;i<1024u;i++) ((uint32_t *)(void *)ram)[page*1024u+i]=page*2654435761u^i;
    }
    uint32_t *list=(uint32_t *)(void *)(ram+0x100000u);
    memset(list,0,96u);
    list[0]=1u|0u<<16;list[1]=0x0c200000u;list[2]=9u|0x20u<<8|1u<<24;list[3]=0x01000000u;
    list[4]=2u|0u<<16;list[5]=0x0c200100u;list[6]=12u|0x01u<<8|2u<<24;list[7]=0x02000000u;list[8]=0;
    /* The VMU's screen: a block write to A1's LCD is not a save. */
    list[9]=2u|0u<<16;list[10]=0x0c200180u;list[11]=12u|0x01u<<8|2u<<24;list[12]=0x04000000u;list[13]=0;
    list[14]=0x80000000u|1u|1u<<16;list[15]=0x0c200200u;list[16]=10u|(0x40u|0x02u)<<8|1u<<24;list[17]=0x02000000u;
    mdstar=0x0c100000u;
    R.shared.card.device.sd.ready=true;R.shared.card.device.sd.blocks=BLOCKS;
    R.shared.card.device.sd.high_capacity=true;
    R.shared.card.device.sd.bus.transfer_block=(bool (*)(void *,const uint8_t *,uint8_t *,size_t,bool,uint16_t *))1;
    e.opened=1u;e.manifest=&host_map;

    /* Session 1: slot 0, A+B+X+Y+Start; one transient write error on the
     * way is retried. */
    fail_once=5u;
    stage(&kui_wce_probe_seed,0u,"T13004N");
    session();
    static const struct retail_display_state display;
    if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_MENU,1u,0x8c123456u,0x8c0fff00u);fail("no restart");}
    check(color_count==2u && colors[0]==PROBE_BLUE && colors[1]==PROBE_GREEN,"blue then green");
    check(stream_stops==1u && writes>4u && cache_purges==1u,"cache written back, stream stopped, slot written");
    check(!fail_once && write_attempts==writes+1u,"a transient write error was retried once");
    uint32_t first_writes=writes;
    /* The dump's expected content, for the decode check. */
    write_file(argv[5],ram,0x01000000u);
    /* Session 2: slot 1; the reader's writes fail: red, slot stays CLAIMED. */
    memset(&probe,0,sizeof(probe));color_count=0u;fail_writes=true;
    stage(&kui_wce_probe_seed,1u,"T42101N");
    session();
    if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_FAULT,7u,0u,0u);fail("no restart");}
    check(color_count==2u && colors[1]==PROBE_RED,"red on a failed write");
    fail_writes=false;
    /* A seed that is not exactly what the stage installed writes nothing:
     * a changed LBA, another version, a run past the partition, or runs that
     * do not cover one slot's terminal area (the last two re-sealed). */
    const struct kui_wce_probe_seed saved=kui_wce_probe_seed;
    static const struct host_manifest small_map={BLOCKS_FOR_MAP,0u,1024u};
    for(volatile unsigned damage=0;damage<4u;damage++) {
        kui_wce_probe_seed=saved;color_count=0u;writes=0u;e.manifest=&host_map;
        if(damage==0u) kui_wce_probe_seed.runs[0].lba^=1u;
        if(damage==1u) kui_wce_probe_seed.version=2u;
        if(damage==2u) e.manifest=&small_map;
        if(damage==3u) {
            kui_wce_probe_seed.runs[0].sectors-=1u;
            kui_wce_probe_seed.crc32=kui_wce_probe_crc32(0u,&kui_wce_probe_seed,offsetof(struct kui_wce_probe_seed,crc32));
        }
        if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_MENU,1u,0u,0u);fail("no restart");}
        check(color_count==1u && colors[0]==PROBE_YELLOW && !writes,"a damaged seed: yellow, nothing written");
    }
    e.manifest=&host_map;
    /* No seed (no data file at launch): yellow, no write at all. */
    memset(&kui_wce_probe_seed,0,sizeof(kui_wce_probe_seed));color_count=0u;writes=0u;
    if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_MENU,1u,0u,0u);fail("no restart");}
    check(color_count==1u && colors[0]==PROBE_YELLOW && !writes,"yellow, nothing written");
    /* Session 3 claims slot 2: a COMPLETE and a CLAIMED slot are both skipped. */
    struct kui_wce_probe_seed third;
    stage(&third,2u,"T9504M");
    /* Power off between the CLAIMED header and the stage report: slot 3 is
     * used with no report, and the next launch claims slot 4. */
    interrupt_claim=true;stage(&third,3u,"T40301N");interrupt_claim=false;
    stage(&third,4u,"T12509N");
    /* Session 6: slot 5 ends in a reader fault (a refused request), named as
     * the probe's report_fault macro names it: by its message's length. */
    memset(&probe,0,sizeof(probe));color_count=0u;writes=0u;
    stage(&kui_wce_probe_seed,5u,"T40601N");
    session();
    if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_FAULT,KUI_GD_REQUEST,
        (uint32_t)sizeof("GD REQUEST REJECTED"),0u);fail("no restart");}
    check(color_count==2u && colors[1]==PROBE_GREEN,"a fault's dump completes");
    /* Session 7: slot 6; the card fails part-way through the pages: red,
     * and the slot keeps its report and the pages written so far. */
    memset(&probe,0,sizeof(probe));color_count=0u;writes=0u;fail_after=6u;
    stage(&kui_wce_probe_seed,6u,"T44102N");
    session();
    if(!setjmp(rebooted)) {kui_wce_probe_terminal(&display,KUI_WCE_PROBE_MENU,1u,0u,0u);fail("no restart");}
    check(color_count==2u && colors[1]==PROBE_RED && writes==6u,"red after a mid-dump failure");
    fail_after=0u;
    /* The claim's refusals on the filled file. */
    {
        static uint8_t work[512];
        static struct kui_wce_probe_layout layout;
        struct kui_wce_probe_volume v={0u,BLOCKS,card_read,NULL};
        struct kui_wce_probe_claim c;
        check(kui_wce_probe_fat_find(&v,&layout,work)==KUI_WCE_PROBE_FAT_OK,"lookup");
        check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_FULL,"every slot used: FULL");
        uint8_t *head=disk+(size_t)kui_wce_probe_lba(&layout,0u)*512u;
        head[0]^=1u;
        check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_NOT_FILE,"bad file header: NOT_FILE");
        head[0]^=1u;
        uint8_t *slot3=disk+(size_t)kui_wce_probe_lba(&layout,2048u+3u*KUI_WCE_PROBE_SLOT_SECTORS)*512u;
        uint8_t keep[64];memcpy(keep,slot3,64u);
        slot3[60]^=1u;
        check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_SLOT_HEADER,"damaged slot header");
        /* An EMPTY header over written sectors (slot 3's report area is
         * blank, so use slot 6's, which holds a report and pages). */
        memcpy(slot3,keep,64u);
        uint8_t *slot6=disk+(size_t)kui_wce_probe_lba(&layout,2048u+6u*KUI_WCE_PROBE_SLOT_SECTORS)*512u;
        struct kui_wce_probe_slot empty;
        kui_wce_probe_slot_header(&empty,6u,KUI_WCE_PROBE_EMPTY,0u,0u,((const struct kui_wce_probe_file *)(const void *)head)->file_id);
        uint8_t keep6[64];memcpy(keep6,slot6,64u);memcpy(slot6,&empty,sizeof(empty));
        check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_NOT_BLANK,"EMPTY over written sectors: NOT_BLANK");
        memcpy(slot6,keep6,64u);
        check(kui_wce_probe_claim_find(&v,&layout,work,&c)==KUI_WCE_PROBE_CLAIM_FULL,"restored");
    }

    /* Copy the file out for the decoder. */
    check(f_mount(&fs,"0:",1)==FR_OK,"mount");
    check(f_open(&out,"0:/KUI/WCEDATA/WCEDATA.BIN",FA_READ)==FR_OK,"reopen");
    FILE *copy=fopen(argv[4],"wb");if(!copy) fail("out data");
    while(f_read(&out,chunk,sizeof(chunk),&done)==FR_OK && done) check(fwrite(chunk,1,done,copy)==done,"out write");
    fclose(copy);f_close(&out);
    FILE *summary=fopen(argv[6],"w");if(!summary) fail("json");
    fprintf(summary,"{\"calls\":%u,\"audio\":%u,\"maple_samples\":%u,\"first_writes\":%u,\"pages\":%u}\n",
        (unsigned)probe.calls,(unsigned)probe.audio_count,(unsigned)probe.maple_samples,first_writes,0u);
    fclose(summary);
    printf("PASS WinCE data probe end to end (%s, %u fragments): stage claim, reader session and "
        "end-of-game dump stayed inside their slots; red, yellow and full-slot paths\n",argv[1],fragment);
    free(disk);free(ram);
    return 0;
}
