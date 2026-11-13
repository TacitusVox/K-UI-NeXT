#!/usr/bin/env python3
"""Exercise linked Test 57 with the original AVI refill and acquisition paths."""
import argparse
from hashlib import sha256
from importlib.util import module_from_spec, spec_from_file_location
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parent
spec = spec_from_file_location("onset55", ROOT / "toy55-onset-replay.py")
onset = module_from_spec(spec)
spec.loader.exec_module(onset)
low, original = onset.low, onset.original

REFILL, ENQUEUE, RETURN, LITERAL = 0x8C098A14, 0x8C097186, 0x8C0989D8, 0x8C098A88
OUT = 0x8C008A00
OWNER, FILE = 0x8C202000, 0x8C203000
CLOCK = {0xFFC00000: 0xE0A, 0xFFD80008: 0xFFFFFFFF, 0xFFD80010: 2,
         0xFFD80004: 1, 0xFFD8000C: 0xF0000000}
QUIET_CTX, QUIET_TICK, QUIET_DEFERS, QUIET_REASON, QUIET_END = (32, 36, 40, 44, 48)
QUIET_FIRST = 1004
ACTIVE = low.REGION + 0x240 + 124
SCI_STATE = low.SERVICE + 324 + 12


class Replay(onset.Replay):
    def __init__(self, image, aux, *, variant=3, initial_t=0):
        super().__init__(image, aux, "ready", True, initial_t, variant, "none", True)
        self.timers = dict(CLOCK)
        self.avi = original.AVI
        self.enqueue_calls = []
        self.parse_visits = 0
        self.lowest_stack = low.STACK
        self.submitted = (FILE, original.RING + 0xA0000, 0x8000)
        self.refill_hook = aux[4]["_kui_toy_movie_refill_hook"][0]
        self.write(LITERAL, self.refill_hook)
        self.write(low.CTX, self.avi)
        self.write(self.avi, OWNER)
        self.write(OWNER + 108, FILE)
        self.write(self.avi + 0x10, 1)
        self.write(self.avi + 0x14, 0x100000)
        self.write(self.avi + 0x18, 0x200000)
        self.write(self.avi + 0x1C, original.RING)
        self.write(self.avi + 0x44, 3)
        self.write(self.avi + 0x48, 1)
        self.write(self.avi + 0x4C, 0)
        self.write(self.avi + 0x54, 0x8000)
        self.write(self.avi + 0x58, 0x8000)
        self.write(self.avi + 0x60, 0xA0000)
        self.write(self.avi + 0x64, 0)
        self.write(self.avi + 0x68, 0xA0000)
        self.start(frame=0, outputs=0, skips=0)
        # The acquisition advances to the ready index 1; keep index 2
        # marked ready as well so the observer may admit the next chunk.
        self.write(original.ENTRIES + 24 + 9, 1, 1)

    def read(self, address, count=4):
        timers = getattr(self, "timers", {})
        if address in timers:
            return timers[address]
        return super().read(address, count)

    def plain(self, op, pc):
        if op & 0xF0FF == 0x4019:
            self.r[(op >> 8) & 15] >>= 8
            self.executing = pc
            self.instructions += 1
        elif op & 0xF0FF == 0x4018:
            n = (op >> 8) & 15
            self.r[n] = (self.r[n] << 8) & 0xFFFFFFFF
            self.executing = pc
            self.instructions += 1
        else:
            super().plain(op, pc)

    def arm(self):
        self.write(original.ENTRIES + 12 + 4, 0)
        self.write(original.ENTRIES + 12 + 9, 1, 1)
        assert self.run_chunk()[0] == 1
        if self.variant >= 2:
            assert self.read(self.slot + QUIET_CTX) == low.CTX
            assert self.read(self.slot + QUIET_TICK) == CLOCK[0xFFD8000C]
        else:
            assert self.read(self.slot + QUIET_CTX) == 0

    def refill(self, *, sr=0x40000000):
        self.pc, self.pr, self.r[15], self.sr = REFILL, OUT, low.STACK, sr
        self.r[4:8] = [self.avi, 0x8000, 0xFFFFFFFE, 0]
        saved = self.r[8:15]
        self.enqueue_calls.clear()
        self.parse_visits = 0
        self.lowest_stack = low.STACK
        entry_sr = None
        for _ in range(3000):
            self.lowest_stack = min(self.lowest_stack, self.r[15])
            if self.pc == OUT:
                assert self.r[8:15] == saved and self.r[15] == low.STACK and self.pr == OUT
                # Original DIV1 changes the Q and M arithmetic flags.
                assert self.sr & ~0x301 == sr & ~0x301
                assert self.parse_visits == 1
                assert self.lowest_stack >= low.STACK - 188
                return self.r[0]
            if self.pc == 0x8C097C58:
                self.parse_visits += 1
            if self.pc == self.refill_hook:
                entry_sr = self.sr
            if self.pc == ENQUEUE:
                assert self.pr == RETURN
                assert tuple(self.r[4:7]) == self.submitted
                assert self.sr == entry_sr
                self.enqueue_calls.append(tuple(self.r[4:8]))
                # Only the original device submission is modeled here.
                self.r[0], self.pc = 0x8000, self.pr
                continue
            if self.pc == RETURN:
                assert self.sr == entry_sr
            if self.pc >> 12 == 0x8C098 and self.read(self.pc, 2) >> 12 == 0xB:
                pc = self.pc
                op = self.read(pc, 2)
                self.pr = pc + 4
                self.plain(self.read(pc + 2, 2), pc + 2)
                self.pc = pc + 4 + 2 * original.signed(op & 0xFFF, 12)
            else:
                self.step()
        raise AssertionError(f"AVI refill exceeded bound at {self.pc:#x}")

    def expect(self, *, suppress, reason, frame, sr=0x40000000):
        self.write(low.CTX + 0x1C0, frame)
        self.writes.clear()
        assert self.refill(sr=sr) == (0 if suppress else 0x8000)
        assert len(self.enqueue_calls) == (0 if suppress else 1)
        assert self.read(self.avi + 0x180) == (0 if suppress else 0x8000)
        assert self.read(self.slot + QUIET_REASON) == reason
        if suppress:
            allowed = {self.slot + off for off in (QUIET_DEFERS, QUIET_REASON, QUIET_END)}
            allowed.add(self.context + QUIET_FIRST)
            for pc, address, _, size in self.writes:
                if not 0x8C422000 <= pc < 0x8C425000:
                    continue
                assert size == 4
                assert address in allowed or low.STACK - 188 <= address < low.STACK
        self.write(self.avi + 0x180, 0)


