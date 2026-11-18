/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_THEME_FILTER_H
#define KUI_THEME_FILTER_H
#include <stdint.h>

/* Each channel gets a byte, so even four maximum RGB565 values can be
 * added and rounded without a carry crossing into an adjacent channel. */
static inline uint32_t kui_theme_rgb565_channels(uint16_t pixel) {
    return ((uint32_t)(pixel&0xf800u)<<5) | ((uint32_t)(pixel&0x07e0u)<<3) | (pixel&0x001fu);
}
static inline uint16_t kui_theme_rgb565_from_sum4(uint32_t sum) {
    sum+=0x020202u;
    return (uint16_t)(((sum>>7)&0xf800u) | ((sum>>5)&0x07e0u) | ((sum>>2)&0x001fu));
}
static inline uint16_t kui_theme_rgb565_average4(uint16_t a,uint16_t b,uint16_t c,uint16_t d) {
    return kui_theme_rgb565_from_sum4(kui_theme_rgb565_channels(a)+kui_theme_rgb565_channels(b)+
        kui_theme_rgb565_channels(c)+kui_theme_rgb565_channels(d));
}
#endif
