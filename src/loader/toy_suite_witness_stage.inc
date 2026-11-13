/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hash.h"
#include "kui/toy_suite_witness.h"

_Static_assert(2u*KUI_TOY_WITNESS_SECTORS*KUI_GAME_DATA_BYTES==sizeof(toy_suite_data),
    "Both complete witness windows must fit the existing arena");

/* SCI is already owned by the cold stage. Validate both raw on-card
 * witnesses before publishing either window to the runtime. The on-card
 * EDC is independent of the SPI transfer CRC and GD output copy. */
static bool toy_suite_witness_seed(void) {
    const struct kui_toy_movie_range *movie=&toy_movie_ranges[3];
    /* Latest original-GDI runs last completed requests at these exclusive
     * LBAs. Sample the preceding 40 sectors and the following eight. */
    static const uint32_t base[2]={UINT32_C(0x724a0)-40u,UINT32_C(0x72858)-40u};
    uint8_t *out=toy_suite_data;
    for(unsigned window=0u;window<2u;window++) {
        uint32_t first=base[window];
        if(first<movie->lba || first-movie->lba>=movie->count ||
           KUI_TOY_WITNESS_SECTORS>movie->count-(first-movie->lba))return false;
        for(unsigned at=0u;at<KUI_TOY_WITNESS_SECTORS;) {
            unsigned count=KUI_TOY_WITNESS_SECTORS-at;
            const unsigned capacity=sizeof(raw_boot)/KUI_GAME_RAW_BYTES;
            if(count>capacity)count=capacity;
            if(kui_retail_image_read(&image,first+at,count,KUI_GAME_SECTOR_RAW,
                 raw_boot,count*KUI_GAME_RAW_BYTES)!=KUI_GAME_OK)return false;
            for(unsigned i=0u;i<count;i++) {
                const uint8_t *raw=raw_boot+i*KUI_GAME_RAW_BYTES;
                if(raw[15]!=1u || !kui_sector_edc_valid_at(raw,first+at+i+150u))return false;
                memcpy(out,raw+16u,KUI_GAME_DATA_BYTES);
                out+=KUI_GAME_DATA_BYTES;
            }
            at+=count;
        }
    }
    for(unsigned i=0u;i<2u;i++) {
        toy_suite_seed.base[i]=base[i];
        toy_suite_seed.count[i]=KUI_TOY_WITNESS_SECTORS;
    }
    return true;
}
