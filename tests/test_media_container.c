/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/media_container.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The container reader should fetch only headers/the seek table, even if the
 * represented file is many megabytes or claims untrusted payload lengths. */
static uint8_t header[8 + 4*(KUI_MANUAL_PAGES_MAX+1u)];
static unsigned reads;
static unsigned mock_payload;
FRESULT f_read(FIL *file, void *buf, UINT count, UINT *got) {
    if(mock_payload && file->fptr >= sizeof(header)) {
        assert(file->fptr+count <= f_size(file));
        memset(buf,mock_payload==1 ? 0 : 4,count);
        *got=count; file->fptr+=count;
        return FR_OK;
    }
    assert(file->fptr + count <= sizeof(header));
    if(file->fptr >= f_size(file)) count = 0;
    else if(count > f_size(file)-file->fptr) count = (UINT)(f_size(file)-file->fptr);
    memcpy(buf,header+file->fptr,count);
    *got=count;
    file->fptr+=count;
    ++reads;
    return FR_OK;
}
FRESULT f_lseek(FIL *file, FSIZE_t pos) {
    assert(pos <= f_size(file));
    file->fptr=pos;
    return FR_OK;
}
static void u32(unsigned off, uint32_t v) {
    for(unsigned i=0;i<4;++i) header[off+i]=(uint8_t)(v>>(8*i));
}
static void setup_preview(FIL *f,unsigned count) {
    memcpy(header,"KPV1\xa0\x00\x78\x00\x0a\x00\x00\x00",12);
    u32(12,count);
    f->obj.objsize=16u+38400u*count;
}
static void setup_manual(FIL *f,unsigned pages,unsigned bytes) {
    memcpy(header,"KDM1",4);
    u32(4,pages);
    unsigned off=8u+(pages+1)*4u;
    for(unsigned i=0;i<=pages;++i) u32(8u+4u*i,off+i*bytes);
    f->obj.objsize=off+pages*bytes;
}
static bool stop(void) { return true; }
static void jpeg_pages(FIL *file) {
    /* A real SOF0/4:2:0 header and EOI; the envelope check deliberately skips
     * scan data, which the viewer decodes on demand. */
    const uint8_t page[] = {0xff,0xd8,0xff,0xe0,0,4,0,0,0xff,0xc0,0,17,
        8,1,44,2,0,3,1,0x22,0,2,0x11,1,3,0x11,1,0xff,0xda,0,12,3,1,0,2,17,3,17,0,63,0,42,0xff,0xd9};
    setup_manual(file,2,sizeof(page));
    unsigned off=20u;
    memcpy(header+off,page,sizeof(page)); memcpy(header+off+sizeof(page),page,sizeof(page));
    assert(kui_manual_check_pages(file,NULL));
    assert(!kui_manual_check_pages(file,stop));
    uint8_t *next=header+off+sizeof(page);
    next[15]=5;
    assert(!kui_manual_check_pages(file,NULL)); /* second page over 1024 wide */
    next[15]=page[15];next[sizeof(page)-1]=0;
    assert(!kui_manual_check_pages(file,NULL));
}
int main(void) {
    FIL f={0};
    struct kui_media_index idx;
    setup_preview(&f,100);
    assert(kui_media_container_index(&f,false,&idx) && idx.count==100 && f_tell(&f)==16 && reads==1);
    assert(idx.version==1 && !idx.audio_rate);
    header[3]='2';
    assert(kui_media_container_index(&f,false,&idx) && idx.version==2 && !idx.audio_rate);
    header[10]=0x80; header[11]=0x3e;
    assert(!kui_media_container_index(&f,false,&idx));
    f.obj.objsize+=3200u*100;
    assert(kui_media_container_index(&f,false,&idx) && idx.audio_rate==16000);
    assert(!kui_preview_load_audio(&f,100,16000,stop));
    assert(!kui_preview_load_audio(&f,100,0,NULL));
    mock_payload=1; assert(!kui_preview_load_audio(&f,100,16000,NULL));
    mock_payload=2;
    int16_t *pcm=kui_preview_load_audio(&f,100,16000,NULL);
    assert(pcm && pcm[159999]==0x0404); free(pcm);
    mock_payload=0;
    header[3]='1'; assert(!kui_media_container_index(&f,false,&idx));
    header[3]='2'; header[10]--; assert(!kui_media_container_index(&f,false,&idx));
    for(unsigned n=0;n<16;++n) {
        setup_preview(&f,100); f.obj.objsize=n;
        assert(!kui_media_container_check(&f,false));
    }
    for(unsigned i=0;i<12;++i) {
        setup_preview(&f,2);header[i]^=1;
        assert(!kui_media_container_check(&f,false));
    }
    setup_preview(&f,0);assert(!kui_media_container_check(&f,false));
    setup_preview(&f,101);assert(!kui_media_container_check(&f,false));
    setup_preview(&f,1);f.obj.objsize++;assert(!kui_media_container_check(&f,false));
    setup_preview(&f,1);f.obj.objsize--;assert(!kui_media_container_check(&f,false));

    setup_manual(&f,256,9);
    reads=0;
    assert(kui_media_container_index(&f,true,&idx) && idx.count==256 && idx.offsets[256]==f_size(&f) && reads==2);
    setup_manual(&f,2,9);
    assert(kui_media_container_check(&f,true));
    for(unsigned n=0;n<20;++n) {
        setup_manual(&f,2,9); f.obj.objsize=n;
        assert(!kui_media_container_check(&f,true));
    }
    for(unsigned i=0;i<8;++i) {
        setup_manual(&f,2,9);header[i]^=1;
        assert(!kui_media_container_check(&f,true));
    }
    setup_manual(&f,2,9);u32(8,19);assert(!kui_media_container_check(&f,true));
    setup_manual(&f,2,9);u32(12,20);assert(!kui_media_container_check(&f,true));
    setup_manual(&f,2,9);u32(12,18);assert(!kui_media_container_check(&f,true));
    setup_manual(&f,2,9);f.obj.objsize--;assert(!kui_media_container_check(&f,true));
    setup_manual(&f,2,512u*1024u+1);assert(!kui_media_container_check(&f,true));
    setup_manual(&f,64,512u*1024u);assert(!kui_media_container_check(&f,true));
    setup_manual(&f,63,512u*1024u);assert(kui_media_container_check(&f,true));
    jpeg_pages(&f);
    puts("Manual and preview headers, offsets, truncation and size limits passed");
    return 0;
}
