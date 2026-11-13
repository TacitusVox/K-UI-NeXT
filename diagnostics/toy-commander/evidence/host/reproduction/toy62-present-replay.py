#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay Toy's original output/scene paths with the linked Test 62 witness.

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
PLAYER_INIT = 0x8C06D268
PLAYER_SETUP = 0x8C08D400
AVI_TIMING = 0x8C08E672
STREAM = 0x8C202000
CHANNEL = STREAM + 0x400
RENDER_INSTALLER = 0x8C04C6EC
RENDER_LITERAL = 0x8C04C774
PROBE_OFFSET = 1184
EXIT_OFFSET = 656
EXIT_RECORD, PROBE_RECORD = 36, 44


class PresentReplay(ChunkReplay):
    def __init__(self, original, aux, runtime=None):
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
        self.render_hook = None
        self.scene_observations = 0
        if aux is not None:
            data, _, segments, _, self.symbols = aux
            if runtime is None:
                for kind, offset, address, _, stored, memory, _, _ in segments:
                    if kind == 1:
                        self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
            else:
                linked = 0x8C421000
                destination, image, wire = runtime
                relocated = bytearray(image)
                delta = destination - linked
                for at in range(72, len(wire), 8):
                    offset, previous = struct.unpack_from('<2I', wire, at)
                    assert struct.unpack_from('<I', relocated, offset)[0] == previous
                    struct.pack_into('<I', relocated, offset, (previous + delta) & MASK)
                self.store_block(destination, relocated)
                self.symbols = {name: (at + delta, *entry[1:])
                                for name, entry in self.symbols.items()
                                if 0x0C421000 <= ((at := entry[0]) & 0x1FFFFFFF) < 0x0C426000}
            self.context = self.symbols['_kui_native_readahead_aux_context'][0]
            slot = self.symbols['_kui_toy_movie_present_slot'][0]
            assert self.read(slot) == self.symbols['_kui_toy_movie_present_hook'][0]
            assert self.read(SCENE_LITERAL) == SCENE_SLOT
            self.write(SCENE_LITERAL, slot | 0x20000000)
            self.render_hook = self.symbols['_kui_toy_movie_render_hook'][0]
            assert self.read(RENDER_LITERAL) == OUT_CALLBACK
            self.write(RENDER_LITERAL, self.render_hook)
        self.write(SCENE_SLOT, SCENE)
        self.write(BEGIN_SLOT, BEGIN)
        self.write(TEXTURE_COUNT, 1)
        self.write(NEXT_TEXTURE, 0x12345678)
        for owner in (OWNER, OTHER):
            assert self.execute(PLAYER_INIT, (owner, RENDER_INSTALLER, 0x8C06DC30))[0] == 0
            assert self.read(owner + 32) == RENDER_INSTALLER
            assert self.read(owner + 44) != (self.render_hook or OUT_CALLBACK)
            if owner == OTHER:
                self.write(owner + 0xBA, self.read(owner + 0xBA, 1) & 127, 1)
            assert self.execute(PLAYER_SETUP, (owner,))[0] == 0x7DF
            assert self.read(owner + 44) == (self.render_hook or OUT_CALLBACK)
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
        for _ in range(6000):
            pc = self.pc
            if pc == terminal:
                assert self.pr == terminal and self.r[15] == STACK and self.r[8:15] == saved
                return self.r[0], self.sr & 1
            if pc == self.symbols.get('_kui_toy_movie_render_enter', (None,))[0]:
                self.observations.append(self.r[4])
            if pc == self.symbols.get('_kui_toy_movie_scene_observe', (None,))[0]:
                self.scene_observations += 1
            if pc == PRESENT_RETURN:
                assert self.r[0] == 0x98765432 and not self.sr & 1
            if pc == SCENE:
                self.present_count += 1
                assert self.r[4] == 1 and self.sr & 1
                if entry == OUTPUT:
                    assert self.r[14] in (OWNER, OTHER)
                if not self.context:
                    render_return = 0x8C0977B2 if entry == OUTPUT else terminal
                    assert self.read(self.r[15] + 80) == render_return
                self.r[0] = 0x98765432
                self.flag(False)
            elif pc == 0x8C06CBEC:
                self.r[0] = 0 if self.early == 'texture' else 103
            elif pc == BEGIN:
                self.r[0] = int(self.early != 'scene')
            elif pc == 0x8C04C1F8:
                self.r[0] = int(self.early is not None)
            elif pc == 0x8C068210:
                target, value, count = self.r[4:7]
                assert target in (OWNER, OTHER) and value == 0 and count == 0x250
                self.store_block(target, bytes(count))
                self.r[0] = target
            elif pc in (0x8C06DDC8, 0x8C06E03A, 0x8C06DD66, 0x8C0583AE, 0x8C04BFE8,
                        0x8C0973D8, 0x8C097534, 0x8C04C4B2):
                self.r[0] = 0
            else:
                self.step()
                continue
            self.pc = self.pr
        raise AssertionError('render replay exceeded bound')

    def boundary(self, clip, phase, status=0x7DB):
        if not self.context:
            return
        witness = self.context + EXIT_OFFSET + clip * EXIT_RECORD
        self.write(witness + 4, status)
        if phase == 1:
            self.write(OWNER + FPS, 0)
        self.execute(self.symbols['_kui_toy_movie_present_boundary'][0], (OWNER, witness, phase))

    def output(self, frame, elapsed, *, early=None, valid=True, owner=OWNER, display=True):
        self.clock = (self.clock - elapsed) & MASK
        self.clock_valid = valid
        self.early = early
        self.write(owner + FRAME, frame - 1)
        result = self.execute(OUTPUT, (owner, int(display)))
        return result, self.read(owner + OUT), self.read(owner + SKIP), self.read(TEXTURE_RING)

    def set_rate(self, fps):
        # Replay the original AVI rate write that runs during first UPDATE.
        self.write(STREAM + 4, CHANNEL)
        self.write(CHANNEL + 8, fps)
        self.execute(AVI_TIMING, (STREAM, 0, OWNER + FPS, OWNER + 0x80))
        assert self.read(OWNER + FPS) == fps

    def record(self, clip):
        return struct.unpack('<11I', self.block(self.context + PROBE_OFFSET + clip * PROBE_RECORD, PROBE_RECORD))


