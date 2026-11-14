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
PACE = os.environ.get("TOY_WIRE_PACE", WORDS)
IDLE = os.environ.get("TOY_WIRE_IDLE", PACE)
LEAN = os.environ.get("TOY_WIRE_LEAN", IDLE)
AUX = "src/loader/native_readahead_aux.c"
SWITCHES = [f"TOY_WIRE_TICK={TICK}", f"TOY_WIRE_CACHED={CACHED}", f"TOY_WIRE_WORDS={WORDS}", f"TOY_WIRE_PACE={PACE}",
            f"TOY_WIRE_IDLE={IDLE}", f"TOY_WIRE_LEAN={LEAN}"]
DEFINES = [f"-DKUI_TOY_WIRE_TICK={TICK}", f"-DKUI_TOY_WIRE_CACHED={CACHED}", f"-DKUI_TOY_WIRE_WORDS={WORDS}",
           f"-DKUI_TOY_WIRE_PACE={PACE}", f"-DKUI_TOY_WIRE_IDLE={IDLE}", f"-DKUI_TOY_WIRE_LEAN={LEAN}"]
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
# Test 72 leaves the notes on unexpected bytes out of the build: their
# mutations go with them.
NOTES = {"note taken of the byte behind", "note's gap not counted from the block's end",
         "register byte not marked in the log", "repeated byte looked for one position off"}
