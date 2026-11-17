/* SPDX-License-Identifier: GPL-3.0-only */
/* Boot-disc stage 1 (1ST_READ.BIN): reads the CD bootstrap from its checked
 * copies through the BIOS GD-ROM functions, the same calls and sequence KOS's
 * cdrom driver makes at every program start (cdrom_init/cdrom_reinit and
 * cdrom_read_sectors in PIO mode). Freestanding: no KOS, no libc. */
#include "kui/boot_copies.h"

/* Filled in on the disc by tools/boot_disc.py; the unpatched build carries
 * only the magic, so an unprepared stage 1 refuses to start anything. */
__attribute__((used, section(".data.kui_boot_copies"), aligned(4)))
struct kui_boot_copies kui_boot_copies = {.magic = KUI_BOOT_COPIES_MAGIC};

#define GD_VECTOR 0x8c0000bcu
enum { GD_SEND = 0, GD_CHECK = 1, GD_EXEC = 2, GD_INIT = 3, GD_DRIVE = 4, GD_SECTOR_MODE = 10 };
enum { CMD_PIOREAD = 16, CMD_INIT = 24 };
enum { CMD_FAILED = -1, CMD_PROCESSING = 1, CMD_COMPLETED = 2, CMD_STREAMING = 3, CMD_BUSY = 4 };
enum { READ_SYS = -1, READ_DISC_CHANGED = -6 };

typedef int (*gd_function)(uint32_t, uint32_t, int32_t, int32_t);

static int gd(uint32_t r4, uint32_t r5, int32_t function) {
    gd_function call = (gd_function)(uintptr_t)*(volatile uint32_t *)GD_VECTOR;
    return call(r4, r5, 0, function);
}

/* cdrom_exec_cmd_timed: queue the command (retrying while the BIOS queue is
 * busy), then run the GD server until it is no longer busy/processing. */
static int exec_cmd(int32_t cmd, void *param) {
    int32_t handle = 0;
    for(uint32_t tries = 0; tries < 1000000u && handle <= 0; ++tries) {
        handle = gd((uint32_t)cmd, (uint32_t)(uintptr_t)param, GD_SEND);
        (void)gd(0, 0, GD_EXEC);
    }
    if(handle <= 0) return READ_SYS;
    int32_t status[4] = {0, 0, 0, 0};
    int response;
    for(;;) {
        (void)gd(0, 0, GD_EXEC);
        response = gd((uint32_t)handle, (uint32_t)(uintptr_t)status, GD_CHECK);
        if(response <= CMD_FAILED) break;
        if(response != CMD_BUSY && response != CMD_PROCESSING) break;
    }
    if(response == CMD_COMPLETED || response == CMD_STREAMING) return 0;
    return status[0] == 6 ? READ_DISC_CHANGED : READ_SYS;
}

static int read_sectors(void *ctx, uint32_t fad, uint32_t count, void *buffer) {
    (void)ctx;
    struct { uint32_t start, count; void *buffer; uint32_t is_test; } params = {fad, count, buffer, 0};
    return exec_cmd(CMD_PIOREAD, &params);
}

/* KOS cdrom_init and cdrom_reinit: initialise the GD functions and the drive,
 * then select 2048-byte user data, CD-ROM XA mode 2 when the drive says so. */
static void drive_init(void) {
    (void)gd(0, 0, GD_INIT);
    for(unsigned tries = 0; tries < 8u && exec_cmd(CMD_INIT, 0) == READ_DISC_CHANGED; ++tries) {}
    int32_t drive[2] = {0, 0};
    (void)gd((uint32_t)(uintptr_t)drive, 0, GD_DRIVE);
    int32_t mode[4] = {0, 0x2000, drive[1] == 0x20 ? 2048 : 1024, 2048};
    (void)gd((uint32_t)(uintptr_t)mode, 0, GD_SECTOR_MODE);
}

/* Called by stage1_entry.S on the relocated copy. Returns 0 when the
 * bootstrap is in place at KUI_STAGE1_ENTRY; the entry code then hands over. */
int kui_stage1_main(void) {
    if(!kui_boot_copies_valid(&kui_boot_copies)) return KUI_STAGE1_BAD_TABLE;
    drive_init();
    /* Write and check through the uncached alias: no stale cache lines of
     * this stage's own first instructions can mix into the new program. */
    uint8_t *dest = (uint8_t *)(uintptr_t)(KUI_STAGE1_ENTRY | 0x20000000u);
    return kui_stage1_load(&kui_boot_copies, dest, read_sectors, 0);
}
