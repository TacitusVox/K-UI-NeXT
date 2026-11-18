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
static bool drop, missing, fail_alloc, pause_ui;
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
static void frame_free(void *p) {if(p){assert(live);--live;free(p);}}
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
    c->open=true;c->frames=4;
    return true;
}
bool kui_hover_clip_read(struct kui_hover_clip *c,unsigned frame,uint16_t *p,kui_cancel_fn cancel) {
    assert(!held && c->open && p && p!=games_hover_visible && frame<c->frames && !cancel());
    ++reads;
    *p=(uint16_t)frame;
    if(drop && frame==1) {
        busy=true; /* The host controller accepts a launch during an SD read. */
        assert(update(NULL) && !games_hover_visible && cancel());
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
                                          "games_hover_cancelled", "games_hover_idle_work")]
checks = r"""
#undef malloc
#undef free
static void arm(void) {
    assert(!games_hover_idle_work());
    assert(!update(&key));
    now+=5999;
    assert(!update(&key) && !games_hover_idle_work());
    now++;
    assert(!update(&key));
}
int main(void) {
    strcpy(key.path,"/Games/One/disc.gdi");strcpy(key.root,"/Games");
    strcpy(shell.games_listing.entries[0].path,key.path);
    /* Even repeated worker polls during browsing do no preview I/O or allocation. */
    for(unsigned i=0;i<20;i++) {
        update(&key);now+=5800;update(&key);assert(!games_hover_idle_work());
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
    missing=false;update(NULL);arm();drop=true;
    assert(games_hover_idle_work() && busy && !games_hover_visible && !live);
    assert(closes==2 && !games_hover_idle_work());
    busy=false;drop=false;arm();fail_alloc=true;
    assert(games_hover_idle_work() && !update(&key) && !live && closes==3);
    fail_alloc=false;update(NULL);arm();

    /* A slow renderer retains exactly one unpublished frame, then expires. */
    pause_ui=true;
    unsigned before=reads;
    assert(games_hover_idle_work() && reads==before+1 && live==1);
    assert(!update(&key) && !live && !games_hover_idle_work());
    puts("PASS Games hover playback, six-second no-I/O dwell, once per title, two frames, cancellation, cache miss, allocation failure and backpressure");
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
