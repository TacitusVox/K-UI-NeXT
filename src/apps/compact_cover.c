/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/compact_cover.h"
#include "kui/cover_image.h"
#include "kui/cover_png_rows.h"
#include "kui/cover_jpeg.h"
#include "kui/hash.h"
#include <stdlib.h>
#include <string.h>

static uint32_t be32(const uint8_t *b) {
    return (uint32_t)b[0]<<24 | (uint32_t)b[1]<<16 | (uint32_t)b[2]<<8 | (uint32_t)b[3];
}
static uint32_t le32(const uint8_t *b) {
    return (uint32_t)b[3]<<24 | (uint32_t)b[2]<<16 | (uint32_t)b[1]<<8 | (uint32_t)b[0];
}
static bool complete_png(const uint8_t *data, size_t size) {
    if(size < 33 || memcmp(data, "\211PNG\r\n\032\n\0\0\0\rIHDR", 16)) return false;
    size_t at = 8;
    while(size-at >= 12) {
        uint32_t n = be32(data+at);
        if(n > size-at-12 || kui_crc32_capture(0, data+at+4, n+4) != be32(data+at+8+n)) return false;
        if(!memcmp(data+at+4, "IEND", 4)) return !n && at+12 == size;
        at += n+12;
    }
    return false;
}
static bool read_exact(FIL *file, void *data, UINT size) {
    UINT read = 0;
    return f_read(file, data, size, &read) == FR_OK && read == size;
}
static size_t plane_offset(enum kui_cover_size size) {
    return kui_cover_offset(size) - KUI_COVER_HEADER_BYTES;
}

static enum kui_compact_result decode(FIL *source, bool small_jpeg, uint16_t *out, kui_cancel_fn cancel) {
    if(f_size(source) < 8 || f_size(source) > KUI_INTERNET_COVER_MAX_BYTES) return KUI_COMPACT_INVALID;
    size_t bytes = (size_t)f_size(source);
    uint8_t sig[8];
    if(f_lseek(source,0) != FR_OK || !read_exact(source,sig,sizeof(sig)) || f_lseek(source,0) != FR_OK)
        return KUI_COMPACT_ERROR;
    bool jpeg = sig[0] == 0xff && sig[1] == 0xd8 && sig[2] == 0xff;
    if((small_jpeg && !jpeg) || (!jpeg && memcmp(sig,"\211PNG\r\n\032\n",8))) return KUI_COMPACT_INVALID;
    enum kui_cover_rows_result r = jpeg ? kui_cover_jpeg_scaled(source,out,small_jpeg,cancel) :
        kui_cover_png_rows(source,out,cancel);
    if(r == KUI_COVER_ROWS_OK) return KUI_COMPACT_OK;
    if(r == KUI_COVER_ROWS_INVALID) return KUI_COMPACT_INVALID;
    if(r != KUI_COVER_ROWS_FALLBACK || f_lseek(source,0) != FR_OK) return KUI_COMPACT_ERROR;
    if(bytes > KUI_COVER_IMAGE_MAX_BYTES) return KUI_COMPACT_INVALID;
    uint8_t *data = malloc(bytes), *pixels = NULL;
    if(!data) return KUI_COMPACT_ERROR;
    enum kui_compact_result result = KUI_COMPACT_ERROR;
    if(read_exact(source,data,(UINT)bytes)) {
        bool valid = jpeg ? bytes >= 4 && data[0] == 0xff && data[1] == 0xd8 &&
            data[bytes-2] == 0xff && data[bytes-1] == 0xd9 : complete_png(data,bytes);
        unsigned w = 0, h = 0, channels = 0;
        enum kui_cover_image_status status = valid ?
            kui_cover_image_decode(data,bytes,&pixels,&w,&h,&channels) : KUI_COVER_IMAGE_CORRUPT;
        free(data);
        if(status != KUI_COVER_IMAGE_OK)
            return status == KUI_COVER_IMAGE_MEMORY ? KUI_COMPACT_ERROR : KUI_COMPACT_INVALID;
        if(w > KUI_INTERNET_COVER_MAX_EDGE || h > KUI_INTERNET_COVER_MAX_EDGE) {
            kui_cover_image_free(pixels);
            return KUI_COMPACT_INVALID;
        }
        kui_cover_reduce(pixels,&w,&h,channels);
        result = KUI_COMPACT_OK;
        for(enum kui_cover_size i=KUI_COVER_SIZE_LARGE; i<=KUI_COVER_SIZE_SMALL; ++i) {
            if((cancel && cancel()) || !kui_cover_scale(pixels,w,h,channels,
                    out + plane_offset(i)/2, kui_cover_edge(i))) {
                result = KUI_COMPACT_ERROR;
                break;
            }
        }
        kui_cover_image_free(pixels);
        return result;
    }
    free(data);
    return result;
}

