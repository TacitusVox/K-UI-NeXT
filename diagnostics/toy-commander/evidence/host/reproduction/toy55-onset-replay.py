#!/usr/bin/env python3
"""Execute the original AVI selection and linked Test 55 onset observer."""

import argparse
from hashlib import sha256
from importlib.util import spec_from_file_location, module_from_spec
from inspect import getsource
from itertools import product
from pathlib import Path
from textwrap import dedent

ROOT = Path(__file__).resolve().parent
spec = spec_from_file_location("low_chunk", ROOT / "toy51-linked-low-chunk-replay.py")
low = module_from_spec(spec)
spec.loader.exec_module(low)
original = low.original_chunk
bounded_run = getsource(original.ChunkReplay.run_chunk)
assert bounded_run.count("range(512)") == 1
bounded_run = bounded_run.replace("range(512)", "range(8192)")
bounded_run = bounded_run.replace("args = (STREAM,", "args = (self.read(CTX + 8),")
exec(dedent(bounded_run), original.__dict__)

ONSET, ONSET_BYTES, EXIT, EXIT_BYTES = 760, 52, 656, 36
UNKNOWN = 0xFFFFFFFF


class Replay(low.Replay):
    run_chunk = original.run_chunk

    def __init__(self, image, aux, case, tracked, t, variant, pending, verified):
        super().__init__(image, aux, case, tracked, t, variant, pending, verified)
        self.variant = variant
        self.slot = self.context + ONSET + (variant // 2) * ONSET_BYTES
        self.ram_reads = []
        self.in_observer = False
        self.observer_words = {}
        self.observer_steps = 0

    def read(self, address, count=4):
        if getattr(self, "in_observer", False) and count == 4:
            self.ram_reads.append(address)
            if address in self.observer_words:
                return self.observer_words[address]
        return super().read(address, count)

    def step(self):
        if self.observer_target <= self.pc < self.observer_limit:
            self.observer_steps += 1
        if self.pc == self.observer_target:
            self.in_observer = True
        elif self.pc == original.CALL_RETURN:
            self.in_observer = False
        super().step()

    def plain(self, op, pc):
        if op & 0xF00F == 0x0006:  # mov.l Rm,@(R0,Rn)
            n, m = (op >> 8) & 15, (op >> 4) & 15
            self.write((self.r[0] + self.r[n]) & original.MASK, self.r[m])
            self.executing = pc
            self.instructions += 1
        else:
            super().plain(op, pc)

    def start(self, *, frame, outputs, skips, flags=1, status=0x7DB):
        self.write(self.context + EXIT + self.variant // 2 * EXIT_BYTES, flags)
        self.write(self.context + EXIT + self.variant // 2 * EXIT_BYTES + 4, status)
        self.write(low.CTX + 0x1C0, frame)
        self.write(low.CTX + 0xAC, outputs)
        self.write(low.CTX + 0x90, skips)

    def ring(self, *, filled=2000, released=1600, count=3, stride_kind=None,
             stream_index=0, aliases=False):
        avi, channel, entries = original.AVI, original.CHANNEL, original.ENTRIES
        stream = original.STREAM
        alias = 0x20000000 if aliases else 0
        self.write(low.CTX + 8, stream | alias)
        self.write(stream + 4, channel | alias)
        self.write(channel + 68, avi | alias)
        self.write(avi + 0x14, 2048)
        self.write(avi + 0x20, 120)
        self.write(avi + 0x38, entries | alias)
        self.write(avi + 0x44, count)
        self.write(avi + 0x64, released)
        self.write(avi + 0x68, filled)
        self.write(stream + 0x10, stream_index)
        for index in range(count):
            entry = entries + index * 12
            self.write(entry, 0x811 + 4 * (index - 1))
            self.write(entry + 4, 128)
            self.write(entry + 8, 7 if stride_kind is None or index == stride_kind else 2, 1)
            self.write(entry + 9, 0, 1)

    def check_only_observer_writes(self, counts, onset):
        observed = [(addr, value, width) for pc, addr, value, width in self.writes
                    if self.observer_target <= pc < self.observer_limit]
        assert all(width == 4 and (low.STACK - 96 <= addr < low.STACK - 16 or
                   self.context + low.CHUNK <= addr < self.context + low.CHUNK + 32 or
                   self.slot <= addr < self.slot + ONSET_BYTES)
                   for addr, value, width in observed)
        assert [self.read(self.context + low.CHUNK + i * 4) for i in range(8)] == counts
        assert [self.read(self.slot + i * 4) for i in range(13)] == onset + [0] * 5
        other = self.context + ONSET + (1 - self.variant // 2) * ONSET_BYTES
        assert self.block(other, ONSET_BYTES) == bytes(ONSET_BYTES)


def check_retained(image, aux):
    cases = 0
    for case, tracked, t, variant in product(original.CASES, (False, True), (0, 1), range(4)):
        for pending in ("matched", "none", "outside") if case == "pending" else ("matched",):
            for verified in (False, True) if tracked and case == "pending" else (True,):
                baseline = original.ChunkReplay(image, None, case, tracked, t)
                baseline.store_block(low.CTX + 0xBC, low.NAMES[variant] + b"\0")
                expected = baseline.run_chunk()
                fixture = Replay(image, aux, case, tracked, t, variant, pending, verified)
                assert fixture.run_chunk() == expected
                result = original.signed(expected[0], 32)
                counts = [0] * 8
                if tracked and verified:
                    bucket = 0 if result >= 0 else 3 if result != -2 else 1 if pending == "matched" else 2
                    counts[(variant // 2) * 4 + bucket] = 1
                fixture.check_only_observer_writes(counts, [0] * 8)
                cases += 1
    return cases


def check_onset(image, aux):
    cases = 0
    for variant, t in product(range(4), (0, 1)):
        for ring, used, missing in (
            ({}, 400, 313),
            ({"filled": 2400, "released": 1600}, 800, 0),
            ({"filled": 0x10, "released": 0xFFFFFFF0}, 32, 2297),
            ({"filled": 4000, "released": 100}, UNKNOWN, UNKNOWN),
            ({"stride_kind": 2}, 400, 317),
            ({"count": 40, "stride_kind": 36}, 400, UNKNOWN),
            ({"stream_index": UNKNOWN}, 400, 309),
            ({"aliases": True}, 400, 313),
        ):
            baseline = Replay(image, aux, "pending", True, t, variant, "matched", False)
            baseline.ring(**ring)
            baseline.start(frame=28, outputs=25, skips=3)
            expected = baseline.run_chunk()
            assert original.signed(expected[0], 32) == -2
            fixture = Replay(image, aux, "pending", True, t, variant, "matched", True)
            fixture.ring(**ring)
            fixture.start(frame=28, outputs=25, skips=3)
            fixture.writes.clear()
            try:
                actual = fixture.run_chunk()
            except AssertionError as error:
                raise AssertionError((variant, t, ring)) from error
            assert actual == expected
            assert fixture.read(original.STREAM + 0x10) == ring.get("stream_index", 0)
            counts = [0] * 8
            counts[variant // 2 * 4 + 1] = 1
            fixture.check_only_observer_writes(counts, [29, used, missing, 29, 29, used, missing, 25])
            cases += 1
    return cases


def check_refusals(image, aux):
    cases = 0
    for address, malformed in (
        (low.CTX + 8, 0x8C00C000),
        (original.STREAM + 4, 0xA5000000),
        (original.CHANNEL + 68, 0x8CFFFFFC),
        (original.AVI + 0x38, 0x8CFFFFF8),
        (original.AVI + 0x44, 0x40000001),
        (original.CHANNEL + 4, 0x100),
    ):
        fixture = Replay(image, aux, "pending", True, 1, 3, "matched", True)
        fixture.start(frame=18, outputs=18, skips=0)
        fixture.ring()
        fixture.observer_words[address] = malformed
        fixture.writes.clear()
        assert original.signed(fixture.run_chunk()[0], 32) == -2
        counts = [0, 0, 0, 0, 0, 1, 0, 0]
        used = UNKNOWN if address in (low.CTX + 8, original.STREAM + 4, original.CHANNEL + 68) else 400
        fixture.check_only_observer_writes(counts, [19, used, UNKNOWN, 0, 19, 0, 0, 18])
        assert not any(malformed <= addr < malformed + 108 for addr in fixture.ram_reads)
        cases += 1
    for flags, status, outputs in ((0, 0x7DB, 20), (3, 0x7DB, 20), (1, 0x7D2, 20), (1, 0x7DB, 0)):
        fixture = Replay(image, aux, "pending", True, 0, 3, "matched", True)
        fixture.ring()
        fixture.start(frame=25, outputs=outputs, skips=0, flags=flags, status=status)
        fixture.writes.clear()
        assert original.signed(fixture.run_chunk()[0], 32) == -2
        fixture.check_only_observer_writes([0, 0, 0, 0, 0, 1, 0, 0], [0] * 8)
        assert low.CTX + 8 not in fixture.ram_reads
        cases += 1

    fixture = Replay(image, aux, "pending", True, 0, 3, "matched", True)
    fixture.ring()
    fixture.start(frame=42, outputs=31, skips=11)
    stored = [20, 320, 190, 21, 25, 460, 10, 23]
    for index, word in enumerate(stored):
        fixture.write(fixture.slot + index * 4, word)
    fixture.writes.clear()
    assert original.signed(fixture.run_chunk()[0], 32) == -2
    stored[4], stored[7] = 43, 31
    fixture.check_only_observer_writes([0, 0, 0, 0, 0, 1, 0, 0], stored)
    assert low.CTX + 8 not in fixture.ram_reads
    return cases + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("aux", type=Path)
    args = parser.parse_args()
    image = args.original.read_bytes()
    assert len(image) == original.SOURCE_BYTES and sha256(image).hexdigest() == original.SOURCE_SHA256
    aux = original.read_elf(args.aux)
    retained = check_retained(image, aux)
    onset = check_onset(image, aux)
    refusals = check_refusals(image, aux)
    print(f"Test 55: {retained} retained original AVI, {onset} linked onset/ring, and {refusals} refusal/latch cases passed")


if __name__ == "__main__":
    main()
