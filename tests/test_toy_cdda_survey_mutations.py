#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether Test 84's survey test notices a defect in the sound chip's survey:
each of a list, in a copy of src/loader/toy_cdda_survey.c, built and run on
its own.

usage: test_toy_cdda_survey_mutations.py [--jobs N] [--work DIR]

Every mutation must still compile and must make toy-cdda-survey-host-test
fail. Run from the repository root (Makefile.vmu-native-toy-suite,
toy-cdda-survey-mutation-check).
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SURVEY = ROOT / "src/loader/toy_cdda_survey.c"
MUTATIONS = [
    # The bus
    ("SR.BL not refused", "    if(sr & SR_BL) {\n", "    if(0) {\n"),
    ("the mask not raised", "    sr_write(sr | 0xf0u);\n", "    sr_write(sr);\n"),
    ("SR not given back", "        if(!b.ok)++V.timeouts;\n    }\n    sr_write(sr);\n}",
     "        if(!b.ok)++V.timeouts;\n    }\n}"),
    ("a DMA channel enabled not refused", "if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u)idle = false;",
     "if(read32(base + 0x18u) & 1u)idle = false;"),
    ("a DMA channel running not refused", "if((read32(base + 0x14u) | read32(base + 0x18u)) & 1u)idle = false;",
     "if(read32(base + 0x14u) & 1u)idle = false;"),
    ("three DMA channels", "for(uint32_t channel = 0; channel < 4u; ++channel) {",
     "for(uint32_t channel = 0; channel < 3u; ++channel) {"),
    ("no FIFO wait in the window", "        if(!drain(b))return;\n        for(uint32_t i = 0; i < BURST; ++i) {",
     "        if(0 && !drain(b))return;\n        for(uint32_t i = 0; i < BURST; ++i) {"),
    ("bursts of sixteen", "#define BURST 8u", "#define BURST 16u"),
    ("one read believed", "const uint32_t first = read32(address), second = read32(address);",
     "const uint32_t first = read32(address), second = first;"),
    ("ten times the polls", "#define POLL_LIMIT 10000u", "#define POLL_LIMIT 100000u"),
    ("ten times the time", "#define TICK_LIMIT 1563u", "#define TICK_LIMIT 15630u"),
    ("ten times as often", "#define SPACING 13021u", "#define SPACING 1302u"),
    ("surveyed while not playing", "    if(kui_toy_cdda_drive.state != KUI_TOY_CDDA_PLAYING)return;",
     "    if(kui_toy_cdda_drive.state == KUI_TOY_CDDA_IDLE)return;"),
    ("timeouts not counted", "        if(!b.ok)++V.timeouts;\n", "\n"),
    ("the clock's profile not checked", "    if(!kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {",
     "    if(0 && !kui_toy_pilot_clock_profile(frqcr, tcor, tcr, tstr)) {"),
    ("TCR0 read as a word", "tcr = read16(TCR0)", "tcr = read32(TCR0)"),
    # Identification
    ("the reset bit ignored", "    if(reset & 1u) {", "    if(0) {"),
    ("MANATEE 2 not running taken for running", "if(major == 2u)return state == EXEC ?", "if(major == 2u)return 1 ?"),
    ("NAMIHEI not running taken for running", "    if(!minor && state == SEGA)return KUI_TOY_CDDA_FAMILY_NAMIHEI;",
     "    if(!minor)return KUI_TOY_CDDA_FAMILY_NAMIHEI;"),
    ("MANATEE 1 asked for a mark its first build never writes",
     "    if(minor == 1u)return build >= 0x37u ?", "    if(minor == 1u && state == SEGA)return build >= 0x37u ?"),
    ("1.01.37 taken for older", "return build >= 0x37u ?", "return build > 0x37u ?"),
    ("a new driver of the same version not seen", "if(V.identified && (version != V.version || hash != V.date_hash))",
     "if(V.identified && version != V.version)"),
    ("the state word as identity", "if(V.identified && (version != V.version || hash != V.date_hash))",
     "if(V.identified && (version != V.version || hash != V.date_hash || state != V.state_word))"),
    ("the state word kept for every driver", "    V.state_word = manatee(family) ? state : 0u;",
     "    V.state_word = manatee(family) || 1 ? state : 0u;"),
    ("the family lost to a cut survey", "    if(!b->ok)return;\n    V.family = family;",
     "    V.family = family;\n    if(!b->ok)return;"),
    ("the date's bytes as one", "hash = (hash ^ (date >> 8 * i & 255u)) * UINT32_C(0x01000193);",
     "hash = (hash ^ (date & 255u)) * UINT32_C(0x01000193);"),
    # Candidates
    ("MANATEE 2's split unproved", " || (word & 255u) + (word >> 8 & 255u) + streams != 64u)return -1;", ")return -1;"),
    ("MANATEE 2's small split unproved", "        if(streams < 2u)return 0;", "        if(streams < 2u)return -1;"),
    ("MANATEE 2's fixed ports", "0x13800u + 0x40u * (streams - 2u + k)", "0x13800u + 0x40u * (6u + k)"),
    ("MANATEE 1's pool unproved", " || word != UINT32_C(0x00801800) + 0x80u * entry ||", " ||"),
    ("MANATEE 1's entries", "(*family == KUI_TOY_CDDA_FAMILY_GEN1 ? 13u : 14u)", "(*family == KUI_TOY_CDDA_FAMILY_GEN1 ? 14u : 14u)"),
    ("NAMIHEI's ports", "0x11200u + 0x40u * (6u + k)", "0x11200u + 0x40u * (5u + k)"),
    ("AUDIO64's base word unproved", "        } else return -1;", "        } else port = 62u;"),
    ("AUDIO64's table past the image", "word >= 0x10000u", "word > 0x10000u"),
    ("AUDIO64's table below the header", "word < 0x104u", "word < 0x100u"),
    ("AUDIO64's neighbour counted", "busy[0] = (other >> 16 & 255u) != 0u;", "busy[0] = (other >> 8 & 255u) != 0u;"),
    ("unproved on a timeout", "    if(!b->ok)return;\n    V.family = family;", "    V.family = family;"),
    # The window
    ("the window for every driver",
     "    if(family == KUI_TOY_CDDA_FAMILY_AUDIO64 || family == KUI_TOY_CDDA_FAMILY_HYBRID)window(b);",
     "    window(b);"),
    ("half a chunk counted", "    V.window_offset += WINDOW_WORDS * 4u;", "    V.window_offset += WINDOW_WORDS * 2u;"),
    ("no change marked", "    else if(V.window_first[kib] != folded)", "    else if(0)"),
    ("a later pass's word taken for the first's", "    if(!V.window_passes && nonzero)V.window_nonzero = 1u;",
     "    if(nonzero)V.window_nonzero = 1u;"),
    ("a cut chunk counted", "        if(!drain(b))return;\n        for(uint32_t i = 0; i < BURST; ++i) {",
     "        if(!drain(b))break;\n        for(uint32_t i = 0; i < BURST; ++i) {"),
]