enum kui_compact_result kui_compact_cover_encode(FIL *source, FIL *dest, bool small_jpeg, kui_cancel_fn cancel) {
    uint8_t *file = malloc(KUI_COMPACT_COVER_BYTES);
    if(!file) return KUI_COMPACT_ERROR;
    uint16_t *planes = (uint16_t *)(file + KUI_COMPACT_COVER_HEADER);
    enum kui_compact_result result = decode(source, small_jpeg, planes, cancel);
    if(result == KUI_COMPACT_OK) {
        memcpy(file,"KCT1",4);
        for(enum kui_cover_size i=KUI_COVER_SIZE_LARGE; i<=KUI_COVER_SIZE_SMALL; ++i) {
            size_t offset = plane_offset(i);
            uint8_t *p = file + KUI_COMPACT_COVER_HEADER + offset;
            unsigned count = kui_cover_edge(i) * kui_cover_edge(i);
            for(unsigned j=0; j<count; ++j) {
                uint16_t pixel = planes[offset/2 + j];
                p[2*j] = (uint8_t)pixel;
                p[2*j+1] = (uint8_t)(pixel >> 8);
            }
            uint32_t crc = kui_crc32_capture(0,p,2*count);
            for(unsigned j=0; j<4; ++j) file[4+4*i+j] = (uint8_t)(crc >> (8*j));
        }
        UINT written;
        if((cancel && cancel()) || f_lseek(dest,0) != FR_OK ||
           f_write(dest,file,KUI_COMPACT_COVER_BYTES,&written) != FR_OK ||
           written != KUI_COMPACT_COVER_BYTES ||
           (f_size(dest) != written && f_truncate(dest) != FR_OK)) result = KUI_COMPACT_ERROR;
    }
    free(file);
    return result;
}
static bool read_header(FIL *file, uint8_t header[KUI_COMPACT_COVER_HEADER]) {
    return f_size(file) == KUI_COMPACT_COVER_BYTES && f_lseek(file,0) == FR_OK &&
        read_exact(file,header,KUI_COMPACT_COVER_HEADER) && !memcmp(header,"KCT1",4);
}
bool kui_compact_cover_pixels(FIL *file, enum kui_cover_size size, uint16_t *pixels) {
    uint8_t header[KUI_COMPACT_COVER_HEADER];
    if((unsigned)size > KUI_COVER_SIZE_SMALL || !read_header(file,header)) return false;
    unsigned count = kui_cover_edge(size)*kui_cover_edge(size);
    uint8_t *b = (uint8_t *)pixels;
    if(f_lseek(file,KUI_COMPACT_COVER_HEADER + plane_offset(size)) != FR_OK ||
        !read_exact(file,b,2u*count) || kui_crc32_capture(0,b,2u*count) != le32(header+4+4*size)) return false;
    for(unsigned j=0; j<count; ++j) pixels[j] = (uint16_t)(b[2*j] | b[2*j+1] << 8);
    return true;
}
enum kui_compact_result kui_compact_cover_verify_digest(FIL *file,kui_cancel_fn cancel,uint8_t digest[32]) {
    struct kui_sha256 sha;
    uint8_t header[KUI_COMPACT_COVER_HEADER], block[2048];
    if(f_size(file) != KUI_COMPACT_COVER_BYTES) return KUI_COMPACT_INVALID;
    if(f_lseek(file,0) != FR_OK || !read_exact(file,header,sizeof(header))) return KUI_COMPACT_ERROR;
    if(memcmp(header,"KCT1",4)) return KUI_COMPACT_INVALID;
    if(digest) { kui_sha256_init(&sha); kui_sha256_update(&sha,header,sizeof(header)); }
    for(enum kui_cover_size i=KUI_COVER_SIZE_LARGE; i<=KUI_COVER_SIZE_SMALL; ++i) {
        unsigned left = 2*kui_cover_edge(i)*kui_cover_edge(i);
        uint32_t crc = 0;
        while(left) {
            UINT n = left < sizeof(block) ? left : sizeof(block);
            if((cancel && cancel()) || !read_exact(file,block,n)) return KUI_COMPACT_ERROR;
            crc = kui_crc32_capture(crc,block,n);
            if(digest) kui_sha256_update(&sha,block,n);
            left -= n;
        }
        if(crc != le32(header+4+4*i)) return KUI_COMPACT_INVALID;
    }
    if(digest) kui_sha256_digest(&sha,digest);
    return KUI_COMPACT_OK;
}
enum kui_compact_result kui_compact_cover_verify(FIL *file,kui_cancel_fn cancel) {
    return kui_compact_cover_verify_digest(file,cancel,NULL);
}
