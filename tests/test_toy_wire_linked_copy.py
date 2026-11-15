#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the linked copy routine of a Test 76 auxiliary.

Test 76 (COPY) copies each card block into the verified window with
kui_wire_copy (src/loader/toy_wire_copy.inc): single words up to the
destination's next cache line, then whole lines, each claimed in the cache
by MOVCA.L without being fetched, then single words again. No host test
runs it: the host builds copy with the C library.

This runs the routine as linked, with the small interpreter of
tests/sh4_mini.py, which takes MOVCA.L at its worst: the instruction's own
four bytes are stored and the other 28 of its line become undefined, as
they are when the line was not in the cache. To read such a byte is a
fault, and one left at the routine's return is a failure. For destinations
at every word of a line, sources at every word of a line, and every length
of whole words from none to more than a block, it checks that

- the destination holds the source's bytes, and the bytes just before and
  just behind it are as they were;
- the source is unchanged, and nothing is read but the source and nothing
  written but the destination (and the routine's own stack);
- MOVCA.L is the only cache instruction, used once for each cache line that
  lies wholly inside the destination, on that line's first word, and never
  on a line the copy does not wholly write;
- every word of a line is loaded before the line's first store, so that a
  source sharing the destination's place in the cache costs one exchange a
  line and not one a word;
- callee-saved registers and the stack pointer come back as they were.

It checks the code as linked. It does not check the Dreamcast: that
MOVCA.L claims a line without reading memory, and what a line costs there,
are the processor's own.

usage: test_toy_wire_linked_copy.py native-ra-aux.elf
"""
import random
import sys

from sh4_mini import Machine, Fault

STACK = 0x8C730000
OUT = 0x8C700000   # the destination's region, cached address as the window's
IN = 0x8C710000
GUARD = 64
SPAN = 2048


def run(machine, entry, out_at, in_at, count, rng):
    out, source = OUT + GUARD + out_at, IN + GUARD + in_at
    before = bytes(rng.randrange(256) for _ in range(SPAN))
    data = bytes(rng.randrange(256) for _ in range(SPAN))
    machine.out[:] = before
    machine.source[:] = data
    machine.cache_ops.clear()
    machine.touched.clear()
    machine.undefined.clear()
    machine.trace = []
    where = f"out +{out_at} in +{in_at} bytes {count}"
    try:
        machine.call(entry, out, source, count, stack=STACK + 0x1000)
    except Fault as fault:
        raise AssertionError(f"{where}: {fault}") from fault
    if machine.undefined:
        raise AssertionError(f"{where}: {len(machine.undefined)} bytes of a claimed line were never stored")
    if bytes(machine.source) != data:
        raise AssertionError(f"{where}: the source changed")
    lo = GUARD + out_at
    got = bytes(machine.out)
    if got[lo:lo + count] != data[GUARD + in_at:GUARD + in_at + count]:
        raise AssertionError(f"{where}: the destination does not hold the source's bytes")
    if got[:lo] != before[:lo] or got[lo + count:] != before[lo + count:]:
        raise AssertionError(f"{where}: bytes outside the destination changed")
    physical = Machine.physical
    for address in machine.touched:
        inside_out = physical(out) <= address < physical(out) + count
        inside_in = physical(source) <= address < physical(source) + count
        on_stack = physical(STACK) <= address < physical(STACK) + 0x1000
        in_code = physical(machine.code_low) <= address < physical(machine.code_high)
        if not (inside_out or inside_in or on_stack or in_code):
            raise AssertionError(f"{where}: memory at {address:#x} was touched")
    lines = [line for line in range((out + 31) & ~31, out + count - 31, 32)]
    ops = list(machine.cache_ops)
    if any(name != "movca.l" for name, _ in ops):
        raise AssertionError(f"{where}: a cache instruction other than MOVCA.L was executed")
    if [address for _, address in ops] != lines:
        raise AssertionError(f"{where}: MOVCA.L on {[hex(a) for _, a in ops]}, the whole lines are {[hex(a) for a in lines]}")
    return len(lines)


class Traced(Machine):
    """Notes, for each MOVCA.L, how many loads came since the store before it."""

    def __init__(self):
        super().__init__()
        self.since_store = 0
        self.line_loads = []

    def read(self, address, width):
        value = super().read(address, width)
        if Machine.physical(address) >= Machine.physical(IN) and Machine.physical(address) < Machine.physical(IN) + SPAN:
            self.since_store += 1
        return value

    def write(self, address, value, width):
        super().write(address, value, width)
        if Machine.physical(OUT) <= Machine.physical(address) < Machine.physical(OUT) + SPAN:
            if self.cache_ops and self.cache_ops[-1][1] == (address & 0xFFFFFFFF) and self.cache_ops[-1][0] == "movca.l":
                self.line_loads.append(self.since_store)
            self.since_store = 0


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    elf = sys.argv[1]
    machine = Traced()
    machine.load_elf(elf)
    symbols = Machine.symbols(elf)
    entry = symbols["_kui_wire_copy"]
    code = [base for base, store in machine.regions]
    machine.code_low = 0x8C000000 | min(code)
    machine.code_high = 0x8C000000 | max(base + len(store) for base, store in machine.regions)
    machine.map(STACK, bytes(0x1000))
    machine.out = machine.map(OUT, bytes(SPAN))
    machine.source = machine.map(IN, bytes(SPAN))
    rng = random.Random(76)
    runs = claimed = 0
    # Every word of a line for the destination and for the source, lengths
    # that end on every word of a line, none at all, under a line, and a
    # block's 512 with its neighbours.
    lengths = sorted(set(list(range(0, 132, 4)) + [252, 256, 260, 496, 500, 504, 508, 512, 516, 520, 524, 528, 540, 544]))
    for out_at in range(0, 32, 4):
        for in_at in range(0, 32, 4):
            for count in lengths:
                claimed += run(machine, entry, out_at, in_at, count, rng)
                runs += 1
    # The pieces the reader really makes: a sector's 2,352 bytes cut by
    # blocks of 512 at every multiple of sixteen.
    for part in range(0, 2352, 16):
        count = min(512, 2352 - part)
        for slot in (0, 16):
            out_at = (slot + part) % 32
            claimed += run(machine, entry, out_at, rng.randrange(0, 32, 4), count, rng)
            runs += 1
    if any(loads != 8 for loads in machine.line_loads) or len(machine.line_loads) != claimed:
        raise AssertionError("a line's eight words were not all loaded before its first store")
    # What a whole line costs: the loop's own instructions.
    machine.counts.clear()
    machine.executed = 0
    run(machine, entry, 0, 0, 512, rng)
    whole = machine.executed
    machine.counts.clear()
    machine.executed = 0
    run(machine, entry, 0, 0, 480, rng)
    per_line = whole - machine.executed
    if per_line != 20:
        raise AssertionError(f"a whole line takes {per_line} instructions, 20 are written")
    print(f"PASS kui_wire_copy as linked: {runs} copies exact, destinations and sources at every word of a line, "
          f"nothing outside the destination written; {claimed} lines claimed by MOVCA.L, each wholly inside its "
          f"destination and wholly stored, its eight words loaded first; {per_line} instructions a line, "
          f"{whole} for a block of 512")
    return 0


if __name__ == "__main__":
    sys.exit(main())