def run_original(image, aux):
    tests = 0
    for variant, sr in product((0, 1, 2, 3), (0x40000000, 0x40000001, 0x400000F0, 0x400000F1)):
        cpu = Replay(image, aux, variant=variant, initial_t=sr & 1)
        cpu.expect(suppress=False, reason=0, frame=0, sr=sr)
        cpu.arm()
        cpu.expect(suppress=variant >= 2, reason=0, frame=10, sr=sr)
        assert cpu.read(cpu.slot + QUIET_DEFERS) == int(variant >= 2)
        assert cpu.read(cpu.context + QUIET_FIRST) == (11 if variant >= 2 else 0)
        if variant >= 2:
            assert cpu.lowest_stack == low.STACK - 188
            cpu.expect(suppress=True, reason=0, frame=18, sr=sr)
            assert cpu.read(cpu.slot + QUIET_DEFERS) == 2
            assert cpu.read(cpu.context + QUIET_FIRST) == 11
            assert cpu.read(cpu.slot + QUIET_END) == 19
            cpu.expect(suppress=False, reason=1, frame=30, sr=sr)
            cpu.expect(suppress=False, reason=1, frame=10, sr=sr)
            assert cpu.read(cpu.slot + QUIET_CTX) == 0xFFFFFFFF
            assert cpu.read(cpu.slot + QUIET_END) == 31
        tests += 1
    return tests


