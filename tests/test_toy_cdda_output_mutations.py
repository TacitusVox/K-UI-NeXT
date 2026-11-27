#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether Test 85's output test notices a defect in the CD-music output:
each of a list, in a copy of src/loader/toy_cdda_output.c, built and run on
its own with tests/test_toy_cdda_output.c.

usage: test_toy_cdda_output_mutations.py [--jobs N] [--work DIR] [--only TEXT] [--way 100|110|111]

Every mutation must still compile and must make the test fail. The test is
built with VOLUME and one of three ways: 16-bit samples in a place of the
table's (the default), ADPCM in one (--way 110), ADPCM in the driver's
spare memory (--way 111, Test 86's build). A mutation of code only some
builds have names them ({"VOLUME": 0}, {"ADPCM": 1}, {"SPARE": 0} ...) and
is passed over where another is asked for. Run from the repository root
(Makefile.vmu-native-toy-suite, toy-cdda-output-mutation-check).
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "src/loader/toy_cdda_output.c"
TEST = ROOT / "tests/test_toy_cdda_output.c"
FLAGS = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
         "-I" + str(ROOT / "include"), "-DKUI_TOY_CDDA_OUTPUT=1", "-DKUI_TOY_CDDA_OUTPUT_HOST_TEST=1"]
