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
STREAM_H = "src/loader/toy_wire_stream.h"
GLUE = "src/loader/toy_wire_ahead.inc"
AHEAD = "src/loader/toy_movie_ahead.inc"
TICK = os.environ.get("TOY_WIRE_TICK", "0")
CACHED = os.environ.get("TOY_WIRE_CACHED", "0" if TICK == "1" else "1")
WORDS = os.environ.get("TOY_WIRE_WORDS", CACHED)
PACE = os.environ.get("TOY_WIRE_PACE", WORDS)
IDLE = os.environ.get("TOY_WIRE_IDLE", PACE)
LEAN = os.environ.get("TOY_WIRE_LEAN", IDLE)
STEADY = os.environ.get("TOY_WIRE_STEADY", LEAN)
FLUID = os.environ.get("TOY_WIRE_FLUID", STEADY)
# Test 77 is asked for by name: the commands recorded for earlier tests keep
# checking what they checked.
SEQ = os.environ.get("TOY_WIRE_SEQ", "0")
ANY = os.environ.get("TOY_WIRE_ANY", "0")
TITLES = os.environ.get("TOY_WIRE_TITLES", "0")
AUX = "src/loader/native_readahead_aux.c"
# The cold stage's scan: its mutants are run by tests/test_toy_titles_stage.c.
SCAN = "src/loader/toy_movie_iso_stage.inc"
# The worker's report text, its lines for the screen and its pages: run by tests/test_toy_wire_report.c.
REPORT = "src/loader/toy_suite_report_save.inc"
SWITCHES = [f"TOY_WIRE_TICK={TICK}", f"TOY_WIRE_CACHED={CACHED}", f"TOY_WIRE_WORDS={WORDS}", f"TOY_WIRE_PACE={PACE}",
            f"TOY_WIRE_IDLE={IDLE}", f"TOY_WIRE_LEAN={LEAN}", f"TOY_WIRE_STEADY={STEADY}",
            f"TOY_WIRE_FLUID={FLUID}"]
