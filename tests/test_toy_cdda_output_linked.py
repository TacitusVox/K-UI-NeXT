#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 87 (the frames' way into the ring) in the linked worker.

usage: test_toy_cdda_output_linked.py native-worker.elf

The sound output turns a play's frames into the sound chip's ADPCM and
stores them in sound memory (src/loader/toy_cdda_output.c). Every frame goes
through one routine written by hand, kui_toy_cdda_squeeze, which the host
model cannot run: it runs the C the routine stands for. This runs the
routine as linked, with the small interpreter of tests/sh4_mini.py, against
a coder of the test's own, written from the chip's decoder:

- words of eight random samples from random decoder states, and the states
  and samples that reach every limit: the signal at either end of its 16
  bits, the step at 127 and at 24,576, the moves of more than 32,767 that
  must not be asked for, samples at and next to each of a step's fractions;
- the routine reads its eight samples and the coder, writes the coder, and
  touches nothing else: no stack, no register it must keep.

Then the whole way, feed() as linked, for a run of whole units: the words
stored in sound memory are the coder's, each plane's in its place and round
the ring's end; the write position, the counts and both coders move on by
what was stored; the G2 FIFO is asked before each eight stores; and a
stretch whose time runs out stops between units with everything in step.
It counts what a kilobyte of frames executes and touches of the worker's
uncached stack and state, and holds that to a bound.

