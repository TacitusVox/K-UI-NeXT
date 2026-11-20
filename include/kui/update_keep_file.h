/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_UPDATE_KEEP_FILE_H
#define KUI_UPDATE_KEEP_FILE_H
#include "kui/update_online.h"

/* Caller owns the mounted card. A new card uses the default. If saved records
 * exist but none is valid, keep all downloads until a new limit is saved.
 * An I/O failure returns false; don't act on an unread limit. */
bool kui_update_preferences_load(unsigned *keep, enum kui_update_channel *channel);
bool kui_update_preferences_save(unsigned keep, enum kui_update_channel channel);
bool kui_update_keep_load(unsigned *keep);
bool kui_update_keep_save(unsigned keep);
#endif
