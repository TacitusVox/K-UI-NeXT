/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_DOWNLOAD_H
#define KUI_MEDIA_DOWNLOAD_H
#include "kui/games.h"
#include "kui/network_bba.h"
enum kui_media_kind { KUI_MEDIA_COVER, KUI_MEDIA_MANUAL, KUI_MEDIA_PREVIEW };
enum kui_media_result {
    KUI_MEDIA_INSTALLED, KUI_MEDIA_EXISTING, KUI_MEDIA_MISSING,
    KUI_MEDIA_UNIDENTIFIED, KUI_MEDIA_STOPPED, KUI_MEDIA_LIMITED, KUI_MEDIA_ERROR
};
#define KUI_MEDIA_DEFAULT_PORT 8732u
/* FatFs path for this entry's media. Manuals and previews use the normalized
 * full image path as identity, including when two folders share a name. */
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
    char out[KUI_GAMES_FILE_CAP]);
/* Storage worker only, one request at a time on slot zero of the caller's
 * online BBA session. No file handles or sockets retained between entries. */
enum kui_media_result kui_media_fetch(const struct kui_bba_port *port, struct kui_bba_session *session,
    const uint8_t ip[4], uint16_t port_number, const struct kui_games_entry *entry,
    enum kui_media_kind kind, kui_log_fn log, kui_cancel_fn cancel);
typedef void (*kui_media_progress_fn)(void *ctx, uint32_t received, uint32_t total);
enum kui_media_result kui_media_fetch_progress(const struct kui_bba_port *port, struct kui_bba_session *session,
    const uint8_t ip[4], uint16_t port_number, const struct kui_games_entry *entry, enum kui_media_kind kind,
    kui_log_fn log, kui_cancel_fn cancel, kui_media_progress_fn progress, void *ctx);
#endif
