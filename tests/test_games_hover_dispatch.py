#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the production Games preview worker and UI frame handoff."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / "src/dreamcast/main.c").read_text()
shell = (root / "src/core/shell.c").read_text()


def function(source, name):
    match = re.search(r"^[\w *]+\b" + name + r"\([^;{}]*\)\s*\{", source, re.M)
    assert match, name
    depth, end = 1, match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


prefix = r"""
#include "kui/hover_clip.h"
#include "kui/shell.h"
#include "kui/music.h"
#include <stdatomic.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct kui_shell_preview_focus games_hover_focus;
static struct kui_shell_games_art_key key;
static struct kui_shell shell;
static bool games_hover_finished, busy, cd_drive_owned, screenshot_frame_pending;
static uint16_t *games_hover_visible, *games_hover_ready, *games_hover_spare;
static unsigned games_hover_served, reads, opens, closes, frames, allocations, live, peak;
static uint64_t now;
static int lock, held;
static bool drop, missing, fail_alloc, pause_ui, with_audio;
static atomic_bool games_hover_audio_cancel;
static int16_t pcm[6400];
static bool audio_loaded, playing;
static unsigned audio_loads, audio_starts, audio_stops;
static kui_cancel_fn audio_cancel;
static bool games_hover_update_locked(const struct kui_shell_games_art_key *,uint64_t);
static void mutex_lock(int *m) {assert(m==&lock && !held);held=1;}
static void mutex_unlock(int *m) {assert(m==&lock && held);held=0;}
static uint64_t timer_ms_gettime64(void) {return now;}
static bool games_idle_blocked_locked(void) {assert(held);return busy;}
static void *frame_alloc(size_t n) {
    assert(!held && n==38400 && opens);
    ++allocations;
    if(fail_alloc)return NULL;
    if(++live>peak)peak=live;
    assert(live<=2u);
    return malloc(n);
}
static void frame_free(void *p) {
    if(p==pcm) {assert(audio_loaded && !playing);audio_loaded=false;}
    else if(p) {assert(live);--live;free(p);}
}
bool kui_music_preview_enabled(void) {assert(!held);return true;}
int16_t *kui_preview_load_audio(FIL *file,unsigned count,unsigned rate,bool (*cancel)(void)) {
    assert(!held && file && count==4 && rate==16000 && !cancel() && !audio_loaded);
    ++audio_loads;audio_loaded=true;return pcm;
}
bool kui_music_preview_play(const int16_t *data,size_t count,unsigned rate,kui_cancel_fn cancel) {
    assert(!held && !playing && audio_loaded && rate==16000 && games_hover_visible && !cancel());
    assert(data>=pcm && data<pcm+1600 && data+count==pcm+6400);
    ++audio_starts;playing=true;audio_cancel=cancel;return true;
}
void kui_music_preview_stop(void) {
    assert(!held && playing);
    playing=false;++audio_stops;
}
static bool update(const struct kui_shell_games_art_key *target) {
    mutex_lock(&lock);
    bool changed=games_hover_update_locked(target,now);
    mutex_unlock(&lock);
    return changed;
}
static void thd_sleep(unsigned ms) {
    assert(!held && ms<=10u);
    now+=ms;
    if(!pause_ui && update(&key) && games_hover_visible) ++frames;
}
#define KUI_STORAGE_AUTO 0
static void kui_sd_set_params(unsigned mode,bool crc) {
    assert(!held && mode==KUI_STORAGE_AUTO && crc && games_hover_focus.due);
}
static void kui_log(const char *fmt,...) {(void)fmt;}
bool kui_hover_clip_open(struct kui_hover_clip *c,const struct kui_games_entry *e,kui_log_fn l,kui_cancel_fn cancel) {
    assert(!held && !c->open && !strcmp(e->path,key.path) && l==kui_log && !cancel());
    ++opens;
    if(missing)return false;
    c->open=true;c->frames=4;c->audio_rate=with_audio?16000:0;
    return true;
}
bool kui_hover_clip_read(struct kui_hover_clip *c,unsigned frame,uint16_t *p,kui_cancel_fn cancel) {
    assert(!held && c->open && p && p!=games_hover_visible && frame<c->frames && !cancel());
    ++reads;
    *p=(uint16_t)frame;
    if(drop && frame==1) {
        busy=true; /* The host controller accepts a launch during an SD read. */
        assert(update(NULL) && !games_hover_visible && cancel());
        if(with_audio) assert(playing && audio_cancel());
        return false;
    }
    return true;
}
void kui_hover_clip_close(struct kui_hover_clip *c) {
    assert(!held);
    if(c->open){++closes;c->open=false;}
}
#define malloc frame_alloc
#define free frame_free
"""
functions = [function(shell, n) for n in ("kui_shell_games_art_same", "kui_shell_preview_focus_update")]
functions += [function(main, n) for n in ("games_hover_update_locked", "games_hover_current_locked",
                                          "games_hover_cancelled", "games_hover_audio_interrupted", "games_hover_idle_work")]
