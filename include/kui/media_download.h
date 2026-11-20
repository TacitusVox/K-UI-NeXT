/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_DOWNLOAD_H
#define KUI_MEDIA_DOWNLOAD_H
#include "kui/games.h"
#include "kui/internet.h"
#include "ff.h"
enum kui_media_kind { KUI_MEDIA_COVER, KUI_MEDIA_MANUAL, KUI_MEDIA_PREVIEW };
enum kui_media_result {
    KUI_MEDIA_INSTALLED, KUI_MEDIA_EXISTING, KUI_MEDIA_MISSING, KUI_MEDIA_UNIDENTIFIED,
    KUI_MEDIA_STOPPED, KUI_MEDIA_LIMITED, KUI_MEDIA_ERROR, KUI_MEDIA_ORIGINAL, KUI_MEDIA_ORIGINAL_EXISTING,
    KUI_MEDIA_CONNECT_FAILED, KUI_MEDIA_OWNER
};
enum kui_media_reason {
    KUI_MEDIA_REASON_NONE, KUI_MEDIA_REASON_DISC_ID, KUI_MEDIA_REASON_NO_RELEASE,
    KUI_MEDIA_REASON_NOT_LISTED, KUI_MEDIA_REASON_PROVIDER_MISSING, KUI_MEDIA_REASON_TOO_LARGE,
    KUI_MEDIA_REASON_INVALID_ASSET, KUI_MEDIA_REASON_LOCAL_PATH
};
static inline const char *kui_media_reason_text(enum kui_media_reason reason) {
    switch(reason) {
    case KUI_MEDIA_REASON_DISC_ID: return "Could not identify the disc image";
    case KUI_MEDIA_REASON_NO_RELEASE: return "No matching release in the media catalogue";
    case KUI_MEDIA_REASON_NOT_LISTED: return "No source listed for this asset";
    case KUI_MEDIA_REASON_PROVIDER_MISSING: return "Provider reported the file missing (404/410)";
    case KUI_MEDIA_REASON_TOO_LARGE: return "Asset exceeds K-UI's size limit";
    case KUI_MEDIA_REASON_INVALID_ASSET: return "Downloaded data failed the format or integrity check";
    case KUI_MEDIA_REASON_LOCAL_PATH: return "Cannot use this game's media path";
    default: return "";
    }
}
/* Viewer paths remain stable for existing downloads and locally prepared media. */
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
/* Older versions filed each disc separately. Only populated for a distinct member. */
bool kui_media_local_fallback(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
/* Open the published upgrade, if any, or the original clip. Caller has mounted storage. */
FRESULT kui_media_open_preview(FIL *file, const char *canonical_path);
/* Original PDF when the provider does not offer page images. */
bool kui_media_original_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
enum kui_media_phase { KUI_MEDIA_RECEIVE, KUI_MEDIA_CONVERTING, KUI_MEDIA_MANUAL_PAGE, KUI_MEDIA_COVER_SCALING };
/* Receive reports transfer bytes, manual-page reports the one-based page/count. */
typedef void (*kui_media_progress_fn)(void *ctx, enum kui_media_phase phase, uint32_t done, uint32_t total);
/* One game for the duration of its scan. A confirmed identity always has a
 * product code. Never carry it to the next entry or across a card remount. */
struct kui_artwork_batch {
    char group[33];
    unsigned count;
    struct {
        const char *release;
        char master[96];
    } checked[9]; /* The catalogue admits at most nine different disc ordinals. */
};
struct kui_media_game {
    const struct kui_games_entry *entry;
    unsigned previews_removed;
    bool preview_cleanup_pending;
    enum kui_media_reason reason; /* Result detail for the most recent asset request. */
    struct kui_artwork_batch *artwork_batch;
    struct kui_games_disc_identity identity;
    struct kui_games_disc_identity set_identity;
    enum kui_artwork_mode artwork_mode;
};
/* A run shares only completed checks of an identical catalogue artwork in the
 * same verified set. The release name is owned by the static provider catalogue. */
const char *kui_artwork_batch_checked(const struct kui_media_game *game,const char *release);
void kui_artwork_batch_remember(const struct kui_media_game *game,const char *release,
    const char *verified_master);
/* Called only when the scan needs an HTTP request. The batch owns the
 * returned connection through its final asset. */
typedef struct kui_internet *(*kui_media_connect_fn)(void *ctx);
enum kui_media_result kui_media_fetch_internet(struct kui_media_game *game,
    enum kui_media_kind kind, kui_media_connect_fn connect, kui_log_fn log,
    kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx);
#endif