# Tests 72 and 73: the player's update calls held back. The stream's side
# and the pump's are the same source for both.
PACE_MUTATIONS = [
 (GLUE, "the clock is never taken as good", "        k->clock_good=entered.valid;", "        k->clock_good=0u;"),
 (STREAM, "an unanswered overrun is not overdue", "    if(w->state == KUI_WIRE_RUN) return (rd8(SSR) & ORER) != 0u;", "    if(w->state == KUI_WIRE_RUN) return false;"),
 (STREAM, "a wait whose timer ran out is not overdue", "    if(w->state == KUI_WIRE_WAIT) return !w->mode || (rd16(w->timer_reg + 8u) & 0x0100u) != 0u;", "    if(w->state == KUI_WIRE_WAIT) return !w->mode;"),
 (STREAM, "a wait without a timer is not overdue", "    if(w->state == KUI_WIRE_WAIT) return !w->mode || (rd16(w->timer_reg + 8u) & 0x0100u) != 0u;", "    if(w->state == KUI_WIRE_WAIT) return w->mode && (rd16(w->timer_reg + 8u) & 0x0100u) != 0u;"),
]
# Test 72's own rule for the update calls.
PACE_ONLY_MUTATIONS = [
 (GLUE, "update calls never held back", "    if(!A.error && k->clock_good && k->update_tick-now<TOY_WIRE_UPDATE_TICKS)return false;", "    if(false)return false;"),
 (GLUE, "update calls visit without a window", "    if(!A.variant)return false;", "    if(!A.variant)return true;"),
 (GLUE, "a ring that stands full does not call for the foreground", "    if((toy_wire_room() && (!k->on || toy_wire_stream()->state==KUI_WIRE_FULL)) ||", "    if((toy_wire_room() && !k->on) ||"),
 (GLUE, "an overrun left unanswered does not call for the foreground", "       (k->on && kui_wire_overdue(toy_wire_stream()))) {", "       false) {"),
 (GLUE, "update calls not counted", "    ++k->report.updates;\n", ""),
 (GLUE, "due calls not counted", "    ++k->report.update_due;\n", ""),
]
# Test 73: the same rule for the update calls and for the game's GD calls
# that carry no request; and what the report takes from the player itself.
IDLE_MUTATIONS = [
 (GLUE, "calls never held back", "    if(!A.error && k->clock_good && k->update_tick-tick<TOY_WIRE_UPDATE_TICKS)return false;", "    if(false)return false;"),
 (GLUE, "update calls visit without a window", "    if(A.variant==0u)return false;", "    if(A.variant==0u)return true;"),
 (GLUE, "a ring that stands full does not call for the foreground", "    if((toy_wire_room() && (k->on==0u || toy_wire_stream()->state==KUI_WIRE_FULL)) ||", "    if((toy_wire_room() && k->on==0u) ||"),
 (GLUE, "an overrun left unanswered does not call for the foreground", "       (k->on!=0u && kui_wire_overdue(toy_wire_stream()))) {", "       false) {"),
 (GLUE, "calls not counted", "    ++*(idle?&w->idles:&w->updates);\n", ""),
 (GLUE, "due calls not counted", "    ++*(idle?&w->idle_due:&w->update_due);\n", ""),
 (GLUE, "idle calls counted with the update calls", "    ++*(idle?&w->idles:&w->updates);", "    ++w->updates;"),
 (GLUE, "idle work counted with the update calls'", "        ++*(idle?&w->idle_work:&w->update_work);", "        ++w->update_work;"),
 (GLUE, "idle due calls counted with the update calls'", "    ++*(idle?&w->idle_due:&w->update_due);", "    ++w->update_due;"),
 (AHEAD, "the game's idle calls always pump", "    if(demand || e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())", "    if(true)"),
 (AHEAD, "a waiting request held to the spacing", "    if(demand || e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())", "    if(e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())"),
 (AHEAD, "an interrupt visit held to the spacing", "    if(demand || e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())", "    if(demand || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())"),
 (AHEAD, "an update call passed on is held again", "    if(demand || e->in_irq || C.current_event!=KUI_NATIVE_RA_DELIVER || toy_wire_idle_due())", "    if(demand || e->in_irq || toy_wire_idle_due())"),
 (AUX, "an update visit not told from a delivery", "            C.current_event=event;\n#endif\n            (void)toy_movie_ahead(r,0u,false,0u);", "#endif\n            (void)toy_movie_ahead(r,0u,false,0u);"),
 (AUX, "an interrupt's delivery visit counted with the game's", "    if(event==KUI_NATIVE_RA_DELIVER && !(visit_context>>24) && before.valid && after.valid) {", "    if(event==KUI_NATIVE_RA_DELIVER && before.valid && after.valid) {"),
 (AUX, "delivery visits not counted", "        ++w->deliver_calls;\n", ""),
 (GLUE, "the output counter read at the skip counter's place", "enum { TOY_PLAYER_OUT=0xacu, TOY_PLAYER_SKIP=0x90u };", "enum { TOY_PLAYER_OUT=0x90u, TOY_PLAYER_SKIP=0x90u };"),
 (GLUE, "a movie's first call taken for a frame", "    if(k->seen_start==toy_present.start_tick) {", "    if(true) {"),
 (GLUE, "a call that did both counted as a skip", "        if(out!=k->seen_out) {", "        if(out!=k->seen_out && skip==k->seen_skip) {"),
 (GLUE, "skipped frames not counted", "            ++p->skip;\n", ""),
 (GLUE, "a skip's time counted with the frames put out", "            p->skip_ticks+=spent;", "            p->out_ticks+=spent;"),
 (GLUE, "interrupt time not taken from the call before", "    k->seen_interrupt=interrupt;\n", ""),
 (GLUE, "the cycle not counted from the call before", "    k->seen_tick=now.counter;\n", ""),
 (GLUE, "the first movie counted with the second", "    const unsigned movie=toy_present.trace==&C.present[1];", "    const unsigned movie=1u;"),
 (GLUE, "update time counted outside the second movie", "    if(piece<0)return;\n    w->update_ticks+=spent;", "    w->update_ticks+=spent;\n    if(piece<0)return;"),
 (GLUE, "the buffer's free space not the most", "    if(capacity-used>w->course_free[piece])w->course_free[piece]=capacity-used;", "    w->course_free[piece]=capacity-used;"),
 (GLUE, "the lag not the highest", "    if(lag<=INT32_MAX && lag>w->course_lag[piece])w->course_lag[piece]=lag;", "    w->course_lag[piece]=lag;"),
 (GLUE, "a buffer without capacity taken", "    if(!capacity || capacity>UINT32_C(0x400000) || used>capacity)return;", "    if(capacity>UINT32_C(0x400000) || used>capacity)return;"),
 (GLUE, "more in the buffer than it holds taken", "    if(!capacity || capacity>UINT32_C(0x400000) || used>capacity)return;", "    if(!capacity || capacity>UINT32_C(0x400000))return;"),
 (GLUE, "a pointer outside the game's memory followed", "    if(!toy_chunk_ram(stream,20u))return;\n    const uint32_t channel=toy_chunk_word(stream+4u);\n    if(!toy_chunk_ram(channel,72u))return;\n    const uint32_t avi=toy_chunk_word(channel+68u);\n    if(!toy_chunk_ram(avi,108u))return;\n    const uint32_t capacity=toy_chunk_word(avi+0x14u);\n    const uint32_t used=toy_chunk_word(avi+0x68u)-toy_chunk_word(avi+0x64u);\n    if(!capacity || capacity>UINT32_C(0x400000) || used>capacity)return;\n    w->buffer_capacity=capacity;", "    const uint32_t channel=toy_chunk_word(stream+4u);\n    if(!toy_chunk_ram(channel,72u))return;\n    const uint32_t avi=toy_chunk_word(channel+68u);\n    if(!toy_chunk_ram(avi,108u))return;\n    const uint32_t capacity=toy_chunk_word(avi+0x14u);\n    const uint32_t used=toy_chunk_word(avi+0x68u)-toy_chunk_word(avi+0x64u);\n    if(!capacity || capacity>UINT32_C(0x400000) || used>capacity)return;\n    w->buffer_capacity=capacity;"),
]
# Test 74. What every build of it has: the game's calls timed whole, and the
# runs cut again soon after they began.
LEAN_MUTATIONS = [
 (STREAM, "runs cut again not counted",
  "        ++w->n->again[w->began - 1u][run < 8u ? 0u : run < 32u ? 1u : 2u];", "        (void)run;"),
 (STREAM, "a run behind a wait counted as one behind a cut", "    w->began = 2u;\n", "    w->began = 1u;\n"),
 (GLUE, "a call's time counted without its start", "    if(!enter && k->gd_open) {", "    if(!enter) {"),
 (GLUE, "the game's calls not counted", "        ++k->report.gd_calls;\n", ""),
 (AUX, "the game's calls not timed", "        toy_wire_gd_time(event==KUI_NATIVE_RA_DEMAND_GD_ENTER);\n", ""),
]
# What Test 74 no longer samples or times.
LEAN_GONE = {"update time counted outside the second movie", "the buffer's free space not the most",
             "the lag not the highest", "a buffer without capacity taken", "more in the buffer than it holds taken",
             "a pointer outside the game's memory followed", "waiting requests not counted"}
# Test 73 does not time restarts or place them in the course.
RESTARTS = {"restart time not kept", "restarts missing from the course"}
if WORDS == "1":
    MUTATIONS += WORDS_MUTATIONS
if TICK == "1" or CACHED == "1":
    MUTATIONS += MEASURE_MUTATIONS
if TICK == "1":
    MUTATIONS += TICK_MUTATIONS
if CACHED == "1":
    MUTATIONS += CACHED_MUTATIONS
if PACE == "1":
    MUTATIONS = [m for m in MUTATIONS if m[1] not in NOTES] + PACE_MUTATIONS
    MUTATIONS += IDLE_MUTATIONS if IDLE == "1" else PACE_ONLY_MUTATIONS
if IDLE == "1":
    MUTATIONS = [m for m in MUTATIONS if m[1] not in RESTARTS]
if LEAN == "1":
    MUTATIONS = [m for m in MUTATIONS if m[1] not in LEAN_GONE] + LEAN_MUTATIONS
if os.environ.get("TOY_WIRE_ONLY"):
    # A named part of the list, for running it in pieces: "from:to" of the list's positions.
    low, high = (int(v) for v in os.environ["TOY_WIRE_ONLY"].split(":"))
    MUTATIONS = MUTATIONS[low:high]


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
