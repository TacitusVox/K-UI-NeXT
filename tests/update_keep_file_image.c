/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/update_keep_file.h"
#include "kui/update_retention.h"
#include "kui/system_settings.h"
#include "kui/media.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_A "0:/KUI/system/nightly-keep-a.bin"
#define SLOT_B "0:/KUI/system/nightly-keep-b.bin"

static uint8_t *disk;
static size_t disk_bytes;
static FATFS fs;
static bool read_failure;
static unsigned operations;
static int fail_after = -1;

static uint64_t blocks(void *ctx) {
    (void)ctx;
    return disk_bytes / 512;
}

static int read_blocks(void *ctx, uint32_t block, size_t count, uint8_t *out) {
    if(read_failure || (uint64_t)block + count > blocks(ctx)) return -1;
    memcpy(out, disk + (size_t)block * 512, count * 512);
    return 0;
}

static bool fail_operation(void) {
    unsigned call = operations++;
    return fail_after >= 0 && call >= (unsigned)fail_after;
}

static int write_blocks(void *ctx, uint32_t block, size_t count, const uint8_t *data) {
    if(fail_operation() || (uint64_t)block + count > blocks(ctx)) return -1;
    memcpy(disk + (size_t)block * 512, data, count * 512);
    return 0;
}

static int sync_blocks(void *ctx) {
    (void)ctx;
    return fail_operation() ? -1 : 0;
}

static const struct kui_media_ops media = {NULL, blocks, read_blocks, write_blocks, sync_blocks};
uint32_t kui_clock_fattime(void) { return 0; }

static void mount(void) {
    kui_media_set(&media);
    assert(f_mount(&fs, "0:", 1) == FR_OK);
}

static void unmount(void) {
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    kui_media_set(NULL);
}

static void write_file(const char *path, const uint8_t *bytes, UINT size) {
    FIL file;
    UINT count;
    assert(f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
    assert(f_write(&file, bytes, size, &count) == FR_OK && count == size);
    assert(f_close(&file) == FR_OK);
}

static void read_file(const char *path, uint8_t *bytes, UINT size) {
    FIL file;
    UINT count;
    assert(f_open(&file, path, FA_READ) == FR_OK && f_size(&file) == size);
    assert(f_read(&file, bytes, size, &count) == FR_OK && count == size);
    assert(f_close(&file) == FR_OK);
}

static void expect_limit(unsigned expected) {
    unsigned value = 99;
    assert(kui_update_keep_load(&value) && value == expected);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    long end = ftell(input);
    assert(end > 0 && !fseek(input, 0, SEEK_SET));
    disk_bytes = (size_t)end;
    disk = malloc(disk_bytes);
    uint8_t *seed = malloc(disk_bytes);
    assert(disk && seed && fread(disk, 1, disk_bytes, input) == disk_bytes && !fclose(input));
    mount();
    operations = 0;
    expect_limit(KUI_UPDATE_KEEP_DEFAULT);
    enum kui_update_channel channel = KUI_UPDATE_NIGHTLIES;
    unsigned default_keep;
    assert(kui_update_preferences_load(&default_keep, &channel) && channel == KUI_UPDATE_RELEASES &&
           default_keep == KUI_UPDATE_KEEP_DEFAULT);
    assert(!kui_update_preferences_save(5, KUI_UPDATE_HARDWARE));
    assert(!kui_update_keep_save(2) && !kui_update_keep_load(NULL) && !operations);

    /* Alternate the original system preference writer and the new preference. */
    struct kui_system_settings prefs;
    kui_system_settings_default(&prefs);
    prefs.menu_repeat = 2;
    assert(kui_system_settings_save(&prefs, NULL));
    assert(kui_update_keep_save(10));
    prefs.stick_deadzone = 2;
    assert(kui_system_settings_save(&prefs, NULL));
    expect_limit(10);
    uint8_t settings_a[KUI_SYSTEM_SETTINGS_RECORD_SIZE], settings_b[sizeof(settings_a)];
    read_file(KUI_SYSTEM_SETTINGS_PATH_A, settings_a, sizeof(settings_a));
    read_file(KUI_SYSTEM_SETTINGS_PATH_B, settings_b, sizeof(settings_b));
    assert(settings_a[8] == 4 && settings_b[8] == 4);
    assert(kui_update_keep_save(0));
    operations = 0;
    assert(kui_update_keep_save(0) && !operations);
    unmount();
    memcpy(seed, disk, disk_bytes);

    mount();
    operations = 0;
    assert(kui_update_preferences_save(1, KUI_UPDATE_NIGHTLIES));
    unsigned boundaries = operations;
    assert(boundaries && boundaries < 100);
    unmount();
    for(unsigned cut = 0; cut <= boundaries; ++cut) {
        memcpy(disk, seed, disk_bytes);
        mount();
        operations = 0;
        fail_after = (int)cut;
        bool saved = kui_update_preferences_save(1, KUI_UPDATE_NIGHTLIES);
        fail_after = -1;
        unmount();
        mount();
        unsigned keep = 99;
        assert(kui_update_preferences_load(&keep, &channel));
        assert((keep == 0 && channel == KUI_UPDATE_RELEASES) || (keep == 1 && channel == KUI_UPDATE_NIGHTLIES));
        if(saved) assert(keep == 1);
        uint8_t bytes[sizeof(settings_a)];
        read_file(KUI_SYSTEM_SETTINGS_PATH_A, bytes, sizeof(bytes));
        assert(!memcmp(bytes, settings_a, sizeof(bytes)));
        read_file(KUI_SYSTEM_SETTINGS_PATH_B, bytes, sizeof(bytes));
        assert(!memcmp(bytes, settings_b, sizeof(bytes)));
        assert(kui_system_settings_load(&prefs, false, NULL) &&
               prefs.menu_repeat == 2 && prefs.stick_deadzone == 2);
        unmount();
    }

    memcpy(disk, seed, disk_bytes);
    mount();
    uint8_t damaged[20];
    read_file(SLOT_B, damaged, sizeof(damaged));
    damaged[12] = 1; /* Valid value, invalid CRC. */
    write_file(SLOT_B, damaged, sizeof(damaged));
    expect_limit(10);
    write_file(SLOT_A, damaged, 13); /* Truncated older slot. */
    expect_limit(0);
    assert(f_unlink(SLOT_A) == FR_OK);
    expect_limit(0); /* One damaged B record and no A record. */
    assert(kui_update_keep_save(3));
    unmount();
    mount();
    expect_limit(3);
    assert(kui_update_preferences_save(0, KUI_UPDATE_NIGHTLIES));
    assert(kui_update_keep_save(5));
    assert(kui_update_preferences_load(&default_keep, &channel) && channel == KUI_UPDATE_NIGHTLIES && default_keep == 5);
    assert(kui_update_preferences_save(5, KUI_UPDATE_RELEASES));
    unmount();
    mount();
    read_failure = true;
    unsigned value = 99;
    operations = 0;
    assert(!kui_update_keep_load(&value) && value == KUI_UPDATE_KEEP_DEFAULT);
    assert(!kui_update_keep_save(5) && !operations);
    read_failure = false;
    unmount();
    free(seed);
    free(disk);
    printf("PASS atomic channel/retention preferences and v4 system records across %u write/sync boundaries\n",
           boundaries + 1);
    return 0;
}
