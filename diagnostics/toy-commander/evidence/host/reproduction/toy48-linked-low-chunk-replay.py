#!/usr/bin/env python3
"""Run original AVI acquisition and the actual linked low-context observer."""
import argparse
from hashlib import sha256
from importlib.util import spec_from_file_location, module_from_spec
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parent
spec = spec_from_file_location("original_chunk", ROOT / "toy42-original-chunk-replay.py")
original_chunk = module_from_spec(spec)
spec.loader.exec_module(original_chunk)

CTX, STACK = 0x8C00DC90, 0x8C00DA00
REGION, CONFIG, INITIALIZED, MOVIE_MASK, CHUNK = 0x8C420400, 1176, 1284, 864, 728
SERVICE = REGION + 0x320
RANGES = ((0x69000, 1000), (0x69800, 1200), (0x6DA00, 26000), (0x74000, 25000))
NAMES = (b"50hz_Bumper_LRTF.avi", b"60hz_Bumper_LRTF.avi",
         b"50hz_Intro_LRTF.avi", b"60hz_Intro_LRTF.avi")
original_chunk.CTX, original_chunk.STACK = CTX, STACK


class Replay(original_chunk.ChunkReplay):
    def __init__(self, image, aux, case, tracked, initial_t, variant, pending, verified):
        super().__init__(image, aux, case, tracked, initial_t)
        self.observer_target = self.observer
        self.observer = None  # Execute the function instead of the old ABI substitute.
        self.observer_limit = self.observer_target + aux[4]["_kui_toy_movie_chunk_observe"][1]
        self.context = aux[4]["_kui_native_readahead_aux_context"][0]
        self.got = []
        assert aux[4]["_kui_native_readahead_aux_context"][1] == 1576
        self.write(self.context + INITIALIZED, 1)
        self.write(self.context + MOVIE_MASK, 15)
        self.write(self.context + CONFIG + 8, REGION)
        self.write(self.context + CONFIG + 84, int(verified))
        self.store_block(CTX + 0xBC, NAMES[variant] + b"\x00")
        for idx, (lba, count) in enumerate(RANGES):
            self.write(self.context + CONFIG + 32 + idx * 8, lba)
            self.write(self.context + CONFIG + 36 + idx * 8, count)
        base_lba, size = RANGES[variant]
        self.write(SERVICE + 44, 0x429F)
        self.write(SERVICE + 48, 16 if variant & 1 else 17)
        self.write(SERVICE + 52, base_lba + (size if pending == "outside" else 396))
        self.write(SERVICE + 56, 4)
        self.write(SERVICE + 80, int(pending != "none"))
        self.writes.clear()

    def step(self):
        if self.pc == self.observer_target:
            assert self.tracked and self.r[4] == CTX and self.r[15] == STACK - 16
            self.got.append(self.r[5])
        super().step()

    def check(self, expected, variant, pending, verified):
        counts = [0] * 8
        if self.tracked and verified:
            bucket = 0 if expected >= 0 else 3 if expected != -2 else 1 if pending == "matched" else 2
            counts[(variant // 2) * 4 + bucket] = 1
        assert [self.read(self.context + CHUNK + i * 4) for i in range(8)] == counts
        assert self.got == ([expected & original_chunk.MASK] if self.tracked else [])
        observed_writes = [(addr, value, width) for pc, addr, value, width in self.writes
                           if self.observer_target <= pc < self.observer_limit]
        assert all(width == 4 and (STACK - 28 <= addr < STACK - 16 or
                   self.context + CHUNK <= addr < self.context + CHUNK + 32)
                   for addr, value, width in observed_writes)
        observed_counts = [(addr, value) for addr, value, _ in observed_writes
                           if self.context + CHUNK <= addr < self.context + CHUNK + 32]
        assert observed_counts == [(self.context + CHUNK + i * 4, count)
                                   for i, count in enumerate(counts) if count]


def case_check(image, aux, case, tracked, initial_t, variant, pending, verified):
    baseline = original_chunk.ChunkReplay(image, None, case, tracked, initial_t)
    baseline.store_block(CTX + 0xBC, NAMES[variant] + b"\x00")
    result = baseline.run_chunk()
    expected = original_chunk.signed(result[0], 32)
    fixture = Replay(image, aux, case, tracked, initial_t, variant, pending, verified)
    assert fixture.run_chunk() == result
    fixture.check(expected, variant, pending, verified)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("original", type=Path)
    parser.add_argument("aux", type=Path, nargs="+")
    parser.add_argument("--expect-rejected", action="store_true", help="Legacy Test 42 rejected IP stack")
    args = parser.parse_args()
    image = args.original.read_bytes()
    assert len(image) == original_chunk.SOURCE_BYTES
    assert sha256(image).hexdigest() == original_chunk.SOURCE_SHA256
    for path in args.aux:
        aux = original_chunk.read_elf(path)
        cases = 0
        for case, tracked, initial_t, variant in product(original_chunk.CASES, (False, True), (0, 1), range(4)):
            for pending in (("matched", "none", "outside") if case == "pending" else ("matched",)):
                for verified in ((False, True) if tracked and case == "pending" else (True,)):
                    case_check(image, aux, case, tracked, initial_t, variant, pending, verified)
                    cases += 1
        print(f"{path}: {cases} original-backed AVI acquisitions with exact low-stack SH observer")


if __name__ == "__main__":
    main()
