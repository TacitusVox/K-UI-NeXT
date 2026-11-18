/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_APP_THEME_THUMBNAIL_H
#define KUI_APP_THEME_THUMBNAIL_H
#include "kui/theme.h"

/* Both a FatFs read and a miniz dictionary flush can split a scanline.
 * Only the destination cursor and the input pixel count cross chunks. */
struct theme_thumbnail {
    uint16_t *pixels;
    unsigned height,x,y,consumed;
};
static inline void theme_thumbnail_sample(struct theme_thumbnail *thumb,
        const uint8_t *data,unsigned bytes) {
    unsigned end=thumb->consumed+bytes/2u;
    while(thumb->y<thumb->height) {
        unsigned row=(thumb->y*5u/2u)*KUI_THEME_WIDTH;
        uint16_t *dest=thumb->pixels+thumb->y*KUI_THEME_THUMB_WIDTH;
        while(thumb->x<KUI_THEME_THUMB_WIDTH) {
            unsigned pos=row+thumb->x*5u/2u;
            if(pos>=end) {
                thumb->consumed=end;
                return;
            }
            unsigned at=(pos-thumb->consumed)*2u;
            dest[thumb->x++]=(uint16_t)(data[at]|((uint16_t)data[at+1]<<8));
        }
        thumb->y++;
        thumb->x=0;
    }
    thumb->consumed=end;
}
#endif
