/* SPDX-License-Identifier: GPL-3.0-only */
/* Run the production offline converter using local files for FatFs I/O. */
#include "kui/internet_preview.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FIL source, dest;
static FILE *input, *output;
static int calls, stop_at;
static const char *fault = "";
static bool cancel(void) { return stop_at && ++calls >= stop_at; }
static void logger(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr,fmt,ap); va_end(ap);
    fputc('\n',stderr);
}
FRESULT f_read(FIL *fp, void *buf, UINT len, UINT *got) {
    if(fp != &source) return FR_INT_ERR;
    if(!strcmp(fault,"short-read") && len) --len;
    *got = (UINT)fread(buf,1,len,input);
    fp->fptr += *got;
    return ferror(input) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *fp, const void *buf, UINT len, UINT *got) {
    if(fp != &dest) return FR_INT_ERR;
    if(!strcmp(fault,"short-write") && len) --len;
    *got = (UINT)fwrite(buf,1,len,output);
    if((fp->fptr += *got) > fp->obj.objsize) fp->obj.objsize = fp->fptr;
    return *got == len ? FR_OK : FR_DISK_ERR;
}
FRESULT f_lseek(FIL *fp, FSIZE_t off) {
    if(off > fp->obj.objsize) return FR_INT_ERR;
    FILE *stream = fp == &source ? input : output;
    if(fseek(stream,(long)off,SEEK_SET)) return FR_DISK_ERR;
    fp->fptr = off;
    return FR_OK;
}
int main(int argc, char **argv) {
    if(argc < 3 || !(input = fopen(argv[1],"rb")) || !(output = fopen(argv[2],"wb+"))) return 2;
    if(fseek(input,0,SEEK_END) || ftell(input)<0) return 2;
    source.obj.objsize = (FSIZE_t)ftell(input);
    stop_at = argc > 3 ? atoi(argv[3]) : 0;
    unsigned start = argc > 4 ? (unsigned)atoi(argv[4]) : 0;
    if(argc > 5) fault = argv[5];
    enum kui_preview_conversion result = kui_internet_preview_convert(&source,&dest,start,cancel,logger,NULL);
    if(fclose(input) || fclose(output)) return 2;
    switch(result) {
    case KUI_CONVERSION_OK: return 0;
    case KUI_CONVERSION_UNPLAYABLE: fputs("Unplayable source\n",stderr); break;
    case KUI_CONVERSION_IO_ERROR: fputs("Card I/O failed\n",stderr); break;
    case KUI_CONVERSION_NO_MEMORY: fputs("Out of memory\n",stderr); break;
    case KUI_CONVERSION_STOPPED: fputs("Cancelled\n",stderr); break;
    }
    return 1;
}
