/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the stream survey's report file (include/kui/toy_survey.h):
 * its text, the lookup of 0:/KUI/SURVEY/STREAM.TXT (src/core/toy_survey_fat.c)
 * and the search for a blank slot (src/core/toy_survey.c), against FatFs on
 * RAM disks. Every launch is played as Test 83's cold stage and the Toy
 * worker play it: the launch record into the claimed slot's first sector, the
 * report into the two sectors of the seed after their CRC-32 is checked
 * again. At the end FatFs must read back exactly the expected file, and no
 * other sector of the card may have changed.
 * Compiled with a private FatFs config that enables f_mkfs, and run with a
 * directory to leave a blank and a used file in (tests/test_toy_survey.py). */
#include "ff.h"
#include "diskio.h"
#include "kui/toy_survey.h"
#include "kui/retail_image.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKS (96u*1024u*1024u/512u)
#define SLOTS 20u
#define FILE_SECTORS (1u+3u*SLOTS)
#define FILE_BYTES (FILE_SECTORS*512u)
#define PATH "0:/KUI/SURVEY/STREAM.TXT"
static uint8_t *disk,*before;
static unsigned reads,fail_read_at,fail_read_from;
static const struct kui_toy_survey_file survey={
    {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10},SLOTS};

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

static void fail(const char *what) {fprintf(stderr,"FAIL %s\n",what);exit(1);}
static void check(bool ok,const char *what) {if(!ok) fail(what);}
/* A sector has no terminator: where `text` begins in it, or NULL. */
static uint8_t *within(uint8_t sector[512],const char *text) {
    size_t n=strlen(text);
    for(size_t at=0u;at+n<=512u;at++) if(!memcmp(sector+at,text,n)) return sector+at;
    return NULL;
}
static int card_read(void *context,uint32_t lba,uint8_t sector[512]) {
    (void)context;
    if(lba>=BLOCKS) return -1;
    if((fail_read_at || fail_read_from) && (++reads==fail_read_at || (fail_read_from && reads>=fail_read_from))) return -1;
    memcpy(sector,disk+(size_t)lba*512u,512u);return 0;
}

static void format(bool exfat,unsigned cluster) {
    memset(disk,0,(size_t)BLOCKS*512u);
    static BYTE work[65536];
    MKFS_PARM p={.fmt=(BYTE)((exfat?FM_EXFAT:FM_FAT32)|FM_SFD),.n_fat=2,.align=1,.n_root=512,.au_size=cluster};
    check(f_mkfs("0:",&p,work,sizeof(work))==FR_OK,"mkfs");
}
/* The blank file, as tools/toy_survey.py makes it. */
static void blank_image(uint8_t *out) {
    kui_toy_survey_file_sector(&survey,out);
    for(uint32_t slot=0u;slot<SLOTS;slot++)
        for(uint32_t part=0u;part<3u;part++)
            kui_toy_survey_blank_sector(&survey,slot,part,out+512u*kui_toy_survey_sector(slot,part));
}
/* With fragment, the file grows in turn with a spacer file, which is then
 * deleted: the allocator must interleave them. */
