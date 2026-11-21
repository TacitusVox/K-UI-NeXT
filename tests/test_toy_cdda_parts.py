#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 84 changes the auxiliary, the worker and the cold stage; the low residents keep their bytes.

usage: test_toy_cdda_parts.py <Test 84 build> <Test 83's recipe, same BUILD_ID> [nm]

Both are built with one BUILD_ID (Makefile.vmu-native-toy-suite, toy-cdda-parts-check).
"""
import re
import struct
import subprocess
import sys
from pathlib import Path

# Imported by unittest discovery, this module defines no tests: the check runs
# only from the command line, with the two builds.

SAME = ("resident-sci.bin", "resident-legacy.bin", "worker.bin")
CHANGED = ("native-ra-aux.bin", "native-worker.bin", "stage.bin")


def section_bytes(elf, address, count, nm):
    objdump = nm.replace("nm", "objdump") if nm.endswith("nm") else "sh-elf-objdump"
    out = subprocess.run([objdump, "-s", "--start-address=%#x" % address, "--stop-address=%#x" % (address + count),
                          str(elf)], check=True, capture_output=True, text=True).stdout
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"^ ([0-9a-f]{8}) ((?:[0-9a-f]{2,8} ){1,4})", line)
        if m:
            data += bytes.fromhex(m.group(2).replace(" ", ""))
    return bytes(data[:count])


def symbols(elf, nm):
    out = {}
    for line in subprocess.run([nm, str(elf)], check=True, capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            out[parts[2]] = int(parts[0], 16)
    return out


def main(test84, test83, nm="sh-elf-nm"):
    for name in SAME:
        a, b = (test84 / name).read_bytes(), (test83 / name).read_bytes()
        if a != b:
            sys.exit(f"FAIL {name} is not Test 83's: {len(a)} bytes against {len(b)}")
    for name in CHANGED:
        if (test84 / name).read_bytes() == (test83 / name).read_bytes():
            sys.exit(f"FAIL {name} is Test 83's: Test 84 is not in it")
    # ROOM: the same span and stack top; the stack gives 2,304 bytes to code.
    s84, s83 = symbols(test84 / "native-ra-aux.elf", nm), symbols(test83 / "native-ra-aux.elf", nm)
    if (s84["__native_ra_stack_top"], s84["__native_ra_memory_end"]) != \
       (s83["__native_ra_stack_top"], s83["__native_ra_memory_end"]):
        sys.exit("FAIL the auxiliary's stack top or span moved")
    if s84["__native_ra_stack_bottom"] - s83["__native_ra_stack_bottom"] != 0x900 or \
       s84["__native_ra_image_end"] > s84["__native_ra_stack_bottom"] - 0x20000000:
        sys.exit("FAIL the auxiliary's code does not stop below its 1,792-byte stack")
    # HOOK: the player's descriptor right behind the loader timeline's, naming the player.
    w = symbols(test84 / "native-worker.elf", nm)
    hook = ""
    if "_kui_toy_cdda_player_exports" in w:
        at = w["_kui_toy_cdda_player_exports"]
        if at != w["_kui_vmu_native_trace_exports"] + 52 or at != w["_kui_native_loader_timeline_exports"] + 12:
            sys.exit("FAIL the player's descriptor is not 12 bytes behind the timeline's")
        words = struct.unpack("<3I", section_bytes(test84 / "native-worker.elf", at, 12, nm))
        if words != (0x4b434431, 1, w["_kui_toy_cdda_player_visit"]):
            sys.exit("FAIL the player's descriptor is %r" % (tuple(hex(x) for x in words),))
        hook = "; the worker's player descriptor names %#x at the trace table +52" % words[2]
    print("PASS Test 84's parts: both low residents and the observing worker are Test 83's; "
          "the auxiliary, the worker and the stage are its own; the auxiliary keeps its span and "
          "stack top, its code ends at %#x below the stack at %#x" %
          (s84["__native_ra_image_end"], s84["__native_ra_stack_bottom"]) + hook)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]), *sys.argv[3:])
