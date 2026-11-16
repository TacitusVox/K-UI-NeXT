/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hash.h"
#include "kui/native_readahead.h"
#include "kui/toy_suite_witness.h"
#include <assert.h>
#include <string.h>

static struct kui_retail_image image;
static struct kui_retail_manifest manifest;
static struct kui_toy_movie_range toy_movie_ranges[4];
static struct kui_toy_suite_seed toy_suite_seed;
static _Alignas(32) uint8_t toy_suite_data[KUI_NATIVE_RA_DATA_BYTES];
static uint8_t raw_boot[16u*KUI_GAME_RAW_BYTES];
static uint32_t bad_edc_lba,bad_fad_lba;
static bool cooked;

static uint8_t bcd(unsigned n) { return (uint8_t)((n/10u)*16u+n%10u); }
static uint8_t data_byte(uint32_t lba,unsigned byte) { return (uint8_t)(lba+byte*7u); }
enum kui_game_result kui_retail_image_read(struct kui_retail_image *p,
    uint32_t lba,uint32_t count,enum kui_game_sector_format format,void *out,size_t bytes) {
    assert(p==&image && count && count<=16u && format==KUI_GAME_SECTOR_RAW &&
           out==raw_boot && bytes==count*KUI_GAME_RAW_BYTES);
    if(cooked)return KUI_GAME_UNSUPPORTED;
    for(uint32_t i=0u;i<count;i++) {
        uint8_t *raw=raw_boot+i*KUI_GAME_RAW_BYTES;
        uint32_t here=lba+i,fad=here+150u+(here==bad_fad_lba);
        memset(raw,0,KUI_GAME_RAW_BYTES);
        memset(raw+1u,255,10u);
        raw[12]=bcd(fad/4500u); raw[13]=bcd((fad/75u)%60u); raw[14]=bcd(fad%75u); raw[15]=1u;
        for(unsigned byte=0u;byte<KUI_GAME_DATA_BYTES;byte++)raw[16u+byte]=data_byte(here,byte);
        uint32_t edc=kui_cd_edc(raw,16u+KUI_GAME_DATA_BYTES);
        for(unsigned j=0u;j<4u;j++)raw[16u+KUI_GAME_DATA_BYTES+j]=(uint8_t)(edc>>(j*8u));
        if(here==bad_edc_lba)raw[16u]^=1u;
    }
    return KUI_GAME_OK;
}
#include "toy_suite_witness_stage.inc"

static struct {
    struct kui_native_ra_config config;
    uint32_t suite[5];
} C;
static _Alignas(32) uint8_t guest[4u*KUI_GAME_DATA_BYTES];
static const uint32_t guest_p1=UINT32_C(0x8c200000);
static unsigned observed_reads;
const uint8_t *kui_toy_witness_test_read(uint32_t p2) {
    uint32_t at=p2-(guest_p1|UINT32_C(0x20000000));
    assert(at<=sizeof(guest)-KUI_GAME_DATA_BYTES);
    ++observed_reads;
    return guest+at;
}
#include "toy_suite_witness.inc"

static void completed(kui_retail_async_region_type *r,uint32_t lba,uint32_t count,unsigned align) {
    memset(r,0,sizeof(*r));
    struct kui_retail_gd *s=&r->shared.service;
    struct kui_retail_async *e=&r->engine;
    s->initialized=1u;s->status=KUI_GD_COMPLETED;s->command=align?KUI_GD_PIOREAD:KUI_GD_DMAREAD;
    s->lba=lba;s->count=count;s->token=1u;s->sector_bytes=KUI_GAME_DATA_BYTES;
    s->destination=guest_p1+align;s->guest_begin=guest_p1;s->guest_end=guest_p1+sizeof(guest);
    s->request_bytes=s->completed_bytes=count*KUI_GAME_DATA_BYTES;
    e->token=s->token;e->destination=s->destination;e->manifest=e->cursor.manifest=&manifest;
    e->cursor.lba=lba;e->cursor.count=e->cursor.done=count;
    for(uint32_t sector=0u;sector<count;sector++)
        for(unsigned byte=0u;byte<KUI_GAME_DATA_BYTES;byte++)
            guest[align+sector*KUI_GAME_DATA_BYTES+byte]=data_byte(lba+sector,byte);
}

int main(void) {
    toy_movie_ranges[3]=(struct kui_toy_movie_range){460000u,12000u};
    bad_edc_lba=UINT32_C(0x72858);
    assert(!toy_suite_witness_seed() && !toy_suite_seed.count[0] && !toy_suite_seed.count[1]);
    bad_edc_lba=0u;bad_fad_lba=UINT32_C(0x724a0);
    assert(!toy_suite_witness_seed() && !toy_suite_seed.count[0]);
    bad_fad_lba=0u;cooked=true;
    assert(!toy_suite_witness_seed() && !toy_suite_seed.count[0]);
    cooked=false;assert(toy_suite_witness_seed());
    assert(toy_suite_seed.count[0]==48u && toy_suite_seed.count[1]==48u);
    C.config.suite_seed=toy_suite_seed;C.config.data=toy_suite_data;
    C.config.lease=UINT32_C(0x0c500000);C.config.lease_bytes=KUI_NATIVE_RA_LEASE_BYTES;

    kui_retail_async_region_type region;
    completed(&region,toy_suite_seed.base[0]-1u,3u,0u);
    toy_suite_witness_complete(&region);
    assert(C.suite[1]==2u && observed_reads==2u && !C.suite[2]);
    completed(&region,toy_suite_seed.base[0]+48u,1u,0u);
    toy_suite_witness_complete(&region);
    assert(C.suite[1]==2u && observed_reads==2u);

    const uint32_t lba=toy_suite_seed.base[1]+5u;
    const unsigned byte=KUI_GAME_DATA_BYTES-7u;
    completed(&region,lba,2u,2u);
    guest[2u+byte]^=0xa0u;
    toy_suite_witness_complete(&region);
    assert(C.suite[1]==4u && observed_reads==4u && C.suite[2]==lba && C.suite[3]==byte &&
           C.suite[4]==((uint32_t)data_byte(lba,byte)<<8|guest[2u+byte]));

    for(unsigned invalid=0u;invalid<6u;invalid++) {
        completed(&region,lba,1u,0u);
        struct kui_retail_gd *s=&region.shared.service;
        switch(invalid) {
        case 0u:s->status=KUI_GD_FAILED;break;
        case 1u:s->completed_bytes--;break;
        case 2u:s->token++;break;
        case 3u:s->guest_end=guest_p1+1u;break;
        case 4u:C.config.lease=guest_p1&UINT32_C(0x1fffffff);break;
        case 5u:region.engine.native_loan_state=1u;break;
        }
        toy_suite_witness_complete(&region);
        assert(C.suite[1]==4u && observed_reads==4u);
        C.config.lease=UINT32_C(0x0c500000);
    }
    return 0;
}
