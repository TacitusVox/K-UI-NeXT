/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include "miniz.h"
#include <stdlib.h>

_Static_assert(KUI_SPLASH_FRAME_BYTES==640u*480u*sizeof(uint16_t),"640x480 RGB565 frames");

/* tools/build_splash.py stores little-endian RGB565, the console's (-ml)
 * and the host tests' byte order. The zlib Adler-32 is verified, and the
 * stream must fill the frame exactly. Writes go to cached RAM only: VRAM
 * cannot take the decoder's byte stores, so callers pass a RAM buffer. */
bool kui_splash_art_inflate(const uint8_t *data,size_t bytes,uint16_t *frame) {
    if(!data || !bytes || !frame) return false;
    tinfl_decompressor *state=malloc(sizeof(*state));
    if(!state) return false;
    size_t source=bytes,destination=KUI_SPLASH_FRAME_BYTES;
    tinfl_init(state);
    tinfl_status status=tinfl_decompress(state,data,&source,(uint8_t *)frame,(uint8_t *)frame,&destination,
        TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(state);
    return status==TINFL_STATUS_DONE && source==bytes && destination==KUI_SPLASH_FRAME_BYTES;
}
uint16_t *kui_splash_art_decode(const uint8_t *data,size_t bytes) {
    uint16_t *pixels=malloc(KUI_SPLASH_FRAME_BYTES);
    if(pixels && !kui_splash_art_inflate(data,bytes,pixels)) {free(pixels);pixels=NULL;}
    return pixels;
}
