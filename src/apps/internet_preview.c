/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/internet_preview.h"
#include "kui/hash.h"
#include "kui/media_container.h"
#include "kui/music_ogg.h"
#include "kui/preview_prefix.h"
#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <stdlib.h>
#include <string.h>

enum { XIPH_MAX = 3 * 1024 * 1024, PACKET_MAX = 256 * 1024, READ_SIZE = 8192 };
/* Only the single storage worker enters this decoder. Track the codec's own
 * allocations too, so large comment/codebook/packet tables cannot grow the
 * heap beyond the budget. libogg and libtheoradec use these four wrappers. */
union allocation {
    size_t bytes;
    max_align_t alignment;
};
static size_t used, peak;

void *kui_xiph_alloc(size_t size) {
    if(size > XIPH_MAX || sizeof(union allocation)+size > XIPH_MAX-used) return NULL;
    size_t bytes = sizeof(union allocation)+size;
    union allocation *p = malloc(bytes);
    if(!p) return NULL;
    p->bytes = bytes;
    used += bytes;
    if(peak < used) peak = used;
    return p+1;
}

void kui_xiph_free(void *ptr) {
    if(ptr) {
        union allocation *p = (union allocation *)ptr-1;
        used -= p->bytes;
        free(p);
    }
}

void *kui_xiph_calloc(size_t count, size_t size) {
    if(size && count > XIPH_MAX/size) return NULL;
    void *p = kui_xiph_alloc(count*size);
    if(p) memset(p, 0, count*size);
    return p;
}

void *kui_xiph_resize(void *ptr, size_t size) {
    if(!ptr) return kui_xiph_alloc(size);
    if(!size) { kui_xiph_free(ptr); return NULL; }
    union allocation *old = (union allocation *)ptr-1;
    size_t others = used-old->bytes;
    if(size > XIPH_MAX || size+sizeof(*old) > XIPH_MAX-others) return NULL;
    size_t bytes = size+sizeof(*old);
    union allocation *p = realloc(old, bytes);
    if(!p) return NULL;
    p->bytes = bytes;
    used = others+bytes;
    if(peak < used) peak = used;
    return p+1;
}

/* Only keep complete, CRC-checked pages of the first supported Vorbis
 * stream. The Theora decoder and its frame are freed before audio decoding. */
struct audio_pages {
    uint8_t *data;
    size_t size, capacity;
    int serial;
    unsigned rate;
    uint32_t next_page;
    ogg_int64_t granule;
    bool done;
};

static bool collect_audio(struct audio_pages *a, ogg_page *page, unsigned end_seconds) {
    if(a->done) return true;
    if(!a->rate) {
        const uint8_t *id = page->body;
        if(!ogg_page_bos(page) || page->body_len < 30 || memcmp(id,"\1vorbis",7) ||
           !id[11] || id[11] > 2) return true;
        uint32_t rate = id[12] | (uint32_t)id[13]<<8 | (uint32_t)id[14]<<16 | (uint32_t)id[15]<<24;
        if(rate < 8000u || rate > 48000u) return true;
        a->rate = rate;
        a->serial = ogg_page_serialno(page);
    }
    if(ogg_page_serialno(page) != a->serial) return true;
    size_t bytes = (size_t)page->header_len + (size_t)page->body_len;
    const size_t limit = 512u*1024u;
    if((uint32_t)ogg_page_pageno(page) != a->next_page++ || bytes > limit-a->size) {
        a->done = true;
        a->size = 0;
        return true;
    }
    if(a->size+bytes > a->capacity) {
        size_t capacity = a->capacity ? a->capacity*2u : 8192u;
        while(capacity < a->size+bytes) capacity *= 2u;
        uint8_t *buf = realloc(a->data,capacity);
        if(!buf) return false;
        a->capacity = capacity;
        a->data = buf;
    }
    memcpy(a->data+a->size,page->header,(size_t)page->header_len);
    memcpy(a->data+a->size+page->header_len,page->body,(size_t)page->body_len);
    a->size += bytes;
    ogg_int64_t pos = ogg_page_granulepos(page);
    if(pos >= 0) a->granule = pos;
    a->done = ogg_page_eos(page) || a->granule >= (ogg_int64_t)a->rate * end_seconds;
    return true;
}

struct convert {
    struct audio_pages audio;
    ogg_sync_state sync;
    ogg_stream_state stream;
    th_info info;
    th_comment comments;
    th_setup_info *setup;
    th_dec_ctx *decoder;
    uint16_t *frame;
    uint32_t *readback_crc;
    unsigned start_seconds, source_frames, written;
    bool found, video_done;
};

static unsigned clamp(int n) {
    return n < 0 ? 0u : n > 255 ? 255u : (unsigned)n;
}

