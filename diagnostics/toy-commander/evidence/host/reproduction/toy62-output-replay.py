#!/usr/bin/env python3
"""Replay authentic Toy player output/skip/terminal paths with timing enabled."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
fixture = ROOT / "toy49-output-proto/prove-output.py"
source = fixture.read_text()
clock = """    def read(self, address, count=4):
        timer = {0xffc00000:(2,0xe0a), 0xffd80008:(4,0xffffffff),
                 0xffd80010:(2,2), 0xffd80004:(1,1), 0xffd8000c:(4,0xffff0000)}
        if address in timer:
            width, value = timer[address]
            assert count == width
            return value
        return super().read(address, count)

    def plain(self, op, pc):
        if op & 0xf0ff == 0x402a:  # lds Rn,PR
            self.pr = self.r[(op >> 8) & 15]
        elif op >> 8 == 0x84:  # mov.b @(disp,Rm),R0
            address = self.r[(op >> 4) & 15] + (op & 15)
            self.r[0] = replay.signed(self.read(address, 1), 8) & replay.MASK
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

"""
assert source.count("    def stub(self, pc):") == 1
source = source.replace("    def stub(self, pc):", clock + "    def stub(self, pc):")
assert source.count('Mode 49 original lifecycle') == 1
source = source.replace('Mode 49 original lifecycle', 'Mode 62 original lifecycle')
exec(compile(source, str(fixture), "exec"), {"__file__": str(fixture)})
