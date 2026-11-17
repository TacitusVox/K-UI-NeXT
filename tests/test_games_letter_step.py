#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the actual Games L/R letter-step worker helpers from main.c.

games_letter_group/games_letter_step are extracted verbatim and run against a
model catalogue served eight rows at a time, with the real '#'/A-Z bucket from
games_query.c, under ASan/UBSan. Forward finds the first later row in another
group; backward finds the start of the current group, or of the previous group
when the row already starts one; both ends report no step.
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
#include "kui/games_library.h"
#include "kui/games_covers.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void kui_log(const char *format,...) {(void)format;}
static const char *rows[64];
static unsigned row_count,queries,fail_at=UINT_MAX;
static const struct kui_games_query *expected_query;
bool kui_games_library_rows_query(const char *root,const char *group,unsigned offset,unsigned view,
        const struct kui_games_query *query,struct kui_games_page *out,kui_log_fn log,kui_cancel_fn cancel) {
    (void)log;(void)cancel;
    assert(!strcmp(root,"/Games") && !group[0] && view==KUI_GAMES_VIEW_SAVED && query==expected_query);
    assert(offset%KUI_GAMES_ROWS==0);
    ++queries;
    if(offset>=fail_at) return false;
    memset(out,0x5a,sizeof(*out));
    out->count=0;
    for(unsigned i=0;i<KUI_GAMES_ROWS && offset+i<row_count;i++) {
        struct kui_games_entry *e=&out->entries[i];
        memset(e,0,sizeof(*e));
        /* A leading '=' models a row with an edited title over its folder name. */
        if(rows[offset+i][0]=='=') {
            snprintf(e->title,sizeof(e->title),"%s",rows[offset+i]+1);
            snprintf(e->name,sizeof(e->name),"zz folder %u",offset+i);
            e->custom_title=true;
        } else snprintf(e->name,sizeof(e->name),"%s",rows[offset+i]);
        ++out->count;
    }
    out->has_more=offset+KUI_GAMES_ROWS<row_count;
    return true;
}
static bool never(void) {return false;}
''' + function("games_letter_group") + "\n" + function("games_letter_step") + r'''
static struct kui_games_query query;
static void catalogue(const char *const *names,unsigned count) {
    assert(count<=64);
    for(unsigned i=0;i<count;i++) rows[i]=names[i];
    row_count=count;fail_at=UINT_MAX;
}
static int step(unsigned from,bool forward) {
    unsigned out=UINT_MAX;queries=0;
    bool found=games_letter_step("/Games","",&query,from,forward,&out,never);
    assert(found==(out!=UINT_MAX));
    return found?(int)out:-1;
}
int main(void) {
    expected_query=&query;
    /* Groups: '#'(0-1) A(2-4) B(5) D(6-9) | page two: D(8-9 cont.) G(10-12) M(13) S(14-17) */
    static const char *const names[]={
        "007 Racing","4 Wheel Thunder","Aero Wings","alien front","Armada","Bangai-O",
        "Dead or Alive 2","dino crisis","Draconus","Dynamite Cop","Giant Gram","Grandia II","  gunbird 2",
        "MDK2","San Francisco Rush","Shenmue","Skies of Arcadia","Soul Calibur"};
    catalogue(names,sizeof(names)/sizeof(names[0]));
    /* Forward: next group start from anywhere in a group, across pages. */
    assert(step(0,true)==2 && step(1,true)==2 && step(2,true)==5 && step(4,true)==5);
    assert(step(5,true)==6 && step(6,true)==10 && step(7,true)==10 && step(9,true)==10);
    assert(step(10,true)==13 && step(13,true)==14);
    /* Leading spaces are skipped exactly as in the letter picker ("  gunbird 2" is G). */
    assert(step(12,true)==13);
    /* The last group has no next group. */
    assert(step(14,true)==-1 && step(17,true)==-1);
    /* Backward: start of this group, or the previous group's start. */
    assert(step(17,false)==14 && step(14,false)==13 && step(13,false)==10);
    assert(step(12,false)==10 && step(10,false)==6 && step(9,false)==6 && step(8,false)==6);
    assert(step(6,false)==5 && step(5,false)==2 && step(3,false)==2 && step(2,false)==0);
    assert(step(1,false)==0 && step(0,false)==-1);
    /* Backward stops reading once the origin row is reached. */
    assert(step(3,false)==2 && queries==1);
    /* Forward starts at the origin row's page. */
    assert(step(9,true)==10 && queries==1);assert(step(13,true)==14 && queries==1);
    assert(step(7,true)==10 && queries==2);
    /* A row outside the catalogue never steps. */
    assert(step(18,true)==-1 && step(40,false)==-1);
    /* A listing failure (e.g. cancellation) reports no step. */
    fail_at=8;assert(step(6,true)==-1 && step(12,false)==-1);fail_at=UINT_MAX;
    /* Edited titles group by the shown title, and a group recurring later is
     * still another run: steps follow list order, never wrap. */
    static const char *const mixed[]={"Alpha","=Beta edit","Bravo","Charlie","=Avalon","Bonus"};
    catalogue(mixed,sizeof(mixed)/sizeof(mixed[0]));
    assert(step(0,true)==1 && step(1,true)==3 && step(3,true)==4 && step(4,true)==5 && step(5,true)==-1);
    assert(step(5,false)==4 && step(4,false)==3 && step(3,false)==1 && step(2,false)==1 && step(1,false)==0);
    /* A single group has no step either way. */
    static const char *const one[]={"Alpha","Armada","Avalon"};
    catalogue(one,3);
    assert(step(0,true)==-1 && step(2,true)==-1 && step(0,false)==-1 && step(2,false)==0);
    puts("PASS Games letter steps: actual worker C, list-order groups across pages, picker letter buckets, both ends, failure and early stop");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="kui-games-letter-") as work:
    source = Path(work) / "letters.c"
    source.write_text(PROGRAM)
    binary = Path(work) / "letters"
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    "-I" + str(ROOT / "include"), "-I" + str(ROOT / ".deps/fatfs/source"),
                    "-I" + str(ROOT / "firmware/kui-wifi/components/kwlink/include"),
                    str(source), str(ROOT / "src/core/games_query.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
