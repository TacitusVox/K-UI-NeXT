#!/usr/bin/env python3
"""Replay the exact original Toy selectors, player and cutoff with a linked witness.

The AVI/card, video-clock and output-device leaves are modeled. The selector,
filename concatenation, player loop, decoder command dispatcher and lag/EOF/
stop branches execute their original SH instructions. No game bytes are stored
in this script or its output.
"""
import argparse
from hashlib import sha256
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent / 'trace-reader'
sys.path[:0] = [str(ROOT / 'tests'), str(ROOT / 'tools')]
from test_vmu_native_heap_execution import SH, MASK, signed
from test_toy_original_gd_poll import SOURCE_BYTES, SOURCE_SHA256
from check_vmu_native_worker_gateway import read_elf, symbol_bytes
from profile_vmu_native_release_sh import ReleaseSH
from sh4_delay_slots import validate_delay_slot

BASE = 0x8c010000
SELECT = 0x8c021fa8
SELECT_RETURN = (0x8c02206c, 0x8c022084)
WRAPPER = 0x8c04c700
PLAYER = 0x8c06d3e2
PLAYER_RETURN = 0x8c04c73c
DISPATCH = 0x8c08d638
DISPATCH_LITERAL = 0x8c06d428
OWNER_STATUS = {0x8c06d3f0, 0x8c06d39a, 0x8c06d3c0, 0x8c06d40c}
SKIP = 0x8c0df1f4
LAG = 0x8c1138c4
DISPLAY = 0x8c0a7790
STACK, RETURN = 0x8c00e000, 0x8c008a00
STATUS, FRAME, SAMPLES = 0x1c4, 0x1c0, 0x244
LAG_LIMIT = 0xa0
WITNESS_OFFSET = 656
RECORD_BYTES = 36
LAG_ABORT = 0xfffff843
NOOP = {
    0x8c04f4f4, 0x8c0109e2, 0x8c026f80, 0x8c04c090, 0x8c057ac8,
    0x8c041774, 0x8c04160a, 0x8c04c088, 0x8c04f514, 0x8c04f482,
    0x8c04f4d8, 0x8c098ea0, 0x8c06dc30, 0x8c06d264, 0x8c098b7c,
    0x8c097452, 0x8c09780a, 0x8c098f66, 0x8c098c40, 0x8c08ef34,
}

