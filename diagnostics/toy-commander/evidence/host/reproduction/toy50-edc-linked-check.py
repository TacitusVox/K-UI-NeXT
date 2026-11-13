#!/usr/bin/env python3
"""Execute the linked EDC and its Mode 1 caller through modeled P1/P2 aliases."""
from pathlib import Path
from random import Random
from struct import pack_into, unpack_from
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "trace-reader" / "tools"))
from profile_vmu_native_release_sh import ReleaseSH, MASK, signed

LINK_BASE = 0x8C421000
SOURCE = 0x8C6E0000
POLY = 0xD8018001


def reference(data):
    state = 0
    for value in data:
        state ^= value
        for _ in range(8):
            state = (state >> 1) ^ (POLY if state & 1 else 0)
    return state


class EDCCPU(ReleaseSH):
    def __init__(self, path, base):
        super().__init__(path)
        self.table_names = [name for name, item in self.symbols.items()
                            if "table" in name and item[1] in (1024, 2048)
                            and item[0] & 0xE0000000 == 0xA0000000]
        assert self.table_names
        delta = base - LINK_BASE
        if delta:
            blob = bytearray(path.with_suffix(".bin").read_bytes())
            wire = path.with_suffix(".reloc").read_bytes()
            assert unpack_from("<II", wire, 20) == (0x4000, LINK_BASE)
            for offset in range(72, len(wire), 8):
                location, value = unpack_from("<II", wire, offset)
                assert unpack_from("<I", blob, location)[0] == value
                pack_into("<I", blob, location, value + delta)
            self.store_block(base | 0x20000000, bytes(blob).ljust(0x4000, b"\0"))
            self.symbols = {
                name: ((item[0] + delta, *item[1:])
                       if LINK_BASE <= (item[0] & 0xDFFFFFFF) <= LINK_BASE + 0x4000 else item)
                for name, item in self.symbols.items()
            }
        self.tables = [(self.symbols[name][0] & 0x1FFFFFFF, self.symbols[name][1])
                       for name in self.table_names]
        self.table_reads = {0x80000000: 0, 0xA0000000: 0}
        self.table_writes = {0x80000000: 0, 0xA0000000: 0}
        self.cache_invalidations = []

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF00F == 0x300A:  # subc Rm,Rn
            rhs = self.r[m] + (self.sr & 1)
            lhs = self.r[n]
            self.r[n] = (lhs - rhs) & MASK
            self.flag(lhs < rhs)
        elif op & 0xF0FF == 0x4019:  # shlr8 Rn
            self.r[n] >>= 8
        elif op & 0xF0FF == 0x4018:  # shll8 Rn
            self.r[n] = self.r[n] << 8 & MASK
        elif op & 0xF00F == 0x3007:  # cmp/gt Rm,Rn
            self.flag(signed(self.r[n], 32) > signed(self.r[m], 32))
        elif op & 0xF0FF == 0x4011:  # cmp/pz Rn
            self.flag(not self.r[n] & 0x80000000)
        elif op & 0xFF00 == 0x8400:  # mov.b @(disp,Rm),R0
            self.r[0] = signed(self.read(self.r[m] + (op & 15), 1), 8) & MASK
        elif op & 0xF00F == 0x000C:  # mov.b @(R0,Rm),Rn
            self.r[n] = signed(self.read((self.r[0] + self.r[m]) & MASK, 1), 8) & MASK
        elif op & 0xF0FF == 0x0093:  # ocbi @Rn
            self.cache_invalidations.append(self.r[n])
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def in_table(self, address, count):
        physical = address & 0x1FFFFFFF
        return any(start <= physical and physical + count <= start + length
                   for start, length in self.tables)

    def read(self, address, count=4):
        if self.active and self.in_table(address, count):
            self.table_reads[address & 0xE0000000] += 1
        return super().read(address, count)

    def write(self, address, value, count=4):
        if self.active and self.in_table(address, count):
            self.table_writes[address & 0xE0000000] += 1
        return super().write(address, value, count)


def test(path, base, paired):
    cpu = EDCCPU(path, base)
    if paired:
        assert all(not start & 31 for start, _ in cpu.tables), cpu.tables
        assert sum(length for _, length in cpu.tables) == 2048
    expected_bytes = sum(length for _, length in cpu.tables)
    start = cpu.call("_kui_cd_edc", (SOURCE, 0))
    assert start["result"] == 0
    assert cpu.table_writes == {0x80000000: 0, 0xA0000000: expected_bytes // 4}, cpu.table_writes
    start_writes = cpu.table_writes.copy()
    expected_invalidations = [0x80000000 | (start + offset)
                              for start, length in cpu.tables for offset in range(0, length, 32)]
    assert cpu.cache_invalidations == (expected_invalidations if paired else [])
    random = Random(71850)
    warm = None
    for offset in range(4):
        for length in (0, 1, 2, 3, 4, 31, 32, 255, 256, 2048, 2064, 2352):
            data = random.randbytes(length)
            cpu.store_block(SOURCE + offset, data)
            old_reads = cpu.table_reads.copy()
            result = cpu.call("_kui_cd_edc", (SOURCE + offset, length))
            assert result["result"] == reference(data), (offset, length, result)
            reads = {key: cpu.table_reads[key] - old_reads[key] for key in old_reads}
            assert reads == {0x80000000: length if paired else 0,
                             0xA0000000: 0 if paired else length}, reads
            if offset == 0 and length == 2064:
                warm = result["instructions"], reads
    assert cpu.table_writes == start_writes
    assert cpu.cache_invalidations == (expected_invalidations if paired else [])
    # Run the linked sector checker and exercise the independent rejection path.
    data = bytearray(b"\0" + b"\xff" * 10 + b"\0\x60\x01\x32\x01" + random.randbytes(2048))
    data += reference(data).to_bytes(4, "little") + bytes(284)
    for offset in (None, 1, 100, 2065):
        sector = data.copy()
        if offset is not None:
            sector[offset] ^= 0x01
        cpu.store_block(SOURCE, sector)
        result = cpu.call("_kui_sector_edc_valid", (SOURCE,))
        assert result["result"] == int(offset is None), (offset, result)
    print(f"{path} at {base:08x}: cold P2 table stores={expected_bytes // 4}; "
          f"warm 2064 bytes instructions={warm[0]}, P1 reads={warm[1][0x80000000]}, "
          f"P2 reads={warm[1][0xA0000000]}; 48 buffer cases and 4 sector cases passed")
    return warm


if __name__ == "__main__":
    baseline = Path(sys.argv[1])
    for base in (LINK_BASE, 0x8C550000):
        old = test(baseline, base, False)
        if len(sys.argv) > 2:
            new = test(Path(sys.argv[2]), base, True)
            assert new[1][0xA0000000] < old[1][0xA0000000], (old, new)
