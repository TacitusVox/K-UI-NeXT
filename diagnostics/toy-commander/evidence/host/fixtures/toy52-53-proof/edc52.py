"""Replay linked Test 52 Mode 1 verification and compare against Test 51."""
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from random import Random
from struct import pack
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = spec_from_file_location('edc50', ROOT / 'toy50-edc-linked-check.py')
edc50 = module_from_spec(spec)
spec.loader.exec_module(edc50)
MASK = edc50.MASK
SOURCE = 0x8C6E0000
TABLE = 0x8C6F0000
BEFORE = b'proof_before_table_guard_032b'[:32].ljust(32, b'!')
AFTER = b'proof_after_table_guard_032b'[:32].ljust(32, b'?')
RAW_BYTES, EDC_OFFSET, TABLE_BYTES = 2352, 2064, 4096


def reference_table():
    return b''.join(pack('<I', edc50.reference(bytes([byte]) + bytes(zeros)))
                    for zeros in range(4) for byte in range(256))


class CPU(edc50.EDCCPU):
    def __init__(self, path, candidate):
        if not candidate:
            super().__init__(path, edc50.LINK_BASE)
        else:
            edc50.ReleaseSH.__init__(self, path)
            self.tables = [(TABLE & 0x1FFFFFFF, TABLE_BYTES)]
            self.table_reads = {0x80000000: 0, 0xA0000000: 0}
            self.table_writes = {0x80000000: 0, 0xA0000000: 0}
            self.cache_invalidations = []
        self.mach = 0
        self.input_loads = {1: 0, 2: 0, 4: 0}
        self.track_input = False

    def read(self, address, count=4):
        if self.active and self.track_input and SOURCE <= address < SOURCE + RAW_BYTES + 3:
            assert count in self.input_loads
            assert not address % count, (hex(address), count)
            self.input_loads[count] += 1
        return super().read(address, count)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF00F == 0x3005:  # dmulu.l Rm,Rn
            product = self.r[m] * self.r[n]
            self.mach, self.macl = product >> 32, product & MASK
        elif op & 0xF0FF == 0x000A:  # sts MACH,Rn
            self.r[n] = self.mach
        elif op & 0xF0FF == 0x4024:  # rotcl Rn
            top = self.r[n] >> 31
            self.r[n] = (self.r[n] << 1 | (self.sr & 1)) & MASK
            self.flag(top)
        elif op & 0xF00F == 0x6008:  # swap.b Rm,Rn
            word = self.r[m]
            self.r[n] = word & 0xFFFF0000 | (word & 255) << 8 | (word >> 8 & 255)
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1


def raw_sector(fad, payload, mode=1):
    def bcd(value):
        return 16 * (value // 10) + value % 10

    raw = (b'\0' + b'\xff' * 10 + b'\0' +
           bytes((bcd(fad // 4500), bcd(fad // 75 % 60), bcd(fad % 75), mode)) +
           payload)
    if mode == 2:
        raw += bytes(8)
        raw = raw[:16] + raw[-8:] + raw[16:-8]
    covered = raw if mode == 1 else raw[16:]
    return (raw + pack('<I', edc50.reference(covered))).ljust(RAW_BYTES, b'\0')


def main():
    baseline, candidate = (CPU(Path(path), bool(index))
                           for index, path in enumerate(sys.argv[1:3]))
    baseline.call('_kui_cd_edc', (SOURCE, 0))
    candidate.store_block(TABLE - 32, BEFORE + b'\xa5' * TABLE_BYTES + AFTER)
    candidate.call('_kui_cd_edc_table_init', (TABLE,))
    assert candidate.block(TABLE - 32, TABLE_BYTES + 64) == BEFORE + reference_table() + AFTER
    assert candidate.table_writes == {0x80000000: 1024, 0xA0000000: 0}
    writes = candidate.table_writes.copy()
    candidate.track_input = baseline.track_input = True
    random = Random(510052)
    payload = random.randbytes(2048)
    fads = (150, 449999, 450000, 0x6923E + 150, 0x6EA21 + 150, 719999)
    cases = 0
    totals = {}
    for fad in fads:
        raw = raw_sector(fad, payload)
        scenarios = [(raw, fad, True), (raw, fad + 1, False),
                     (raw, 149, False), (raw, 720000, False)]
        for bit_at in (0, 3, 11, 12, 13, 14, 15, 16, 17, 1023, 2063, 2064, 2067, 2068):
            mutated = bytearray(raw)
            mutated[bit_at] ^= 8
            scenarios.append((mutated, fad, bit_at >= 2068))
        # A valid EDC with a mismatched FAD proves the separate address guard.
        scenarios.append((raw, fad - 1, False))
        for sector, address, expected in scenarios:
            for cpu, name, extra in (
                (baseline, '_kui_sector_edc_valid_at', ()),
                (candidate, '_kui_mode1_edc_table_valid_at', (TABLE,)),
            ):
                cpu.store_block(SOURCE, sector)
                old_reads = cpu.table_reads.copy()
                cpu.input_loads = dict.fromkeys(cpu.input_loads, 0)
                result = cpu.call(name, (SOURCE, address, *extra))
                assert result['result'] == int(expected), (name, fad, address, expected, result)
                if sector is raw and address == fad:
                    reads = {key: value - old_reads[key]
                             for key, value in cpu.table_reads.items()}
                    assert reads == {0x80000000: EDC_OFFSET, 0xA0000000: 0}
                    totals[fad, name] = (result['instructions'], cpu.input_loads)
            cases += 1
        for offset in (1, 2, 3):
            candidate.store_block(SOURCE + offset, raw)
            candidate.input_loads = dict.fromkeys(candidate.input_loads, 0)
            old_reads = candidate.table_reads.copy()
            assert candidate.call('_kui_mode1_edc_table_valid_at',
                                  (SOURCE + offset, fad, TABLE))['result'] == 0
            assert candidate.table_reads == old_reads and not candidate.input_loads[4]
            cases += 1
    # Original staging admits Mode 1 only. A valid Mode 2 remains in the
    # ordinary validator and fails the new strict Mode 1 entry point.
    mode2 = raw_sector(450000, payload, 2)
    baseline.store_block(SOURCE, mode2)
    candidate.store_block(SOURCE, mode2)
    assert baseline.call('_kui_sector_edc_valid_at', (SOURCE, 450000))['result'] == 1
    assert candidate.call('_kui_mode1_edc_table_valid_at', (SOURCE, 450000, TABLE))['result'] == 0
    assert candidate.table_writes == writes
    assert candidate.block(TABLE - 32, TABLE_BYTES + 64) == BEFORE + reference_table() + AFTER
    old, new = (totals[fads[-1], name] for name in
                ('_kui_sector_edc_valid_at', '_kui_mode1_edc_table_valid_at'))
    assert new[1][4] >= EDC_OFFSET // 4
    print(f'{cases + 1} linked Mode 1/2 cases; all 1024 table entries, guard and P1 use verified')
    print(f'Aligned 2064-byte validation: Test 51 {old[0]} instructions, Test 52 {new[0]} '
          f'({(old[0]-new[0])/old[0]:.1%} fewer); input reads {old[1]} -> {new[1]}')
    assert all(totals[fad, '_kui_mode1_edc_table_valid_at'][0] <
               totals[fad, '_kui_sector_edc_valid_at'][0] for fad in fads)


if __name__ == '__main__':
    main()
