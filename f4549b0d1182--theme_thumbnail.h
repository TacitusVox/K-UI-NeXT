/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_APP_THEME_THUMBNAIL_H
#define KUI_APP_THEME_THUMBNAIL_H
#include "kui/theme.h"
#include "kui/theme_filter.h"

_Static_assert(KUI_THEME_WIDTH==2*KUI_THEME_THUMB_WIDTH &&
    KUI_THEME_HEIGHT==2*KUI_THEME_THUMB_HEIGHT &&
    KUI_THEME_HEADER_HEIGHT==2*KUI_THEME_THUMB_HEADER_HEIGHT,"preview scale");

/* FatFs reads and miniz dictionary flushes can split source scanlines.
 * Keep just the top pair of each 2x2 footprint until its bottom row arrives. */
struct theme_thumbnail {
    uint16_t *pixels;
    unsigned consumed;
    uint32_t upper[KUI_THEME_THUMB_WIDTH];
};
static inline uint32_t theme_thumbnail_pair(const uint8_t *data) {
    uint16_t a=(uint16_t)(data[0]|((uint16_t)data[1]<<8));
    uint16_t b=(uint16_t)(data[2]|((uint16_t)data[3]<<8));
    return kui_theme_rgb565_channels(a)+kui_theme_rgb565_channels(b);
}
/* Source buffers are a whole number of two-pixel pairs in little-endian order. */
static inline void theme_thumbnail_sample(struct theme_thumbnail *thumb,const uint8_t *data,unsigned bytes) {
    unsigned pairs=bytes/4u;
    while(pairs) {
        unsigned row=thumb->consumed/KUI_THEME_WIDTH;
        unsigned x=(thumb->consumed%KUI_THEME_WIDTH)/2u;
        unsigned count=KUI_THEME_THUMB_WIDTH-x;
        if(count>pairs)count=pairs;
        if((row&1u)==0) {
            for(unsigned i=0;i<count;i++)thumb->upper[x+i]=theme_thumbnail_pair(data+4u*i);
        } else {
            uint16_t *out=thumb->pixels+(row/2u)*KUI_THEME_THUMB_WIDTH+x;
            for(unsigned i=0;i<count;i++)
                out[i]=kui_theme_rgb565_from_sum4(thumb->upper[x+i]+theme_thumbnail_pair(data+4u*i));
        }
        thumb->consumed+=count*2u;
        data+=count*4u;
        pairs-=count;
    }
}
#endif
