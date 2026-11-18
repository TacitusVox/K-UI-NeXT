/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include "theme_thumbnail.h"
#include "miniz.h"
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

uint16_t *kui_splash_thumbnail_decode(bool factory,bool (*cancelled)(void)) {
    if(cancelled && cancelled())return NULL;
    const uint8_t *source=factory?kui_factory_splash_art:kui_splash_art;
    size_t remaining=factory?sizeof(kui_factory_splash_art):sizeof(kui_splash_art);
    uint16_t *pixels=malloc(KUI_THEME_THUMB_WIDTH*KUI_THEME_THUMB_HEIGHT*sizeof(*pixels));
    tinfl_decompressor *decoder=malloc(sizeof(*decoder));
    uint8_t *window=malloc(TINFL_LZ_DICT_SIZE);
    struct theme_thumbnail thumb={.pixels=pixels};
    bool ok=false;
    if(pixels && decoder && window) {
        tinfl_init(decoder);
        for(;;) {
            size_t input=remaining,output=TINFL_LZ_DICT_SIZE;
            tinfl_status status=tinfl_decompress(decoder,source,&input,window,window,&output,
                TINFL_FLAG_PARSE_ZLIB_HEADER);
            if((cancelled && cancelled()) || (output&3u) || output>KUI_SPLASH_FRAME_BYTES-thumb.consumed*2u)break;
            theme_thumbnail_sample(&thumb,window,(unsigned)output);
            source+=input;
            remaining-=input;
            if(status==TINFL_STATUS_DONE) {
                ok=!remaining && thumb.consumed*2u==KUI_SPLASH_FRAME_BYTES;
                break;
            }
            if(status!=TINFL_STATUS_HAS_MORE_OUTPUT || output!=TINFL_LZ_DICT_SIZE)break;
        }
    }
    free(decoder);free(window);
    if(!ok) {free(pixels);return NULL;}
    return pixels;
}
