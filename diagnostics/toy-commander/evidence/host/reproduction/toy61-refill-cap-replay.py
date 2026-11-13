#!/usr/bin/env python3
"""Replay the linked bumper refill cap through the original AVI and GDFS."""
import argparse
from hashlib import sha256
from importlib.util import module_from_spec, spec_from_file_location
from inspect import getsource
from itertools import product
from pathlib import Path
from textwrap import dedent

ROOT = Path(__file__).resolve().parent
spec = spec_from_file_location("refill57", ROOT / "toy57-refill-replay.py")
refill = module_from_spec(spec)
spec.loader.exec_module(refill)
low, original = refill.low, refill.original

DEVICE, BIOS = 0x8C204000, 0x8C008A80
START, POSITION, FILE_LENGTH = 8, 20, 16
FILE_MODE, FILE_STATUS, DEVICE_OWNED = 76, 80, 28
PORT = low.SERVICE + 444
GD_COMMAND, GD_PENDING = low.SERVICE + 48, low.SERVICE + 80
ENGINE_IRQ = refill.ACTIVE + 16
FRAME, POSITIONED, USED = 142, 0xA0000, 0x10000
STOCK, CAPPED = 13, 8
SECTOR = 2048


def bind_fixture(aux):
    # Test 61 appends report fields before config. Read the actual linked
    # selector's DWARF externally when updating these layout constants.
    low.CONFIG, low.INITIALIZED = 1320, 1428
    assert aux[4]["_kui_native_readahead_aux_context"][1] == 1728
    source = dedent(getsource(low.Replay.__init__))
    source = source.replace("super().__init__(", "original_chunk.ChunkReplay.__init__(self, ")
    exec(source.replace("== 1584", "== 1728"), low.__dict__)
    low.Replay.__init__ = low.__dict__["__init__"]
    chunk = dedent(getsource(original.ChunkReplay.run_chunk))
    chunk = chunk.replace("range(512)", "range(8192)")
    chunk = chunk.replace("args = (STREAM,", "args = (self.read(CTX + 8),")
    chunk = chunk.replace("self.r[0] == 2048", "self.r[0] == self.read(AVI + 20)")
    exec(chunk, original.__dict__)
    Replay.run_chunk = original.run_chunk


