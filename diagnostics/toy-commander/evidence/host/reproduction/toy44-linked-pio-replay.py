#!/usr/bin/env python3
"""Replay the exact resident SCI helper against bounded synthetic SCI flags.

No target execution or SH-4 cycle timing is implied. The card token already
arrived. The model grants the next wire byte only after the previous RDR byte
has been drained and its status cleared. All three linked SH-4 bodies execute.
"""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "trace-reader" / "tools"))
from profile_vmu_native_release_sh import ReleaseSH
from test_vmu_native_heap_execution import MASK

SCR, TDR, SSR, RDR = 0xFFE00008, 0xFFE0000C, 0xFFE00010, 0xFFE00014
TDRE, RDRF, ORER, TEND = 0x80, 0x40, 0x20, 0x04
REGION = 0x8C408000
PORT = REGION + 0x4E4
WORK, ACQUIRED, FAULT, SLOW = PORT, PORT + 15, PORT + 16, PORT + 17
DEST = 0x8C450020
COUNT = 514
HELPER = "_kui_sci_sd_pio_payload"


def reverse(value):
    return int(f"{value:08b}"[::-1], 2)


def payload(seed):
    return bytes((index * 37 + seed) & 0xFF for index in range(512)) + b"\x96\x3d"


class SciCredit:
    def __init__(self, incoming, timeout, overrun):
        self.incoming, self.timeout, self.overrun = incoming, timeout, overrun
        self.scr, self.ssr, self.tdr, self.rdr = 0x30, TDRE | TEND, 0, 0
        self.launches = self.drains = self.tdr_writes = self.scr_writes = 0
        self.status_reads = self.wait = self.read_flags = 0

    def read(self, address):
        if address == SSR:
            self.status_reads += 1
            if self.wait:
                self.ssr |= TDRE
                if self.launches != self.timeout:
                    self.wait -= 1
                    if not self.wait:
                        assert not self.ssr & RDRF
                        self.ssr |= TEND
                        if self.launches == self.overrun:
                            self.ssr |= ORER
                        else:
                            self.rdr = reverse(self.incoming[self.launches - 1])
                            self.ssr |= RDRF
            self.read_flags |= self.ssr
            return self.ssr
        assert address == RDR and self.read_flags & RDRF
        assert self.ssr & RDRF and self.launches == self.drains + 1 and not self.wait
        self.drains += 1
        return self.rdr

    def write(self, address, value):
        value &= 0xFF
        if address == SCR:
            assert value in (0, 0x30)
            self.scr_writes += 1
            self.scr = value
            if not value and self.wait:
                assert self.launches == self.timeout
                self.wait = 0
            return
        if address == TDR:
            assert value == 0xFF and self.read_flags & TDRE
            self.tdr_writes += 1
            self.tdr = value
            return
        assert address == SSR and value in (0x7C, 0xBC)
        launch = self.scr & 0x20 and self.ssr & TDRE and not value & TDRE
        if self.ssr & RDRF and not value & RDRF:
            assert self.read_flags & RDRF and self.launches == self.drains
            self.read_flags &= ~RDRF
        self.ssr = (self.ssr & value & 0xF8) | (self.ssr & 7)
        if launch:
            assert self.read_flags & TDRE and self.tdr == 0xFF and self.scr == 0x30
            assert not self.wait and self.launches == self.drains and not self.ssr & RDRF
            self.launches += 1
            self.wait = 4 + self.launches % 5
            self.read_flags &= ~TDRE
            self.ssr &= ~TEND


class Replay(ReleaseSH):
    def __init__(self, path, sci):
        self.sci = sci
        super().__init__(path)

    def read(self, address, count=4):
        if address >= 0xE0000000:
            assert count == 1 and address in (SSR, RDR), (hex(address), count)
            return self.sci.read(address)
        return super().read(address, count)

    def write(self, address, value, count=4):
        if address >= 0xE0000000:
            assert count == 1 and address in (SCR, TDR, SSR), (hex(address), count)
            self.sci.write(address, value)
        else:
            super().write(address, value, count)

    def plain(self, op, pc):
        if op & 0xFF00 == 0x8400:  # mov.b @(disp,Rm),R0
            value = self.read(self.r[(op >> 4) & 15] + (op & 15), 1)
            self.r[0] = (value - 256 if value & 128 else value) & MASK
            self.instructions += 1
            self.executing = pc
            return
        return super().plain(op, pc)


def check(reader, seed, condition, timeout=0, overrun=0):
    expected = payload(seed)
    machine = SciCredit(expected, timeout, overrun)
    cpu = Replay(reader, machine)
    cpu.write(cpu.symbols["_kui_retail_async_region_storage"][0], REGION)
    cpu.write(WORK, 0x12345678)
    cpu.write(ACQUIRED, condition != "unacquired", 1)
    cpu.write(FAULT, condition == "fault", 1)
    cpu.write(SLOW, condition == "slow", 1)
    cpu.store_block(DEST - 16, b"\xa5" * (COUNT + 32))
    run = condition == "fast"
    failed = bool(timeout or overrun)
    result = cpu.call(HELPER, [DEST], measure=False)
    assert result["result"] == int(run and not failed), result
    assert machine.tdr_writes == int(run)
    assert machine.scr_writes == int(run) + int(failed)
    assert machine.launches == (timeout or overrun or COUNT if run else 0)
    assert machine.drains == machine.launches - int(failed)
    assert cpu.read(WORK) == 0x12345678 + (COUNT * 8 if run else 0)
    assert cpu.read(FAULT, 1) == int(condition == "fault" or failed)
    assert cpu.block(DEST - 16, COUNT + 32) == (
        b"\xa5" * 16 + expected[:machine.drains] + b"\xa5" * (COUNT + 16 - machine.drains))
    assert machine.status_reads < 14_000
    return result["instructions"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("reader", type=Path, nargs="+")
    args = parser.parse_args()
    for reader in args.reader:
        cases = [check(reader, seed, "fast") for seed in (11, 53, 209)]
        cases.extend(check(reader, 11, condition) for condition in ("unacquired", "slow", "fault"))
        cases.extend(check(reader, 11, "fast", timeout=at) for at in (1, 77, 514))
        cases.extend(check(reader, 11, "fast", overrun=at) for at in (1, 77, 514))
        print(f"{reader}: {len(cases)} exact linked SH-4 PIO cases, {sum(cases)} instructions")


if __name__ == "__main__":
    main()
