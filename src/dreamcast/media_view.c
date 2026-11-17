/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_view.h"
#include "kui/media_container.h"
#include "kui/games.h"
#include "kui/cover_image.h"
#include "kui/shell_font.h"
#include "platform.h"
#include <arch/timer.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/sq.h>
#include <dc/video.h>
#include <kos/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCREEN_W=640, SCREEN_H=480, VIEW_W=576, VIEW_H=390, VIEW_TOP=22 };

static unsigned buttons(void) {
    maple_device_t *pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *state = pad ? maple_dev_status(pad) : NULL;
    return state ? state->buttons : 0;
}

static void present(uint16_t *canvas) {
    if(canvas) {
        sq_cpy(vram_s, canvas, SCREEN_W*SCREEN_H*sizeof(*canvas));
        sq_wait();
    }
    vid_waitvbl();
    vid_flip(-1);
}

static uint16_t *clear_frame(uint16_t *canvas) {
    uint16_t *frame = canvas ? canvas : vram_s;
    /* The SH-4 framebuffer accepts 16/32-bit stores. */
    if(canvas) memset(canvas, 0, SCREEN_W*SCREEN_H*sizeof(*canvas));
    else vid_clear(0,0,0);
    return frame;
}

static const char *preview(FIL *file, unsigned count, uint16_t *canvas) {
    uint16_t *pixels = malloc(KUI_PREVIEW_FRAME_BYTES);
    if(!pixels) return "Insufficient memory for preview";
    unsigned previous = buttons(), shown = count, next = 0;
    uint64_t start = timer_ms_gettime64();
    const char *error = NULL;
    for(;;) {
        unsigned held = buttons(), pressed = held & ~previous;
        previous = held;
        if(pressed & CONT_B) break;
        if(pressed & CONT_A) {
            shown = count;
            start = timer_ms_gettime64();
        }
        uint64_t ticks = (timer_ms_gettime64()-start)/(1000u/KUI_PREVIEW_FPS);
        unsigned wanted = ticks < count ? (unsigned)ticks : count-1u;
        if(wanted != shown) {
            UINT got;
            if((wanted != next && f_lseek(file,KUI_PREVIEW_HEADER_BYTES+wanted*KUI_PREVIEW_FRAME_BYTES)!=FR_OK) ||
               f_read(file,pixels,KUI_PREVIEW_FRAME_BYTES,&got)!=FR_OK || got!=KUI_PREVIEW_FRAME_BYTES) {
                error = "Cannot read preview frame"; break;
            }
            next = wanted+1u;
            uint16_t *fb = clear_frame(canvas);
            for(unsigned y=0; y<KUI_PREVIEW_HEIGHT; ++y) {
                uint16_t *row = fb+(36u+y*3u)*SCREEN_W+80u;
                const uint16_t *src = pixels+y*KUI_PREVIEW_WIDTH;
                for(unsigned x=0; x<KUI_PREVIEW_WIDTH; ++x)
                    row[3u*x] = row[3u*x+1u] = row[3u*x+2u] = src[x];
                memcpy(row+SCREEN_W, row, KUI_PREVIEW_WIDTH*6u);
                memcpy(row+2u*SCREEN_W, row, KUI_PREVIEW_WIDTH*6u);
            }
            kui_shell_font_draw(fb,32,422,0xffff,
                wanted+1u==count ? "Preview finished     A Replay     B Back" : "Game preview     A Replay     B Back",false);
            present(canvas);
            shown = wanted;
        }
        thd_sleep(12);
    }
    free(pixels);
    return error;
}

struct page {
    uint8_t *rgb;
    unsigned w, h, channels;
};

