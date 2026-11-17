/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_DOWNLOAD_H
#define KUI_MEDIA_DOWNLOAD_H
#include "kui/games.h"
#include "kui/internet.h"
enum kui_media_kind { KUI_MEDIA_COVER, KUI_MEDIA_MANUAL, KUI_MEDIA_PREVIEW };
enum kui_media_result {
    KUI_MEDIA_INSTALLED, KUI_MEDIA_EXISTING, KUI_MEDIA_MISSING, KUI_MEDIA_UNIDENTIFIED,
    KUI_MEDIA_STOPPED, KUI_MEDIA_LIMITED, KUI_MEDIA_ERROR, KUI_MEDIA_ORIGINAL
};
/* Viewer paths remain stable for existing downloads and locally prepared media. */
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
/* Original PDF when the provider does not offer page images. */
bool kui_media_original_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
enum kui_media_phase { KUI_MEDIA_RECEIVE, KUI_MEDIA_CONVERTING, KUI_MEDIA_MANUAL_PAGE };
/* Receive reports transfer bytes, manual-page reports the one-based page/count. */
typedef void (*kui_media_progress_fn)(void *ctx, enum kui_media_phase phase, uint32_t done, uint32_t total);
enum kui_media_result kui_media_fetch_internet(struct kui_internet *session,
    const struct kui_games_entry *entry, enum kui_media_kind kind, kui_log_fn log,
    kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx);
#endif