static void sample_frame(struct convert *c, th_ycbcr_buffer planes) {
    const th_info *info = &c->info;
    unsigned w = KUI_PREVIEW_WIDTH, h = KUI_PREVIEW_HEIGHT;
    if(info->pic_width*h > info->pic_height*w)
        h = (info->pic_height*w + info->pic_width/2u)/info->pic_width;
    else
        w = (info->pic_width*h + info->pic_height/2u)/info->pic_height;
    if(!w) w = 1;
    if(!h) h = 1;
    /* Source is bounded Y'CbCr 4:2:0 studio-range. The surrounding KPV
     * letterbox stays black in the initially zero-filled frame. */
    unsigned columns[KUI_PREVIEW_WIDTH];
    for(unsigned x=0; x<w; ++x) columns[x] = info->pic_x+x*info->pic_width/w;
    for(unsigned y=0; y<h; ++y) {
        unsigned sy = info->pic_y+y*info->pic_height/h;
        const unsigned char *luma = planes[0].data+(int)sy*planes[0].stride;
        const unsigned char *cb = planes[1].data+(int)(sy/2u)*planes[1].stride;
        const unsigned char *cr = planes[2].data+(int)(sy/2u)*planes[2].stride;
        uint16_t *row = c->frame + ((KUI_PREVIEW_HEIGHT-h)/2u+y)*KUI_PREVIEW_WIDTH
                                 + (KUI_PREVIEW_WIDTH-w)/2u;
        for(unsigned x=0; x<w; ++x) {
            unsigned sx = columns[x];
            int lum = 298*((int)luma[sx]-16)+128;
            int u = (int)cb[sx/2u]-128, v = (int)cr[sx/2u]-128;
            unsigned red = clamp((lum+409*v)>>8), green = clamp((lum-100*u-208*v)>>8);
            unsigned blue = clamp((lum+516*u)>>8);
            row[x] = (uint16_t)(((red & 248u)<<8) | ((green & 252u)<<3) | (blue>>3));
        }
    }
}

static bool write_bytes(FIL *dest, const void *data, UINT size) {
    UINT written;
    return f_write(dest, data, size, &written) == FR_OK && written == size;
}
static bool write_checked(struct convert *c,FIL *dest,const void *data,UINT size) {
    if(!write_bytes(dest,data,size)) return false;
    if(c->readback_crc) *c->readback_crc=kui_crc32_capture(*c->readback_crc,data,size);
    return true;
}

static bool frame_packet(struct convert *c, ogg_packet *packet, FIL *dest) {
    if(!c->decoder) {
        int header = th_decode_headerin(&c->info, &c->comments, &c->setup, packet);
        if(header < 0) return false;
        const th_info *i = &c->info;
        if(!i->pic_width || !i->pic_height || i->frame_width > 640 || i->frame_height > 480 ||
           i->pixel_fmt != TH_PF_420 || !i->fps_denominator || !i->fps_numerator ||
           i->fps_numerator > UINT64_C(60)*i->fps_denominator ||
           i->fps_numerator < i->fps_denominator)
            return false;
        if(header > 0) return true;
        c->decoder = th_decode_alloc(i, c->setup);
        th_setup_free(c->setup); c->setup = NULL;
        if(!c->decoder) return false;
    }
    int decoded = th_decode_packetin(c->decoder, packet, NULL);
    if(decoded != 0 && decoded != TH_DUPFRAME) return false;
    /* Output frame k samples the source at start_seconds + k/10, retaining
     * source timing even for e.g. 60000/1001 fps or a sub-10 fps clip. */
    uint64_t end = (uint64_t)++c->source_frames*c->info.fps_denominator*KUI_PREVIEW_FPS;
    unsigned first = c->start_seconds * KUI_PREVIEW_FPS;
    if((uint64_t)(first + c->written)*c->info.fps_numerator < end) {
        th_ycbcr_buffer planes;
        if(th_decode_ycbcr_out(c->decoder, planes) < 0) return false;
        sample_frame(c, planes);
        do {
            if(!write_checked(c,dest,c->frame,KUI_PREVIEW_FRAME_BYTES)) return false;
            ++c->written;
        } while(c->written < KUI_PREVIEW_FRAMES_MAX &&
                (uint64_t)(first + c->written)*c->info.fps_numerator < end);
    }
    return true;
}

