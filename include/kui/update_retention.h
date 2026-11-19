/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_UPDATE_RETENTION_H
#define KUI_UPDATE_RETENTION_H
#include "kui/probe.h"
#include "kui/update_browser.h"

#define KUI_UPDATE_KEEP_DEFAULT 5u
#define KUI_UPDATE_KEEP_MAX 10u
static inline bool kui_update_keep_valid(unsigned keep) {
    return keep == 0 || keep == 1 || keep == 3 || keep == KUI_UPDATE_KEEP_DEFAULT || keep == KUI_UPDATE_KEEP_MAX;
}

#define KUI_NIGHTLY_DIRECTORY KUI_UPDATE_BROWSER_ROOT "/GitHub"
#define KUI_NIGHTLY_PACKAGE KUI_NIGHTLY_DIRECTORY "/nightly-%.40s.kup"
#define KUI_NIGHTLY_STATE "0:/KUI/nightly-cache"
#define KUI_NIGHTLY_PART KUI_NIGHTLY_STATE "/download.part"

struct kui_nightly_asset {
    char commit[41], digest[65];
    uint32_t size;
};

enum kui_nightly_verify { KUI_NIGHTLY_VERIFIED, KUI_NIGHTLY_MISSING, KUI_NIGHTLY_MISMATCH,
    KUI_NIGHTLY_STORAGE_ERROR, KUI_NIGHTLY_STOPPED };
enum kui_nightly_verify kui_nightly_verify_file(const char *path, const struct kui_nightly_asset *asset,
    kui_cancel_fn cancel, void (*progress)(void *, uint32_t), void *context);

/* Exclusive mounted worker. A changed manifest invalidates the sole resume slot.
 * Publish the receipt only for a verified transfer whose destination was absent. */
bool kui_nightly_prepare_partial(const struct kui_nightly_asset *asset);
bool kui_nightly_record_download(const struct kui_nightly_asset *asset, kui_cancel_fn cancel);
/* Pass NULL for protected_commit when changing the limit offline. */
bool kui_nightly_trim(unsigned keep, const char *protected_commit, kui_cancel_fn cancel);
/* Any attempted unlink can have affected storage, including a failed write. */
bool kui_nightly_trim_report(unsigned keep, const char *protected_commit, kui_cancel_fn cancel, bool *wrote);
#endif
