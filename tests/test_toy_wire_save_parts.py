#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 83 changes the cold stage and nothing else. Hold its other parts by their bytes.

usage: test_toy_wire_save_parts.py <Test 83 build> <the same recipe without TOY_WIRE_SAVE> [nm]

Both are built with one BUILD_ID (Makefile.vmu-native-toy-suite, toy-wire-save-parts-check).
"""
import subprocess
import sys
from pathlib import Path

# Imported by unittest discovery, this module defines no tests: the check runs
# only from the command line, with the two builds.

SAME = ("resident-sci.bin", "resident-legacy.bin", "native-ra-aux.bin", "native-ra-aux.reloc",
        "native-worker.bin", "native-worker.reloc", "worker.bin")
STAGE_ONLY = (b"SURVEY ON / REPORT SAVES TO SLOT", b"SURVEY OFF / NO KUI/SURVEY/STREAM.TXT",
              b"K-UI STREAM SURVEY FILE 1\nID ", b"==== SLOT ",
              # The stop screen's line for a launch that stopped after the survey wrote (retail_stage.c).
              b"ONLY THE SURVEY FILE WAS WRITTEN")


def symbol(elf, name, nm):
    for line in subprocess.run([nm, "-S", str(elf)], check=True, capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == name:
            return int(parts[0], 16), int(parts[1], 16)
    sys.exit(f"FAIL {elf}: no symbol {name}")


def main(survey, reader, nm="sh-elf-nm"):
    sizes = {}
    for name in SAME:
        a, b = (survey / name).read_bytes(), (reader / name).read_bytes()
        if a != b:
            sys.exit(f"FAIL {name} is not the reader's recipe's: {len(a)} bytes against {len(b)}")
        sizes[name] = len(a)
    stage, plain = (survey / "stage.bin").read_bytes(), (reader / "stage.bin").read_bytes()
    if stage == plain:
        sys.exit("FAIL the stage is the reader's recipe's: the survey is not in it")
    for text in STAGE_ONLY:
        if text not in stage:
            sys.exit(f"FAIL the stage lacks {text!r}")
        if text in plain:
            sys.exit(f"FAIL the reader's own stage has {text!r}")
        for name in SAME:
            if text in (survey / name).read_bytes():
                sys.exit(f"FAIL {name} has the stage's {text!r}")
    # The survey's own file is looked for, and the Windows CE data probe's is not.
    if b"SURVEY\0" not in stage or b"STREAM.TXT\0" not in stage or b"WCEDATA" in stage:
        sys.exit("FAIL the stage does not look for KUI/SURVEY/STREAM.TXT alone")
    # The stop screen keeps the reader's own line for a launch that wrote nothing.
    if b"STORAGE WAS READ ONLY" not in stage or b"STORAGE WAS READ ONLY" not in plain:
        sys.exit("FAIL the stop screen's line for a launch that wrote nothing is gone")
    # The survey's two sector buffers each begin a cache line, as the image reader's sector does: a
    # read into them takes the card bus's block path the same way wherever the link puts them.
    places = []
    for name in ("_toy_survey_work", "_toy_survey_text"):
        address, size = symbol(survey / "stage.elf", name, nm)
        if address % 32 or size != 512:
            sys.exit(f"FAIL the stage's {name} is {size} bytes at {address:#010x}: not a sector on a cache line")
        places.append(f"{address:#010x}")
    # Still a build that stops for the virtual VMU: its worker has none.
    if b"THIS TEST BUILD NEEDS VIRTUAL VMU OFF" not in stage:
        sys.exit("FAIL the stage no longer stops a launch that has the virtual VMU on")
    print(f"PASS Test 83's parts: the low residents ({sizes['resident-sci.bin']:,} and "
          f"{sizes['resident-legacy.bin']:,} bytes), the auxiliary ({sizes['native-ra-aux.bin']:,}) and the worker "
          f"({sizes['native-worker.bin']:,}) are the reader's own recipe's, byte for byte with one label; the stage "
          f"alone differs ({len(plain):,} -> {len(stage):,} bytes) and looks for KUI/SURVEY/STREAM.TXT; its two "
          f"sector buffers begin cache lines ({places[0]}, {places[1]})")


if __name__ == "__main__":
    main(*(Path(p) for p in sys.argv[1:3]), *sys.argv[3:4])
