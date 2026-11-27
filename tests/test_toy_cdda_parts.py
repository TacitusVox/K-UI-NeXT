#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 84 changes the auxiliary, the worker and the cold stage; the low residents keep their bytes.

usage: test_toy_cdda_parts.py <Test 84 build> <Test 83's recipe, same BUILD_ID> [--card <Test 84's recipe>]
                              [--yield <Test 85's recipe without YIELD>] [nm]

Both are built with one BUILD_ID (Makefile.vmu-native-toy-suite, toy-cdda-parts-check).

Test 85 (CARD) is held against both: it changes the same three parts again,
and no other. The auxiliary gains the audio's card work, the worker its
heartbeat, the consumer and the block the two share, and the stage the proof
of that block's place, which it hands to the auxiliary; both low residents
and the observing worker are Test 83's and Test 84's byte for byte. The
worker's player descriptor has its second form: four words, the last naming
the shared block, which lies in the worker's cleared state on whole cache
lines at its uncached address (include/kui/toy_cdda_mail.h).

Test 85 (YIELD) changes the auxiliary alone: held against the same recipe
without it, the worker keeps its bytes as well, and the stage differs only as
the part that carries the auxiliary.
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


def sizes(elf, nm):
    out = {}
    for line in subprocess.run([nm, "-S", str(elf)], check=True, capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 4:
            out[parts[3]] = int(parts[1], 16)
    return out


MAIL_BYTES = 1312


def main(test84, test83, nm="sh-elf-nm", card=None, yielding=None):
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
    mail = w.get("_kui_toy_cdda_mail")
    if card is not None and mail is None:
        sys.exit("FAIL a build held against Test 84's has no shared block: it is not Test 85's")
    if "_kui_toy_cdda_player_exports" in w:
        at = w["_kui_toy_cdda_player_exports"]
        if at != w["_kui_vmu_native_trace_exports"] + 52 or at != w["_kui_native_loader_timeline_exports"] + 12:
            sys.exit("FAIL the player's descriptor is not 12 bytes behind the timeline's")
        count = 3 if mail is None else 4
        words = struct.unpack("<%dI" % count, section_bytes(test84 / "native-worker.elf", at, 4 * count, nm))
        expected = (0x4b434431, 1, w["_kui_toy_cdda_player_visit"]) if mail is None else \
            (0x4b434431, 2, w["_kui_toy_cdda_player_visit"], mail)
        if words != expected:
            sys.exit("FAIL the player's descriptor is %r" % (tuple(hex(x) for x in words),))
        hook = "; the worker's player descriptor names %#x at the trace table +52" % words[2]
    if mail is not None:
        # CARD: the shared block as the stage will prove it and the auxiliary reach it.
        begin, end = w["__vmu_native_worker_bss_begin"], w["__vmu_native_worker_bss_end"]
        size = sizes(test84 / "native-worker.elf", nm).get("_kui_toy_cdda_mail")
        if size != MAIL_BYTES:
            sys.exit("FAIL the shared block is %r bytes, not %d" % (size, MAIL_BYTES))
        if mail & 31 or mail >> 29 != 5 or begin >> 29 != 5 or not begin <= mail or mail + size > end:
            sys.exit("FAIL the shared block at %#x is not whole cache lines of the worker's cleared state "
                     "[%#x, %#x) at its uncached address" % (mail, begin, end))
        if "_kui_toy_cdda_player_exports" not in w:
            sys.exit("FAIL the shared block is named by no descriptor")
        hook += "; its fourth word names the shared block, %d bytes at %#x in the cleared state [%#x, %#x)" % (
            size, mail, begin, end)
    if card is not None:
        # CARD against Test 84's own build: the same three parts, and no other.
        for name in SAME:
            if (test84 / name).read_bytes() != (card / name).read_bytes():
                sys.exit(f"FAIL {name} is not Test 84's")
        for name in CHANGED:
            if (test84 / name).read_bytes() == (card / name).read_bytes():
                sys.exit(f"FAIL {name} is Test 84's: Test 85 is not in it")
        c = symbols(card / "native-ra-aux.elf", nm)
        if (s84["__native_ra_stack_top"], s84["__native_ra_stack_bottom"], s84["__native_ra_memory_end"]) != \
           (c["__native_ra_stack_top"], c["__native_ra_stack_bottom"], c["__native_ra_memory_end"]):
            sys.exit("FAIL the auxiliary's stack or span is not Test 84's")
        if "_kui_toy_cdda_mail" in symbols(card / "native-worker.elf", nm):
            sys.exit("FAIL the build named as Test 84's has the shared block")
        hook += "; against Test 84's own build the same three parts differ and the stack is where it was, " \
                "%d bytes of the auxiliary's room used by the audio" % (
                    s84["__native_ra_image_end"] - c["__native_ra_image_end"])
    if yielding is not None:
        # YIELD against CARD's own build: the auxiliary, and the stage that carries it.
        for name in SAME + ("native-worker.bin",):
            if (test84 / name).read_bytes() != (yielding / name).read_bytes():
                sys.exit(f"FAIL {name} is not Test 85's without YIELD")
        for name in ("native-ra-aux.bin", "stage.bin"):
            if (test84 / name).read_bytes() == (yielding / name).read_bytes():
                sys.exit(f"FAIL {name} is Test 85's without YIELD: YIELD is not in it")
        y = symbols(yielding / "native-ra-aux.elf", nm)
        if (s84["__native_ra_stack_top"], s84["__native_ra_stack_bottom"], s84["__native_ra_memory_end"]) != \
           (y["__native_ra_stack_top"], y["__native_ra_stack_bottom"], y["__native_ra_memory_end"]):
            sys.exit("FAIL the auxiliary's stack or span is not Test 85's without YIELD")
        hook += "; against this recipe without YIELD the worker keeps its bytes and the auxiliary differs, " \
                "by %d bytes of its room" % (s84["__native_ra_image_end"] - y["__native_ra_image_end"])
    print("PASS Test 84's parts: both low residents and the observing worker are Test 83's; "
          "the auxiliary, the worker and the stage are its own; the auxiliary keeps its span and "
          "stack top, its code ends at %#x below the stack at %#x" %
          (s84["__native_ra_image_end"], s84["__native_ra_stack_bottom"]) + hook)


if __name__ == "__main__":
    arguments = sys.argv[1:]
    named = {}
    for option in ("--card", "--yield"):
        if option in arguments:
            at = arguments.index(option)
            if at + 1 >= len(arguments):
                sys.exit(__doc__)
            named[option] = Path(arguments[at + 1])
            del arguments[at:at + 2]
    if len(arguments) not in (2, 3):
        sys.exit(__doc__)
    main(Path(arguments[0]), Path(arguments[1]), *arguments[2:], card=named.get("--card"), yielding=named.get("--yield"))
