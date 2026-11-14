#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that the Test 68, 69 and 70 host tests notice deliberate defects.

Each mutation changes one behaviour of the card stream, its ring reader, or
the glue that feeds the verified movie window: a position, a lost-byte
rule, a register that must be given back, the CRC, the flow control, the
re-read of a failed sector. Stream mutants are built against
tests/test_toy_wire_stream.c; glue mutants against the reader integration
test (tests/test_toy_wire_integration.c) through its make target. A
mutation that no test fails is a gap in the tests. This checks the tests,
not the Dreamcast.

The tests are built as the delivered reader is: Test 70's switch
(TOY_WIRE_CACHED=1) unless the environment says otherwise. TOY_WIRE_TICK=1
checks Test 69's recipe with the mutations of its timer; both at 0 check
Test 68's and leave out the mutations of the report's measurements. Test
70's own change is in the visit gate's assembly, which no host test can
run: tests/test_toy_wire_cached_stack.py reads the linked module for it.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
STREAM = "src/loader/toy_wire_stream.c"
GLUE = "src/loader/toy_wire_ahead.inc"
AHEAD = "src/loader/toy_movie_ahead.inc"
TICK = os.environ.get("TOY_WIRE_TICK", "0")
CACHED = os.environ.get("TOY_WIRE_CACHED", "0" if TICK == "1" else "1")
WORDS = os.environ.get("TOY_WIRE_WORDS", CACHED)
SWITCHES = [f"TOY_WIRE_TICK={TICK}", f"TOY_WIRE_CACHED={CACHED}", f"TOY_WIRE_WORDS={WORDS}"]
DEFINES = [f"-DKUI_TOY_WIRE_TICK={TICK}", f"-DKUI_TOY_WIRE_CACHED={CACHED}", f"-DKUI_TOY_WIRE_WORDS={WORDS}"]
MUTATIONS = [
 (STREAM, "resume one byte early", "    w->head = lost + 1u;\n", "    w->head = lost;\n"),
 (STREAM, "resume one byte late", "    w->head = lost + 1u;\n", "    w->head = lost + 2u;\n"),
 (STREAM, "held byte not stored", "    if(held) w->ring[at & MASK] = tail;\n", "    (void)tail;\n"),
 (STREAM, "held byte taken for the lost one", "    const uint32_t lost = at + held;\n", "    const uint32_t lost = at;\n"),
 (STREAM, "lost position not logged", "    w->hole[w->hole_put++ & HOLE_MASK] = lost;\n", "    (void)lost;\n"),
 (STREAM, "no module reset", "        wr8(STBCR, before | 1u);", "        (void)before;"),
 (STREAM, "run started without room for it", "    if(room < RUN_ROOM || w->hole_put", "    if(room < 2u || w->hole_put"),
 (STREAM, "short run for a waiting caller overshoots the ring", "        if(most < length) length = most; /* never beyond the ring's end */", "        length = most;"),
 (STREAM, "ring index ignores the wrap", "    wr32(DAR1, w->ring_physical + index);", "    wr32(DAR1, w->ring_physical + w->head);"),
 (STREAM, "channel not restored at stop", "    if(usable && state != KUI_WIRE_FAULT)\n        for(unsigned i = 0; i < 4u; ++i) wr32(SAR1 + 4u * i, w->saved_dma[i]);", "    (void)usable;"),
 (STREAM, "stop does not wait for a whole byte", "        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}\n        uint32_t count, status;", "        uint32_t count, status;"),
 (STREAM, "wrong CRC polynomial", "c >> 1 ^ 0x8408u", "c >> 1 ^ 0x8404u"),
 (STREAM, "clean block not checked", "        else if(!lost_data && !lost_crc) accept = !crc;", "        else if(!lost_data && !lost_crc) accept = true;"),
 (STREAM, "rebuild counts one byte too few behind", "wire_solve(w, crc, FRAME - 1u - w->lost[0])", "wire_solve(w, crc, FRAME - 2u - w->lost[0])"),
 (STREAM, "rebuilt byte not reversed", "                w->block[w->lost[0]] = w->reverse[value];", "                w->block[w->lost[0]] = (uint8_t)value;"),
 (STREAM, "lost data byte not listed", "                w->lost[(*lost_data)++] = (uint16_t)(at - first);\n", "                ++*lost_data;\n"),
 (STREAM, "lost index off by one", "                w->lost[(*lost_data)++] = (uint16_t)(at - first);", "                w->lost[(*lost_data)++] = (uint16_t)(at - first + 1u);"),
 (STREAM, "lost gap byte never the token", "                w->data_at = w->gap_hole;\n                w->alternative = 0;", "                return wire_refuse(w, KUI_WIRE_LOST_FRAME);"),
 (STREAM, "second reading of the gap dropped", "                w->alternative = w->gap_hole;", "                w->alternative = 0;"),
 (STREAM, "reader runs past what has landed", "        if(landed < need) return KUI_WIRE_NEED;", "        if(landed + 64u < need) return KUI_WIRE_NEED;"),
 (STREAM, "data token taken as received", "#define TOKEN_WIRE 0x7fu", "#define TOKEN_WIRE 0xfeu"),
 (STREAM, "block one byte short", "#define FRAME 514u", "#define FRAME 513u"),
 (GLUE, "lost bytes not mapped into the sector", "        if(k->lost_count<=KUI_MODE1_ERASURE_MAX)k->lost[k->lost_count]=(uint16_t)(part+at-from);", "        if(k->lost_count<=KUI_MODE1_ERASURE_MAX)k->lost[k->lost_count]=(uint16_t)(at-from);"),
 (GLUE, "failed sector not read again", "            toy_wire_rewind(r,KUI_TOY_WIRE_STOP_SECTOR);\n            return false;", "            k->bad=0u;++A.verified;\n            return false;"),
 (GLUE, "extent jump ignored", "        if(A.cursor.block!=next) {\n            toy_wire_halt(r,KUI_TOY_WIRE_STOP_JUMP);\n            return false;\n        }", ""),
 (GLUE, "stream state not handed back", "    stream->state=RA_LOST;\n    stream->ready[0]", "    stream->state=RA_PAUSED;\n    stream->ready[0]"),
 (GLUE, "resident not told the stream receives", "    stream->state=RA_DMA;\n    k->on=1u;", "    k->on=1u;"),
 (GLUE, "rewind keeps the old cursor", "       kui_retail_cursor_begin(&A.cursor,A.cursor.manifest,A.verified,end-A.verified,\n           KUI_GAME_SECTOR_RAW,toy_ahead_write,NULL)!=KUI_GAME_OK)A.error=true;", "       false)A.error=true;"),
 (AHEAD, "sector served without its check", "    if(toy_wire_sector(k,raw,lba))++A.verified;\n    else k->bad=1u;", "    (void)toy_wire_sector;++A.verified;"),
 (AHEAD, "stream left running when the window closes", "    toy_wire_halt(r,KUI_TOY_WIRE_STOP_CLOSE);\n#endif\n    if(A.receiving) {", "#endif\n    if(A.receiving) {"),
]
# What Tests 69 and 70 share: the report's account of the reader's time.
MEASURE_MUTATIONS = [
 (STREAM, "note taken of the byte behind", "    struct kui_wire_counters *n = w->n;\n    const uint8_t value = w->ring[at & MASK];", "    struct kui_wire_counters *n = w->n;\n    const uint8_t value = w->ring[(at + 1u) & MASK];"),
 (STREAM, "note's gap not counted from the block's end", "        w->gap_begin = end;\n", ""),
 (GLUE, "a visit out of time is not told apart", "            } else if(start.counter-clock_read().counter>=ticks) {more=true;break;}", "            } else if(start.counter-clock_read().counter>=ticks) {break;}"),
 (GLUE, "waiting requests not counted", "    ++k->report.wait_calls[movie];\n", ""),
 (GLUE, "restart time not kept", "            k->report.restart_ticks+=spent;\n", ""),
 (GLUE, "visits not counted by caller", "    ++k->report.visit_calls[who];\n", ""),
]
# What only Test 69 has: reading continued from the stream's timer.
TICK_MUTATIONS = [
 (STREAM, "timer call arranged over a running wait", "    if(!w->mode || (w->state != KUI_WIRE_RUN && w->state != KUI_WIRE_FULL)) return false;", "    if(!w->mode || w->state == KUI_WIRE_OFF) return false;"),
 (STREAM, "timer call counted but not arranged", "    timer_arm(w, ticks);\n    ++w->n->ticks;", "    ++w->n->ticks;"),
 (GLUE, "no timer call when a visit runs out of time", "    (void)kui_wire_tick(toy_wire_stream(),TOY_WIRE_TICK_GAP);", ""),
 (GLUE, "timer visit does not read", "        toy_wire_read(r,TOY_WIRE_TICKS_INTERRUPT);\n", ""),
 (GLUE, "timer visit not accounted", "    }\n    toy_wire_visit_time(k,KUI_TOY_WIRE_VISIT_TIMER,entered);", "    }"),
]
# What only Test 70's report has.
CACHED_MUTATIONS = [
 (GLUE, "timer visit not accounted", "    kui_wire_timer(toy_wire_stream());\n    toy_wire_visit_time(k,KUI_TOY_WIRE_VISIT_TIMER,entered);", "    kui_wire_timer(toy_wire_stream());"),
 (GLUE, "visits out of time not counted", "           k->on)++k->report.budget_ends;", "           k->on) {}"),
]
# What only Test 71 has: the block loop a word at a time, and the report's
# course of the second movie and count of positions that were not lost bytes.
WORDS_MUTATIONS = [
 (STREAM, "two-byte CRC table built from the wrong byte", "        w->crc2[i] = (uint16_t)(c >> 8 ^ w->crc[c & 0xffu]);", "        w->crc2[i] = (uint16_t)(c >> 8 ^ w->crc[c >> 8]);"),
 (STREAM, "second pair of a word not shifted down", "        t = (uint32_t)c ^ x >> 16;", "        t = (uint32_t)c ^ x >> 8;"),
 (STREAM, "table halves swapped", "        c = two[t & 0xffu] ^ one[t >> 8 & 0xffu];\n        t = (uint32_t)c ^ x >> 16;", "        c = one[t & 0xffu] ^ two[t >> 8 & 0xffu];\n        t = (uint32_t)c ^ x >> 16;"),
 (STREAM, "third byte of a word reversed from the second", "        out[2] = reverse[x >> 16 & 0xffu];", "        out[2] = reverse[x >> 8 & 0xffu];"),
 (STREAM, "last byte of a word not written", "        out[3] = reverse[x >> 24];\n", ""),
 (STREAM, "sign bits of the table left in the CRC", "    return (uint32_t)c & 0xffffu;", "    return (uint32_t)c;"),
 (STREAM, "bytes behind a word run dropped", "            p += 4u * words; out += 4u * words; piece &= 3u;", "            p += 4u * words; out += 4u * words; piece = 0u;"),
 (STREAM, "register byte not marked in the log", "        if(held) w->held[entry >> 5] |= UINT32_C(1) << (entry & 31u);", "        if(!held) w->held[entry >> 5] |= UINT32_C(1) << (entry & 31u);"),
 (STREAM, "repeated byte looked for one position off", "            if(w->ring[(position - 1u) & MASK] == w->ring[(position - 2u) & MASK]) twice = true;", "            if(w->ring[(position - 2u) & MASK] == w->ring[(position - 3u) & MASK]) twice = true;"),
 (GLUE, "course pieces not counted from the movie's start", "    const uint32_t piece=(toy_present.start_tick-now)>>KUI_TOY_WIRE_BUCKET_SHIFT;", "    const uint32_t piece=now>>KUI_TOY_WIRE_BUCKET_SHIFT;"),
 (GLUE, "course not held to its last piece", "    return piece<KUI_TOY_WIRE_BUCKETS?(int)piece:(int)KUI_TOY_WIRE_BUCKETS-1;", "    return piece<KUI_TOY_WIRE_BUCKETS?(int)piece:-1;"),
 (GLUE, "interrupt and game time swapped in the course", "(who==KUI_TOY_WIRE_VISIT_GAME?k->report.course_game:k->report.course_interrupt)", "(who!=KUI_TOY_WIRE_VISIT_GAME?k->report.course_game:k->report.course_interrupt)"),
 (GLUE, "restarts missing from the course", "        if(piece>=0)++k->report.course_stops[piece];\n", ""),
]
if WORDS == "1":
    MUTATIONS += WORDS_MUTATIONS
