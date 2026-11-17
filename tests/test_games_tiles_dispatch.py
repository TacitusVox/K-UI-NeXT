#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the actual Compact/Gallery tile ownership helpers from main.c.

The worker may write a tile only while its slot is unpublished and must
publish it under the lock; only main unpublishes (stale row, other view or a
worker invalidation request) and frees, so frames can draw published tiles
without the lock. The helpers are extracted verbatim and run with ASan/UBSan.
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


state_start = SOURCE.index("#define GAMES_TILE_BYTES")
state_end = SOURCE.index("static bool games_tiles_changed,games_tiles_invalid;") + \
    len("static bool games_tiles_changed,games_tiles_invalid;")
STATE = SOURCE[state_start:state_end]

PROGRAM = r'''
#include "kui/shell.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct kui_shell shell;
static int lock,locked;
static void mutex_lock(int *m) {assert(m==&lock && !locked);locked=1;}
static void mutex_unlock(int *m) {assert(m==&lock && locked);locked=0;}
static bool prefetch_current=true;
static bool games_prefetch_current(void) {assert(locked);return prefetch_current;}
static uint16_t games_prefetch_pixels[KUI_COVER_PIXELS];
unsigned kui_shell_games_view(const struct kui_shell *s) {return s->games_view;}
enum kui_cover_size kui_games_view_size(unsigned view) {
    return view==KUI_GAMES_VIEW_GALLERY?KUI_COVER_SIZE_MEDIUM:
        view==KUI_GAMES_VIEW_COMPACT?KUI_COVER_SIZE_SMALL:KUI_COVER_SIZE_LARGE;
}
unsigned kui_cover_edge(enum kui_cover_size size) {
    return size==KUI_COVER_SIZE_MEDIUM?KUI_COVER_MEDIUM:size==KUI_COVER_SIZE_SMALL?KUI_COVER_SMALL:KUI_COVER_LARGE;
}
''' + STATE + "\n" + function("games_tiles_free_locked") + "\n" + \
    function("games_tiles_snapshot_locked") + "\n" + function("games_tile_publish") + r'''
static void page(unsigned view,const char *prefix) {
    memset(&shell.games_listing,0,sizeof(shell.games_listing));
    shell.games_view=view;shell.games_listing.view=view;shell.games_listing.count=KUI_GAMES_ROWS;
    for(unsigned i=0;i<KUI_GAMES_ROWS;i++) {
        snprintf(shell.games_listing.entries[i].name,sizeof(shell.games_listing.entries[i].name),"%s %u",prefix,i);
        snprintf(shell.games_listing.entries[i].path,sizeof(shell.games_listing.entries[i].path),"/Games/%s%u/disc.gdi",prefix,i);
    }
}
static struct kui_shell_view snapshot(void) {
    struct kui_shell_view v={0};
    mutex_lock(&lock);games_tiles_snapshot_locked(&v);mutex_unlock(&lock);return v;
}
static bool publish(unsigned row,unsigned view,uint16_t color) {
    for(unsigned i=0;i<KUI_COVER_PIXELS;i++) games_prefetch_pixels[i]=color;
    return games_tile_publish(row,&shell.games_listing.entries[row],view,kui_games_view_size(view));
}
int main(void) {
    page(KUI_GAMES_VIEW_GALLERY,"A");
    /* List/Flow and other sizes never use tiles. */
    assert(publish(0,KUI_GAMES_VIEW_LIST,1) && !games_tiles[0] && !games_tiles_changed);
    assert(games_tile_publish(0,&shell.games_listing.entries[0],KUI_GAMES_VIEW_GALLERY,KUI_COVER_SIZE_SMALL));
    assert(!games_tiles[0]);
    /* Publish, then expose to frames. */
    assert(publish(0,KUI_GAMES_VIEW_GALLERY,0x1111) && games_tiles[0] && games_tile_info[0].published);
    assert(games_tiles_changed && !locked);games_tiles_changed=false;
    struct kui_shell_view v=snapshot();
    assert(v.game_tile_covers[0]==games_tiles[0] && v.game_tile_covers[0][KUI_COVER_MEDIUM*KUI_COVER_MEDIUM-1]==0x1111);
    for(unsigned i=1;i<KUI_GAMES_ROWS;i++) assert(!v.game_tile_covers[i]);
    /* The same entry is already published: done, unchanged, no rewrite. */
    assert(publish(0,KUI_GAMES_VIEW_GALLERY,0x2222) && games_tiles[0][0]==0x1111 && !games_tiles_changed);
    /* A new page: the stale slot blocks the worker until main releases it. */
    page(KUI_GAMES_VIEW_GALLERY,"B");
    assert(!publish(0,KUI_GAMES_VIEW_GALLERY,0x3333) && games_tiles[0][0]==0x1111);
    v=snapshot();assert(!v.game_tile_covers[0] && !games_tile_info[0].published && games_tiles[0]);
    assert(publish(0,KUI_GAMES_VIEW_GALLERY,0x3333) && games_tiles[0][0]==0x3333);
    v=snapshot();assert(v.game_tile_covers[0]==games_tiles[0]);
    /* Another view on screen hides and releases the tile. */
    shell.games_view=shell.games_listing.view=KUI_GAMES_VIEW_COMPACT;
    v=snapshot();assert(!v.game_tile_covers[0] && !games_tile_info[0].published);
    assert(publish(0,KUI_GAMES_VIEW_COMPACT,0x4444) && games_tiles[0][KUI_COVER_SMALL*KUI_COVER_SMALL-1]==0x4444);
    v=snapshot();assert(v.game_tile_covers[0]==games_tiles[0]);
    /* A worker invalidation is only a request: main releases at its snapshot. */
    games_tiles_invalid=true;
    assert(publish(1,KUI_GAMES_VIEW_COMPACT,0x5555) && !games_tile_info[1].published);
    assert(games_tile_info[0].published);
    v=snapshot();assert(!v.game_tile_covers[0] && !games_tile_info[0].published && !games_tiles_invalid);
    /* A prefetch that is no longer current publishes nothing. */
    prefetch_current=false;assert(publish(2,KUI_GAMES_VIEW_COMPACT,0x6666) && !games_tile_info[2].published);
    prefetch_current=true;
    /* Short pages expose only rows that exist. */
    assert(publish(7,KUI_GAMES_VIEW_COMPACT,0x7777));shell.games_listing.count=3;
    v=snapshot();assert(!v.game_tile_covers[7] && !games_tile_info[7].published);
    /* Handoff frees every buffer; later publications allocate again. */
    mutex_lock(&lock);games_tiles_free_locked();mutex_unlock(&lock);
    for(unsigned i=0;i<KUI_GAMES_ROWS;i++) assert(!games_tiles[i] && !games_tile_info[i].published);
    assert(publish(0,KUI_GAMES_VIEW_COMPACT,0x8888) && games_tiles[0]);
    mutex_lock(&lock);games_tiles_free_locked();mutex_unlock(&lock);
    puts("PASS Games tiles: actual helper C, worker writes only unpublished slots, stale-page retry, main-only release/free, view/row/short-page matching, invalidation request");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="kui-games-tiles-") as work:
    source = Path(work) / "tiles.c"
    source.write_text(PROGRAM)
    binary = Path(work) / "tiles"
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", "-fno-pie", "-no-pie",
                    "-I" + str(ROOT / "include"), "-I" + str(ROOT / ".deps/fatfs/source"),
                    "-I" + str(ROOT / "firmware/kui-wifi/components/kwlink/include"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
