/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_THEME_RUNTIME_H
#define KUI_THEME_RUNTIME_H
#include "kui/theme.h"

#define KUI_THEME_LOAD_SPLASH (1u << KUI_THEME_SPLASH)
#define KUI_THEME_LOAD_MENU ((1u << KUI_THEME_HEADER) | (1u << KUI_THEME_BACKDROP))
#define KUI_THEME_LOAD_ALL (KUI_THEME_LOAD_SPLASH | KUI_THEME_LOAD_MENU)

/* A bundle has one owner. The storage worker prepares an unpublished bundle;
 * the drawing thread takes ownership between frames. Never free active pixels
 * from the worker or copy a live bundle without clearing its previous owner. */
struct kui_theme_bundle {
    struct kui_theme_settings settings;
    struct kui_theme_palette palette;
    struct kui_theme_image images[KUI_THEME_ASSET_COUNT];
    enum kui_theme_asset_status status[KUI_THEME_ASSET_COUNT];
    unsigned fallback_count;
    bool branded_banner;
};
void kui_theme_bundle_default(struct kui_theme_bundle *out);
/* Called with the boot card mounted, before the first GUI frame. The menu
 * worker later loads only header/backdrop using the selected settings. */
void kui_theme_startup_splash_prepare(struct kui_theme_bundle *out,kui_log_fn log);
void kui_theme_bundle_release(struct kui_theme_bundle *bundle);
void kui_theme_bundle_release_asset(struct kui_theme_bundle *bundle, unsigned kind);
/* Out must be empty. Caller owns mounted storage. Strict preparation refuses
 * missing/damaged selected files (Preview/Apply); tolerant boot retains saved
 * choices but uses built-in artwork for a damaged component. Neither writes.
 * Cancel/failure releases all candidate allocations. */
bool kui_theme_bundle_prepare(struct kui_theme_bundle *out,
    const struct kui_theme_settings *settings, unsigned asset_mask, bool strict,
    kui_log_fn log, kui_cancel_fn cancelled);
#endif
