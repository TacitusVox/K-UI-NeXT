/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_PREVIEW_H
#define KUI_INTERNET_PREVIEW_H
#include "kui/probe.h"

/* Storage worker only. Both files are open, dest is a new empty temp file.
 * On success dest contains at most ten seconds of muted KPV1 playback. */
bool kui_internet_preview_convert(FIL *source, FIL *dest, kui_cancel_fn cancel, kui_log_fn log);
#endif
