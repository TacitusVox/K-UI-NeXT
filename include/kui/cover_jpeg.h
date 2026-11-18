/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_COVER_JPEG_H
#define KUI_COVER_JPEG_H
#include "kui/cover_png_rows.h"

/* Large baseline JPEGs use a reduced IDCT and block reads. Small legal
 * JPEGs ask for the normal stb path, including progressive thumbnails. */
enum kui_cover_rows_result kui_cover_jpeg_scaled(FIL *source,uint16_t *planes,bool small,
                                                  kui_cancel_fn cancel);
#endif
