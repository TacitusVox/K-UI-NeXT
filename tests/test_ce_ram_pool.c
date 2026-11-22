/* SPDX-License-Identifier: GPL-3.0-only */
/* The universal Windows CE RAM pool plan (include/kui/ce_ram_pool.h) over
 * every kernel layout the data probe recorded: synthetic images that carry
 * only each surveyed ROM header's derived fields at its surveyed offset
 * (docs/evidence/wince-kernel-ram-layouts-2026-11-21.json, and Worms World
 * Party's from docs/evidence/wince-pool-session-2026-11-21.json; no kernel
 * bytes), and every refusal. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kui/ce_ram_pool.h"

#define BASE UINT32_C(0x8c010000)
#define KEEP UINT32_C(0x400000)

struct layout { uint32_t bytes, header, physlast, ram_start, ram_free, ram_end, drivglob_len; };
static const struct layout layouts[] = {
    {819200u, 817060u, 817628u, UINT32_C(0x8c0d8000), UINT32_C(0x8c0e1000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0xf6f9a169 */
    {1069056u, 1067512u, 1068304u, UINT32_C(0x8c115000), UINT32_C(0x8c121000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0x47a76557 */
    {1107968u, 1105520u, 1106348u, UINT32_C(0x8c11f000), UINT32_C(0x8c129000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0xcd686951 */
    {1169408u, 1166904u, 1167796u, UINT32_C(0x8c12e000), UINT32_C(0x8c139000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0xda53fed5 */
    {1445888u, 1444352u, 1445336u, UINT32_C(0x8c171000), UINT32_C(0x8c17d000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0x0b4d5af3 */
    {4345856u, 4343556u, 4344712u, UINT32_C(0x8c435000), UINT32_C(0x8c441000), UINT32_C(0x8cef0000), UINT32_C(0x110000)}, /* 0xdc875f41 */
    {1064960u, 1063716u, 1064452u, UINT32_C(0x8c114000), UINT32_C(0x8c120000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x5ec7ec3f */
    {1114112u, 1112176u, 1113004u, UINT32_C(0x8c120000), UINT32_C(0x8c12c000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0xa8bc830c */
    {1157120u, 1155072u, 1155960u, UINT32_C(0x8c12b000), UINT32_C(0x8c138000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0xc3823cca */
    {1204224u, 1202236u, 1203000u, UINT32_C(0x8c136000), UINT32_C(0x8c144000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x6eb69d17 */
    {1251328u, 1250336u, 1251192u, UINT32_C(0x8c142000), UINT32_C(0x8c150000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0xb9f755bc */
    {1320960u, 1319196u, 1320020u, UINT32_C(0x8c153000), UINT32_C(0x8c160000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x13182ebc */
    {1353728u, 1351468u, 1352420u, UINT32_C(0x8c15b000), UINT32_C(0x8c168000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x31294673 */
    {1630208u, 1628032u, 1629272u, UINT32_C(0x8c19e000), UINT32_C(0x8c1ac000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x6d147148 */
    {1695744u, 1694068u, 1695120u, UINT32_C(0x8c1ae000), UINT32_C(0x8c1bc000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x29bf6186 */
    {1826816u, 1824276u, 1825324u, UINT32_C(0x8c1ce000), UINT32_C(0x8c1dc000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x12f68791 */
    {1828864u, 1827228u, 1828276u, UINT32_C(0x8c1cf000), UINT32_C(0x8c1dc000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0xf6902800 */
    {2422784u, 2420232u, 2421216u, UINT32_C(0x8c260000), UINT32_C(0x8c26c000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0x1c0f852b */
    {3352576u, 3349704u, 3350976u, UINT32_C(0x8c343000), UINT32_C(0x8c350000), UINT32_C(0x8cfd0000), UINT32_C(0x30000)}, /* 0xb370f288 */
    {1200128u, 1198860u, 1199748u, UINT32_C(0x8c135000), UINT32_C(0x8c144000), UINT32_C(0x8cff0000), UINT32_C(0x10000)}, /* 0x9eca10d4 */
};

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_header(uint8_t *image, const struct layout *l, uint32_t at) {
    uint8_t *h = image + at;
    memset(h, 0, KUI_CE_ROMHDR_BYTES);
    put32(h + 0u, UINT32_C(0x01d40000)); put32(h + 4u, UINT32_C(0x02000000));
    put32(h + 8u, BASE); put32(h + 12u, BASE + l->physlast);
    put32(h + 16u, 21u);
    put32(h + 20u, l->ram_start); put32(h + 24u, l->ram_free); put32(h + 28u, l->ram_end);
    put32(h + 60u, l->ram_end); put32(h + 64u, l->drivglob_len);
}
static uint8_t *image_of(const struct layout *l) {
    uint8_t *image = calloc(1, l->bytes);
    assert(image);
    put_header(image, l, l->header);
    return image;
}
static enum kui_ce_ram_pool_result plan(const uint8_t *image, uint32_t bytes, uint32_t pool,
    struct kui_ce_ram_pool_plan *out) {
    return kui_ce_ram_pool_plan(image, BASE, bytes, pool, KEEP, out);
}

static unsigned test_surveyed(void) {
    unsigned checks = 0;
    for(size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        const struct layout *l = &layouts[i];
        uint8_t *image = image_of(l), *copy = malloc(l->bytes);
        assert(copy); memcpy(copy, image, l->bytes);
        static const uint32_t pools[] = {0x10000u, 0x80000u};
        for(size_t p = 0; p < 2; ++p) {
            struct kui_ce_ram_pool_plan out;
            memset(&out, 0xa5, sizeof(out));
            assert(plan(image, l->bytes, pools[p], &out) == KUI_CE_RAM_POOL_OK);
            assert(out.header == BASE + l->header);
            assert(out.ram_start == l->ram_start && out.ram_free == l->ram_free && out.ram_end == l->ram_end);
            assert(out.pool_end == l->ram_end && out.pool_begin == l->ram_end - pools[p]);
            assert(!(out.pool_begin & 0xffffu) && out.pool_begin - out.ram_free >= KEEP);
            assert(!memcmp(image, copy, l->bytes)); /* the plan writes nothing */
            checks += 6;
        }
        free(copy); free(image);
    }
    return checks;
}

static unsigned test_refusals(void) {
    unsigned checks = 0;
    const struct layout *l = &layouts[0];
    struct kui_ce_ram_pool_plan out, untouched;
    memset(&untouched, 0x5a, sizeof(untouched));
#define REFUSED(call, want) do { out = untouched; assert((call) == (want)); \
        assert(!memcmp(&out, &untouched, sizeof(out))); ++checks; } while(0)
    uint8_t *image = image_of(l);
    /* Arguments. */
    REFUSED(kui_ce_ram_pool_plan(NULL, BASE, l->bytes, 0x10000u, KEEP, &out), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(kui_ce_ram_pool_plan(image, BASE, l->bytes, 0x10000u, KEEP, NULL), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(kui_ce_ram_pool_plan(image, BASE + 2u, l->bytes, 0x10000u, KEEP, &out), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(kui_ce_ram_pool_plan(image, BASE, 83u, 0x10000u, KEEP, &out), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(plan(image, l->bytes, 0u, &out), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(plan(image, l->bytes, 0x8000u, &out), KUI_CE_RAM_POOL_ARGUMENT);
    REFUSED(plan(image, l->bytes, 0x18000u, &out), KUI_CE_RAM_POOL_ARGUMENT);
    /* A second header anywhere: which one CE uses is not known. */
    put_header(image, l, 0x100u);
    REFUSED(plan(image, l->bytes, 0x10000u, &out), KUI_CE_RAM_POOL_AMBIGUOUS);
    memset(image + 0x100u, 0, KUI_CE_ROMHDR_BYTES);
    assert(plan(image, l->bytes, 0x10000u, &out) == KUI_CE_RAM_POOL_OK); ++checks;
    /* One field off at a time: no header. */
    static const struct { uint32_t offset, value; } broken[] = {
        {8u, BASE + 0x1000u},                    /* another load address */
        {12u, BASE},                             /* empty image */
        {20u, UINT32_C(0x8c000000)},             /* RAM starts inside the image */
        {24u, UINT32_C(0x8c0d0000)},             /* RAMFree below RAMStart */
        {28u, UINT32_C(0x8cef0800)},             /* RAM end off a page */
        {60u, UINT32_C(0x8cee0000)},             /* driver globals elsewhere */
        {64u, UINT32_C(0x100000)},               /* short of the top of RAM */
    };
    for(size_t b = 0; b < sizeof(broken) / sizeof(broken[0]); ++b) {
        uint8_t saved[4];
        memcpy(saved, image + l->header + broken[b].offset, 4);
        put32(image + l->header + broken[b].offset, broken[b].value);
        REFUSED(plan(image, l->bytes, 0x10000u, &out), KUI_CE_RAM_POOL_NO_HEADER);
        memcpy(image + l->header + broken[b].offset, saved, 4);
    }
    /* A RAM end off a page, the driver globals moved with it: not CE's layout. */
    put32(image + l->header + 28u, UINT32_C(0x8cef0800));
    put32(image + l->header + 60u, UINT32_C(0x8cef0800));
    put32(image + l->header + 64u, KUI_CE_RAM_TOP - UINT32_C(0x8cef0800));
    REFUSED(plan(image, l->bytes, 0x10000u, &out), KUI_CE_RAM_POOL_NO_HEADER);
    put_header(image, l, l->header);
    memset(image + l->header, 0, KUI_CE_ROMHDR_BYTES);
    REFUSED(plan(image, l->bytes, 0x10000u, &out), KUI_CE_RAM_POOL_NO_HEADER);
    /* The header in the image's last 84 bytes is still found. */
    put_header(image, l, l->bytes - KUI_CE_ROMHDR_BYTES);
    assert(plan(image, l->bytes, 0x10000u, &out) == KUI_CE_RAM_POOL_OK &&
        out.header == BASE + l->bytes - KUI_CE_ROMHDR_BYTES); ++checks;
    free(image);
    /* Too little RAM left for CE, or a RAM end off the pool's alignment. */
    struct layout tight = *l;
    tight.ram_free = tight.ram_end - KEEP - 0x8000u;
    image = image_of(&tight);
    REFUSED(plan(image, tight.bytes, 0x10000u, &out), KUI_CE_RAM_POOL_TOO_SMALL);
    free(image);
    struct layout odd = *l;
    odd.ram_end = UINT32_C(0x8cef1000); odd.drivglob_len = KUI_CE_RAM_TOP - odd.ram_end;
    image = image_of(&odd);
    REFUSED(plan(image, odd.bytes, 0x10000u, &out), KUI_CE_RAM_POOL_TOO_SMALL);
    free(image);
    return checks;
#undef REFUSED
}

int main(void) {
    unsigned checks = test_surveyed();
    checks += test_refusals();
    printf("CE RAM pool: %zu surveyed kernel layouts, %u checks passed\n",
        sizeof(layouts) / sizeof(layouts[0]), checks);
    return 0;
}
