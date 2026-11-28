/* SPDX-License-Identifier: GPL-3.0-only */
#include "game_vga_cable.h"
#include "kui/game_vga.h"
#include "kui/hash.h"

/* Only this temporary boot stage owns the proofs. The gameplay reader and
 * its VMU/audio workers have no dependency on the VGA catalog. */
static const struct kui_game_vga_profile *native_vga_profile;
static uint8_t native_vga_sha[32];
static uint32_t native_vga_adler;

static void native_vga_identify(const uint8_t *ip, const uint8_t *boot) {
    const struct kui_game_vga_profile *profile = kui_game_vga_candidate(ip, exec_bytes);
    native_vga_profile = profile && kui_game_vga_connected() ? profile : NULL;
    if (!native_vga_profile) return;
    if (profile->sha256 && !kui_game_vga_target_sha(profile)) {
        struct kui_sha256 sha;
        kui_sha256_init(&sha);
        kui_sha256_update(&sha, boot, exec_bytes);
        kui_sha256_digest(&sha, native_vga_sha);
    }
    native_vga_adler = kui_game_vga_capture(profile, boot, exec_bytes, boot_crc, native_vga_sha);
}

void kui_native_vga_handoff(void) {
    if (native_vga_profile)
        kui_game_vga_apply_profile(native_vga_profile, (uint8_t *)(uintptr_t)KUI_RETAIL_EXEC_ADDRESS,
            exec_bytes, boot_crc, native_vga_sha, native_vga_adler);
}
