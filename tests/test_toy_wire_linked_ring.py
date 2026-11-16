#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the linked cache-view routine of a Test 75 auxiliary.

Test 75 lets the card stream's block loop read the ring through the cache
(RING, src/loader/toy_wire_stream.c). The DMA channel writes the ring
behind the cache's back, so the rule that makes this safe is one line of
code: every cache line a word run is about to read is dropped from the
cache first. The host tests hold the C against a view of the ring that
never forgets a line. They cannot run the SH-4 code: the address
arithmetic and the cache instruction are the linked module's own.

This runs kui_wire_fresh as linked, with the small interpreter of
tests/sh4_mini.py, for rings at several addresses and for runs of every
alignment, length and place, the ring's two ends among them. For each call
it checks that

- a run shorter than a line comes back at its own uncached address and no
  cache instruction is executed;
- any other run comes back at the same bytes' cached (P1) address;
- the instructions executed on the cache are OCBI and nothing else, one for
  each cache line the run touches, each line once, none outside the run,
  none outside the ring;
- nothing is read from or written to the ring or anything else in memory
  but the routine's own stack and literals.

It checks the code as linked. It does not check the Dreamcast: that OCBI
drops a line and that the next read then comes from memory is the
processor's documented behaviour, not something this can show.

usage: test_toy_wire_linked_ring.py native-ra-aux.elf
"""
import random
import sys

from sh4_mini import Machine, Fault

RING_BYTES = 32768
STACK = 0x8C730000
MIN_WORDS = 8  # WIRE_FRESH_WORDS


def check(machine, entry, ring, offset, words):
    p = ring + offset
    machine.cache_ops.clear()
    machine.touched.clear()
    loads, stores = machine.loads, machine.stores
    result = machine.call(entry, ring, p, words, stack=STACK + 0x1000)
    ops = list(machine.cache_ops)
    where = f"ring {ring:#x} offset {offset} words {words}"
    stack_only = all(STACK <= (a | 0x80000000) < STACK + 0x1000 or
                     machine.code_low <= (a | 0x80000000) < machine.code_high for a in machine.touched)
    if not stack_only:
        raise AssertionError(f"{where}: memory outside the stack and the module was accessed")
    if words < MIN_WORDS:
        if result != p or ops:
            raise AssertionError(f"{where}: a short run must stay at its uncached address untouched")
        return 0
    view = (p & 0x1FFFFFFF) | 0x80000000
    if result != view:
        raise AssertionError(f"{where}: returned {result:#x}, the cached address is {view:#x}")
    if any(name != "ocbi" for name, _ in ops):
        raise AssertionError(f"{where}: a cache instruction other than OCBI: {ops[:4]}")
    dropped = [address & ~31 for _, address in ops]
    expected = list(range(view & ~31, view + 4 * words, 32))
    if sorted(dropped) != expected:
        raise AssertionError(f"{where}: dropped {len(dropped)} lines, the run touches {len(expected)}")
    low, high = (ring & 0x1FFFFFFF) | 0x80000000, ((ring & 0x1FFFFFFF) | 0x80000000) + RING_BYTES
    if any(not low <= line < high for line in dropped):
        raise AssertionError(f"{where}: a line outside the ring was dropped")
    return len(dropped)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    symbols = Machine.symbols(path)
    if "_kui_wire_fresh" not in symbols:
        print("FAIL: the module has no kui_wire_fresh (not a Test 75 build with RING)")
        return 1
    machine = Machine()
    machine.load_elf(path)
    machine.map(STACK, bytearray(0x1000))
    bases = [base for base, data in machine.regions]
    machine.code_low = min(b | 0x80000000 for b in bases if (b | 0x80000000) != STACK)
    machine.code_high = max((b | 0x80000000) + len(d) for b, d in machine.regions if (b | 0x80000000) != STACK)
    entry = symbols["_kui_wire_fresh"]
    rng = random.Random(75)
    calls = lines = 0
    try:
        # The ring is never mapped: any access to it would fault.
        for ring in (0xAC4A5000, 0xAC445000, 0xACFF8000, 0xAC010000):
            cases = []
            # Every alignment in a line with every short and some long lengths, at the ring's start,
            # inside it and up to its last byte.
            for start in (0, 4096, RING_BYTES - 4096):
                for lead in range(0, 32, 4):
                    for words in list(range(1, 41)) + [128, 129, 255, 256, 1000]:
                        if start + lead + 4 * words <= RING_BYTES:
                            cases.append((start + lead, words))
            for words in list(range(1, 41)) + [128, 513, 2048]:
                cases.append((RING_BYTES - 4 * words, words))  # ending at the ring's end
            cases.append((0, RING_BYTES // 4))  # the whole ring
            for _ in range(400):
                words = rng.choice((rng.randint(1, 16), rng.randint(8, 140), 128, rng.randint(1, 8192)))
                offset = 4 * rng.randint(0, (RING_BYTES - 4 * words) // 4)
                cases.append((offset, words))
            for offset, words in cases:
                lines += check(machine, entry, ring, offset, words)
                calls += 1
    except (Fault, AssertionError) as error:
        print(f"FAIL: {error}")
        return 1
    print(f"PASS linked cache view: {calls} runs, every line of a run dropped once and nothing else "
          f"({lines} lines); short runs left uncached; no memory but the stack touched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
