#!/usr/bin/env python3
"""Prove exact original masked GDFS REQUEST/EXEC order and PR."""
import argparse
from hashlib import sha256
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def load(name, path):
    spec = spec_from_file_location(name, ROOT / path)
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


mode61 = load('bumper61', 'toy61-refill-cap-replay.py')
refill62 = load('refill62', 'toy62-refill-replay.py')
original = mode61.original
BIOS = mode61.BIOS
OUT = mode61.refill.OUT
DEVICE = mode61.DEVICE
FILE = mode61.refill.FILE
LOADER_PTR = 0x8C0C69EC
SR_TEST_MASK = 0x100000F0


class OriginalKick(mode61.Replay):
    def __init__(self, image, aux, variant):
        super().__init__(image, aux, variant)
        self.calls = []
        self.write(mode61.refill.LITERAL, mode61.refill.ENQUEUE)

    def step(self):
        if self.pc == BIOS:
            self.calls.append((self.r[7], self.sr & SR_TEST_MASK, self.pr,
                               self.read(self.r[15]), self.read(self.r[15] + 4), self.r[15]))
        super().step()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('aux', type=Path)
    args = parser.parse_args()
    image = args.original.read_bytes()
    assert len(image) == original.SOURCE_BYTES and sha256(image).hexdigest() == original.SOURCE_SHA256
    aux = original.read_elf(args.aux)
    mode61.low.CONFIG, mode61.low.INITIALIZED = refill62.CONFIG, refill62.INITIALIZED
    mode61.low.Replay.__init__ = refill62.low.Replay.__init__
    OriginalKick.symbols = aux[4]
    cases = 0
    for variant in range(4):
        for sr in (0x40000000, 0x40000010, 0x400000F0):
            cpu = OriginalKick(image, aux, variant)
            sectors = mode61.STOCK
            assert cpu.stock_refill(sr) == sectors * mode61.SECTOR
            assert [entry[0] for entry in cpu.calls] == [2, 0, 2], cpu.calls
            prior, request, kick = cpu.calls
            assert prior[1] == (sr & SR_TEST_MASK) and prior[2:5] == (0x8c0bd55a, 0x8c0b1fe6, 0x8c09712a), cpu.calls
            assert request[1] == kick[1] == 0xf0, cpu.calls
            assert kick[2] == 0x8c0bd55a and kick[3] == 0x8c0b2b4e, cpu.calls
            assert cpu.read(DEVICE + mode61.DEVICE_OWNED) == FILE
            assert cpu.read(LOADER_PTR) == DEVICE
            assert cpu.run_at(0x8C097114, [FILE, 0, 0, 0], sr) == 0
            assert [entry[0] for entry in cpu.calls] == [2, 0, 2, 2, 1], cpu.calls
            later = cpu.calls[3]
            assert later[1] == (sr & SR_TEST_MASK) and later[2:4] == (0x8c0bd55a, 0x8c0b1fe6), cpu.calls
            assert later[4] == 0x8c097144, cpu.calls
            assert cpu.read(FILE + mode61.POSITION) == 550 + sectors
            cases += 1
    print(f'{cases} original AVI → GDFS → BIOS cases preserve masked initial and ordinary followup EXEC ancestry')


if __name__ == '__main__':
    main()
