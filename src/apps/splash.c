/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include <stdlib.h>
#include <string.h>
#include "../../build/splash_art.inc"

/* The startup art is stored compressed in the runtime and inflated once on
 * its first frame. Later splash frames copy that RAM copy, as the original
 * raw array was copied; kui_splash_release returns the RAM after the splash.
 * Without RAM for it the frame is black rather than an unchecked decode. */
static uint16_t *splash;
static bool splash_tried;
void kui_splash_draw(uint16_t *frame) {
    if(!frame) return;
    if(!splash && !splash_tried) {
        splash_tried=true;splash=kui_splash_art_decode(kui_splash_art,sizeof(kui_splash_art));
    }
    if(splash) memcpy(frame,splash,KUI_SPLASH_FRAME_BYTES);
    else for(unsigned i=0;i<640u*480u;i++) frame[i]=0;
}
void kui_splash_release(void) {
    free(splash);splash=NULL;splash_tried=false;
}

uint16_t *kui_splash_preview_decode(void) {
    return kui_splash_art_decode(kui_splash_art,sizeof(kui_splash_art));
}
