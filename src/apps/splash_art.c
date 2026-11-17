/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include "miniz.h"
#include <stdlib.h>

_Static_assert(KUI_SPLASH_FRAME_BYTES==640u*480u*sizeof(uint16_t),"640x480 RGB565 frames");

/* tools/build_splash.py and tools/generate_shell_art.py store little-endian
 * RGB565, the console's (-ml) and the host tests' byte order. The zlib
 * Adler-32 is verified, and the stream must fill the output exactly. Writes
 * go to cached RAM only: VRAM cannot take the decoder's byte stores. */
bool kui_art_inflate(const uint8_t *data,size_t bytes,void *out,size_t out_bytes) {
    if(!data || !bytes || !out || !out_bytes) return false;
    tinfl_decompressor *state=malloc(sizeof(*state));
    if(!state) return false;
    size_t source=bytes,destination=out_bytes;
    tinfl_init(state);
    tinfl_status status=tinfl_decompress(state,data,&source,(uint8_t *)out,(uint8_t *)out,&destination,
        TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(state);
    return status==TINFL_STATUS_DONE && source==bytes && destination==out_bytes;
}
bool kui_splash_art_inflate(const uint8_t *data,size_t bytes,uint16_t *frame) {
    return kui_art_inflate(data,bytes,frame,KUI_SPLASH_FRAME_BYTES);
}
uint16_t *kui_splash_art_decode(const uint8_t *data,size_t bytes) {
    uint16_t *pixels=malloc(KUI_SPLASH_FRAME_BYTES);
    if(pixels && !kui_splash_art_inflate(data,bytes,pixels)) {free(pixels);pixels=NULL;}
    return pixels;
}
