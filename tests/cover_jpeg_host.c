/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/cover_jpeg.h"
#include "kui/game_cover.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *input;
static unsigned polls, stop_after;
static bool fail_read;
static bool cancelled(void) { return stop_after && ++polls >= stop_after; }
FRESULT f_read(FIL *file, void *dst, UINT length, UINT *read) {
    /* TJpgDec consumes only bounded blocks of compressed input. */
    assert(length <= 1024);
    if(fail_read && file->fptr > 4096) return FR_DISK_ERR;
    *read = (UINT)fread(dst,1,length,input);
    file->fptr += *read;
    return ferror(input) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_lseek(FIL *file, FSIZE_t pos) {
    if(pos > file->obj.objsize || fseek(input,(long)pos,SEEK_SET)) return FR_DISK_ERR;
    file->fptr = pos;
    return FR_OK;
}
int main(int argc,char **argv) {
    if(argc != 4) return 2;
    input = fopen(argv[1],"rb");
    assert(input && !fseek(input,0,SEEK_END));
    FIL file = {0};
    file.obj.objsize = (FSIZE_t)ftell(input);
    stop_after = !strcmp(argv[3],"cancel") ? 500u : 0u;
    fail_read = !strcmp(argv[3],"io");
    static uint16_t pixels[(KUI_COVER_FILE_BYTES - KUI_COVER_HEADER_BYTES)/2u];
    enum kui_cover_rows_result result = kui_cover_jpeg_scaled(&file,pixels,false,cancelled);
    bool ok = !strcmp(argv[3],"ok");
    enum kui_cover_rows_result expected = ok ? KUI_COVER_ROWS_OK : fail_read || stop_after ?
        KUI_COVER_ROWS_ERROR : !strcmp(argv[3],"small") ? KUI_COVER_ROWS_FALLBACK : KUI_COVER_ROWS_INVALID;
    if(result != expected) {
        fprintf(stderr,"%s: result %d, expected %d\n",argv[3],result,expected);
        return 1;
    }
    if(ok) {
        FILE *out = fopen(argv[2],"wb");
        assert(out && fwrite(pixels,1,sizeof(pixels),out) == sizeof(pixels) && !fclose(out));
    }
    assert(!fclose(input));
    return 0;
}
