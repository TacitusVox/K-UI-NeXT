/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_INTERNET_PREVIEW_H
#define KUI_INTERNET_PREVIEW_H
#include "kui/probe.h"

enum kui_preview_conversion {
    KUI_CONVERSION_OK, KUI_CONVERSION_UNPLAYABLE, KUI_CONVERSION_IO_ERROR,
    KUI_CONVERSION_NO_MEMORY, KUI_CONVERSION_STOPPED
};

/* Storage worker only. Both files are open, dest is a new empty temp file.
 * Starting at start_seconds, emit at most ten seconds of KPV2 video and optional mono PCM.
 * An optional readback CRC covers the emitted payload followed by its final header. */
enum kui_preview_conversion kui_internet_preview_convert(FIL *source, FIL *dest, unsigned start_seconds,
                                  kui_cancel_fn cancel, kui_log_fn log,
                                  uint32_t *readback_crc);
#endif
