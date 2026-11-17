/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the WinCE data probe's FAT32/exFAT lookup
 * (src/core/wce_probe_fat.c) against FatFs on RAM disks: every sector the
 * lookup maps must hold the bytes FatFs reads from the same file offset.
 * Compiled with a private FatFs config that enables f_mkfs
 * (tests/test_wce_probe_fat.py). */
#include "ff.h"
#include "diskio.h"
#include "kui/wce_probe.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKS (96u*1024u*1024u/512u)
static uint8_t *disk;
static unsigned reads;

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
    (void)context;
    if(lba>=BLOCKS) return -1;
    ++reads;memcpy(sector,disk+(size_t)lba*512u,512u);return 0;
}
static void fail(const char *what) {fprintf(stderr,"FAIL %s\n",what);exit(1);}
static void check(bool ok,const char *what) {if(!ok) fail(what);}

static uint32_t pattern(uint32_t offset) {return offset*2654435761u^0x5a17c3e1u;}

static void format(bool exfat,unsigned cluster) {
    memset(disk,0,(size_t)BLOCKS*512u);
    static BYTE work[65536];
    MKFS_PARM p={.fmt=(BYTE)((exfat?FM_EXFAT:FM_FAT32)|FM_SFD),.n_fat=2,.align=1,.n_root=512,.au_size=cluster};
    check(f_mkfs("0:",&p,work,sizeof(work))==FR_OK,"mkfs");
}

/* Write `bytes` of pattern into path. With fragment, the file grows in turn
 * with a spacer file, which is then deleted: the allocator must interleave
 * them, so the data file has about `fragment` pieces. */