checks = r"""
#undef malloc
#undef free
static void arm(void) {
    assert(!games_hover_idle_work());
    assert(!update(&key));
    now+=2999;
    assert(!update(&key) && !games_hover_idle_work());
    now++;
    assert(!update(&key));
}
int main(void) {
    strcpy(key.path,"/Games/One/disc.gdi");strcpy(key.root,"/Games");
    strcpy(shell.games_listing.entries[0].path,key.path);
    /* Even repeated worker polls during browsing do no preview I/O or allocation. */
    for(unsigned i=0;i<20;i++) {
        update(&key);now+=2800;update(&key);assert(!games_hover_idle_work());
        update(NULL);++key.generation;
    }
    assert(!opens && !allocations);
    arm();
    assert(games_hover_idle_work() && opens==1 && closes==1 && reads==4 && frames==4);
    assert(update(&key) && !live && !games_hover_visible && peak==2 && allocations==2);
    assert(!games_hover_idle_work()); /* One playback per dwell. */

    update(NULL);arm();missing=true;
    assert(games_hover_idle_work() && !games_hover_idle_work());
    assert(!update(&key) && opens==2 && allocations==2 && !live);
    assert(!audio_loads && !audio_starts); /* KPV1 and misses keep music untouched. */
    missing=false;update(NULL);arm();drop=true;with_audio=true;
    assert(games_hover_idle_work() && busy && !games_hover_visible && !live);
    assert(closes==2 && !games_hover_idle_work());
    assert(audio_loads==1 && audio_starts==1 && audio_stops==1 && !audio_loaded);
    busy=false;drop=false;arm();fail_alloc=true;
    assert(games_hover_idle_work() && !update(&key) && !live && closes==3);
    fail_alloc=false;update(NULL);arm();

    /* A slow renderer retains exactly one unpublished frame, then expires. */
    pause_ui=true;
    unsigned before=reads;
    assert(games_hover_idle_work() && reads==before+1 && live==1);
    assert(!update(&key) && !live && !games_hover_idle_work());
    assert(audio_loads==3 && audio_starts==1 && audio_stops==1 && !audio_loaded);
    pause_ui=false;update(NULL);arm();assert(games_hover_idle_work());
    assert(audio_starts==2 && audio_stops==2 && !audio_loaded);
    assert(update(&key) && !live);
    puts("PASS Games hover playback, three-second no-I/O dwell, once per title, two frames, cancellation, cache miss, allocation failure and backpressure");
}
"""
with tempfile.TemporaryDirectory(prefix="kui-hover-") as tmp:
    src = Path(tmp) / "hover.c"
    binary = Path(tmp) / "hover"
    src.write_text("\n".join([prefix, *functions, checks]))
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g", "-Wall",
                    "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined", "-fno-pie",
                    "-no-pie", "-Iinclude", "-I.deps/fatfs/source", "-Ifirmware/kui-wifi/components/kwlink/include",
                    str(src), "-o", str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), check=True)