# (name, old, new) or (name, old, new, the switches it needs)
MUTATIONS = [
    # The bus
    ("SR.BL not refused", "    if(*sr & SR_BL) {\n", "    if(0) {\n"),
    ("the mask not raised", "    sr_write(*sr | 0xf0u);\n", "    sr_write(*sr);\n"),
    ("SR not given back", "    ++O.heartbeats;\n    sr_write(sr);\n", "    ++O.heartbeats;\n    (void)sr;\n"),
    ("SR not given back before a G2 transfer", "            ++O.skipped_dma;\n            sr_write(*sr);\n",
     "            ++O.skipped_dma;\n"),
    ("a DMA channel enabled not refused", "if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u) {",
     "if(read32(base + 0x18u) & 1u) {"),
    ("a DMA channel running not refused", "if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u) {",
     "if(read32(base + 0x14u) & 1u) {"),
    ("three DMA channels", "for(uint32_t dma = 0; dma < 4u; ++dma) {", "for(uint32_t dma = 0; dma < 3u; ++dma) {"),
    ("bursts of sixteen", "#define BURST 8u", "#define BURST 16u"),
    ("no FIFO wait before a burst", "    if(!(b->burst & (BURST - 1u)) && !drain(b))return false;",
     "    if(0 && !drain(b))return false;"),
    ("one read believed", "const uint32_t first = read32(address), second = read32(address);",
     "const uint32_t first = read32(address), second = first;"),
    ("ten times the polls", "#define POLL_LIMIT 40000u", "#define POLL_LIMIT 400000u"),
    ("ten times the time", "#define TICK_LIMIT 2344u", "#define TICK_LIMIT 23440u"),
    ("heartbeats a tenth of 2 ms apart", "#define SPACING 1628u", "#define SPACING 163u"),
    ("a silent play looked at ten times as often", "#define CADENCE 13021u", "#define CADENCE 1302u"),
    ("a silent play looked at a tenth as often", "#define CADENCE 13021u", "#define CADENCE 130210u"),
    ("the bus used with no play",
     "    if(!wanted && !tail && O.phase == KUI_TOY_CDDA_OUT_OFF && !O.started && !O.keyed && !O.asked)return;\n", "\n"),
    ("the clock's profile not checked", "    if(!kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {",
     "    if(0 && !kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {"),
    ("TCR0 read as a word", "tcr = read16(TCR0)", "tcr = read32(TCR0)"),
    # The store guard stops what no other line of the file would make: it is
    # held by a defect it stops, not by its own removal.
    ("the right plane beyond the ring", "    return O.ring + GUARD_BYTES + side * (plane_bytes(O.frames) + PLANE_REPEAT);",
     "    return O.ring + GUARD_BYTES + side * (2u * plane_bytes(O.frames) + PLANE_REPEAT);"),
    # The driver
    ("the reset bit ignored", "    if(reset & 1u) {\n        *why = KUI_TOY_CDDA_OUT_WHY_ARM;", "    if(0) {\n        *why = KUI_TOY_CDDA_OUT_WHY_ARM;"),
    ("any version of a date admitted", "        if(version != p->version || hash != p->date_hash ||",
     "        if(hash != p->date_hash ||"),
    ("any date of a version admitted", "        if(version != p->version || hash != p->date_hash ||",
     "        if(version != p->version ||"),
    ("a driver that is not running admitted", "           state != (p->family == KUI_TOY_CDDA_FAMILY_GEN2 ? EXEC : SEGA))continue;",
     "           0)continue;"),
    ("another admitted build taken for the same driver", "        if(O.family)return version == O.version;",
     "        if(O.family)return 1;"),
    ("MANATEE 2 on MANATEE 1's channels", "        O.channel[0] = top ? 62u : 61u;", "        O.channel[0] = 61u;"),
    ("MANATEE 1 on channel 63", "        O.channel[1] = top ? 63u : 62u;", "        O.channel[1] = 63u;"),
    # Its records
    ("a split with one stream port taken", "        if(streams < 2u || streams > 16u ||", "        if(streams > 16u ||"),
    ("the split's sum not checked", "(word & 255u) + (word >> 8 & 255u) + streams != 64u ||", ""),
    ("the stream ports' base not checked", "           other != UINT32_C(0x00800000) + 0x80u * (64u - streams))return 0;",
     "           0)return 0;"),
    ("the wrong port records", "0x13800u + 0x40u * (streams - 2u + side)", "0x13800u + 0x40u * (streams - 3u + side)"),
    ("a playing port taken for free", "            busy[side] = word & 1u;", "            busy[side] = false;"),
    ("the pool's entries 14 and 15", "const uint32_t entry = 13u + side, at = 0x12080u + 12u * entry;",
     "const uint32_t entry = 14u + side, at = 0x12080u + 12u * entry;"),
    ("a pool entry's base not checked", "        if(word != UINT32_C(0x00801800) + 0x80u * entry)return 0;", ""),
    ("an owned pool entry taken for free", "        used[side] = other != 0u;", "        used[side] = false;"),
    ("unproved records written to all the same", "    if(state == 2) {", "    if(0) {"),
    ("a used channel written to at the start",
     "    if(busy[0] || busy[1]) {\n        ++O.takes;\n        stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_TAKE, busy[0] | busy[1] << 1);",
     "    if(0) {\n        ++O.takes;\n        stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_TAKE, busy[0] | busy[1] << 1);"),
    # The ring's place
    ("the table not asked", "        if(address < at + bytes && address + size > at) {", "        if(0 && address < at + bytes) {",
     {"SPARE": 0}),
    ("a pair that begins below the ring not seen", "        if(address < at + bytes && address + size > at) {",
     "        if(address < at + bytes && address >= at) {", {"SPARE": 0}),
    ("an unexplained pair passed over", "        if(address >= MEMORY) {\n            *below = pair;", "        if(0) {\n            *below = pair;", {"SPARE": 0}),
    ("83 pairs", "#define TABLE_PAIRS 84u", "#define TABLE_PAIRS 63u", {"SPARE": 0}),
    ("CRI's slots first", "(O.place_try & 1u ? MEMORY : CRI)", "(O.place_try & 1u ? CRI : MEMORY)", {"SPARE": 0}),
    ("below the driver's end", "        if(below < LOWEST + bytes) {", "        if(below < bytes) {", {"SPARE": 0}),
    ("the smallest ring first", "plane_frames[] = {16384u, 8192u, 4096u};", "plane_frames[] = {4096u, 8192u, 16384u};", {"ADPCM": 0}),
    ("the smallest ADPCM ring first", "plane_frames[] = {65472u, 32768u, 16384u};", "plane_frames[] = {16384u, 32768u, 65472u};", {"ADPCM": 1, "SPARE": 0}),
    ("no guard lines written", "    if(guards(b, true))return 1;\n    O.placed = 0u;\n    return 0;", "    (void)b;\n    return 1;"),
    ("the guards not looked at", "    if(clear != 1 || !guards(b, false)) {", "    if(clear != 1) {"),
    ("the table not read again before a store", "    const int clear = table(b, O.ring, ring_bytes(), &detail);",
     "    const int clear = 1;", {"SPARE": 0}),
    ("a lost ring kept", "        O.placed = O.place_try = O.place_at = O.spare = 0u;\n        return;\n    }\n    if(O.phase",
     "        return;\n    }\n    if(O.phase"),
    ("a lost ring ends the play", "        else restart(b, busy);\n        O.placed = O.place_try",
     "        else stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_RING, 0u);\n        O.placed = O.place_try"),
    ("a ring lost again and again never given up", "        if(++O.rearms > REARMS)\n            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_RING,",
     "        if(0)\n            stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_RING,"),
    ("the ring not cleared", "        if(!wipe(b))return;\n", "        (void)wipe;\n"),
    # Keys, registers, levels
    ("no execute strobe at key-on", "!put(b, channel(1), key_word(1) | KEY_EXECUTE))return;",
     "!put(b, channel(1), key_word(1)))return;"),
    ("no loop", "#define KEY_ON 0x4200u", "#define KEY_ON 0x4000u"),
    ("the loop's end one frame early", "           !put(b, at + 0x0cu, O.frames))return;", "           !put(b, at + 0x0cu, O.frames - 1u))return;"),
    ("the loop's end one frame late", "           !put(b, at + 0x0cu, O.frames))return;", "           !put(b, at + 0x0cu, O.frames + 1u))return;"),
    ("no repeat of a plane's first frames", "           (!at && !put(b, SOUND + plane(side) + 2u * O.frames, word[side])))return false;",
     "           0)return false;", {"ADPCM": 0}),
    ("the planes' addresses exchanged", "        if(!put(b, at + 4u, plane(side) & 0xffffu) ||", "        if(!put(b, at + 4u, plane(!side) & 0xffffu) ||"),
    ("a channel left keyed on keyed on again", "        if(on[0] || on[1]) {", "        if(0) {"),
    ("the level left at a stop", "        if(ours[side])level_write(b, side, 0u);", "        if(0 && ours[side])level_write(b, side, 0u);"),
    ("the key left at a stop", "            (key_word(side) & ~KEY_BIT) | (side || !ours[1] ? KEY_EXECUTE : 0u)))break;",
     "            key_word(side) | (side || !ours[1] ? KEY_EXECUTE : 0u)))break;"),
    ("no execute strobe at a stop", "            (key_word(side) & ~KEY_BIT) | (side || !ours[1] ? KEY_EXECUTE : 0u)))break;",
     "            (key_word(side) & ~KEY_BIT)))break;"),
    ("a taken channel keyed off", "ours[side] = !busy[side] && held(b, side, &on) == 1;",
     "ours[side] = held(b, side, &on) >= -1 || busy[side];"),
    ("a channel with another address keyed off", "ours[side] = !busy[side] && held(b, side, &on) == 1;",
     "ours[side] = !busy[side] && held(b, side, &on) >= 0;"),
    ("another address not seen in play", "        if(!holds) {\n            ++O.takes;", "        if(0) {\n            ++O.takes;"),
    ("a dropped key not seen", "        if(!on) {\n", "        if(0) {\n"),
    ("a dropped key begun again for ever", "            if(++O.rearms > REARMS)", "            if(0)"),
    ("changed registers left", "            if(!put(b, channel(side) + at[i], want[i]))return;", ""),
    ("the level register not watched", "static const uint8_t at[] = {0x04u, 0x0cu, 0x18u, 0x20u, 0x24u};",
     "static const uint8_t at[] = {0x04u, 0x0cu, 0x18u, 0x20u, 0x20u};"),
    ("a moved play goes on where it was", "            if(i < 3u)moved = true;", "            if(0)moved = true;"),
    ("a pitch put back goes on", "            if(i < 3u)moved = true;", "            if(i < 2u)moved = true;"),
    ("the ring begun again for a level put back", "            if(i < 3u)moved = true;", "            if(i < 5u)moved = true;"),
    ("a play moved again and again never given up", "    if(moved) {\n        if(++O.rearms > REARMS)", "    if(moved) {\n        if(0)"),
    ("the mixers' pans copied", "    uint32_t level = value[0] & 0x0f00u;", "    uint32_t level = value[0] & 0x0f1fu;"),
    ("the smaller of the two levels", "    if((value[1] & 0x0f00u) > level)level = value[1] & 0x0f00u;",
     "    if((value[1] & 0x0f00u) < level)level = value[1] & 0x0f00u;"),
    ("the first mixer's level alone", "    if((value[1] & 0x0f00u) > level)level = value[1] & 0x0f00u;", ""),
    ("a pan with no level", "    O.level[1] = level ? level | (LEVEL_RIGHT & 0x1fu) : 0u;", "    O.level[1] = level | (LEVEL_RIGHT & 0x1fu);"),
    ("both planes on the left", "    O.level[1] = level ? level | (LEVEL_RIGHT & 0x1fu) : 0u;", "    O.level[1] = level ? level | (LEVEL_LEFT & 0x1fu) : 0u;"),
    ("the mixers' first words not kept", "        if(O.mixer[side] && !O.mixer_first[side])O.mixer_first[side] = O.mixer[side];",
     "        O.mixer_first[side] = O.mixer[side];"),
    ("the left mixer for both", "!stable(b, MIXER + 4u, &value[1]))return;", "!stable(b, MIXER, &value[1]))return;"),
    ("MANATEE 2's level bytes at once", "++O.mixer_zero < 8u)return;", "++O.mixer_zero < 1u)return;"),
    ("MANATEE 1 read for level bytes", "    if(O.family != KUI_TOY_CDDA_FAMILY_GEN2 || ++O.mixer_zero < 8u)return;",
     "    if(++O.mixer_zero < 8u)return;"),
    ("the right level byte not read", "    if(!(value[0] & UINT32_C(0x00ff00ff)))return;", "    if(!(value[0] & UINT32_C(0x000000ff)))return;"),
    ("the pan bytes read for a level", "    if(!(value[0] & UINT32_C(0x00ff00ff)))return;", "    if(!(value[0] & UINT32_C(0xffffffff)))return;"),
    ("without VOLUME, both hard left", "    (void)b;\n    O.level[0] = LEVEL_LEFT;\n    O.level[1] = LEVEL_RIGHT;",
     "    (void)b;\n    O.level[0] = LEVEL_LEFT;\n    O.level[1] = LEVEL_LEFT;", {"VOLUME": 0}),
    # The audio
    ("left and right exchanged", "{unit[0] | unit[1] << 8 | unit[4] << 16 | (uint32_t)unit[5] << 24,",
     "{unit[2] | unit[3] << 8 | unit[6] << 16 | (uint32_t)unit[7] << 24,", {"ADPCM": 0}),
    ("the second frame of a pair lost", "{unit[0] | unit[1] << 8 | unit[4] << 16 | (uint32_t)unit[5] << 24,",
     "{unit[0] | unit[1] << 8 | unit[0] << 16 | (uint32_t)unit[1] << 24,", {"ADPCM": 0}),
    ("a frame lost behind each unit", "            taken += 4u * UNIT;\n            continue;", "            taken += 4u * UNIT + 4u;\n            continue;"),
    ("a byte gathered twice", "        O.carry[O.carried++] = bytes ? bytes[taken] : 0u;\n        ++taken;",
     "        O.carry[O.carried++] = bytes ? bytes[taken] : 0u;\n        if(O.carried & 3u)++taken;"),
    ("a unit that waits is forgotten", "            if(!store(b, O.carry))return taken;\n            O.carried = 0u;",
     "            O.carried = 0u;"),
    ("the unit gathered not stored at the end", "    if(O.carried == 4u * UNIT && store(b, O.carry))O.carried = 0u;\n    return taken;",
     "    if(O.carried == 4u * UNIT)O.carried = 0u;\n    return taken;"),
    ("the frames gathered dropped at each piece", "    O.got = 0u;\n    O.card = KUI_TOY_CDDA_CARD_IDLE;",
     "    O.got = O.carried = 0u;\n    O.card = KUI_TOY_CDDA_CARD_IDLE;", {"ADPCM": 1}),
    ("the write position not begun again with the ring", "O.broken = O.at = O.carried = 0u;", "O.broken = O.carried = 0u;"),
    ("the ring written past its end", "    if(O.at >= O.frames)O.at = 0u;", "    if(O.at > O.frames)O.at = 0u;"),
    # SPARE
    ("the spare ring longer than its place", "    return settle(b, at, LOWEST - at - 2u * GUARD_BYTES);",
     "    return settle(b, at, LOWEST - at);", {"SPARE": 1}),
    ("MANATEE 2's spare ring a page lower", "#define SPARE_GEN2 UINT32_C(0x16000)", "#define SPARE_GEN2 UINT32_C(0x15000)", {"SPARE": 1}),
    ("MANATEE 1's spare ring a page lower", "#define SPARE_GEN1 UINT32_C(0x15000)", "#define SPARE_GEN1 UINT32_C(0x14000)", {"SPARE": 1}),
    ("MANATEE 1's spare memory under MANATEE 2", "O.family == KUI_TOY_CDDA_FAMILY_GEN2 ? SPARE_GEN2 : SPARE_GEN1",
     "O.family != KUI_TOY_CDDA_FAMILY_GEN2 ? SPARE_GEN2 : SPARE_GEN1", {"SPARE": 1}),
    ("the spare ring not said to be one", "    (void)detail;\n    O.spare = 1u;", "    (void)detail;", {"SPARE": 1}),
    # ADPCM
    ("the codes of a word in the other order", "        word |= code << shift;", "        word |= code << (28u - shift);", {"ADPCM": 1}),
    ("a step of the coder's table", "{230u, 230u, 230u, 230u, 307u, 409u, 512u, 614u}", "{230u, 230u, 230u, 230u, 307u, 409u, 512u, 615u}", {"ADPCM": 1}),
    ("a move by sixteenths", "        uint32_t move = step * (2u * (code & 7u) + 1u) >> 3;", "        uint32_t move = step * (2u * (code & 7u) + 1u) >> 4;", {"ADPCM": 1}),
    ("a move the chip's accounts differ on", "        while(move > 0x7fffu) {", "        while(0) {", {"ADPCM": 1}),
    ("the least step not kept", "        if(step < 127u)step = 127u;", "        if(step < 126u)step = 126u;", {"ADPCM": 1}),
    ("the largest step not kept", "        if(step > 24576u)step = 24576u;", "        if(step > 24577u)step = 24577u;", {"ADPCM": 1}),
    ("the signal not held to 16 bits", "        if(signal > 32767)signal = 32767;", "", {"ADPCM": 1}),
    ("a fall coded as a rise", "            code = 8u;\n            delta = -delta;", "            code = 0u;\n            delta = -delta;", {"ADPCM": 1}),
    ("the half step not tried", "        for(uint32_t bit = 4u; bit; bit >>= 1, part >>= 1)", "        for(uint32_t bit = 4u; bit; bit >>= 2, part >>= 2)", {"ADPCM": 1}),
    ("the left side's codes for both planes", "encode(&next[side], unit + 2u * side)", "encode(&next[side], unit)", {"ADPCM": 1}),
    ("the coders move on with a word not stored", "        next[side] = O.coder[side];\n", "", {"ADPCM": 1}),
    ("the coders not begun again with the ring", "        O.coder[0] = O.coder[1] = (struct kui_toy_cdda_coder){0, 127u};", "", {"ADPCM": 1}),
    ("ADPCM that begins again at the loop", "#define KEY_FORMAT 0x0180u", "#define KEY_FORMAT 0x0100u", {"ADPCM": 1}),
    ("a cleared ring that is not silence", "#define WIPE UINT32_C(0x80808080)", "#define WIPE UINT32_C(0x08080808)", {"ADPCM": 1}),
    ("eight frames a word, a quarter of a plane", "#define UNIT 8u                   /* frames a word of a plane holds */", "#define UNIT 4u", {"ADPCM": 1}),
    ("bytes out of their order taken", "    if(O.broken || offset != O.got) {", "    if(O.broken || (0 && offset != O.got)) {"),
    ("a gap's sectors two at a time", "    O.piece_end = O.track ? (end < D.end ? end : D.end) : O.sector + 1u;",
     "    O.piece_end = O.track ? (end < D.end ? end : D.end) : O.sector + 2u;"),
    ("a track read past the play's end", "    O.piece_end = O.track ? (end < D.end ? end : D.end) : O.sector + 1u;",
     "    O.piece_end = O.track ? end : O.sector + 1u;"),
    ("one pass more", "        if(O.passes && !--O.passes) {", "        if(O.passes && !O.passes--) {"),
    ("a refused track asked for again and again", "        if(!O.track || O.card == KUI_TOY_CDDA_CARD_REFUSED) {", "        if(!O.track) {"),
    ("the card asked twice a heartbeat", "            if(asked)return;\n", "            if(0 && asked)return;\n"),
    ("more taken than there is room", "            O.card = kui_toy_cdda_card_heartbeat(&want, 4u * room - O.carried, take, b);",
     "            O.card = kui_toy_cdda_card_heartbeat(&want, 4u * room + 2048u, take, b);"),
    ("no more asked for than the piece has left", "            O.card = kui_toy_cdda_card_heartbeat(&want, 4u * room - O.carried, take, b);",
     "            O.card = kui_toy_cdda_card_heartbeat(&want, 4u * room - O.carried < bytes - O.got ? 4u * room - O.carried : bytes - O.got, take, b);"),
    ("the card not given back", "            (void)kui_toy_cdda_card_heartbeat(&none, 0u, take, b);\n", "            (void)none;\n"),
    ("the drive's place a sector off", "    O.sector = D.first + (uint32_t)offset;", "    O.sector = D.first + (uint32_t)offset + 1u;"),
    ("a play that is over begun", "    if(D.passes && done >= D.passes)return false;", ""),
    # The clock
    ("883 frames in 15,625 ticks", "    return ticks / 15625u * 882u + ticks % 15625u * 882u / 15625u;",
     "    return ticks / 15625u * 883u + ticks % 15625u * 883u / 15625u;"),
    ("the ring's clock TMU0 again", "    O.played = counted;", "    O.played = timed;"),
    ("no margin for the count's own step", "#define MARGIN (COUNT_FRAMES + EDGE)", "#define MARGIN EDGE"),
    ("a count's frames a tenth short", "    return counts / 5u * 882u + counts % 5u * 882u / 5u;",
     "    return counts / 5u * 794u + counts % 5u * 794u / 5u;"),
    ("the count at key-on not kept", "        O.key_count = count;\n", ""),
    ("no witness since key-on", "    bool borne = apart(counted, timed) <= timed / 64u + SLACK;",
     "    bool borne = apart(counted, timed) <= timed + SLACK;"),
    ("a count that stands still not seen", "    if(counted == O.moved)borne = borne && lately <= SLACK;", "    if(counted == O.moved);"),
    ("a count that jumped not seen", "        borne = borne && apart(counted - O.moved, lately) <= lately / 64u + SLACK;\n", ""),
    ("the count's last move not kept", "        O.moved = counted;\n        O.moved_tick = tmu;", "        O.moved = counted;"),
    ("a count not borne out plays on", "    if(!borne) {", "    if(0) {"),
    ("a count not borne out never given up", "        if(++O.rearms > REARMS)stop(b, busy, KUI_TOY_CDDA_OUT_SILENT, KUI_TOY_CDDA_OUT_WHY_CLOCK, 2u);\n        else restart(b, busy);",
     "        restart(b, busy);"),
    ("the witness too strict for the console's clocks", "timed / 64u + SLACK;", "timed / 1024u + SLACK;"),
    ("a dry ring not seen", "    } else if(O.written < O.played + margin) {", "    } else if(0) {"),
    ("written over what is not yet played", "most = O.frames - margin - EDGE;", "most = O.frames + margin + EDGE;"),
    ("silence over what is not yet played", "        const uint32_t quiet = room < QUIET_FRAMES ? room : QUIET_FRAMES;",
     "        const uint32_t quiet = QUIET_FRAMES;"),
    ("no silence behind the last frame", "        if(O.finished) {\n            (void)silence(b, quiet);\n            return;",
     "        if(O.finished) {\n            return;"),
    ("stopped at the last frame by TMU0 alone", "        if(O.played >= O.audio_end + margin) {", "        if(O.played >= O.audio_end) {"),
    ("keyed on before the ring is primed", "        if(!b->ok || (O.written < prime && !O.finished))return;", "        if(!b->ok)return;"),
    # For the report
    ("the driver's clock not begun again when the driver is", "    if(!O.clocked || (int32_t)(chip - O.chip_first - O.chip_ticks) < 0) {",
     "    if(!O.clocked) {", {"SPARE": 1}),
    ("MANATEE 2's count read under MANATEE 1", "? COUNT_GEN2 : COUNT_GEN1;", "? COUNT_GEN2 : COUNT_GEN2;"),
    ("TMU0's ticks counted the other way", "    O.tmu_ticks = O.tmu_first - tmu;", "    O.tmu_ticks = tmu - O.tmu_first;"),
    ("the two clocks looked at once only", "    O.chip_ticks = chip - O.chip_first;", "    if(!O.chip_ticks)O.chip_ticks = chip - O.chip_first + 1u;"),
    ("no level kept behind the key-off", "    if(value)O.level_last[side] = value;", "    O.level_last[side] = value;"),
    ("the level at the key-on not kept", "        if(O.level[side])O.level_last[side] = O.level[side];\n", ""),
    # The play
    ("a paused play sounds on", "    bool wanted = D.state == KUI_TOY_CDDA_PLAYING &&", "    bool wanted = D.state != KUI_TOY_CDDA_IDLE &&"),
    ("a new play not begun", "(O.phase == KUI_TOY_CDDA_OUT_OFF || D.generation == O.generation);", "1;"),
    ("the last frames cut when the drive ends", "    bool tail = D.state == KUI_TOY_CDDA_ENDED && O.phase == KUI_TOY_CDDA_OUT_RUN &&\n        D.generation == O.generation + 1u;",
     "    bool tail = false;"),
    ("an ended play read on from the card", "        O.finished = 1u;\n        O.audio_end = O.written;\n    }\n    const uint32_t ahead", "    }\n    const uint32_t ahead"),
    ("the output on any stack", "    if(kui_vmu_native_passive_stack_room() < KUI_TOY_CDDA_OUTPUT_STACK) {\n        ++O.skipped_stack;",
     "    if(0) {\n        ++O.skipped_stack;"),
    ("the terminal on any stack", "    if(!O.keyed || kui_vmu_native_passive_stack_room() < KUI_TOY_CDDA_OUTPUT_STACK)return;",
     "    if(!O.keyed)return;"),
    ("a short stack not counted", "        ++O.skipped_stack;\n", "\n"),
    ("nothing at the terminal report", "    if(O.family && owned(&b, busy, &why) == 1)quiet(&b, busy);",
     "    if(0 && O.family && owned(&b, busy, &why) == 1)quiet(&b, busy);"),
    ("the terminal with SR.BL set", "    if(!bus_begin(&b, read32(TCNT0), &sr))return;\n    if(O.family && owned",
     "    (void)bus_begin(&b, read32(TCNT0), &sr);\n    if(O.family && owned"),
    ("another driver's channels keyed off", "            O.keyed = O.family = O.placed = O.place_try = O.place_at = O.spare = 0u;",
     "            O.family = O.placed = O.place_try = O.place_at = O.spare = 0u;busy[0] = busy[1] = false;"),
]


