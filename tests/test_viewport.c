/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/viewport.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    uint16_t p[2*640+2];p[0]=123;p[1281]=456;
    for(unsigned i=0;i<1280;i++) p[i+1]=(uint16_t)i;
    kui_viewport_inset(p+1,640,2,0,0xffff);assert(p[101]==100);
    kui_viewport_inset(p+1,640,2,32,0xffff);
    assert(p[0]==123 && p[1281]==456);
    assert(p[1]==0xffff && p[32]==0xffff && p[33]==0 && p[608]==639 && p[609]==0xffff);
    assert(p[673]==640 && p[1248]==1279);
    for(unsigned y=0;y<2;y++) for(unsigned x=1;x<576;x++)
        assert(p[1+y*640+32+x]>=p[y*640+32+x]);
    /* The cached column table must match the original per-pixel formula for
     * every setting, including after switching settings back and forth. */
    static const unsigned insets[]={16,48,32,16,320-1,1,48};
    static uint16_t frame[3*640],expected[3*640];
    for(unsigned k=0;k<sizeof(insets)/sizeof(insets[0]);k++) {
        unsigned inset=insets[k],inner=640-2*inset;
        for(unsigned i=0;i<3*640;i++) frame[i]=expected[i]=(uint16_t)(i*7u+k);
        for(unsigned y=0;y<3;y++) {
            uint16_t row[640];
            for(unsigned x=0;x<640;x++) row[x]=expected[y*640+x];
            for(unsigned x=0;x<inset;x++) expected[y*640+x]=expected[y*640+639-x]=0x0864;
            for(unsigned x=0;x<inner;x++) expected[y*640+inset+x]=row[x*639u/(inner-1)];
        }
        kui_viewport_inset(frame,640,3,inset,0x0864);
        for(unsigned i=0;i<3*640;i++) assert(frame[i]==expected[i]);
    }
    puts("viewport: bounds, preserved endpoints, ordered pixels and cached columns passed");
}
