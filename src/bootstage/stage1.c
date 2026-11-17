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
enum { GD_SEND = 0, GD_CHECK = 1, GD_EXEC = 2, GD_INIT = 3, GD_DRIVE = 4, GD_ABORT = 8,
       GD_RESET = 9, GD_SECTOR_MODE = 10 };
enum { CMD_PIOREAD = 16, CMD_INIT = 24 };
enum { CMD_FAILED = -1, CMD_NOT_FOUND = 0, CMD_PROCESSING = 1, CMD_COMPLETED = 2, CMD_STREAMING = 3,
       CMD_BUSY = 4 };
enum { READ_SYS = -1, READ_NOT_READY = -2, READ_DISC_CHANGED = -6, READ_TIMEOUT = -9 };
/* KOS waits 10 s for CMD_INIT. A damaged area makes the drive retry, but a
 * 16-sector read that has not finished in 10 s is abandoned for the next copy. */
#define COMMAND_SECONDS 10u
#define ABORT_SECONDS 2u
static bool gd_system_ready, drive_ready;

typedef int (*gd_function)(uint32_t, uint32_t, int32_t, int32_t);

static int gd(uint32_t r4, uint32_t r5, int32_t function) {
    gd_function call = (gd_function)(uintptr_t)*(volatile uint32_t *)GD_VECTOR;
    return call(r4, r5, 0, function);
}

/* Seconds from the AICA real-time clock: there is no timer set up this early.
 * Like KOS rtc_unix_secs, re-read until two readings agree. */
static uint32_t rtc_seconds(void) {
    uint32_t previous = ~0u;
    for(unsigned i = 0; i < 4u; ++i) {
        uint32_t now = (*(volatile uint32_t *)0xa0710000u & 0xffffu) << 16 |
                       (*(volatile uint32_t *)0xa0710004u & 0xffffu);
        if(now == previous) break;
        previous = now;
    }
    return previous;
}

/* cdrom_abort_cmd: abort, wait until the BIOS lets go of the command, and if
 * it does not within the limit, reset and re-initialise the GD functions.
 * The drive is then set up again before the next read. */
static void abort_cmd(int32_t handle) {
    int32_t status[4] = {0, 0, 0, 0};
    (void)gd((uint32_t)handle, 0, GD_ABORT);
    const uint32_t start = rtc_seconds();
    for(uint32_t spins = 0;; ++spins) {
        (void)gd(0, 0, GD_EXEC);
        int response = gd((uint32_t)handle, (uint32_t)(uintptr_t)status, GD_CHECK);
        if(response <= CMD_FAILED || response == CMD_NOT_FOUND || response == CMD_COMPLETED) return;
        if((spins & 1023u) == 1023u && rtc_seconds() - start >= ABORT_SECONDS) break;
    }
    (void)gd(0, 0, GD_RESET);
    (void)gd(0, 0, GD_INIT);
    drive_ready = false;
}

/* cdrom_exec_cmd_timed: queue the command (retrying while the BIOS queue is
 * busy), then run the GD server until it is no longer busy/processing, for
 * at most COMMAND_SECONDS. */
static int exec_cmd(int32_t cmd, void *param) {
    int32_t handle = 0;
    for(uint32_t tries = 0; tries < 1000000u && handle <= 0; ++tries) {
        handle = gd((uint32_t)cmd, (uint32_t)(uintptr_t)param, GD_SEND);
        (void)gd(0, 0, GD_EXEC);
    }
    if(handle <= 0) return READ_SYS;
    int32_t status[4] = {0, 0, 0, 0};
    int response;
    const uint32_t start = rtc_seconds();
    for(uint32_t spins = 0;; ++spins) {
        (void)gd(0, 0, GD_EXEC);
        response = gd((uint32_t)handle, (uint32_t)(uintptr_t)status, GD_CHECK);
        if(response <= CMD_FAILED) break;
        if(response != CMD_BUSY && response != CMD_PROCESSING) break;
        if((spins & 1023u) == 1023u && rtc_seconds() - start >= COMMAND_SECONDS) {
            abort_cmd(handle);
            return READ_TIMEOUT;
        }
    }
    if(response == CMD_COMPLETED || response == CMD_STREAMING) return 0;
    if(status[0] == 2) return READ_NOT_READY;
    return status[0] == 6 ? READ_DISC_CHANGED : READ_SYS;
}

/* KOS cdrom_init (bus reactivation and the GD functions, once) and cdrom_reinit (the drive, again
 * after "not ready", "disc changed" or a reset): then select 2048-byte user
 * data, CD-ROM XA mode 2 when the drive says so. Like cdrom_reinit, keep
 * asking for up to 10 s while the drive reports a disc change; here also
 * while it is not ready, since this disc just booted and is certainly in the
 * drive. */
static void drive_init(void) {
    if(!gd_system_ready) {
        /* KOS cdrom_init: unless the G1 bus protection check has passed, the
         * drive is locked out and every GD command stalls. Reactivate it by
         * sending the BIOS size and reading each word across the bus, so the
         * controller can verify it (1 KiB for a custom 0xe6ff BIOS). */
        if(*(volatile uint32_t *)0xa05f74ecu != 3u) {
            const uint32_t bytes = *(volatile uint16_t *)0xa0000000u == 0xe6ffu ? 0x400u : 0x200000u;
            *(volatile uint32_t *)0xa05f74e4u = bytes - 1u;
            for(uint32_t p = 0; p < bytes / 4u; ++p) (void)((volatile uint32_t *)0xa0000000u)[p];
        }
        (void)gd(0, 0, GD_INIT);
        gd_system_ready = true;
    }
    const uint32_t start = rtc_seconds();
    for(;;) {
        const int result = exec_cmd(CMD_INIT, 0);
        if(result != READ_DISC_CHANGED && result != READ_NOT_READY) break;
        if(rtc_seconds() - start >= COMMAND_SECONDS) break;
    }
    int32_t drive[2] = {0, 0};
    (void)gd((uint32_t)(uintptr_t)drive, 0, GD_DRIVE);
    int32_t mode[4] = {0, 0x2000, drive[1] == 0x20 ? 2048 : 1024, 2048};
    (void)gd((uint32_t)(uintptr_t)mode, 0, GD_SECTOR_MODE);
    drive_ready = true;
}

/* After "not ready" or "disc changed" the drive is set up again before the
 * next read, as KOS's iso9660 driver does after ERR_NO_DISC/ERR_DISC_CHG. */
static int read_sectors(void *ctx, uint32_t fad, uint32_t count, void *buffer) {
    (void)ctx;
    if(!drive_ready) drive_init();
    struct { uint32_t start, count; void *buffer; uint32_t is_test; } params = {fad, count, buffer, 0};
    const int result = exec_cmd(CMD_PIOREAD, &params);
    if(result == READ_NOT_READY || result == READ_DISC_CHANGED) drive_ready = false;
    return result;
}

/* Called by stage1_entry.S on the relocated copy. Returns 0 when the
 * bootstrap is in place at KUI_STAGE1_ENTRY; the entry code then hands over. */
int kui_stage1_main(void) {
    if(!kui_boot_copies_valid(&kui_boot_copies)) return KUI_STAGE1_BAD_TABLE;
    /* Write and check through the uncached alias: no stale cache lines of
     * this stage's own first instructions can mix into the new program. */
    uint8_t *dest = (uint8_t *)(uintptr_t)(KUI_STAGE1_ENTRY | 0x20000000u);
    return kui_stage1_load(&kui_boot_copies, dest, read_sectors, 0);
}
