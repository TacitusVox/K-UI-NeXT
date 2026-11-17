/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include <stdlib.h>
#include <string.h>
#include "../../build/boot_splash_art.inc"

/* The bootstrap keeps its backdrop for its whole life, so the compressed
 * art is inflated once into RAM on first use. Pixels and dimming are as for
 * the former raw array; without RAM for it the backdrop is black. */
static uint16_t *boot_art;
static bool boot_art_tried;
static const uint16_t *art(void) {
    if(!boot_art && !boot_art_tried) {
        boot_art_tried=true;
        boot_art=kui_splash_art_decode(kui_boot_splash_art,sizeof(kui_boot_splash_art));
    }
    return boot_art;
}
void kui_boot_splash_draw(uint16_t *frame) {
    if(!frame) return;
    const uint16_t *pixels=art();
    if(pixels) memcpy(frame,pixels,KUI_SPLASH_FRAME_BYTES);
    else for(unsigned i=0;i<640u*480u;i++) frame[i]=0;
}
/* Every menu frame needs the same dimmed backdrop. It is built once from
 * the RAM copy and then copied, so a status repaint no longer recomputes
 * 307,200 pixels; without RAM for it, dimming is computed per frame. */
static uint16_t *boot_dimmed;
static bool boot_dimmed_tried;
void kui_boot_splash_draw_dimmed(uint16_t *frame) {
    if(!frame) return;
    const uint16_t *pixels=art();
    if(pixels && !boot_dimmed && !boot_dimmed_tried) {
        boot_dimmed_tried=true;
        if((boot_dimmed=malloc(KUI_SPLASH_FRAME_BYTES)))
            for(unsigned i=0;i<640u*480u;i++) boot_dimmed[i]=(uint16_t)((pixels[i]&0xe79cu)>>2);
    }
    if(boot_dimmed) {memcpy(frame,boot_dimmed,KUI_SPLASH_FRAME_BYTES);return;}
    /* Read the RAM copy, not the uncached framebuffer: the original RGB565
     * dimming without a VRAM readback pass. */
    for(unsigned i=0;i<640u*480u;i++)
        frame[i]=pixels?(uint16_t)((pixels[i]&0xe79cu)>>2):0;
}
