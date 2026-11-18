/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_COMPACT_COVER_H
#define KUI_COMPACT_COVER_H

#include "kui/game_cover.h"
#include "kui/probe.h"

#define KUI_INTERNET_COVER_MAX_BYTES (32u * 1024u * 1024u)
#define KUI_INTERNET_COVER_MAX_EDGE 4096u

/* KCT1: magic + one little-endian CRC32 per display size, then the same
 * little-endian RGB565 planes as a full KCV record, without its header. */
#define KUI_COMPACT_COVER_HEADER 16u
#define KUI_COMPACT_COVER_BYTES (KUI_COMPACT_COVER_HEADER + KUI_COVER_FILE_BYTES - KUI_COVER_HEADER_BYTES)
enum kui_compact_result { KUI_COMPACT_OK, KUI_COMPACT_INVALID, KUI_COMPACT_ERROR };
/* Caller provides a temporary destination. It may be the source FIL: all
 * thumbnails are complete before any source bytes would be overwritten. */
enum kui_compact_result kui_compact_cover_encode(FIL *source, FIL *dest, bool small_jpeg, kui_cancel_fn cancel);
bool kui_compact_cover_pixels(FIL *file, enum kui_cover_size size, uint16_t *pixels);
enum kui_compact_result kui_compact_cover_verify(FIL *file, kui_cancel_fn cancel);
#endif
