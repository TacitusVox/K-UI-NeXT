/* SPDX-License-Identifier: GPL-3.0-only */
/* Build the production GUI mapper and each fixed-size reader separately.
 * The GUI writes a real 128 KiB-cluster exFAT image. Each simulated detached
 * reader sees only its own decoded manifest and raw 512-byte card reads. */
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/retail_cursor.h"

enum { cluster_bytes=131072 };
static uint8_t file_byte(unsigned track,unsigned stride,uint32_t offset) {
    uint32_t at=offset%stride;
    if(stride==2352) {
        if(at==0 || at==11)return 0;
        if(at<11)return 255;
        if(at==15)return 1;
    }
    return (uint8_t)(track*13u+offset/stride*37u+at*19u+(at>>8));
}
static uint8_t wire[KUI_RETAIL_IMAGE_WIRE_BYTES];
static struct kui_retail_manifest disc;
#if TEST_GUI
#include "kui/native_map_capacity.h"
#include "../src/apps/games_retail.c"
/* Reuse the project's host block backend; its formatter main isn't called. */
#define main formatter_main
#include "format_test_image.c"
#undef main
static unsigned largest_track,largest_fragments;
static void log_map(const char *format,...) {
    char line[160];va_list ap;va_start(ap,format);vsnprintf(line,sizeof(line),format,ap);va_end(ap);
    (void)sscanf(line,"Retail boot: track %u is in %u pieces",&largest_track,&largest_fragments);
}
static void write_track(FIL *track,unsigned number,unsigned stride,uint32_t bytes,FIL *spacer) {
    static uint8_t buffer[cluster_bytes];
    uint32_t offset=0;
    while(offset<bytes) {
        unsigned n=bytes-offset;if(n>sizeof(buffer))n=sizeof(buffer);
        for(unsigned j=0;j<n;++j)buffer[j]=file_byte(number,stride,offset+j);
        UINT done;assert(f_write(track,buffer,n,&done)==FR_OK && done==n);
        if(spacer)assert(f_write(spacer,buffer,sizeof(buffer),&done)==FR_OK && done==sizeof(buffer));
        offset+=n;
    }
}
static unsigned module_capacity(int extended) {
    uint8_t module[KUI_NATIVE_MAP_CAP_OFFSET+KUI_NATIVE_MAP_CAP_BYTES]={0};
    static const uint8_t record[16]={'K','U','I','N','M','P','0','1',1,0,0,0,160,0,0,0};
    assert(kui_native_map_capacity(module,sizeof(module)-1u)==0);
    memcpy(module,record,sizeof(record)); /* incidental magic outside the header */
    assert(kui_native_map_capacity(module,sizeof(module))==64);
    if(!extended)return 64;
    uint8_t *header=module+KUI_NATIVE_MAP_CAP_OFFSET;memcpy(header,record,sizeof(record));
    for(unsigned i=0;i<sizeof(record);++i) {
        header[i]^=1u;assert(kui_native_map_capacity(module,sizeof(module))==0);header[i]^=1u;
    }
    header[12]=128;assert(kui_native_map_capacity(module,sizeof(module))==128);
    header[12]=129;assert(kui_native_map_capacity(module,sizeof(module))==0);
    header[12]=160;
    unsigned n=kui_native_map_capacity(module,sizeof(module));assert(n==160);return n;
}
int main(int argc,char **argv) {
    assert(argc==6);
    unsigned tracks=(unsigned)atoi(argv[3]),pieces=(unsigned)atoi(argv[4]),stride=(unsigned)atoi(argv[5]);
    image=fopen(argv[1],"w+b");assert(image);
    assert(!fseek(image,SECTORS*512L-1,SEEK_SET) && fputc(0,image)==0);
    uint8_t work[8192];MKFS_PARM format={.fmt=FM_EXFAT|FM_SFD,.n_fat=1,.align=1,.au_size=cluster_bytes};
    FATFS fs;assert(f_mkfs("0:",&format,work,sizeof(work))==FR_OK);
    assert(f_mount(&fs,"0:",1)==FR_OK && fs.csize==cluster_bytes/512);
    struct kui_game_image game={.count=tracks};uint32_t lba=0;
    for(unsigned i=0;i<tracks;++i) {
        struct kui_game_image_track *t=&game.tracks[i];
        snprintf(t->name,sizeof(t->name),"T%02u.bin",i+1);
        if(i==2)lba=45000;
        t->start_lba=lba;t->control=(i==0 || i==2 || i==tracks-1)?4:0;
        t->sector_bytes=t->control?stride:2352u;
        uint32_t sectors=i==2?((pieces-1u)*cluster_bytes)/stride+2u:11u;
        t->end_lba=lba+=sectors;t->file_bytes=sectors*t->sector_bytes;
        FIL file,spacer;assert(f_open(&file,t->name,FA_CREATE_NEW|FA_WRITE)==FR_OK);
        if(i==2)assert(f_open(&spacer,"separate.bin",FA_CREATE_NEW|FA_WRITE)==FR_OK);
        write_track(&file,i+1,t->sector_bytes,(uint32_t)t->file_bytes,i==2?&spacer:NULL);
        if(i==2)assert(f_close(&spacer)==FR_OK);
        assert(f_close(&file)==FR_OK);
    }
    struct files files={.root="/"};struct kui_volume vol={.count=SECTORS};
    disc.track_count=tracks;disc.storage_transport=KUI_STORAGE_SCI;disc.reader=KUI_RETAIL_READER_ASYNC;
    disc.card_sectors=disc.partition_end=SECTORS;disc.session_lba=disc.boot_lba=45000;disc.boot_bytes=2048;
    strcpy(disc.title,"Host data");strcpy(disc.bootfile,"1ST_READ.BIN");
    for(int extended=0;extended<=1;extended++) {
        unsigned capacity=module_capacity(extended);
        bool audio=false;
        enum map_result r=map_tracks(&files,&fs,&vol,&game,&disc,capacity,&audio);
        unsigned data_slots=tracks+pieces+2u,all_slots=tracks*2u+pieces-1u;
        if(data_slots>capacity) {
            assert(r==MAP_FULL);
            largest_track=largest_fragments=0;
            const char *reason=background_map_refusal("Background",capacity,&files,&fs,&vol,&game,&disc,log_map);
            char total[96];snprintf(total,sizeof(total),"%u tracks + %u data pieces need %u",tracks,pieces+2u,data_slots);
            assert(strstr(reason,total) && largest_track==3u && largest_fragments==pieces);
            continue;
        }
        assert(r==MAP_OK && audio==(all_slots<=capacity));
        assert(disc.track_count+disc.extent_count==(audio?all_slots:data_slots));
        assert(disc.slots[2].track.extent_count==pieces);
        assert(kui_retail_manifest_encode_capacity(&disc,wire,capacity)==KUI_GAME_OK);
        char output_path[1024];int n=snprintf(output_path,sizeof(output_path),"%s-%u",argv[2],capacity);
        assert(n>0 && (size_t)n<sizeof(output_path));
        FILE *out=fopen(output_path,"wb");assert(out && fwrite(wire,1,sizeof(wire),out)==sizeof(wire));
        assert(!fclose(out));
    }
    assert(f_mount(NULL,"0:",0)==FR_OK && !fclose(image));
    return 0;
}
#else
/* Exercise the real GD request path and the interrupt reader's physical
 * cursor, driven from the decoded wire, without a console or FatFs. */