static const char *load_page(FIL *file, struct kui_media_index *index, unsigned number,
                             struct page *page) {
    unsigned len = index->offsets[number+1u]-index->offsets[number];
    uint8_t *encoded = malloc(len);
    if(!encoded) return "Insufficient memory for manual page";
    UINT got;
    const char *problem = NULL;
    if(f_lseek(file, index->offsets[number])!=FR_OK ||
       f_read(file, encoded, len, &got)!=FR_OK || got!=len)
        problem = "Cannot read manual page";
    else if(len<4 || encoded[0]!=0xff || encoded[1]!=0xd8 || encoded[len-2]!=0xff || encoded[len-1]!=0xd9)
        problem = "Invalid manual page";
    else {
        enum kui_cover_image_status s = kui_cover_image_decode(encoded,len,&page->rgb,&page->w,
                                                               &page->h,&page->channels);
        if(s!=KUI_COVER_IMAGE_OK) problem = kui_cover_image_status_text(s);
        else if(page->w > KUI_MANUAL_EDGE_MAX || page->h > KUI_MANUAL_EDGE_MAX)
            problem = "Manual page exceeds 1024 pixels";
    }
    free(encoded);
    return problem;
}

static unsigned minimum(unsigned a,unsigned b) { return a<b ? a : b; }
static unsigned clamp_pan(int n, unsigned limit) { return n<0 ? 0 : minimum((unsigned)n,limit); }

/* Fit the complete page or show source pixels 1:1 with a movable viewport.
 * Precompute source columns; the SH-4 then needs no division per pixel. */
static void page_draw(struct page *page, unsigned number, unsigned total, bool zoomed,
                      unsigned sx, unsigned sy, uint16_t *canvas) {
    unsigned src_w = zoomed ? minimum(page->w, VIEW_W) : page->w;
    unsigned src_h = zoomed ? minimum(page->h, VIEW_H) : page->h;
    unsigned w = src_w, h = src_h;
    if(!zoomed) {
        if(src_w*VIEW_H > src_h*VIEW_W) {
            w = VIEW_W; h = src_h*w/src_w;
        } else {
            h = VIEW_H; w = src_w*h/src_h;
        }
        if(!w) w=1;
        if(!h) h=1;
    }
    unsigned columns[VIEW_W];
    for(unsigned x=0; x<w; ++x) columns[x]=(sx+x*src_w/w)*page->channels;
    uint16_t *fb = clear_frame(canvas);
    for(unsigned y=0; y<h; ++y) {
        const uint8_t *row = page->rgb+((size_t)(sy+y*src_h/h)*page->w*page->channels);
        uint16_t *out = fb+(VIEW_TOP+(VIEW_H-h)/2u+y)*SCREEN_W+(SCREEN_W-w)/2u;
        for(unsigned x=0; x<w; ++x) {
            const uint8_t *rgb = row+columns[x];
            out[x]=(uint16_t)((rgb[0]&0xf8u)<<8 | (rgb[1]&0xfcu)<<3 | rgb[2]>>3);
        }
    }
    char hint[100];
    snprintf(hint,sizeof(hint),"Page %u/%u    %s",number+1u,total,
        zoomed ? "D-pad Pan    X/Y Page    A Fit    B Back" : "Left/Right Page    A Zoom    B Back");
    kui_shell_font_draw(fb,32,422,0xffff,hint,false);
    present(canvas);
}

