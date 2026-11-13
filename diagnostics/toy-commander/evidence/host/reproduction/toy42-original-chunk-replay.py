#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the original AVI selection and linked Test 42 call-site witness.

The fixture supplies only stream objects and entries. The native call site,
selection and return paths execute the supplied original SH instructions. The
observer ABI boundary is modeled. No game bytes are embedded.
"""

import argparse
from hashlib import sha256
from itertools import product
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent / "trace-reader"
sys.path[:0] = [str(ROOT / "tests"), str(ROOT / "tools")]
from check_vmu_native_worker_gateway import read_elf, symbol_bytes
from profile_vmu_native_release_sh import ReleaseSH
from sh4_delay_slots import validate_delay_slot
from test_toy_original_gd_poll import SOURCE_BYTES, SOURCE_SHA256, STACK
from test_vmu_native_heap_execution import MASK, SH, signed


IMAGE_BASE = 0x8C010000
CALL_PREPARE = 0x8C08D884
CALL_RETURN = 0x8C08D898
CALL_LITERAL = 0x8C08D950
ACQUIRE = 0x8C08EDBC
RING_MODULO = 0x8C081B74
OTHER_RETURN = 0x8C008A00
CTX, STREAM, CHANNEL, AVI, ENTRIES, RING = (0x8C200000 + n * 0x400 for n in range(6))
CASES = {
    "ready": (5, 128, 7, 1),
    "empty-ready": (1, 0, 7, 1),
    "pending": (0, 128, 7, -2),
    "unavailable": (2, 128, 7, -4),
    "missing": (1, 128, 8, -1),
}


class ChunkReplay(ReleaseSH):
    def __init__(self, image, aux, case, tracked, initial_t):
        SH.__init__(self, image)
        self.active = False
        self.macl = 0
        self.hook = self.observer = None
        self.observations = []
        self.original_entries = []
        self.tracked = tracked
        self.initial_t = initial_t
        self.return_pc = CALL_RETURN if tracked else OTHER_RETURN
        self.expected_stack_arg = 1 if tracked else 2
        self.write(CTX + 8, STREAM)
        self.write(STREAM + 4, CHANNEL)
        self.write(STREAM + 12, 30)
        self.write(CHANNEL + 4, 7)
        self.write(CHANNEL + 68, AVI)
        self.write(AVI + 20, 2048)
        self.write(AVI + 28, RING)
        self.write(AVI + 56, ENTRIES)
        self.write(AVI + 68, 3)
        flags, count, kind, _ = CASES[case]
        for index in (1, 2):
            entry = ENTRIES + index * 12
            self.write(entry, 0x811 + 4 * (index - 1))
            self.write(entry + 4, count)
            self.write(entry + 8, kind, 1)
            self.write(entry + 9, flags, 1)
        if aux:
            data, _, segments, _, symbols = aux
            self.hook = symbols["_kui_toy_movie_chunk_hook"][0]
            self.observer = symbols["_kui_toy_movie_chunk_observe"][0]
            for kind, offset, address, _, stored, memory, _, _ in segments:
                if kind == 1:
                    self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
            self.write(CALL_LITERAL, self.hook)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF0FF == 0x002A:  # sts PR,Rn
            self.r[n] = self.pr
        elif op & 0xF0FF == 0x402A:  # lds Rn,PR
            self.pr = self.r[n]
        elif op & 0xF0FF == 0x4011:  # cmp/pz Rn
            self.flag(signed(self.r[n], 32) >= 0)
        elif op & 0xF0FF == 0x4015:  # cmp/pl Rn
            self.flag(signed(self.r[n], 32) > 0)
        elif op & 0xF00F in (0x3003, 0x3007):  # cmp/ge and cmp/gt
            left, right = signed(self.r[n], 32), signed(self.r[m], 32)
            self.flag(left >= right if op & 15 == 3 else left > right)
        elif op & 0xF00F == 0x000C:  # mov.b @(R0,Rm),Rn
            self.r[n] = signed(self.read((self.r[0] + self.r[m]) & MASK, 1), 8) & MASK
        elif op & 0xFF00 == 0x8400:  # mov.b @(disp,Rm),R0
            self.r[0] = signed(self.read(self.r[m] + (op & 15), 1), 8) & MASK
        elif op == 0x0019:  # div0u clears Q/M/T
            self.sr &= ~0x301
        elif op & 0xF0FF == 0x4024:  # rotcl Rn
            value = self.r[n]
            self.r[n] = ((value << 1) | (self.sr & 1)) & MASK
            self.flag(value >> 31)
        elif op & 0xF00F == 0x3004:  # div1 Rm,Rn
            old_q, mode = (self.sr >> 8) & 1, (self.sr >> 9) & 1
            q = self.r[n] >> 31
            value = ((self.r[n] << 1) | (self.sr & 1)) & MASK
            if old_q == mode:
                updated = (value - self.r[m]) & MASK
                carry = updated > value
            else:
                updated = (value + self.r[m]) & MASK
                carry = updated < value
            new_q = int(carry) ^ q ^ mode
            self.r[n] = updated
            self.sr = (self.sr & ~0x101) | (new_q << 8) | int(new_q == mode)
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def run_chunk(self):
        self.flag(self.initial_t)
        self.r[14] = CTX
        saved = self.r[8:15]
        args = (STREAM, CTX + 0x84, CTX + 0x7C, 1)
        if self.tracked:
            self.pc, self.r[15] = CALL_PREPARE, STACK + 4
        else:
            self.pc, self.r[15] = self.hook or ACQUIRE, STACK
            self.pr = OTHER_RETURN
            self.r[4:8] = args
            self.write(STACK, self.expected_stack_arg)
        for _ in range(512):
            pc = self.pc
            if pc == self.return_pc:
                assert self.pr == pc and self.r[15] == STACK and self.r[8:15] == saved
                assert self.read(STACK) == self.expected_stack_arg
                assert len(self.original_entries) == 1
                return self.r[0], self.sr, self.block(CTX, 0x250), self.block(STREAM, 0x40)
            if pc == ACQUIRE:
                assert tuple(self.r[4:8]) == args and self.sr & 1 == self.initial_t
                assert self.read(self.r[15]) == self.expected_stack_arg
                assert self.r[15] == STACK - (16 if self.hook and self.tracked else 0)
                if not self.tracked:
                    assert self.pr == OTHER_RETURN
                self.original_entries.append(pc)
            if pc == RING_MODULO:
                assert self.pr == 0x8C08EF06 and self.r[0] == 2048
            if pc == self.observer:
                assert self.tracked and self.r[4] == CTX and self.r[15] == STACK - 16
                self.observations.append(self.r[5])
                self.flag(not self.sr & 1)
                self.r[:8] = [0xDEAD0000 + i for i in range(8)]
                self.pc = self.pr
                continue
            self.step()
        raise AssertionError("AVI acquisition exceeded its bounded path")

    def step(self):
        pc = self.pc
        op = self.read(pc, 2)
        target, delay = pc + 2, False
        if op >> 12 == 0xA:  # bra
            target, delay = pc + 4 + 2 * signed(op & 0xFFF, 12), True
        elif op & 0xF0FF in (0x400B, 0x402B):  # jsr and jmp
            target, delay = self.r[(op >> 8) & 15], True
            if op & 0xF0FF == 0x400B:
                self.pr = pc + 4
        elif op == 0x000B:  # rts
            target, delay = self.pr, True
        elif op >> 8 in (0x89, 0x8B, 0x8D, 0x8F):
            delay = op >> 8 in (0x8D, 0x8F)
            if bool(self.sr & 1) == (op >> 8 in (0x89, 0x8D)):
                target = pc + 4 + 2 * signed(op & 255, 8)
            else:
                target = pc + (4 if delay else 2)
        else:
            self.plain(op, pc)
        if delay:
            slot = self.read(pc + 2, 2)
            validate_delay_slot(slot, pc)
            self.plain(slot, pc + 2)
        self.pc = target & MASK


def verify_case(image, aux, case, tracked, initial_t):
    baseline = ChunkReplay(image, None, case, tracked, initial_t)
    expected = CASES[case][3]
    if expected >= 0 and not tracked:
        expected = 2
    result = baseline.run_chunk()
    assert signed(result[0], 32) == expected, (case, result[0], expected)
    if case == "ready":
        assert baseline.read(CTX + 0x84) == RING + 2048 + 47 + 4 * (expected - 1)
    linked = ChunkReplay(image, aux, case, tracked, initial_t)
    assert linked.run_chunk() == result, (case, tracked, initial_t)
    assert linked.observations == ([expected & MASK] if tracked else [])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("aux", type=Path, help="Linked Test 42 auxiliary ELF")
    args = parser.parse_args()
    image = args.original.read_bytes()
    assert len(image) == SOURCE_BYTES and sha256(image).hexdigest() == SOURCE_SHA256
    assert struct.unpack_from("<I", image, CALL_LITERAL - IMAGE_BASE) == (ACQUIRE,)
    aux = read_elf(args.aux)
    entry, _, section = symbol_bytes(aux, "_kui_toy_movie_chunk_hook", 2)
    _, exports, _ = symbol_bytes(aux, "_kui_native_readahead_aux_exports", 1)
    assert section[2] & 7 == 6 and not entry & 1
    words = struct.unpack("<16I", exports)
    assert words[11:] == (entry, 0, 0, 0, 0) and words[10] != entry
    for case, tracked, initial_t in product(CASES, (True, False), (0, 1)):
        verify_case(image, aux, case, tracked, initial_t)
    print("Original AVI acquisition plus linked witness: 20 return, T and fifth-argument cases.")


if __name__ == "__main__":
    main()
