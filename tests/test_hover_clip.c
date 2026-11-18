/* SPDX-License-Identifier: GPL-3.0-only */
#include "kui/hover_clip.h"
#include "kui/games_storage.h"
#include "kui/media_download.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Generate deterministic frame bytes on demand, so a tested reader trying
 * to buffer the entire card file will fail the per-read size assertion. */
static bool mounted, opened, keep, missing, bad_header, no_card, short_read, fail_close;
static unsigned frames, mounts, ends, opens, closes, reads, seeks, body_read, cancel_at;
static FSIZE_t size, reject_seek;
static const char *const clip_path = "0:/KUI/media/test.kpv";
static struct kui_games_entry game = {.path="/Games/Test/disc.gdi"};

static bool cancelled(void) { return reads >= cancel_at; }
bool kui_media_file_path(const struct kui_games_entry *entry, enum kui_media_kind kind,
                         char out[KUI_GAMES_FILE_CAP]) {
    assert(entry && kind == KUI_MEDIA_PREVIEW);
    if(!entry->path[0]) return false;
    strcpy(out, clip_path);
    return true;
}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;
    assert(!mounted); ++mounts;
    if(no_card) return KUI_GAMES_STORAGE_NO_CARD;
    mounted = true;
    return KUI_GAMES_STORAGE_OK;
}
bool kui_games_storage_done(bool keep_mount) {
    assert(mounted && !opened);
    mounted = false; keep = keep_mount; ++ends;
    return true;
}
FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode) {
    assert(mounted && !opened && mode == FA_READ && !strcmp(path, clip_path));
    ++opens;
    if(missing) return FR_NO_FILE;
    opened = true;
    memset(file, 0, sizeof(*file)); file->obj.objsize = size;
    return FR_OK;
}
FRESULT f_close(FIL *file) {
    (void)file;
    assert(mounted && opened);
    opened = false; ++closes;
    return fail_close ? FR_DISK_ERR : FR_OK;
}
FRESULT f_lseek(FIL *file, FSIZE_t off) {
    assert(mounted && opened); ++seeks;
    if(off == reject_seek) return FR_DISK_ERR;
    assert(off <= size);
    file->fptr = off;
    return FR_OK;
}
static uint8_t plane_byte(FSIZE_t offset) {
    return (uint8_t)((offset / KUI_PREVIEW_FRAME_BYTES)*37u + offset%193u);
}
FRESULT f_read(FIL *file, void *dest, UINT bytes, UINT *got) {
    assert(mounted && opened && bytes && bytes <= 8192 && file->fptr+bytes <= size);
    ++reads;
    uint8_t *out = dest;
    if(!file->fptr) {
        static const uint8_t header[12] = {'K','P','V','1',160,0,120,0,10,0,0,0};
        assert(bytes == 16);
        memcpy(out,header,12);
        if(bad_header) out[6] = 0;
        for(unsigned i=0; i<4; ++i) out[12+i] = (uint8_t)(frames >> (i*8));
    } else {
        assert(file->fptr >= KUI_PREVIEW_HEADER_BYTES);
        if(short_read) --bytes;
        for(UINT i=0; i<bytes; ++i)
            out[i] = plane_byte(file->fptr+i-KUI_PREVIEW_HEADER_BYTES);
        body_read += bytes;
    }
    file->fptr += bytes; *got = bytes;
    return FR_OK;
}

static void reset(unsigned count) {
    assert(!mounted && !opened);
    frames = count; size = KUI_PREVIEW_HEADER_BYTES + (FSIZE_t)count*KUI_PREVIEW_FRAME_BYTES;
    no_card = missing = bad_header = short_read = fail_close = false;
    mounts=ends=opens=closes=reads=seeks=body_read=0;
    cancel_at = UINT32_MAX; reject_seek = (FSIZE_t)-1;
}
static uint16_t pixels[KUI_PREVIEW_FRAME_BYTES / 2];
static void check_pixels(unsigned frame) {
    const uint8_t *b = (const uint8_t *)pixels;
    for(unsigned i=0; i<KUI_PREVIEW_FRAME_BYTES; ++i)
        assert(b[i] == plane_byte((FSIZE_t)frame*KUI_PREVIEW_FRAME_BYTES+i));
}
int main(void) {
    struct kui_hover_clip clip = {0};
    reset(100);
    assert(kui_hover_clip_open(&clip,&game,NULL,cancelled) && clip.frames == 100);
    assert(mounts == 1 && opens == 1 && reads == 1 && !body_read);
    unsigned initial_seeks = seeks;
    assert(kui_hover_clip_read(&clip,0,pixels,cancelled)); check_pixels(0);
    assert(kui_hover_clip_read(&clip,1,pixels,cancelled)); check_pixels(1);
    assert(seeks == initial_seeks && body_read == 2*KUI_PREVIEW_FRAME_BYTES);
    assert(kui_hover_clip_read(&clip,99,pixels,cancelled)); check_pixels(99);
    assert(seeks == initial_seeks+1 && body_read == 3*KUI_PREVIEW_FRAME_BYTES);
    assert(!kui_hover_clip_read(&clip,100,pixels,NULL) && !clip.open && !clip.frames);
    assert(closes == 1 && ends == 1);
    kui_hover_clip_close(&clip);
    assert(closes == 1);

    reset(100);
    assert(kui_hover_clip_open(&clip,&game,NULL,NULL));
    cancel_at = reads+2;
    assert(!kui_hover_clip_read(&clip,0,pixels,cancelled));
    assert(!clip.open && keep && body_read > 8192 && body_read <= 16384);
    unsigned old_reads = reads;
    assert(!kui_hover_clip_read(&clip,0,pixels,cancelled) && old_reads == reads);

    reset(2);
    assert(kui_hover_clip_open(&clip,&game,NULL,NULL));
    assert(kui_hover_clip_open(&clip,&game,NULL,NULL));
    assert(mounts == 2 && opens == 2 && ends == 1 && closes == 1);
    reject_seek = KUI_PREVIEW_HEADER_BYTES + KUI_PREVIEW_FRAME_BYTES;
    assert(!kui_hover_clip_read(&clip,1,pixels,NULL) && !keep && !clip.open);
    assert(!body_read);
    reset(2);
    assert(kui_hover_clip_open(&clip,&game,NULL,NULL));
    short_read = true;
    assert(!kui_hover_clip_read(&clip,0,pixels,NULL) && !keep && !clip.open);

    reset(100);
    cancel_at=0;
    assert(!kui_hover_clip_open(&clip,&game,NULL,cancelled) && !opens && !mounts);
    reset(100);
    no_card=true;
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !opens && !ends);
    reset(100);
    missing=true;
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !reads && ends==1 && keep);
    reset(100);
    size=5u*1024u*1024u;
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !body_read && ends==1);
    reset(0);
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !body_read && ends==1);
    reset(1);
    bad_header=true;
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !body_read && ends==1);
    reset(1);
    size--;
    assert(!kui_hover_clip_open(&clip,&game,NULL,NULL) && !body_read && ends==1);
    reset(1);
    assert(kui_hover_clip_open(&clip,&game,NULL,NULL));
    fail_close=true;
    kui_hover_clip_close(&clip);
    assert(!keep && !clip.open && ends==1);

    puts("Hover clip: bounded frames, sequential/late seeks, cancel, storage release, missing and invalid KPV passed");
    return 0;
}
