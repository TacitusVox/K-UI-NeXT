#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the linked block loops of a Test 74 auxiliary on test data.

Test 74 writes the card stream's block loop by hand, once for a receiver
that delivers bytes LSB first and once for one that delivers them as sent
(src/loader/toy_wire_words.inc). The host tests cannot run SH-4 code: they
run the C loops these replace. This runs the two routines as linked, with
the small interpreter of tests/sh4_mini.py, against the byte steps they
stand for: random words, lengths and remainders, every alignment of the
destination, with guards around everything they may write. It also counts
what a turn of each loop executes and touches.

With a second module, one that still has the compiler's loop (a Test 71 to
73 auxiliary), the same data is put through that loop too: the interpreter
then agrees with the compiler's own code on the same computation, which is
the check on the interpreter.

It checks the code as linked. It does not check the Dreamcast.

usage: test_toy_wire_linked_loops.py native-ra-aux.elf [reference-aux.elf]
"""
import random
import sys

from sh4_mini import Machine, Fault

TABLES = 0x8C700000   # the stream's table block, as wire_tables lays it out
RING = 0x8C710000     # ring words (the real ring is read at its uncached address; one memory here)
OUT = 0x8C720000      # where the bytes go
STACK = 0x8C730000
FAKE_W = 0x8C740000   # for the compiler's loop: a stream structure with the three table pointers


def bit_reverse(value):
    result = 0
    for bit in range(8):
        if value >> bit & 1:
            result |= 0x80 >> bit
    return result


def build_tables(true_order):
    """reverse[256], inverse[256], step[256] and pair[256] as wire_tables builds them."""
    reverse, step = [], []
    for i in range(256):
        if true_order:
            c = i << 8
            for _ in range(8):
                c = ((c << 1) ^ 0x1021 if c & 0x8000 else c << 1) & 0xFFFF
            c = (c >> 8 | c << 8) & 0xFFFF
            reverse.append(i)
        else:
            c = i
            for _ in range(8):
                c = (c >> 1) ^ 0x8408 if c & 1 else c >> 1
            reverse.append(bit_reverse(i))
        step.append(c)
    inverse = [0] * 256
    for i, c in enumerate(step):
        inverse[c >> 8] = i
    pair = [(c >> 8) ^ step[c & 0xFF] for c in step]
    blob = bytes(reverse) + bytes(inverse)
    for table in (step, pair):
        for entry in table:
            blob += entry.to_bytes(2, "little")
    return reverse, step, blob


def byte_steps(data, crc, reverse, step):
    """What the loops stand for: wire_run's STEP, a byte at a time."""
    out = bytearray()
    for byte in data:
        crc = (crc >> 8) ^ step[(crc ^ byte) & 0xFF]
        out.append(reverse[byte])
    return crc, bytes(out)


def standard_crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


class Harness:
    def __init__(self, path):
        self.m = Machine()
        self.m.load_elf(path)
        self.symbols = Machine.symbols(path)
        self.tables = self.m.map(TABLES, bytes(1536))
        self.ring = self.m.map(RING, bytes(4096))
        self.out = self.m.map(OUT, bytes(4096))
        self.stack = self.m.map(STACK, bytes(4096))
        self.fake = self.m.map(FAKE_W, bytes(256))

    def run(self, name, data, crc, out_shift, ring_word, compiled):
        """One call. Returns (remainder, bytes written, instructions, loads, stores)."""
        m = self.m
        words = len(data) // 4
        self.ring[:] = bytes([0x5A]) * len(self.ring)
        self.ring[4 * ring_word:4 * ring_word + len(data)] = data
        self.out[:] = bytes([0xA5]) * len(self.out)
        out_at = 64 + out_shift
        self.stack[:] = bytes(len(self.stack))
        sp = STACK + 2048
        m.r = [0xDEAD0000 + i for i in range(16)]
        if compiled:
            # uint32_t wire_words(const struct kui_wire_stream *w, p, words, out, crc)
            self.fake[124:128] = TABLES.to_bytes(4, "little")
            self.fake[128:132] = (TABLES + 512).to_bytes(4, "little")
            self.fake[160:164] = (TABLES + 1024).to_bytes(4, "little")
            self.stack[2048:2052] = crc.to_bytes(4, "little")
            args = (FAKE_W, RING + 4 * ring_word, words, OUT + out_at)
        else:
            # uint32_t kui_wire_words_*(p, words, out, tables, crc)
            self.stack[2048:2052] = crc.to_bytes(4, "little")
            args = (RING + 4 * ring_word, words, OUT + out_at, TABLES)
        before = (m.executed, m.loads, m.stores)
        result = m.call(self.symbols[name], *args, stack=sp)
        if m.r[15] != sp:
            raise Fault("the stack pointer was not restored")
        written = bytes(self.out[out_at:out_at + len(data)])
        if bytes(self.out[:out_at]) != bytes([0xA5]) * out_at or \
           bytes(self.out[out_at + len(data):]) != bytes([0xA5]) * (len(self.out) - out_at - len(data)):
            raise Fault(f"{name} wrote outside its {len(data)} bytes")
        if self.ring[4 * ring_word:4 * ring_word + len(data)] != data:
            raise Fault(f"{name} wrote to the ring")
        # The caller's argument slot above the stack pointer is the caller's.
        if int.from_bytes(self.stack[2048:2052], "little") != crc:
            raise Fault(f"{name} wrote to its caller's stack")
        return result, written, m.executed - before[0], m.loads - before[1], m.stores - before[2]


