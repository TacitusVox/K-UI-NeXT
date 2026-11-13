#!/usr/bin/env python3
"""Execute linked low take with the actual high two-byte CRC callback."""
from functools import partial
from itertools import product
from pathlib import Path
import runpy
from struct import pack_into, unpack_from
import sys

ROOT = Path(__file__).resolve().parent
support = runpy.run_path(str(ROOT / "toy51-linked-resume.py"))
checks = runpy.run_path(str(ROOT / "toy53-linked-crc16.py"))
Base, cut, pio = support["Replay"], support["cut"], support["pio"]
RESULT = support["RESULT"]
BODY = support["BODY"]
SLOT = cut.R + 0x500 + 15 * 4
CRC = "_kui_toy_sd_crc16_check"


def bind(cpu, path, load=None):
    from check_vmu_native_worker_gateway import read_elf
    blob, _, segments, _, symbols = read_elf(path)
    header = symbols["_kui_native_readahead_aux_exports"][0]
    if load is None:
        for kind, offset, address, _, stored, memory, _, _ in segments:
            if kind == 1:
                cpu.store_block(address, blob[offset:offset+stored] + bytes(memory-stored))
        delta = 0
    else:
        image = bytearray(path.with_suffix(".bin").read_bytes())
        wire = path.with_suffix(".reloc").read_bytes()
        version, start, size, span, base, records, total, _ = unpack_from("<8I", wire, 8)
        assert (version, start, size, base, total) == (1, 72, len(image), header, len(wire))
        assert len(wire) == 72 + records * 8 and span == 0x5000
        delta = load - base
        for offset, old in (unpack_from("<II", wire, i) for i in range(start,total,8)):
            assert unpack_from("<I", image, offset)[0] == old
            pack_into("<I", image, offset, old+delta)
        cpu.store_block(load, image + bytes(span-len(image)))
    assert cpu.read(header + delta + 12 * 4) == symbols[CRC][0] + delta
    cpu.write(SLOT, symbols[CRC][0] + delta)
    return symbols


class Replay(Base):
    def __init__(self, image, area, received, held, later, *, aux=None):
        super().__init__(image, area, received, held, later)
        if aux:
            self.high_symbols = bind(self, aux)

    def plain(self, op, pc):
        if op & 0xf00f == 0x600f:  # exts.w Rm,Rn
            n, m = op >> 8 & 15, op >> 4 & 15
            self.r[n] = pio.signed(self.r[m] & 0xffff, 16) & 0xffffffff
            self.executing = pc
            self.instructions += 1
        elif op & 0xf0ff == 0x4015:  # cmp/pl Rn
            self.flag(0 < self.r[op >> 8 & 15] < 0x80000000)
            self.executing = pc
            self.instructions += 1
        else:
            super().plain(op, pc)


def crc_table(candidate, aux):
    cpu = Replay(candidate, 0, 1, 0, False, aux=aux)
    address, length = cpu.high_symbols["_table"][:2]
    assert not address & 31 and length == 1024
    for row in range(2):
        for value in range(256):
            payload = bytes([value, 0]) if row == 0 else bytes([value])
            crc = int.from_bytes(pio.crc16(payload).to_bytes(2, "big"), "little")
            assert cpu.read(address + 512 * row + value * 2, 2) == crc
    # A relocated callback must load the relocated table, not the link
    # address. The original link address remains empty in these fixtures.
    for load in (0x8c500000, 0x8cc00000):
        cpu = Replay(candidate, 0, 1, 0, False)
        bind(cpu, aux, load)
        cpu.write(cut.AREA_READY, 1, 1)
        cpu.write(cut.AREA_WIRE, 1, 1)
        cpu.write(checks["READY_LBA"], pio.LBA)
        high, low = pio.crc16(BODY).to_bytes(2, "big")
        cpu.store_block(cpu.area, bytes(map(pio.reverse, BODY + bytes([high]))))
        cpu.write(checks["RDR"], pio.reverse(low), 1)
        assert cpu.call(pio.STREAM_TAKE, [pio.LBA, RESULT])["result"] == cpu.area
        assert cpu.read(RESULT) == 0 and cpu.block(cpu.area, 512) == BODY


def recovered_edges(candidate, aux):
    for area, wire, fault in product(range(2), (False, True),
                                      ("good", "zero", "fe", "ff", "no-syndrome", "unbound")):
        if not wire and fault == "unbound":
            continue
        cpu = Replay(candidate, area, 43, 0, True, aux=aux if fault != "unbound" else None)
        data = bytearray(BODY)
        if fault == "zero":
            data[43] = 0
        tail = pio.crc16(data).to_bytes(2, "big")
        known_crc = int.from_bytes(tail, "big")
        if fault in ("fe", "ff"):
            tail = tail[:1] + bytes([int(fault, 16)])
            assert int.from_bytes(tail, "big") != known_crc
        elif fault == "no-syndrome":
            possible = {pio.crc16(data[:43] + bytes([x]) + data[44:]) for x in range(256)}
            tail = next(x.to_bytes(2, "big") for x in range(65534) if x not in possible)
        source = data[:43] + bytes(1) + data[44:] + tail[:1]
        encode = pio.reverse if wire else lambda x: x
        cpu.store_block(cpu.area, bytes(map(encode, source)))
        cpu.write(cpu.area+516, support["VALUE"])
        cpu.write(cpu.area+520, 44)
        cpu.write(cut.AREA_READY+area, 1, 1)
        cpu.write(cut.AREA_WIRE+area, 8 | wire, 1)
        cpu.write(checks["READY_LBA"]+area*4, pio.LBA)
        cpu.write(checks["RDR"]+area, encode(tail[1]), 1)
        cpu.reg_trace.clear()
        success = fault in ("good", "zero")
        assert cpu.call(pio.STREAM_TAKE, [pio.LBA, RESULT])["result"] == (cpu.area if success else 0)
        assert cpu.read(RESULT) == (0 if success else 5)
        assert cpu.read(support["COUNT"]) == int(success)
        assert cpu.read(support["KEPT"], 1) == 0 and not cpu.reg_trace
        if success:
            assert cpu.block(cpu.area, 512) == data


def main(baseline, candidate, aux):
    crc_table(candidate, aux)
    # The saved standalone comparison uses no callback on the base.
    def make_replay(image, *args, **kwargs):
        return Replay(image, *args, aux=aux if image == candidate else None, **kwargs)
    checks["intact"].__globals__["Replay"] = make_replay
    checks["recovered"].__globals__["Replay"] = make_replay
    checks["main"](baseline, candidate)
    checks["intact"].__globals__["Replay"] = Replay
    for area, body, wire, damage in product(range(2), checks["CASE_PATTERNS"],
                                           (False, True), (None, "payload", "first_crc", "second_crc")):
        checks["intact"](candidate, area, body, wire, damage)
    recovered_edges(candidate, aux)
    support["main"].__globals__["Replay"] = partial(Replay, aux=aux)
    sys.argv = ["toy51-linked-resume.py", str(candidate)]
    support["main"]()
    print("512 exact table entries, 2 relocated callbacks, 128 unbound intact checks, "
          "and 22 resumed edge cases passed")


if __name__ == "__main__":
    main(*(Path(p) for p in sys.argv[1:]))
