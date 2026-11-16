#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay the exact Toy Commander GDFS paths around GD EXEC and CHECK.

Requires the original private executable as input. The test models BIOS GD
returns, ASIC status and cache-line addresses. Game instructions and stack
frames execute from the executable with initialized objects supplied by the
test. The ASIC tests start at the SDK's installed dispatchers after hardware
interrupt acceptance, so the varied entry masks test the local code.
"""

import argparse
from hashlib import sha256
from itertools import product
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from profile_vmu_native_release_sh import ReleaseSH
from check_vmu_native_worker_gateway import read_elf, symbol_bytes
from sh4_delay_slots import validate_delay_slot
from test_vmu_native_heap_execution import SH, MASK, signed


SOURCE_BYTES = 748444
SOURCE_SHA256 = "ee68a326da964fec5892b7226311f2293fd3a0bbf5bac496ec82991964d49bbd"
MOVIE_POLL = 0x8C097114
ALIGNED_REFILL = 0x8C0B2A98
ASIC_DISPATCH, ASIC_FPU_DISPATCH = 0x8C085B8C, 0x8C085C78
ASIC_NORMAL_STATUS, ASIC_EXTERNAL_STATUS = 0xA05F6900, 0xA05F6908
ASIC_CONTEXT, ASIC_CALLBACKS = 0x8C1135F4, 0x8C1135F4 + 48
ASIC_EVENT, ASIC_CALLBACK = 29, 0x8C0884B0
SCAN_INTERRUPT = 1 << 3
EVENT_WRAPPER = 0x8C0B3B6E
EVENT_STEP = 0x8C0B1704
EVENT_HEAD = 0x8C117CFC
EVENT_LIST = 0x8C113574
EVENT_NODE = 0x8C116ACC
GDFS_MANAGER_CELL = 0x8C0C69EC
GDFS_DISPATCH_CELL = 0x8C0C69F8
GDFS_DISPATCH = 0x8C0C5980
GD_BACKEND = 0x8C0C6260
GD_VECTOR = 0x8C0000BC
GD_FIRMWARE = 0x8C000800
MANAGER, FILE, TOKEN = 0x8C117F00, 0x8C118000, 0x42
REFILL_LBA, REFILL_SECTORS, REFILL_DST = 0x70000, 14, 0x8C200000
STACK, RETURN = 0x8C00E000, 0x8C008A00
REQUEST, CHECK, EXEC = 0, 1, 2
GD_PROCESSING, GD_COMPLETED = 1, 2
IMASK = 0xF0
CONTINUATIONS = {
    "event": 0x8C0B3B88,
    "event_fpu": 0x8C0B3B88,
    "nonblocking": 0x8C097144,
    "blocking": 0x8C09712A,
    "refill": 0x8C0B2B4E,
}


class ToyGdReplay(ReleaseSH):
    def __init__(self, image, completed_on, resident):
        SH.__init__(self, image)
        self.active = False
        self.completed_on = completed_on
        self.events = []
        self.cache_invalidations = []
        self.asic_status = {ASIC_NORMAL_STATUS: SCAN_INTERRUPT, ASIC_EXTERNAL_STATUS: 0}
        self.mach, self.macl, self.fpscr = 0x45671234, 0x87653210, 0x180001
        self.fr = [0x12345678 + i for i in range(16)]
        self.checks = 0
        self.hook = self.dispatch = self.poll_frame = self.caller = None
        self.publications = 0
        self.resident_code = []
        if resident:
            data, _, segments, _, symbols = resident
            bottom = symbols["__retail_hook_stack_bottom"][0]
            top = symbols["__retail_hook_stack"][0]
            assert (bottom, top) in ((0x8C007BA0, 0x8C007D00), (0x8C007CE0, 0x8C007E40),
                                     (0x8C007D60, 0x8C007EC0), (0x8C007E80, 0x8C007FE0))
            for kind, offset, address, _, stored, memory, flags, _ in segments:
                if kind == 1:
                    assert 0x8C004000 <= address <= bottom - memory
                    self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
                    if flags & 1:
                        self.resident_code.append(range(address, address + stored))
            self.hook = symbols["_kui_retail_resident_hook"][0]
            self.dispatch = symbols["_kui_retail_resident_dispatch"][0]
            self.poll_frame = symbols["_kui_toy_poll_frame"][0]
            self.hook_active = symbols["_kui_retail_hook_active"][0]
            self.hook_source = symbols["_kui_retail_hook_source"][0]
            self.hook_stack = top - 16
            self.hook_fault = symbols["_kui_retail_hook_fault"][0]
            self.write(bottom, 0x4B554947)
        self.write(GD_VECTOR, self.hook or GD_FIRMWARE)
        self.write(GDFS_MANAGER_CELL, MANAGER)
        self.write(GDFS_DISPATCH_CELL, GDFS_DISPATCH)
        self.write(MANAGER + 12, GD_BACKEND)
        self.write(MANAGER + 28, FILE)
        self.write(FILE + 60, TOKEN)
        self.write(FILE + 76, 1, 2)  # Open file handle.
        self.write(FILE + 80, 2, 2)  # GDFS file is reading.
        self.write(ASIC_CONTEXT + 24, SCAN_INTERRUPT)
        self.write(ASIC_CALLBACKS + 16 * ASIC_EVENT, ASIC_CALLBACK)
        self.write(EVENT_LIST + 8, EVENT_WRAPPER)
        self.write(EVENT_HEAD, EVENT_NODE)
        self.write(EVENT_NODE, EVENT_STEP)
        self.write(EVENT_NODE + 8, MANAGER)

    def read(self, address, count=4):
        if address in self.asic_status:
            assert count == 4
            return self.asic_status[address]
        return super().read(address, count)

    def write(self, address, value, count=4):
        if address in self.asic_status:
            assert count == 4 and address == ASIC_NORMAL_STATUS and value == SCAN_INTERRUPT
            self.asic_status[address] &= ~value
        else:
            if address == self.poll_frame:
                pr, stack, sr = self.caller
                assert count == 4 and self.r[15] == self.r[8] == value == stack - 36
                assert self.read(value + 28) == pr
                assert self.read(value + 32) == sr
                assert self.read(self.hook_active) == 1 and self.read(self.hook_source) == 0
                self.publications += 1
            super().write(address, value, count)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF00F == 0x0006:  # mov.l Rm,@(R0,Rn)
            self.write((self.r[0] + self.r[n]) & MASK, self.r[m])
        elif op & 0xF00F == 0x3003:  # cmp/ge Rm,Rn
            self.flag(signed(self.r[n], 32) >= signed(self.r[m], 32))
        elif op & 0xF00F == 0x3007:  # cmp/gt Rm,Rn
            self.flag(signed(self.r[n], 32) > signed(self.r[m], 32))
        elif op & 0xF0FF == 0x0093:  # ocbi @Rn
            self.cache_invalidations.append(self.r[n])
        elif op & 0xF0FF == 0x4018:  # shll8 Rn
            self.r[n] = self.r[n] << 8 & MASK
        elif op & 0xF0FF == 0x006A:  # sts FPSCR,Rn
            self.r[n] = self.fpscr
        elif op & 0xF0FF == 0x406A:  # lds Rn,FPSCR
            self.fpscr = self.r[n]
        elif op & 0xF0FF in (0x4002, 0x4012):  # sts.l MACH/MACL,@-Rn
            self.r[n] -= 4
            self.write(self.r[n], self.macl if op & 0x10 else self.mach)
        elif op & 0xF0FF in (0x4006, 0x4016):  # lds.l @Rn+,MACH/MACL
            value = self.read(self.r[n])
            self.r[n] += 4
            if op & 0x10:
                self.macl = value
            else:
                self.mach = value
        elif op & 0xF00F == 0xF00B:  # fmov FRm,@-Rn, single width
            assert not self.fpscr & 0x180000
            self.r[n] -= 4
            self.write(self.r[n], self.fr[m])
        elif op & 0xF00F == 0xF009:  # fmov @Rm+,FRn, single width
            assert not self.fpscr & 0x180000
            self.fr[n] = self.read(self.r[m])
            self.r[m] += 4
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def bios_gd(self):
        function = self.r[7]
        assert self.r[6] == 0
        if self.hook:
            pr, stack, sr = self.caller
            assert self.r[15] == self.hook_stack
            assert self.read(self.poll_frame) == stack - 36
            assert self.sr & 0x100000F0 == 0x100000F0
            assert self.read(self.hook_active) == 1 and self.read(self.hook_fault) == 0
            self.caller = None
        else:
            pr, stack, sr = self.pr, self.r[15], self.sr
        if function == REQUEST:
            assert pr == 0x8C0BD2B6 and self.r[4] == 17
            assert [self.read(self.r[5] + 4 * i) for i in range(4)] == [
                REFILL_LBA, REFILL_SECTORS, REFILL_DST & 0x1FFFFFFF, 0,
            ]
            continuation = self.read(stack + 20)
            self.r[0] = TOKEN
        elif function == EXEC:
            assert pr == 0x8C0BD55A
            continuation = self.read(stack)
            if continuation == 0x8C0B1FE6:
                continuation = self.read(stack + 4)
            self.r[0] = 0
        else:
            assert function == CHECK and pr == 0x8C0BD374
            assert self.read(stack + 16) == 0x8C0B145C
            continuation = self.read(stack + 64)
            assert self.r[4] == TOKEN
            assert self.r[5] == stack
            self.checks += 1
            complete = self.checks >= self.completed_on
            for i, word in enumerate((0, 0, 14 * 2048 if complete else 0, 0)):
                self.write(self.r[5] + 4 * i, word)
            self.r[0] = GD_COMPLETED if complete else GD_PROCESSING
        self.events.append((function, sr & IMASK, continuation))
        self.pc = self.pr

    def run_poll(self, entry, mask, args):
        saved = self.r[8:15]
        saved_math = self.fr[:], self.fpscr, self.mach, self.macl
        self.pc, self.pr, self.r[15] = entry, RETURN, STACK
        self.sr = 0x40000000 | mask << 4
        self.r[4:4 + len(args)] = args
        for _ in range(4000):
            pc = self.pc
            if pc == RETURN:
                assert self.pr == RETURN and self.r[15] == STACK
                assert self.r[8:15] == saved
                assert (self.fr, self.fpscr, self.mach, self.macl) == saved_math
                assert not self.sr & 0x10000000  # No new SR.BL.
                if self.hook:
                    assert self.publications == len(self.events)
                    assert self.read(self.hook_active) == self.read(self.hook_fault) == 0
                return self.r[0]
            if pc == self.hook:
                assert self.caller is None
                self.caller = self.pr, self.r[15], self.sr
            if pc == (self.dispatch or GD_FIRMWARE):
                self.bios_gd()
                continue
            in_original = 0x8C010000 <= pc < 0x8C010000 + SOURCE_BYTES
            assert in_original or any(pc in region for region in self.resident_code), hex(pc)
            op = self.read(pc, 2)
            target, delayed = pc + 2, False
            if op >> 12 in (0xA, 0xB):
                target = pc + 4 + 2 * signed(op & 0xFFF, 12)
                delayed = True
                if op >> 12 == 0xB:
                    self.pr = pc + 4
            elif op & 0xF0FF in (0x400B, 0x402B):
                target, delayed = self.r[(op >> 8) & 15], True
                if op & 0xF0FF == 0x400B:
                    self.pr = pc + 4
            elif op == 0x000B:
                target, delayed = self.pr, True
            elif op >> 8 in (0x89, 0x8B, 0x8D, 0x8F):
                delayed = op >> 8 in (0x8D, 0x8F)
                taken = bool(self.sr & 1) == (op >> 8 in (0x89, 0x8D))
                target = pc + 4 + 2 * signed(op & 255, 8) if taken else pc + (4 if delayed else 2)
            else:
                self.plain(op, pc)
            if delayed:
                slot = self.read(pc + 2, 2)
                validate_delay_slot(slot, pc)
                self.plain(slot, pc + 2)
            self.pc = target & MASK
        raise AssertionError("Original GDFS poll exceeded its instruction bound")


def check_poll_paths(image, resident):
    count = 0
    for mask, route in product(range(16), CONTINUATIONS):
        continuation = CONTINUATIONS[route]
        if route in ("event", "event_fpu"):
            cpu = ToyGdReplay(image, 2, resident)
            entry = ASIC_DISPATCH if route == "event" else ASIC_FPU_DISPATCH
            cpu.run_poll(entry, mask, ())
            assert cpu.asic_status[ASIC_NORMAL_STATUS] == 0
            expected_mask, passes = 6, 1
        elif route == "nonblocking":
            cpu = ToyGdReplay(image, 2, resident)
            assert cpu.run_poll(MOVIE_POLL, mask, (FILE, 0)) == 0
            expected_mask, passes = mask, 1
        elif route == "blocking":
            cpu = ToyGdReplay(image, 3, resident)
            assert cpu.run_poll(MOVIE_POLL, mask, (FILE, 1)) == 1
            expected_mask, passes = mask, 3
        else:
            cpu = ToyGdReplay(image, 2, resident)
            cpu.write(MANAGER + 28, 0)
            cpu.write(FILE, MANAGER)
            cpu.write(FILE + 8, REFILL_LBA)
            cpu.write(FILE + 16, 64)
            cpu.write(FILE + 78, 1, 2)  # Aligned direct DMA.
            assert cpu.run_poll(ALIGNED_REFILL, mask, (FILE, REFILL_SECTORS, REFILL_DST)) == REFILL_SECTORS
            assert cpu.read(FILE + 60) == TOKEN
            assert cpu.cache_invalidations == list(range(REFILL_DST, REFILL_DST + 14 * 2048, 32))
            expected_mask, passes = mask, 0
        if route == "refill":
            expected = [(REQUEST, 0xF0, 0x8C0B2B24), (EXEC, 0xF0, continuation)]
        else:
            expected = [(EXEC, expected_mask << 4, continuation),
                        (CHECK, expected_mask << 4, continuation)] * passes
        assert cpu.events == expected, (route, mask, cpu.events)
        assert cpu.sr & IMASK == expected_mask << 4
        count += len(cpu.events)
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("--resident", type=Path, help="Linked Toy movie-poll SCI resident ELF")
    parser.add_argument("--stage", type=Path, help="Linked Toy movie-poll stage ELF")
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == SOURCE_BYTES and sha256(original).hexdigest() == SOURCE_SHA256
    if args.stage:
        stage = read_elf(args.stage)
        digests = [name for name in stage[4]
                   if name.split(".")[0] == "_toy_poll_digest"]
        assert len(digests) == 1, "Linked stage is missing the exact poll-owner digest"
        _, digest, _ = symbol_bytes(stage, digests[0], 1)
        assert digest == sha256(original).digest(), (
            "Linked stage's poll-owner digest rejects the original executable", digest.hex(),
        )
    resident = read_elf(args.resident) if args.resident else None
    calls = check_poll_paths(original, resident)
    entry = "linked GD hook" if resident else "firmware stub"
    print(f"Original Toy GDFS via {entry}: 80 cases, {calls} GD calls and exact caller frames verified")


if __name__ == "__main__":
    main()
