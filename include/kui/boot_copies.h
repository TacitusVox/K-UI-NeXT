/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_BOOT_COPIES_H
#define KUI_BOOT_COPIES_H
/* Redundant boot-disc layout. The console BIOS reads only IP.BIN and a tiny
 * stage 1 (1ST_READ.BIN). Stage 1 then reads the real CD bootstrap from up to
 * four identical copies placed at different radii of the disc, checking every
 * 32 KiB chunk against the CRC-32 recorded here, and starts it at
 * KUI_STAGE1_ENTRY exactly as the BIOS would have. tools/boot_disc.py writes
 * this table into stage 1 after it has placed the copies on the disc. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KUI_BOOT_COPIES_MAGIC "KUI-BOOT-COPIES"   /* 15 characters + NUL */
#define KUI_BOOT_COPIES_VERSION 1u
#define KUI_BOOT_COPIES_MAX_COPIES 4u
#define KUI_BOOT_COPIES_CHUNK_SECTORS 16u         /* 32 KiB per read */
#define KUI_BOOT_COPIES_MAX_CHUNKS 256u           /* 8 MiB bootstrap */
#define KUI_BOOT_COPIES_SECTOR_BYTES 2048u
#define KUI_STAGE1_ENTRY 0x8c010000u               /* where the BIOS starts programs */

struct kui_boot_copies {
    char magic[16];
    uint32_t version;
    uint32_t image_bytes;      /* bootstrap binary size */
    uint32_t chunk_sectors;    /* sectors per chunk */
    uint32_t chunk_count;      /* ceil(image_bytes / chunk bytes) */
    uint32_t copy_count;       /* 1..KUI_BOOT_COPIES_MAX_COPIES */
    uint32_t copy_fad[KUI_BOOT_COPIES_MAX_COPIES]; /* first frame address, read order */
    uint32_t image_crc32;      /* IEEE CRC-32 of the whole image */
    uint32_t chunk_crc32[KUI_BOOT_COPIES_MAX_CHUNKS];
    uint32_t table_crc32;      /* CRC-32 of every byte before this field */
};

enum kui_stage1_result {
    KUI_STAGE1_OK = 0,
    KUI_STAGE1_BAD_TABLE = 1,  /* not filled in by boot_disc.py, or damaged */
    KUI_STAGE1_UNREADABLE = 2, /* a sector could not be read from any copy, after retries */
    KUI_STAGE1_CORRUPT = 3,    /* read without errors but failed its CRC */
};

/* Reads COUNT 2048-byte sectors starting at frame address FAD; 0 on success. */
typedef int (*kui_stage1_read_fn)(void *ctx, uint32_t fad, uint32_t count, void *buffer);

uint32_t kui_stage1_crc32(uint32_t previous, const void *data, size_t bytes);
bool kui_boot_copies_valid(const struct kui_boot_copies *table);
/* Fill DEST with the verified bootstrap. Each chunk is read whole, from the
 * copy that delivered the previous chunk first, until one passes its CRC. A
 * chunk that no copy delivers intact is rebuilt from the copies in turn, each
 * supplying the sectors still missing, in up to four rounds; if the result
 * fails its CRC, each sector takes the version two copies agree on. */
enum kui_stage1_result kui_stage1_load(const struct kui_boot_copies *table,
    uint8_t *dest, kui_stage1_read_fn read, void *ctx);
#endif
