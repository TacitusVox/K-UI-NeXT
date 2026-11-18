/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/splash.h"
#include "kui/theme.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The same generated streams the firmware embeds, with the CRC-32 of the
 * exact RGB565 pixels tools/build_splash.py compressed. */
#include "../build/splash_art.inc"
#include "../build/boot_splash_art.inc"

static uint16_t guarded[640u*480u+2u];
static uint16_t first[640u*480u],expected_dimmed[640u*480u];
static uint8_t damaged[sizeof(kui_splash_art)+1u];

static uint32_t crc32(const void *data,size_t bytes) {
    const uint8_t *p=data;uint32_t crc=0xffffffffu;
    while(bytes--) {
        crc^=*p++;
        for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
static void fill(uint16_t value) {for(unsigned i=1;i<=640u*480u;i++) guarded[i]=value;}

int main(void) {
    guarded[0]=0x1234;guarded[640u*480u+1u]=0xabcd;
    /* Runtime splash: every pixel, within bounds, identical on repeat draws. */
    fill(0x5555);kui_splash_draw(guarded+1);
    assert(guarded[0]==0x1234 && guarded[640u*480u+1u]==0xabcd);
    assert(crc32(guarded+1,KUI_SPLASH_FRAME_BYTES)==KUI_SPLASH_ART_PIXELS_CRC32);
    memcpy(first,guarded+1,sizeof(first));
    fill(0xffff);kui_splash_draw(guarded+1);
    assert(!memcmp(first,guarded+1,sizeof(first)));
    /* Released after the splash; a later draw inflates again. */
    kui_splash_release();kui_splash_release();
    fill(0);kui_splash_draw(guarded+1);
    assert(!memcmp(first,guarded+1,sizeof(first)));
    kui_splash_release();
    kui_splash_draw(NULL);

    /* A worker-owned preview has no shared lifetime with the startup cache. */
    uint16_t *preview=kui_splash_preview_decode();
    assert(preview && !memcmp(first,preview,sizeof(first)));
    kui_splash_release();
    assert(!memcmp(first,preview,sizeof(first)));
    preview[0]^=0xffffu;
    fill(0);kui_splash_draw(guarded+1);
    assert(!memcmp(first,guarded+1,sizeof(first)));
    kui_splash_release();free(preview);

    /* Compare the factory fallback directly against the card pack's pixel payload. */
    FILE *packed=fopen("resources/themes/moonlit-muramasa/splash.kuiimg","rb");
    assert(packed && !fseek(packed,32,SEEK_SET));
    assert(fread(expected_dimmed,1,KUI_SPLASH_FRAME_BYTES,packed)==KUI_SPLASH_FRAME_BYTES);
    assert(fgetc(packed)==EOF && !ferror(packed) && !fclose(packed));
    kui_splash_draw_factory(guarded+1);
    assert(!memcmp(expected_dimmed,guarded+1,sizeof(expected_dimmed)));
    assert(memcmp(first,guarded+1,sizeof(first)));
    preview=kui_splash_preview_factory_decode();
    assert(preview && !memcmp(preview,expected_dimmed,sizeof(expected_dimmed)));
    free(preview);
    /* Both picker thumbnails match the exact full preview, including rows
     * which start in one inflater dictionary flush and finish in the next. */
    for(unsigned factory=0;factory<2;factory++) {
        const uint16_t *source=factory?expected_dimmed:first;
        uint16_t *thumbnail=kui_splash_thumbnail_decode(factory!=0);
        assert(thumbnail);
        for(unsigned y=0;y<KUI_THEME_THUMB_HEIGHT;y++)
            for(unsigned x=0;x<KUI_THEME_THUMB_WIDTH;x++)
                assert(thumbnail[y*KUI_THEME_THUMB_WIDTH+x]==source[(y*5u/2u)*640u+x*5u/2u]);
        free(thumbnail);
    }
    kui_splash_draw(guarded+1);assert(!memcmp(first,guarded+1,sizeof(first)));
    kui_splash_draw_factory(guarded+1);assert(!memcmp(expected_dimmed,guarded+1,sizeof(expected_dimmed)));
    kui_splash_release();
    /* Boot backdrop: exact pixels; dimming reads RAM, not the old frame. */
    fill(0x5555);kui_boot_splash_draw(guarded+1);
    assert(guarded[0]==0x1234 && guarded[640u*480u+1u]==0xabcd);
    assert(crc32(guarded+1,KUI_SPLASH_FRAME_BYTES)==KUI_BOOT_SPLASH_ART_PIXELS_CRC32);
    for(unsigned i=0;i<640u*480u;i++) {
        expected_dimmed[i]=(uint16_t)((guarded[i+1]&0xe79cu)>>2);
        guarded[i+1]=0xffff;
    }
    kui_boot_splash_draw_dimmed(guarded+1);
    assert(guarded[0]==0x1234 && guarded[640u*480u+1u]==0xabcd);
    assert(!memcmp(expected_dimmed,guarded+1,sizeof(expected_dimmed)));
    kui_boot_splash_draw(NULL);
    kui_boot_splash_draw_dimmed(NULL);

    /* The decoder accepts only a complete, undamaged stream. */
    static uint16_t frame[640u*480u];
    assert(kui_splash_art_inflate(kui_splash_art,sizeof(kui_splash_art),frame));
    assert(!memcmp(first,frame,sizeof(frame)));
    memcpy(damaged,kui_splash_art,sizeof(kui_splash_art));
    damaged[sizeof(kui_splash_art)/2u]^=0x10u;
    assert(!kui_splash_art_inflate(damaged,sizeof(kui_splash_art),frame));
    memcpy(damaged,kui_splash_art,sizeof(kui_splash_art));
    assert(!kui_splash_art_inflate(damaged,sizeof(kui_splash_art)-1u,frame));      /* truncated */
    damaged[sizeof(kui_splash_art)]=0;
    assert(!kui_splash_art_inflate(damaged,sizeof(kui_splash_art)+1u,frame));      /* trailing byte */
    assert(!kui_splash_art_inflate(kui_boot_splash_art,sizeof(kui_boot_splash_art),NULL));
    assert(!kui_splash_art_inflate(NULL,10u,frame) && !kui_splash_art_decode(damaged,0u));
    printf("PASS compressed splash and boot art: %zu and %zu bytes inflate to the exact RGB565 pixels; "
        "repeat/release draws, RAM-only dimming and damaged streams checked\n",
        sizeof(kui_splash_art),sizeof(kui_boot_splash_art));
    return 0;
}
