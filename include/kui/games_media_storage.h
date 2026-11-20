/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAMES_MEDIA_STORAGE_H
#define KUI_GAMES_MEDIA_STORAGE_H
#include "kui/games.h"
#include "kui/game_launch_settings.h"
#include "kui/hash.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#define KUI_DOWNLOADED_COVERS "0:/KUI/covers/downloads"
/* Card filenames depend on the full selector path. A second game named
 * "Disc 1" cannot inherit another game's download when caches are rebuilt. */
static inline bool kui_games_media_key(const char *path, char key[33]) {
    char normalized[KUI_GAME_LAUNCH_SETTINGS_PATH_CAP];
    if(!key || !kui_game_launch_settings_normalize(normalized,path)) return false;
    uint8_t digest[32];
    struct kui_sha256 hash;
    kui_sha256_init(&hash);
    kui_sha256_update(&hash,normalized,strlen(normalized));
    kui_sha256_digest(&hash,digest);
    kui_hex(digest,16,key);
    return true;
}
static inline bool kui_games_media_path(const char *path, const char *folder, const char *suffix,
        char *out, size_t cap) {
    char hex[33];
    if(!kui_games_media_key(path,hex)) return false;
    int n = snprintf(out, cap, "%s/%s.%s", folder, hex, suffix);
    return n > 0 && (size_t)n < cap;
}
/* A KCL1 record points directly to one native .kct in the downloads folder. */
#define KUI_COMPACT_LINK_BYTES 36u
/* Enumerate the same usable KCL1/KCU1 references as the cover reader.
 * file is positioned at its start. Empty keys indicate unused slots. */
FRESULT kui_games_cover_reference_keys(FIL *file,bool overlay,char keys[2][33]);
/* Optionally returns the resolved native file path. Caller closes on FR_OK. */
FRESULT kui_games_open_compact_target(FIL *file, const char *path, char *target, size_t cap);
FRESULT kui_games_open_compact_direct(FIL *file,const char *base);
bool kui_games_publish_cover(const char *staged,const char *final,kui_cancel_fn cancel);
enum kui_games_media_cover_state {
    KUI_GAMES_MEDIA_COVER_MISSING, KUI_GAMES_MEDIA_COVER_MANAGED, KUI_GAMES_MEDIA_COVER_OWNER
};
bool kui_games_prepare_media_cover_state(const struct kui_games_entry *entry,
    enum kui_games_media_cover_state *state,struct kui_games_disc_identity *identity,
    kui_log_fn log,kui_cancel_fn cancel);
FRESULT kui_games_open_compact(FIL *file, const char *path);
/* One storage worker, card already mounted. Uses the normal box-art scanner
 * to prepare an individual record, with a collision check before any write.
 * On success, present includes an assigned or on-card owner image. If it
 * read the disc's IP sector, return its verified identity for this scan. */
bool kui_games_prepare_media_cover(const struct kui_games_entry *entry,bool *present,
    struct kui_games_disc_identity *identity,kui_log_fn log,kui_cancel_fn cancel);
/* Reads the current image's IP sector, including single-disc games. */
enum kui_games_identity_result kui_games_media_identity(const char *path,
    struct kui_games_disc_identity *out,kui_log_fn log,kui_cancel_fn cancel);
#endif