def run_vetoes(image, aux):
    tests = 0
    for address, bad, reason, permanent in (
        (low.SERVICE + 80, 1, 3, False), (low.SERVICE + 84, 1, 3, False),
        (ACTIVE, 1, 3, False), (SCI_STATE, 2, 3, False), (0x8C0B0228, 7, 3, False),
        (original.AVI + 0x68, 0x30000, 2, False),
        (original.ENTRIES + 24 + 8, 7, 2, False),
        (original.ENTRIES + 24 + 8, 0x307, 2, False),
        (low.CTX + 0x244, 44100, 1, True),
        (low.CTX, 0x8C005000, 4, True),
        (0xFFD8000C, CLOCK[0xFFD8000C] - 976562, 1, True),
        (0xFFD80010, 3, 4, True),
    ):
        for armed in (False, True):
            cpu = Replay(image, aux)
            cpu.arm()
            if armed:
                cpu.expect(suppress=True, reason=0, frame=5)
            if address in cpu.timers:
                before = cpu.timers[address]
                cpu.timers[address] = bad
            else:
                before = cpu.read(address)
                cpu.write(address, bad)
            try:
                cpu.expect(suppress=False, reason=reason, frame=10)
            except AssertionError as error:
                raise AssertionError((hex(address), bad, reason, armed)) from error
            if address in cpu.timers:
                cpu.timers[address] = before
            else:
                cpu.write(address, before)
            ended = armed or permanent
            cpu.expect(suppress=not ended, reason=reason if ended else 0, frame=11)
            assert cpu.read(cpu.slot + QUIET_CTX) == (0xFFFFFFFF if ended else low.CTX)
            tests += 1
    return tests


def run_completed_arrival(image, aux):
    cases = 0
    for suppress in (False, True):
        cpu = Replay(image, aux)
        if suppress:
            cpu.arm()
        # The original routine retires a completed earlier read before it
        # reaches the patched enqueue. Its completed byte count has a real
        # effect on the original buffer frontier even when the next enqueue
        # is postponed.
        cpu.write(cpu.avi + 0x180, 0x800)
        cpu.write(cpu.avi + 0x44, 4)
        cpu.write(original.ENTRIES, 1)
        cpu.write(original.ENTRIES + 36, 0xA1000)
        cpu.write(original.ENTRIES + 40, 0x800)
        cpu.write(original.ENTRIES + 44, 7)
        cpu.submitted = (FILE, original.RING + 0xA0800, 0x8000)
        cpu.expect(suppress=suppress, reason=0, frame=10)
        assert cpu.read(cpu.avi + 0x68) == 0xA0800
        assert cpu.read(cpu.avi + 0x60) == 0xA0800
        cases += 1
    return cases


def run_other_callers(image, aux):
    cases = 0
    for t in (0, 1):
        cpu = Replay(image, aux)
        cpu.arm()
        cpu.pc, cpu.pr, cpu.r[15], cpu.sr = cpu.refill_hook, OUT, low.STACK, 0x40000000 | t
        cpu.r[4:8] = [FILE, original.RING + 0xA0000, 0x8000, 0x183]
        saved = cpu.r[4:16].copy()
        for _ in range(15):
            if cpu.pc == ENQUEUE:
                break
            cpu.step()
        assert cpu.pc == ENQUEUE and cpu.pr == OUT and cpu.r[4:16] == saved
        assert cpu.sr == 0x40000000 | t and cpu.read(cpu.slot + QUIET_DEFERS) == 0
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

    class RetainedReplay(onset.Replay):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.write(self.slot + QUIET_CTX, 0xFFFFFFFF)

        def check_only_observer_writes(self, counts, trace):
            assert self.read(self.slot + QUIET_CTX) == 0xFFFFFFFF
            self.write(self.slot + QUIET_CTX, 0)
            super().check_only_observer_writes(counts, trace)

    onset.Replay = RetainedReplay
    retained = onset.check_retained(image, aux) + onset.check_onset(image, aux) + onset.check_refusals(image, aux)
    orig = run_original(image, aux)
    vetoes = run_vetoes(image, aux)
    completed = run_completed_arrival(image, aux)
    other = run_other_callers(image, aux)
    print(f"Test 57: {retained} retained movie observations, {orig} original AVI refill routes, "
          f"{vetoes} guard transitions, {completed} completed-arrival and {other} unrelated-call cases passed")


if __name__ == "__main__":
    main()
