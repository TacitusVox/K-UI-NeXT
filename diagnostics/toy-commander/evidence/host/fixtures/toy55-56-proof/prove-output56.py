#!/usr/bin/env python3
"""Run authentic Toy update/exit paths and the linked output/skip witness."""
from hashlib import sha256
import importlib.util
from pathlib import Path
import sys

base = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("replay39", base / "toy39-original-replay.py")
replay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(replay)

class OutputReplay(replay.MovieReplay):
    def __init__(self, original, aux, clip, pal, scenario):
        self.outcome = scenario
        self.clip = clip
        super().__init__(original, aux, clip, pal, "eof" if scenario == "skip" else scenario)

    def stub(self, pc):
        if self.hook and pc == 0x8c06d3f0:
            expected_flags = 0x21 if self.clip == 1 else 1
            assert self.record()[:4] == (expected_flags, self.r[0], 0, 0)
        if pc == 0x8c08d400:
            self.write(self.ctx + 0x0c, 0x8c410000)
            self.write(self.ctx + 44, self.read(0x8c04c774))  # Game output callback installed by the media setup.
            return super().stub(pc)
        if pc == 0x8c06d260 and self.outcome == "skip":
            assert self.r[4] == self.ctx
            if not self.clock_calls:
                self.write(self.ctx + replay.FRAME, 100)
                self.write(self.ctx + replay.SAMPLES, 44100)
            self.clock_calls += 1
            self.r[0] = 104
        elif pc == 0x8c06d260 and self.outcome == "lag":
            # Represent frames emitted and skipped earlier in this playback.
            self.write(self.ctx + 0xac, 63)
            self.write(self.ctx + 0x90, 37)
            return super().stub(pc)
        elif pc == 0x8c09774c:
            # Run the actual game routine for both output and no-output.
            assert self.outcome in ("eof", "skip")
            return False
        elif pc == 0x8c06ddc8:
            self.r[0] = 0
        elif pc == 0x8c06e03a:
            assert (self.r[5] != 0) == (self.outcome == "eof"), (self.outcome, hex(self.r[5]), [hex(v) for v in self.pc_trace[-12:]])
            self.r[0] = 0
        elif pc == 0x8c04c238:
            assert self.outcome == "eof" and self.r[4] == self.ctx
            self.rendered += 1
            self.r[0] = 0
        else:
            return super().stub(pc)
        self.pc = self.pr
        return True

original = Path(sys.argv[1]).read_bytes()
assert len(original) == replay.SOURCE_BYTES and sha256(original).hexdigest() == replay.SOURCE_SHA256
aux = replay.read_elf(Path(sys.argv[2]))
cases = {
    "lag": (3, 0x7db, 63, 37, replay.LAG_ABORT, 100, 44100, 7, 6),
    "eof": (11, 0x7db, 1, 0, 0x7dc, 101, 44100, 0, 0),
    "skip": (11, 0x7db, 0, 1, 0x7dc, 101, 44100, 0, 0),
    "input": (7, 0x7db, 0, 0, 0x7dc, 0, 0, 0, 0),
    "open": (1, 0xfffff837, 0, 0, 0, 0, 0, 0, 0),
}
for clip in (0, 1):
    for pal in (False, True):
        for scenario, stock in cases.items():
            expected = stock
            bare = OutputReplay(original, None, clip, pal, scenario)
            witness = OutputReplay(original, aux, clip, pal, scenario)
            assert bare.run_movie() == witness.run_movie(), scenario
            assert witness.boundaries == bare.boundaries
            assert witness.dispatches == bare.dispatches
            owner_ctx = bytearray(bare.block(bare.ctx, 0x250))
            if clip == 1:
                owner_ctx[0x1e0:0x1e4] = (0xf0000).to_bytes(4, "little")
            assert witness.block(witness.ctx, 0x250) == owner_ctx
            assert bare.rendered == witness.rendered == int(scenario == "eof"), (scenario, bare.rendered, hex(bare.read(bare.ctx+44)), witness.record())
            if clip == 1:
                expected = (expected[0] | 0x20, *expected[1:])
            assert witness.record() == expected, (scenario, witness.record(), expected)
            assert witness.block(witness.witness + (1 - clip) * replay.RECORD_BYTES,
                                 replay.RECORD_BYTES) == bytes(replay.RECORD_BYTES)
print("Mode 56 original prefill lifecycle / linked witness: 20 cases, actual output and skip branches")
