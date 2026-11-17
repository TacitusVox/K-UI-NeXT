/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_THEME_H
#define KUI_THEME_H
#include "kui/probe.h"

#define KUI_THEME_ID_BYTES 32u
#define KUI_THEME_NAME_BYTES 48u
#define KUI_THEME_FILE_BYTES 64u
#define KUI_THEME_CATALOG_MAX 24u
#define KUI_THEME_MANIFEST_MAX 4096u
#define KUI_THEME_ROOT "0:/KUI/themes"
#define KUI_THEME_SETTINGS_PATH_A "0:/KUI/apps/system/theme-a.bin"
#define KUI_THEME_SETTINGS_PATH_B "0:/KUI/apps/system/theme-b.bin"
#define KUI_THEME_SETTINGS_RECORD_SIZE 256u
#define KUI_THEME_IMAGE_HEADER_SIZE 32u
#define KUI_THEME_WIDTH 640u
#define KUI_THEME_HEIGHT 480u
#define KUI_THEME_HEADER_HEIGHT 100u

enum kui_theme_color {
    KUI_THEME_BACKGROUND, KUI_THEME_PANEL, KUI_THEME_SELECTION,
    KUI_THEME_EDGE, KUI_THEME_TEXT, KUI_THEME_MUTED, KUI_THEME_ACCENT,
    KUI_THEME_ACCENT2, KUI_THEME_SUCCESS, KUI_THEME_WARNING,
    KUI_THEME_SELECTION_TEXT, KUI_THEME_COLOR_COUNT
};
enum kui_theme_asset_kind {
    KUI_THEME_SPLASH, KUI_THEME_HEADER, KUI_THEME_BACKDROP, KUI_THEME_ASSET_COUNT
};
struct kui_theme_palette { uint32_t colors[KUI_THEME_COLOR_COUNT]; };
struct kui_theme_definition {
    char id[KUI_THEME_ID_BYTES], name[KUI_THEME_NAME_BYTES], author[KUI_THEME_NAME_BYTES];
    char assets[KUI_THEME_ASSET_COUNT][KUI_THEME_FILE_BYTES];
    struct kui_theme_palette palette;
};
struct kui_theme_settings {
    char theme_id[KUI_THEME_ID_BYTES];
    /* Empty inherits theme_id; "default" uses built-in art; "none" disables.
     * Any other identifier selects that component from that theme folder. */
    char splash_id[KUI_THEME_ID_BYTES], header_id[KUI_THEME_ID_BYTES], background_id[KUI_THEME_ID_BYTES];
    uint32_t override_mask;
    struct kui_theme_palette palette;
};
struct kui_theme_catalog {
    struct kui_theme_definition themes[KUI_THEME_CATALOG_MAX];
    unsigned count;
    bool truncated;
};
struct kui_theme_image { uint16_t *pixels; unsigned width,height; };
enum kui_theme_asset_status {
    KUI_THEME_ASSET_DEFAULT, KUI_THEME_ASSET_READY, KUI_THEME_ASSET_DISABLED,
    KUI_THEME_ASSET_FAILED, KUI_THEME_ASSET_CANCELLED
};

uint16_t kui_theme_rgb565(uint32_t rgb);
uint32_t kui_theme_rgb888(uint16_t rgb565);
const char *kui_theme_color_name(unsigned role);
const char *kui_theme_color_key(unsigned role);
void kui_theme_palette_default(struct kui_theme_palette *out);
void kui_theme_definition_default(struct kui_theme_definition *out);
void kui_theme_settings_default(struct kui_theme_settings *out);
/* Reserved selectors default/none must use their canonical lowercase form. */
bool kui_theme_id_valid(const char *id);
bool kui_theme_settings_valid(const struct kui_theme_settings *settings);
const char *kui_theme_component_id(const struct kui_theme_settings *settings,enum kui_theme_asset_kind kind);
void kui_theme_effective_palette(struct kui_theme_palette *out,const struct kui_theme_definition *definition,
                                 const struct kui_theme_settings *settings);
/* Strict bounded ASCII INI; unknown/duplicate keys and unsafe paths fail.
 * Decode/parse failures leave the caller's output unchanged. */
bool kui_theme_parse(struct kui_theme_definition *out,const char *id,const void *text,size_t bytes);
bool kui_theme_settings_encode(uint8_t out[KUI_THEME_SETTINGS_RECORD_SIZE],
                               const struct kui_theme_settings *settings,uint64_t sequence);
bool kui_theme_settings_decode(struct kui_theme_settings *settings,uint64_t *sequence,
                               const void *record,size_t bytes);
/* KUITEX01, LE version=1, u16 width/height, u32 payload bytes/CRC32, eight
 * reserved zero bytes. Exactly width*height*2 LE RGB565 payload follows.
 * Returns payload CRC through crc only for a valid kind/dimension/size. */
bool kui_theme_image_header(const void *header,size_t bytes,enum kui_theme_asset_kind kind,
                            size_t file_bytes,uint32_t *crc);

/* Storage functions never mount or choose a device. Caller owns the mounted
 * FatFs volume and excludes other file workers. Never call while rendering. */
bool kui_theme_settings_load(struct kui_theme_settings *out,kui_log_fn log);
bool kui_theme_settings_save(const struct kui_theme_settings *settings,kui_log_fn log);
bool kui_theme_definition_load(struct kui_theme_definition *out,const char *id,kui_log_fn log);
bool kui_theme_catalog_scan(struct kui_theme_catalog *out,kui_log_fn log,kui_cancel_fn cancelled);
/* A zeroed image is required. Failure/default/disabled leave it empty.
 * Missing manifest asset = built-in fallback; asset value "none" = disabled.
 * A custom asset's damage returns FAILED, never unchecked pixel data. */
enum kui_theme_asset_status kui_theme_image_load(struct kui_theme_image *out,
    const struct kui_theme_definition *definition,enum kui_theme_asset_kind kind,
    kui_log_fn log,kui_cancel_fn cancelled);
void kui_theme_image_release(struct kui_theme_image *image);
#endif
