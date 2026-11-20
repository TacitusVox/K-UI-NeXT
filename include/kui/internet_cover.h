/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_COVER_H
#define KUI_INTERNET_COVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct kui_games_disc_identity;

/* Exact Libretro Named_Boxarts filename stem, held in read-only storage.
 * NULL means this region, product and title do not identify a single cover. */
const char *kui_internet_cover_name(const struct kui_games_disc_identity *id);

/* Writes the percent-encoded absolute origin URL including its terminator.
 * Clears the destination on a miss or insufficient space. */
bool kui_internet_cover_url(const struct kui_games_disc_identity *id, char *url, size_t cap);
/* Pre-generated regional JPEG from the source. False selects the full PNG. */
bool kui_internet_cover_small_url(const struct kui_games_disc_identity *id, char *url, size_t cap);
/* A catalogue-pinned generic item tile is identified by both the expected
 * source size and CRC, including across conditional HTTP requests. */
bool kui_internet_cover_small_pin(const struct kui_games_disc_identity *id,uint32_t *bytes,uint32_t *crc);
/* Generic item tiles may change; only the verified front-image bytes qualify. */
bool kui_internet_cover_small_matches(const struct kui_games_disc_identity *id, uint32_t bytes, uint32_t crc);
#endif