def verify(original, aux, runtime=None):
    bare = PresentReplay(original, None)
    probe = PresentReplay(original, aux, runtime)
    bare.boundary(0, 1)
    probe.boundary(0, 1)
    assert probe.read(OWNER + FPS) == 0
    assert probe.record(0)[1] == MASK
    # The original first UPDATE fills the frame rate after START returned.
    probe.set_rate(30)
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
    assert probe.observations == [OWNER, OWNER, OWNER, OWNER, OWNER, OTHER, OWNER, OWNER, OWNER]
    assert probe.scene_observations == 7
    assert bare.execute(OUT_CALLBACK, (OWNER,)) == probe.execute(OUT_CALLBACK, (OWNER,))
    assert bare.present_count == probe.present_count == 8 and len(probe.observations) == 9
    assert probe.scene_observations == 8
    probe.clock -= 2500000
    probe.boundary(0, 2)
    assert probe.record(0) == (6, 2, 20, 40000, 2, 2500000, 9, 4933890, 1, 9, 8), probe.record(0)
    assert probe.clock_reads == 8
    before = probe.record(0)
    probe.output(10, 20)
    assert probe.record(0) == before and probe.clock_reads == 8
    probe.boundary(1, 1, status=0xFFFFF837)
    probe.output(1, 20)
    assert probe.record(1) == (0,) * 11 and probe.clock_reads == 8

    probe.clock = 50
    probe.boundary(1, 1)
    assert probe.read(OWNER + FPS) == 0
    assert probe.record(1)[1] == MASK
    probe.output(1, 100)
    assert probe.record(1)[1] == MASK
    probe.set_rate(25)
    probe.output(2, 45000)
    probe.clock -= 90000
    probe.boundary(1, 2)
    assert probe.record(0) == before
    assert probe.record(1) == (2, 1, 100, 45000, 1, 90000, 2, (50-100-45000) & MASK, 0, 2, 2), probe.record(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('aux', type=Path)
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == SOURCE_BYTES and sha256(original).hexdigest() == SOURCE_SHA256
    aux = read_elf(args.aux)
    verify(original, aux)
    image = args.aux.with_suffix('.bin').read_bytes()
    wire = args.aux.with_suffix('.reloc').read_bytes()
    for address in (0x8C413000, 0x8C513000, 0x8CD73000):
        verify(original, aux, (address, image, wire))
    print('Mode 62 original renderer timing: zero FPS at START, late rate at 30/25 FPS, unknown '
          'cadence until valid, actual submit, early returns, skip, clock loss/wrap, 3 relocated leases')


if __name__ == '__main__':
    main()