class Replay(refill.Replay):
    def __init__(self, image, aux, variant):
        super().__init__(image, aux, variant=variant)
        self.requests = []
        self.gd_token = None
        self.write(GD_COMMAND, 0)
        self.write(low.SERVICE + 40, SECTOR)
        self.write(PORT + 15, 1, 1)
        self.write(0x8C0000BC, BIOS)
        self.write(0x8C0C69F8, 0x8C0C5980)
        self.write(0x8C0C69EC, DEVICE)
        self.write(DEVICE + 12, 0x8C0C6260)
        self.write(DEVICE + 60, 0x8C0C6260)
        self.write(refill.FILE, DEVICE)
        base, count = low.RANGES[variant]
        self.write(refill.FILE + START, base + 150)
        self.write(refill.FILE + FILE_LENGTH, count)
        self.write(refill.FILE + POSITION, 550)
        self.write(refill.FILE + FILE_MODE, 0x00010001)
        self.write(refill.FILE + FILE_STATUS, 1, 2)
        self.write(self.avi + 0x10, 6)
        self.write(self.avi + 0x18, 0x100000)
        self.write(self.avi + 0x54, STOCK * SECTOR)
        self.write(self.avi + 0x58, STOCK * SECTOR)
        self.write(original.ENTRIES, POSITIONED - USED + 1)
        self.write(original.ENTRIES + 12, POSITIONED - 0x9000)
        self.write(original.ENTRIES + 24, POSITIONED - 0x4000)
        self.write(original.ENTRIES + 12 + 4, 0x800)
        self.write(original.ENTRIES + 24 + 4, 0x800)
        self.write(self.avi + 0x64, POSITIONED - USED)
        self.start(frame=FRAME, outputs=FRAME - 1, skips=0)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF00F == 0x400C:  # shad Rm,Rn
            shift = original.signed(self.r[m], 32)
            v = self.r[n]
            self.r[n] = ((v << (shift & 31)) if shift >= 0 else
                         (original.signed(v, 32) >> ((-shift) & 31 or 32))) & 0xFFFFFFFF
        elif op & 0xF00F == 0x000D:  # indexed word read
            self.r[n] = original.signed(self.read((self.r[0] + self.r[m]) & 0xFFFFFFFF, 2), 16) & 0xFFFFFFFF
        elif op & 0xF00F == 0x0005:  # indexed word write
            self.write((self.r[0] + self.r[n]) & 0xFFFFFFFF, self.r[m], 2)
        elif op & 0xF0FF in (0x0093, 0x00A3, 0x00B3):  # original cache maintenance
            pass
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def step(self):
        if self.pc == BIOS:
            function = self.r[7]
            if function == 0:
                assert self.r[4] == 17 and self.gd_token is None
                params = tuple(self.read(self.r[5] + i * 4) for i in range(4))
                self.requests.append(params)
                self.gd_token = 17 + len(self.requests)
                self.write(GD_COMMAND, 17)
                self.write(GD_PENDING, 1)
                self.r[0] = self.gd_token
            elif function == 1:
                if self.gd_token is None:
                    assert self.r[4] == 0
                    self.r[0] = 0
                else:
                    assert self.r[4] == self.gd_token
                    self.write(GD_COMMAND, 0)
                    self.write(GD_PENDING, 0)
                    self.write(self.r[5], self.requests[-1][1])
                    self.r[0] = 2
                    self.gd_token = None
            elif function == 2:
                self.r[0] = 0
            else:
                raise AssertionError(f"unexpected GD function {function}")
            self.pc = self.pr
            return
        op = self.read(self.pc, 2)
        if op >> 12 == 0xB:
            pc = self.pc
            self.pr = pc + 4
            self.plain(self.read(pc + 2, 2), pc + 2)
            self.pc = pc + 4 + 2 * original.signed(op & 0xFFF, 12)
        else:
            super().step()

    def run_at(self, entry, args, sr=0x40000000):
        self.pc, self.pr, self.r[15], self.sr = entry, refill.OUT, low.STACK, sr
        self.r[4:8] = args
        saved = self.r[8:15]
        for _ in range(6000):
            if self.pc == refill.OUT:
                assert self.r[8:15] == saved and self.r[15] == low.STACK and self.pr == refill.OUT
                assert self.sr & ~0x301 == sr & ~0x301
                return self.r[0]
            self.step()
        raise AssertionError(f"original path did not return at {self.pc:#x}")

    def cap_state(self):
        return tuple(self.read(self.slot + x) for x in (
            refill.QUIET_CTX, refill.QUIET_TICK, refill.QUIET_DEFERS,
            refill.QUIET_REASON, refill.QUIET_END))

    def latch(self):
        assert self.run_chunk()[0] == 1
        assert self.cap_state() == (low.CTX, 0, 0, 0, 0)

    def stock_refill(self, sr):
        return self.run_at(refill.REFILL, [self.avi, 0, 0xFFFFFFFE, 0], sr)

    def select_length(self, *args):
        return self.run_at(self.symbols["_kui_toy_movie_refill_length"][0], args)


def linked_roundtrip(image, aux):
    cases = 0
    for variant, sr, enabled in product((0, 1), (0x40000000, 0x400000F1), (False, True)):
        cpu = Replay(image, aux, variant)
        if enabled:
            cpu.latch()
        sectors = CAPPED if enabled else STOCK
        fad = low.RANGES[variant][0] + 150 + 550
        dest = original.RING + POSITIONED
        assert cpu.stock_refill(sr) == sectors * SECTOR
        assert cpu.requests == [(fad, sectors, dest & 0x1FFFFFFF, 0)]
        assert cpu.read(cpu.avi + 0x180) == sectors * SECTOR
        assert cpu.read(refill.FILE + 28) == sectors
        assert cpu.read(DEVICE + DEVICE_OWNED) == refill.FILE
        assert cpu.read(refill.FILE + POSITION) == 550
        cpu.run_at(0x8C0B1426, [0, 0, 0, 0], sr)
        assert cpu.read(refill.FILE + POSITION) == 550 + sectors
        assert cpu.read(DEVICE + DEVICE_OWNED) == 0
        cpu.write(low.CTX + 0x1C0, FRAME + 1)
        cpu.write(cpu.avi + 0x44, 4)
        cpu.write(original.ENTRIES + 36, POSITIONED + 2 * STOCK * SECTOR)
        cpu.write(original.ENTRIES + 40, 0x800)
        cpu.write(original.ENTRIES + 44, 7)
        assert cpu.stock_refill(sr) == sectors * SECTOR
        assert cpu.requests[-1] == (fad + sectors, sectors, (dest + sectors * SECTOR) & 0x1FFFFFFF, 0)
        assert cpu.read(cpu.avi + 0x68) == POSITIONED + sectors * SECTOR
        assert cpu.read(cpu.avi + 0x60) == POSITIONED + sectors * SECTOR
        assert cpu.cap_state() == ((low.CTX, FRAME + 1, 2, 10, FRAME + 2) if enabled else (0,) * 5)
        cases += 1
    return cases


