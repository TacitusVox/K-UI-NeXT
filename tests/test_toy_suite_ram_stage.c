/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/native_readahead_layout.h"
#include "kui/retail_image.h"
#include "kui/toy_movie_trace.h"
#include "retail_storage.h"
#include <assert.h>
#include <string.h>

static struct kui_retail_manifest manifest;
static struct kui_retail_storage card;
static struct kui_toy_movie_range toy_movie_ranges[4];
static struct kui_toy_suite_seed toy_suite_seed;
static uint8_t toy_suite_data[KUI_NATIVE_RA_DATA_BYTES];
static uint8_t original[KUI_NATIVE_RA_DATA_BYTES];
static uint32_t first_block,reads,fail_at;

enum kui_loader_sd_result kui_retail_storage_read_run(struct kui_retail_storage *storage,
    uint32_t lba,uint32_t available,uint8_t out[512]) {
    assert(storage==&card);
    if(++reads==fail_at)return KUI_LOADER_SD_CRC;
    for(unsigned i=2u;i<=3u;i++) {
        const struct kui_retail_extent *extent=&manifest.slots[i].extent;
        uint32_t offset=lba-extent->card_lba;
        if(offset<extent->blocks) {
            assert(available<=extent->blocks-offset);
            memcpy(out,original+(extent->file_block+offset-first_block)*512u,512u);
            return KUI_LOADER_SD_OK;
        }
    }
    assert(false);
    return KUI_LOADER_SD_ARGUMENT;
}

#include "toy_suite_ram_stage.inc"

static void raw_sector(uint8_t *sector,uint32_t lba) {
    memset(sector,0,KUI_GAME_RAW_BYTES);
    memset(sector+1u,0xff,10u);
    uint32_t fad=lba+150u;
    unsigned fields[3]={fad/4500u,(fad/75u)%60u,fad%75u};
    for(unsigned i=0u;i<3u;i++)sector[12u+i]=(uint8_t)((fields[i]/10u)*16u+fields[i]%10u);
    sector[15]=1u;
    for(unsigned i=16u;i<16u+KUI_GAME_DATA_BYTES;i++)sector[i]=(uint8_t)(lba+i);
    uint32_t edc=kui_cd_edc(sector,16u+KUI_GAME_DATA_BYTES);
    for(unsigned i=0u;i<4u;i++)sector[16u+KUI_GAME_DATA_BYTES+i]=(uint8_t)(edc>>(8u*i));
}

int main(void) {
#if KUI_TOY_SUITE_MODE == 35
    const uint32_t first=0x69263u-40u,last=0x69263u+40u;
    const unsigned movie=1u;
#else
    const uint32_t center=KUI_TOY_SUITE_MODE==2?469080u:468128u;
    const uint32_t first=center-40u,last=center+40u;
    const unsigned movie=3u;
#endif
    const uint32_t track_start=45000u;
    const uint32_t first_byte=(first-track_start)*KUI_GAME_RAW_BYTES+17u;
    first_block=first_byte/512u;
    uint32_t blocks=((last-track_start)*KUI_GAME_RAW_BYTES+17u+511u)/512u-first_block;
    manifest.slots[0].track=(struct kui_retail_track){track_start,last+100u,1u|(17u<<8),3u,4u};
    manifest.slots[1].extent=(struct kui_retail_extent){0u,1000u,first_block};
    manifest.slots[2].extent=(struct kui_retail_extent){first_block,2000000u,111u};
    manifest.slots[3].extent=(struct kui_retail_extent){first_block+111u,3000000u,blocks-111u};
#if KUI_TOY_SUITE_MODE == 35
    toy_movie_ranges[movie]=(struct kui_toy_movie_range){first-386u,386u+last-first};
#else
    toy_movie_ranges[movie]=(struct kui_toy_movie_range){452810u,26208u};
#endif
    for(uint32_t n=0u;n<last-first;n++)
        raw_sector(original+(first_byte&511u)+n*KUI_GAME_RAW_BYTES,first+n);
    const struct kui_retail_track *track=&manifest.slots[0].track;
    assert(toy_suite_ram_seed(track));
    assert(toy_suite_seed.base[0]==2000000u && toy_suite_seed.count[0]==111u);
    assert(toy_suite_seed.base[1]==3000000u && toy_suite_seed.count[1]==blocks-111u);
    assert(reads==blocks && !memcmp(toy_suite_data,original,blocks*512u));

    memset(&toy_suite_seed,0,sizeof(toy_suite_seed));
    reads=0u;fail_at=137u;
    assert(!toy_suite_ram_seed(track) && !toy_suite_seed.count[0] && reads==fail_at);
    reads=fail_at=0u;original[(first_byte&511u)+31u]^=1u;
    assert(!toy_suite_ram_seed(track) && !toy_suite_seed.count[0] && reads==blocks);
    reads=0u;--toy_movie_ranges[movie].count;
    assert(!toy_suite_ram_seed(track) && !reads);
#if KUI_TOY_SUITE_MODE == 35
    ++toy_movie_ranges[movie].count;
    ++toy_movie_ranges[movie].lba;
    assert(!toy_suite_ram_seed(track) && !reads);
    --toy_movie_ranges[movie].lba;

    /* A window that crosses more than two card extents is never published. */
    ++manifest.slots[0].track.extent_count;
    --manifest.slots[3].extent.blocks;
    manifest.slots[4].extent=(struct kui_retail_extent){first_block+blocks-1u,4000000u,1u};
    assert(!toy_suite_ram_seed(track) && !reads && !toy_suite_seed.count[0]);
#endif
}
