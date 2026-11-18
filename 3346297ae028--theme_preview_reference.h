/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TEST_THEME_PREVIEW_REFERENCE_H
#define KUI_TEST_THEME_PREVIEW_REFERENCE_H
#include <stdint.h>

/* Scalar oracle independent of the runtime's packed RGB565 arithmetic. */
static uint16_t reference_box565(const uint16_t *source,unsigned stride) {
    unsigned red=0,green=0,blue=0;
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++) {
        uint16_t pixel=source[y*stride+x];
        red+=pixel>>11;
        green+=(pixel>>5)&63u;
        blue+=pixel&31u;
    }
    return (uint16_t)((((red+2u)/4u)<<11)|(((green+2u)/4u)<<5)|((blue+2u)/4u));
}
#endif
