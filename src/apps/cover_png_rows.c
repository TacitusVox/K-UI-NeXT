/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/cover_png_rows.h"
#include "kui/compact_cover.h"
#include "kui/game_cover.h"
#include "kui/hash.h"
#ifndef MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ZLIB_APIS
#endif
#include "../../third_party/miniz/miniz.h"
#include <stdlib.h>
#include <string.h>

struct view {
    uint16_t *pixels;
    unsigned edge, width, height, next;
};

/* 4096 -> 2048 -> 1024 -> 512. One pending source row per step. */
struct half_row {
    uint8_t *saved;
    unsigned width;
    bool pending;
};

struct png {
    tinfl_decompressor inflater;
    uint8_t history[TINFL_LZ_DICT_SIZE], input[4096];
    uint8_t *rows, *source_rows, *reduced_row, *expanded;
    uint8_t palette[256][4];
    uint16_t key[3];
    unsigned palette_count, depth, color, filter_bpp;
    bool key_present;
    struct half_row half[4];
    unsigned levels, sample_y, sample_w, sample_h, sample_stride;
    struct view views[3];
    unsigned width, height, channels, stride, slots;
    unsigned history_pos, y, row_bytes;
    uint8_t filter;
    bool ended;
};

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}

static bool read_exact(FIL *file, void *data, UINT size) {
    UINT got;
    return f_read(file, data, size, &got) == FR_OK && got == size;
}

/* Store the rows needed by one output row. Mirroring this small window
 * makes a band spanning the end of the ring contiguous for the area filter. */
static bool prepare(struct png *p, uint16_t *out) {
    unsigned w = p->width, h = p->height;
    static const unsigned samples[] = {1,0,3,1,2,0,4};
    unsigned bits = samples[p->color] * p->depth;
    p->channels = p->color == 3 || p->color == 4 || p->color == 6 || p->key_present ? 4 : 3;
    p->stride = (w*bits+7)/8;
    p->filter_bpp = (bits+7)/8;
    if(p->depth != 8 || (p->color != 2 && p->color != 6) || p->key_present) {
        p->expanded = malloc(w*p->channels);
        if(!p->expanded) return false;
    }
    while(kui_cover_can_halve(w,h)) {
        if(p->levels == sizeof(p->half)/sizeof(p->half[0])) return false;
        struct half_row *step = &p->half[p->levels++];
        step->width = w;
        step->saved = malloc(w*p->channels);
        if(!step->saved) return false;
        w/=2; h/=2;
    }
    if(p->levels || p->expanded) {
        p->source_rows = calloc(2,p->stride);
        if(!p->source_rows) return false;
    }
    if(p->levels) {
        p->reduced_row = malloc((p->width/2)*p->channels);
        if(!p->reduced_row) return false;
    }
    p->sample_w = w;
    p->sample_h = h;
    p->sample_stride = w*p->channels;
    for(unsigned i=0; i<3; ++i) {
        struct view *v = &p->views[i];
        v->edge = kui_cover_edge((enum kui_cover_size)i);
        kui_cover_fit(w, h, v->edge, &v->width, &v->height);
        v->pixels = out;
        for(unsigned j=0; j<v->edge*v->edge; ++j) *out++ = KUI_COVER_BACKGROUND;
        unsigned needed = 1 + (h + v->height - 1) / v->height;
        if(p->slots < needed) p->slots = needed;
    }
    p->rows = calloc(2u*p->slots, p->sample_stride);
    return p->rows != NULL;
}

static int paeth(int a, int b, int c) {
    int prediction = a + b - c;
    int da = abs(prediction-a), db = abs(prediction-b), dc = abs(prediction-c);
    return da <= db && da <= dc ? a : db <= dc ? b : c;
}