if TICK == "1" or CACHED == "1":
    MUTATIONS += MEASURE_MUTATIONS
if TICK == "1":
    MUTATIONS += TICK_MUTATIONS
if CACHED == "1":
    MUTATIONS += CACHED_MUTATIONS


def run(cmd, cwd, timeout):
    try:
        return subprocess.run(cmd, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 124  # a mutant that hangs is caught too


def main():
    cc = os.environ.get("CC", "cc")
    survived = []
    with TemporaryDirectory(prefix="kui-wire-mutants-") as tmp:
        tree = Path(tmp) / "tree"
        # A private copy of what the tests build from; the working tree is never touched.
        for part in ("src", "include", "tests", "fatfs"):
            shutil.copytree(ROOT / part, tree / part, symlinks=True)
        for makefile in ROOT.glob("Makefile*"):
            shutil.copy(makefile, tree / makefile.name)
        for path, name, old, new in MUTATIONS:
            original = (ROOT / path).read_text()
            if original.count(old) != 1:
                print(f"STALE   {name}: its source text occurs {original.count(old)} times", flush=True)
                survived.append(name)
                continue
            (tree / path).write_text(original.replace(old, new))
            if path == STREAM:
                binary = Path(tmp) / "stream"
                code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                            "-DKUI_WIRE_STREAM_TEST=1", *DEFINES, "-Iinclude", "-Isrc/loader",
                            "tests/test_toy_wire_stream.c", path, "-o", str(binary)], tree, 120)
                if code == 0:
                    code = run([str(binary), "8"], tree, 300)
                if code == 0:  # the stream mutant may still be caught end to end
                    code = run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                                "TOY_WIRE_STREAM=1", *SWITCHES, f"BUILD={tmp}/build",
                                "toy-wire-integration-host-test"], tree, 900)
            else:
                code = run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                            "TOY_WIRE_STREAM=1", *SWITCHES, f"BUILD={tmp}/build",
                            "toy-wire-integration-host-test"], tree, 900)
            (tree / path).write_text(original)
            print(("caught  " if code else "MISSED  ") + name, flush=True)
            if not code:
                survived.append(name)
    print(f"{len(MUTATIONS) - len(survived)} of {len(MUTATIONS)} mutations caught")
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())