def check(harness, name, true_order, compiled, trials, seed):
    reverse, step, blob = build_tables(true_order)
    harness.tables[:] = blob
    rng = random.Random(seed)
    special = [bytes([0xFF]) * 16, bytes(16), bytes([0x7F, 0xFE, 0xFF, 0x00] * 4), bytes(range(240, 256))]
    for trial in range(trials):
        words = rng.choice((1, 1, 2, 3, 4, 5, 7, 8, 16, 31, 64, 127, 128))
        data = special[trial] if trial < len(special) else bytes(rng.randrange(256) for _ in range(4 * words))
        crc = 0 if trial % 5 == 0 else rng.randrange(0x10000)
        result, written, *_ = harness.run(name, data, crc, rng.randrange(4), rng.randrange(8), compiled)
        want_crc, want_out = byte_steps(data, crc, reverse, step)
        if result != want_crc or written != want_out:
            raise Fault(f"{name}: trial {trial}, {len(data)} bytes from remainder {crc:#06x}: got {result:#x}, want {want_crc:#x}; "
                        f"bytes {'equal' if written == want_out else 'differ'}")
    # A whole block: 512 data bytes and their CRC16 leave no remainder; one wrong bit does.
    block = bytes(rng.randrange(256) for _ in range(512))
    crc = standard_crc16(block)
    frame = block + bytes([crc >> 8, crc & 0xFF]) + bytes(2)
    wire = frame if true_order else bytes(bit_reverse(b) for b in frame)
    result, written, *_ = harness.run(name, wire[:512], 0, 0, 0, compiled)
    remainder, _ = byte_steps(wire[512:514], result, reverse, step)
    if remainder or written != block:
        raise Fault(f"{name}: a block and its CRC16 left remainder {remainder:#x}")
    damaged = bytearray(wire[:512])
    damaged[rng.randrange(512)] ^= 1 << rng.randrange(8)
    result, *_ = harness.run(name, bytes(damaged), 0, 0, 0, compiled)
    remainder, _ = byte_steps(wire[512:514], result, reverse, step)
    if not remainder:
        raise Fault(f"{name}: a damaged block left no remainder")
    # What one more word costs.
    one = harness.run(name, bytes(4), 0, 0, 0, compiled)
    many = harness.run(name, bytes(4 * 101), 0, 0, 0, compiled)
    return tuple((many[i] - one[i]) // 100 for i in (2, 3, 4)), tuple((many[i] - one[i]) % 100 for i in (2, 3, 4))


def main():
    elf = sys.argv[1]
    harness = Harness(elf)
    expect = {"_kui_wire_words_lsb": (False, (41, 9, 4)), "_kui_wire_words_true": (True, (34, 5, 4))}
    for name, (true_order, cost) in expect.items():
        if name not in harness.symbols:
            raise SystemExit(f"{elf} has no {name}: not a Test 74 build with the hand-written loops")
        per_word, rest = check(harness, name, true_order, False, 400, 74)
        if per_word != cost or rest != (0, 0, 0):
            raise SystemExit(f"{name}: a turn executes {per_word} (instructions, loads, stores), expected {cost}")
        print(f"PASS {name[1:]} as linked: 400 runs equal to the byte steps, a block's CRC16 taken to zero and a damaged "
              f"block's not; {per_word[0]} instructions, {per_word[1]} loads and {per_word[2]} stores for four bytes; "
              f"registers, stack and everything around the destination left alone")
    if len(sys.argv) > 2:
        reference = Harness(sys.argv[2])
        if "_wire_words" not in reference.symbols:
            raise SystemExit(f"{sys.argv[2]} has no compiled word loop to compare the interpreter with")
        per_word, _ = check(reference, "_wire_words", False, True, 400, 74)
        print(f"PASS the compiler's wire_words of {sys.argv[2]} on the same data: the interpreter agrees with it "
              f"({per_word[0]} instructions, {per_word[1]} loads and {per_word[2]} stores for four bytes)")


if __name__ == "__main__":
    try:
        main()
    except Fault as fault:
        raise SystemExit(f"FAIL {fault}")