#define main retail_gd_fixture_main
#include "test_retail_gd.c"
#undef main
static uint8_t output_bytes[3u*2352u];
static void take_piece(void *ctx,uint32_t at,const uint8_t *bytes,uint32_t n) {
    (void)ctx;assert(at<=sizeof(output_bytes) && n<=sizeof(output_bytes)-at);
    memcpy(output_bytes+at,bytes,n);
}
static void read_sample(FILE *card,unsigned number,unsigned stride,uint32_t start,enum kui_game_sector_format mode) {
    uint32_t lba=disc.slots[number-1u].track.start_lba+start;
    const unsigned width=mode==KUI_GAME_SECTOR_RAW?2352u:2048u;
    struct kui_retail_cursor cursor;
    assert(kui_retail_image_check_validated(&disc,lba,3,mode)==KUI_GAME_OK);
    assert(kui_retail_cursor_begin(&cursor,&disc,lba,3,mode,take_piece,NULL)==KUI_GAME_OK);
    while(cursor.done!=3) {
        uint8_t block[512];assert(!fseek(card,(long)cursor.block*512,SEEK_SET));
        assert(fread(block,1,sizeof(block),card)==sizeof(block) && kui_retail_cursor_feed(&cursor,block)==KUI_GAME_OK);
    }
    for(unsigned i=0;i<3u;++i)for(unsigned j=0;j<width;++j) {
        uint32_t file_at=(i+start)*stride+j+(stride==2352 && mode!=KUI_GAME_SECTOR_RAW?16u:0u);
        assert(output_bytes[i*width+j]==file_byte(number,stride,file_at));
    }
}
int main(int argc,char **argv) {
    assert(argc==5);unsigned tracks=(unsigned)atoi(argv[3]),stride=(unsigned)atoi(argv[4]);
    FILE *f=fopen(argv[2],"rb");assert(f && fread(wire,1,sizeof(wire),f)==sizeof(wire) && !fclose(f));
    uint32_t used=(uint32_t)wire[20]+(uint32_t)wire[24];
    enum kui_game_result r=kui_retail_manifest_decode(wire,&disc);
    if(used>KUI_RETAIL_ASYNC_SLOTS) {assert(r!=KUI_GAME_OK);return 0;}
    assert(r==KUI_GAME_OK && disc.track_count==tracks);
    reset();assert(kui_retail_gd_init(&service,disc.slots,tracks,&ops,BEGIN,END)==0);
    for(unsigned area=0;area<2;++area) {
        put(PARAM,area);put(PARAM+4,OUTPUT);
        int32_t t=call(KUI_GD_REQUEST,KUI_GD_GETTOC2,PARAM);
        assert(t>0 && call(KUI_GD_EXEC,0,0)==0 && call(KUI_GD_CHECK,(uint32_t)t,STATUS)==KUI_GD_COMPLETED);
        for(unsigned i=0;i<tracks;++i) {
            const struct kui_retail_track *tr=&disc.slots[i].track;
            uint32_t entry=(kui_retail_track_control(tr)<<28)|0x01000000u|(tr->start_lba+150u);
            assert(get(OUTPUT+i*4u)==((i>=2)==(area==1)?entry:UINT32_MAX));
        }
    }
    FILE *card=fopen(argv[1],"rb");assert(card);
    const struct kui_retail_track *t=&disc.slots[2].track;
    for(unsigned n=1;n<t->extent_count;++n) {
        const struct kui_retail_extent *e=&disc.slots[kui_retail_track_first_extent(t)+n].extent;
        unsigned start=e->file_block*512u/stride-1u;
        read_sample(card,3,stride,start,KUI_GAME_SECTOR_MODE1);
        if(stride==2352)read_sample(card,3,stride,start,KUI_GAME_SECTOR_RAW);
    }
    const struct kui_retail_track *audio=&disc.slots[3].track;
    assert(!kui_retail_track_control(audio));
    if(audio->extent_count)read_sample(card,4,2352,0,KUI_GAME_SECTOR_RAW);
    else assert(kui_retail_image_check_validated(&disc,audio->start_lba,1,KUI_GAME_SECTOR_RAW)==KUI_GAME_AUDIO);
    if(stride==2048)
        assert(kui_retail_image_check_validated(&disc,t->start_lba,1,KUI_GAME_SECTOR_RAW)==KUI_GAME_UNSUPPORTED);
    assert(!fclose(card));
    /* Damage the physical map and recompute the wire CRC: the structural
     * validator must catch the out-of-partition extent independent of CRC. */
    size_t at=320u+tracks*12u+4u;memset(wire+at,0xff,4u);memset(wire+16u,0,4u);
    uint32_t crc=kui_retail_crc32(0,wire,sizeof(wire));
    for(unsigned i=0;i<4;i++)wire[16+i]=(uint8_t)(crc>>(i*8u));
    assert(kui_retail_manifest_decode(wire,&disc)!=KUI_GAME_OK);
    return 0;
}
#endif
