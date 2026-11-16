#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the linked visit gate of a Toy wire-stream auxiliary.

Test 70 uses the auxiliary's private stack through the cache: with the MMU
off the gate turns the linked uncached (P2) stack address into the P0
address of the same page, and reads the two guard words through that same
address. No host test can run SH-4 code or model the cache, so this reads
the linked module instead: the gate's instructions, in order, and its
literal words. It checks what was built, not how the console behaves.

usage: test_toy_wire_cached_stack.py native-ra-aux.elf {cached|uncached}
"""
import re
import subprocess
import sys


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def main():
    elf, want = sys.argv[1], sys.argv[2]
    assert want in ("cached", "uncached")
    symbols = {}
    for line in run("sh-elf-nm", elf).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    gate = symbols["_kui_native_readahead_aux_visit"]
    bottom, top = symbols["__native_ra_stack_bottom"], symbols["__native_ra_stack_top"]
    # The stack is linked at its uncached address either way: the stage
    # checks and seeds it there.
    assert bottom >> 28 == 0xa and top - bottom == 0x1000, (hex(bottom), hex(top))
    text = run("sh-elf-objdump", "-d", f"--start-address={gate:#x}", f"--stop-address={gate + 0x120:#x}", elf)
    code, literal_at = [], {}
    for line in text.splitlines():
        m = re.match(r"^\s*([0-9a-f]{8}):\t([0-9a-f]{2}) ([0-9a-f]{2}) +\t(.*)$", line)
        if m:
            address = int(m.group(1), 16)
            literal_at[address] = int(m.group(3) + m.group(2), 16)
            # Without the value objdump notes behind a literal load.
            code.append((address, re.sub(r"\s+", " ", re.sub(r"!.*$", "", m.group(4))).strip()))
    def word(address):
        return literal_at[address] | literal_at[address + 2] << 16
    # Up to the first return: the path every admitted visit takes.
    end = next(i for i, (_, t) in enumerate(code) if t.startswith("rts"))
    body = [t for _, t in code[:end + 2]]
    loads = {}
    for address, t in code[:end]:
        m = re.match(r"mov\.l ([0-9a-f]{8}) <[^>]*>,(r\d+)", t)
        if m:
            loads.setdefault(word(int(m.group(1), 16)), []).append((address, m.group(2)))
    def index(pattern, start=0):
        for i in range(start, len(body)):
            if re.fullmatch(pattern, body[i]):
                return i
        raise AssertionError(f"gate lacks {pattern!r} after instruction {start}")
    assert bottom in loads and top - 16 in loads and top - 32 in loads, [hex(v) for v in loads]
    assert len(loads[top - 32]) == 1 and loads[top - 32][0][1] == "r15"
    assert len(loads[bottom]) == 2 and len(loads[top - 16]) == 2, "guards are checked on entry and on return"
    call = index(r"jsr @r0")
    if want == "uncached":
        assert 0xff000010 not in loads and 0xa0000000 not in loads
        assert not any("r12" in t for t in body)
        print(f"PASS uncached gate at {gate:#x}: stack {top - 32:#x}, guards {bottom:#x} and {top - 16:#x}")
        return
    # The MMU's control register decides the address: enabled, nothing is subtracted.
    assert [r for _, r in loads.get(0xff000010, [])] == ["r1"], "the gate does not read the MMU's control register"
    assert [r for _, r in loads.get(0xa0000000, [])] == ["r3"]
    at = index(r"mov\.l [0-9a-f]{8} <[^>]*>,r3")
    at = index(r"mov #1,r1", at)
    at = index(r"tst r1,r2", at)
    at = index(r"bt [0-9a-f]{8} <[^>]*>", at)
    assert body[at + 1] == "mov #0,r3", body[at + 1]
    # Both guards on entry through the stack's own address, before anything is written.
    first_push = index(r"mov\.l r8,@-r15")
    entry = body[at + 2:first_push]
    assert entry.count("sub r3,r1") == 2 and entry.count("mov.l @r1,r2") == 2, entry
    assert not any("@-r15" in t or ",@r" in t for t in entry), "nothing is written before the guards are read"
    for i, t in enumerate(entry):
        if t == "mov.l @r1,r2":
            assert entry[i - 1] == "sub r3,r1", entry
    # The adjustment survives the call in R12, which is saved and restored.
    pushes = [t for t in body[first_push:call] if "@-r15" in t]
    assert pushes == ["mov.l r8,@-r15", "mov.l r9,@-r15", "mov.l r10,@-r15", "mov.l r11,@-r15",
                      "mov.l r12,@-r15", "sts.l pr,@-r15"], pushes
    assert "mov r3,r12" in body[first_push:call]
    stack = index(r"mov\.l [0-9a-f]{8} <[^>]*>,r15", first_push)
    assert stack < call and body[stack + 1] == "sub r12,r15", body[stack:stack + 2]
    after = body[call + 2:]
    assert after.count("sub r12,r1") == 2
    assert sum(1 for i, t in enumerate(after) if t == "mov.l @r1,r2" and after[i - 1] == "sub r12,r1") == 2
    pops = [t for t in after if "@r15+" in t]
    assert pops == ["lds.l @r15+,pr", "mov.l @r15+,r12", "mov.l @r15+,r11", "mov.l @r15+,r10",
                    "mov.l @r15+,r9", "mov.l @r15+,r8"], pops
    # The arguments of the visit reach the dispatcher untouched.
    assert not any(re.search(r",r[4-7]$", t) for t in body[:call]), "R4..R7 carry the visit's arguments"
    cached = 0xa0000000
    print(f"PASS cached gate at {gate:#x}: with the MMU off the stack is used at {top - 32 - cached:#010x} "
          f"and the guards read at {bottom - cached:#010x} and {top - 16 - cached:#010x}; "
          f"with it on at {top - 32:#x}, {bottom:#x} and {top - 16:#x} as before")


if __name__ == "__main__":
    main()
