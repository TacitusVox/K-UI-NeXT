#!/usr/bin/env python3
"""Exercise the exact original Toy start, heap query and media allocation leaves."""
from hashlib import sha256
import importlib.util
from pathlib import Path
import sys

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("replay39", root / "toy39-original-replay.py")
replay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(replay)
MASK = replay.MASK
HEAP_QUERY = 0x8C0B3E6E
HEAP_TABLE_SLOT, HEAP_TABLE = 0x8C117DBC, 0x8C0C5AC8
HEAP_CURSOR, SENTINEL = 0x8C117DE0, 0x8C117DC0
FREE_A, FREE_B = 0x8C210000, 0x8C600000
BUFFER, PREFILL, LARGER = 0x1DC, 0x1E0, 16
MIB = 1024 * 1024


class OpeningReplay(replay.MovieReplay):
    def __init__(self, original, aux, clip, pal, largest=3 * MIB + 32,
                 default_owner=True, initial_bytes=MIB, prefill=MASK,
                 name_override=None, caller_sr=0x40000000, launch_scenario="eof"):
        super().__init__(original, aux, clip, pal, launch_scenario)
        self.largest = largest
        self.initial_bytes = initial_bytes
        self.prefill = prefill
        self.name_override = name_override
        self.buffer_seen = self.query_seen = 0
        self.query_start = None
        self.sr = caller_sr
        self.write(HEAP_TABLE_SLOT, HEAP_TABLE if default_owner else 0x8C100000)
        self.write(HEAP_CURSOR, SENTINEL)
        self.write(SENTINEL, FREE_A)
        self.write(FREE_A, FREE_B)
        self.write(FREE_A + 4, 1 + largest // 32)
        self.write(FREE_B, SENTINEL)
        self.write(FREE_B + 4, 1 + (MIB // 32))

    def stub(self, pc):
        if pc == 0x8C06D3EC:
            # Execute the actual wrapper and mutably model only negative
            # states a later SDK or title could place in this context.
            assert self.r[4] == self.r[15] + 16 == self.ctx
            assert self.read(self.ctx + BUFFER) == MIB
            assert self.read(self.ctx + PREFILL) == MASK
            if self.initial_bytes != MIB:
                self.write(self.ctx + BUFFER, self.initial_bytes)
            if self.prefill != MASK:
                self.write(self.ctx + PREFILL, self.prefill)
            if self.name_override is not None:
                self.store_block(self.ctx + 0xBC, self.name_override)
        if pc == HEAP_QUERY:
            self.query_seen += 1
            assert self.r[4] == self.r[15]
            assert self.r[5] == self.r[15] + 4
            self.query_start = (self.pr, self.sr, self.r[4], self.r[5], len(self.writes))
        if self.query_start and pc == self.query_start[0]:
            _, sr, total, largest, first_write = self.query_start
            assert self.sr & ~1 == sr & ~1
            assert self.read(total) == self.largest + MIB
            assert self.read(largest) == max(MIB, self.largest)
            assert all(total - 12 <= at <= largest for _, at, _, _ in self.writes[first_write:])
            self.query_start = None
        if pc == 0x8C0977DC:
            assert self.r[4] == self.ctx
            assert self.cstring(self.ctx + 0xBC) == (self.name_override or self.name)
            self.buffer_seen = self.read(self.ctx + BUFFER)
            assert self.read(self.ctx + PREFILL) == self.prefill
            if self.scenario == "input":
                self.write(replay.SKIP, 1)
            self.r[0] = 0xFFFFF837 if self.scenario == "open" else 0
            self.pc = self.pr
            return True
        return super().stub(pc)


class LeafReplay(OpeningReplay):
    """Exercise original media setup/free and the original startup-size arithmetic."""
    MEDIA_ALLOC, MEDIA_CLOSE = 0x8C0973D8, 0x8C097452
    ALLOC, FREE = 0x8C04C16C, 0x8C04C1C6
    HEAP_ALLOC, HEAP_FREE = 0x8C0B3EF2, 0x8C0B3FC0
    CTX = 0x8C2F0000
    MEDIA_STATE, FILE, STREAM = 0x8C340000, 0x8C341000, 0x8C342000

    def __init__(self, original, capacity, success, actual_heap_bytes=None, aux=None):
        super().__init__(original, aux, 1, False)
        self.ctx, self.capacity, self.success = self.CTX, capacity, success
        self.allocated = self.descriptor = self.freed = 0
        self.alloc_calls = []
        self.actual_heap_bytes = actual_heap_bytes
        self.write(self.ctx + BUFFER, capacity)
        self.write(self.ctx + PREFILL, MASK)
        self.write(self.ctx + 64, 0x8C06D25C)
        self.write(self.ctx + 60, 0x8C06D25C)
        self.write(0x8C0B023C, 0)
        self.store_block(self.ctx + 0xBC, self.name)
        if actual_heap_bytes is not None:
            # Route media allocation to the genuine default heap, modeling
            # a custom heap that has no blocks and one SDK free range.
            self.write(0x8C0DF1C0, 0)
            self.write(SENTINEL, FREE_A)
            self.write(FREE_A, SENTINEL)
            self.write(FREE_A + 4, 1 + actual_heap_bytes // 32)

    def plain(self, op, pc):
        if op & 0xF0FF == 0x4021:  # shar Rn
            n = (op >> 8) & 15
            self.flag(self.r[n] & 1)
            self.r[n] = replay.signed(self.r[n], 32) >> 1 & MASK
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def leaf_stub(self, pc):
        if pc == self.ALLOC:
            size = self.r[4]
            self.alloc_calls.append(size)
            assert size in (0x1B0, self.capacity)
            if self.actual_heap_bytes is not None:
                return False  # original wrapper and SDK heap allocator
            if size == 0x1B0:
                result = self.MEDIA_STATE
            else:
                assert self.r[5] == 2
                result = self.allocated = 0x8C700000 if self.success else 0
        elif pc in (0x8C099386, 0x8C099394) and self.actual_heap_bytes is not None:
            if pc == 0x8C099386:
                self.descriptor = self.r[0]
            else:
                self.allocated = self.r[0]
            return False
        elif pc == self.FREE:
            if self.r[4] == self.allocated and self.allocated:
                self.freed += 1
            elif self.actual_heap_bytes is not None:
                assert self.r[4] == self.descriptor
            else:
                assert self.r[4] == self.MEDIA_STATE
            if self.actual_heap_bytes is not None:
                return False  # original wrapper and SDK heap free/coalescing
            result = 0
        elif pc == 0x8C04EEE0:  # original heap bookkeeping leaf
            result = 0
        elif pc == 0x8C099098:  # media open leaf
            assert self.cstring(self.r[4]) == self.name
            result = self.FILE
        elif pc == 0x8C09913C:  # media stream construction leaf
            assert self.r[4] == self.FILE
            result = self.STREAM
        elif pc in (0x8C099298, 0x8C099044, 0x8C06D25C):
            result = 0
        else:
            return False
        self.r[0], self.pc = result, self.pr
        return True

    def run_leaf(self, entry, args, stop=None, caller=replay.RETURN):
        self.pc, self.pr, self.r[15] = entry, caller, replay.STACK
        self.r[4:4 + len(args)] = args
        saved = self.r[8:15]
        for _ in range(1000):
            pc = self.pc
            if pc == stop:
                return self.r[4:8]
            if pc == replay.RETURN:
                assert self.r[8:15] == saved and self.r[15] == replay.STACK
                return self.r[0]
            if self.leaf_stub(pc):
                continue
            op = self.read(pc, 2)
            target, delay, n = pc + 2, False, (op >> 8) & 15
            if op >> 12 in (0xA, 0xB):
                target, delay = pc + 4 + 2 * replay.signed(op & 0xFFF, 12), True
                if op >> 12 == 0xB:
                    self.pr = pc + 4
            elif op & 0xF0FF in (0x400B, 0x402B):
                target, delay = self.r[n], True
                if op & 0xF0FF == 0x400B:
                    self.pr = pc + 4
            elif op == 0x000B:
                target, delay = self.pr, True
            elif op >> 8 in (0x89, 0x8B, 0x8D, 0x8F):
                delay = op >> 8 in (0x8D, 0x8F)
                if bool(self.sr & 1) == (op >> 8 in (0x89, 0x8D)):
                    target = pc + 4 + 2 * replay.signed(op & 255, 8)
                else:
                    target = pc + (4 if delay else 2)
            else:
                self.plain(op, pc)
            if delay:
                slot = self.read(pc + 2, 2)
                replay.validate_delay_slot(slot, pc)
                self.plain(slot, pc + 2)
            self.pc = target & MASK
        raise AssertionError("Original media leaf exceeded instruction bound")


def verify_movie(original, aux):
    cases = 0
    for pal in (False, True):
        for sr in (0x40000000, 0x40000071, 0x400000F0):
            for clip, input_args, expected_bytes, queries, enlarged in (
                (0, {}, MIB, 0, False),
                (1, {}, 2 * MIB, 1, True),
                (1, {"largest": 3 * MIB}, MIB, 1, False),
                (1, {"largest": MIB}, MIB, 1, False),
                (1, {"default_owner": False}, MIB, 0, False),
                (1, {"initial_bytes": 2 * MIB}, 2 * MIB, 0, False),
                (1, {"prefill": 0x0E0000}, MIB, 0, False),
                (1, {"name_override": b"50hz_Intro_LRTF.avx\0"}, MIB, 0, False),
                (1, {"name_override": b"60hz_Intro_LRTF.aviX\0"}, MIB, 0, False),
                (1, {"launch_scenario": "input"}, 2 * MIB, 1, True),
                (1, {"launch_scenario": "open"}, 2 * MIB, 1, True),
            ):
                bare = OpeningReplay(original, None, clip, pal, caller_sr=sr, **input_args)
                hook = OpeningReplay(original, aux, clip, pal, caller_sr=sr, **input_args)
                assert bare.run_movie() == hook.run_movie()
                assert bare.boundaries == hook.boundaries
                assert bare.dispatches == hook.dispatches, (clip, input_args, bare.dispatches, hook.dispatches)
                assert hook.query_seen == queries and bare.query_seen == 0
                assert hook.buffer_seen == expected_bytes
                assert hook.read(hook.ctx + PREFILL) == hook.prefill
                assert hook.record()[0] & LARGER == (LARGER if enlarged else 0)
                assert hook.block(hook.witness + (1 - clip) * replay.RECORD_BYTES,
                                  replay.RECORD_BYTES) == bytes(replay.RECORD_BYTES)
                guest = bytearray(bare.block(bare.ctx, 0x250))
                guest[BUFFER:BUFFER + 4] = expected_bytes.to_bytes(4, "little")
                assert hook.block(hook.ctx, 0x250) == guest
                cases += 1
    return cases


def verify_media(original):
    cases = 0
    for capacity in (MIB, 2 * MIB):
        for success in (False, True):
            cpu = LeafReplay(original, capacity, success)
            result = cpu.run_leaf(cpu.MEDIA_ALLOC, [cpu.ctx])
            assert result == (0 if success else MASK)
            assert cpu.alloc_calls == [0x1B0, capacity]
            if success:
                assert cpu.allocated
                cpu.write(cpu.ctx + 0x0C, 0)
                cpu.write(cpu.ctx + 0x10, 0)
                cpu.run_leaf(cpu.MEDIA_CLOSE, [cpu.ctx])
                assert cpu.freed == 1 and cpu.read(cpu.ctx) == 0
                cpu.run_leaf(cpu.MEDIA_CLOSE, [cpu.ctx])
                assert cpu.freed == 1
                # Exact source initializes r14 to context in caller setup.
                cpu.r[14] = cpu.ctx
                cpu.r[13] = 0
                params = cpu.run_leaf(0x8C09759A, [], stop=0x8C097DEE)
                assert params[3] == capacity * 3 // 4
            else:
                assert cpu.allocated == cpu.freed == cpu.read(cpu.ctx) == 0
            cases += 1
        for available in (capacity - 32, capacity + MIB + 32):
            success = available >= capacity
            cpu = LeafReplay(original, capacity, success, actual_heap_bytes=available)
            result = cpu.run_leaf(cpu.MEDIA_ALLOC, [cpu.ctx])
            assert result == (0 if success else MASK)
            assert cpu.alloc_calls == [0x1B0, capacity]
            if success:
                assert cpu.read(cpu.allocated - 28) * 32 == capacity + 32
                cpu.run_leaf(cpu.MEDIA_CLOSE, [cpu.ctx])
                assert cpu.freed == 1
            else:
                assert cpu.allocated == cpu.freed == 0
            assert cpu.read(cpu.ctx) == 0
            assert cpu.read(SENTINEL) == FREE_A and cpu.read(FREE_A) == SENTINEL
            assert cpu.read(FREE_A + 4) * 32 == available + 32
            cases += 1
    return cases


def verify_call_refusals(original, aux):
    for caller, command, frame in ((replay.RETURN, 0x200, replay.STACK + 16),
                                    (0x8C06D3F0, 0x200, 0x8C2F0000),
                                    (0x8C06D3F0, 0x201, replay.STACK + 16),
                                    (0x8C06D39A, 0, replay.STACK + 16)):
        cpu = LeafReplay(original, MIB, True, aux=aux)
        cpu.store_block(frame + 0xBC, cpu.name)
        cpu.write(frame + BUFFER, MIB)
        cpu.write(frame + PREFILL, MASK)
        first_write = len(cpu.writes)
        sr = cpu.sr | 1
        cpu.sr = sr
        registers = cpu.run_leaf(cpu.hook, [frame, command], stop=replay.DISPATCH, caller=caller)
        assert registers[:2] == [frame, command] and cpu.sr == sr
        assert cpu.read(frame + BUFFER) == MIB and cpu.read(frame + PREFILL) == MASK
        assert all(at < replay.STACK for _, at, _, _ in cpu.writes[first_write:])
    return 4


if __name__ == "__main__":
    original = Path(sys.argv[1]).read_bytes()
    assert len(original) == replay.SOURCE_BYTES
    assert sha256(original).hexdigest() == replay.SOURCE_SHA256
    aux = replay.read_elf(Path(sys.argv[2]))
    movie = verify_movie(original, aux)
    media = verify_media(original)
    refusals = verify_call_refusals(original, aux)
    print(f"Test 54 exact original movie/query {movie} cases, original media allocation/prefill/free {media} cases, and dispatch refusal {refusals} cases passed")
