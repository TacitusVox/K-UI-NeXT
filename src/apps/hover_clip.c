/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hover_clip.h"
#include "kui/games_storage.h"
#include "kui/media_download.h"

static void quiet(const char *format, ...) { (void)format; }

/* Complete exactly one storage job for each successful mount acquisition. */
static void finish(struct kui_hover_clip *clip, bool keep_mount) {
    bool closed = f_close(&clip->file) == FR_OK;
    clip->open = false;
    clip->frames = clip->audio_rate = 0;
    (void)kui_games_storage_done(keep_mount && closed);
}

void kui_hover_clip_close(struct kui_hover_clip *clip) {
    if(clip && clip->open) finish(clip, true);
}

bool kui_hover_clip_open(struct kui_hover_clip *clip, const struct kui_games_entry *entry,
                         kui_log_fn log, kui_cancel_fn cancel) {
    char path[KUI_GAMES_FILE_CAP];
    if(!clip) return false;
    kui_hover_clip_close(clip);
    if(!entry || entry->disabled || entry->directory || (cancel && cancel()) ||
       !kui_media_file_path(entry, KUI_MEDIA_PREVIEW, path)) return false;
    if(kui_games_storage_open(log ? log : quiet) != KUI_GAMES_STORAGE_OK) return false;
    if(cancel && cancel()) {
        (void)kui_games_storage_done(true);
        return false;
    }
    FRESULT result = kui_media_open_preview(&clip->file, path);
    if((result == FR_NO_FILE || result == FR_NO_PATH) &&
       kui_media_local_fallback(entry,KUI_MEDIA_PREVIEW,path))
        result = kui_media_open_preview(&clip->file,path);
    if(result != FR_OK) {
        (void)kui_games_storage_done(result == FR_NO_FILE || result == FR_NO_PATH);
        return false;
    }

    /* Reading the header validates the complete expected video/audio size.
     * PCM and video frames are read separately after focus is established. */
    struct kui_media_index index;
    if(!kui_media_container_index(&clip->file, false, &index) || (cancel && cancel())) {
        finish(clip, f_error(&clip->file) == 0);
        return false;
    }
    clip->open = true;
    clip->frames = index.count;
    clip->audio_rate = index.audio_rate;
    return true;
}

bool kui_hover_clip_read(struct kui_hover_clip *clip, unsigned frame,
                         uint16_t *pixels, kui_cancel_fn cancel) {
    if(!clip || !clip->open) return false;
    if(kui_preview_read_frame(&clip->file, clip->frames, frame, pixels, cancel)) return true;
    /* Partial or stale pixels are never published by the caller. */
    finish(clip, cancel && cancel());
    return false;
}
