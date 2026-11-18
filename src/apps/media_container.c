/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_container.h"
#include <string.h>

static unsigned le16(const uint8_t *p) {
    return p[0] | (unsigned)p[1] << 8;
}

static uint32_t le32(const uint8_t *p) {
    return le16(p) | (uint32_t)le16(p+2) << 16;
}

static bool read_exact(FIL *file, void *out, UINT length) {
    UINT got;
    return f_read(file, out, length, &got) == FR_OK && got == length;
}

bool kui_media_container_index(FIL *file, bool manual, struct kui_media_index *index) {
    uint8_t header[KUI_PREVIEW_HEADER_BYTES];
    if(!file || !index || f_lseek(file, 0) != FR_OK ||
       !read_exact(file, header, manual ? 8u : KUI_PREVIEW_HEADER_BYTES))
        return false;
    if(!manual) {
        uint32_t count = le32(header+12);
        if(memcmp(header,"KPV1",4) || le16(header+4) != KUI_PREVIEW_WIDTH ||
           le16(header+6) != KUI_PREVIEW_HEIGHT || le16(header+8) != KUI_PREVIEW_FPS ||
           le16(header+10) || !count || count > KUI_PREVIEW_FRAMES_MAX ||
           f_size(file) != KUI_PREVIEW_HEADER_BYTES + count*KUI_PREVIEW_FRAME_BYTES)
            return false;
        index->count = count;
        return true;
    }

    unsigned pages = le16(header+4);
    if(memcmp(header,"KDM1",4) || !pages || pages > KUI_MANUAL_PAGES_MAX ||
       le16(header+6) || f_size(file) > KUI_MANUAL_BYTES_MAX)
        return false;
    /* Read a contiguous table even on SCI, then decode LE explicitly. */
    uint8_t *table = (uint8_t *)index->offsets;
    unsigned bytes = (pages+1u)*4u;
    if(!read_exact(file, table, bytes)) return false;
    for(unsigned i=0; i<=pages; ++i) {
        uint32_t off = le32(table+i*4u);
        index->offsets[i] = off;
        if(i==0 && off != 8u+bytes) return false;
        if(i>0 && (off <= index->offsets[i-1] ||
                   off-index->offsets[i-1] > KUI_MANUAL_PAGE_BYTES_MAX)) return false;
    }
    if(index->offsets[pages] != f_size(file)) return false;
    index->count = pages;
    return true;
}

bool kui_preview_read_frame(FIL *file, unsigned count, unsigned frame,
                            uint16_t *pixels, bool (*cancel)(void)) {
    if(!file || !pixels || count > KUI_PREVIEW_FRAMES_MAX || frame >= count ||
       (cancel && cancel())) return false;
    FSIZE_t offset = KUI_PREVIEW_HEADER_BYTES + (FSIZE_t)frame*KUI_PREVIEW_FRAME_BYTES;
    if(f_tell(file) != offset && f_lseek(file, offset) != FR_OK) return false;
    uint8_t *out = (uint8_t *)pixels;
    UINT remaining = KUI_PREVIEW_FRAME_BYTES;
    while(remaining) {
        /* End the first chunk on a card-sector boundary. Later bursts go
         * straight into the caller's frame plane without a second buffer. */
        UINT chunk = 8192u - (UINT)(f_tell(file) % 512u);
        if(chunk > remaining) chunk = remaining;
        if(!read_exact(file, out, chunk) || (cancel && cancel())) return false;
        remaining -= chunk;
        out += chunk;
    }
    return true;
}

bool kui_media_container_check(FIL *file, bool manual) {
    struct kui_media_index index;
    return kui_media_container_index(file, manual, &index);
}

static unsigned be16(const uint8_t *p) {
    return (unsigned)p[0] << 8 | p[1];
}

static bool jpeg_page(FIL *file, uint32_t begin, uint32_t end, bool (*cancel)(void)) {
    uint8_t buf[6];
    if(end-begin<16u || f_lseek(file,begin)!=FR_OK ||
       !read_exact(file,buf,2) || buf[0]!=0xff || buf[1]!=0xd8)
        return false;
    /* Follow header marker lengths without reading the compressed scan. The
     * producer uses ordinary Poppler JPEG headers, which fit comfortably in
     * these bounds. Limit work on malformed files independently of filesize. */
    unsigned frame_components=0;
    for(unsigned segments=0; segments<256u && f_tell(file)+4u<end-2u; ++segments) {
        if(cancel && cancel()) return false;
        if(!read_exact(file,buf,1) || buf[0]!=0xff) return false;
        unsigned fill=0;
        do {
            if(++fill>64u || f_tell(file)>=end-2u || !read_exact(file,buf,1)) return false;
        } while(buf[0]==0xff);
        unsigned marker=buf[0];
        if(marker==0xd8 || marker==0xd9 || !marker || !read_exact(file,buf,2)) return false;
        unsigned len=be16(buf);
        if(len<2u || f_tell(file)+len-2u>end-2u) return false;
        FSIZE_t after=f_tell(file)+len-2u;
        if(marker==0xc0 || marker==0xc1 || marker==0xc2) {
            if(frame_components || len<8u || !read_exact(file,buf,6) || buf[0]!=8) return false;
            unsigned w=be16(buf+3), h=be16(buf+1);
            frame_components=buf[5];
            if(!w || w>KUI_MANUAL_EDGE_MAX || !h || h>KUI_MANUAL_EDGE_MAX ||
               (frame_components!=1 && frame_components!=3 && frame_components!=4) ||
               len!=8u+frame_components*3u) return false;
        } else if(marker==0xda) {
            if(len<8u || !read_exact(file,buf,1) || !buf[0] || buf[0]>frame_components ||
               len!=6u+buf[0]*2u || after>=end-2u) return false;
            return f_lseek(file,end-2u)==FR_OK && read_exact(file,buf,2) && buf[0]==0xff && buf[1]==0xd9;
        }
        if(f_lseek(file,after)!=FR_OK) return false;
    }
    return false;
}

bool kui_manual_check_pages(FIL *file, bool (*cancel)(void)) {
    struct kui_media_index index;
    if(!kui_media_container_index(file,true,&index)) return false;
    for(unsigned i=0; i<index.count; ++i)
        if(!jpeg_page(file,index.offsets[i],index.offsets[i+1],cancel)) return false;
    return true;
}
