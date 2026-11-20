/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_ARTWORK_VALIDATORS_H
#define KUI_ARTWORK_VALIDATORS_H
#include "kui/internet.h"

/* Mounted Games worker only. Each catalogue release has one thumbnail and
 * one fallback slot. source is the URL including any expected tile size/CRC.
 * A condition applies only to matching native KCT bytes and source.
 * A cache miss clears out. Save only after verifying the installed pixels. */
bool kui_artwork_validators_load(const char *release,bool small,const char *master,
    const char *source,struct kui_http_validators *out);
bool kui_artwork_validators_save(const char *release,bool small,const char *master,
    const char *source,const struct kui_http_validators *in);
#endif