static uint8_t *source_row(struct png *p, bool previous) {
    unsigned count = p->source_rows ? 2 : p->slots;
    unsigned slot = (p->y+count-(previous ? 1u : 0u))%count;
    return (p->source_rows ? p->source_rows : p->rows) + slot*p->stride;
}
static unsigned sample(const uint8_t *row, unsigned n, unsigned depth) {
    if(depth == 16) return (unsigned)row[2*n]<<8 | row[2*n+1];
    if(depth == 8) return row[n];
    unsigned bit=n*depth;
    return row[bit/8] >> (8-depth-bit%8) & ((1u<<depth)-1);
}
static uint8_t byte_sample(unsigned value, unsigned depth) {
    if(depth == 16) return (uint8_t)(value>>8);
    return (uint8_t)(depth == 8 ? value : value * (255u/((1u<<depth)-1u)));
}
static bool expand_row(struct png *p, const uint8_t *row) {
    uint8_t *out=p->expanded;
    for(unsigned x=0; x<p->width; ++x, out+=p->channels) {
        if(p->color == 3) {
            unsigned index=sample(row,x,p->depth);
            if(index >= p->palette_count) return false;
            memcpy(out,p->palette[index],4);
        } else if(p->color == 0 || p->color == 4) {
            unsigned n=p->color == 0 ? x : x*2;
            unsigned gray=sample(row,n,p->depth);
            out[0]=out[1]=out[2]=byte_sample(gray,p->depth);
            if(p->color == 4) out[3]=byte_sample(sample(row,n+1,p->depth),p->depth);
            else if(p->key_present) out[3]=gray == p->key[0] ? 0 : 255;
        } else {
            unsigned n=x*(p->color == 2 ? 3u : 4u);
            bool transparent=p->key_present;
            for(unsigned c=0;c<3;c++) {
                unsigned v=sample(row,n+c,p->depth);
                out[c]=byte_sample(v,p->depth);
                if(v != p->key[c]) transparent=false;
            }
            if(p->color == 6) out[3]=byte_sample(sample(row,n+3,p->depth),p->depth);
            else if(p->key_present) out[3]=transparent ? 0 : 255;
        }
    }
    return true;
}

static bool finish_row(struct png *p) {
    uint8_t *row = source_row(p,false);
    const uint8_t *prev = source_row(p,true);
    /* The unused previous slot starts zero for the first scanline. */
    if(p->filter) {
        for(unsigned i=0; i<p->stride; ++i) {
            int a = i < p->filter_bpp ? 0 : row[i-p->filter_bpp];
            int b = prev[i];
            int c = i < p->filter_bpp ? 0 : prev[i-p->filter_bpp];
            int delta;
            switch(p->filter) {
            case 1: delta = a; break;
            case 2: delta = b; break;
            case 3: delta = (a+b)/2; break;
            default: delta = paeth(a,b,c); break;
            }
            row[i] = (uint8_t)(row[i]+delta);
        }
    }
    if(p->expanded) {
        if(!expand_row(p,row)) return false;
        row=p->expanded;
    }
    ++p->y;
    p->row_bytes = 0;
    for(unsigned i=0; i<p->levels; ++i) {
        struct half_row *step = &p->half[i];
        if(!step->pending) {
            memcpy(step->saved,row,step->width*p->channels);
            step->pending = true;
            return true;
        }
        kui_cover_half_row(step->saved,row,step->width,p->channels,p->reduced_row);
        row = p->reduced_row;
        step->pending = false;
    }
    uint8_t *slot = p->rows + (p->sample_y%p->slots)*p->sample_stride;
    if(row != slot) memcpy(slot,row,p->sample_stride);
    memcpy(slot+p->slots*p->sample_stride,slot,p->sample_stride);
    ++p->sample_y;
    for(unsigned i=0; i<3; ++i) {
        struct view *v = &p->views[i];
        if(v->next >= v->height || (v->next+1)*p->sample_h > p->sample_y*v->height) continue;
        unsigned start = v->next*p->sample_h/v->height;
        const uint8_t *band = p->rows + (start%p->slots)*p->sample_stride;
        unsigned top = (v->edge-v->height)/2u, left = (v->edge-v->width)/2u;
        kui_cover_shrink_row(band,p->sample_w,p->sample_h,p->channels,v->width,v->height,
                             v->next,v->pixels+(top+v->next)*v->edge+left);
        ++v->next;
    }
    return true;
}