STAGE_WITNESS = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
static const uintptr_t literal = 0x8c06d428u;
static unsigned publications;
static bool drop;
static uint32_t uncached;
static uint32_t native_word(uint32_t address) {
    assert(address == (literal | 0x20000000u));
    return uncached;
}
static void native_publish(uint32_t address) {
    assert(address == literal);
    ++publications;
    if (!drop) uncached = *(volatile uint32_t *)literal;
}
#include "toy_movie_exit_stage.inc"
int main(int argc, char **argv) {
    assert(argc == 2);
    uint32_t entry = (uint32_t)strtoul(argv[1], NULL, 0);
    void *page = (void *)(literal & ~(uintptr_t)4095);
    assert(mmap(page, 4096, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == page);
    uint32_t *cached = (void *)literal;
    uncached = *cached = 0x8c08d639u;
    assert(!toy_movie_exit_install(entry) && !publications);
    assert(*cached == 0x8c08d639u && uncached == *cached);
    uncached = *cached = 0x8c08d638u;
    drop = true;
    assert(!toy_movie_exit_install(entry) && publications == 1u);
    assert(*cached == entry && uncached == 0x8c08d638u);
    *cached = uncached;
    drop = false;
    assert(toy_movie_exit_install(entry) && publications == 2u);
    assert(*cached == entry && uncached == entry);
}
'''


class MovieReplay(ReleaseSH):
    def __init__(self, original, aux, clip, pal, scenario):
        SH.__init__(self, original)
        self.active = False
        self.macl = 0
        self.clip, self.pal, self.scenario = clip, pal, scenario
        self.name = (b'50hz_' if pal else b'60hz_') + (b'Intro_LRTF.avi\0' if clip else b'Bumper_LRTF.avi\0')
        self.hook = self.witness = self.ctx = None
        self.pc_trace = []
        self.boundaries = []
        self.dispatches = []
        self.hook_frame = None
        self.clock_calls = 0
        self.rendered = 0
        self.write(DISPLAY, int(not pal))
        if aux is not None:
            data, _, segments, _, syms = aux
            self.hook = syms['_kui_toy_movie_exit_hook'][0]
            context = syms['_kui_native_readahead_aux_context'][0]
            self.witness = context + WITNESS_OFFSET
            for kind, offset, address, _, stored, memory, _, _ in segments:
                if kind == 1:
                    self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
            self.write(DISPATCH_LITERAL, self.hook)
            assert self.block(self.witness, 2 * RECORD_BYTES) == bytes(2 * RECORD_BYTES)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op & 0xf0ff == 0x002a:  # sts pr,Rn
            self.r[n] = self.pr
        elif op & 0xf0ff == 0x4011:  # cmp/pz Rn
            self.flag(not self.r[n] & 0x80000000)
        elif op & 0xf00f in (0x3003, 0x3007):  # cmp/ge and cmp/gt
            left, right = signed(self.r[n], 32), signed(self.r[m], 32)
            self.flag(left >= right if op & 15 == 3 else left > right)
        elif op & 0xff00 == 0xc700:  # mova @(disp,PC),r0
            self.r[0] = ((pc + 4) & ~3) + 4 * (op & 255)
        elif op & 0xf00f == 0x000c:  # mov.b @(r0,Rm),Rn
            self.r[n] = signed(self.read(self.r[0] + self.r[m] & MASK, 1), 8) & MASK
        elif op & 0xf00f == 0x0006:  # mov.l Rm,@(r0,Rn)
            self.write(self.r[0] + self.r[n] & MASK, self.r[m])
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def cstring(self, at):
        for i in range(64):
            if not self.read(at + i, 1):
                return self.block(at, i + 1)
        raise AssertionError('unterminated modeled filename')

    def stub(self, pc):
        if pc in NOOP:
            self.r[0] = 0
        elif pc == 0x8c068210:  # ordinary memset helper
            destination, byte, count = self.r[4:7]
            assert count == 0x250 and not byte
            self.store_block(destination, bytes(count))
            self.r[0] = destination
        elif pc == 0x8c067e90:  # compiler strcpy helper uses R0 destination, R1 source
            destination, source = self.r[:2]
            self.store_block(destination, self.cstring(source))
        elif pc == 0x8c0977dc:  # media open/AVI header
            assert self.r[4] == self.ctx
            assert self.cstring(self.ctx + 0xbc) == self.name
            if self.scenario == 'input':
                self.write(SKIP, 1)
            self.r[0] = 0xfffff837 if self.scenario == 'open' else 0
        elif pc == 0x8c08d400:  # output/audio device setup
            assert self.r[4] == self.ctx
            self.write(self.ctx + LAG_LIMIT, 3)
            self.write(self.ctx + 0x74, 0)
            self.write(self.ctx + 0x1b8, 1)
            self.r[0] = 0
        elif pc == 0x8c09932a:  # transport queue state, ready
            self.r[0] = 1
        elif pc == 0x8c06d260:  # modeled external frame/audio clock
            assert self.r[4] == self.ctx
            if not self.clock_calls:
                self.write(self.ctx + FRAME, 100)
                self.write(self.ctx + SAMPLES, 44100)
            self.clock_calls += 1
            self.r[0] = 107 if self.scenario == 'lag' else 100
        elif pc == 0x8c09774c:  # produce the final decoded frame; owner advances frame count
            assert self.scenario == 'eof' and self.r[4] == self.ctx
            self.rendered += 1
            self.r[0] = 0
        else:
            return False
        self.pc = self.pr
        return True

    def run_movie(self):
        self.pc, self.pr, self.r[15] = SELECT, RETURN, STACK
        self.r[4] = self.clip
        saved = self.r[8:15]
        for _ in range(5000):
            pc = self.pc
            if pc == RETURN:
                assert self.r[8:15] == saved and self.r[15] == STACK and self.pr == RETURN
                return self.r[0]
            if pc == WRAPPER:
                assert self.pr == SELECT_RETURN[self.clip]
                assert self.cstring(self.r[4]) == self.name, (self.cstring(self.r[4]), self.name)
            if pc == PLAYER:
                assert self.pr == PLAYER_RETURN
                self.ctx = self.r[4]
            if pc == PLAYER_RETURN or pc in OWNER_STATUS:
                preserved = self.r[8:16]
                if pc in (0x8c06d39a, 0x8c06d3c0) and preserved[4] == self.hook:
                    preserved[4] = DISPATCH  # R12 caches the one patched literal.
                self.boundaries.append((pc, self.r[0], preserved, self.sr & 1))
                if self.hook_frame is not None and self.hook_frame[0] == pc:
                    owner, _, _, prior = self.hook_frame
                    assert self.r[8:16] == prior and self.pr == owner
                    self.hook_frame = None
            if pc == self.hook:
                assert self.hook_frame is None and self.pr in OWNER_STATUS
                self.hook_frame = self.pr, tuple(self.r[4:6]), self.sr & 1, self.r[8:16]
            if pc == DISPATCH:
                if self.hook_frame is not None:
                    owner, args, t, _ = self.hook_frame
                    assert tuple(self.r[4:6]) == args and self.sr & 1 == t
                else:
                    owner = self.pr
                self.dispatches.append((owner, tuple(self.r[4:6]), self.r[0], self.sr & 1))
            self.pc_trace.append(pc)
            if self.stub(pc):
                continue
            op = self.read(pc, 2)
            target, delay, n = pc + 2, False, (op >> 8) & 15
            if op >> 12 in (0xa, 0xb):
                target, delay = pc + 4 + 2 * signed(op & 0xfff, 12), True
                if op >> 12 == 0xb:
                    self.pr = pc + 4
            elif op & 0xf0ff in (0x400b, 0x402b):
                target, delay = self.r[n], True
                if op & 0xf0ff == 0x400b:
                    self.pr = pc + 4
            elif op & 0xf0ff == 0x0023:  # braf Rn
                target, delay = pc + 4 + self.r[n], True
            elif op == 0x000b:
                target, delay = self.pr, True
            elif op >> 8 in (0x89, 0x8b, 0x8d, 0x8f):
                delay = op >> 8 in (0x8d, 0x8f)
                if bool(self.sr & 1) == (op >> 8 in (0x89, 0x8d)):
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
        raise AssertionError('movie lifecycle exceeded instruction bound')

    def record(self):
        return struct.unpack('<9I', self.block(self.witness + self.clip * RECORD_BYTES, RECORD_BYTES))


def verify_stage_publishing(entry):
    with tempfile.TemporaryDirectory(prefix='kui-toy39-') as work:
        source, exe = Path(work) / 'witness.c', Path(work) / 'witness'
        source.write_text(STAGE_WITNESS)
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-I', str(ROOT / 'src/loader'), str(source), '-o', str(exe)], check=True)
        subprocess.run([str(exe), hex(entry)], check=True)


def verify_case(original, aux, clip, pal, scenario):
    baseline = MovieReplay(original, None, clip, pal, scenario)
    result = baseline.run_movie()
    player_return = next(value for address, value, _, _ in baseline.boundaries if address == PLAYER_RETURN)
    if scenario == 'lag':
        assert player_return == 0xffffffff and 0x8c08d8ec in baseline.pc_trace
        assert baseline.read(LAG) == 7 and not baseline.rendered
        expected = (3, 0x7db, 0, 0, LAG_ABORT, 100, 44100, 7, 6)
    elif scenario == 'eof':
        assert player_return == 1 and 0x8c08da3a in baseline.pc_trace
        assert baseline.rendered == 1 and 0x8c08d8ec not in baseline.pc_trace
        expected = (11, 0x7db, 0, 0, 0x7dc, 101, 44100, 0, 0)
    elif scenario == 'input':
        assert player_return == result == 1 and 0x8c08d3b4 in baseline.pc_trace
        assert not baseline.rendered and 0x8c08da3a not in baseline.pc_trace
        expected = (7, 0x7db, 0, 0, 0x7dc, 0, 0, 0, 0)
    else:
        assert player_return == 0xfffffffe and 0x8c06d3f4 in baseline.pc_trace
        expected = (1, 0xfffff837, 0, 0, 0, 0, 0, 0, 0)
    if aux is None:
        return
    witness = MovieReplay(original, aux, clip, pal, scenario)
    assert witness.run_movie() == result
    # The observed call returns, callee-saved registers, original stack and
    # SR.T must match. The instrumented call adds one private save frame.
    assert witness.boundaries == baseline.boundaries
    assert witness.dispatches == baseline.dispatches
    assert witness.block(witness.ctx, 0x250) == baseline.block(baseline.ctx, 0x250)
    assert witness.record() == expected, (scenario, witness.record(), expected)
    other = witness.witness + (1 - clip) * RECORD_BYTES
    assert witness.block(other, RECORD_BYTES) == bytes(RECORD_BYTES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    parser.add_argument('--aux', type=Path, help='Linked read-ahead ELF with the movie exit witness')
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert len(original) == SOURCE_BYTES and sha256(original).hexdigest() == SOURCE_SHA256
    assert struct.unpack_from('<I', original, DISPATCH_LITERAL - BASE) == (DISPATCH,)
    aux = read_elf(args.aux) if args.aux else None
    if aux:
        syms = aux[4]
        _, descriptor, _ = symbol_bytes(aux, '_kui_native_readahead_aux_exports', 1)
        entry, _, section = symbol_bytes(aux, '_kui_toy_movie_exit_hook', 2)
        assert section[2] & 7 == 6 and not entry & 1
        words = struct.unpack('<16I', descriptor)
        chunk = 0
        if '_kui_toy_movie_chunk_hook' in syms:
            chunk, _, chunk_section = symbol_bytes(aux, '_kui_toy_movie_chunk_hook', 2)
            assert chunk_section[2] & 7 == 6 and not chunk & 1
            assert chunk not in (*words[3:5], entry) and chunk < words[6]
        assert words[10:] == (entry, chunk, 0, 0, 0, 0)
        assert words[0] == 0x4b524131 and words[2] == 64
        assert entry not in words[3:5] and entry < words[6]
        assert words[5] == syms['_kui_native_readahead_aux_context'][0]
    for clip in (0, 1):
        for pal in (False, True):
            for scenario in ('lag', 'eof', 'input', 'open'):
                verify_case(original, aux, clip, pal, scenario)
    if aux:
        verify_stage_publishing(entry)
    print(f"Original {'plus linked witness' if aux else 'baseline'}: 16 lifecycle cases "
          "(two clips, two video modes, lag/EOF/input/startup failure).")


if __name__ == '__main__':
    main()
