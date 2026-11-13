#!/usr/bin/env python3
"""Run exact-owner cold install and six original C2 start paths.

The CPU model verifies actual linked words, memory/cache writes and original
MMIO setup instructions. It does not emulate the Holly/SH4 bus arbiter.
"""
import argparse
import importlib.util
from hashlib import sha256
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path[:0] = [str(ROOT / "trace-reader/tests"), str(ROOT / "trace-reader/tools")]
import test_toy_dma_priority as original_proof

spec = importlib.util.spec_from_file_location("toy46_cold", ROOT / "toy46-lease-cold-matrix.py")
cold = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cold)

BASE = original_proof.BASE
P2 = 0x20000000
STOCK, ROUND_ROBIN = 0x8201, 0x8301
PRIORITY = original_proof.PRIORITY_LITERALS
CHCR2_LITERALS = (0x8C094F82, 0x8C095A60, 0x8C095BAC)
ROUTES = (
    (0x8C094EE0, 0x8C094EFA, 0x8C094F52),
    (0x8C0959C0, 0x8C0959DA, 0x8C095AD0),
    (0x8C0959E6, 0x8C095A00, 0x8C095AD0),
    (0x8C095A38, 0x8C095A52, 0x8C095AD0),
    (0x8C095AA4, 0x8C095AC0, 0x8C095AD0),
    (0x8C095AEC, 0x8C095B08, 0x8C095B1A),
)

def verify_original(original, patched):
    expected = bytearray(original)
    assert (STOCK ^ ROUND_ROBIN) == 0x0100
    for address in PRIORITY:
        offset = address - BASE
        assert int.from_bytes(original[offset:offset + 4], "little") == STOCK
        expected[offset:offset + 4] = ROUND_ROBIN.to_bytes(4, "little")
    for address in CHCR2_LITERALS:
        offset = address - BASE
        assert int.from_bytes(original[offset:offset + 2], "little") == 0x12C1
    assert expected == patched
    for route in ROUTES:
        old = original_proof.C2Start(original).run_setup(*route, STOCK)
        new = original_proof.C2Start(patched).run_setup(*route, ROUND_ROBIN)
        assert new == [(pc, address, ROUND_ROBIN if address == original_proof.DMAOR else value, count)
                       for pc, address, value, count in old]


class RoundRobinBinder(cold.ToyBinder):
    def __init__(self, stage, resident, aux, original, payload, cache, exact, *, drop=0):
        super().__init__(stage, resident, aux, original, 47, payload, cache, exact)
        for address in PRIORITY + CHCR2_LITERALS:
            size = 4 if address in PRIORITY else 2
            offset = address - BASE
            self.put_bytes(address, original[offset:offset + size])
        self.drop = drop

    def plain(self, op, pc):
        if self.drop and op & 0xF0FF == 0x00A3:
            reg = (op >> 8) & 15
            if self.r[reg] == (self.drop & ~31):
                return  # Fault injection: drop this cache publication only.
        return super().plain(op, pc)

    def game_writes(self):
        return [(address, value, count) for _, address, value, count in self.writes
                if BASE <= address < BASE + original_proof.SOURCE_BYTES]

    def verify(self):
        self.run_symbol("_native_readahead_bind")
        cold.sdk.assert_c_abi(self, self.initial)
        assert self.init_calls == 1 and self.sdk_state() == self.expected_sdk
        assert self.display_lines[0] == b"COLD 42 READ AHEAD BIND BEGIN"
        assert self.display_lines[-1] == b"COLD 45 READ AHEAD BOUND"
        ready = b"SCI ROUND-ROBIN DMA READY" if self.exact else b"SCI DMA OWNER UNVERIFIED"
        assert self.display_lines[-2] == ready, self.display_lines
        entry, context = self.read(self.dest + 12), self.read(self.dest + 20)
        assert self.read((self.region + 0x500) | P2) == entry
        assert self.read((self.region + 0x504) | P2) == context
        expected = [(address, ROUND_ROBIN, 4) for address in PRIORITY] if self.exact else []
        for address in PRIORITY:
            word = ROUND_ROBIN if self.exact else STOCK
            assert self.read(address | P2) == self.read(address) == word
            assert self.flushes.count(self.offset(address, 4) & ~31) == int(self.exact)
        for address in CHCR2_LITERALS:
            assert self.read(address | P2, 2) == self.read(address, 2) == 0x12C1
        for index, address in enumerate(cold.HOOKS):
            word = self.read(self.dest + 40 + 4 * index) if self.exact else cold.HOOKS[address]
            assert self.read(address | P2) == word
            if self.exact:
                expected.append((address, word, 4))
        assert self.game_writes() == expected


def check_linked(original, build):
    reader, = build.glob("retail-toy-test-47.kui")
    build_id = cold.EntryReplay(build / "entry.elf").run_module(
        reader.read_bytes(), (build / "stage.bin").read_bytes())
    stage, resident, aux = [cold.read_elf(build / f"{name}.elf")
                            for name in ("stage", "resident-sci", "native-ra-aux")]
    for cache in (False, True):
        for payload in (0x0C400000, 0x8C500000, 0xACD60000):
            for exact in (False, True):
                RoundRobinBinder(stage, resident, aux, original, payload, cache, exact).verify()
    for bad in PRIORITY:
        case = RoundRobinBinder(stage, resident, aux, original, 0x8C500000, True, True)
        case.patch_fixture32(bad, STOCK ^ 2)
        try:
            case.run_symbol("_native_readahead_bind")
        except cold.base.BinderStop as error:
            assert (error.phase, error.detail) == (11, 0x47)
        else:
            raise AssertionError("corrupt priority literal was accepted")
        assert case.init_calls == 0 and case.game_writes() == []
    for dropped in PRIORITY:
        case = RoundRobinBinder(stage, resident, aux, original, 0x8C500000, True, True, drop=dropped)
        try:
            case.run_symbol("_native_readahead_bind")
        except cold.base.BinderStop as error:
            assert (error.phase, error.detail) == (11, 0x47)
        else:
            raise AssertionError("unpublished priority literal was accepted")
        assert case.init_calls == 0 and case.read(dropped | P2) == STOCK
    print(f"{build_id}: packaged entry copy, 12 exact linked cold binds, 4 literal/publication refusals")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("build", type=Path, nargs="*")
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == original_proof.SOURCE_BYTES
    assert sha256(original).hexdigest() == original_proof.SOURCE_SHA256
    original_proof.HOST = "#define KUI_TOY_SUITE_DMA_ROUND_ROBIN 1\n" + original_proof.HOST
    patched = original_proof.install_with_host(args.original)
    verify_original(original, patched)
    print("Exact original: two priority literal edits, untouched burst mode, six C2 starts and host refusals verified")
    for build in args.build:
        check_linked(original, build)


if __name__ == "__main__":
    main()
