/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_CE_RAM_POOL_H
#define KUI_CE_RAM_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A RAM pool for K-UI's own services below Windows CE's RAM end, taken by
 * lowering one word, ROMHDR.ulRAMEnd, before NK builds its page allocator.
 * The CD-music modules reserve their pool the same way, gated on one exact
 * header per kernel (kui/armada_ce_excerpt_pool.h); this finds the header by
 * its own structure instead, so one rule serves every kernel whose header
 * obeys the layout every surveyed kernel has (docs/wince-unified-runtime.md):
 *
 *   physfirst == the image's load address, physfirst < physlast <= its end;
 *   physlast <= ulRAMStart <= ulRAMFree < ulRAMEnd, ulRAMEnd page aligned;
 *   ulDrivglobStart == ulRAMEnd and ulRAMEnd + ulDrivglobLen == the top of RAM
 *   (the driver globals fill the RAM above CE's own).
 *
 * Exactly one such header must exist in the image. Nothing here writes
 * memory or touches CE state: the caller applies the plan's one word. */
#define KUI_CE_ROMHDR_BYTES 84u
#define KUI_CE_RAM_TOP UINT32_C(0x8d000000)
#define KUI_CE_RAM_POOL_ALIGN UINT32_C(0x10000)

struct kui_ce_ram_pool_plan {
    uint32_t header;            /* the ROM header's address (in the image) */
    uint32_t ram_start, ram_free, ram_end; /* the header's values as found */
    uint32_t pool_begin;        /* the new ulRAMEnd */
    uint32_t pool_end;          /* == ram_end */
};

enum kui_ce_ram_pool_result {
    KUI_CE_RAM_POOL_OK,
    KUI_CE_RAM_POOL_ARGUMENT,
    KUI_CE_RAM_POOL_NO_HEADER,
    KUI_CE_RAM_POOL_AMBIGUOUS,
    KUI_CE_RAM_POOL_TOO_SMALL
};

static inline uint32_t kui_ce_ram_pool_le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Whether the 84 bytes at h are a ROM header of the image loaded at base
 * (bytes long), by the layout above. */
static inline bool kui_ce_romhdr_layout_valid(const uint8_t *h, uint32_t base, uint32_t bytes) {
    const uint32_t physfirst = kui_ce_ram_pool_le32(h + 8u), physlast = kui_ce_ram_pool_le32(h + 12u);
    if(physfirst != base || physlast <= base || physlast - base > bytes) return false;
    const uint32_t ram_start = kui_ce_ram_pool_le32(h + 20u), ram_free = kui_ce_ram_pool_le32(h + 24u);
    const uint32_t ram_end = kui_ce_ram_pool_le32(h + 28u);
    const uint32_t drivglob_start = kui_ce_ram_pool_le32(h + 60u), drivglob_len = kui_ce_ram_pool_le32(h + 64u);
    return physlast <= ram_start && ram_start <= ram_free && ram_free < ram_end &&
        ram_end <= KUI_CE_RAM_TOP && !(ram_end & 0xfffu) &&
        drivglob_start == ram_end && drivglob_len == KUI_CE_RAM_TOP - ram_end;
}

/* image: the kernel body as loaded at base, bytes long (base word aligned).
 * pool_bytes: a non-zero multiple of 64 KiB. keep: the RAM CE must still have
 * between ulRAMFree and the new RAM end. Every refusal leaves out unchanged. */
static inline enum kui_ce_ram_pool_result kui_ce_ram_pool_plan(const uint8_t *image, uint32_t base,
    uint32_t bytes, uint32_t pool_bytes, uint32_t keep, struct kui_ce_ram_pool_plan *out) {
    if(!image || !out || (base & 3u) || bytes < KUI_CE_ROMHDR_BYTES || !pool_bytes ||
       (pool_bytes & (KUI_CE_RAM_POOL_ALIGN - 1u)))
        return KUI_CE_RAM_POOL_ARGUMENT;
    uint32_t found = 0, at = 0;
    for(uint32_t o = 0; o <= bytes - KUI_CE_ROMHDR_BYTES; o += 4u)
        if(kui_ce_romhdr_layout_valid(image + o, base, bytes)) {
            if(found++) return KUI_CE_RAM_POOL_AMBIGUOUS;
            at = o;
        }
    if(!found) return KUI_CE_RAM_POOL_NO_HEADER;
    const uint8_t *h = image + at;
    const uint32_t ram_start = kui_ce_ram_pool_le32(h + 20u), ram_free = kui_ce_ram_pool_le32(h + 24u);
    const uint32_t ram_end = kui_ce_ram_pool_le32(h + 28u);
    if((ram_end & (KUI_CE_RAM_POOL_ALIGN - 1u)) || ram_end - ram_free < pool_bytes ||
       ram_end - ram_free - pool_bytes < keep)
        return KUI_CE_RAM_POOL_TOO_SMALL;
    out->header = base + at;
    out->ram_start = ram_start; out->ram_free = ram_free; out->ram_end = ram_end;
    out->pool_begin = ram_end - pool_bytes; out->pool_end = ram_end;
    return KUI_CE_RAM_POOL_OK;
}

#endif
