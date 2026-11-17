/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAMES_MEDIA_H
#define KUI_GAMES_MEDIA_H
#include "kui/games.h"

/* The media companion listens on a computer on the same LAN. */
#define KUI_GAMES_MEDIA_ART 1u
#define KUI_GAMES_MEDIA_MANUAL 2u
#define KUI_GAMES_MEDIA_PREVIEW 4u
struct kui_games_media_job {
    struct kui_games_entry selected;
    uint8_t ip[4];
    unsigned kinds;
    bool all;
};
/* All storage and BBA operations run on the exclusive worker. An absent
 * preference successfully loads as 0.0.0.0 (explicit setup required). */
bool kui_games_media_address(uint8_t ip[4],bool save,kui_log_fn log);
void kui_games_media_run(const struct kui_games_media_job *job,struct kui_app_status *out,
    kui_log_fn log,kui_cancel_fn cancel,kui_app_progress_fn progress);
#endif