WAY = {"VOLUME": 1, "ADPCM": 0, "SPARE": 0}


def run(index, work):
    mutation = MUTATIONS[index]
    name, old, new = mutation[:3]
    switches = dict(WAY, **(mutation[3] if len(mutation) > 3 else {}))
    text = OUTPUT.read_text()
    if text.count(old) != 1:
        return index, "not applied (%d matches)" % text.count(old)
    directory = Path(work) / ("m%03d" % index)
    directory.mkdir(parents=True, exist_ok=True)
    mutated = directory / "toy_cdda_output.c"
    mutated.write_text(text.replace(old, new))
    binary = directory / "test"
    built = subprocess.run([os.environ.get("HOST_CC", "cc")] + FLAGS +
                           ["-DKUI_TOY_CDDA_%s=%d" % item for item in sorted(switches.items())] + [str(TEST), str(mutated), "-o", str(binary)], cwd=ROOT, capture_output=True, text=True)
    if built.returncode:
        (directory / "log.txt").write_text(built.stdout + built.stderr)
        return index, "did not build"
    try:
        result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=600,
                                env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    except subprocess.TimeoutExpired:
        return index, "did not end"
    log = result.stdout + result.stderr
    (directory / "log.txt").write_text(log)
    if result.returncode == 0:
        return index, "SURVIVED"
    if "test_toy_cdda_output.c:" in log or "AddressSanitizer" in log or "runtime error" in log:
        return index, "caught"
    return index, "failed otherwise"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--work")
    parser.add_argument("--only", help="run the mutations whose name holds this text")
    parser.add_argument("--way", default="100", choices=("100", "110", "111"), help="VOLUME, ADPCM, SPARE")
    args = parser.parse_args()
    WAY.update(VOLUME=int(args.way[0]), ADPCM=int(args.way[1]), SPARE=int(args.way[2]))
    work = args.work or tempfile.mkdtemp(prefix="toy-cdda-output-mutations-")
    print("mutations in", work)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        chosen = [i for i in range(len(MUTATIONS)) if (not args.only or args.only in MUTATIONS[i][0]) and
                  all(WAY[k] == v for k, v in (MUTATIONS[i][3] if len(MUTATIONS[i]) > 3 else {}).items() if k != "VOLUME")]
        results = sorted(pool.map(lambda i: run(i, work), chosen))
    bad = [(i, r) for i, r in results if r != "caught"]
    for i, r in results:
        print("%-9s %s" % (r, MUTATIONS[i][0]))
    if bad:
        sys.exit("FAIL %d of %d mutations not caught: %s" % (len(bad), len(results),
                 "; ".join("%s (%s)" % (MUTATIONS[i][0], r) for i, r in bad)))
    print("PASS Test 85 output mutations: all %d caught by the output test" % len(results))


if __name__ == "__main__":
    main()
