/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_HOVER_CLIP_H
#define KUI_HOVER_CLIP_H

#include "kui/media_container.h"
#include "kui/probe.h"

struct kui_games_entry;

/* Storage worker only. Start zeroed. Close before running a different job or
 * releasing the Games mount. The GUI owns the frame buffers and generation. */
struct kui_hover_clip {
    FIL file;
    unsigned frames;
    bool open;
};
bool kui_hover_clip_open(struct kui_hover_clip *clip, const struct kui_games_entry *entry,
                         kui_log_fn log, kui_cancel_fn cancel);
bool kui_hover_clip_read(struct kui_hover_clip *clip, unsigned frame,
                         uint16_t *pixels, kui_cancel_fn cancel);
void kui_hover_clip_close(struct kui_hover_clip *clip);
#endif
