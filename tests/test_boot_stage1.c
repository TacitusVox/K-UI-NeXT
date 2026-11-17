/* SPDX-License-Identifier: GPL-3.0-only */
/* Host model of boot-disc stage 1: the real stage1_load.c against a fake
 * disc holding three copies, with injected read errors and silent damage.
 * Usage: test-boot-stage1                      (built-in cases)
 *        test-boot-stage1 TABLE IMAGE DISC BASE (load a tools/boot_disc.py
 *        table from the 2048-byte-sector user data of a real data track, where
 *        DISC holds LBA BASE onwards, and compare with IMAGE) */
#include "kui/boot_copies.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR KUI_BOOT_COPIES_SECTOR_BYTES
#define IMAGE_BYTES (15u * 32768u + 12345u)   /* 16 chunks, the last one partial */
#define DISC_SECTORS 4000u
static const uint32_t fads[3] = {300u, 1500u, 2700u};

static uint8_t image[IMAGE_BYTES];
static uint8_t disc[DISC_SECTORS * SECTOR];
static uint8_t loaded[16u * 32768u];
static bool unreadable[DISC_SECTORS];
static unsigned reads, sector_reads;

static int fake_read(void *ctx, uint32_t fad, uint32_t count, void *buffer) {
    (void)ctx;
    ++reads;
    if(count == 1) ++sector_reads;
    assert(fad >= 150u && fad - 150u + count <= DISC_SECTORS);
    for(uint32_t s = 0; s < count; ++s) if(unreadable[fad - 150u + s]) return -1;
    memcpy(buffer, disc + (size_t)(fad - 150u) * SECTOR, (size_t)count * SECTOR);
    return 0;
}