static bool scanlines(struct png *p, const uint8_t *bytes, size_t size) {
    while(size) {
        if(p->y >= p->height) return false;
        if(!p->row_bytes) {
            p->filter = *bytes++;
            if(p->filter > 4) return false;
            ++p->row_bytes;
            --size;
        }
        size_t offset = p->row_bytes-1, count = p->stride-offset;
        if(count > size) count = size;
        memcpy(source_row(p,false)+offset,bytes,count);
        p->row_bytes += (unsigned)count;
        bytes += count;
        size -= count;
        if(p->row_bytes == p->stride+1 && !finish_row(p)) return false;
    }
    return true;
}

static bool inflate_part(struct png *p, const uint8_t *bytes, size_t size) {
    if(p->ended) return size == 0;
    tinfl_status status;
    do {
        size_t consumed = size, produced = sizeof(p->history)-p->history_pos;
        uint8_t *output = p->history + p->history_pos;
        status = tinfl_decompress(&p->inflater,bytes,&consumed,p->history,output,&produced,
                                  TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_HAS_MORE_INPUT);
        if(status < 0 || !scanlines(p,output,produced)) return false;
        p->history_pos = (p->history_pos+(unsigned)produced) % sizeof(p->history);
        bytes += consumed;
        size -= consumed;
        if(status == TINFL_STATUS_DONE) {
            p->ended = true;
            return !size && p->y == p->height && p->sample_y == p->sample_h && !p->row_bytes;
        }
        if(!consumed && !produced && (size || status == TINFL_STATUS_HAS_MORE_OUTPUT)) return false;
    } while(size || status == TINFL_STATUS_HAS_MORE_OUTPUT);
    return true;
}

static enum kui_cover_rows_result decode_chunks(FIL *file, struct png *p, uint16_t *out, kui_cancel_fn cancel) {
    bool seen_data = false, after_data = false;
    while(f_tell(file) < f_size(file)) {
        uint8_t chunk[8], tail[4];
        if(!read_exact(file,chunk,sizeof(chunk))) return KUI_COVER_ROWS_ERROR;
        uint32_t size = be32(chunk);
        if(size+UINT64_C(4) > f_size(file)-f_tell(file)) return KUI_COVER_ROWS_INVALID;
        const uint8_t *name = chunk+4;
        bool data = !memcmp(name,"IDAT",4), end = !memcmp(name,"IEND",4);
        if(data && after_data) return KUI_COVER_ROWS_INVALID;
        if(!data && seen_data) after_data = true;
        bool palette = !memcmp(name,"PLTE",4), key = !memcmp(name,"tRNS",4);
        if((palette || key) && seen_data) return KUI_COVER_ROWS_INVALID;
        if(palette && (!size || size%3 || size > 768 || p->palette_count)) return KUI_COVER_ROWS_INVALID;
        if(key && ((p->color == 0 && size != 2) || (p->color == 2 && size != 6) ||
                   (p->color == 3 && (!size || size > p->palette_count)) ||
                   p->color == 4 || p->color == 6 || p->key_present)) return KUI_COVER_ROWS_INVALID;
        if(data && !seen_data) {
            if(p->color == 3 && !p->palette_count) return KUI_COVER_ROWS_INVALID;
            if(!prepare(p,out)) return KUI_COVER_ROWS_ERROR;
            seen_data = true;
        }
        if(!data && !end && !(name[0]&32) && memcmp(name,"PLTE",4)) return KUI_COVER_ROWS_INVALID;
        if(end && size) return KUI_COVER_ROWS_INVALID;
        uint32_t crc = kui_crc32_capture(0,name,4);
        while(size) {
            if(cancel && cancel()) return KUI_COVER_ROWS_ERROR;
            unsigned n = size < sizeof(p->input) ? size : sizeof(p->input);
            if(!read_exact(file,p->input,n)) return KUI_COVER_ROWS_ERROR;
            crc = kui_crc32_capture(crc,p->input,n);
            if(palette) {
                p->palette_count=n/3;
                for(unsigned i=0;i<p->palette_count;i++) {
                    memcpy(p->palette[i],p->input+3*i,3);
                    p->palette[i][3]=255;
                }
            } else if(key) {
                p->key_present=true;
                if(p->color == 3) {
                    for(unsigned i=0;i<n;i++) p->palette[i][3]=p->input[i];
                } else {
                    for(unsigned i=0;i<n/2;i++) p->key[i]=(uint16_t)((unsigned)p->input[2*i]<<8|p->input[2*i+1]);
                }
            }
            if(data && !inflate_part(p,p->input,n)) return KUI_COVER_ROWS_INVALID;
            size -= n;
        }
        if(!read_exact(file,tail,4)) return KUI_COVER_ROWS_ERROR;
        if(crc != be32(tail)) return KUI_COVER_ROWS_INVALID;
        if(end) return p->ended && f_tell(file) == f_size(file) ? KUI_COVER_ROWS_OK : KUI_COVER_ROWS_INVALID;
    }
    return KUI_COVER_ROWS_INVALID;
}

