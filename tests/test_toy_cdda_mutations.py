#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether Test 84's drive test notices a defect in the player: each of a list,
in a copy of src/loader/toy_cdda_player.c, built and run on its own.

usage: test_toy_cdda_mutations.py [--jobs N] [--work DIR]

Every mutation must still compile and must make toy-cdda-drive-host-test
fail. Left out as equivalent: dropping PLAY2's test of a start below FAD 150,
which the search for its track refuses as well; and in KEEP, dropping the test
of the command (only a read's token is the engine's), of the sector size (the
service admits 2,048 and 2,352 bytes alone) and the bound to the request's
sectors (the engine's cursor never passes it); and of Test 89's hold, dropping
the visit that brings a play up to now before it is held (the ticks kept are
the base's behind now either way, and the output looks at where the play
stands itself), and dropping the test that something plays (a hold of
nothing does not come back from the visit: the test would not end). Run from
the repository root
(Makefile.vmu-native-toy-suite, toy-cdda-mutation-check); the recipe's switches
are this file's own.
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAYER = ROOT / "src/loader/toy_cdda_player.c"
RECIPE = ("TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 "
          "TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 "
          "TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 "
          "TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1").split()
# Test 89's hold is the sound output's: its mutations (a fourth element) are
# built with the output's switches as delivered.
OUTPUT = ("TOY_CDDA_SURVEY=0 TOY_CDDA_CARD=1 TOY_CDDA_YIELD=1 TOY_CDDA_OUTPUT=1 TOY_CDDA_VOLUME=1 TOY_CDDA_ADPCM=1 "
          "TOY_CDDA_SPARE=1").split()
MUTATIONS = [
    ("status 1 while playing", "s->drive_status=D.state==KUI_TOY_CDDA_PLAYING?3u:1u;",
     "s->drive_status=D.state==KUI_TOY_CDDA_PLAYING?1u:1u;"),
    ("no status written", "s->drive_status=D.state==KUI_TOY_CDDA_PLAYING?3u:1u;", "(void)0;"),
    ("no position projected", "        s->position_lba=play_sector();\n", "        (void)play_sector();\n"),
    ("projection not brought up to date",
     "            ++D.projections;\n        }\n        s->position_lba=play_sector();",
     "            ++D.projections;\n            s->position_lba=play_sector();\n        }"),
    ("data position not restored", "s->position_lba=D.saved_position;D.projected=0u;", "D.projected=0u;"),
    ("restored while still pending", "(s->token!=D.projected_token || !s->pending)", "(s->token!=D.projected_token || 1)"),
    ("the catalogue projected too", "s->outputs[0]<=1u", "s->outputs[0]<=2u"),
    ("one pass too many", "(repeat&15u)+1u", "(repeat&15u)+2u"),
    ("repeat 15 not endless", "(repeat&15u)==15u", "(repeat&15u)==14u"),
    ("the end a pass late", "if(D.passes && D.done>=D.passes)", "if(D.passes && D.done>D.passes)"),
    ("two sectors a group", "uint32_t step=groups*3u;", "uint32_t step=groups*2u;"),
    ("a group's part left out", "    offset+=(int32_t)(ticks*3u/KUI_TOY_CDDA_TICKS_3_SECTORS);", "    offset+=0;"),
    ("a lap within a group counted twice", "offset-=length;D.base_offset-=length;", "offset-=length;"),
    ("PAUSE ignored", "if(D.state==KUI_TOY_CDDA_PLAYING)D.state=KUI_TOY_CDDA_PAUSED;",
     "if(0)D.state=KUI_TOY_CDDA_PAUSED;"),
    ("RELEASE from the start", "D.base_offset=(int32_t)D.frozen;", "D.base_offset=0;"),
    ("a data track plays", "return t && !kui_retail_track_control(t) && t->start_lba<t->end_lba;",
     "return t && t->start_lba<t->end_lba;"),
    ("PLAY2 in a gap refused", "(lba<t->start_lba && !audio(track_at(s,n-1u)))", "(lba<t->start_lba)"),
    ("a range through data", "while(n<limit && audio(track_at(s,n+1u)))++n;", "while(n<limit && track_at(s,n+1u))++n;"),
    ("PLAY of a data track", "if(!audio(t) || b<a)return false;", "if(!t || b<a)return false;"),
    ("SEEK keeps the play", "case KUI_GD_STOP: case KUI_RETAIL_GD_SEEK: case KUI_GD_COMMAND_INIT:",
     "case KUI_GD_STOP: case KUI_GD_COMMAND_INIT:"),
    ("an ended play leaves its 3", "    s->drive_status=1u;D.state=KUI_TOY_CDDA_IDLE;",
     "    s->drive_status=s->drive_status;D.state=KUI_TOY_CDDA_IDLE;"),
    ("RESET ignored", "if((s->status==KUI_GD_NOT_FOUND && !s->command && !s->pending) ||",
     "if((0 && !s->command && !s->pending) ||"),
    ("ABORT ignored", "(s->error==KUI_GD_ERROR_CANCELLED && s->token==D.play_token &&",
     "(0 && s->token==D.play_token &&"),
    ("interrupt visits answer", "if(event!=KUI_NATIVE_RA_DELIVER || r->engine.in_irq)return 0u;",
     "if(event!=KUI_NATIVE_RA_DELIVER)return 0u;"),
    ("a command seen again", "        D.seen_token=s->token;\n", "\n"),
    ("the clock always trusted", "    return kui_toy_pilot_clock_profile(frqcr,tcor,tcr,tstr);", "    (void)frqcr;(void)tcor;(void)tcr;(void)tstr;return true;"),
    ("a refused play keeps the last one", "    ++D.refused;\n    stop(s);", "    ++D.refused;"),
    ("written with no play", "    if(D.state==KUI_TOY_CDDA_IDLE)return 0u;\n    if(D.state==KUI_TOY_CDDA_PLAYING)advance();",
     "    if(D.state==KUI_TOY_CDDA_PLAYING)advance();"),
    ("a lost play still plays", "        D.state=KUI_TOY_CDDA_LOST;++D.lost;++D.generation;return;\n    }\n    const int32_t length",
     "        ++D.lost;return;\n    }\n    const int32_t length"),
    # KEEP
    ("KEEP keeps nothing", "        if(!r->engine.in_irq)keep(s,r);", "        if(0)keep(s,r);"),
    ("KEEP writes 1", "s->position_lba=s->lba+sectors-1u;s->drive_status=3u;",
     "s->position_lba=s->lba+sectors-1u;s->drive_status=1u;"),
    ("KEEP in an interrupt's visit", "        if(!r->engine.in_irq)keep(s,r);", "        keep(s,r);"),
    ("KEEP while paused or ended", "    if(D.state!=KUI_TOY_CDDA_PLAYING || !s->pending ||",
     "    if(D.state==KUI_TOY_CDDA_IDLE || !s->pending ||"),
    ("KEEP of a failed read", " || r->engine.failed ||\n", " ||\n"),
    ("KEEP of another read's engine", "r->engine.token!=s->token || ", ""),
    ("KEEP's raw sectors as 2,048 bytes", "return sector_bytes==2048u?bytes>>11:(bytes>>4)/147u;", "return sector_bytes?bytes>>11:0u;"),
    ("KEEP's position one on", "s->position_lba=s->lba+sectors-1u;s->drive_status=3u;",
     "s->position_lba=s->lba+sectors;s->drive_status=3u;"),
    ("KEEP's sectors not counted", "    s->diag.sectors_read+=sectors-done;++s->diag.read_steps;\n",
     "    ++s->diag.read_steps;\n"),
    ("KEEP's step not counted", "    s->diag.sectors_read+=sectors-done;++s->diag.read_steps;\n",
     "    s->diag.sectors_read+=sectors-done;\n"),
    ("KEEP publishes no new sector", "    if(sectors<=done)return;\n", "    if(sectors<done)return;\n"),
    ("KEEP's bytes of one sector less", "    s->completed_bytes=sectors*s->sector_bytes;\n",
     "    s->completed_bytes=(sectors-1u)*s->sector_bytes;\n"),
    ("a held play not standing", "        } else D.base_counter=now+D.held;\n", "        }\n", OUTPUT),
    ("a hold with no end", "        if(D.held_at-now>KUI_TOY_CDDA_HOLD_TICKS) {", "        if(0) {", OUTPUT),
    ("a hold over at the visit that finds it, not at the second's end",
     "D.base_counter=D.held_at-KUI_TOY_CDDA_HOLD_TICKS+D.held;", "D.base_counter=now+D.held;", OUTPUT),
    ("a group whole between the hold's two reads of the clock taken at every visit", "    if(D.hold)D.held=ticks;\n", "",
     OUTPUT),
    ("a play let go not brought up to now", "        if(D.hold)advance();\n", "", OUTPUT),
    ("a ring given up ends the second", "        if(hold==KUI_TOY_CDDA_SOUNDS)D.held_open=0u;", "        D.held_open=0u;",
     OUTPUT),
    ("a ring keyed on does not end the second", "        if(hold==KUI_TOY_CDDA_SOUNDS)D.held_open=0u;", "        (void)0;",
     OUTPUT),
    ("a play whose second is spent held again", "    } else if(D.held_at-now>KUI_TOY_CDDA_HOLD_TICKS)return;", "    }",
     OUTPUT),
    ("a second begun at every hold", "    if(!D.held_open) {", "    if(1) {", OUTPUT),
    ("the ticks of a held play's sector lost", "    D.held=D.base_counter-now;\n    D.hold=1u;",
     "    D.held=0u;\n    D.hold=1u;", OUTPUT),
    ("a second hold taken over the first", "    if(D.hold)return;\n    advance();", "    advance();", OUTPUT),
    ("a new play keeps a hold", "\n    D.hold=D.held_open=0u;", "\n    D.held_open=0u;", OUTPUT),
    ("a new play keeps a spent second", "\n    D.hold=D.held_open=0u;", "\n    D.hold=0u;", OUTPUT),
    ("a release keeps a hold", "\n                D.hold=D.held_open=0u;", "\n                D.held_open=0u;", OUTPUT),
    ("a release keeps a spent second", "\n                D.hold=D.held_open=0u;", "\n                D.hold=0u;", OUTPUT),
]


def run(index, work):
    name, old, new = MUTATIONS[index][:3]
    recipe = RECIPE + (OUTPUT if len(MUTATIONS[index]) > 3 else [])
    text = PLAYER.read_text()
    if text.count(old) != 1:
        return index, "not applied (%d matches)" % text.count(old)
    directory = Path(work) / ("m%02d" % index)
    directory.mkdir(parents=True, exist_ok=True)
    mutated = directory / "toy_cdda_player.c"
    mutated.write_text(text.replace(old, new))
    env = dict(os.environ)
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)
    result = subprocess.run(["make", "-f", "Makefile.vmu-native-toy-suite", *recipe,
                             "HOST_CC=" + env.get("HOST_CC", "cc"), "BUILD_ID=000000000000",
                             "BUILD=" + str(directory / "build"), "TOY_CDDA_PLAYER_SRC=" + str(mutated),
                             "toy-cdda-drive-host-test"], cwd=ROOT, env=env, capture_output=True, text=True)
    log = result.stdout + result.stderr
    (directory / "log.txt").write_text(log)
    if result.returncode == 0:
        return index, "SURVIVED"
    if "test_toy_cdda_drive.c:" in log or "AddressSanitizer" in log or "runtime error" in log:
        return index, "caught"
    return index, "did not build"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--work")
    args = parser.parse_args()
    work = args.work or tempfile.mkdtemp(prefix="toy-cdda-mutations-")
    print("mutations in", work)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = sorted(pool.map(lambda i: run(i, work), range(len(MUTATIONS))))
    bad = [(i, r) for i, r in results if r != "caught"]
    for i, r in results:
        print("%-9s %s" % (r, MUTATIONS[i][0]))
    if bad:
        sys.exit("FAIL %d of %d mutations not caught: %s" % (len(bad), len(MUTATIONS),
                 "; ".join("%s (%s)" % (MUTATIONS[i][0], r) for i, r in bad)))
    print("PASS Test 84 drive mutations: all %d caught by the drive test" % len(MUTATIONS))


if __name__ == "__main__":
    main()