static void write_file(const char *path,const uint8_t *data,uint32_t bytes,unsigned fragment) {
    FIL file,spacer;UINT done;
    check(f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"data open");
    if(fragment) check(f_open(&spacer,"0:/SPACER.BIN",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK,"spacer open");
    static uint8_t fill[4096];memset(fill,0x77,sizeof(fill));
    uint32_t piece=fragment?((bytes/fragment+4095u)&~4095u):bytes;
    for(uint32_t at=0u;at<bytes;) {
        uint32_t n=bytes-at<piece?bytes-at:piece;
        check(f_write(&file,data+at,n,&done)==FR_OK && done==n,"data write");
        check(f_sync(&file)==FR_OK,"data sync");
        at+=n;
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
static void read_file(const char *path,uint8_t *out,uint32_t bytes) {
    static FATFS fs;FIL file;UINT done;
    check(f_mount(&fs,"0:",1)==FR_OK,"mount to read");
    check(f_open(&file,path,FA_READ)==FR_OK && f_size(&file)==bytes,"open to read");
    check(f_read(&file,out,bytes,&done)==FR_OK && done==bytes,"read back");
    check(f_close(&file)==FR_OK && f_mount(NULL,"0:",0)==FR_OK,"close after reading");
}

/* Whether a sector is a launch record of this file's slot, by its first line. */
static bool launch_record(const struct kui_toy_survey_file *file,uint32_t slot,const uint8_t sector[512]) {
    char line[128];int n=snprintf(line,sizeof(line),"==== SLOT %03u PART 0 ==== ID ",(unsigned)slot);
    for(unsigned i=0u;i<16u;i++) n+=snprintf(line+n,sizeof(line)-(size_t)n,"%02X",file->id[i]);
    n+=snprintf(line+n,sizeof(line)-(size_t)n," ==== LAUNCH ====\n");
    return !memcmp(sector,line,(size_t)n) && sector[511]=='\n';
}
static const struct kui_wce_probe_volume volume={0u,BLOCKS,card_read,NULL};
static struct kui_wce_probe_layout layout;
static uint8_t work[512],expect[512];
static enum kui_wce_probe_fat_result find(void) {return kui_wce_probe_fat_find(&volume,&layout,work);}
static enum kui_toy_survey_claim_result claim(struct kui_toy_survey_claim *c) {
    return kui_toy_survey_claim_find(&volume,&layout,work,expect,c);
}

static struct kui_toy_survey_launch launch_of(uint32_t slot) {
    static char title[128],product[16],bootfile[24],region[16];
    memset(title,' ',sizeof(title));memcpy(title,"SURVEY GAME NUMBER ",19u);
    title[19]=(char)('A'+slot%26u);
    memset(product,' ',sizeof(product));memcpy(product,"T-12345N",8u);
    memset(bootfile,0,sizeof(bootfile));memcpy(bootfile,"1ST_READ.BIN",12u);
    memset(region,0,sizeof(region));memcpy(region,"JUE",3u);
    return (struct kui_toy_survey_launch){.build="0123456789ab",
        .title=title,.title_bytes=sizeof(title),.product=product,.product_bytes=sizeof(product),
        .bootfile=bootfile,.bootfile_bytes=sizeof(bootfile),.region=region,.region_bytes=sizeof(region),
        .ip_crc32=0x11111111u+slot,.gdi_crc32=0x22222222u,.boot_crc32=0x33333333u,.boot_bytes=0x00123456u,
        .exec_crc32=0x44444444u+slot,.exec_bytes=0x00123456u,.reader=1u,.transport=1u,.flags=0x4u,
        .source=slot&1u,.tracks=5u+slot,.raw=1u,.cooked=1u,.audio=2u+slot,.audio_high=1u+slot,.audio_mapped=slot/2u,
        .other=1u,
        .extents=7u,.session_lba=45150u,.boot_lba=45200u+slot};
}
static void report_of(uint32_t slot,uint8_t out[1024]) {
    memset(out,' ',1024u);
    int n=snprintf((char *)out,1024u,"K-UI TOY COMMANDER REPORT\nTEST 81\nBUILD 0123456789ab\nSLOT %u\n",(unsigned)slot);
    out[n]=' ';
    memcpy(out+1024u-12u,"\nEND REPORT\n",12u);
}
/* A sector write of the stage's or the worker's: only ever a file sector. */
static unsigned writes;
static void card_write(uint32_t lba,const uint8_t *data) {
    check(lba && lba<BLOCKS,"write inside the card");
    memcpy(disk+(size_t)lba*512u,data,512u);++writes;
}
static bool reportless(uint32_t slot) {return slot%5u==3u;}

/* Every launch until the file is full, then the whole card compared. */
static void sessions(bool exfat,unsigned cluster,unsigned fragment,uint8_t *image,const char *leave) {
    static FATFS fs;
    static const uint8_t cid[16]={9,8,7,6,5,4,3,2,1,0,1,2,3,4,5,6};
    format(exfat,cluster);
    check(f_mount(&fs,"0:",1)==FR_OK,"mount");
    /* Distracting entries, the Windows CE data probe's own file among them. */
    check(f_mkdir("0:/KUIX")==FR_OK && f_mkdir("0:/KUI")==FR_OK && f_mkdir("0:/KUI/WCEDATA")==FR_OK,"folders");
    blank_image(image);
    write_file("0:/KUI/WCEDATA/WCEDATA.BIN",image,4096u,0u);
    check(f_mkdir("0:/KUI/SURVEY2")==FR_OK && f_mkdir("0:/KUI/SURVEY")==FR_OK,"survey folders");
    write_file("0:/KUI/SURVEY/STREAM.BAK",image,8192u,0u);
    write_file(exfat?"0:/KUI/SURVEY/Stream.txt":PATH,image,FILE_BYTES,fragment);
    check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
    memcpy(before,disk,(size_t)BLOCKS*512u);
    writes=0u;
    unsigned saved=0u;
    for(uint32_t launch=0u;;launch++) {
        struct kui_toy_survey_claim c;
        check(find()==KUI_WCE_PROBE_FAT_OK && layout.bytes==FILE_BYTES,"lookup");
        if(fragment) check(layout.run_count>1u,"fragmented file has several runs");
        enum kui_toy_survey_claim_result r=claim(&c);
        if(launch==SLOTS) {check(r==KUI_TOY_SURVEY_CLAIM_FULL && c.used==SLOTS && !c.lba[0] && !c.lba[1] && !c.lba[2],"full file");break;}
        check(r==KUI_TOY_SURVEY_CLAIM_OK && c.slot==launch && c.used==launch,"next blank slot");
        check(!memcmp(&c.file,&survey,sizeof(survey)),"the file's ID and slots");
        for(uint32_t part=0u;part<3u;part++) {
            kui_toy_survey_blank_sector(&survey,launch,part,expect);
            check(c.lba[part]==kui_wce_probe_lba(&layout,kui_toy_survey_sector(launch,part)) &&
                  !memcmp(disk+(size_t)c.lba[part]*512u,expect,512u) &&
                  c.blank_crc[part]==kui_retail_crc32(0u,expect,512u),"claimed sectors are that slot's blank parts");
        }
        /* The stage: the launch record, read back; then the worker's seed. */
        struct kui_toy_survey_launch facts=launch_of(launch);
        uint8_t record[512];
        kui_toy_survey_launch_sector(&survey,launch,&facts,record);
        check(launch_record(&survey,launch,record) && !launch_record(&survey,launch+1u,record),
            "a launch record names its slot");
        card_write(c.lba[0],record);
        memcpy(image+512u*kui_toy_survey_sector(launch,0u),record,512u);
        struct kui_toy_report_wire seed;
        kui_toy_survey_seed_build(&c,cid,launch&1u,&seed);
        /* What the stage's and the worker's own checks ask of a seed. */
        check(seed.magic[0]==KUI_TOY_REPORT_MAGIC0 && seed.magic[1]==KUI_TOY_REPORT_MAGIC1 &&
              seed.version==KUI_TOY_REPORT_VERSION && seed.bytes==sizeof(seed) && seed.test==66u &&
              !seed.reserved && seed.high_capacity==(launch&1u) && seed.lba[0]!=seed.lba[1] &&
              seed.lba[0]==c.lba[1] && seed.lba[1]==c.lba[2] && !memcmp(seed.cid,cid,16u) &&
              kui_retail_crc32(0u,&seed,KUI_TOY_REPORT_CRC_OFFSET)==seed.crc32,"seed");
        if(reportless(launch)) continue; /* powered off before the gesture */
        uint8_t report[1024];
        report_of(launch,report);
        for(unsigned i=0u;i<2u;i++)
            check(kui_retail_crc32(0u,disk+(size_t)seed.lba[i]*512u,512u)==seed.placeholder_crc[i],
                "the worker finds the blank sectors it was told of");
        for(unsigned i=0u;i<2u;i++) card_write(seed.lba[i],report+512u*i);
        memcpy(image+512u*kui_toy_survey_sector(launch,1u),report,1024u);
        ++saved;
    }
    /* Nothing but the written file sectors changed on the card. */
    unsigned changed=0u;
    for(uint32_t lba=0u;lba<BLOCKS;lba++)
        if(memcmp(disk+(size_t)lba*512u,before+(size_t)lba*512u,512u)) ++changed;
    check(writes==SLOTS+2u*saved && changed==writes,"only the written sectors changed");
    static uint8_t back[FILE_BYTES];
    read_file(PATH,back,FILE_BYTES);
    check(!memcmp(back,image,FILE_BYTES),"FatFs reads back the expected file");
    if(leave) {
        FILE *out=fopen(leave,"wb");
        check(out && fwrite(back,1u,FILE_BYTES,out)==FILE_BYTES && !fclose(out),"leave the used file");
    }
    printf("PASS %s cluster %u fragment %u: %u runs; %u launches, %u reports, %u sectors written, no other changed\n",
        exfat?"exFAT":"FAT32",cluster,fragment,(unsigned)layout.run_count,SLOTS,saved,writes);
}

static void set_sector(uint32_t slot,uint32_t part,const uint8_t *data) {
    memcpy(disk+(size_t)kui_wce_probe_lba(&layout,kui_toy_survey_sector(slot,part))*512u,data,512u);
}
static void refusals(bool exfat,uint8_t *image) {
    static FATFS fs;
    struct kui_toy_survey_claim c;
    uint8_t sector[512];
    /* No folder, no file, a folder named like the file; the probe's file is not taken for it. */
    format(exfat,exfat?4096u:512u);
    check(f_mount(&fs,"0:",1)==FR_OK && f_mkdir("0:/KUI")==FR_OK && f_mkdir("0:/KUI/WCEDATA")==FR_OK,"folders");
    blank_image(image);
    write_file("0:/KUI/WCEDATA/WCEDATA.BIN",image,FILE_BYTES,0u);
    check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
    check(find()==KUI_WCE_PROBE_FAT_MISSING,"no survey folder");
    check(f_mount(&fs,"0:",1)==FR_OK && f_mkdir("0:/KUI/SURVEY")==FR_OK,"folder");
    write_file("0:/KUI/SURVEY/STREAM.TX",image,FILE_BYTES,0u);
    check(f_mount(NULL,"0:",0)==FR_OK,"unmount");
    check(find()==KUI_WCE_PROBE_FAT_MISSING,"no survey file");
    check(f_mount(&fs,"0:",1)==FR_OK && f_mkdir(PATH)==FR_OK && f_mount(NULL,"0:",0)==FR_OK,"folder as file");
    check(find()==KUI_WCE_PROBE_FAT_MISSING,"a folder named like the file");
    /* A file of the wrong size, short or long. */
    for(unsigned longer=0u;longer<2u;longer++) {
        check(f_mount(&fs,"0:",1)==FR_OK && f_unlink(PATH)==FR_OK,"remove");
        static uint8_t wrong[FILE_BYTES+512u];
        memcpy(wrong,image,FILE_BYTES);memset(wrong+FILE_BYTES,' ',512u);
        write_file(PATH,wrong,longer?FILE_BYTES+512u:FILE_BYTES-512u,0u);
        check(f_mount(NULL,"0:",0)==FR_OK && find()==KUI_WCE_PROBE_FAT_OK,"wrong size found");
        check(claim(&c)==KUI_TOY_SURVEY_CLAIM_NOT_FILE && !c.lba[0] && !c.lba[1] && !c.lba[2],"wrong size refused");
    }
    check(f_mount(&fs,"0:",1)==FR_OK && f_unlink(PATH)==FR_OK,"remove");
    write_file(PATH,image,FILE_BYTES,0u);
    check(f_mount(NULL,"0:",0)==FR_OK && find()==KUI_WCE_PROBE_FAT_OK,"good file");
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_OK && c.slot==0u,"good file's first slot");
    /* Every byte of the first sector counts. */
    uint8_t *first=disk+(size_t)kui_wce_probe_lba(&layout,0u)*512u;
    unsigned other_id=0u;
    for(unsigned at=0u;at<512u;at++) {
        first[at]^=0x01u;
        enum kui_toy_survey_claim_result r=claim(&c);
        /* An ID digit changed into another digit is another file's first
         * sector over this file's slots: no slot of it is that file's blank,
         * and the file reads as full. */
        bool digit=at>=29u && at<61u && ((first[at]>='0' && first[at]<='9') || (first[at]>='A' && first[at]<='F'));
        check(r==(digit?KUI_TOY_SURVEY_CLAIM_FULL:KUI_TOY_SURVEY_CLAIM_NOT_FILE),"edited first sector refused");
        check(!c.lba[0] && !c.lba[1] && !c.lba[2],"nothing claimed of an edited file");
        other_id+=digit;
        first[at]^=0x01u;
    }
    check(other_id==28u,"the ID digits that stay digits");
    struct kui_toy_survey_file other=survey;
    other.slots=SLOTS-1u;kui_toy_survey_file_sector(&other,sector);memcpy(first,sector,512u);
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_NOT_FILE,"a slot count the size does not give");
    kui_toy_survey_file_sector(&survey,sector);memcpy(first,sector,512u);
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_OK,"first sector restored");
    /* A slot whose first sector is anything but its blank text is passed
     * over: another file's blank sector, another slot's or another part's,
     * a launch record, nothing at all, a blank with one byte changed. */
    struct kui_toy_survey_launch facts=launch_of(0u);
    other=survey;other.id[5]^=0x80u;
    for(unsigned kind=0u;kind<7u;kind++) {
        if(kind==0u) kui_toy_survey_blank_sector(&other,0u,0u,sector);
        else if(kind==1u) kui_toy_survey_blank_sector(&survey,1u,0u,sector);
        else if(kind==2u) kui_toy_survey_blank_sector(&survey,0u,1u,sector);
        else if(kind==3u) kui_toy_survey_launch_sector(&survey,1u,&facts,sector);
        else if(kind==4u) memset(sector,0,512u);
        else {kui_toy_survey_blank_sector(&survey,0u,0u,sector);sector[kind==5u?511u:0u]^=0x01u;}
        set_sector(0u,0u,sector);
        check(claim(&c)==KUI_TOY_SURVEY_CLAIM_OK && c.slot==1u && c.used==1u &&
              c.lba[0]==kui_wce_probe_lba(&layout,kui_toy_survey_sector(1u,0u)),"a slot that is not blank is passed over");
    }
    /* Slots that are not wholly blank are passed over, never written. */
    kui_toy_survey_launch_sector(&survey,0u,&facts,sector);set_sector(0u,0u,sector); /* launched, no report */
    kui_toy_survey_blank_sector(&survey,1u,1u,sector);sector[100]='x';set_sector(1u,1u,sector); /* part 1 not blank */
    kui_toy_survey_blank_sector(&survey,2u,2u,sector);sector[0]='x';set_sector(2u,2u,sector);   /* part 2 not blank */
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_OK && c.slot==3u && c.used==3u,"used and cut-off slots passed over");
    /* A read that fails, wherever it falls: the first sector's ends the
     * search; a slot's passes that slot over, used or blank. (Ten reads:
     * the first sector, slot 0's one, slot 1's two, slot 2's and 3's three.) */
    for(unsigned at=1u;at<=11u;at++) {
        reads=0u;fail_read_at=at;
        enum kui_toy_survey_claim_result r=claim(&c);
        if(at==1u) check(r==KUI_TOY_SURVEY_CLAIM_IO && !c.lba[0] && !c.lba[1] && !c.lba[2],"failed first read refused");
        else check(r==KUI_TOY_SURVEY_CLAIM_OK && c.slot==(at>=8u && at<=10u?4u:3u) &&
                   c.lba[2]==kui_wce_probe_lba(&layout,kui_toy_survey_sector(c.slot,2u)),"a slot with a sector not read is passed over");
    }
    fail_read_at=0u;
    /* A card that gives nothing more is not asked for every slot. */
    for(unsigned from=2u;from<=10u;from++) {
        reads=0u;fail_read_from=from;
        check(claim(&c)==KUI_TOY_SURVEY_CLAIM_IO && !c.lba[0] && !c.lba[1] && !c.lba[2] &&
              reads==from-1u+KUI_TOY_SURVEY_UNREAD_MAX,"the search gives up at four sectors not read");
    }
    fail_read_from=0u;
    /* Sectors not read and no blank slot behind them: not a full file. */
    for(uint32_t slot=3u;slot<SLOTS;slot++) {kui_toy_survey_launch_sector(&survey,slot,&facts,sector);set_sector(slot,0u,sector);}
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_FULL && c.used==SLOTS,"a file with no blank slot is full");
    reads=0u;fail_read_at=3u;
    check(claim(&c)==KUI_TOY_SURVEY_CLAIM_IO && !c.lba[0],"no blank slot and a sector not read: not called full");
    fail_read_at=0u;
    /* A partition that ends before the file. */
    const struct kui_wce_probe_volume small={0u,layout.runs[0].lba+2u,card_read,NULL};
    check(kui_toy_survey_claim_find(&small,&layout,work,expect,&c)==KUI_TOY_SURVEY_CLAIM_IO,"sector outside the partition");
    printf("PASS %s refusals\n",exfat?"exFAT":"FAT32");
}

