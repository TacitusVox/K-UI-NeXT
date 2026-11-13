#!/usr/bin/env python3
"""Exercise the original AVI and retained Test 57 refill policy in Test 60."""
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

CONFIG, INITIALIZED, CONTEXT_BYTES, WAIT = 1288, 1396, 1688, 1256
low.CONFIG, low.INITIALIZED = CONFIG, INITIALIZED
source = dedent(getsource(low.Replay.__init__))
assert source.count("== 1584") == 1
assert source.count("super().__init__(") == 1
source = source.replace("super().__init__(", "original_chunk.ChunkReplay.__init__(self, ")
exec(source.replace("== 1584", f"== {CONTEXT_BYTES}"), low.__dict__)
low.Replay.__init__ = low.__dict__["__init__"]

original_check = onset.Replay.check_only_observer_writes


class RetainedReplay(onset.Replay):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.write(self.slot + refill.QUIET_CTX, 0xFFFFFFFF)

    def check_only_observer_writes(self, counts, trace):
        assert self.read(self.slot + refill.QUIET_CTX) == 0xFFFFFFFF
        self.write(self.slot + refill.QUIET_CTX, 0)
        wait = self.context + WAIT + self.variant // 2 * 16
        lba = self.read(low.SERVICE + 52)
        observed_first_wait = trace[0] and trace[0] == self.read(low.CTX + 0x1C0) + 1
        expected = [lba, 4, 0xFFFFFFFF, 0xFFFFFFFF] if observed_first_wait else [0] * 4
        assert [self.read(wait + i * 4) for i in range(4)] == expected
        self.writes = [entry for entry in self.writes if not wait <= entry[1] < wait + 16]
        original_check(self, counts, trace)


def check_first_wait(image, aux):
    cases = 0
    resume = 1592
    owner = aux[4]["_toy_present"][0]
    for variant in range(4):
        for state, matching, verified in ((0, True, False), (1, False, False), (1, True, True)):
            cpu = refill.Replay(image, aux, variant=variant, initial_t=variant & 1)
            cpu.ring()
            cpu.start(frame=145, outputs=144, skips=3)
            cpu.write(low.SERVICE + 80, 1)
            cpu.write(cpu.context + resume + 64, state, 1)
            cpu.write(low.SERVICE + 44, 0x13)
            cpu.write(cpu.context + resume + 48, 0x13 if matching else 0x14)
            cpu.write(cpu.context + resume + 56, 3)
            cpu.write(owner, low.CTX)
            cpu.write(owner + 4, cpu.timers[0xFFD8000C] + 0x23456)
            cpu.write(owner + 16, 1, 1)
            assert original.signed(cpu.run_chunk()[0], 32) == -2
            wait = cpu.context + WAIT + variant // 2 * 16
            values = [cpu.read(wait+i*4) for i in range(4)]
            assert values == [cpu.read(low.SERVICE + 52), 4,
                              3 if verified else 0xFFFFFFFF, 0x23456], values
            assert cpu.block(cpu.context + WAIT + (1-variant//2)*16, 16) == bytes(16)
            cpu.write(cpu.context + resume + 56, 4)
            cpu.write(low.CTX + 0x1C0, 146)
            cpu.original_entries.clear()
            assert original.signed(cpu.run_chunk()[0], 32) == -2
            assert [cpu.read(wait+i*4) for i in range(4)] == values
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
    print(f"Test 60: {retained} retained movie observations, {orig} original AVI refill routes, "
          f"{vetoes} guard transitions, {completed} completed-arrival, {other} unrelated-call, "
          f"and {wait} first-wait cases passed")


if __name__ == "__main__":
    main()
