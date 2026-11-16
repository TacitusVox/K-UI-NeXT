/* SPDX-License-Identifier: GPL-3.0-only */
/* Test 79: which disc the cold stage calls a source for the card stream's
 * window. This is the stage's own text (toy_movie_scan in
 * src/loader/toy_movie_iso_stage.inc) with the stage's globals supplied
 * here: its manifest, its storage and its runtime mode. The stage itself
 * runs on the console only. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kui/retail_image.h"
#include "kui/storage.h"
#include "kui/vmu_native_mode.h"
#include "retail_storage.h"

/* The stage's text is built as the module's is: the compact Background
 * family with the card stream's layout. */
#define KUI_TOY_SUITE_MODE 66
#define KUI_NATIVE_BACKGROUND_COMPACT 1
#define KUI_NATIVE_GENERAL_LEAN_LAYOUT 1
#define KUI_NATIVE_SPLIT_CONTROLLER_GENERAL 1
#define KUI_NATIVE_READAHEAD 1
#define KUI_TOY_WIRE_STREAM 1
#ifndef KUI_TOY_WIRE_TITLES
#define KUI_TOY_WIRE_TITLES 0
#endif

static struct kui_retail_manifest manifest;
static struct kui_retail_image image;
static struct kui_retail_storage card;
static uint8_t raw_boot[2u * KUI_GAME_RAW_BYTES];
static uint32_t boot_crc, exec_bytes;

static uint32_t mode;
static uint32_t native_runtime_mode(void) { return mode; }
/* The storage as the stage's scan uses it. Acquiring it fails here: the
 * scan of Toy Commander's own files is not what this test is about, only
 * whether that path is the one taken. */
static unsigned acquired, read_sectors;
enum kui_loader_sd_result kui_retail_storage_acquire(struct kui_retail_storage *s) {
    (void)s;
    ++acquired;
    return KUI_LOADER_SD_TIMEOUT;
}
enum kui_loader_sd_result kui_retail_storage_stop(struct kui_retail_storage *s) { (void)s; return KUI_LOADER_SD_OK; }
void kui_retail_storage_release(struct kui_retail_storage *s) { (void)s; }
enum kui_game_result kui_retail_image_read(struct kui_retail_image *i, uint32_t lba, uint32_t count,
    enum kui_game_sector_format format, void *out, size_t bytes) {
    (void)i; (void)lba; (void)count; (void)format; (void)out; (void)bytes;
    ++read_sectors;
    return KUI_GAME_IO;
}
enum kui_game_result kui_retail_image_check_validated(const struct kui_retail_manifest *m, uint32_t lba,
    uint32_t count, enum kui_game_sector_format format) {
    (void)m; (void)lba; (void)count; (void)format;
    return KUI_GAME_IO;
}

#include "toy_movie_iso_stage.inc"

enum { RAW_DATA = 4u, COOKED_DATA = 4u | KUI_RETAIL_TRACK_COOKED, MODE2_DATA = 4u | KUI_RETAIL_TRACK_MODE2,
    SUBCODE_DATA = 4u | KUI_RETAIL_TRACK_2448, SHORT_DATA = 4u | KUI_RETAIL_TRACK_2336, AUDIO = 0u };
static void disc(uint32_t reader, uint32_t transport, uint32_t runtime, bool toy, const uint8_t *controls,
    unsigned tracks) {
    memset(&manifest, 0, sizeof(manifest));
    manifest.reader = reader;
    manifest.storage_transport = transport;
    manifest.session_lba = 45000u;
    manifest.ip_crc32 = toy ? KUI_VMU_LAUNCH_TOY_IP_CRC : UINT32_C(0x12345678);
    boot_crc = toy ? KUI_VMU_LAUNCH_TOY_EXEC_CRC : UINT32_C(0x9abcdef0);
    exec_bytes = toy ? KUI_VMU_LAUNCH_TOY_EXEC_BYTES : 4057056u;
    manifest.track_count = tracks;
    for(unsigned i = 0; i < tracks; ++i)
        manifest.slots[i].track = (struct kui_retail_track){45000u + 1000u * i, 46000u + 1000u * i, 0u, 0u, controls[i]};
    mode = runtime;
    toy_suite_source = 0u;
    acquired = read_sectors = 0u;
    toy_movie_scan();
}

int main(void) {
    static const uint8_t raw[3] = {AUDIO, AUDIO, RAW_DATA}, cooked[3] = {AUDIO, AUDIO, COOKED_DATA},
        audio[2] = {AUDIO, AUDIO}, mode2[1] = {MODE2_DATA}, mixed[4] = {COOKED_DATA, AUDIO, RAW_DATA, AUDIO},
        subcode[2] = {AUDIO, SUBCODE_DATA}, shorter[1] = {SHORT_DATA};
    const uint32_t off = KUI_VMU_NATIVE_MODE_READER_ONLY, on = KUI_VMU_NATIVE_MODE_NORMAL;
    /* Another title, the New reader, the virtual VMU off, the SCI card. */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, raw, 3u);
    /* Test 79 calls it a source by its raw Mode 1 track; up to Test 78 no
     * title but Toy Commander was one. Nothing of the disc is read. */
    assert(toy_suite_source == (KUI_TOY_WIRE_TITLES ? 1u : 0u) && !acquired && !read_sectors);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, mixed, 4u);
    assert(toy_suite_source == (KUI_TOY_WIRE_TITLES ? 1u : 0u) && !acquired);
    /* No raw Mode 1 data track: 2,048-byte sectors, audio alone, Mode 2. */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, cooked, 3u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, audio, 2u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, mode2, 1u);
    assert(!toy_suite_source && !acquired);
    /* Sectors of 2,448 bytes (with their subcode) and of 2,336. */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, subcode, 2u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, shorter, 1u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, false, raw, 0u);
    assert(!toy_suite_source && !acquired);
    /* Another card adapter, another reader. */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCIF, off, false, raw, 3u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_IDE, off, false, raw, 3u);
    assert(!toy_suite_source && !acquired);
    disc(KUI_RETAIL_READER_ASYNC_EAGER, KUI_STORAGE_SCI, off, false, raw, 3u);
    assert(!toy_suite_source && !acquired);
    /* The virtual VMU on: no source. (The stage's own look at the mode
     * stops such a launch of this module before it comes here.) */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, on, false, raw, 3u);
    assert(!toy_suite_source && !acquired);
    /* Toy Commander takes the path it always took: its files are looked
     * for on the card. Here the card cannot be had, and it is no source. */
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, off, true, raw, 3u);
    assert(!toy_suite_source && acquired == 1u);
    disc(KUI_RETAIL_READER_ASYNC, KUI_STORAGE_SCI, on, true, raw, 3u);
    assert(!toy_suite_source && !acquired);
    printf("PASS the stage's source for the window%s: another title on the New reader with the virtual VMU off, by a "
           "raw Mode 1 track on the SCI card and nothing else; Toy Commander by its own files as before\n",
        KUI_TOY_WIRE_TITLES ? "" : " (up to Test 78: Toy Commander alone)");
    return 0;
}