static const char *manual_view(FIL *file, struct kui_media_index *index, uint16_t *canvas) {
    unsigned previous = buttons(), number = 0, sx = 0, sy = 0;
    bool zoomed = false, load = true, paint = true;
    struct page page = {0};
    const char *error = NULL;
    uint64_t pan_deadline = 0;
    for(;;) {
        if(load) {
            kui_cover_image_free(page.rgb);
            page.rgb = NULL;
            error = load_page(file, index, number, &page);
            if(error) break;
            zoomed = false; sx = sy = 0; load = false;
            paint = true;
        }
        if(paint) {
            page_draw(&page, number, index->count, zoomed, sx, sy, canvas);
            paint = false;
        }
        unsigned held = buttons(), pressed = held & ~previous;
        previous = held;
        if(pressed & CONT_B) break;
        if(pressed & CONT_A) {
            zoomed = !zoomed;
            sx = sy = 0;
            paint = true;
        }
        if(pressed & (CONT_X | (zoomed ? 0 : CONT_DPAD_LEFT))) {
            if(number) { --number; load = true; }
        } else if(pressed & (CONT_Y | (zoomed ? 0 : CONT_DPAD_RIGHT))) {
            if(number+1u<index->count) { ++number; load = true; }
        }
        unsigned arrows = CONT_DPAD_UP|CONT_DPAD_DOWN|CONT_DPAD_LEFT|CONT_DPAD_RIGHT;
        uint64_t now = timer_ms_gettime64();
        if(zoomed && (held & arrows) && ((pressed & arrows) || now>=pan_deadline)) {
            int dx = ((held&CONT_DPAD_RIGHT)!=0)-((held&CONT_DPAD_LEFT)!=0);
            int dy = ((held&CONT_DPAD_DOWN)!=0)-((held&CONT_DPAD_UP)!=0);
            unsigned new_x = clamp_pan((int)sx+dx*32, page.w-minimum(page.w, VIEW_W));
            unsigned new_y = clamp_pan((int)sy+dy*32, page.h-minimum(page.h, VIEW_H));
            paint |= new_x!=sx || new_y!=sy;
            sx = new_x; sy = new_y;
            pan_deadline = now+((pressed & arrows) ? 220u : 60u);
        }
        thd_sleep(12);
    }
    kui_cover_image_free(page.rgb);
    return error;
}

bool kui_media_view(const char *path, bool manual, kui_log_fn log, char *message, size_t cap,
                    uint16_t *canvas) {
    const char *problem = NULL;
    if(!path || (strncmp(path,"0:/",3) && path[0]!='/') || !log)
        problem = "Invalid game media path";
    else if(!kui_sd_connect()) problem = "Cannot connect to game media storage";
    else {
        FATFS fs;
        if(!kui_mount(&fs,log)) problem = "Cannot mount game media storage";
        else {
            FIL file;
            FRESULT r = f_open(&file, path, FA_READ);
            if(r==FR_NO_FILE || r==FR_NO_PATH) {
                problem = manual ? "Download this manual first" : "Download this preview first";
                if(manual) {
                    /* A provider may only offer an original PDF. Keep its status
                     * distinct from a missing or damaged page-format manual. */
                    size_t n = strlen(path);
                    char original[KUI_GAMES_FILE_CAP];
                    if(n > 4 && n < sizeof(original) && !strcmp(path+n-4,".kdm")) {
                        memcpy(original,path,n-4);
                        memcpy(original+n-4,".pdf",5);
                        FILINFO info;
                        r = f_stat(original,&info);
                        if(r == FR_OK && !(info.fattrib & AM_DIR)) {
                            problem = "Original PDF saved in KUI/media. Page images unavailable.";
                            log("Game media: saved original %s",original);
                        } else if(r != FR_NO_FILE && r != FR_NO_PATH) problem = "Cannot check saved manual";
                    }
                }
            }
            else if(r!=FR_OK) problem = "Cannot open game media file";
            else {
                struct kui_media_index index;
                if(!kui_media_container_index(&file,manual,&index)) problem = "Damaged or unsupported game media file";
                else problem = manual ? manual_view(&file,&index,canvas) : preview(&file,index.count,canvas);
                if(f_close(&file)!=FR_OK && !problem) problem = "Cannot close game media file";
            }
            if(f_mount(NULL,"0:",0)!=FR_OK && !problem) problem = "Cannot release game media storage";
        }
        kui_sd_disconnect();
    }
    if(message && cap) snprintf(message,cap,"%s",problem ? problem : manual ? "Manual closed" : "Preview closed");
    if(problem && log) log("Game media: %s",problem);
    return !problem;
}
