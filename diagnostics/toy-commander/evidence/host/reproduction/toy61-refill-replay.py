#!/usr/bin/env python3
"""Exercise the original AVI and retained Test 57 refill policy in Test 61."""
import argparse
from hashlib import sha256
from importlib.util import module_from_spec, spec_from_file_location
from inspect import getsource
from pathlib import Path
from textwrap import dedent

ROOT = Path(__file__).resolve().parent
spec = spec_from_file_location("refill57", ROOT / "toy57-refill-replay.py")
refill = module_from_spec(spec)
spec.loader.exec_module(refill)
onset, low, original = refill.onset, refill.low, refill.original

CONFIG, INITIALIZED, CONTEXT_BYTES, WAIT = 1320, 1428, 1728, 1272
WAIT_BYTES = 24
low.CONFIG, low.INITIALIZED = CONFIG, INITIALIZED
source = dedent(getsource(low.Replay.__init__))
assert source.count("== 1584") == 1
assert source.count("super().__init__(") == 1
source = source.replace("super().__init__(", "original_chunk.ChunkReplay.__init__(self, ")
exec(source.replace("== 1584", f"== {CONTEXT_BYTES}"), low.__dict__)
low.Replay.__init__ = low.__dict__["__init__"]

original_check = onset.Replay.check_only_observer_writes

# Mode 61 latches the authenticated bumper context on its first chunk.
arm = dedent(getsource(refill.Replay.arm)).replace(
    'assert self.read(self.slot + QUIET_CTX) == 0',
    'assert self.read(self.slot + QUIET_CTX) == low.CTX',
)
exec(arm, refill.__dict__)
refill.Replay.arm = refill.__dict__['arm']
# The length selector adds a frame ahead of the original intro guard.
for name in ('refill', 'expect', 'run_original'):
    original_refill = getattr(refill.Replay, name, None) or getattr(refill, name)
    body = dedent(getsource(original_refill)).replace('low.STACK - 188', 'low.STACK - 224')
    exec(body, refill.__dict__)
    if hasattr(refill.Replay, name):
        setattr(refill.Replay, name, refill.__dict__[name])


class RetainedReplay(onset.Replay):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.write(self.slot + refill.QUIET_CTX, 0xFFFFFFFF)

    def check_only_observer_writes(self, counts, trace):
        assert self.read(self.slot + refill.QUIET_CTX) == 0xFFFFFFFF
        self.write(self.slot + refill.QUIET_CTX, 0)
        wait = self.context + WAIT + self.variant // 2 * WAIT_BYTES
        lba = self.read(low.SERVICE + 52)
        observed_first_wait = trace[0] and trace[0] == self.read(low.CTX + 0x1C0) + 1
        expected = [lba, 4, *([0xFFFFFFFF] * 4)] if observed_first_wait else [0] * 6
        assert [self.read(wait + i * 4) for i in range(6)] == expected
        self.writes = [entry for entry in self.writes if not wait <= entry[1] < wait + WAIT_BYTES]
        original_check(self, counts, trace)


def check_first_wait(image, aux):
    cases = 0
    resume = 1624
    engine = low.REGION + 0x240
    owner = aux[4]["_toy_present"][0]
    for variant in range(4):
        for state, matching, verified in ((0, True, False), (1, False, False), (1, True, True)):
            cpu = refill.Replay(image, aux, variant=variant, initial_t=variant & 1)
            cpu.ring()
            cpu.start(frame=145, outputs=144, skips=3)
            cpu.write(low.SERVICE + 80, 1)
            cpu.write(cpu.context + resume + 64, state, 1)
            cpu.write(low.SERVICE + 44, 0x13)
            cpu.write(engine + 112, 0x13 if matching else 0x14)
            cpu.write(engine + 172, cpu.timers[0xFFD8000C] + 0x1234)
            cpu.write(cpu.context + resume + 48, 0x13 if matching else 0x14)
            cpu.write(cpu.context + resume + 56, 3)
            cpu.write(owner, low.CTX)
            cpu.write(owner + 4, cpu.timers[0xFFD8000C] + 0x23456)
            cpu.write(owner + 16, 1, 1)
            assert original.signed(cpu.run_chunk()[0], 32) == -2
            wait = cpu.context + WAIT + variant // 2 * WAIT_BYTES
            values = [cpu.read(wait+i*4) for i in range(6)]
            assert values == [cpu.read(low.SERVICE + 52), 4,
                              3 if verified else 0xFFFFFFFF, 0x23456,
                              0x1234 if matching else 0xFFFFFFFF, 0xFFFFFFFF], values
            other = cpu.context + WAIT + (1 - variant // 2) * WAIT_BYTES
            assert cpu.block(other, WAIT_BYTES) == bytes(WAIT_BYTES)
            cpu.write(cpu.context + resume + 56, 4)
            cpu.write(low.CTX + 0x1C0, 146)
            cpu.original_entries.clear()
            assert original.signed(cpu.run_chunk()[0], 32) == -2
            assert [cpu.read(wait+i*4) for i in range(6)] == values
            ops, storage = 0x8C207000, 0x8C207040
            cpu.write(cpu.context + CONFIG + 12, ops)
            cpu.write(ops + 12, storage)
            cpu.write(storage, low.REGION)
            cpu.write(low.SERVICE + 80, 0)
            cpu.write(low.SERVICE + 136, 2)
            for token, delay, expected in ((0x14, 0x2800, 0xFFFFFFFF),
                    (0x13, 0x3000, 0x3000 if matching else 0xFFFFFFFF),
                    (0x13, 0x4000, 0x3000 if matching else 0xFFFFFFFF)):
                cpu.write(low.SERVICE + 44, token)
                cpu.write(engine + 112, token)
                cpu.timers[0xFFD8000C] = refill.CLOCK[0xFFD8000C] - delay
                cpu.pc = aux[4]['_kui_native_readahead_aux_dispatch'][0]
                cpu.pr, cpu.r[15], cpu.sr = refill.OUT, low.STACK, 0x400000F0
                cpu.r[4:8] = [7, 0, low.REGION, 0]
                for _ in range(2000):
                    if cpu.pc == refill.OUT:
                        break
                    cpu.step()
                assert cpu.pc == refill.OUT and cpu.r[0] == 1
                assert cpu.read(wait+20) == expected
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
    onset.Replay = RetainedReplay
    retained = onset.check_retained(image, aux) + onset.check_onset(image, aux) + onset.check_refusals(image, aux)
    orig = refill.run_original(image, aux)
    vetoes = refill.run_vetoes(image, aux)
    completed = refill.run_completed_arrival(image, aux)
    other = refill.run_other_callers(image, aux)
    wait = check_first_wait(image, aux)
    print(f"Test 61: {retained} retained movie observations, {orig} original AVI refill routes, "
          f"{vetoes} guard transitions, {completed} completed-arrival, {other} unrelated-call, "
          f"and {wait} first-wait cases passed")


if __name__ == "__main__":
    main()