static void text_checks(const char *leave) {
    uint8_t sector[512];
    struct kui_toy_survey_file file;
    static uint8_t image[FILE_BYTES];
    blank_image(image);
    check(!kui_toy_survey_file_parse(image,&file) && !memcmp(&file,&survey,sizeof(file)),"first sector parses");
    for(uint32_t at=0u;at<FILE_BYTES;at++)
        check(image[at]=='\n' || (image[at]>=0x20u && image[at]<0x7fu),"a blank file is text");
    for(uint32_t s=0u;s<FILE_SECTORS;s++) check(image[512u*s+511u]=='\n',"every sector ends its line");
    /* Blank sectors differ from each other and are no launch records. */
    for(uint32_t a=1u;a<FILE_SECTORS;a++) {
        check(!launch_record(&survey,(a-1u)/3u,image+512u*a),"a blank sector is no launch record");
        for(uint32_t b=a+1u;b<FILE_SECTORS;b++)
            check(memcmp(image+512u*a,image+512u*b,512u),"blank sectors differ");
    }
    /* Slot counts at the limits; counts that are refused. */
    for(uint32_t slots=1u;slots<=KUI_TOY_SURVEY_SLOTS_MAX;slots+=slots<10u?1u:slots<990u?89u:1u) {
        struct kui_toy_survey_file f=survey;f.slots=slots;
        kui_toy_survey_file_sector(&f,sector);
        check(!kui_toy_survey_file_parse(sector,&file) && file.slots==slots,"slot count parses");
    }
    static const char *const bad[]={"SLOTS 0\n","SLOTS 1000\n","SLOTS 007\n","SLOTS \n","SLOTS 1x\n"};
    for(unsigned i=0u;i<sizeof(bad)/sizeof(bad[0]);i++) {
        kui_toy_survey_file_sector(&survey,sector);
        uint8_t *at=within(sector,"SLOTS ");
        check(at!=NULL,"slots line");
        memcpy(at,bad[i],strlen(bad[i]));
        check(kui_toy_survey_file_parse(sector,&file) && !file.slots,"bad slot count refused");
    }
    kui_toy_survey_file_sector(&survey,sector);sector[29]='a'; /* a lower-case ID digit */
    check(kui_toy_survey_file_parse(sector,&file),"lower-case ID refused");
    /* The longest launch record there can be still ends with its last line. */
    char full[129];memset(full,'W',128u);full[128]=0;
    struct kui_toy_survey_launch widest={.build="WWWWWWWWWWWWWWWWWWWW",.title=full,.title_bytes=128u,
        .product=full,.product_bytes=16u,.bootfile=full,.bootfile_bytes=24u,.region=full,.region_bytes=16u,
        .ip_crc32=~0u,.gdi_crc32=~0u,.boot_crc32=~0u,.boot_bytes=~0u,.exec_crc32=~0u,.exec_bytes=~0u,
        .reader=~0u,.transport=~0u,.flags=~0u,.source=~0u,.tracks=~0u,.raw=~0u,.cooked=~0u,.audio=~0u,
        .audio_high=~0u,.audio_mapped=~0u,.other=~0u,.extents=~0u,.session_lba=~0u,.boot_lba=~0u};
    kui_toy_survey_launch_sector(&survey,KUI_TOY_SURVEY_SLOTS_MAX-1u,&widest,sector);
    check(sector[511]=='\n' && within(sector," BOOTLBA FFFFFFFF\n ")!=NULL,"the widest launch record fits");
    check(within(sector,"\nTRACKS 999 RAW 999 COOKED 999 AUDIO 999 HDAUDIO 999 MAPPED 999 OTHER 999 EXTENTS 999\n")!=NULL,
        "counts are cut at three digits");
    check(launch_record(&survey,KUI_TOY_SURVEY_SLOTS_MAX-1u,sector),"and is a launch record");
    /* Text that is no text: replaced, cut at its terminator, trimmed. */
    static const char odd[16]={'A','\t',(char)0x80,'B',' ',' ',0,'Z'};
    struct kui_toy_survey_launch strange=launch_of(0u);
    strange.product=odd;strange.product_bytes=sizeof(odd);strange.title=NULL;strange.title_bytes=128u;
    kui_toy_survey_launch_sector(&survey,0u,&strange,sector);
    check(within(sector,"\nPRODUCT A??B\nTITLE \nBOOT ")!=NULL,"odd text");
    for(unsigned at=0u;at<512u;at++)
        check(sector[at]=='\n' || (sector[at]>=0x20u && sector[at]<0x7fu),"a launch record is text");
    if(leave) {
        FILE *out=fopen(leave,"wb");
        check(out && fwrite(image,1u,FILE_BYTES,out)==FILE_BYTES && !fclose(out),"leave the blank file");
    }
    printf("PASS survey file text: %u slots, %u bytes\n",SLOTS,FILE_BYTES);
}

int main(int argc,char **argv) {
    char blank_path[1024]="",used_path[1024]="";
    if(argc>1) {
        snprintf(blank_path,sizeof(blank_path),"%s/blank.txt",argv[1]);
        snprintf(used_path,sizeof(used_path),"%s/used.txt",argv[1]);
    }
    disk=calloc(BLOCKS,512u);before=calloc(BLOCKS,512u);
    static uint8_t image[FILE_BYTES];
    if(!disk || !before) return 2;
    text_checks(argc>1?blank_path:NULL);
    const struct {bool exfat;unsigned cluster,fragment;} cases[]={
        {false,512u,0u},{false,512u,9u},{false,1024u,5u},{true,4096u,0u},{true,4096u,7u},{true,32768u,0u},
        {true,1024u,6u},
    };
    for(unsigned i=0u;i<sizeof(cases)/sizeof(cases[0]);i++)
        sessions(cases[i].exfat,cases[i].cluster,cases[i].fragment,image,i==1u && argc>1?used_path:NULL);
    for(unsigned exfat=0u;exfat<2u;exfat++) refusals(exfat,image);
    printf("PASS stream survey report file\n");
    free(disk);free(before);
    return 0;
}
