#!/usr/bin/env python3
"""Replay the linked intro prefill selection and original media startup."""

from hashlib import sha256
import importlib.util
from pathlib import Path
import sys

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("opening54", root / "toy54-opening-replay.py")
previous = importlib.util.module_from_spec(spec)
spec.loader.exec_module(previous)

replay = previous.replay
MIB = previous.MIB
BUFFER = previous.BUFFER
PREFILL = previous.PREFILL
STOCK = replay.MASK
MORE = 0xF0000
FLAG = 0x20


class PrefillReplay(previous.OpeningReplay):
    def __init__(self, original, aux, clip, pal, admitted, **options):
        super().__init__(original, aux, clip, pal, **options)
        self.expected_prefill = MORE if admitted else self.prefill

    def stub(self, pc):
        if pc == 0x8C0977DC:
            assert self.r[4] == self.ctx
            self.buffer_seen = self.read(self.ctx + BUFFER)
            assert self.read(self.ctx + PREFILL) == self.expected_prefill
            if self.scenario == "input":
                self.write(replay.SKIP, 1)
            self.r[0] = 0xFFFFF837 if self.scenario == "open" else 0
            self.pc = self.pr
            return True
        return super().stub(pc)


class PrefillLeaves(previous.LeafReplay):
    def plain(self, op, pc):
        if op & 0xF0FF in (0x4018, 0x4019):  # shll8/shlr8 Rn
            n = (op >> 8) & 15
            self.r[n] = self.r[n] >> 8 if op & 1 else self.r[n] << 8 & replay.MASK
            self.executing = pc
            self.instructions += 1
        elif op & 0xF00F == 0x400C:  # shad Rm,Rn
            n, m = (op >> 8) & 15, (op >> 4) & 15
            shift = self.r[m]
            if shift & 0x80000000:
                self.r[n] = replay.signed(self.r[n], 32) >> (32 - (shift & 31))
            else:
                self.r[n] <<= shift & 31
            self.r[n] &= replay.MASK
            self.executing = pc
            self.instructions += 1
        else:
            super().plain(op, pc)


def verify_movie(original, aux):
    checked = 0
    for pal in (False, True):
        for sr in (0x40000000, 0x40000071, 0x400000F0):
            for clip, options, admitted in (
                (0, {}, False),
                (1, {}, True),
                (1, {"largest": MIB, "default_owner": False}, True),
                (1, {"initial_bytes": 2 * MIB}, False),
                (1, {"prefill": 0xE0000}, False),
                (1, {"name_override": b"50hz_Intro_LRTF.avx\0"}, False),
                (1, {"name_override": b"60hz_Intro_LRTF.aviX\0"}, False),
                (1, {"launch_scenario": "input"}, True),
                (1, {"launch_scenario": "open"}, True),
            ):
                stock = PrefillReplay(original, None, clip, pal, False, caller_sr=sr, **options)
                tested = PrefillReplay(original, aux, clip, pal, admitted, caller_sr=sr, **options)
                assert stock.run_movie() == tested.run_movie()
                assert stock.boundaries == tested.boundaries
                assert stock.dispatches == tested.dispatches
                assert stock.buffer_seen == tested.buffer_seen == tested.initial_bytes
                assert not stock.query_seen and not tested.query_seen
                assert tested.record()[0] & FLAG == (FLAG if admitted else 0)
                assert tested.record()[0] & previous.LARGER == 0
                other_witness = tested.witness + (1 - clip) * replay.RECORD_BYTES
                assert tested.block(other_witness, replay.RECORD_BYTES) == bytes(replay.RECORD_BYTES)
                guest = bytearray(stock.block(stock.ctx, 0x250))
                if admitted:
                    guest[PREFILL:PREFILL + 4] = MORE.to_bytes(4, "little")
                assert tested.block(tested.ctx, 0x250) == guest
                checked += 1
    return checked


def verify_media(original):
    checked = 0
    for prefill in (STOCK, MORE):
        for heap in (MIB - 32, 2 * MIB + 32):
            success = heap >= MIB
            cpu = PrefillLeaves(original, MIB, success, actual_heap_bytes=heap)
            cpu.write(cpu.ctx + PREFILL, prefill)
            assert cpu.run_leaf(cpu.MEDIA_ALLOC, [cpu.ctx]) == (0 if success else replay.MASK)
            assert cpu.alloc_calls == [0x1B0, MIB]
            if success:
                assert cpu.allocated and cpu.read(cpu.allocated - 28) * 32 == MIB + 32
                cpu.r[14] = cpu.ctx
                cpu.r[13] = 0
                params = cpu.run_leaf(0x8C09759A, [], stop=0x8C097DEE)
                expected = MIB * 3 // 4 if prefill == STOCK else MORE
                assert params[3] == expected
                cpu.r[9] = params[3]
                cpu.write(cpu.ctx + 20, MIB)
                requested = cpu.run_leaf(0x8C0982B2, [], stop=0x8C098388)
                assert requested[:2] == [cpu.ctx, expected]
                cpu.write(cpu.ctx + 20, 0)
                cpu.run_leaf(cpu.MEDIA_CLOSE, [cpu.ctx])
                cpu.run_leaf(cpu.MEDIA_CLOSE, [cpu.ctx])
                assert cpu.freed == 1
            else:
                assert cpu.allocated == cpu.freed == 0
            assert cpu.read(cpu.ctx) == 0
            assert cpu.read(previous.SENTINEL) == previous.FREE_A
            assert cpu.read(previous.FREE_A) == previous.SENTINEL
            assert cpu.read(previous.FREE_A + 4) * 32 == heap + 32
            checked += 1
    return checked


def verify_clamp(original):
    checked = 0
    for rounded_index, file_bytes in (
        (16 * 1024, 16 * MIB),
        (96 * 1024, 16 * MIB),
        (96 * 1024, 880 * 1024 + 31),
    ):
        cpu = PrefillLeaves(original, MIB, True)
        cpu.r[14] = cpu.ctx
        cpu.r[4] = MIB
        cpu.r[0] = 80
        cpu.r[3] = 2047
        cpu.write(cpu.ctx + 24, ~31 & replay.MASK)
        cpu.write(cpu.ctx + 80, rounded_index - 1)
        cpu.run_leaf(0x8C098006, [], stop=0x8C098024)
        ring = MIB - rounded_index - 2048
        assert cpu.read(cpu.ctx + 20) == ring
        cpu.write(cpu.ctx + 24, file_bytes)
        cpu.write(cpu.ctx + 96, 0)
        cpu.write(cpu.ctx + 100, 0)
        cpu.write(cpu.ctx + 104, 0)
        for target in (MIB * 3 // 4, MORE):
            cpu.r[12] = target
            cpu.run_leaf(0x8C098950, [], stop=0x8C098994)
            assert cpu.r[13] == min(target, min(ring, file_bytes) & ~2047)
            checked += 1
    return checked


original = Path(sys.argv[1]).read_bytes()
assert len(original) == replay.SOURCE_BYTES and sha256(original).hexdigest() == replay.SOURCE_SHA256
aux = replay.read_elf(Path(sys.argv[2]))
movie = verify_movie(original, aux)
media = verify_media(original)
clamps = verify_clamp(original)
refusals = previous.verify_call_refusals(original, aux)
print(f"Test 56 linked intro selection {movie}, original media allocation/prefill/free {media}, "
      f"ring/file clamps {clamps}, and dispatcher refusal {refusals} cases passed")
