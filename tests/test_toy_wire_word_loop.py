#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the linked block loop of a Test 71 auxiliary.

Test 71's change is what the compiler makes of wire_words() in
src/loader/toy_wire_stream.c: a loop that takes four bytes a turn and keeps
nothing in memory between turns. What it computes is checked on the host
against the byte steps; that it is the loop intended on the console can
only be read from the linked module. This counts the loop's instructions
and looks for stack accesses in it. It checks what was built, not how fast
the console runs it.

usage: test_toy_wire_word_loop.py native-ra-aux.elf
"""
import re
import subprocess
import sys


def main():
    elf = sys.argv[1]
    symbols = {}
    for line in subprocess.run(["sh-elf-nm", "-S", elf], check=True, capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 4:
            symbols[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    assert "_wire_words" in symbols, "the module has no word loop: not a Test 71 build"
    start, size = symbols["_wire_words"]
    text = subprocess.run(["sh-elf-objdump", "-d", f"--start-address={start:#x}", f"--stop-address={start + size:#x}", elf],
                          check=True, capture_output=True, text=True).stdout
    code = []
    for line in text.splitlines():
        m = re.match(r"^\s*([0-9a-f]{8}):\t[0-9a-f ]+\t(.*)$", line)
        if m:
            code.append((int(m.group(1), 16), re.sub(r"\s+", " ", re.sub(r"!.*$", "", m.group(2))).strip()))
    # The one backward branch closes the loop; its delay slot belongs to it.
    back = [(i, int(m.group(1), 16)) for i, (a, t) in enumerate(code)
            for m in [re.match(r"b[tf](?:\.s|/s)? ([0-9a-f]{8})", t)] if m and int(m.group(1), 16) <= a]
    assert len(back) == 1, [code[i] for i, _ in back]
    at, target = back[0]
    first = next(i for i, (a, _) in enumerate(code) if a == target)
    loop = [t for _, t in code[first:at + 2]]
    assert not any("r15" in t for t in loop), [t for t in loop if "r15" in t]
    loads = [t for t in loop if re.match(r"mov\.[bwl] @", t)]
    stores = [t for t in loop if re.match(r"mov\.[bwl] r\d+,@", t)]
    # One word from the ring, four CRC table words, four reversed bytes; four bytes out.
    assert len([t for t in loads if t.startswith("mov.l")]) == 1, loads
    assert len([t for t in loads if t.startswith("mov.w")]) == 4, loads
    assert len([t for t in loads if t.startswith("mov.b")]) == 4, loads
    assert len(stores) == 4 and all(t.startswith("mov.b") for t in stores), stores
    assert len(loop) <= 52, len(loop)
    print(f"PASS word loop at {target:#x}: {len(loop)} instructions for four bytes, no stack access, "
          f"1 ring word, 4 CRC table loads, 4 reversed bytes stored")


if __name__ == "__main__":
    main()
