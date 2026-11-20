/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_MEDIA_CONTAINER_H
#define KUI_MEDIA_CONTAINER_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"

#define KUI_PREVIEW_WIDTH 160u
#define KUI_PREVIEW_HEIGHT 120u
#define KUI_PREVIEW_FPS 10u
#define KUI_PREVIEW_AUDIO_RATE 16000u
#define KUI_PREVIEW_AUDIO_SAMPLES_PER_FRAME (KUI_PREVIEW_AUDIO_RATE / KUI_PREVIEW_FPS)
#define KUI_PREVIEW_FRAMES_MAX 100u
#define KUI_PREVIEW_HEADER_BYTES 16u
#define KUI_PREVIEW_FRAME_BYTES (2u * KUI_PREVIEW_WIDTH * KUI_PREVIEW_HEIGHT)
#define KUI_MANUAL_PAGES_MAX 256u
#define KUI_MANUAL_PAGE_BYTES_MAX (512u * 1024u)
#define KUI_MANUAL_BYTES_MAX (32u * 1024u * 1024u)
#define KUI_MANUAL_EDGE_MAX 1024u

/* A manual index stores absolute byte offsets including the final EOF offset.
 * A preview has fixed-size frames and does not use the offsets. */
struct kui_media_index {
    unsigned count;
    unsigned version, audio_rate;
    uint32_t offsets[KUI_MANUAL_PAGES_MAX + 1u];
};

enum kui_preview_index_result {
    KUI_PREVIEW_INDEX_OK, KUI_PREVIEW_INDEX_INVALID, KUI_PREVIEW_INDEX_IO_ERROR
};
/* Validate an installed preview without confusing malformed content with a failed card read. */
enum kui_preview_index_result kui_preview_index(FIL *file, struct kui_media_index *index);

/* Caller owns an open readable FatFs file. File position is unspecified on return.
 * Checks the complete container structure; JPEG pixels are decoded on demand. */
bool kui_media_container_index(FIL *file, bool manual, struct kui_media_index *index);
bool kui_media_container_check(FIL *file, bool manual);
/* Read one frame from a validated preview into a caller-owned RGB565 plane.
 * A cancelled or failed read leaves pixels incomplete; discard that buffer. */
bool kui_preview_read_frame(FIL *file, unsigned count, unsigned frame,
                            uint16_t *pixels, bool (*cancel)(void));
/* Load the optional mono PCM16 track from a validated preview. Caller frees it.
 * A silent track, cancellation or failed read returns NULL. */
int16_t *kui_preview_load_audio(FIL *file, unsigned frames, unsigned rate,
                                bool (*cancel)(void));
/* New manuals only. Verify every page's JPEG envelope and declared dimensions
 * before publication, skipping compressed scan bytes on the SD card. */
bool kui_manual_check_pages(FIL *file, bool (*cancel)(void));

#endif