static void write_file(const char *path,uint32_t bytes,unsigned fragment) {
    FIL file,spacer;UINT done;
    check(f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"data open");
    if(fragment) check(f_open(&spacer,"0:/SPACER.BIN",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"spacer open");
    static uint32_t block[2048];
    static uint8_t fill[4096];memset(fill,0x77,sizeof(fill));
    uint32_t piece=fragment?((bytes/fragment+4095u)&~4095u):bytes;
    for(uint32_t at=0;at<bytes;) {
        uint32_t end=at+piece<bytes?at+piece:bytes;
        for(;at<end;) {
            for(unsigned i=0;i<2048u;i++) block[i]=pattern(at/4u+i);
            UINT n=end-at<sizeof(block)?(UINT)(end-at):(UINT)sizeof(block);
            check(f_write(&file,block,n,&done)==FR_OK && done==n,"data write");
            at+=n;
        }
        check(f_sync(&file)==FR_OK,"data sync");
        if(fragment) {
            check(f_write(&spacer,fill,sizeof(fill),&done)==FR_OK && done==sizeof(fill),"spacer write");
            check(f_sync(&spacer)==FR_OK,"spacer sync");
        }
    }
    check(f_close(&file)==FR_OK,"data close");
    if(fragment) {
        check(f_close(&spacer)==FR_OK,"spacer close");
        check(f_unlink("0:/SPACER.BIN")==FR_OK,"spacer unlink");
    }
}

static uint32_t partition_start(void) {return 0u;} /* FM_SFD: the volume starts at LBA 0 */

static enum kui_wce_probe_fat_result find(struct kui_wce_probe_layout *layout) {
    struct kui_wce_probe_volume v={partition_start(),BLOCKS,card_read,NULL};
    static uint8_t work[512];
    return kui_wce_probe_fat_find(&v,layout,work);
}

static void verify(const struct kui_wce_probe_layout *layout,uint32_t bytes) {
    check(layout->bytes==bytes,"size");
    uint32_t sectors=bytes/512u;
    for(uint32_t s=0;s<sectors;s++) {
        uint32_t lba=kui_wce_probe_lba(layout,s);
        check(lba && lba<BLOCKS,"lba range");
        const uint32_t *w=(const uint32_t *)(const void *)(disk+(size_t)lba*512u);
        for(unsigned i=0;i<128u;i++) if(w[i]!=pattern(s*128u+i)) fail("sector content");
    }
    check(!kui_wce_probe_lba(layout,sectors),"past end");
    /* Runs over an arbitrary window agree with per-sector lookups. */
    struct kui_wce_probe_run runs[KUI_WCE_PROBE_FILE_RUNS];
    uint32_t first=sectors/3u,count=sectors/2u;
    uint32_t n=kui_wce_probe_runs(layout,first,count,runs,KUI_WCE_PROBE_FILE_RUNS);
    check(n>0u,"window runs");
    uint32_t s=first;
    for(uint32_t i=0;i<n;i++)
        for(uint32_t k=0;k<runs[i].sectors;k++,s++) check(runs[i].lba+k==kui_wce_probe_lba(layout,s),"run agreement");
    check(s==first+count,"run coverage");
    check(!kui_wce_probe_runs(layout,first,count,runs,0u),"zero capacity refused");
    check(!kui_wce_probe_runs(layout,sectors-1u,2u,runs,4u),"window past end refused");
}

static void mkdirs(void) {
    check(f_mkdir("0:/KUI")==FR_OK,"mkdir KUI");
    check(f_mkdir("0:/KUI/WCEDATA")==FR_OK,"mkdir WCEDATA");
}

int main(void) {
    disk=calloc(BLOCKS,512u);
    if(!disk) return 2;
    static FATFS fs;
    struct kui_wce_probe_layout layout;
    const struct {bool exfat;unsigned cluster;unsigned fragment;uint32_t bytes;const char *name;} cases[]={
        {false,512u,0u,6u*1024u*1024u,"0:/KUI/WCEDATA/WCEDATA.BIN"},
        {false,1024u,12u,8u*1024u*1024u+512u,"0:/KUI/WCEDATA/WCEDATA.BIN"},
        {false,512u,0u,1u*1024u*1024u,"0:/KUI/WCEDATA/wcedata.bin"},
        {true,4096u,0u,6u*1024u*1024u,"0:/KUI/WCEDATA/WCEDATA.BIN"},
        {true,4096u,12u,8u*1024u*1024u+512u,"0:/KUI/WCEDATA/WCEDATA.BIN"},
        {true,32768u,0u,3u*1024u*1024u,"0:/KUI/WCEDATA/WceData.bin"},
    };
    for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
        format(cases[c].exfat,cases[c].cluster);
        check(f_mount(&fs,"0:",1)==FR_OK,"mount");
        /* Distracting entries before the real ones. */
        check(f_mkdir("0:/KUIX")==FR_OK,"mkdir KUIX");
        check(f_mkdir("0:/Games")==FR_OK,"mkdir Games");
        mkdirs();
        check(f_mkdir("0:/KUI/WCEDATA2")==FR_OK,"mkdir WCEDATA2");
        write_file("0:/KUI/WCEDATA/WCEDATA.TXT",4096u,0u);
        write_file(cases[c].name,cases[c].bytes,cases[c].fragment);
        check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
        reads=0u;
        check(find(&layout)==KUI_WCE_PROBE_FAT_OK,"find");
        if(cases[c].fragment) check(layout.run_count>1u,"fragmented file has several runs");
        else check(layout.run_count==1u,"contiguous file has one run");
        verify(&layout,cases[c].bytes);
        printf("PASS %s cluster %u fragment %u: %u runs, %u lookup reads\n",
            cases[c].exfat?"exFAT":"FAT32",cases[c].cluster,cases[c].fragment,
            (unsigned)layout.run_count,reads);
    }
    /* Missing pieces are refused, never guessed. */
    for(unsigned exfat=0;exfat<2u;exfat++) {
        format(exfat,exfat?4096u:512u); /* FAT32 needs 65525 clusters */
        check(f_mount(&fs,"0:",1)==FR_OK,"mount");
        check(f_mkdir("0:/KUI")==FR_OK,"mkdir KUI");
        check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
        check(find(&layout)==KUI_WCE_PROBE_FAT_MISSING,"missing folder");
        check(f_mount(&fs,"0:",1)==FR_OK,"mount");
        check(f_mkdir("0:/KUI/WCEDATA")==FR_OK,"mkdir WCEDATA");
        write_file("0:/KUI/WCEDATA/WCEDATA.BAK",4096u,0u);
        check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
        check(find(&layout)==KUI_WCE_PROBE_FAT_MISSING,"missing file");
        /* A file name that is a directory is refused. */
        check(f_mount(&fs,"0:",1)==FR_OK,"mount");
        check(f_mkdir("0:/KUI/WCEDATA/WCEDATA.BIN")==FR_OK,"mkdir as file name");
        check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
        check(find(&layout)==KUI_WCE_PROBE_FAT_MISSING,"directory named like the file");
        printf("PASS %s refusals\n",exfat?"exFAT":"FAT32");
    }
    /* Over the run limit: 300 fragments. */
    format(false,512u);
    check(f_mount(&fs,"0:",1)==FR_OK,"mount");
    mkdirs();
    write_file("0:/KUI/WCEDATA/WCEDATA.BIN",3u*1024u*1024u,300u);
    check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
    check(find(&layout)==KUI_WCE_PROBE_FAT_FRAGMENTED,"over the run limit");
    printf("PASS FAT32 refuses more than %u runs\n",KUI_WCE_PROBE_FILE_RUNS);
    /* Volumes the lookup must not trust (raw edits of a good volume). */
    for(unsigned exfat=0;exfat<2u;exfat++) {
        format(exfat,exfat?4096u:512u);
        check(f_mount(&fs,"0:",1)==FR_OK,"mount");
        mkdirs();
        write_file("0:/KUI/WCEDATA/WCEDATA.BIN",2u*1024u*1024u,0u);
        check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
        check(find(&layout)==KUI_WCE_PROBE_FAT_OK && layout.run_count==1u,"good volume");
        uint8_t *boot=disk;
        if(exfat) {
            /* ActiveFat=1 (VolumeFlags bit 0): the second FAT would be live. */
            boot[0x6a]|=1u;
            check(find(&layout)==KUI_WCE_PROBE_FAT_VOLUME,"exFAT ActiveFat 1 refused");
            boot[0x6a]&=(uint8_t)~1u;
            /* ValidDataLength short of DataLength: the tail is not file data. */
            uint8_t *entry=NULL;
            for(size_t at=0;at+64u<=(size_t)BLOCKS*512u && !entry;at+=32u) {
                uint8_t *d=disk+at;
                if(d[0]==0xc0u && (d[24]|d[25]<<8|d[26]<<16|(uint32_t)d[27]<<24)==2u*1024u*1024u &&
                   d[32]==0xc1u && d[34]=='W' && d[36]=='C' && d[38]=='E') entry=d;
            }
            check(entry!=NULL,"stream extension entry");
            entry[8]=0u;entry[9]=0u;entry[10]=0x10u;entry[11]=0u; /* 1 MiB valid of 2 */
            check(find(&layout)==KUI_WCE_PROBE_FAT_CHAIN,"exFAT ValidDataLength short refused");
        } else {
            /* Mirroring off with FAT 1 active: FAT 0 may be stale. */
            boot[40]=0x81u;
            check(find(&layout)==KUI_WCE_PROBE_FAT_VOLUME,"FAT32 active FAT 1 refused");
            boot[40]=0u;
            /* A chain that loops back into itself. */
            check(find(&layout)==KUI_WCE_PROBE_FAT_OK,"good again");
            uint32_t reserved=boot[14]|boot[15]<<8,fats=boot[16];
            uint32_t fat=boot[36]|boot[37]<<8|boot[38]<<16|(uint32_t)boot[39]<<24;
            uint32_t data=reserved+fats*fat,first=(layout.runs[0].lba-data)/boot[13]+2u;
            uint32_t last=first+2u*1024u*1024u/512u-1u; /* one contiguous run */
            #define SET_FAT(cluster,value) do {uint8_t *x=disk+(size_t)reserved*512u+(size_t)(cluster)*4u; \
                x[0]=(uint8_t)(value);x[1]=(uint8_t)((value)>>8);x[2]=(uint8_t)((value)>>16);x[3]=(uint8_t)((value)>>24);} while(0)
            /* An early loop: refused by the run limit or the overlap check. */
            SET_FAT(first+5u,first+2u);
            enum kui_wce_probe_fat_result r=find(&layout);
            check(r==KUI_WCE_PROBE_FAT_CHAIN || r==KUI_WCE_PROBE_FAT_FRAGMENTED,"early loop refused");
            SET_FAT(first+5u,first+6u);
            check(find(&layout)==KUI_WCE_PROBE_FAT_OK,"chain restored");
            /* A loop three clusters before the end: two runs that overlap. */
            SET_FAT(last-3u,first);
            check(find(&layout)==KUI_WCE_PROBE_FAT_CHAIN,"late loop refused by the overlap check");
            #undef SET_FAT
            /* A FAT size that wraps the data start. */
            format(false,512u);
            boot[36]=0xffu;boot[37]=0xffu;boot[38]=0xffu;boot[39]=0x7fu;
            check(find(&layout)==KUI_WCE_PROBE_FAT_VOLUME,"huge FAT size refused");
        }
        printf("PASS %s refuses an untrusted volume\n",exfat?"exFAT":"FAT32");
    }
    /* Not a FAT32/exFAT volume. */
    memset(disk,0,512u);
    check(find(&layout)==KUI_WCE_PROBE_FAT_VOLUME,"blank volume");
    /* CRC-32 check value. */
    check(kui_wce_probe_crc32(0u,"123456789",9u)==UINT32_C(0xcbf43926),"crc32");
    printf("PASS WinCE data probe FAT lookup\n");
    free(disk);
    return 0;
}
