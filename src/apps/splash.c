/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include <stdlib.h>
#include <string.h>
#include "../../build/splash_art.inc"
#include "../../build/factory_splash_art.inc"

/* The startup art is stored compressed in the runtime and inflated once on
 * its first frame. Later splash frames copy that RAM copy, as the original
 * raw array was copied; kui_splash_release returns the RAM after the splash.
 * Without RAM for it the frame is black rather than an unchecked decode. */
static uint16_t *splash;
static const uint8_t *cached_source;
static void draw_embedded(uint16_t *frame,const uint8_t *source,size_t size) {
    if(!frame) return;
    if(cached_source!=source) {
        free(splash);
        cached_source=source;
        splash=kui_splash_art_decode(source,size);
    }
    if(splash) memcpy(frame,splash,KUI_SPLASH_FRAME_BYTES);
    else for(unsigned i=0;i<640u*480u;i++) frame[i]=0;
}
void kui_splash_draw(uint16_t *frame) {
    draw_embedded(frame,kui_splash_art,sizeof(kui_splash_art));
}
void kui_splash_draw_factory(uint16_t *frame) {
    draw_embedded(frame,kui_factory_splash_art,sizeof(kui_factory_splash_art));
}
void kui_splash_release(void) {
    free(splash);splash=NULL;cached_source=NULL;
}

uint16_t *kui_splash_preview_decode(void) {
    return kui_splash_art_decode(kui_splash_art,sizeof(kui_splash_art));
}

uint16_t *kui_splash_preview_factory_decode(void) {
    return kui_splash_art_decode(kui_factory_splash_art,sizeof(kui_factory_splash_art));
}
