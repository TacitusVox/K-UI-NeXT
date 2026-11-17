/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAMES_MEDIA_STORAGE_H
#define KUI_GAMES_MEDIA_STORAGE_H
#include "kui/games.h"
#include "kui/game_launch_settings.h"
#include "kui/hash.h"
#include <stdio.h>
#include <string.h>
#define KUI_DOWNLOADED_COVERS "0:/KUI/covers/downloads"
/* Card filenames depend on the full selector path. A second game named
 * "Disc 1" cannot inherit another game's download when caches are rebuilt. */
static inline bool kui_games_media_path(const char *path, const char *folder, const char *suffix,
        char *out, size_t cap) {
    char normalized[KUI_GAME_LAUNCH_SETTINGS_PATH_CAP], hex[33];
    if(!kui_game_launch_settings_normalize(normalized, path)) return false;
    uint8_t digest[32];
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    kui_sha256_update(&hash, normalized, strlen(normalized));
    kui_sha256_digest(&hash, digest);
    kui_hex(digest, 16, hex);
    int n = snprintf(out, cap, "%s/%s.%s", folder, hex, suffix);
    return n > 0 && (size_t)n < cap;
}
/* One storage worker, card already mounted. Uses the normal box-art scanner
 * to prepare an individual record, with a collision check before any write.
 * On success, present includes an assigned or on-card owner image. */
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,
    kui_log_fn log,kui_cancel_fn cancel);
/* Reads the current image's IP sector, including single-disc games. */
enum kui_games_identity_result kui_games_media_identity(const char *path,
    struct kui_games_disc_identity *out,kui_log_fn log,kui_cancel_fn cancel);
#endif