def guard_matrix(image, aux):
    cases = 0
    for address, value, width in (
        (low.CTX, original.AVI + 4, 4), (original.AVI + 16, 1, 4),
        (original.AVI + 0x64, POSITIONED - 0x40000, 4),
        (original.AVI + 0x64, POSITIONED - 0x2000, 4),
        (original.AVI + 0x18, POSITIONED + 0x2000, 4),
        (refill.OWNER + 108, refill.FILE + 4, 4), (refill.FILE + FILE_MODE, 1, 4),
        (DEVICE + 12, 0x8C0C6264, 4), (DEVICE + DEVICE_OWNED, refill.FILE, 4),
        (refill.FILE + FILE_LENGTH, 551, 4), (refill.FILE + POSITION, 384, 4),
        (refill.FILE + START, 0, 4), (GD_COMMAND, 17, 4), (GD_PENDING, 1, 4),
        (low.SERVICE + 84, 1, 4), (low.SERVICE + 40, 2352, 4),
        (refill.ACTIVE, 1, 4), (ENGINE_IRQ, 2, 4), (refill.SCI_STATE, 2, 4),
        (PORT + 15, 0, 1), (PORT + 16, 1, 1), (0x8C0B0228, 2048, 4),
        (0x8C11546C, 0x800, 4),
    ):
        cpu = Replay(image, aux, 1)
        cpu.latch()
        cpu.write(address, value, width)
        params = (cpu.avi, refill.FILE, STOCK * SECTOR, original.RING + POSITIONED)
        assert cpu.select_length(*params) == STOCK * SECTOR, hex(address)
        assert cpu.cap_state() == (low.CTX, 0, 0, 0, 0)
        cases += 1
    return cases


def hook_abi(image, aux):
    cases = 0
    for tracked, sr, bytes, alignment in product(
            (False, True), (0x40000000, 0x400000F1),
            (0, CAPPED * SECTOR, STOCK * SECTOR), (0, 4)):
        cpu = Replay(image, aux, 1)
        cpu.latch()
        pr = refill.RETURN if tracked else refill.OUT
        cpu.pc, cpu.pr, cpu.r[15], cpu.sr = cpu.refill_hook, pr, low.STACK, sr
        cpu.r[4:8] = [refill.FILE, original.RING + POSITIONED + alignment, bytes, 0xABCDEF]
        cpu.r[14] = cpu.avi
        expected = cpu.r[4:16].copy()
        if tracked and bytes == STOCK * SECTOR and not alignment:
            expected[2] = CAPPED * SECTOR
        for _ in range(1000):
            if cpu.pc == refill.ENQUEUE:
                break
            cpu.step()
        assert cpu.pc == refill.ENQUEUE and cpu.pr == pr
        assert cpu.r[4:16] == expected and cpu.sr == sr, (tracked, sr, bytes, alignment)
        cases += 1
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("aux", type=Path)
    args = parser.parse_args()
    image = args.original.read_bytes()
    assert len(image) == original.SOURCE_BYTES and sha256(image).hexdigest() == original.SOURCE_SHA256
    aux = original.read_elf(args.aux)
    bind_fixture(aux)
    Replay.symbols = aux[4]
    routes = linked_roundtrip(image, aux)
    guards = guard_matrix(image, aux)
    abi = hook_abi(image, aux)
    print(f"Test 61: {routes} original AVI/GDFS/GD round trips, "
          f"{guards} cap vetoes and {abi} exact wrapper ABI cases passed")


if __name__ == "__main__":
    main()