static bool convert_stream(struct convert *c, FIL *source, FIL *dest, kui_cancel_fn cancel) {
    unsigned first = c->start_seconds * KUI_PREVIEW_FPS;
    while(!cancel || !cancel()) {
        if(c->video_done && (!c->audio.rate || c->audio.done ||
           c->audio.granule >= (ogg_int64_t)(first + c->written) * c->audio.rate / KUI_PREVIEW_FPS))
            return true;
        ogg_page page;
        int status = ogg_sync_pageout(&c->sync, &page);
        if(status < 0) return false;
        if(!status) {
            char *buf = ogg_sync_buffer(&c->sync, READ_SIZE);
            UINT got;
            if(!buf || f_read(source, buf, READ_SIZE, &got) != FR_OK) return false;
            if(!got) return c->video_done;
            if(ogg_sync_wrote(&c->sync, got)) return false;
            continue;
        }
        if(!collect_audio(&c->audio,&page,c->start_seconds + KUI_PREVIEW_FRAMES_MAX / KUI_PREVIEW_FPS)) return false;
        if(c->video_done) continue;
        if(!c->found && ogg_page_bos(&page) && page.body_len >= 7 &&
           memcmp(page.body, "\200theora", 7) == 0) {
            if(ogg_stream_init(&c->stream, ogg_page_serialno(&page))) return false;
            c->found = true;
        }
        if(!c->found || ogg_page_serialno(&page) != c->stream.serialno) continue;
        if(page.body_len + c->stream.body_fill-c->stream.body_returned > PACKET_MAX ||
           ogg_stream_pagein(&c->stream, &page)) return false;
        ogg_packet packet;
        while((status = ogg_stream_packetout(&c->stream, &packet)) > 0) {
            if((cancel && cancel()) || !frame_packet(c, &packet, dest)) return false;
            if(c->written == KUI_PREVIEW_FRAMES_MAX) {
                c->video_done = true;
                break;
            }
        }
        if(status < 0) return false;
        if(ogg_page_eos(&page)) {
            if(!c->written) return false;
            c->video_done = true;
        }
    }
    return false;
}

static bool append_audio(struct convert *c, FIL *dest, uint8_t *header, kui_cancel_fn cancel) {
    if(!c->audio.size) return true;
    size_t samples = c->written * KUI_PREVIEW_AUDIO_SAMPLES_PER_FRAME;
    int16_t *pcm = calloc(samples,sizeof(*pcm));
    void *workspace = malloc(KUI_OGG_WORKSPACE_BYTES);
    bool ok = workspace && pcm;
    if(ok) {
        size_t decoded = kui_ogg_preview_pcm(c->audio.data,c->audio.size,workspace,pcm,
                                              samples,KUI_PREVIEW_AUDIO_RATE,c->start_seconds,cancel);
        if(cancel && cancel()) ok = false;
        if(ok && decoded) {
            /* The final video frame may outlast the source audio. The zeroed
             * tail preserves synchronization without looping shorter tracks. */
            for(size_t pos=0; ok && pos<samples;) {
                size_t n = samples-pos < 4096u ? samples-pos : 4096u;
                ok = (!cancel || !cancel()) && write_checked(c,dest,pcm+pos,(UINT)(2u*n));
                pos += n;
            }
            header[10] = (uint8_t)KUI_PREVIEW_AUDIO_RATE;
            header[11] = (uint8_t)(KUI_PREVIEW_AUDIO_RATE>>8);
        }
    }
    free(workspace);
    free(pcm);
    return ok;
}

bool kui_internet_preview_convert(FIL *source, FIL *dest, unsigned start_seconds,
                                  kui_cancel_fn cancel, kui_log_fn log,
                                  uint32_t *readback_crc) {
    if(start_seconds > KUI_PREVIEW_START_MAX_SECONDS || !source || !dest || used || f_size(dest) || !f_size(source) ||
       f_size(source) > 32u*1024u*1024u || f_lseek(source, 0) != FR_OK) return false;
    struct convert c = {.readback_crc=readback_crc, .start_seconds=start_seconds};
    if(readback_crc) *readback_crc=0;
    /* Dreamcast and the host test target are little-endian RGB565. */
    uint8_t header[KUI_PREVIEW_HEADER_BYTES] = {'K','P','V','2',KUI_PREVIEW_WIDTH,0,
                                                KUI_PREVIEW_HEIGHT,0,KUI_PREVIEW_FPS,0,0,0};
    peak = 0;
    c.frame = calloc(KUI_PREVIEW_WIDTH*KUI_PREVIEW_HEIGHT, sizeof(*c.frame));
    ogg_sync_init(&c.sync); th_info_init(&c.info); th_comment_init(&c.comments);
    bool ok = c.frame && write_bytes(dest, header, sizeof(header)) && convert_stream(&c, source, dest, cancel);
    if(c.decoder) th_decode_free(c.decoder);
    th_setup_free(c.setup);
    th_comment_clear(&c.comments); th_info_clear(&c.info);
    if(c.found) ogg_stream_clear(&c.stream);
    ogg_sync_clear(&c.sync);
    free(c.frame);
    if(ok) ok = append_audio(&c,dest,header,cancel);
    free(c.audio.data);
    if(ok) {
        header[12] = (uint8_t)c.written;
        ok = f_lseek(dest, 0) == FR_OK && write_checked(&c,dest,header,sizeof(header));
    }
    if(log) log("Internet preview: %u source frames, %u preview frames, codec peak %lu bytes",
                c.source_frames, c.written, (unsigned long)peak);
    return ok && used == 0;
}
