/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/cover_png_rows.h"
#include "kui/cover_image.h"
#include "kui/game_cover.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *source;
static bool stop;
static bool cancel(void) { return stop; }
FRESULT f_lseek(FIL *f, FSIZE_t offset) {
    if(fseek(source,(long)offset,SEEK_SET)) return FR_DISK_ERR;
    f->fptr = offset;
    return FR_OK;
}
FRESULT f_read(FIL *f, void *data, UINT count, UINT *got) {
    *got = (UINT)fread(data,1,count,source);
    f->fptr += *got;
    return ferror(source) ? FR_DISK_ERR : FR_OK;
}

/* Measure the streamed decoder's own heap and its output buffer together.
 * All successful allocations by this test phase are tracked by the linker. */
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void __real_free(void *);
static struct { void *ptr; size_t bytes; } allocs[12];
static bool track;
static size_t live, peak;
static void *observe(void *ptr, size_t bytes) {
    if(!track || !ptr) return ptr;
    for(size_t i=0;i<sizeof(allocs)/sizeof(allocs[0]);++i) if(!allocs[i].ptr) {
        allocs[i].ptr=ptr; allocs[i].bytes=bytes;
        live+=bytes; if(live>peak) peak=live;
        return ptr;
    }
    abort();
}
void *__wrap_malloc(size_t n) { return observe(__real_malloc(n),n); }
void *__wrap_calloc(size_t n,size_t s) { return observe(__real_calloc(n,s),n*s); }
void __wrap_free(void *ptr) {
    if(track && ptr) {
        for(size_t i=0;i<sizeof(allocs)/sizeof(allocs[0]);++i) if(allocs[i].ptr==ptr) {
            live-=allocs[i].bytes; allocs[i].ptr=NULL; break;
        }
    }
    __real_free(ptr);
}

int main(int argc, char **argv) {
    if(argc!=3) return 2;
    source=fopen(argv[1],"rb");
    assert(source && !fseek(source,0,SEEK_END));
    long length=ftell(source); assert(length>0);
    FIL file={0}; file.obj.objsize=(FSIZE_t)length;
    size_t out_bytes=KUI_COVER_FILE_BYTES-KUI_COVER_HEADER_BYTES;
    track=true;
    uint16_t *small=malloc(out_bytes);
    assert(small);
    stop=!strcmp(argv[2],"cancel");
    enum kui_cover_rows_result r=kui_cover_png_rows(&file,small,cancel);
    if(!strcmp(argv[2],"match")) {
        assert(r==KUI_COVER_ROWS_OK);
        track=false;
        uint8_t *data=malloc((size_t)length),*px;
        unsigned w,h,ch;
        assert(data && !fseek(source,0,SEEK_SET) && fread(data,1,(size_t)length,source)==(size_t)length);
        assert(kui_cover_image_decode(data,(size_t)length,&px,&w,&h,&ch)==KUI_COVER_IMAGE_OK);
        kui_cover_reduce(px,&w,&h,ch);
        uint16_t *wanted=malloc(KUI_COVER_PIXELS*sizeof(*wanted));
        assert(wanted);
        size_t offset=0;
        for(unsigned i=0;i<3;i++) {
            unsigned edge=kui_cover_edge((enum kui_cover_size)i);
            assert(kui_cover_scale(px,w,h,ch,wanted,edge));
            assert(!memcmp(wanted,small+offset,edge*edge*sizeof(*small)));
            offset+=edge*edge;
        }
        free(wanted); kui_cover_image_free(px); free(data);
        printf("Identical 160/104/56 RGB565 pixels; streamed decoder + output: %zu bytes\n",peak);
        track=true;
    } else if(!strcmp(argv[2],"fallback")) assert(r==KUI_COVER_ROWS_FALLBACK);
    else if(stop) assert(r==KUI_COVER_ROWS_ERROR);
    else assert(r==KUI_COVER_ROWS_INVALID);
    free(small);
    assert(!live && !fclose(source));
}