def run(index, work):
    name, old, new = MUTATIONS[index]
    text = SURVEY.read_text()
    if text.count(old) != 1:
        return index, "not applied (%d matches)" % text.count(old)
    directory = Path(work) / ("m%02d" % index)
    directory.mkdir(parents=True, exist_ok=True)
    mutated = directory / "toy_cdda_survey.c"
    mutated.write_text(text.replace(old, new))
    env = dict(os.environ)
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)
    result = subprocess.run(["make", "-f", "Makefile.vmu-native-toy-suite", "TOY_SUITE_MODE=66",
                             "HOST_CC=" + env.get("HOST_CC", "cc"), "BUILD=" + str(directory / "build"),
                             "TOY_CDDA_SURVEY_SRC=" + str(mutated), "toy-cdda-survey-host-test"],
                            cwd=ROOT, env=env, capture_output=True, text=True)
    log = result.stdout + result.stderr
    (directory / "log.txt").write_text(log)
    if result.returncode == 0:
        return index, "SURVIVED"
    if "test_toy_cdda_survey.c:" in log or "AddressSanitizer" in log or "runtime error" in log:
        return index, "caught"
    return index, "did not build"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--work")
    args = parser.parse_args()
    work = args.work or tempfile.mkdtemp(prefix="toy-cdda-survey-mutations-")
    print("mutations in", work)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        results = sorted(pool.map(lambda i: run(i, work), range(len(MUTATIONS))))
    bad = [(i, r) for i, r in results if r != "caught"]
    for i, r in results:
        print("%-9s %s" % (r, MUTATIONS[i][0]))
    if bad:
        sys.exit("FAIL %d of %d mutations not caught: %s" % (len(bad), len(MUTATIONS),
                 "; ".join("%s (%s)" % (MUTATIONS[i][0], r) for i, r in bad)))
    print("PASS Test 84 survey mutations: all %d caught by the survey test" % len(MUTATIONS))


if __name__ == "__main__":
    main()
