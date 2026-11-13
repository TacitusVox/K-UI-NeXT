#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay Toy's original output/scene paths with the linked Test 60 witness.

The physical rendering leaves and clock hardware are modeled. The original
output, both early render returns, scene dispatch and post-submit rotation run
their real instructions. The linked witness and timing arithmetic run as SH.
"""

import argparse
from hashlib import sha256
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / 'trace-reader/tests'))
from test_toy_original_movie_chunk import ChunkReplay
from test_toy_original_gd_poll import MASK, SH, SOURCE_BYTES, SOURCE_SHA256, STACK, read_elf, signed
from sh4_delay_slots import validate_delay_slot


OWNER = 0x8C200000
OTHER = OWNER + 0x400
OUTPUT = 0x8C09774C
RETURN = 0x8C008A00
PRESENT_RETURN = 0x8C04C484
SCENE_LITERAL = 0x8C04C568
SCENE_SLOT = 0x8C0FC464
SCENE = 0x8C008A20
BEGIN_SLOT = SCENE_SLOT - 4
BEGIN = 0x8C008A40
NEXT_TEXTURE = 0x8C0FCEC4
TEXTURE_RING = 0x8C0DF1D4
TEXTURE_CURSOR = TEXTURE_RING + 16
TEXTURE_COUNT = TEXTURE_RING + 20
CLOCK = 0xFFD8000C
FPS = 0xA0
FRAME = 0x1C0
OUT = 0xAC
SKIP = 0x90
OUT_CALLBACK = 0x8C04C238
PROBE_OFFSET = 1184
EXIT_OFFSET = 656
EXIT_RECORD = PROBE_RECORD = 36


class PresentReplay(ChunkReplay):
    def __init__(self, original, aux):
        SH.__init__(self, original)
        self.active = False
        self.macl = 0
        self.fr = [0] * 16
        self.present_count = 0
        self.clock_reads = 0
        self.clock = 5000000
        self.clock_valid = True
        self.early = None
        self.observations = []
        self.symbols = {}
        self.context = None
        if aux is not None:
            data, _, segments, _, self.symbols = aux
            for kind, offset, address, _, stored, memory, _, _ in segments:
                if kind == 1:
                    self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
            self.context = self.symbols['_kui_native_readahead_aux_context'][0]
            slot = self.symbols['_kui_toy_movie_present_slot'][0]
            assert self.read(slot) == self.symbols['_kui_toy_movie_present_hook'][0]
            assert self.read(SCENE_LITERAL) == SCENE_SLOT
            self.write(SCENE_LITERAL, slot | 0x20000000)
        self.write(SCENE_SLOT, SCENE)
        self.write(BEGIN_SLOT, BEGIN)
        self.write(TEXTURE_COUNT, 1)
        self.write(NEXT_TEXTURE, 0x12345678)
        for owner in (OWNER, OTHER):
            self.write(owner + 40, 0x8C04C234)
            self.write(owner + 44, OUT_CALLBACK)
            self.write(owner + 12, 1)

    def read(self, address, count=4):
        mmio = {0xFFC00000: (2, 0xE0A), 0xFFD80008: (4, MASK),
                0xFFD80010: (2, 2), 0xFFD80004: (1, int(self.clock_valid)),
                CLOCK: (4, self.clock)}
        if address in mmio:
            width, value = mmio[address]
            assert count == width
            if address == CLOCK:
                self.clock_reads += 1
            return value
        return super().read(address, count)

    def write(self, address, value, count=4):
        if 0xF4000000 <= address < 0xF4004000:
            assert count == 4 and value == 0
            return
        super().write(address, value, count)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xF0FF == 0xF08D:  # fldi0 Fn
            self.fr[n] = 0
        elif op & 0xF00F == 0xF008:  # fmov.s @Rm,Fn
            self.fr[n] = self.read(self.r[m])
        elif op & 0xF00F == 0xF006:  # fmov.s @(R0,Rm),Fn
            self.fr[n] = self.read((self.r[0] + self.r[m]) & MASK)
        elif op & 0xF00F == 0xF007:  # fmov.s Fm,@(R0,Rn)
            self.write((self.r[0] + self.r[n]) & MASK, self.fr[m])
        elif op & 0xF00F == 0xF00A:  # fmov.s Fm,@Rn
            self.write(self.r[n], self.fr[m])
        elif op & 0xF00F == 0x0006:  # mov.l Rm,@(R0,Rn)
            self.write((self.r[0] + self.r[n]) & MASK, self.r[m])
        elif op & 0xF0FF == 0x4000:  # shll
            self.flag(self.r[n] >> 31)
            self.r[n] = self.r[n] << 1 & MASK
        elif op & 0xF00F == 0x6009:  # swap.w
            value = self.r[m]
            self.r[n] = (value << 16 | value >> 16) & MASK
        elif op & 0xF00F == 0x200D:  # xtrct
            self.r[n] = (self.r[m] << 16 | self.r[n] >> 16) & MASK
        elif op & 0xFF00 == 0xC700:  # mova
            self.r[0] = ((pc + 4) & ~3) + 4 * (op & 255)
        elif op == 0x0019:  # div0u clears Q/M/T
            self.sr &= ~0x301
        elif op & 0xF0FF == 0x4024:  # rotcl Rn
            value = self.r[n]
            self.r[n] = (value << 1 | self.sr & 1) & MASK
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

    def step(self):
        op = self.read(self.pc, 2)
        if op >> 12 == 0xB:
            self.pr = self.pc + 4
            slot = self.read(self.pc + 2, 2)
            validate_delay_slot(slot, self.pc)
            self.plain(slot, self.pc + 2)
            self.pc = self.pr + 2 * signed(op & 0xFFF, 12)
        else:
            super().step()

    def execute(self, entry, args, terminal=RETURN):
        self.pc, self.pr, self.r[15] = entry, terminal, STACK
        self.r[4:4 + len(args)] = args
        saved = self.r[8:15]
        render_return = 0x8C0977B2 if entry == OUTPUT else terminal
        for _ in range(6000):
            pc = self.pc
            if pc == terminal:
                assert self.pr == terminal and self.r[15] == STACK and self.r[8:15] == saved
                return self.r[0], self.sr & 1
            if pc == self.symbols.get('_kui_toy_movie_present_observe', (None,))[0]:
                self.observations.append(self.r[4])
            if pc == PRESENT_RETURN:
                assert self.r[0] == 0x98765432 and not self.sr & 1
            if pc == SCENE:
                self.present_count += 1
                assert self.r[4] == 1 and self.sr & 1
                if entry == OUTPUT:
                    assert self.r[14] in (OWNER, OTHER)
                wrapper_bytes = 16 if self.context and entry == OUTPUT else 0
                assert self.read(self.r[15] + 80 + wrapper_bytes) == render_return
                self.r[0] = 0x98765432
                self.flag(False)
            elif pc == 0x8C06CBEC:
                self.r[0] = 0 if self.early == 'texture' else 103
            elif pc == BEGIN:
                self.r[0] = int(self.early != 'scene')
            elif pc == 0x8C04C1F8:
                self.r[0] = int(self.early is not None)
            elif pc in (0x8C06DDC8, 0x8C06E03A, 0x8C06DD66, 0x8C0583AE, 0x8C04BFE8):
                self.r[0] = 0
            else:
                self.step()
                continue
            self.pc = self.pr
        raise AssertionError('render replay exceeded bound')

    def boundary(self, clip, phase, status=0x7DB, fps=30):
        if not self.context:
            return
        witness = self.context + EXIT_OFFSET + clip * EXIT_RECORD
        self.write(witness + 4, status)
        self.write(OWNER + FPS, fps)
        self.execute(self.symbols['_kui_toy_movie_present_boundary'][0], (OWNER, witness, phase))

    def output(self, frame, elapsed, *, early=None, valid=True, owner=OWNER, display=True):
        self.clock = (self.clock - elapsed) & MASK
        self.clock_valid = valid
        self.early = early
        self.write(owner + FRAME, frame - 1)
        result = self.execute(OUTPUT, (owner, int(display)))
        return result, self.read(owner + OUT), self.read(owner + SKIP), self.read(TEXTURE_RING)

    def record(self, clip):
        return struct.unpack('<9I', self.block(self.context + PROBE_OFFSET + clip * PROBE_RECORD, PROBE_RECORD))


def verify(original, aux):
    bare = PresentReplay(original, None)
    probe = PresentReplay(original, aux)
    bare.boundary(0, 1)
    probe.boundary(0, 1)
    for frame, elapsed, flags in (
        (1, 20, {}),
        (2, 26000, {}),
        (3, 10000, {'early': 'texture'}),
        (4, 10000, {'early': 'scene'}),
        (5, 10000, {'display': False}),
        (6, 10000, {}),
        (7, 20, {'owner': OTHER}),
        (7, 20, {'valid': False}),
        (8, 20, {}),
        (9, 30, {}),
    ):
        assert bare.output(frame, elapsed, **flags) == probe.output(frame, elapsed, **flags)
    assert bare.present_count == probe.present_count == 7
    assert probe.observations == [OWNER, OWNER, OWNER, OTHER, OWNER, OWNER, OWNER]
    assert bare.execute(OUT_CALLBACK, (OWNER,)) == probe.execute(OUT_CALLBACK, (OWNER,))
    assert bare.present_count == probe.present_count == 8 and len(probe.observations) == 7
    probe.clock -= 2500000
    probe.boundary(0, 2)
    assert probe.record(0) == (6, 2, 20, 40000, 2, 2500000, 9, 4933890, 1), probe.record(0)
    assert probe.clock_reads == 8
    before = probe.record(0)
    probe.output(10, 20)
    assert probe.record(0) == before and probe.clock_reads == 8
    probe.boundary(1, 1, status=0xFFFFF837)
    probe.output(1, 20)
    assert probe.record(1) == (0,) * 9 and probe.clock_reads == 8

    probe.clock = 50
    probe.boundary(1, 1, fps=25)
    probe.output(1, 100)
    probe.output(2, 45000)
    probe.clock -= 90000
    probe.boundary(1, 2, fps=25)
    assert probe.record(0) == before
    assert probe.record(1) == (2, 1, 100, 45000, 1, 90000, 2, (50-100-45000) & MASK, 0), probe.record(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('aux', type=Path)
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == SOURCE_BYTES and sha256(original).hexdigest() == SOURCE_SHA256
    verify(original, read_elf(args.aux))
    print('Mode 60 original movie render and linked timing: actual submit, both early returns, '
          'skip, wrong owner, clock loss and wrap, both clip boundaries, long silent tail')


if __name__ == '__main__':
    main()
