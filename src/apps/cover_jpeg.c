/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/cover_jpeg.h"
#include "kui/compact_cover.h"
#include "kui/cover_image.h"
#include "../../third_party/tjpgd/tjpgd.h"
#include <stdlib.h>
#include <string.h>

struct input {
    FIL *file;
    kui_cancel_fn cancel;
    uint8_t *rgb;
    unsigned width, height;
    bool io_error;
};
static size_t input_bytes(JDEC *jd, uint8_t *dest, size_t count) {
    struct input *in = jd->device;
    if(in->cancel && in->cancel()) { in->io_error = true; return 0; }
    FSIZE_t remaining = f_size(in->file) - f_tell(in->file);
    if(!dest) {
        if(count > remaining) return 0;
        if(f_lseek(in->file,f_tell(in->file)+count) != FR_OK) { in->io_error = true; return 0; }
        return count;
    }
    if(count > remaining) count = (size_t)remaining;
    UINT got = 0;
    if(f_read(in->file,dest,(UINT)count,&got) != FR_OK || got != count) in->io_error = true;
    return in->io_error ? 0 : got;
}
static int output_block(JDEC *jd, void *rgb, JRECT *r) {
    struct input *in = jd->device;
    if((in->cancel && in->cancel()) || r->left > r->right || r->top > r->bottom ||
       r->right >= in->width || r->bottom >= in->height) return 0;
    const uint8_t *src = rgb;
    size_t bytes = (r->right-r->left+1u)*3u;
    for(unsigned y = r->top; y <= r->bottom; ++y) {
        memcpy(in->rgb+3u*((size_t)y*in->width+r->left),src,bytes);
        src += bytes;
    }
    return 1;
}

/* Identify the frame before handing a small progressive or baseline image
 * back to stb. JPEG comments/EXIF may precede SOF and can exceed 64 KiB in
 * aggregate, so skip them on the card instead of loading a header buffer. */
static enum kui_cover_rows_result frame_info(struct input *in, unsigned *w,unsigned *h,bool *baseline) {
    FIL *f = in->file;
    uint8_t b[6];
    JDEC io = {.device = in};
    if(f_size(f) < 8) return KUI_COVER_ROWS_INVALID;
    if(f_lseek(f,f_size(f)-2) != FR_OK) return KUI_COVER_ROWS_ERROR;
    if(input_bytes(&io,b,2) != 2 || b[0] != 0xff || b[1] != 0xd9) return KUI_COVER_ROWS_INVALID;
    if(f_lseek(f,0) != FR_OK) return KUI_COVER_ROWS_ERROR;
    if(input_bytes(&io,b,2) != 2 || b[0] != 0xff || b[1] != 0xd8) return KUI_COVER_ROWS_INVALID;
    while(f_tell(f) < f_size(f)) {
        if(input_bytes(&io,b,1) != 1 || b[0] != 0xff) return KUI_COVER_ROWS_INVALID;
        do { if(input_bytes(&io,b,1) != 1) return KUI_COVER_ROWS_INVALID; } while(b[0] == 0xff);
        uint8_t marker = b[0];
        if(!marker || (marker >= 0xd0 && marker <= 0xda) || input_bytes(&io,b,2) != 2)
            return KUI_COVER_ROWS_INVALID;
        size_t len = (size_t)b[0]*256+b[1];
        if(len < 2 || len-2 > f_size(f)-f_tell(f)) return KUI_COVER_ROWS_INVALID;
        if(marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
            if(len < 11 || input_bytes(&io,b,sizeof(b)) != sizeof(b) || b[0] != 8 ||
               len != 8u+3u*b[5]) return KUI_COVER_ROWS_INVALID;
            *h = (unsigned)b[1]*256+b[2]; *w = (unsigned)b[3]*256+b[4]; *baseline = marker == 0xc0;
            return *w && *h && *w <= KUI_INTERNET_COVER_MAX_EDGE && *h <= KUI_INTERNET_COVER_MAX_EDGE ?
                KUI_COVER_ROWS_OK : KUI_COVER_ROWS_INVALID;
        }
        if(f_lseek(f,f_tell(f)+len-2) != FR_OK) return KUI_COVER_ROWS_ERROR;
    }
    return KUI_COVER_ROWS_INVALID;
}

enum kui_cover_rows_result kui_cover_jpeg_scaled(FIL *source, uint16_t *planes, bool small,
                                                  kui_cancel_fn cancel) {
    struct input in = {.file=source,.cancel=cancel};
    bool baseline = false;
    unsigned w=0,h=0;
    enum kui_cover_rows_result result = frame_info(&in,&w,&h,&baseline);
    if(in.io_error) return KUI_COVER_ROWS_ERROR;
    if(result != KUI_COVER_ROWS_OK) return result;
    if(small && ((w < KUI_COVER_LARGE && h < KUI_COVER_LARGE) || w > 256 || h > 256))
        return KUI_COVER_ROWS_INVALID;
    if((uint64_t)w*h <= KUI_COVER_IMAGE_MAX_PIXELS && f_size(source) <= KUI_COVER_IMAGE_MAX_BYTES)
        return KUI_COVER_ROWS_FALLBACK;
    if(small || !baseline) return KUI_COVER_ROWS_INVALID;

    /* The long edge stays at least as large as the Games display where
     * possible. At 4K this produces at most 512x512 RGB before final scaling. */
    unsigned shift = 0, longest = w > h ? w : h;
    while(shift < 3 && (longest >> (shift+1)) >= KUI_COVER_LARGE &&
          (w >> (shift+1)) && (h >> (shift+1))) ++shift;
    in.width=w>>shift; in.height=h>>shift;
    if((uint64_t)in.width*in.height > 512u*512u) return KUI_COVER_ROWS_INVALID;
    if(f_lseek(source,0) != FR_OK) return KUI_COVER_ROWS_ERROR;
    JDEC jd;
    unsigned pool_bytes = 16384;
    void *pool = malloc(pool_bytes);
    if(!pool) return KUI_COVER_ROWS_ERROR;
    JRESULT status = jd_prepare(&jd,input_bytes,pool,pool_bytes,&in);
    result = KUI_COVER_ROWS_INVALID;
    if(status == JDR_OK && jd.width == w && jd.height == h) {
        in.rgb = malloc(3u*in.width*in.height);
        if(!in.rgb) result = KUI_COVER_ROWS_ERROR;
        else {
            status = jd_decomp(&jd,output_block,(uint8_t)shift);
            if(status == JDR_OK) {
                result = KUI_COVER_ROWS_OK;
                for(enum kui_cover_size i=KUI_COVER_SIZE_LARGE; i<=KUI_COVER_SIZE_SMALL; ++i) {
                    if((cancel && cancel()) || !kui_cover_scale(in.rgb,in.width,in.height,3,planes,kui_cover_edge(i))) {
                        result = KUI_COVER_ROWS_ERROR;
                        break;
                    }
                    planes += kui_cover_edge(i)*kui_cover_edge(i);
                }
            }
        }
    }
    free(in.rgb);
    free(pool);
    return in.io_error || (cancel && cancel()) ? KUI_COVER_ROWS_ERROR : result;
}