# Run the full-screen viewer's production replay/exit loop on a synthetic clock.
viewer = (root / "src/dreamcast/media_view.c").read_text()
modal = r"""
#include "kui/media_container.h"
#include "kui/music.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
enum { SCREEN_W=640, CONT_A=1, CONT_B=2 };
static uint64_t now;
static unsigned starts,stops,paints,loads,allocations;
static void *modal_alloc(size_t bytes) {
    void *p=malloc(bytes);if(p) ++allocations;return p;
}
static void modal_free(void *p) {if(p){assert(allocations);--allocations;free(p);}}
#define malloc modal_alloc
#define free modal_free
static bool playing,muted,corrupt,unavailable,early_back;
static uint16_t framebuffer[640*480];
static unsigned buttons(void) {
    if(early_back || now>=360)return CONT_B;
    return now==300 ? CONT_A : 0;
}
static uint64_t timer_ms_gettime64(void) {return now;}
static void thd_sleep(unsigned ms) {now+=ms;assert(now<=400);}
static void kui_shell_font_draw(uint16_t *fb,int x,int y,int color,const char *text,bool invert) {
    (void)fb;(void)x;(void)y;(void)color;(void)text;(void)invert;
}
static void present(uint16_t *canvas) {assert(canvas==framebuffer);++paints;}
static uint16_t *clear_frame(uint16_t *canvas) {return canvas;}
bool kui_music_preview_enabled(void) {return !muted;}
bool kui_music_preview_play(const int16_t *pcm,size_t samples,unsigned rate,kui_cancel_fn cancel) {
    assert(pcm && samples==3200 && rate==16000 && !cancel && !playing && paints);
    ++starts;playing=true;return true;
}
void kui_music_preview_stop(void) {assert(playing);++stops;playing=false;}
int16_t *kui_preview_load_audio(FIL *file,unsigned count,unsigned rate,bool (*cancel)(void)) {
    assert(file && count==2 && rate==16000 && cancel);
    ++loads;
    return cancel() || unavailable ? NULL : malloc(3200*sizeof(int16_t));
}
bool kui_preview_read_frame(FIL *file,unsigned count,unsigned frame,uint16_t *pixels,bool (*cancel)(void)) {
    assert(file && count==2 && frame<count && !cancel);
    if(corrupt && frame==1)return false;
    memset(pixels,0,KUI_PREVIEW_FRAME_BYTES);return true;
}
"""
modal_checks = r"""
int main(void) {
    struct kui_media_index index={.count=2,.audio_rate=16000};
    FIL file={0};
    for(unsigned n=0;n<6;++n) {
        now=0;paints=loads=starts=stops=0;
        corrupt=n==1;muted=n==2;unavailable=n==3;early_back=n==4;
        index.audio_rate=n==5?0:16000;
        const char *problem=preview(&file,&index,framebuffer);
        assert(!playing && !allocations && (!!problem)==corrupt);
        assert(loads==(muted || !index.audio_rate ? 0u : 1u));
        if(n==0) assert(starts==2 && stops==2 && paints==3);
        else if(corrupt) assert(starts==1 && stops==1 && paints==1);
        else assert(!starts && !stops && paints==(early_back?0u:3u));
    }
}
"""
with tempfile.TemporaryDirectory(prefix="kui-modal-preview-") as tmp:
    src = Path(tmp)/"modal.c"
    binary = Path(tmp)/"modal"
    src.write_text("\n".join([modal, function(viewer,"preview_back_held"), function(viewer,"preview"), modal_checks]))
    subprocess.run([*shlex.split(os.environ.get("CC","cc")), "-std=c11","-O1","-g","-Wall","-Wextra",
                    "-Wpedantic","-Werror","-fsanitize=address,undefined","-fno-pie","-no-pie","-Iinclude",
                    "-I.deps/fatfs/source",str(src),"-o",str(binary)], cwd=root,check=True)
    subprocess.run([str(binary)], env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), check=True)
    print("PASS full-screen preview soundtrack, natural end, replay, Back, failed frame, mute and silent cache")
