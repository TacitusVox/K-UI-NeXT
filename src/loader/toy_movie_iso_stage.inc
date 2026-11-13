/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/vmu_launch_package.h"
#include "kui/toy_movie_trace.h"

static struct kui_toy_movie_range toy_movie_ranges[4];
#if defined(KUI_TOY_SUITE_MODE)
#include "kui/native_readahead_layout.h"
static struct kui_toy_suite_seed toy_suite_seed;
static uint32_t toy_suite_source;
#if KUI_TOY_SUITE_MODE == 1 || KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6 || KUI_TOY_SUITE_MODE == 35
static _Alignas(32) uint8_t toy_suite_data[KUI_NATIVE_RA_DATA_BYTES];
#endif
#if KUI_TOY_SUITE_MODE == 1
#include "toy_suite_witness_stage.inc"
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6 || KUI_TOY_SUITE_MODE == 35
#include "toy_suite_ram_stage.inc"
#endif

static const struct kui_retail_track *toy_suite_movie_track(void) {
#if KUI_TOY_SUITE_MODE == 35
    const struct kui_toy_movie_range *movie=&toy_movie_ranges[KUI_TOY_RAM_WINDOW_VARIANT];
#else
    const struct kui_toy_movie_range *movie=&toy_movie_ranges[3];
#endif
    if(!movie->count)return NULL;
    for(uint32_t i=0u;i<manifest.track_count;i++) {
        const struct kui_retail_track *track=&manifest.slots[i].track;
        if(movie->lba>=track->start_lba && movie->lba+movie->count<=track->end_lba)return track;
    }
    return NULL;
}
#endif

static bool movie_iso_dual32(const uint8_t *p, uint32_t *out) {
    uint32_t lo=0, hi=0;
    for(unsigned i=0;i<4u;i++) {
        lo|=(uint32_t)p[i]<<(i*8u);
        hi=(hi<<8)|p[4u+i];
    }
    *out=lo;
    return lo==hi && lo!=0u;
}

static bool movie_iso_name(const uint8_t *name, unsigned length, const char *expected) {
    while(*expected && length) {
        unsigned ch=*name++;
        if(ch>='a' && ch<='z')ch-=32u;
        if(ch!=(unsigned)*expected++)return false;
        --length;
    }
    return !*expected && (!length || (length==2u && name[0]==';' && name[1]=='1'));
}

/* Only checked data sectors in the already validated image map are read.
 * An unsupported or incomplete inventory disables this optional trace. */
static bool movie_iso_read(uint32_t lba) {
    return lba>=manifest.session_lba &&
        kui_retail_image_read(&image,lba,1u,KUI_GAME_SECTOR_MODE1,
            raw_boot,KUI_GAME_DATA_BYTES)==KUI_GAME_OK;
}

static bool movie_range_mapped(uint32_t lba, uint32_t sectors) {
    while(sectors) {
        uint32_t n=sectors>KUI_RETAIL_IMAGE_MAX_SECTORS?KUI_RETAIL_IMAGE_MAX_SECTORS:sectors;
        if(kui_retail_image_check_validated(&manifest,lba,n,KUI_GAME_SECTOR_MODE1)!=KUI_GAME_OK)
            return false;
        lba+=n;sectors-=n;
    }
    return true;
}

