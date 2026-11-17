/* SPDX-License-Identifier: GPL-3.0-only */
/* Portable part of boot-disc stage 1: no library calls, so the console build
 * links with libgcc only and the same code runs in the host tests. */
#include "kui/boot_copies.h"

#define CHUNK_BYTES (KUI_BOOT_COPIES_CHUNK_SECTORS * KUI_BOOT_COPIES_SECTOR_BYTES)
/* Rounds over every copy for the sectors still missing from a damaged chunk.
 * On a console a scratched sector has failed twice and then read on the
 * third attempt (docs/evidence/scratched-omikron-retry-2026-09-20.txt), and a
 * drive that briefly reports "not ready" fails every read for a moment. */
#define REBUILD_PASSES 4u

static const uint32_t nibble[16] = {
    0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu, 0x76dc4190u, 0x6b6b51f4u,
    0x4db26158u, 0x5005713cu, 0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
    0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};

static uint32_t crc_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    crc = (crc >> 4) ^ nibble[crc & 15u];
    return (crc >> 4) ^ nibble[crc & 15u];
}

/* Aligned data is read a 32-bit word at a time (little-endian SH-4/host):
 * the bootstrap is checked through its uncached alias, so this costs one
 * uncached access per four bytes instead of one per byte. */
uint32_t kui_stage1_crc32(uint32_t previous, const void *data, size_t bytes) {
    const uint8_t *p = data;
    uint32_t crc = ~previous;
    while(bytes && ((uintptr_t)p & 3u)) {crc = crc_byte(crc, *p++); --bytes;}
    const uint32_t *w = (const uint32_t *)(const void *)p;
    for(; bytes >= 4u; bytes -= 4u) {
        const uint32_t v = *w++;
        crc = crc_byte(crc, (uint8_t)v);
        crc = crc_byte(crc, (uint8_t)(v >> 8));
        crc = crc_byte(crc, (uint8_t)(v >> 16));
        crc = crc_byte(crc, (uint8_t)(v >> 24));
    }
    p = (const uint8_t *)w;
    while(bytes--) crc = crc_byte(crc, *p++);
    return ~crc;
}

bool kui_boot_copies_valid(const struct kui_boot_copies *t) {
    static const char magic[16] = KUI_BOOT_COPIES_MAGIC;
    if(!t) return false;
    for(unsigned i = 0; i < sizeof(magic); ++i)
        if(t->magic[i] != magic[i]) return false;
    /* A fixed chunk size keeps every division a shift: no libgcc divide. */
    if(t->version != KUI_BOOT_COPIES_VERSION || !t->image_bytes ||
       t->chunk_sectors != KUI_BOOT_COPIES_CHUNK_SECTORS ||
       !t->copy_count || t->copy_count > KUI_BOOT_COPIES_MAX_COPIES ||
       t->image_bytes > KUI_BOOT_COPIES_MAX_CHUNKS * CHUNK_BYTES ||
       t->chunk_count != (t->image_bytes + CHUNK_BYTES - 1u) / CHUNK_BYTES)
        return false;
    for(unsigned c = 0; c < t->copy_count; ++c)
        if(t->copy_fad[c] < 150u) return false;
    return kui_stage1_crc32(0, t, offsetof(struct kui_boot_copies, table_crc32)) == t->table_crc32;
}

static bool chunk_good(const struct kui_boot_copies *t, uint32_t index,
                       const uint8_t *data, uint32_t bytes) {
    return kui_stage1_crc32(0, data, bytes) == t->chunk_crc32[index];
}

/* Scratch for one sector from each copy while a damaged chunk is rebuilt. */
static uint8_t sector_copy[KUI_BOOT_COPIES_MAX_COPIES][KUI_BOOT_COPIES_SECTOR_BYTES] __attribute__((aligned(4)));

static bool same_sector(const uint8_t *a, const uint8_t *b) {
    const uint32_t *x = (const uint32_t *)(const void *)a, *y = (const uint32_t *)(const void *)b;
    for(uint32_t k = 0; k < KUI_BOOT_COPIES_SECTOR_BYTES / 4u; ++k)
        if(x[k] != y[k]) return false;
    return true;
}

static void copy_sector(uint8_t *to, const uint8_t *from) {
    uint32_t *x = (uint32_t *)(void *)to;
    const uint32_t *y = (const uint32_t *)(const void *)from;
    for(uint32_t k = 0; k < KUI_BOOT_COPIES_SECTOR_BYTES / 4u; ++k) x[k] = y[k];
}

