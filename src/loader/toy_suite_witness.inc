/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/retail_loader_layout.h"
#include "kui/toy_suite_witness.h"

#ifdef KUI_TOY_WITNESS_HOST_TEST
extern const uint8_t *kui_toy_witness_test_read(uint32_t);
#endif

static unsigned toy_witness_diff(const uint8_t *expected,const uint8_t *actual) {
    unsigned at=0u;
    typedef uint32_t word __attribute__((__may_alias__));
    if(!(((uintptr_t)expected|(uintptr_t)actual)&3u)) {
        for(;at<KUI_GAME_DATA_BYTES;at+=sizeof(word))
            if(*(const word *)(expected+at)!=*(const word *)(actual+at))break;
    }
    for(;at<KUI_GAME_DATA_BYTES;at++)if(expected[at]!=actual[at])break;
    return at;
}

/* Runs after native GD publishes the terminal result, still under the
 * serialized hook. Read the actual destination from P2, like write_out()
 * after it purges the relevant P1 lines. No storage or cache operations. */
static void toy_suite_witness_complete(const kui_retail_async_region_type *r) {
    const struct kui_retail_gd *s=&r->shared.service;
    const struct kui_retail_async *e=&r->engine;
    const struct kui_toy_suite_seed *seed=&C.config.suite_seed;
    if(!seed->count[0] || !s->initialized || !s->token || s->token!=e->token ||
       s->pending || s->executing || s->error || s->status!=KUI_GD_COMPLETED ||
       (s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD) ||
       s->sector_bytes!=KUI_GAME_DATA_BYTES || !s->count ||
       s->lba>=KUI_GAME_LBA_LIMIT || s->count>KUI_GAME_LBA_LIMIT-s->lba ||
       e->active || e->failed || e->native_loan_state || e->cursor.raw ||
       !e->manifest || e->cursor.manifest!=e->manifest || e->cursor.lba!=s->lba ||
       e->cursor.count!=s->count || e->cursor.done!=s->count)return;

    const uint32_t bytes=s->count*KUI_GAME_DATA_BYTES;
    const uint32_t dest=e->destination;
    const uint32_t physical=dest&UINT32_C(0x1fffffff);
    const uint32_t lease=C.config.lease;
    if(s->request_bytes!=bytes || s->completed_bytes!=bytes ||
       dest!=((s->destination&UINT32_C(0x00ffffff))|UINT32_C(0x8c000000)) ||
       dest<KUI_RETAIL_IP_ADDRESS || dest<s->guest_begin ||
       dest>=KUI_RETAIL_RAM_END || dest>=s->guest_end ||
       bytes>KUI_RETAIL_RAM_END-dest || bytes>s->guest_end-dest ||
       (physical<lease+C.config.lease_bytes && physical+bytes>lease-32u))return;

    const uint32_t end=s->lba+s->count;
    const uint8_t *expected=C.config.data;
    for(unsigned w=0u;w<2u;w++,expected+=KUI_TOY_WITNESS_SECTORS*KUI_GAME_DATA_BYTES) {
        uint32_t first=seed->base[w],count=KUI_TOY_WITNESS_SECTORS;
        if(seed->count[w]!=count || first>=end || first+count<=s->lba)continue;
        uint32_t lba=first>s->lba?first:s->lba;
        uint32_t last=first+count<end?first+count:end;
        for(;lba<last;lba++) {
            uint32_t address=(dest+(lba-s->lba)*KUI_GAME_DATA_BYTES)|UINT32_C(0x20000000);
#ifdef KUI_TOY_WITNESS_HOST_TEST
            const uint8_t *actual=kui_toy_witness_test_read(address);
#else
            const uint8_t *actual=(const void *)(uintptr_t)address;
#endif
            const uint8_t *wanted=expected+(lba-first)*KUI_GAME_DATA_BYTES;
            unsigned byte=toy_witness_diff(wanted,actual);
            ++C.suite[1];
            if(byte<KUI_GAME_DATA_BYTES && !C.suite[2]) {
                C.suite[2]=lba; C.suite[3]=byte;
                C.suite[4]=(uint32_t)wanted[byte]<<8|actual[byte];
            }
        }
    }
}
