/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/viewport.h"
#include <string.h>
/* The source column for each output column depends only on the frame width
 * and inset, so compute it once per setting instead of dividing for every
 * pixel of every frame (the SH-4 has no hardware divider). Only the drawing
 * thread calls this; the table is rebuilt whenever the setting changes. */
static uint16_t columns[640];
static unsigned columns_width,columns_inset;
void kui_viewport_inset(uint16_t *pixels,unsigned width,unsigned height,unsigned inset,uint16_t border) {
    if(!pixels || !inset || width>640 || inset>=width/2) return;
    uint16_t row[640];unsigned inner=width-2*inset;
    if(inner<2) return;
    if(columns_width!=width || columns_inset!=inset) {
        for(unsigned x=0;x<inner;x++) columns[x]=(uint16_t)(x*(width-1)/(inner-1));
        columns_width=width;columns_inset=inset;
    }
    for(unsigned y=0;y<height;y++) {
        uint16_t *out=pixels+y*width;memcpy(row,out,width*sizeof(*row));
        for(unsigned x=0;x<inset;x++) out[x]=out[width-1-x]=border;
        for(unsigned x=0;x<inner;x++) out[inset+x]=row[columns[x]];
    }
}