DEFINES = [f"-DKUI_TOY_WIRE_TICK={TICK}", f"-DKUI_TOY_WIRE_CACHED={CACHED}", f"-DKUI_TOY_WIRE_WORDS={WORDS}",
           f"-DKUI_TOY_WIRE_PACE={PACE}", f"-DKUI_TOY_WIRE_IDLE={IDLE}", f"-DKUI_TOY_WIRE_LEAN={LEAN}",
           f"-DKUI_TOY_WIRE_STEADY={STEADY}", f"-DKUI_TOY_WIRE_FLUID={FLUID}"]
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
 (AHEAD, "sector served without its check", "    const bool good=toy_wire_sector(k,raw,lba);", "    const bool good=true;(void)toy_wire_sector;"),
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
# OPEN: the second movie's refills are not held back at its start.
OPEN_MUTATIONS = [
 (AHEAD, "the second movie held back all the same",
  "#if !(KUI_TOY_WIRE_STREAM && KUI_TOY_WIRE_OPEN)\n        /* Test 57's hold on the second movie's refills at its start: the",
  "#if 1\n        /* Test 57's hold on the second movie's refills at its start: the"),
 (AUX, "the player's refills held back", "    (void)avi;(void)file;(void)bytes;\n    return false;", "    (void)avi;(void)file;(void)bytes;\n    return true;"),
]
# CLEAN: a sector taken on its blocks' CRC16, a rebuilt byte on its P code
# word, everything else through the EDC, and the sample that watches it.
# Three of its conditions have no mutation here, because no test can fail
# on them: the count of rebuilt bytes kept for a sector is held to six, and
# a sector cannot have more (four blocks lie whole inside it and two reach
# into it); a sector is taken the lighter way only behind the one accepted
# before it, and a rewound sector's marks are cleared, where every stream's
# first sector is the low reader's and takes the EDC whatever its marks say.
CLEAN_MUTATIONS = [
 (STREAM, "a rebuilt block called clean",
  "        out->kind = rebuilt ? KUI_WIRE_REBUILT : lost_data || lost_crc ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;",
  "        out->kind = rebuilt ? KUI_WIRE_CLEAN : lost_data || lost_crc ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;"),
 (STREAM, "a block with lost bytes called clean",
  "        out->kind = rebuilt ? KUI_WIRE_REBUILT : lost_data || lost_crc ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;",
  "        out->kind = rebuilt ? KUI_WIRE_REBUILT : KUI_WIRE_CLEAN;"),
 (STREAM, "the rebuilt byte's place given wrong", "        out->rebuilt_at = w->lost[0];", "        out->rebuilt_at = w->lost[0] + 1u;"),
 (GLUE, "a rebuilt byte taken without its code word",
  "        for(uint32_t i=0u;i<mends && light;i++)light=toy_wire_mend_holds(raw,k->mend[i]);", "        (void)toy_wire_mend_holds;"),
 (GLUE, "the code word summed one byte off", "    uint32_t word=at-12u,sum=0u;", "    uint32_t word=at-11u,sum=0u;"),
 (GLUE, "the code word summed short of its last byte",
  "    for(const uint8_t *p=raw+12u+word,*end=raw+TOY_WIRE_P_END;p<end;p+=86u)sum^=*p;",
  "    for(const uint8_t *p=raw+12u+word,*end=raw+TOY_WIRE_P_END-86u;p<end;p+=86u)sum^=*p;"),
 (GLUE, "a byte no code word reaches taken as covered",
  "        } else if(part+at-first>=TOY_WIRE_P_END || k->mend_count==TOY_WIRE_MENDS) {",
  "        } else if(k->mend_count==TOY_WIRE_MENDS) {"),
 (GLUE, "a block never confirmed taken as checked in the next sector", "            if(k->block_open)k->loose=1u;", "            (void)0;"),
 (GLUE, "a block confirmed by a sector that was not asked", "            k->block_open=0u;\n            goto accepted;",
  "            goto accepted;"),
 (GLUE, "a block the stream could not check taken on trust", "    } else if(k->block_kind!=KUI_WIRE_CLEAN)k->loose=1u;", "    }"),
 (GLUE, "the low reader's blocks taken as the stream's", "        k->block_kind=KUI_WIRE_LOOSE;", "        k->block_kind=KUI_WIRE_CLEAN;"),
 (GLUE, "the lighter check without the address",
  "    return (raw[12]|(uint32_t)raw[13]<<8|(uint32_t)raw[14]<<16)==next && raw[15]==1u;", "    (void)next;return raw[15]==1u;"),
 (GLUE, "the address not carried on to the next sector", "    k->head[2]=raw[14];\n", ""),
 (GLUE, "the sample never taken",
  "            if(!(++k->light_run%TOY_WIRE_SAMPLE) && !kui_sector_edc_valid_at(raw,lba+150u)) {",
  "            if(!++k->light_run) {"),
 (GLUE, "a failed sample leaves the lighter check on", "                k->light_off=1u;\n", ""),
 (GLUE, "a failed sample's sector taken all the same",
  "                k->light_off=1u;\n                return false;", "                k->light_off=1u;"),
 (GLUE, "the lighter check not switched off", "    bool light=!lost && !k->loose && !k->light_off && k->head_fad+1u==lba+150u;",
  "    bool light=!lost && !k->loose && k->head_fad+1u==lba+150u;"),
]
# MSB: the bit order asked of the receiver and read from the first token.
MSB_MUTATIONS = [

 (STREAM, "the bit order left changed for the module reset", "    if(w->asked) wr8(SCMR, 0);\n", ""),
 (STREAM, "MSB first asked again after the receiver's answer",
  "    w->asked = w->msb = counters->order != KUI_WIRE_ORDER_LSB && counters->order != KUI_WIRE_ORDER_BACK;",
  "    w->asked = w->msb = true;"),
 (STREAM, "the bit not written for a run", "    if(w->msb) wr8(SCMR, SDIR);\n", ""),
 (STREAM, "wrong polynomial for bytes as sent", "c << 1 ^ 0x1021u", "c << 1 ^ 0x1001u"),
 (STREAM, "the remainder for bytes as sent not byte-swapped", "            c = (c >> 8 | c << 8) & 0xffffu;\n", ""),
 (STREAM, "bytes as sent stored reversed all the same", "            w->reverse[i] = (uint8_t)i;",
  "            w->reverse[i] = (uint8_t)(((i * 0x0802u & 0x22110u) | (i * 0x8020u & 0x88440u)) * 0x10101u >> 16);"),
 (STREAM, "the order settled with a lost byte ahead of the token",
  "                if(w->gap_hole || (value != TOKEN_TRUE && value != TOKEN_WIRE)) {",
  "                if(value != TOKEN_TRUE && value != TOKEN_WIRE) {"),
 (STREAM, "the question never given up",
  "                    if(++w->n->undecided >= KUI_WIRE_ASKS) w->n->order = KUI_WIRE_ORDER_LSB;",
  "                    ++w->n->undecided;"),
 (STREAM, "a token the old way round not believed",
  "                    w->n->order = KUI_WIRE_ORDER_LSB;\n                    w->msb = 0u;",
  "                    w->n->order = KUI_WIRE_ORDER_SEEN;\n                    w->msb = 0u;"),
 (STREAM, "the old tables not rebuilt for a token the old way round",
  "                    wire_tables(w, w->reverse);\n", ""),
 (STREAM, "a byte rebuilt before the order is confirmed", "            && w->n->order != KUI_WIRE_ORDER_SEEN\n", ""),
 (STREAM, "a first block's failed check does not end the asking",
  "                w->n->order = KUI_WIRE_ORDER_BACK;", "                (void)0;"),
 (STREAM, "the order confirmed by a block with lost bytes",
  "        if(w->n->order == KUI_WIRE_ORDER_SEEN && !lost_data && !lost_crc) w->n->order = KUI_WIRE_ORDER_MSB;",
  "        if(w->n->order == KUI_WIRE_ORDER_SEEN) w->n->order = KUI_WIRE_ORDER_MSB;"),
 (STREAM, "bytes as sent: second pair of a word not shifted down", "        index = (uint32_t)remainder ^ word >> 16;",
  "        index = (uint32_t)remainder ^ word >> 8;"),
 (STREAM, "bytes as sent: table halves swapped",
  "        remainder = pair[index & 0xffu] ^ step[index >> 8 & 0xffu];\n        index = (uint32_t)remainder ^ word >> 16;",
  "        remainder = step[index & 0xffu] ^ pair[index >> 8 & 0xffu];\n        index = (uint32_t)remainder ^ word >> 16;"),
 (STREAM, "bytes as sent: third byte of a word stored from the second", "        out[2] = (uint8_t)(word >> 16);",
  "        out[2] = (uint8_t)(word >> 8);"),
 (STREAM, "bytes as sent: last byte of a word not written", "        out[3] = (uint8_t)(word >> 24);\n", ""),
 (STREAM, "bytes as sent: sign bits of the table left in the CRC", "    return (uint32_t)remainder & 0xffffu;",
  "    return (uint32_t)remainder;"),
]
# QUICK: the short way through the dispatcher, and each term of the
# condition under which it may be taken. Two terms have no mutation here,
# because no test can fail on them: a request under way and a wait asked
# for, which the request's status, not yet "completed", rules out as well.
QUICK_MUTATIONS = [
 (AHEAD, "the short way while the region is lent out",
  "        e->native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE &&\n        r->shared.stream.state!=RA_RESUME &&",
  "        r->shared.stream.state!=RA_RESUME &&"),
 (AHEAD, "the short way with the low reader mid-resume",
  "        e->native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE &&\n        r->shared.stream.state!=RA_RESUME &&",
  "        e->native_loan_state==KUI_VMU_NATIVE_ASYNC_LOAN_IDLE &&"),
 (AHEAD, "the short way after the movie", "        !A.error && toy_ahead_live() && s->status==KUI_GD_COMPLETED &&",
  "        !A.error && s->status==KUI_GD_COMPLETED &&"),
 (AHEAD, "the short way after a request that did not complete",
  "        !A.error && toy_ahead_live() && s->status==KUI_GD_COMPLETED &&", "        !A.error && toy_ahead_live() &&"),
 (AHEAD, "the short way for a window in error", "        !A.error && toy_ahead_live() && s->status==KUI_GD_COMPLETED &&",
  "        toy_ahead_live() && s->status==KUI_GD_COMPLETED &&"),
 (AHEAD, "the short way under a command that is not a read",
  "        (!s->command || s->command==KUI_GD_PIOREAD || s->command==KUI_GD_DMAREAD) &&\n        e->opened",
  "        e->opened"),
 (AHEAD, "the short way with the reader closed", "        e->opened && !r->shared.port.fault && r->shared.port.acquired;",
  "        !r->shared.port.fault && r->shared.port.acquired;"),
 (AHEAD, "the short way on a faulted bus", "        e->opened && !r->shared.port.fault && r->shared.port.acquired;",
  "        e->opened && r->shared.port.acquired;"),
 (AHEAD, "the short way on a released bus", "        e->opened && !r->shared.port.fault && r->shared.port.acquired;",
  "        e->opened && !r->shared.port.fault;"),
 (AUX, "an idle call always pumps on the short way", "        if(!r->engine.in_irq && !toy_wire_idle_due())return 1u;\n", ""),
 (AUX, "an interrupt held to the idle rule on the short way",
  "        if(!r->engine.in_irq && !toy_wire_idle_due())return 1u;", "        if(!toy_wire_idle_due())return 1u;"),
 (AUX, "a pump's error on the short way leaves the window", "        if(!KUI_TOY_AHEAD.error)return 1u;\n        ++C.stats.abandoned;",
  "        return 1u;\n        ++C.stats.abandoned;"),
 (AUX, "a window given up on the short way not counted", "        if(!KUI_TOY_AHEAD.error)return 1u;\n        ++C.stats.abandoned;",
  "        if(!KUI_TOY_AHEAD.error)return 1u;"),
]
# Test 75. What every build of it has: the reader's time taken apart by the
# clock, and the frame at which the second movie's player first skipped.
STEADY_MUTATIONS = [
 (GLUE, "a block's time in the ring taken backwards", "        k->report.parse_ticks+=parse_tick-feed_tick;",
  "        k->report.parse_ticks+=feed_tick-parse_tick;"),
 (GLUE, "a block's feeding added to its time in the ring",
  "        k->report.feed_ticks+=feed_tick-toy_wire_phase();", "        k->report.parse_ticks+=feed_tick-toy_wire_phase();"),
 (GLUE, "the feeding timed from the block before", "        const uint32_t feed_tick=toy_wire_phase();\n        k->report.parse_ticks+=parse_tick-feed_tick;",
  "        const uint32_t feed_tick=parse_tick;toy_wire_phase();\n        k->report.parse_ticks+=parse_tick-feed_tick;"),
 (GLUE, "a reopening's time not kept", "        k->report.seek_ticks+=seek_tick-toy_wire_phase();", "        (void)seek_tick;"),
 (AHEAD, "the copying out not timed", "    toy_wire_keep()->report.publish_ticks+=publish_tick-toy_wire_phase();", "    (void)publish_tick;"),
 (AHEAD, "the copying out timed twice over",
  "    toy_wire_keep()->report.publish_ticks+=publish_tick-toy_wire_phase();",
  "    toy_wire_keep()->report.publish_ticks+=2u*(publish_tick-toy_wire_phase());"),
 (GLUE, "the stream's service not timed", "    k->report.service_ticks+=tick-toy_wire_phase();", "    (void)tick;"),
 (GLUE, "the stream's service timed backwards", "    k->report.service_ticks+=tick-toy_wire_phase();",
  "    k->report.service_ticks+=toy_wire_phase()-tick;"),
 (AHEAD, "the sectors' checks not timed", "    k->report.check_ticks+=check_tick-toy_wire_phase();", "    (void)check_tick;"),
 (AHEAD, "the sectors' checks added to the copying out",
  "    k->report.check_ticks+=check_tick-toy_wire_phase();", "    k->report.publish_ticks+=check_tick-toy_wire_phase();"),
 (GLUE, "the first movie's skip taken for the second's",
  "        if(movie && skip!=k->seen_skip && !w->skip_first)", "        if(skip!=k->seen_skip && !w->skip_first)"),
 (GLUE, "a later skip takes the first one's place",
  "        if(movie && skip!=k->seen_skip && !w->skip_first)", "        if(movie && skip!=k->seen_skip)"),
 (GLUE, "the first skip's frame counted from nought",
  "w->skip_first=toy_chunk_word(ctx+0x1c0u)+1u;", "w->skip_first=toy_chunk_word(ctx+0x1c0u);"),
 (GLUE, "a frame put out taken for a skip",
  "        if(movie && skip!=k->seen_skip && !w->skip_first)", "        if(movie && out!=k->seen_out && !w->skip_first)"),
]
# STAY: a request the game waits for in a loop of its own keeps its window.
# (That the window must stand at the request's first sector has no mutation:
# a request's first visit is never a waiting one, and that visit already
# gives up a window standing elsewhere. The fixture checks the outcome.)
STAY_MUTATIONS = [
 (AHEAD, "a waited request gives the window up all the same",
  "         (A.variant==variant && s->lba==A.first && !A.error && toy_wire_stay(s->token)))", "         false)"),
 (GLUE, "a wait in vain never ends", "return k->stay_tick-now.counter<TOY_WIRE_STAY_TICKS;", "return true;"),
 (GLUE, "a wait given up at once", "return k->stay_tick-now.counter<TOY_WIRE_STAY_TICKS;", "return false;"),
 (GLUE, "a wait the window grows under is given up all the same",
  "    } else if(k->stay_verified==A.verified)return", "    } else return"),
 (GLUE, "a wait renewed although the window has not grown",
  "    } else if(k->stay_verified==A.verified)return", "    } else if(false)return"),
 (GLUE, "the window's end at a wait's start not kept",
  "    k->stay_verified=A.verified;\n    k->stay_tick=now.counter;\n    return true;", "    k->stay_tick=now.counter;\n    return true;"),
 (GLUE, "a wait in vain ends a tenth too late", "enum { TOY_WIRE_STAY_TICKS=195312u };", "enum { TOY_WIRE_STAY_TICKS=215312u };"),
 (GLUE, "a wait given up a tenth too early", "enum { TOY_WIRE_STAY_TICKS=195312u };", "enum { TOY_WIRE_STAY_TICKS=175000u };"),
 (GLUE, "a request's waiting calls each counted as a request",
  "    if(k->stay_token!=token) {\n        k->stay_token=token;", "    if(k->stay_token!=token || true) {\n        k->stay_token=token;"),
 (GLUE, "waits kept not counted", "        ++k->report.stays;\n", ""),
 (GLUE, "a wait kept without a clock", "    if(!now.valid)return false;\n    if(k->stay_token!=token) {",
  "    if(k->stay_token!=token) {"),
]
# LATER: an interrupt visit answers the stream and leaves the reading.
LATER_MUTATIONS = [
 (GLUE, "an interrupt visit never leaves the reading",
  "    return (w->state==KUI_WIRE_RUN || w->state==KUI_WIRE_WAIT) &&\n        w->head-w->tail<TOY_WIRE_LATER_BYTES && w->hole_put-w->hole_get<TOY_WIRE_LATER_HOLES;",
  "    return false;"),
 (GLUE, "the reading left with the ring half unread",
  "        w->head-w->tail<TOY_WIRE_LATER_BYTES && w->hole_put-w->hole_get<TOY_WIRE_LATER_HOLES;",
  "        w->hole_put-w->hole_get<TOY_WIRE_LATER_HOLES;"),
 (GLUE, "the reading left with the log of lost positions half full",
  "        w->head-w->tail<TOY_WIRE_LATER_BYTES && w->hole_put-w->hole_get<TOY_WIRE_LATER_HOLES;",
  "        w->head-w->tail<TOY_WIRE_LATER_BYTES;"),
 (GLUE, "the reading left with the stream standing still",
  "    return (w->state==KUI_WIRE_RUN || w->state==KUI_WIRE_WAIT) &&\n        w->head-w->tail",
  "    return w->state!=KUI_WIRE_OFF &&\n        w->head-w->tail"),
 (GLUE, "the ring's mark at three quarters",
  "enum { TOY_WIRE_LATER_BYTES=KUI_WIRE_RING_BYTES/2u, TOY_WIRE_LATER_HOLES=KUI_WIRE_HOLES/2u };",
  "enum { TOY_WIRE_LATER_BYTES=3u*KUI_WIRE_RING_BYTES/4u, TOY_WIRE_LATER_HOLES=KUI_WIRE_HOLES/2u };"),
 (GLUE, "the log's mark one entry late",
  "enum { TOY_WIRE_LATER_BYTES=KUI_WIRE_RING_BYTES/2u, TOY_WIRE_LATER_HOLES=KUI_WIRE_HOLES/2u };",
  "enum { TOY_WIRE_LATER_BYTES=KUI_WIRE_RING_BYTES/2u, TOY_WIRE_LATER_HOLES=KUI_WIRE_HOLES/2u+1u };"),
 (GLUE, "a wait just begun not looked at again by a visit that leaves",
  "    if(w->state==KUI_WIRE_WAIT)toy_wire_serve(k,w);\n", ""),
 (GLUE, "the stream not served by a visit that leaves",
  "static bool toy_wire_later(struct toy_wire_keep *k,struct kui_wire_stream *w) {\n    toy_wire_serve(k,w);\n",
  "static bool toy_wire_later(struct toy_wire_keep *k,struct kui_wire_stream *w) {\n"),
 (GLUE, "the reading left while a request waits",
  "        if(interrupt && !waiting && toy_wire_later(k,w))++k->report.later;",
  "        if(interrupt && toy_wire_later(k,w))++k->report.later;"),
 (GLUE, "a call of the game's leaves the reading too",
  "        if(interrupt && !waiting && toy_wire_later(k,w))++k->report.later;",
  "        if(!waiting && toy_wire_later(k,w))++k->report.later;"),
 (GLUE, "visits that left not counted",
  "        if(interrupt && !waiting && toy_wire_later(k,w))++k->report.later;",
  "        if(interrupt && !waiting && toy_wire_later(k,w)) {}"),
 (GLUE, "visits out of time not counted where interrupt visits may leave",
  "        else if(toy_wire_drain(r,TOY_WIRE_TICKS_FOREGROUND) && k->on)++k->report.budget_ends;",
  "        else if(toy_wire_drain(r,TOY_WIRE_TICKS_FOREGROUND) && k->on) {}"),
 (GLUE, "an interrupt visit that reads keeps to the short time",
  "        else if(toy_wire_drain(r,TOY_WIRE_TICKS_FOREGROUND) && k->on)++k->report.budget_ends;",
  "        else if(toy_wire_drain(r,interrupt && !waiting?TOY_WIRE_TICKS_INTERRUPT:TOY_WIRE_TICKS_FOREGROUND) && k->on)++k->report.budget_ends;"),
]
# RING: the block loop reads the ring through the cache, every line of a
# run dropped from it first. The fixtures' view of the ring keeps what
# memory held at a line's last drop.
RING_MUTATIONS = [
 (STREAM, "a run's last line read without being dropped",
  "line < end; line += 32) wire_drop(line);", "line + 32 < end; line += 32) wire_drop(line);"),
 (STREAM, "lines dropped from the run's own address on",
  "    for(const uint8_t *line = view - ((uintptr_t)view & 31u); line < end;", "    for(const uint8_t *line = view; line < end;"),
 (STREAM, "every other line dropped", "line < end; line += 32) wire_drop(line);", "line < end; line += 64) wire_drop(line);"),
 (STREAM, "no line dropped", "line < end; line += 32) wire_drop(line);", "line < end; line += 32) (void)line;"),
 (STREAM, "lines dropped up to a run's first word only",
  "*const end = view + 4u * words;", "*const end = view + 4u;"),
 (STREAM, "a short run read through the cache as it stands",
  "    if(words < WIRE_FRESH_WORDS) return p;", "    if(words < WIRE_FRESH_WORDS) return wire_view(ring) + (p - ring);"),
 (STREAM, "the cache's view taken a line further on",
  "    const uint8_t *const view = wire_view(ring) + (p - ring), ", "    const uint8_t *const view = wire_view(ring) + (p - ring) + 32, "),
]
# Test 76. What every build of it has: the ending of streams timed, the
# wire's standstill placed in the course, and the player's buffer looked at
# again, in the calls in which the player took a frame.
FLUID_MUTATIONS = [
 (GLUE, "the ending of a stream not timed", "    k->report.halt_ticks+=halt_tick-toy_wire_phase();", "    (void)halt_tick;"),
 (GLUE, "the ending of a stream timed backwards", "    k->report.halt_ticks+=halt_tick-toy_wire_phase();",
  "    k->report.halt_ticks+=toy_wire_phase()-halt_tick;"),
 (GLUE, "the wire's standstill placed in no piece",
  "    if(piece>=0)k->report.course_pause[piece]+=k->report.n.wait_ticks-k->seen_pause;\n", ""),
 (GLUE, "the wire's standstill counted again at every visit", "    k->seen_pause=k->report.n.wait_ticks;\n#endif\n}", "#endif\n}"),
 (GLUE, "the wire's standstill before the movie placed in its first piece",
  "    if(piece>=0)k->report.course_pause[piece]+=k->report.n.wait_ticks-k->seen_pause;\n    k->seen_pause=k->report.n.wait_ticks;",
  "    if(piece>=0) {k->report.course_pause[piece]+=k->report.n.wait_ticks-k->seen_pause;\n    k->seen_pause=k->report.n.wait_ticks;}"),
 (GLUE, "the buffer looked at in a call that took no frame", "    if(piece<0 || !frame)return;", "    if(piece<0)return;"),
 (GLUE, "the buffer looked at in a movie's first call",
  "    const bool frame=k->seen_start==toy_present.start_tick && (out!=k->seen_out || skip!=k->seen_skip);",
  "    const bool frame=out!=k->seen_out || skip!=k->seen_skip;"),
 (GLUE, "the buffer not looked at for a frame skipped",
  "    const bool frame=k->seen_start==toy_present.start_tick && (out!=k->seen_out || skip!=k->seen_skip);",
  "    const bool frame=k->seen_start==toy_present.start_tick && out!=k->seen_out;"),
 (GLUE, "the first movie's buffer placed in the course", "    if(piece<0 || !frame)return;", "    if(!frame)return;"),
]
# The corner the stream leaves to its caller, as tests/test_toy_wire_stream.c
# makes it by construction since Test 76: two readings of a block's
# beginning open, and what decides between them.
CORNER_MUTATIONS = [
 (STREAM, "a reading that cannot be checked taken without the look behind it",
  "        } else if(w->alternative) accept = wire_gap_like(w, end);", "        } else if(w->alternative) accept = true;"),
 (STREAM, "the choice between two readings not held for the byte behind",
  "        const uint32_t need = w->alternative ? end + 1u : end;", "        const uint32_t need = end;"),
 (STREAM, "a first reading whose lost byte the CRC16 cannot explain handed on",
  "            } else if(w->alternative) accept = false;\n            else ++w->n->unsolved;", "            } else ++w->n->unsolved;"),
 (STREAM, "a block whose CRC16 lost a byte called clean",
  "        out->kind = rebuilt ? KUI_WIRE_REBUILT : lost_data || lost_crc ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;",
  "        out->kind = rebuilt ? KUI_WIRE_REBUILT : lost_data ? KUI_WIRE_LOOSE : KUI_WIRE_CLEAN;"),
 (STREAM, "the rebuilt byte's place reported one off",
  "        out->rebuilt_at = w->lost[0];", "        out->rebuilt_at = w->lost[0] ^ 1u;"),
]
# QUIT: a stream given up while it receives is ended at once.
# (Clearing the channel's end flag with its enable bit has no mutation: a
# stop asks only for the count, and the flag matters to a run that is
# accounted, which a stopped one is not.)
QUIT_MUTATIONS = [
 (STREAM, "a stream that receives is waited out",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif", "                (void)address;\n        }\n#endif"),
 (STREAM, "the receiver switched off in mid byte",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif", "                wr8(SCR, 0);\n        }\n#endif"),
 (STREAM, "the channel taken off and the byte not waited for",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif\n        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif"),
 (STREAM, "the byte waited for two looks long",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif\n        for(uint32_t n = 0; n < 200000u && !(rd8(SSR) & ORER); ++n) {}",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif\n        for(uint32_t n = 0; n < 2u && !(rd8(SSR) & ORER); ++n) {}"),
 (STREAM, "the channel's end flag written where its enable bit should go",
  "                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));\n        }\n#endif", "                wr32(CHCR1, rd32(CHCR1) | UINT32_C(2));\n        }\n#endif"),
 (STREAM, "a channel that is no longer the stream's switched off",
  "            if(rd32(SAR1) == RDR_PHYSICAL && address >= start && address <= start + w->seg_len)\n                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));",
  "            (void)start; (void)address;\n                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));"),
 (STREAM, "a channel pointed elsewhere switched off",
  "            if(rd32(SAR1) == RDR_PHYSICAL && address >= start && address <= start + w->seg_len)\n                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));",
  "            if(rd32(SAR1) == RDR_PHYSICAL)\n                wr32(CHCR1, rd32(CHCR1) & ~UINT32_C(1));"),
]
# COPY: a block goes into the window by the hand-written copy. The routine
# itself is SH-4 code no host test runs: tests/test_toy_wire_linked_copy.py
# runs it as linked. Here: when it is used. (That the source and the length
# are multiples of four have no mutations of their own. A raw sector and a
# block both are whole words, and the blocks the reader is handed begin on
# one: a piece whose source or length is not comes only with a destination
# or a length that is not, which the mutation below takes away with them.)
COPY_MUTATIONS = [
 (AHEAD, "a place that is not a multiple of four given to the word copy",
  "    if(((uintptr_t)out|(uintptr_t)in|bytes)&3u)memcpy(out,in,bytes);", "    if(false)memcpy(out,in,bytes);"),
 (AHEAD, "the window's copy left to the library",
  "    if(((uintptr_t)out|(uintptr_t)in|bytes)&3u)memcpy(out,in,bytes);", "    if(true)memcpy(out,in,bytes);"),
 (AHEAD, "a piece copied to the sector's start",
  "    toy_wire_copy(raw+part,data,bytes);", "    toy_wire_copy(raw,data,bytes);"),
]
# GAP: the search for a block's token reads the ring through the cache,
# each line dropped before the call's first read of it.
GAP_MUTATIONS = [
 (STREAM, "a gap's line read as the cache has it",
  "                fresh = index >> 5;\n                wire_drop(view + (index & ~UINT32_C(31)));", "                fresh = index >> 5;"),
 (STREAM, "only the search's first line dropped",
  "            if(index >> 5 != fresh) {\n                fresh = index >> 5;", "            if(fresh == NONE) {\n                fresh = index >> 5;"),
 (STREAM, "the line behind the gap's dropped",
  "                wire_drop(view + (index & ~UINT32_C(31)));", "                wire_drop(view + ((index + 32u & MASK) & ~UINT32_C(31)));"),
 (STREAM, "a gap's byte read a line further on",
  "            const uint8_t value = view[index];", "            const uint8_t value = view[(index + 32u) & MASK];"),
 (STREAM, "a line taken as dropped from the call before",
  "        uint32_t fresh = NONE;\n#endif\n        for(; at < landed; ++at) {",
  "        static uint32_t fresh = NONE;\n#endif\n        for(; at < landed; ++at) {"),
]
# APART: a block is taken into a buffer that shares no place in the cache
# with its bytes in the ring.
APART_MUTATIONS = [
 (STREAM, "a block always taken into the first buffer",
  "    if(((run - own + 17u) & 511u) < 17u + 16u) return w->blocks + KUI_WIRE_BLOCK_BYTES + KUI_WIRE_TABLE_BYTES;",
  "    (void)run; (void)own;"),
 (STREAM, "the second buffer used for the wrong blocks",
  "    if(((run - own + 17u) & 511u) < 17u + 16u) return", "    if(((run - own + 17u) & 511u) >= 17u + 16u) return"),
 (STREAM, "a block's last line in the ring left out",
  "    if(((run - own + 17u) & 511u) < 17u + 16u) return", "    if(((run - own + 16u) & 511u) < 16u + 16u) return"),
 (STREAM, "the buffer's last line left out",
  "    if(((run - own + 17u) & 511u) < 17u + 16u) return", "    if(((run - own + 17u) & 511u) < 17u + 15u) return"),
 (STREAM, "the first buffer taken for the second's room",
  "    w->block = w->blocks = tables - KUI_WIRE_BLOCK_BYTES;", "    w->block = w->blocks = tables + KUI_WIRE_TABLE_BYTES;"),
 (STREAM, "the block's place taken from where the ring was read before",
  "    const uintptr_t run = (uintptr_t)(wire_view(w->ring) + (first & MASK)) >> 5,",
  "    const uintptr_t run = (uintptr_t)(wire_view(w->ring) + (w->tail & MASK)) >> 5,"),
]
# Test 77 (SEQ): the window is opened by a read that begins where the read
# before it ended and is given up for a read elsewhere, not with the movie.
# The lines of the movie's rule are not compiled in it and their mutations
# go with them (SEQ_GONE); the same defects are made in the lines that take
# their place. One has no successor: the short way after the movie, whose
# end is no reason any more.
# Without a mutation, because no test can fail on them:
# - suite_raw's two tests of the track each by itself. The fixture's other
#   kind of track fails both, its sector size and its header, so a build
#   that asks only one of them still refuses it: the mutation takes both.
# - that the low reader is not still receiving when a stop command has
#   failed: the stop itself waits a reception out.
# - that a read counted as waited begins at the window's first sector and
#   is no larger than a window takes: one that is not is given up for by
#   the request's first call, which the game never waits in.
# - that a request is not for whole 2,352-byte sectors. suite_raw asks it
#   twice, of the service's sector size and of the cursor's format, as
#   suite_movie did, and either says it alone. The fixture asks for such
#   sectors at an open window's first sector, which both together refuse.
# - that a file's variant is not given to a read that begins ahead of the
#   file: the test that follows it, in unsigned arithmetic, refuses the
#   same reads.
SEQ_GONE = {"the short way after the movie", "the short way after a request that did not complete",
            "the short way for a window in error", "a waited request gives the window up all the same"}
SEQ_MUTATIONS = [
 (AHEAD, "the short way after a request that did not complete",
  "        !A.error && s->status==KUI_GD_COMPLETED &&\n#else", "        !A.error &&\n#else"),
 (AHEAD, "the short way for a window in error",
  "        !A.error && s->status==KUI_GD_COMPLETED &&\n#else", "        s->status==KUI_GD_COMPLETED &&\n#else"),
 (AHEAD, "a waited request gives the window up all the same",
  "         (A.variant && s->lba==A.first && !A.error && toy_wire_stay(s->token)))", "         false)"),
 (AHEAD, "a window for any read, consecutive or not",
  "        if(s->lba!=s->position_lba+1u || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;",
  "        if(!e->release.armed || r->shared.stream.state>RA_RESUME)return false;"),
 (AHEAD, "consecutive counted from the last sector read, not from behind it",
  "        if(s->lba!=s->position_lba+1u || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;",
  "        if(s->lba!=s->position_lba || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;"),
 (AHEAD, "a window where the stream cannot take the card over",
  "        if(s->lba!=s->position_lba+1u || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;",
  "        if(s->lba!=s->position_lba+1u || r->shared.stream.state>RA_RESUME)return false;"),
 (AHEAD, "a window on top of a token search the low reader left standing",
  "        if(s->lba!=s->position_lba+1u || !e->release.armed || r->shared.stream.state>RA_RESUME)return false;",
  "        if(s->lba!=s->position_lba+1u || !e->release.armed)return false;"),
 (AHEAD, "a stop command that failed once fails the game's read",
  "        if(!C.config.ops->busy()) {\n            A.variant=0u;", "        if(false) {\n            A.variant=0u;"),
 (AHEAD, "a window left marked open when its stop command failed",
  "        if(!C.config.ops->busy()) {\n            A.variant=0u;\n", "        if(!C.config.ops->busy()) {\n"),
 (AHEAD, "a read over the track's end counted as one the game waited for",
  "            wait && !kick && !warm && s->count<=A.cursor.lba+A.cursor.count-A.first?",
  "            wait && !kick && !warm?"),
 (AUX, "a window on a track that is not raw",
  "    return kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&\n        kui_retail_track_header_bytes(track)==16u;\n}\n/* And which",
  "    (void)track;return true;\n}\n/* And which"),
 (AHEAD, "the window ends a sector before its track's end",
  "    const uint32_t end=e->manifest->slots[e->cursor.track].track.end_lba;",
  "    const uint32_t end=e->manifest->slots[e->cursor.track].track.end_lba-1u;"),
 (AHEAD, "the window reaches a sector into the next track",
  "    const uint32_t end=e->manifest->slots[e->cursor.track].track.end_lba;",
  "    const uint32_t end=e->manifest->slots[e->cursor.track].track.end_lba+1u;"),
 (AHEAD, "a window outside the movie files left unmarked",
  "    A.variant=(uint8_t)(variant?variant:AHEAD_OTHER);", "    A.variant=(uint8_t)variant;"),
 (AHEAD, "the other windows' sectors counted as a movie's",
  "        if(A.variant==AHEAD_OTHER)toy_wire_keep()->report.other_sectors+=s->count;",
  "        if(false)toy_wire_keep()->report.other_sectors+=s->count;"),
 (AHEAD, "a movie's sectors counted with the other windows'",
  "        if(A.variant==AHEAD_OTHER)toy_wire_keep()->report.other_sectors+=s->count;",
  "        if(true)toy_wire_keep()->report.other_sectors+=s->count;"),
 (AHEAD, "the other windows not counted", "        else ++toy_wire_keep()->report.other_windows;", "        else (void)0;"),
 (AHEAD, "a movie's window counted with the others",
  "        if(variant)++C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][2u+(variant-1u)/2u];\n        else",
  "        if(false)++C.diagnostics.stage_result[KUI_NATIVE_RA_STAGE_NONE][2u+(variant-1u)/2u];\n        else"),
 (AHEAD, "a request served in an interrupt visit taken for a failed one",
  "    const bool idle=!demand && !s->pending && s->status!=KUI_GD_COMPLETED;",
  "    const bool idle=!demand && s->status!=KUI_GD_COMPLETED;"),
 (AHEAD, "a request served in an interrupt visit taken for a failed one by the interrupt visits behind it",
  "    const bool idle=!demand && !s->pending && s->status!=KUI_GD_COMPLETED;",
  "    const bool idle=!demand && (!s->pending || e->in_irq) && s->status!=KUI_GD_COMPLETED;"),
 (AHEAD, "a request that did not complete keeps its window",
  "    const bool idle=!demand && !s->pending && s->status!=KUI_GD_COMPLETED;", "    const bool idle=false;"),
 (AHEAD, "a read elsewhere served from the window",
  "       (demand && (!selected || s->lba!=A.first ||\n", "       (demand && (!selected ||\n"),
 (AHEAD, "a read the window may not serve keeps the window",
  "       (demand && (!selected || s->lba!=A.first ||\n", "       (demand && (s->lba!=A.first ||\n"),
 (AHEAD, "a read beyond the window's end waits for it",
  "           s->count>A.cursor.lba+A.cursor.count-A.first)))) {", "           false)))) {"),
 (AHEAD, "a read to the window's last sector given up for",
  "           s->count>A.cursor.lba+A.cursor.count-A.first)))) {",
  "           s->count>=A.cursor.lba+A.cursor.count-A.first)))) {"),
 (AHEAD, "a window counted as given up before it is gone",
  "        if(toy_ahead_fallback(r))return true;\n        ++toy_wire_keep()->report.drops[why];",
  "        ++toy_wire_keep()->report.drops[why];\n        if(toy_ahead_fallback(r))return true;"),
 (AHEAD, "windows given up not counted", "        ++toy_wire_keep()->report.drops[why];\n", ""),
 (AHEAD, "a window in error counted as one whose request did not complete",
  "        const unsigned why=A.error?KUI_TOY_WIRE_DROP_ERROR:", "        const unsigned why=A.error?KUI_TOY_WIRE_DROP_IDLE:"),
 (AHEAD, "a window whose request did not complete counted as given up for a read",
  "            idle?KUI_TOY_WIRE_DROP_IDLE:", "            idle?KUI_TOY_WIRE_DROP_OTHER:"),
 (AHEAD, "a window given up for a command counted as given up for a read",
  "s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:\n            wait && !kick && !warm && s->count",
  "s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_OTHER:\n            wait && !kick && !warm && s->count"),
 (AHEAD, "a window given up for a waited request counted as given up for another read",
  "            KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_OTHER;", "            KUI_TOY_WIRE_DROP_OTHER:KUI_TOY_WIRE_DROP_OTHER;"),
 (AHEAD, "a window given up for a read counted as given up for a waited request",
  "            KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_OTHER;", "            KUI_TOY_WIRE_DROP_WAITING:KUI_TOY_WIRE_DROP_WAITING;"),
 (AUX, "a window in another title",
  "    if(toy_movie_ahead(r,variant,raw && C.config.toy_poll_owner,wait))return;",
  "    if(toy_movie_ahead(r,variant,raw,wait))return;"),
 (AUX, "a window on a source the stage did not find raw",
  "    if(C.suite_source!=1u || !e->active || e->failed || !s->pending || e->token!=s->token ||\n"
  "       s->sector_bytes!=KUI_GAME_DATA_BYTES || !s->count ||\n       !e->cursor.manifest",
  "    if(!e->active || e->failed || !s->pending || e->token!=s->token ||\n"
  "       s->sector_bytes!=KUI_GAME_DATA_BYTES || !s->count ||\n       !e->cursor.manifest"),
 (AUX, "the movie's rule for the window after all",
  "    if(toy_movie_ahead(r,variant,raw && C.config.toy_poll_owner,wait))return;",
  "    if(toy_movie_ahead(r,variant,in_play,wait))return;"),
 (AUX, "a file's variant for a read that reaches beyond its end",
  "    return s->lba<movie->lba || s->lba-movie->lba>=movie->count ||\n        s->count>movie->count-(s->lba-movie->lba)?0u:variant;",
  "    return s->lba<movie->lba || s->lba-movie->lba>=movie->count?0u:variant;"),
 (AUX, "no file's variant for a read inside the file",
  "    unsigned variant=raw?suite_file(&r->shared.service):0u;", "    unsigned variant=0u;(void)suite_file;"),
]
# What Test 76 samples again of what Test 74 gave up: the buffer, not the lag
# and not the update calls' time.
FLUID_BACK = {"the buffer's free space not the most", "a buffer without capacity taken",
              "more in the buffer than it holds taken", "a pointer outside the game's memory followed"}
# What Test 76 no longer times: the game's GD calls, whole.
FLUID_GONE = {"a call's time counted without its start", "the game's calls not counted", "the game's calls not timed"}
# With LATER the pump's call of the reading is another line: the same
# mutation is made there instead.
LATER_GONE = {"visits out of time not counted"}
# What Test 75 no longer counts: the runs cut again soon after they began.
STEADY_GONE = {"runs cut again not counted", "a run behind a wait counted as one behind a cut"}
# SWIFT, the hand-written block loops, is SH-4 code no host test runs:
# tests/test_toy_wire_linked_loops.py runs them as linked.
# What Test 74 no longer samples or times.
LEAN_GONE = {"update time counted outside the second movie", "the buffer's free space not the most",
             "the lag not the highest", "a buffer without capacity taken", "more in the buffer than it holds taken",
             "a pointer outside the game's memory followed", "waiting requests not counted"}
# With the short way, a call or an interrupt visit without a request no longer reaches Test 73's rule in
# toy_movie_ahead: the same two mutations are made on the short way instead.
QUICK_GONE = {"the game's idle calls always pump", "an interrupt visit held to the spacing"}
# Test 73 does not time restarts or place them in the course.
RESTARTS = {"restart time not kept", "restarts missing from the course"}
# Test 79: the same for TOY_WIRE_TITLES.
TITLES_GONE = {"a window in another title", "the movie's rule for the window after all"}
TITLES_MUTATIONS = [
 (AUX, "the window for the verified title alone",
  "    if(toy_movie_ahead(r,variant,raw,wait))return;",
  "    if(toy_movie_ahead(r,variant,raw && C.config.toy_poll_owner,wait))return;"),
 (AUX, "the movie's rule for the window after all",
  "    if(toy_movie_ahead(r,variant,raw,wait))return;",
  "    if(toy_movie_ahead(r,variant,in_play,wait))return;"),
 # The stage: which disc is a source for the window.
 (SCAN, "an audio track taken for a raw data track",
  "        if(kui_retail_track_control(track) &&\n", "        if(\n"),
 (SCAN, "a track with its subcode taken for a raw one",
  "        if(kui_retail_track_control(track) &&\n           kui_retail_track_sector_bytes(track)==KUI_GAME_RAW_BYTES &&\n",
  "        if(kui_retail_track_control(track) &&\n"),
 (SCAN, "a Mode 2 track taken for Mode 1",
  "           kui_retail_track_header_bytes(track)==16u)return true;", "           true)return true;"),
 (SCAN, "a source on another card adapter",
  "        if(manifest.storage_transport==KUI_STORAGE_SCI && toy_suite_raw_track())toy_suite_source=1u;",
  "        if(toy_suite_raw_track())toy_suite_source=1u;"),
 (SCAN, "a source with no raw track",
  "        if(manifest.storage_transport==KUI_STORAGE_SCI && toy_suite_raw_track())toy_suite_source=1u;",
  "        if(manifest.storage_transport==KUI_STORAGE_SCI)toy_suite_source=1u;"),
 (SCAN, "another title never a source",
  "        if(manifest.storage_transport==KUI_STORAGE_SCI && toy_suite_raw_track())toy_suite_source=1u;",
  "        if(manifest.storage_transport==KUI_STORAGE_SCI && toy_suite_raw_track())toy_suite_source=0u;"),
 (SCAN, "a source with the virtual VMU on",
  "       native_runtime_mode()==KUI_VMU_NATIVE_MODE_READER_ONLY &&\n       (boot_crc!=",
  "       (boot_crc!="),
 (SCAN, "a source for another reader",
  "    if(manifest.reader==KUI_RETAIL_READER_ASYNC &&\n       native_runtime_mode()==KUI_VMU_NATIVE_MODE_READER_ONLY &&\n       (boot_crc!=",
  "    if(native_runtime_mode()==KUI_VMU_NATIVE_MODE_READER_ONLY &&\n       (boot_crc!="),
 (SCAN, "Toy Commander taken for another title",
  "       (boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC || exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {",
  "       (true)) {"),
 # After the review: which title it is, is the executable's to say.
 (SCAN, "Toy Commander's executable on another boot sector taken for another title",
  "       (boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC || exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {",
  "       (manifest.ip_crc32!=KUI_VMU_LAUNCH_TOY_IP_CRC || boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC ||\n"
  "        exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {"),
 (SCAN, "an executable of Toy Commander's size taken for Toy Commander's",
  "       (boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC || exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {",
  "       (exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {"),
 (SCAN, "an executable with Toy Commander's checksum taken for Toy Commander's",
  "       (boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC || exec_bytes!=KUI_VMU_LAUNCH_TOY_EXEC_BYTES)) {",
  "       (boot_crc!=KUI_VMU_LAUNCH_TOY_EXEC_CRC)) {"),
 # The report on the screen.
 (REPORT, "a line for the screen cut in the middle of a value",
  "    if(at+n<end && at[n]!='\\n' && space)n=space;\n", ""),
 (REPORT, "a line for the screen of fifty-one places",
  "    while(at+n<end && at[n]!='\\n' && n<TOY_PAGE_PLACES) {", "    while(at+n<end && at[n]!='\\n' && n<=TOY_PAGE_PLACES) {"),
 (REPORT, "the space behind a cut shown on the next line",
  "    if(at<end && (*at=='\\n' || *at==' '))at++;", "    if(at<end && *at=='\\n')at++;"),
 (REPORT, "the text's padding shown as lines",
  "    if(at>=end || *at==' ')return false;", "    if(at>=end)return false;"),
 (REPORT, "thirteen lines of the text to a page",
  "            for(unsigned line=0u;line<TOY_PAGE_LINES && toy_report_screen_line(&at,end,row);line++)",
  "            for(unsigned line=0u;line<=TOY_PAGE_LINES && toy_report_screen_line(&at,end,row);line++)"),
 (REPORT, "the screen not put back behind a page",
  "            toy_report_pause(frames,&start_released,&reboot_refused);\n            retail_display_restore(display);",
  "            toy_report_pause(frames,&start_released,&reboot_refused);"),
 (REPORT, "a page without its heading",
  "            retail_display_line(\"REPORT NOT SAVED / FILM EVERY PAGE\");\n", ""),
 (REPORT, "the page never says that Start was refused",
  "            retail_display_line(reboot_refused?\"RESTART BUSY / TRY AGAIN OR POWER OFF\":\"START / REBOOT TO K-UI CD\");",
  "            retail_display_line(\"START / REBOOT TO K-UI CD\");"),
 (REPORT, "the pages shown once and no more",
  "    for(;;) {\n        const char *at=text;\n        while(at<end && *at!=' ') {\n            char row[TOY_PAGE_PLACES+1u];",
  "    {\n        const char *at=text;\n        while(at<end && *at!=' ') {\n            char row[TOY_PAGE_PLACES+1u];"),
 (REPORT, "no card asked for where there is no report file",
  "    if(!state.armed || state.pending ||\n#else", "    if(!config.toy_report.test || !state.armed || state.pending ||\n#else"),
 (REPORT, "the card asked for by an unarmed worker",
  "    if(!state.armed || state.pending ||\n#else", "    if(state.pending ||\n#else"),
 (REPORT, "the low reader's overruns and its programmed blocks changed places",
  "        (uint32_t[]){sd->blocks,sd->polled,sd->overruns,sd->crc_errors,w->declined},5u);",
  "        (uint32_t[]){sd->blocks,sd->overruns,sd->polled,sd->crc_errors,w->declined},5u);"),
 (REPORT, "the stream's refused take-overs left out of the report",
  "        (uint32_t[]){sd->blocks,sd->polled,sd->overruns,sd->crc_errors,w->declined},5u);",
  "        (uint32_t[]){sd->blocks,sd->polled,sd->overruns,sd->crc_errors,0u},5u);"),
 (REPORT, "every title reported as Toy Commander",
  "w->large_returned,ra->demand.owner_verified},4u);", "w->large_returned,1u},4u);"),
]
# Test 78: mutations on lines the build with TOY_WIRE_ANY no longer compiles
# go (ANY_GONE), and the same defects are made in the lines that take their
# place; then the defects of the change itself.
ANY_GONE = {"course pieces not counted from the movie's start", "course not held to its last piece",
            "the copying out not timed", "the copying out timed twice over",
            "a waited request gives the window up all the same",
            "a read over the track's end counted as one the game waited for",
            "the other windows' sectors counted as a movie's", "a movie's sectors counted with the other windows'",
            "a read elsewhere served from the window", "a read the window may not serve keeps the window",
            "a read beyond the window's end waits for it", "a read to the window's last sector given up for",
            "a window given up for a command counted as given up for a read",
            "a window in error counted as one whose request did not complete",
            "a window whose request did not complete counted as given up for a read",
            "a window given up for a waited request counted as given up for another read",
            "a window given up for a read counted as given up for a waited request",
            # The course by piece and the player's buffer behind it. Test 78 still compiles
            # these lines, but every one of them lies behind toy_wire_course(), which in
            # Test 78 names no piece: nothing they compute is kept, printed or checked.
            # (Found by the list's first whole run under Test 79: ten entries not caught.)
            "interrupt and game time swapped in the course",
            "the buffer's free space not the most", "a buffer without capacity taken",
            "more in the buffer than it holds taken", "a pointer outside the game's memory followed",
            "the wire's standstill placed in no piece", "the wire's standstill counted again at every visit",
            "the wire's standstill before the movie placed in its first piece",
            "the buffer looked at in a movie's first call", "the buffer not looked at for a frame skipped"}
ANY_MUTATIONS = [
 # The same defects as the entries of ANY_GONE, in the lines Test 78 compiles.
 # (The course by piece has none: it is not kept.)
 (AHEAD, "the copying out not timed",
  "    w->publish_ticks+=publish_tick-toy_wire_phase();", "    (void)publish_tick;"),
 (AHEAD, "the copying out timed twice over",
  "    w->publish_ticks+=publish_tick-toy_wire_phase();", "    w->publish_ticks+=2u*(publish_tick-toy_wire_phase());"),
 (AHEAD, "a waited request gives the window up all the same",
  "         (A.variant && at==A.first && !A.error && toy_wire_stay(s->token)))", "         false)"),
 (AHEAD, "a read over the track's end counted as one the game waited for",
  "            wait && !kick && !warm && !beyond?KUI_TOY_WIRE_DROP_WAITING:",
  "            wait && !kick && !warm?KUI_TOY_WIRE_DROP_WAITING:"),
 (AHEAD, "a window in error counted as one whose request did not complete",
  "            A.error?KUI_TOY_WIRE_DROP_ERROR:idle?", "            A.error?KUI_TOY_WIRE_DROP_IDLE:idle?"),
 (AHEAD, "a window whose request did not complete counted as given up for a read",
  "ERROR:idle?KUI_TOY_WIRE_DROP_IDLE:", "ERROR:idle?KUI_TOY_WIRE_DROP_OTHER:"),
 (AHEAD, "a window given up for a waited request counted as given up for another read",
  "!beyond?KUI_TOY_WIRE_DROP_WAITING:", "!beyond?KUI_TOY_WIRE_DROP_OTHER:"),
 (AHEAD, "a window given up for a read counted as given up for a waited request",
  "            KUI_TOY_WIRE_DROP_OTHER;", "            KUI_TOY_WIRE_DROP_WAITING;"),
 (AHEAD, "the other windows' sectors counted as a movie's",
  "        if(A.variant==AHEAD_OTHER)w->other_sectors+=give;", "        if(false)w->other_sectors+=give;"),
 (AHEAD, "a movie's sectors counted with the other windows'",
  "        if(A.variant==AHEAD_OTHER)w->other_sectors+=give;", "        if(true)w->other_sectors+=give;"),
 (AHEAD, "a read elsewhere served from the window",
  "       (demand && (!selected || at!=A.first || beyond)))) {", "       (demand && (!selected || beyond)))) {"),
 (AHEAD, "a read the window may not serve keeps the window",
  "       (demand && (!selected || at!=A.first || beyond)))) {", "       (demand && (at!=A.first || beyond)))) {"),
 (AHEAD, "a read beyond the window's end waits for it",
  "    const bool beyond=left>A.cursor.lba+A.cursor.count-A.first;", "    const bool beyond=false;"),
 (AHEAD, "a read to the window's last sector given up for",
  "    const bool beyond=left>A.cursor.lba+A.cursor.count-A.first;",
  "    const bool beyond=left>=A.cursor.lba+A.cursor.count-A.first;"),
 (AHEAD, "a window given up for a command counted as given up for a read",
  "                s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_COMMAND:",
  "                s->command!=KUI_GD_DMAREAD?KUI_TOY_WIRE_DROP_OTHER:"),
 # The change itself.
 (AHEAD, "a large request waits for all of itself in the window",
  "        if(!large)return false;\n        give=have;", "        return false;"),
 (AHEAD, "a small request handed over in pieces",
  "        if(!large)return false;\n        give=have;", "        give=have;"),
 (AHEAD, "a slice of any length",
  "    if(large && give>AHEAD_SLICE)give=AHEAD_SLICE;", "    (void)large;"),
 (AHEAD, "every slice written at the request's start",
  "        e->cursor.write(e->cursor.context,(had+i)*KUI_GAME_DATA_BYTES,",
  "        e->cursor.write(e->cursor.context,i*KUI_GAME_DATA_BYTES,"),
 (AHEAD, "a slice not counted in the request's cursor",
  "        e->cursor.done=had+give;", "        e->cursor.done=give;"),
 (AHEAD, "a request complete with its first slice",
  "        if(e->cursor.done<s->count)return false;\n", ""),
 (AHEAD, "a large request not counted",
  "            ++w->large_requests;\n", ""),
 (AHEAD, "a large request's last slice counted for its sectors",
  "            w->large_sectors+=s->count;", "            w->large_sectors+=give;"),
 (AHEAD, "a request of sixty sectors counted as large",
  "    const bool large=s->count>AHEAD_REQUEST_MAX;", "    const bool large=s->count>=AHEAD_REQUEST_MAX;"),
 (AHEAD, "a returned request not counted",
  "    ++toy_wire_keep()->report.large_returned;\n", ""),
 (AHEAD, "a returned request read one sector short",
  "    if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba,s->count,\n           KUI_GAME_SECTOR_MODE1,write,context)",
  "    if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba,s->count-1u,\n           KUI_GAME_SECTOR_MODE1,write,context)"),
 (AHEAD, "a returned request read on from where the window stopped",
  "    if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba,s->count,\n           KUI_GAME_SECTOR_MODE1,write,context)",
  "    if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba+e->cursor.done,s->count-e->cursor.done,\n           KUI_GAME_SECTOR_MODE1,write,context)"),
 (AHEAD, "a request the window began goes back with its cursor as it stood",
  "    if(toy_ahead_clear(r)) {\n        if(begun)toy_ahead_restart(r);\n        return false;\n    }",
  "    if(toy_ahead_clear(r)) {\n        (void)begun;\n        return false;\n    }"),
 (AHEAD, "the same where the stop command had failed",
  "#if AHEAD_ANY\n            if(begun)toy_ahead_restart(r);\n#endif\n            return false;",
  "            return false;"),
 (AHEAD, "a request with nothing delivered begun again all the same",
  "    const bool begun=A.variant && e->active && e->cursor.done &&\n",
  "    const bool begun=A.variant && e->active &&\n"),
 (AHEAD, "the game told of a slice takes the request from the window",
  "        s->completed_bytes<=had*KUI_GAME_DATA_BYTES &&", "        !s->completed_bytes &&"),
 (AHEAD, "a request the window has begun is no longer its own",
  "        (!had || (left && s->count>AHEAD_REQUEST_MAX && A.variant && at==A.first)) &&", "        !had &&"),
 (AHEAD, "the window's place taken by the request's first sector",
  "    const uint32_t at=s->lba+had,left=s->count-had;", "    const uint32_t at=s->lba,left=s->count-had;"),
 (AHEAD, "what is owed taken for the whole request",
  "    const uint32_t at=s->lba+had,left=s->count-had;", "    const uint32_t at=s->lba+had,left=s->count;"),
]
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
    MUTATIONS = [m for m in MUTATIONS if m[1] not in LEAN_GONE - (FLUID_BACK if FLUID == "1" else set())] + LEAN_MUTATIONS
    for part, mutations in (("OPEN", OPEN_MUTATIONS), ("CLEAN", CLEAN_MUTATIONS), ("MSB", MSB_MUTATIONS),
                            ("QUICK", QUICK_MUTATIONS)):
        # A part left out of the build under test is left out here too.
        if os.environ.get(f"TOY_WIRE_{part}", "1") == "1":
            MUTATIONS += mutations
            if part == "QUICK":
                MUTATIONS = [m for m in MUTATIONS if m[1] not in QUICK_GONE]
        else:
            SWITCHES.append(f"TOY_WIRE_{part}=0")
            DEFINES.append(f"-DKUI_TOY_WIRE_{part}=0")
if STEADY == "1":
    MUTATIONS = [m for m in MUTATIONS if m[1] not in STEADY_GONE] + STEADY_MUTATIONS
    for part, mutations in (("STAY", STAY_MUTATIONS), ("LATER", LATER_MUTATIONS), ("RING", RING_MUTATIONS)):
        # Test 76 leaves LATER out unless a build asks for it.
        if os.environ.get(f"TOY_WIRE_{part}", "0" if part == "LATER" and FLUID == "1" else "1") == "1":
            MUTATIONS += mutations
            if part == "LATER":
                MUTATIONS = [m for m in MUTATIONS if m[1] not in LATER_GONE]
        else:
            SWITCHES.append(f"TOY_WIRE_{part}=0")
            DEFINES.append(f"-DKUI_TOY_WIRE_{part}=0")
if FLUID == "1":
    MUTATIONS = [m for m in MUTATIONS if m[1] not in FLUID_GONE] + FLUID_MUTATIONS
    # The two about a block's kind need a build that names kinds.
    MUTATIONS += [m for m in CORNER_MUTATIONS if os.environ.get("TOY_WIRE_CLEAN", "1") == "1" or "KUI_WIRE_" not in m[2] and "rebuilt_at" not in m[2]]
    # HOT changes which address the linked module uses for the window's
    # state and nothing a host build can see: tests/test_toy_wire_linked_hot.py
    # reads the linked module for it, and fails either way round.
    for part, mutations in (("QUIT", QUIT_MUTATIONS), ("HOT", []), ("COPY", COPY_MUTATIONS), ("GAP", GAP_MUTATIONS),
                            ("APART", APART_MUTATIONS)):
        # GAP reads as RING reads: a build without RING has no GAP either.
        # Test 77 leaves it out unless it is asked for.
        if os.environ.get(f"TOY_WIRE_{part}", ("0" if SEQ == "1" else os.environ.get("TOY_WIRE_RING", "1"))
                          if part == "GAP" else "1") == "1":
            MUTATIONS += mutations
            if part == "QUIT":
                # With the channel taken off, the old wait's own mutation is this part's last.
                MUTATIONS = [m for m in MUTATIONS if m[1] != "stop does not wait for a whole byte"]
        else:
            SWITCHES.append(f"TOY_WIRE_{part}=0")
            DEFINES.append(f"-DKUI_TOY_WIRE_{part}=0")
if SEQ == "1" and os.environ.get("TOY_WIRE_GAP", "0") == "1":
    # A Test 77 build that asks for GAP has to say so: it is out by default there.
    SWITCHES.append("TOY_WIRE_GAP=1")
    DEFINES.append("-DKUI_TOY_WIRE_GAP=1")
if SEQ == "1":
    SWITCHES.append("TOY_WIRE_SEQ=1")
    DEFINES.append("-DKUI_TOY_WIRE_SEQ=1")
    # Those of its mutations that take the place of STAY's and QUICK's go
    # with those parts.
    MUTATIONS = [m for m in MUTATIONS if m[1] not in SEQ_GONE] + [
        m for m in SEQ_MUTATIONS
        if not (m[1].startswith("the short way") and os.environ.get("TOY_WIRE_QUICK", "1") != "1")
        and not (m[1].startswith("a waited request") and os.environ.get("TOY_WIRE_STAY", "1") != "1")]
if ANY == "1":
    SWITCHES.append("TOY_WIRE_ANY=1")
    DEFINES.append("-DKUI_TOY_WIRE_ANY=1")
    MUTATIONS = [m for m in MUTATIONS if m[1] not in ANY_GONE] + ANY_MUTATIONS
if TITLES == "1":
    SWITCHES.append("TOY_WIRE_TITLES=1")
    DEFINES.append("-DKUI_TOY_WIRE_TITLES=1")
    MUTATIONS = [m for m in MUTATIONS if m[1] not in TITLES_GONE] + TITLES_MUTATIONS
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
        if os.environ.get("TOY_WIRE_BASELINE", "1") == "1":
            # The tests have to pass as they stand: where they fail without
            # a mutation, every mutation would count as caught.
            binary = Path(tmp) / "stream"
            code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                        "-DKUI_WIRE_STREAM_TEST=1", *DEFINES, "-Iinclude", "-Isrc/loader",
                        "tests/test_toy_wire_stream.c", STREAM, "-o", str(binary)], tree, 120)
            if code == 0:
                code = run([str(binary), "8"], tree, 300)
            if code == 0:
                code = run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                            "TOY_WIRE_STREAM=1", *SWITCHES, f"BUILD={tmp}/build",
                            "toy-wire-integration-host-test"], tree, 900)
            if code:
                print("FAILS   the tests without a mutation: nothing can be said of the list", flush=True)
                return 2
            print("passes  the tests without a mutation", flush=True)
        for path, name, old, new in MUTATIONS:
            original = (ROOT / path).read_text()
            if original.count(old) != 1:
                print(f"STALE   {name}: its source text occurs {original.count(old)} times", flush=True)
                survived.append(name)
                continue
            (tree / path).write_text(original.replace(old, new))
            if path == SCAN:
                binary = Path(tmp) / "scan"
                code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                            *[d for d in DEFINES if d.startswith("-DKUI_TOY_WIRE_TITLES")], "-Iinclude", "-Isrc/loader",
                            "tests/test_toy_titles_stage.c", "-o", str(binary)], tree, 120)
                if code == 0:
                    code = run([str(binary)], tree, 60)
                code = code or 0
                (tree / path).write_text(original)
                print(("caught  " if code else "MISSED  ") + name, flush=True)
                if not code:
                    survived.append(name)
                continue
            if path == REPORT:
                binary = Path(tmp) / "report"
                code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined", *DEFINES,
                            "-Iinclude", "-Isrc/loader", "tests/test_toy_wire_report.c", "-o", str(binary)], tree, 120)
                if code == 0:
                    code = run([str(binary)], tree, 60)
                (tree / path).write_text(original)
                print(("caught  " if code else "MISSED  ") + name, flush=True)
                if not code:
                    survived.append(name)
                continue
            if path in (STREAM, STREAM_H):
                binary = Path(tmp) / "stream"
                code = run([cc, "-std=c11", "-O1", "-g", "-w", "-fsanitize=address,undefined",
                            "-DKUI_WIRE_STREAM_TEST=1", *DEFINES, "-Iinclude", "-Isrc/loader",
                            "tests/test_toy_wire_stream.c", STREAM, "-o", str(binary)], tree, 120)
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