It checks the code as linked. It does not check the Dreamcast.
"""
import random
import struct
import sys

from sh4_mini import Machine, Fault

SCALE = (230, 230, 230, 230, 307, 409, 512, 614)
SCRATCH = 0x8C700000
SOUND = 0xA0800000
FIFO = 0xA05F688C
TCNT0 = 0xFFD8000C


def decode(signal, step, code):
    """The chip's decoder, one code on."""
    size = code & 7
    move = step * (2 * size + 1) // 8
    assert move <= 32767, 'a move the accounts of the chip differ on'
    signal = max(-32768, min(32767, signal - move if code & 8 else signal + move))
    step = max(127, min(24576, step * SCALE[size] // 256))
    return signal, step


def code_for(signal, step, sample):
    """The code for a sample where a decoder stands: the largest size within
    the difference by the step, its half and its quarter, and none that
    moves more than 32,767."""
    far = abs(sample - signal)
    size = reach = 0
    if far >= step:
        size, reach = 4, step
    if far - reach >= step // 2:
        size, reach = size + 2, reach + step // 2
    if far - reach >= step // 4:
        size += 1
    while step * (2 * size + 1) // 8 > 32767:
        size -= 1
    return size | (8 if sample < signal else 0)


def word_for(signal, step, samples):
    word = 0
    for i, sample in enumerate(samples):
        code = code_for(signal, step, sample)
        signal, step = decode(signal, step, code)
        word |= code << 4 * i
    return word, signal, step


def s16(value):
    return value - 0x10000 if value & 0x8000 else value


class Worker(Machine):
    """The linked worker with its accesses counted by where they go."""

    def __init__(self, elf):
        super().__init__()
        self.load_elf(elf)
        self.sym = Machine.symbols(elf)
        self.by = {}
        self.classes = []
        self.since_empty = 8
        self.fifo_polls = 0
        self.tick = 0xFFFF0000
        self.tick_step = 3
        self.devices[TCNT0] = (self._tcnt, None)
        self.devices[FIFO] = (self._fifo, None)

    def _tcnt(self, width):
        self.tick -= self.tick_step
        return self.tick & 0xFFFFFFFF

    def _fifo(self, width):
        self.fifo_polls += 1
        self.since_empty = 0
        return 0

    def _class(self, address):
        p = self.physical(address)
        for name, lo, hi in self.classes:
            if lo <= p < hi:
                return name
        return 'other'

    def read(self, address, width):
        if (address & 0xFFFFFFFF) not in self.devices:
            k = (self._class(address), 'load')
            self.by[k] = self.by.get(k, 0) + 1
        return super().read(address, width)

    def write(self, address, value, width):
        k = (self._class(address), 'store')
        self.by[k] = self.by.get(k, 0) + 1
        if k[0] == 'sound memory':
            self.since_empty += 1
            if self.since_empty > 8:
                raise Fault('a ninth store to sound memory since the G2 FIFO was asked')
        return super().write(address, value, width)


def squeeze_check(elf, trials, seed):
    m = Worker(elf)
    scratch = m.map(SCRATCH, bytes(4096))
    if '_kui_toy_cdda_squeeze' not in m.sym:
        raise Fault('the worker has no kui_toy_cdda_squeeze')
    entry = m.sym['_kui_toy_cdda_squeeze']
    stack_lo, stack_hi = m.sym['__vmu_native_worker_stack_bottom'], m.sym['__vmu_native_worker_stack_top']
    if not any(base <= m.physical(stack_lo) < base + len(store) for base, store in m.regions):
        m.map(stack_lo, bytes(stack_hi - stack_lo))
    rng = random.Random(seed)
    steps = [127, 128, 129, 141, 255, 256, 1000, 4096, 17476, 17477, 17478, 19000, 21845, 21846, 23831, 23832, 24575, 24576]
    signals = [0, 1, -1, 32767, 32766, -32768, -32767, 12345, -12345, 30000, -30000]
    most = 0
    seen_sizes, seen_limits = set(), set()
    for trial in range(trials):
        signal = rng.choice(signals) if trial % 3 == 0 else rng.randint(-32768, 32767)
        step = rng.choice(steps) if trial % 2 == 0 else rng.randint(127, 24576)
        kind = trial % 5
        samples = []
        sig, stp = signal, step
        for i in range(8):
            if kind == 0:
                sample = rng.randint(-32768, 32767)
            elif kind == 1:
                sample = max(-32768, min(32767, sig + rng.randint(-3, 3) * (stp // 4) + rng.randint(-1, 1)))
            elif kind == 2:
                sample = rng.choice((-32768, 32767, 0, sig))
            elif kind == 3:
                sample = max(-32768, min(32767, sig + rng.choice((-1, 1)) * rng.choice((stp, stp // 2, stp // 4,
                             stp + stp // 2, stp + stp // 2 + stp // 4)) + rng.randint(-1, 1)))
            else:
                sample = max(-32768, min(32767, sig + rng.randint(-2 * stp, 2 * stp)))
            samples.append(sample)
            code = code_for(sig, stp, sample)
            seen_sizes.add(code)
            if stp * (2 * (code & 7) + 1) // 8 > 32767 - 4096 or stp * 15 // 8 > 32767:
                seen_limits.add('move')
            sig, stp = decode(sig, stp, code)
            if sig in (-32768, 32767):
                seen_limits.add('signal %d' % sig)
            if stp in (127, 24576):
                seen_limits.add('step %d' % stp)
        want, want_signal, want_step = word_for(signal, step, samples)
        # The side's samples lie every other 16 bits; the other side's between them are not its business.
        at = 256 + 4 * rng.randint(0, 31)
        side = rng.randint(0, 1)
        scratch[:] = bytes([0xA5]) * len(scratch)
        for i, sample in enumerate(samples):
            struct.pack_into('<h', scratch, at + 2 * side + 4 * i, sample)
        coder = 1024 + 8 * rng.randint(0, 31)
        struct.pack_into('<iI', scratch, coder, signal, step)
        before = bytes(scratch)
        m.touched = set()
        m.r = [0xDEAD0000 + i for i in range(16)]
        executed = m.executed
        got = m.call(entry, SCRATCH + at + 2 * side, SCRATCH + coder, stack=stack_hi - 64)
        most = max(most, m.executed - executed)
        got_signal, got_step = struct.unpack_from('<iI', scratch, coder)
        if (got, got_signal, got_step) != (want, want_signal, want_step):
            raise Fault('trial %d: from signal %d step %d the samples %s give %08x, signal %d, step %d; the coder gives '
                        '%08x, %d, %d' % (trial, signal, step, samples, got, got_signal, got_step, want, want_signal, want_step))
        after = bytearray(scratch)
        after[coder:coder + 8] = before[coder:coder + 8]
        if bytes(after) != before:
            raise Fault('trial %d: the routine wrote beside the coder' % trial)
        p = m.physical
        allowed = {p(SCRATCH + at + 2 * side + 4 * i) for i in range(8)} | {p(SCRATCH + coder), p(SCRATCH + coder + 4)}
        text = (p(entry), p(entry) + 0x200)
        stray = [a for a in m.touched if a not in allowed and not text[0] <= a < text[1]]
        if stray:
            raise Fault('trial %d: the routine touched %s' % (trial, ', '.join('%#x' % a for a in sorted(stray)[:4])))
    if len(seen_sizes) != 16 or len(seen_limits) != 5:
        raise Fault('the trials did not reach every code and limit: %d codes, %s' % (len(seen_sizes), sorted(seen_limits)))
    return most


def feed_check(elf, bound_accesses, bound_instructions):
    m = Worker(elf)
    sym = m.sym
    O, mail = sym['_kui_toy_cdda_output'], sym['_kui_toy_cdda_mail']
    stack_lo, stack_hi, bss_end = sym['__vmu_native_worker_stack_bottom'], sym['__vmu_native_worker_stack_top'], \
        sym['__vmu_native_worker_bss_end']
    sound = m.map(SOUND, bytes(0x200000))
    if not any(base <= m.physical(stack_lo) < base + len(store) for base, store in m.regions):
        m.map(stack_lo, bytes(stack_hi - stack_lo))
    p = Machine.physical
    sink = mail + 1312 - 1024
    state_lo = min(a for n, a in sym.items() if n in ('_kui_toy_cdda_output', '_kui_toy_cdda_mail', '_kui_toy_cdda_drive')) & ~0xFFF
    m.classes = [('the bytes', p(sink), p(sink) + 1024), ('stack', p(stack_lo), p(stack_hi)), ('state', p(state_lo), p(bss_end)),
                 ('sound memory', 0x00800000, 0x00A00000)]
    # The output's state as the header lays it out (include/kui/toy_cdda_output.h): all 32-bit words up to the unit's own room.
    names = ('phase why why_detail generation started version family seen_version seen_hash channel0 channel1 ring frames '
             'placed place_try place_at wiped spare sector piece_end passes finished audio_end segment track got card asked '
             'broken written at key_tick key_count played played_tick moved moved_tick carried').split()
    field = {name: O + 4 * i for i, name in enumerate(names)}
    carry = O + 4 * len(names)
    coder = carry + 32

    def w32(address, value):
        Machine.write(m, address, value & 0xFFFFFFFF, 4)

    def r32(address):
        return Machine.read(m, address, 4)

    def run(frames, ring, at, units, tick_step=3):
        for i in range(0, coder + 16 - O, 4):
            w32(O + i, 0)
        w32(field['channel0'], 62)
        w32(field['channel1'], 63)
        w32(field['ring'], ring)
        w32(field['frames'], frames)
        w32(field['placed'], 1)
        w32(field['at'], at)
        w32(field['written'], 100 * frames + at)
        rng = random.Random(1000 * frames + at + units)
        states = [(rng.randint(-20000, 20000), rng.randint(127, 6000)) for _ in range(2)]
        for side in range(2):
            w32(coder + 8 * side, states[side][0])
            w32(coder + 8 * side + 4, states[side][1])
        data = bytearray()
        level = [states[0][0], states[1][0]]
        for _ in range(8 * units):
            for side in range(2):
                level[side] = max(-32768, min(32767, level[side] + rng.randint(-3000, 3000)))
                data += struct.pack('<h', level[side])
        for i in range(0, len(data), 4):
            w32(sink + i, int.from_bytes(data[i:i + 4], 'little'))
        sound[:] = bytes([0x5A]) * len(sound)
        b = stack_hi - 64
        m.tick, m.tick_step = 0xFFFF0000, tick_step
        for i, v in enumerate((~m.tick & 0xFFFFFFFF, ~m.tick & 0xFFFFFFFF, 0, 5, 1)):
            w32(b + 4 * i, v)
        m.by, m.since_empty, m.fifo_polls = {}, 8, 0
        executed = m.executed
        taken = m.call(sym['_feed'], b, sink, len(data), stack=stack_hi - 128)
        executed = m.executed - executed
        done = taken // 32
        if taken % 32 or done > units:
            raise Fault('feed took %d bytes of %d' % (taken, len(data)))
        # What the coder of this test's own stores for the units taken.
        span, base = frames // 2, ring + 32
        expect = bytearray(bytes([0x5A]) * len(sound))
        where = at
        for unit in range(done):
            for side in range(2):
                samples = [s16(int.from_bytes(data[32 * unit + 4 * i + 2 * side:32 * unit + 4 * i + 2 * side + 2], 'little'))
                           for i in range(8)]
                word, signal, step = word_for(states[side][0], states[side][1], samples)
                states[side] = (signal, step)
                struct.pack_into('<I', expect, base + side * span + where // 2, word)
            where = where + 8 if where + 8 < frames else 0
        if bytes(sound) != bytes(expect):
            first = next(i for i in range(0, len(sound), 4) if sound[i:i + 4] != expect[i:i + 4])
            raise Fault('sound memory at %#x holds %s, the coder gives %s' % (first, sound[first:first + 4].hex(),
                        expect[first:first + 4].hex()))
        for side in range(2):
            got = (s32(r32(coder + 8 * side)), r32(coder + 8 * side + 4))
            if got != states[side]:
                raise Fault('side %d: the coder stands at %s, a decoder behind the codes at %s' % (side, got, states[side]))
        if r32(field['at']) != where or r32(field['written']) != 100 * frames + at + 8 * done:
            raise Fault('the write position did not move on by the units stored')
        if m.fifo_polls < (done + 3) // 4:
            raise Fault('%d units stored and the G2 FIFO asked %d times' % (done, m.fifo_polls))
        ok = Machine.read(m, b + 16, 1)
        return done, executed, dict(m.by), ok

    # A kilobyte in the middle of MANATEE 2's spare ring: all of it, and what it costs.
    done, executed, by, ok = run(8128, 0x16000, 4000, 32)
    if done != 32 or not ok:
        raise Fault('a kilobyte was not taken whole')
    uncached = sum(n for (where, _), n in by.items() if where in ('stack', 'state', 'the bytes'))
    if uncached > bound_accesses or executed > bound_instructions:
        raise Fault('a kilobyte of frames takes %d instructions and %d uses of uncached memory (bounds %d and %d)' % (
            executed, uncached, bound_instructions, bound_accesses))
    cost = (executed, uncached, by)
    # Round the ring's end, in each family's ring; a run of one unit, and of five.
    for frames, ring, at, units in ((8128, 0x16000, 8128 - 24, 32), (12224, 0x15000, 12224 - 8, 9), (12224, 0x15000, 0, 1),
                                    (8128, 0x16000, 8, 5), (8128, 0x16000, 8128 - 8, 1)):
        done, _, _, ok = run(frames, ring, at, units)
        if done != units or not ok:
            raise Fault('%d units at %d of %d: %d stored' % (units, at, frames, done))
    # A stretch whose time runs out: the run stops between units, a multiple of four on, and says so.
    done, _, _, ok = run(8128, 0x16000, 4000, 32, tick_step=700)
    if not 0 < done < 32 or done % 4 or ok:
        raise Fault('a stretch out of time stored %d units of 32 (bus still good: %d)' % (done, ok))
    return cost


def s32(value):
    return value - (1 << 32) if value & 0x80000000 else value


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    elf = sys.argv[1]
    most = squeeze_check(elf, 20000, 87)
    print('PASS kui_toy_cdda_squeeze as linked: 20000 words of eight samples equal to the test\'s own coder, every code, both '
          'ends of the signal and of the step and the moves it must not ask for among them; %d instructions for eight samples '
          'at the most; no stack, no register and nothing beside its samples and its coder touched' % most)
    executed, uncached, by = feed_check(elf, 1300, 42000)
    print('PASS the frames\' way into the ring as linked: the words stored are the coder\'s, in their planes and round the '
          'ring\'s end, the write position and both coders in step, the G2 FIFO asked before each eight stores, a stretch '
          'out of time stopped between units; a kilobyte of frames takes %d instructions and %d uses of the worker\'s '
          'uncached memory (%d of the stack, %d of its state, %d of the bytes themselves) for %d words stored' % (
              executed, uncached, by.get(('stack', 'load'), 0) + by.get(('stack', 'store'), 0),
              by.get(('state', 'load'), 0) + by.get(('state', 'store'), 0), by.get(('the bytes', 'load'), 0),
              by.get(('sound memory', 'store'), 0)))


if __name__ == '__main__':
    try:
        main()
    except Fault as fault:
        sys.exit('FAIL ' + str(fault))
