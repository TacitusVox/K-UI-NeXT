#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether Test 85's output test notices a defect in the CD-music output:
each of a list, in a copy of src/loader/toy_cdda_output.c, built and run on
its own with tests/test_toy_cdda_output.c.

usage: test_toy_cdda_output_mutations.py [--jobs N] [--work DIR]

Every mutation must still compile and must make the test fail. Run from the repository root
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
# (name, old, new)
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
    ("the right plane beyond the ring", "    return O.ring + GUARD_BYTES + side * (2u * O.frames + PLANE_REPEAT);",
     "    return O.ring + GUARD_BYTES + side * (4u * O.frames + PLANE_REPEAT);"),
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
    ("the table not asked", "        if(address < at + bytes && address + size > at) {", "        if(0 && address < at + bytes) {"),
    ("a pair that begins below the ring not seen", "        if(address < at + bytes && address + size > at) {",
     "        if(address < at + bytes && address >= at) {"),
    ("an unexplained pair passed over", "        if(address >= MEMORY) {\n            *below = pair;", "        if(0) {\n            *below = pair;"),
    ("83 pairs", "#define TABLE_PAIRS 84u", "#define TABLE_PAIRS 63u"),
    ("CRI's slots first", "(O.place_try & 1u ? MEMORY : CRI)", "(O.place_try & 1u ? CRI : MEMORY)"),
    ("below the driver's end", "        if(below < LOWEST + bytes) {", "        if(below < bytes) {"),
    ("the smallest ring first", "plane_frames[] = {16384u, 8192u, 4096u};", "plane_frames[] = {4096u, 8192u, 16384u};"),
    ("no guard lines written", "            if(guards(b, true))return 1;", "            return 1;"),
    ("the guards not looked at", "    if(clear != 1 || !guards(b, false)) {", "    if(clear != 1) {"),
    ("the table not read again before a store", "    const int clear = table(b, O.ring, ring_bytes(), &detail);",
     "    const int clear = 1;"),
    ("a lost ring kept for the next play", "        O.placed = O.place_try = O.place_at = 0u;\n        return;\n    }\n    if(O.phase",
     "        return;\n    }\n    if(O.phase"),
    ("the ring not cleared", "        if(!wipe(b))return;\n", "        (void)wipe;\n"),
    # Keys, registers, levels
    ("no execute strobe at key-on", "!put(b, channel(1), key_word(1) | KEY_EXECUTE))return;",
     "!put(b, channel(1), key_word(1)))return;"),
    ("no loop", "#define KEY_ON 0x4200u", "#define KEY_ON 0x4000u"),
    ("the loop's end one frame early", "           !put(b, at + 0x0cu, O.frames))return;", "           !put(b, at + 0x0cu, O.frames - 1u))return;"),
    ("the loop's end one frame late", "           !put(b, at + 0x0cu, O.frames))return;", "           !put(b, at + 0x0cu, O.frames + 1u))return;"),
    ("no repeat of a plane's first frames", "           (!at && !put(b, SOUND + plane(side) + 2u * O.frames, word[side])))return false;",
     "           0)return false;"),
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
    ("both hard left", "    (void)b;\n    O.level[0] = LEVEL_LEFT;\n    O.level[1] = LEVEL_RIGHT;",
     "    (void)b;\n    O.level[0] = LEVEL_LEFT;\n    O.level[1] = LEVEL_LEFT;"),
    # The audio
    ("left and right exchanged", "{pair[0] | pair[1] << 8 | pair[4] << 16 | (uint32_t)pair[5] << 24,",
     "{pair[2] | pair[3] << 8 | pair[6] << 16 | (uint32_t)pair[7] << 24,"),
    ("the second frame of a pair lost", "{pair[0] | pair[1] << 8 | pair[4] << 16 | (uint32_t)pair[5] << 24,",
     "{pair[0] | pair[1] << 8 | pair[0] << 16 | (uint32_t)pair[1] << 24,"),
    ("a byte lost on the way", "    O.got += count;\n    for(; count; --count) {", "    O.got += count;\n    for(--count; count; --count) {"),
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
    ("no margin for the clocks' drift", "    return frames / 5000u + EDGE;", "    return frames * 0u + EDGE;"),
    ("the ring never begun again for drift", "    } else if(margin > O.frames / 8u) {", "    } else if(0) {"),
    ("a dry ring not seen", "    } else if(O.written < O.played + margin) {", "    } else if(0) {"),
    ("written over what is not yet played", "most = O.frames - margin - EDGE;", "most = O.frames + margin + EDGE;"),
    ("silence over what is not yet played", "        const uint32_t quiet = room < QUIET_FRAMES ? room : QUIET_FRAMES;",
     "        const uint32_t quiet = QUIET_FRAMES;"),
    ("no silence behind the last frame", "        if(O.finished) {\n            (void)silence(b, quiet);\n            return;",
     "        if(O.finished) {\n            return;"),
    ("stopped at the last frame by TMU0 alone", "        if(O.played >= O.audio_end + margin) {", "        if(O.played >= O.audio_end) {"),
    ("keyed on before the ring is primed", "        if(!b->ok || (O.written < prime && !O.finished))return;", "        if(!b->ok)return;"),
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
    ("another driver's channels keyed off", "            O.keyed = O.family = O.placed = O.place_try = O.place_at = 0u;",
     "            O.family = O.placed = O.place_try = O.place_at = 0u;busy[0] = busy[1] = false;"),
]


def run(index, work):
    mutation = MUTATIONS[index]
    name, old, new = mutation
    text = OUTPUT.read_text()
    if text.count(old) != 1:
        return index, "not applied (%d matches)" % text.count(old)
    directory = Path(work) / ("m%03d" % index)
    directory.mkdir(parents=True, exist_ok=True)
    mutated = directory / "toy_cdda_output.c"
    mutated.write_text(text.replace(old, new))
    binary = directory / "test"
    built = subprocess.run([os.environ.get("HOST_CC", "cc")] + FLAGS + [str(TEST), str(mutated), "-o", str(binary)],
                           cwd=ROOT, capture_output=True, text=True)
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
    args = parser.parse_args()
    work = args.work or tempfile.mkdtemp(prefix="toy-cdda-output-mutations-")
    print("mutations in", work)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        chosen = [i for i in range(len(MUTATIONS)) if not args.only or args.only in MUTATIONS[i][0]]
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