static bool movie_iso_ranges(void) {
    static const char *const names[]={"50HZ_BUMPER_LRTF.AVI","60HZ_BUMPER_LRTF.AVI",
        "50HZ_INTRO_LRTF.AVI","60HZ_INTRO_LRTF.AVI"};
    const uint8_t *dir=NULL;
    for(unsigned n=16;n<32u;n++) {
        if(!movie_iso_read(manifest.session_lba+n) || memcmp(raw_boot+1,"CD001\1",6u))return false;
        if(raw_boot[0]==1u) {dir=raw_boot+156u;break;}
        if(raw_boot[0]==255u)return false;
    }
    uint32_t root,bytes;
    if(!dir || raw_boot[128] || raw_boot[129]!=8u || raw_boot[130]!=8u || raw_boot[131] ||
       dir[0]!=34u || dir[1] || dir[25]!=2u || dir[26] || dir[27] || dir[32]!=1u || dir[33] ||
       !movie_iso_dual32(dir+2,&root) || !movie_iso_dual32(dir+10,&bytes) ||
       root<manifest.session_lba || root>=719850u || bytes>128u*1024u)return false;
    for(uint32_t at=0;at<bytes;at+=KUI_GAME_DATA_BYTES) {
        if(!movie_iso_read(root+at/KUI_GAME_DATA_BYTES))return false;
        uint32_t limit=bytes-at;
        if(limit>KUI_GAME_DATA_BYTES)limit=KUI_GAME_DATA_BYTES;
        for(uint32_t pos=0;pos<limit && raw_boot[pos];) {
            const uint8_t *entry=raw_boot+pos;
            unsigned length=entry[0];
            if(length<34u || length>limit-pos || !entry[32] || entry[32]>length-33u)return false;
            pos+=length;
            for(unsigned i=0;i<4u;i++) {
                if(!movie_iso_name(entry+33u,entry[32],names[i]))continue;
                uint32_t lba,size;
                if(toy_movie_ranges[i].count || entry[1] || entry[25] || entry[26] || entry[27] ||
                   !movie_iso_dual32(entry+2,&lba) || !movie_iso_dual32(entry+10,&size) ||
                   lba<manifest.session_lba || lba>=719850u)return false;
                uint32_t sectors=(size-1u)/KUI_GAME_DATA_BYTES+1u;
                if(sectors>719850u-lba || !movie_range_mapped(lba,sectors))return false;
                for(unsigned j=0;j<4u;j++) {
                    const struct kui_toy_movie_range *known=&toy_movie_ranges[j];
                    if(known->count && lba<known->lba+known->count && known->lba<lba+sectors &&
                       !(i/2u==j/2u && lba==known->lba && sectors==known->count))return false;
                }
                toy_movie_ranges[i]=(struct kui_toy_movie_range){lba,sectors};
            }
        }
    }
    return true;
}

static void toy_movie_scan(void) {
    if(manifest.reader!=KUI_RETAIL_READER_ASYNC ||
       native_runtime_mode()!=KUI_VMU_NATIVE_MODE_READER_ONLY ||
       manifest.ip_crc32!=KUI_VMU_LAUNCH_TOY_IP_CRC ||
       boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC ||
       exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES ||
       kui_retail_storage_acquire(&card)!=KUI_LOADER_SD_OK)return;
    bool ok=movie_iso_ranges();
#if defined(KUI_TOY_SUITE_MODE)
    const struct kui_retail_track *track=ok?toy_suite_movie_track():NULL;
    if(track) {
        if(kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&
           kui_retail_track_header_bytes(track)==16u)toy_suite_source=1u;
        else if(kui_retail_track_sector_bytes(track)==KUI_GAME_DATA_BYTES)toy_suite_source=2u;
    }
    if(toy_suite_source==1u && card.transport==KUI_STORAGE_SCI) {
#if KUI_TOY_SUITE_MODE == 1
        if(!toy_suite_witness_seed())memset(&toy_suite_seed,0,sizeof(toy_suite_seed));
#elif KUI_TOY_SUITE_MODE == 2 || KUI_TOY_SUITE_MODE == 6 || KUI_TOY_SUITE_MODE == 35
        (void)toy_suite_ram_seed(track);
#endif
    }
#endif
    enum kui_loader_sd_result stopped_stream=kui_retail_storage_stop(&card);
    kui_retail_storage_release(&card);
    image.cache_valid=0;
    if(!ok || stopped_stream!=KUI_LOADER_SD_OK) {
        memset(toy_movie_ranges,0,sizeof(toy_movie_ranges));
#if defined(KUI_TOY_SUITE_MODE)
        memset(&toy_suite_seed,0,sizeof(toy_suite_seed));
        toy_suite_source=0u;
#endif
    }
}
