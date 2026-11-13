/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hash.h"
#include "kui/toy_ram_window.h"
/* Load a narrow original-data window while the cold stage owns the card. */
static bool toy_suite_ram_seed(const struct kui_retail_track *track) {
#if KUI_TOY_SUITE_MODE == 35
    const struct kui_toy_movie_range *movie=&toy_movie_ranges[KUI_TOY_RAM_WINDOW_VARIANT];
    const uint32_t first=KUI_TOY_RAM_WINDOW_FIRST,last=KUI_TOY_RAM_WINDOW_END;
    if(first<movie->lba+386u || last>movie->lba+movie->count)return false;
#else
    const struct kui_toy_movie_range *movie=&toy_movie_ranges[3];
    uint32_t center=KUI_TOY_SUITE_MODE==2?469080u:468128u;
    uint32_t first=center-40u,last=center+40u;
    if(movie->lba!=452810u || movie->count!=26208u ||
       first<movie->lba || last>movie->lba+movie->count)return false;
#endif
    uint32_t offset=kui_retail_track_file_offset(track);
    uint32_t first_byte=offset+(first-track->start_lba)*KUI_GAME_RAW_BYTES;
    uint32_t block=first_byte/512u;
    uint32_t end=(offset+(last-track->start_lba)*KUI_GAME_RAW_BYTES+511u)/512u;
    if(end-block>KUI_NATIVE_RA_DATA_BYTES/512u)return false;

    /* The checked manifest maps each physical run; the window may cross
     * one allocation boundary even when the two card runs are fragmented. */
    struct kui_toy_suite_seed seed={{0},{0}};
    uint32_t extent=kui_retail_track_first_extent(track),used=0u;
    for(unsigned i=0u;i<track->extent_count && block<end;i++) {
        const struct kui_retail_extent *e=&manifest.slots[extent+i].extent;
        if(block<e->file_block || block-e->file_block>=e->blocks)continue;
        if(used==2u)return false;
        uint32_t count=e->blocks-(block-e->file_block);
        if(count>end-block)count=end-block;
        seed.base[used]=e->card_lba+block-e->file_block;
        seed.count[used++]=count;
        block+=count;
    }
    if(block!=end)return false;

    uint8_t *out=toy_suite_data;
    for(unsigned i=0u;i<used;i++) {
        for(uint32_t j=0u;j<seed.count[i];j++,out+=512u) {
            if(kui_retail_storage_read_run(&card,seed.base[i]+j,seed.count[i]-j,out)!=
               KUI_LOADER_SD_OK)return false;
        }
    }
    const uint8_t *raw=toy_suite_data+(first_byte&511u);
    for(uint32_t sector=first;sector<last;sector++,raw+=KUI_GAME_RAW_BYTES) {
        if(raw[15]!=1u || !kui_sector_edc_valid_at(raw,sector+150u))return false;
    }
    toy_suite_seed=seed;
    return true;
}
