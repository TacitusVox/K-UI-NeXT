#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the actual CD boot screen painter from main.c.

draw_boot paints changed frames into a lazily allocated 32-byte-aligned RAM
canvas and copies the whole frame to the back buffer with the store queues
before the unchanged vertical-blank wait and flip. When the canvas cannot be
allocated it paints VRAM directly, as before, and does not retry. Unchanged
frames still paint nothing. The function is extracted verbatim and run under
ASan/UBSan against recording stubs.
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


state_start = SOURCE.index("static uint16_t *boot_canvas;")
state_end = SOURCE.index("static bool boot_canvas_tried;") + len("static bool boot_canvas_tried;")
STATE = SOURCE[state_start:state_end]

PROGRAM = r'''
#include "kui/boot_ui.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KUI_BUILD_ID "012345abcdef"
#define LINE_BYTES 77
#define LOG_LINES 64
static char lines[LOG_LINES][LINE_BYTES];
static unsigned line_count;
static struct kui_boot_ui boot_ui;
static bool busy,cancel_requested,saving_report,boot_worker_available=true;
static char boot_notice[128]="Ready";
static uint64_t boot_last_draw,boot_draw_us;
static unsigned boot_draw_count;
static int lock,locked;
static void mutex_lock(int *m) {assert(m==&lock && !locked);locked=1;}
static void mutex_unlock(int *m) {assert(m==&lock && locked);locked=0;}
static bool kui_storage_boot_from_card(void) {return false;}
static uint64_t now_ms=1000;
static uint64_t timer_ms_gettime64(void) {return now_ms;}
static uint64_t timer_us_gettime64(void) {return now_ms*1000u;}
static uint16_t screen[2][640u*480u];
static uint16_t *vram_s=screen[0];
static char events[64];static unsigned event_count;
static void event(char c) {assert(event_count+1u<sizeof(events));events[event_count++]=c;events[event_count]=0;}
static unsigned allocations;static bool allocation_fails;
static void *test_aligned_alloc(size_t alignment,size_t bytes) {
    assert(alignment==32u && bytes==640u*480u*2u);++allocations;event('M');
    return allocation_fails?NULL:aligned_alloc(alignment,bytes);
}
#define aligned_alloc test_aligned_alloc
static uint16_t *painted;
void kui_boot_ui_draw(uint16_t *frame,const struct kui_boot_ui *ui,const struct kui_boot_view *view) {
    assert(!locked && frame && ui==&boot_ui && view && view->status);
    painted=frame;event('D');
    for(unsigned i=0;i<640u*480u;i++) frame[i]=(uint16_t)(i*7u+view->line_count);
}
static void sq_cpy(void *dest,const void *src,size_t bytes) {
    assert(dest==vram_s && src==painted && !((uintptr_t)src&31u) && bytes==640u*480u*2u);
    memcpy(dest,src,bytes);event('Q');
}
static void sq_wait(void) {event('S');}
static void vid_waitvbl(void) {event('V');}
static void vid_flip(int which) {assert(which==-1);event('F');vram_s=vram_s==screen[0]?screen[1]:screen[0];}
''' + STATE + "\n" + function("draw_boot") + r'''
static void add_line(const char *text) {snprintf(lines[line_count++],LINE_BYTES,"%s",text);}
static void reset_events(void) {event_count=0;events[0]=0;}
int main(void) {
    add_line("Running");
    /* First frame: allocate once, paint RAM, burst-copy, then wait and flip. */
    uint16_t *target=vram_s;
    draw_boot();
    assert(!strcmp(events,"MDQSVF") && allocations==1 && boot_canvas && painted==boot_canvas);
    assert(!memcmp(target,boot_canvas,640u*480u*2u) && boot_draw_count==1 && boot_last_draw==now_ms);
    /* Unchanged: nothing is painted, copied or flipped. */
    reset_events();now_ms+=200;draw_boot();assert(!event_count && boot_draw_count==1);
    /* Changed: the same canvas is reused. */
    add_line("Reading");target=vram_s;draw_boot();
    assert(!strcmp(events,"DQSVF") && allocations==1 && painted==boot_canvas);
    assert(!memcmp(target,boot_canvas,640u*480u*2u) && boot_draw_count==2);
    /* A busy status change from the loader's cancel callback repaints too. */
    reset_events();busy=true;draw_boot();assert(!strcmp(events,"DQSVF") && boot_draw_count==3);
    free(boot_canvas);boot_canvas=NULL;boot_canvas_tried=false;
    /* Without a canvas VRAM is painted directly, as before; no retry. */
    allocation_fails=true;reset_events();add_line("No canvas");target=vram_s;draw_boot();
    assert(!strcmp(events,"MDVF") && painted==target && !boot_canvas && allocations==2);
    reset_events();add_line("Still direct");target=vram_s;draw_boot();
    assert(!strcmp(events,"DVF") && painted==target && allocations==2);
    puts("PASS boot canvas: actual draw_boot C, one aligned RAM canvas, full-frame store-queue copy before the unchanged wait/flip, unchanged frames skipped, direct VRAM fallback without retry");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="kui-boot-canvas-") as work:
    source = Path(work) / "boot_canvas.c"
    source.write_text(PROGRAM)
    binary = Path(work) / "boot_canvas"
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    "-I" + str(ROOT / "include"), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
