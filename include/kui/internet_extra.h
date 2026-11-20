/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_EXTRA_H
#define KUI_INTERNET_EXTRA_H
#include "kui/media_download.h"

/* Archive item page indices start at zero. Counts and asset names come from
 * public catalogue metadata. No Internet, filesystem, or heap use in lookup. */
unsigned kui_internet_manual_pages(const struct kui_games_disc_identity *id);
bool kui_internet_manual_page_url(const struct kui_games_disc_identity *id, unsigned page,
                                  char *url, size_t cap);
/* An alternate English-language PDF, or the Ogg Theora gameplay source. */
bool kui_internet_extra_url(const struct kui_games_disc_identity *id, enum kui_media_kind kind,
                            char *url, size_t cap);
/* Start attempt at zero. Try USA, PAL, then matched footage of unspecified region. */
bool kui_internet_preview_fallback_url(const struct kui_games_disc_identity *id, unsigned *attempt,
                                      char *url, size_t cap);
#endif