enum kui_cover_rows_result kui_cover_png_rows(FIL *file, uint16_t *out, kui_cancel_fn cancel) {
    uint8_t h[33];
    if(!file || !out || f_lseek(file,0) != FR_OK) return KUI_COVER_ROWS_ERROR;
    if(f_size(file) < sizeof(h)) return KUI_COVER_ROWS_FALLBACK;
    if(f_size(file) > KUI_INTERNET_COVER_MAX_BYTES) return KUI_COVER_ROWS_INVALID;
    if(!read_exact(file,h,sizeof(h))) return KUI_COVER_ROWS_ERROR;
    if(memcmp(h,"\x89PNG\r\n\x1a\n",8)) return KUI_COVER_ROWS_FALLBACK;
    if(be32(h+8) != 13 || memcmp(h+12,"IHDR",4) ||
       kui_crc32_capture(0,h+12,17) != be32(h+29)) return KUI_COVER_ROWS_INVALID;
    unsigned w = be32(h+16), height = be32(h+20);
    if(!w || !height || w > KUI_INTERNET_COVER_MAX_EDGE || height > KUI_INTERNET_COVER_MAX_EDGE) return KUI_COVER_ROWS_INVALID;
    unsigned edge = w > height ? w : height;
    /* Smaller sources use the existing bilinear enlarger. */
    unsigned depth=h[24], color=h[25];
    bool packed=depth==1 || depth==2 || depth==4;
    if((color != 0 && color != 2 && color != 3 && color != 4 && color != 6) ||
       (packed && color != 0 && color != 3) || (color == 3 && depth == 16) ||
       (!packed && depth != 8 && depth != 16) || h[26] || h[27] || h[28]>1) return KUI_COVER_ROWS_INVALID;
    if(h[28] || edge < KUI_COVER_LARGE) return KUI_COVER_ROWS_FALLBACK;
    struct png *p = calloc(1,sizeof(*p));
    if(!p) return KUI_COVER_ROWS_ERROR;
    p->width = w; p->height = height; p->depth=depth; p->color=color;
    tinfl_init(&p->inflater);
    enum kui_cover_rows_result result = decode_chunks(file,p,out,cancel);
    for(unsigned i=0; i<p->levels; ++i) free(p->half[i].saved);
    free(p->expanded);
    free(p->source_rows);
    free(p->reduced_row);
    free(p->rows);
    free(p);
    return result;
}
