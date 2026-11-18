/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_UPDATE_NIGHTLY_H
#define KUI_UPDATE_NIGHTLY_H
#include "kui/internet.h"
#include "kui/update_browser.h"

/* Caller owns the exclusive mounted storage worker. Returns a verified local
 * package for the ordinary review/install flow, or true with an empty path
 * when the running official nightly already matches the feed. Pass NULL as
 * running_build for local or test GUIs. A stopped or failed transfer
 * stays outside the package browser and can be resumed for the same commit. */
bool kui_update_nightly_download(enum kui_internet_route route, const char *running_build,
    char path[KUI_UPDATE_BROWSER_PATH_BYTES], struct kui_app_status *status,
    kui_log_fn log, kui_cancel_fn cancel, kui_app_progress_fn progress);
#endif
