/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_GAMES_MEDIA_H
#define KUI_GAMES_MEDIA_H
#include "kui/games.h"
#include "kui/internet.h"
#include "kui/media_theme_jobs.h"

#define KUI_GAMES_MEDIA_ART 1u
#define KUI_GAMES_MEDIA_MANUAL 2u
#define KUI_GAMES_MEDIA_PREVIEW 4u

struct kui_games_media_job {
    struct kui_games_entry selected;
    unsigned kinds;
    bool all;
    enum kui_internet_route route;
};
/* The exclusive storage worker owns the network session. Returns whether
 * this selected-game scan found or installed a locally readable manual. */
bool kui_games_media_run(const struct kui_games_media_job *job,struct kui_app_status *out,
    kui_log_fn log,kui_cancel_fn cancel,kui_app_progress_fn progress);
#endif