static void merge_sector(const struct kui_boot_copies *t, uint32_t sector, uint8_t *target,
                         kui_stage1_read_fn read, void *ctx) {
    bool readable[KUI_BOOT_COPIES_MAX_COPIES];
    int first = -1;
    for(uint32_t c = 0; c < t->copy_count; ++c) {
        readable[c] = read(ctx, t->copy_fad[c] + sector, 1, sector_copy[c]) == 0;
        if(readable[c] && first < 0) first = (int)c;
    }
    /* Nothing read this time: keep the reading already in place from the
     * rebuild; the chunk CRC decides. */
    if(first < 0) return;
    for(uint32_t a = 0; a < t->copy_count; ++a)
        for(uint32_t b = a + 1u; b < t->copy_count; ++b)
            if(readable[a] && readable[b] && same_sector(sector_copy[a], sector_copy[b])) {
                copy_sector(target, sector_copy[a]);
                return;
            }
    copy_sector(target, sector_copy[first]);
}

/* Copy order for one chunk: first the copy that delivered the previous chunk
 * (the nearest copy on an intact disc), then the others, nearest first. A
 * damaged copy then costs one failed read, not one per remaining chunk and a
 * seek back and forth each time. */
static uint32_t copy_at(uint32_t k, uint32_t preferred) {
    return k == 0u ? preferred : (k - 1u < preferred ? k - 1u : k);
}

/* Read the chunk's sectors still missing from copy C, one at a time in disc
 * order, straight into place. Returns the updated set of sectors held. */
static uint32_t fill_from_copy(const struct kui_boot_copies *t, uint32_t c, uint32_t first,
                               uint32_t sectors, uint8_t *chunk, uint32_t have,
                               kui_stage1_read_fn read, void *ctx) {
    for(uint32_t s = 0; s < sectors; ++s)
        if(!(have & 1u << s) &&
           read(ctx, t->copy_fad[c] + first + s, 1, chunk + s * KUI_BOOT_COPIES_SECTOR_BYTES) == 0)
            have |= 1u << s;
    return have;
}

enum kui_stage1_result kui_stage1_load(const struct kui_boot_copies *t,
    uint8_t *dest, kui_stage1_read_fn read, void *ctx) {
    if(!kui_boot_copies_valid(t) || !dest || !read) return KUI_STAGE1_BAD_TABLE;
    uint32_t preferred = 0;
    for(uint32_t i = 0; i < t->chunk_count; ++i) {
        const uint32_t offset = i * CHUNK_BYTES;
        const uint32_t bytes = t->image_bytes - offset < CHUNK_BYTES ?
            t->image_bytes - offset : CHUNK_BYTES;
        const uint32_t sectors = (bytes + KUI_BOOT_COPIES_SECTOR_BYTES - 1u) /
            KUI_BOOT_COPIES_SECTOR_BYTES;
        const uint32_t first = i * KUI_BOOT_COPIES_CHUNK_SECTORS;
        uint8_t *chunk = dest + offset;
        bool ok = false;
        /* Normal path: one read of the whole chunk. */
        for(uint32_t k = 0; k < t->copy_count && !ok; ++k) {
            const uint32_t c = copy_at(k, preferred);
            ok = read(ctx, t->copy_fad[c] + first, sectors, chunk) == 0 && chunk_good(t, i, chunk, bytes);
            if(ok) preferred = c;
        }
        if(ok) continue;
        /* Every copy failed somewhere in this chunk. Damage at different radii
         * rarely hits the same sector of every copy, so rebuild the chunk from
         * the copies in turn: each supplies, in disc order, the sectors still
         * missing. The drive does not seek between copies for every sector,
         * and a damaged sector fails its own EDC check, so it reads as an
         * error rather than as wrong data. A sector no copy reads is tried
         * again in up to REBUILD_PASSES rounds before stage 1 gives up. */
        const uint32_t all = (1u << sectors) - 1u;
        uint32_t have = 0;
        for(uint32_t pass = 0; pass < REBUILD_PASSES && have != all; ++pass)
            for(uint32_t k = 0; k < t->copy_count && have != all; ++k)
                have = fill_from_copy(t, copy_at(k, preferred), first, sectors, chunk, have, read, ctx);
        if(have != all) return KUI_STAGE1_UNREADABLE;
        if(chunk_good(t, i, chunk, bytes)) continue;
        /* Every sector read, but the chunk is still wrong: some copy holds
         * silently damaged data. Take each sector's version two copies agree
         * on, else the first that could be read. */
        for(uint32_t s = 0; s < sectors; ++s)
            merge_sector(t, first + s, chunk + s * KUI_BOOT_COPIES_SECTOR_BYTES, read, ctx);
        if(!chunk_good(t, i, chunk, bytes)) return KUI_STAGE1_CORRUPT;
    }
    return kui_stage1_crc32(0, dest, t->image_bytes) == t->image_crc32 ?
        KUI_STAGE1_OK : KUI_STAGE1_CORRUPT;
}
