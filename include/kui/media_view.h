/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_VIEW_H
#define KUI_MEDIA_VIEW_H

#include "kui/probe.h"
#include <stddef.h>

/* Synchronous modal viewer on the display thread. Park the storage worker first,
 * including its background SD tasks. Returns after dropping the filesystem and
 * display. Caller invalidates its shell frame and consumes the exit button. */
bool kui_media_view(const char *path, bool manual, kui_log_fn log, char *message, size_t cap, uint16_t *canvas);

#endif