static uint32_t reference_crc(const uint8_t *p, size_t n) {
    uint32_t crc = 0xffffffffu;
    while(n--) {
        crc ^= *p++;
        for(int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static struct kui_boot_copies table;
static void make_table(void) {
    memset(&table, 0, sizeof(table));
    memcpy(table.magic, KUI_BOOT_COPIES_MAGIC, 16);
    table.version = KUI_BOOT_COPIES_VERSION;
    table.image_bytes = IMAGE_BYTES;
    table.chunk_sectors = KUI_BOOT_COPIES_CHUNK_SECTORS;
    table.chunk_count = 16;
    table.copy_count = 3;
    for(int c = 0; c < 3; ++c) table.copy_fad[c] = fads[c];
    table.image_crc32 = reference_crc(image, IMAGE_BYTES);
    for(uint32_t i = 0; i < 16; ++i) {
        uint32_t off = i * 32768u, n = IMAGE_BYTES - off < 32768u ? IMAGE_BYTES - off : 32768u;
        table.chunk_crc32[i] = reference_crc(image + off, n);
    }
    table.table_crc32 = reference_crc((const uint8_t *)&table, offsetof(struct kui_boot_copies, table_crc32));
}

static void reset_disc(void) {
    memset(disc, 0x5a, sizeof(disc));
    memset(unreadable, 0, sizeof(unreadable));
    for(int c = 0; c < 3; ++c) memcpy(disc + (size_t)(fads[c] - 150u) * SECTOR, image, IMAGE_BYTES);
    memset(loaded, 0xee, sizeof(loaded));
    reads = sector_reads = 0;
}
static uint32_t lba(int copy, uint32_t chunk, uint32_t sector) {
    return fads[copy] - 150u + chunk * 16u + sector;
}
static void damage(int copy, uint32_t chunk, uint32_t sector) {
    disc[(size_t)lba(copy, chunk, sector) * SECTOR + 77u] ^= 0x40u;
}
static enum kui_stage1_result run(void) {
    return kui_stage1_load(&table, loaded, fake_read, NULL);
}
static bool loaded_ok(void) {return !memcmp(loaded, image, IMAGE_BYTES);}

struct track_ctx {const uint8_t *data; size_t bytes; unsigned long base;};
int read_track(void *ctx, uint32_t fad, uint32_t count, void *buffer);
static int real_disc(char **argv) {
    FILE *f = fopen(argv[1], "rb");
    if(!f || fread(&table, 1, sizeof(table), f) != sizeof(table)) return 2;
    fclose(f);
    f = fopen(argv[2], "rb");
    static uint8_t want[8u << 20];
    size_t want_bytes = f ? fread(want, 1, sizeof(want), f) : 0;
    if(f) fclose(f);
    static uint8_t track[200u << 20];
    f = fopen(argv[3], "rb");
    size_t track_bytes = f ? fread(track, 1, sizeof(track), f) : 0;
    if(f) fclose(f);
    unsigned long base = strtoul(argv[4], NULL, 0);
    if(!kui_boot_copies_valid(&table) || table.image_bytes != want_bytes) {puts("table rejected");return 1;}
    struct track_ctx c = {track, track_bytes, base};
    static uint8_t out[8u << 20];
    enum kui_stage1_result r = kui_stage1_load(&table, out, read_track, &c);
    printf("real disc: result %d, %u chunks from %u copies\n", (int)r, table.chunk_count, table.copy_count);
    return r == KUI_STAGE1_OK && !memcmp(out, want, want_bytes) ? 0 : 1;
}
int read_track(void *ctx, uint32_t fad, uint32_t count, void *buffer) {
    const struct track_ctx *c = ctx;
    size_t off = ((size_t)fad - 150u - c->base) * SECTOR;
    if(fad < 150u + c->base || off + (size_t)count * SECTOR > c->bytes) return -1;
    memcpy(buffer, c->data + off, (size_t)count * SECTOR);
    return 0;
}

int main(int argc, char **argv) {
    if(argc == 5) return real_disc(argv);
    for(uint32_t i = 0; i < IMAGE_BYTES; ++i) image[i] = (uint8_t)(i * 2654435761u >> 13);
    /* CRC-32 matches the IEEE reference, also when split. */
    assert(kui_stage1_crc32(0, "123456789", 9) == 0xcbf43926u);
    assert(kui_stage1_crc32(kui_stage1_crc32(0, image, 1000), image + 1000, 5000) ==
           reference_crc(image, 6000));
    make_table();
    assert(sizeof(struct kui_boot_copies) == 1084u);

    /* Intact disc: 16 whole-chunk reads from the nearest copy only. */
    reset_disc();
    assert(run() == KUI_STAGE1_OK && loaded_ok() && reads == 16 && !sector_reads);
    /* A read error and silent damage in copy 1: those chunks come from copy 2. */
    reset_disc(); unreadable[lba(0, 3, 9)] = true; damage(0, 8, 1);
    assert(run() == KUI_STAGE1_OK && loaded_ok() && reads == 18 && !sector_reads);
    /* Copies 1 and 2 both bad in the final, partial chunk: copy 3 serves it. */
    reset_disc(); damage(0, 15, 0); unreadable[lba(1, 15, 2)] = true;
    assert(run() == KUI_STAGE1_OK && loaded_ok());
    /* All three copies unreadable in chunk 4, at different sectors: rebuilt
     * sector by sector from whichever copy reads. */
    reset_disc(); unreadable[lba(0, 4, 1)] = unreadable[lba(1, 4, 7)] = unreadable[lba(2, 4, 12)] = true;
    assert(run() == KUI_STAGE1_OK && loaded_ok() && sector_reads == 16u * 3u);
    /* All three copies silently damaged in chunk 6 at different sectors:
     * the two undamaged copies of each sector outvote the damaged one. */
    reset_disc(); damage(0, 6, 0); damage(1, 6, 5); damage(2, 6, 15);
    assert(run() == KUI_STAGE1_OK && loaded_ok());
    /* A read error in one copy and silent damage at other sectors of the
     * other two: every sector still has two agreeing readings. */
    reset_disc(); unreadable[lba(0, 2, 3)] = true; damage(1, 2, 9); damage(2, 2, 12);
    assert(run() == KUI_STAGE1_OK && loaded_ok());
    /* One reading of a sector failing and the other two disagreeing leaves no
     * majority: refused by the CRC rather than guessed. (On a real disc a bad
     * sector fails its own EDC check and reads as an error, not as data.) */
    reset_disc(); unreadable[lba(0, 2, 3)] = true; damage(1, 2, 3);
    damage(1, 2, 9); damage(2, 2, 9);
    assert(run() == KUI_STAGE1_CORRUPT);
    /* The same sector unreadable on every copy cannot be recovered. */
    reset_disc(); for(int c = 0; c < 3; ++c) unreadable[lba(c, 11, 4)] = true;
    assert(run() == KUI_STAGE1_UNREADABLE);
    /* Two of three copies damaged identically in one sector: refused by CRC. */
    reset_disc(); damage(0, 9, 2); damage(1, 9, 2); unreadable[lba(2, 9, 0)] = true;
    assert(run() == KUI_STAGE1_CORRUPT);

    /* The table must be filled in, consistent and undamaged. */
    struct kui_boot_copies good = table;
    reset_disc();
    struct kui_boot_copies empty = {.magic = KUI_BOOT_COPIES_MAGIC};
    assert(!kui_boot_copies_valid(&empty) && kui_stage1_load(&empty, loaded, fake_read, NULL) == KUI_STAGE1_BAD_TABLE);
    table.chunk_crc32[2] ^= 1; assert(!kui_boot_copies_valid(&table)); table = good;
    table.magic[0] = 'k'; table.table_crc32 = reference_crc((const uint8_t *)&table, offsetof(struct kui_boot_copies, table_crc32));
    assert(!kui_boot_copies_valid(&table)); table = good;
#define REJECT(field, value) do { table.field = value; \
        table.table_crc32 = reference_crc((const uint8_t *)&table, offsetof(struct kui_boot_copies, table_crc32)); \
        assert(!kui_boot_copies_valid(&table)); table = good; } while(0)
    REJECT(version, 2u); REJECT(chunk_sectors, 32u); REJECT(chunk_count, 15u); REJECT(copy_count, 0u);
    REJECT(copy_count, 5u); REJECT(image_bytes, 0u); REJECT(copy_fad[1], 149u);
    REJECT(image_bytes, 256u * 32768u + 1u);
    assert(kui_boot_copies_valid(&table) && kui_stage1_load(&table, NULL, fake_read, NULL) == KUI_STAGE1_BAD_TABLE);
    puts("PASS boot stage 1: chunk reads from the nearest good copy, per-sector rebuild with "
         "majority vote, unrecoverable sectors and silent identical damage refused, table checks");
    return 0;
}
