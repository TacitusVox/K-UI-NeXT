/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t edc_reference(const uint8_t *bytes,size_t count) {
    uint32_t edc=0;
    while(count--) {
        edc^=*bytes++;
        for(unsigned bit=0;bit<8u;++bit)
            edc=(edc>>1)^((edc&1u)?UINT32_C(0xd8018001):0u);
    }
    return edc;
}

static void fill(uint8_t *bytes,size_t count) {
    uint32_t state=UINT32_C(0x6351a5ed);
    while(count--) {
        state^=state<<13;
        state^=state>>17;
        state^=state<<5;
        *bytes++=(uint8_t)state;
    }
}

static uint8_t bcd(unsigned n) { return (uint8_t)((n/10u)*16u+n%10u); }

static void store_edc(uint8_t *bytes,unsigned begin,unsigned end) {
    const uint32_t edc=edc_reference(bytes+begin,end-begin);
    for(unsigned i=0;i<4u;++i)bytes[end+i]=(uint8_t)(edc>>(i*8u));
}

static void test_lengths(void) {
    _Alignas(32) uint8_t bytes[KUI_RAW_BYTES+3u];
    fill(bytes,sizeof(bytes));
    assert(kui_cd_edc(NULL,0)==0u);
    for(unsigned alignment=0;alignment<4u;++alignment)
        for(size_t length=0;length<=KUI_RAW_BYTES;++length)
            assert(kui_cd_edc(bytes+alignment,length)==edc_reference(bytes+alignment,length));
}

static void test_sectors(void) {
    uint8_t raw[KUI_RAW_BYTES];
    static const uint32_t fads[]={150u,450001u,719999u};
    fill(raw,sizeof(raw));
    raw[0]=raw[11]=0u;
    memset(raw+1,255,10);
    raw[15]=1u;
    for(unsigned i=0;i<sizeof(fads)/sizeof(fads[0]);++i) {
        const uint32_t fad=fads[i];
        raw[12]=bcd(fad/4500u);
        raw[13]=bcd(fad/75u%60u);
        raw[14]=bcd(fad%75u);
        store_edc(raw,0,2064);
        assert(kui_sector_edc_valid_at(raw,fad));
        assert(!kui_sector_edc_valid_at(raw,fad+1u));
    }
    for(unsigned i=0;i<2068u;++i) {
        raw[i]^=1u;
        assert(!kui_sector_edc_valid_at(raw,719999u));
        raw[i]^=1u;
    }
    assert(kui_sector_edc_valid_at(raw,719999u));

    raw[15]=2u;
    raw[18]&=~0x20u;
    memcpy(raw+20,raw+16,4);
    store_edc(raw,16,2072);
    assert(kui_sector_edc_valid(raw));
    raw[24]^=0x80u;
    assert(!kui_sector_edc_valid(raw));
}

int main(void) {
    test_lengths();
    test_sectors();
    puts("CD EDC pair parity: all sector-length prefixes, alignments, and Mode 1/2 checks passed");
    return 0;
}
