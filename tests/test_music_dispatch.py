#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the actual main.c song preload helper.

music_cache reads through the open Games storage session instead of
unmounting it, and only an unexplained failure gives that session up. The
helper is extracted verbatim and run with ASan/UBSan against recording stubs.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/dreamcast/main.c").read_text()


def function(name):
    """Return the definition (not a forward declaration) of a static function."""
    match = re.search(r"static [^;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", SOURCE)
    assert match, name
    at = match.start()
    brace = SOURCE.index("{", match.end() - 1)
    depth, end = 1, brace + 1
    while depth:
        depth += (SOURCE[end] == "{") - (SOURCE[end] == "}")
        end += 1
    return SOURCE[at:end]


PROGRAM = r'''
#include "kui/music.h"
#include "kui/games_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int locked;
static struct kui_music_status live;
static unsigned status_copies;
static unsigned mounted_reads,own_reads,releases,opens;
static bool session_active,session_ok=true,cache_ok=true,mark_missing,cancelled;
static void kui_log(const char *format,...) {(void)format;}
void kui_music_status_copy(struct kui_music_status *out) {assert(!locked);++status_copies;*out=live;}
/* Worker-side storage stubs. */
static bool music_preload_cancelled(void) {return cancelled;}
bool kui_games_storage_active(void) {return session_active;}
enum kui_games_storage_result kui_games_storage_open(kui_log_fn log) {
    (void)log;assert(session_active);++opens;return session_ok?KUI_GAMES_STORAGE_OK:KUI_GAMES_STORAGE_NO_MOUNT;
}
bool kui_games_storage_release(void) {++releases;session_active=false;return true;}
static bool cache_result(unsigned index) {
    if(!cache_ok && mark_missing) live.missing_mask|=1u<<index;
    return cache_ok;
}
bool kui_music_cache_menu_mounted(unsigned index,kui_cancel_fn cancel) {
    assert(cancel==music_preload_cancelled && session_active);++mounted_reads;return cache_result(index);
}
bool kui_music_cache_menu(unsigned index,kui_cancel_fn cancel) {
    assert(cancel==music_preload_cancelled && !session_active);++own_reads;return cache_result(index);
}
''' + function("music_cache") + r'''
int main(void) {
    /* Preload with the Games session open: read through it, keep it. */
    session_active=true;
    assert(music_cache(2) && mounted_reads==1 && !own_reads && !releases && opens==1);
    /* A missing song keeps the session; any other failure gives it up. */
    cache_ok=false;mark_missing=true;
    assert(!music_cache(3) && mounted_reads==2 && !releases && session_active && status_copies==1);
    mark_missing=false;
    assert(!music_cache(4) && mounted_reads==3 && releases==1 && !session_active);
    /* A cancelled preload is not treated as a storage failure. */
    session_active=true;cancelled=true;
    assert(!music_cache(1) && mounted_reads==4 && releases==1 && session_active);
    cancelled=false;cache_ok=true;
    /* A session that cannot be reopened is released and music mounts alone. */
    session_ok=false;
    assert(music_cache(5) && own_reads==1 && releases==2 && mounted_reads==4);
    /* No session: the original connect/mount/release path. */
    session_ok=true;
    assert(music_cache(0) && own_reads==2 && releases==3 && opens==5);
    puts("PASS music dispatch: actual preload C, Games session shared and released only on unexplained failure");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="kui-music-dispatch-") as work:
    source = Path(work) / "music_dispatch.c"
    source.write_text(PROGRAM)
    binary = Path(work) / "music_dispatch"
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    "-I" + str(ROOT / "include"), "-I" + str(ROOT / ".deps/fatfs/source"),
                    "-I" + str(ROOT / "firmware/kui-wifi/components/kwlink/include"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
