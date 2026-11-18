/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_COVER_PNG_ROWS_H
#define KUI_COVER_PNG_ROWS_H
#include "kui/probe.h"

enum kui_cover_rows_result {
    KUI_COVER_ROWS_OK, KUI_COVER_ROWS_FALLBACK, KUI_COVER_ROWS_INVALID, KUI_COVER_ROWS_ERROR
};
/* Scales a non-interlaced PNG in bounded rows. The
 * output is three contiguous native-endian RGB565 squares, largest first.
 * Other legal image encodings use the existing decoder after rewinding. */
enum kui_cover_rows_result kui_cover_png_rows(FIL *source, uint16_t *thumbnail_pixels,
                                             kui_cancel_fn cancel);
#endif
