#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 84 (SURVEY) in the linked worker: what the survey calls, how deep it
goes on the worker's private stack, where it is called from, and that its
source can reach the hardware only by reading.

usage: test_toy_cdda_survey_linked.py WORKER_ELF WORKER_BUILD_DIR [--self-test]

WORKER_BUILD_DIR holds the worker's objects with the compiler's call graphs
(-fcallgraph-info=su: one .ci file a source, every call site an edge, every
frame with its size and whether it is fixed). The check holds three readings
against each other:

- The compiler's graph: the survey (src/loader/toy_cdda_survey.c) is three
  functions with fixed frames that call only each other, never through a
  pointer; the only calls into it are kui_vmu_native_passive_complete's.
- The linked code (sh-elf-objdump of WORKER_ELF): every instruction reached
  from each function's entry, with the stack pointer's offset and the values
  PC-relative loads put in registers. Each function's frame is the compiler's;
  each call the survey makes resolves to the survey's own functions, as many
  as the graph's edges; the completion's calls into the survey are the graph's
  two, each after its call to stack_valid on every path, and the completion
  loads the survey's address once for each of them; the address is in no word
  of the linked image but the completion's own literals.
- The source: stack_valid admits a completion only with the stack pointer at
  least 256 bytes above the stack's low end, whose lowest 16 words are its
  guard (src/loader/vmu_native_passive.c); the survey reads the hardware only
  through its three volatile const reads, writes only SR (its interrupt mask,
  raised and restored), and copies nothing through a pointer.

The bound, on the least stack the completion's own check admits: a call into
the survey at stack offset s (from the completion's entry), with stack_valid
called at offset v, keeps the survey's lowest stack pointer at least
256 - v + s - depth above the low end. It must stay clear of the guard words
with half of the survey's depth again to spare. And on the stack the
completion is entered with (the completion hook sets the top of the private
stack): the survey's deepest point from the top.

--self-test runs the same check on altered copies of the readings (an added
call, a larger frame, another caller, a write in the source, a call through a
pointer, less room, a call before stack_valid) and fails unless each is
refused.
"""
import argparse
import copy
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SURVEY_SOURCE = 'src/loader/toy_cdda_survey.c'
PASSIVE_SOURCE = 'src/loader/vmu_native_passive.c'
HEARTBEAT = 'kui_toy_cdda_survey_heartbeat'
COMPLETE = 'kui_vmu_native_passive_complete'
SURVEY = (HEARTBEAT, SURVEY_SOURCE + ':stable', SURVEY_SOURCE + ':drain')
LINKED = {HEARTBEAT: '_' + HEARTBEAT, SURVEY_SOURCE + ':stable': '_stable', SURVEY_SOURCE + ':drain': '_drain'}
MARGIN = 1.5
DELAYED = {'bra', 'bt.s', 'bf.s', 'jsr', 'bsr', 'rts', 'jmp', 'braf', 'bsrf'}
NO_WRITE = {'jsr', 'jmp', 'tst', 'tst.b', 'cmp/eq', 'cmp/hi', 'cmp/hs', 'cmp/ge', 'cmp/gt', 'cmp/pl', 'cmp/pz',
            'cmp/str', 'bt', 'bf', 'bt.s', 'bf.s', 'bra', 'braf', 'bsr', 'bsrf', 'rts', 'nop', 'pref', 'ocbp',
            'ocbi', 'ocbwb', 'movca.l', 'lds', 'lds.l', 'ldc', 'ldc.l', 'clrt', 'sett', 'clrmac', 'sleep',
            'trapa', 'rte', 'div0u', 'div0s', 'mul.l', 'mulu.w', 'muls.w', 'dmulu.l', 'dmuls.l'}
UNKNOWN = frozenset(['?'])


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


# ---- the compiler's call graphs ------------------------------------------------

NODE = re.compile(r'^node: \{ title: "([^"]+)" label: "([^"]*)"')
EDGE = re.compile(r'^edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)" label: "([^"]*)"')
FRAME = re.compile(r'\\n(\d+) bytes \(([a-z,]+)\)$')


def read_graphs(build):
    nodes, edges = {}, []
    for ci in sorted(Path(build).rglob('*.ci')):
        for line in ci.read_text().splitlines():
            m = NODE.match(line)
            if m:
                f = FRAME.search(m.group(2))
                # A function's own graph gives its frame; another source's names it as a callee only.
                if f:
                    frame = (int(f.group(1)), f.group(2))
                    assert nodes.get(m.group(1)) in (None, frame), 'two frames for ' + m.group(1)
                    nodes[m.group(1)] = frame
                else:
                    nodes.setdefault(m.group(1), None)
                continue
            m = EDGE.match(line)
            if m:
                edges.append((m.group(1), m.group(2), m.group(3)))
    assert nodes, 'no call graph (.ci) under %s: the worker was not built with -fcallgraph-info=su' % build
    return {'nodes': nodes, 'edges': edges}


def graph_facts(g):
    nodes, edges = g['nodes'], g['edges']
    in_survey = sorted(t for t in nodes if t == HEARTBEAT or t.startswith(SURVEY_SOURCE + ':'))
    assert in_survey == sorted(SURVEY), 'the survey is %s, not the reviewed heartbeat, stable and drain' % in_survey
    frames = {}
    for t in SURVEY:
        assert nodes[t] is not None, '%s has no frame in the graph' % t
        size, kind = nodes[t]
        assert kind == 'static', '%s has a %s frame, not a fixed one' % (t, kind)
        frames[t] = size
    calls = {t: [] for t in SURVEY}
    for source, target, where in edges:
        if source in SURVEY:
            assert target in SURVEY, '%s calls %s (%s), outside the survey' % (source, target, where)
            calls[source].append(target)
        elif target in SURVEY:
            assert target == HEARTBEAT and source == COMPLETE, '%s calls %s (%s)' % (source, target, where)
    callers = [where for source, target, where in edges if target == HEARTBEAT]
    assert callers, 'nothing calls the survey'
    assert nodes.get(COMPLETE) and nodes[COMPLETE][1] == 'static', 'the completion has no fixed frame'
    memo = {}

    def depth(t, path=()):
        assert t not in path, 'recursion through ' + ' -> '.join(path + (t,))
        if t not in memo:
            memo[t] = frames[t] + max((depth(c, path + (t,)) for c in calls[t]), default=0)
        return memo[t]
    return {'frames': frames, 'calls': calls, 'callers': callers, 'depth': depth(HEARTBEAT),
            'complete_frame': nodes[COMPLETE][0]}


# ---- the linked code ----------------------------------------------------------

INSN = re.compile(r'^\s*([0-9a-f]{8}):\s+(\S+)\s*(.*)$')
LITERAL = re.compile(r'^([0-9a-f]{8}) <[^>]*>,(r\d+)\s*!\s*([0-9a-f]+)(?: <([^>]+)>)?\s*$')


def exact(m):
    """The symbol a literal names exactly (not symbol+offset), or None."""
    name = m.group(4)
    return name if name and not re.search(r'[+-]0x', name) else None


def symbols(elf):
    out = {}
    for line in run('sh-elf-nm', '-n', elf).splitlines():
        parts = line.split()
        if len(parts) == 3:
            out.setdefault(parts[2], (int(parts[0], 16), parts[1]))
    return out


def disassemble(elf):
    code, names = {}, {}
    current = None
    for line in run('sh-elf-objdump', '-d', '--no-show-raw-insn', elf).splitlines():
        m = re.match(r'^([0-9a-f]{8}) <([^>]+)>:$', line)
        if m:
            current = m.group(2)
            names[int(m.group(1), 16)] = current
            code[current] = []
            continue
        m = INSN.match(line)
        if m and current:
            code[current].append((int(m.group(1), 16), m.group(2), m.group(3).split('!')[0].strip()
                                  if m.group(2) != 'mov.l' else m.group(3).strip()))
    return code, names


def written(op, args):
    """The general registers an instruction writes, other than through @-rn/@rn+."""
    if op in NO_WRITE or not args:
        return set()
    operands = [a.strip() for a in args.split(',')]
    last = operands[-1]
    return {last} if re.fullmatch(r'r\d+', last) else set()


def target_of(args):
    m = re.match(r'^([0-9a-f]{8})\b', args)
    return int(m.group(1), 16) if m else None


def walk(name, code, names):
    """Every instruction reached from the function's entry: the stack offset at
    it, and the calls made, each with the stack offset the callee starts with,
    its kind and the set of targets the jumped register can hold. Registers and
    the frame's words hold what PC-relative loads give them (a symbol or a
    number), copies of those and spills of them to the frame; anything else is
    '?'. A store through any register but r15 is taken to write the frame's
    address-taken data, never a spill slot, and a callee writes the caller's
    frame only through the pointers it is given, as the compiler lays frames
    out; the callee may change r0-r7."""
    insns = code[name]
    index = {a: i for i, (a, _, _) in enumerate(insns)}
    states, todo = {0: (0, tuple(UNKNOWN for _ in range(16)), (), False)}, [0]
    calls, lowest, literals, loads = {}, 0, set(), {}

    def gpr(operand):
        return int(operand[1:]) if re.fullmatch(r'r\d+', operand) else None

    def step(i, state):
        """The state after instruction i, or raise for what the walk refuses."""
        address, op, args = insns[i]
        sp, regs, slots, checked = state
        regs, slots = list(regs), dict(slots)

        def done():
            return (sp, tuple(regs), tuple(sorted(slots.items())), checked)
        m = LITERAL.match(args) if op == 'mov.l' else None
        if m:
            value = exact(m) or ('#' + m.group(3))
            if exact(m):
                literals.add(exact(m))
                loads.setdefault(exact(m), set()).add(address)
            assert m.group(2) != 'r15', '%s loads the stack pointer at %#x' % (name, address)
            regs[gpr(m.group(2))] = frozenset([value])
            return done()
        plain = args.split('!')[0].strip()
        ops = [o.strip() for o in plain.split(',')] if plain else []

        def frame_word(operand):
            if operand == '@r15':
                return sp
            mm = re.fullmatch(r'@\((\d+),r15\)', operand)
            return sp + int(mm.group(1)) if mm else None
        if len(ops) == 2 and ops[1] == '@-r15':
            assert op in ('mov.l', 'sts.l', 'stc.l'), '%s pushes with %s at %#x' % (name, op, address)
            sp -= 4
            slots[sp] = regs[gpr(ops[0])] if op == 'mov.l' and gpr(ops[0]) is not None else UNKNOWN
            return done()
        if len(ops) == 2 and ops[0] == '@r15+':
            assert op in ('mov.l', 'lds.l', 'ldc.l'), '%s pops with %s at %#x' % (name, op, address)
            if op == 'mov.l':
                assert ops[1] != 'r15', '%s pops the stack pointer at %#x' % (name, address)
                regs[gpr(ops[1])] = slots.get(sp, UNKNOWN)
            sp += 4
            return done()
        assert not any(('@-r15' in o or '@r15+' in o) for o in ops), '%s moves the stack with %s %s at %#x' % (
            name, op, plain, address)
        if op == 'add' and len(ops) == 2 and ops[1] == 'r15':
            mm = re.fullmatch(r'#(-?\d+)', ops[0])
            assert mm, '%s adds a register to the stack pointer at %#x' % (name, address)
            sp += int(mm.group(1))
            return done()
        if len(ops) == 2 and ops[1].startswith('@'):           # a store
            word = frame_word(ops[1])
            if word is not None:
                slots[word & ~3] = regs[gpr(ops[0])] if op == 'mov.l' and gpr(ops[0]) is not None else UNKNOWN
            elif re.fullmatch(r'@\(r0,r15\)', ops[1]):
                slots = {k: UNKNOWN for k in slots}
        if len(ops) == 2 and ops[0].startswith('@') and gpr(ops[1]) is not None and op == 'mov.l':
            word = frame_word(ops[0])
            if word is not None:                               # a load from the frame
                assert ops[1] != 'r15', '%s loads the stack pointer at %#x' % (name, address)
                regs[gpr(ops[1])] = slots.get(word, UNKNOWN)
                return done()
        for r in written(op, plain):
            assert r != 'r15', '%s writes the stack pointer with %s %s at %#x' % (name, op, plain, address)
            if op == 'mov' and re.fullmatch(r'r\d+,r\d+', plain):
                regs[gpr(r)] = regs[gpr(ops[0])]
            else:
                regs[gpr(r)] = UNKNOWN
        for r in set(re.findall(r'@-?(r\d+)', plain)):
            if r != 'r15' and (re.search(r'@-%s(?!\d)' % r, plain) or re.search(r'@%s\+' % r, plain)):
                regs[gpr(r)] = UNKNOWN
        return done()

    def merge(a, b):
        u = a | b
        return u if len(u) <= 8 else UNKNOWN

    def join(i, state):
        old = states.get(i)
        if old is None:
            states[i] = state
            todo.append(i)
            return
        assert old[0] == state[0], '%s reaches %#x with stack offsets %d and %d' % (
            name, insns[i][0], old[0], state[0])
        a, b = dict(old[2]), dict(state[2])
        slots = tuple(sorted((k, merge(a.get(k, UNKNOWN), b.get(k, UNKNOWN))) for k in set(a) | set(b)))
        new = (old[0], tuple(merge(x, y) for x, y in zip(old[1], state[1])), slots, old[3] and state[3])
        if new != old:
            states[i] = new
            todo.append(i)

    def at(address):
        assert address in index, '%s branches to %#x outside itself' % (name, address)
        return index[address]

    while todo:
        i = todo.pop()
        state = states[i]
        lowest = min(lowest, state[0])
        address, op, args = insns[i]
        plain = args.split('!')[0].strip()
        assert op not in ('braf', 'bsrf', 'rte', 'trapa', 'sleep', '.word'), '%s has %s at %#x' % (name, op, address)
        if op in DELAYED:
            assert i + 1 < len(insns), '%s ends in a delay slot' % name
            assert insns[i + 1][1] not in DELAYED, '%s has a branch in a delay slot at %#x' % (name, address)
            after = step(i + 1, state)
            lowest = min(lowest, after[0])
            if op in ('jsr', 'jmp', 'bsr'):
                if op == 'bsr':
                    t = target_of(plain)
                    targets = frozenset([names[t][1:] if names.get(t, '').startswith('_') else '#%x' % t])
                else:
                    targets = frozenset(x[1:] if x.startswith('_') else x for x in state[1][gpr(plain.strip('@'))])
                # The callee starts with the stack the delay slot leaves; the
                # last visit of a call is its settled state.
                calls[address] = (address, op, after[0], targets, state[3])
                if op == 'jmp':
                    continue                      # a tail call: this path's end
                regs = list(after[1])
                for r in range(8):
                    regs[r] = UNKNOWN
                checked = after[3] or targets == frozenset(['stack_valid'])
                join(i + 2, (after[0], tuple(regs), after[2], checked))
            elif op == 'rts':
                assert after[0] == 0, '%s returns with the stack %d off its entry' % (name, after[0])
            elif op == 'bra':
                join(at(target_of(plain)), after)
            else:                                 # bt.s / bf.s
                join(at(target_of(plain)), after)
                join(i + 2, after)
            continue
        after = step(i, state)
        if op in ('bt', 'bf'):
            join(at(target_of(plain)), after)
        assert i + 1 < len(insns), '%s runs off its end' % name
        join(i + 1, after)
    return {'frame': -lowest, 'calls': sorted(calls.values()), 'literals': literals, 'loads': loads}


def segments(elf):
    data = Path(elf).read_bytes()
    assert data[:4] == b'\x7fELF' and data[4] == 1 and data[5] == 1, 'not a 32-bit little-endian ELF'
    phoff, = struct.unpack_from('<I', data, 28)
    phentsize, phnum = struct.unpack_from('<HH', data, 42)
    for k in range(phnum):
        ptype, offset, vaddr, _, filesz, _, _, _ = struct.unpack_from('<8I', data, phoff + k * phentsize)
        if ptype == 1:
            yield vaddr, data[offset:offset + filesz]


def linked_facts(elf):
    sym = symbols(elf)
    code, names = disassemble(elf)
    for linked in list(LINKED.values()) + ['_' + COMPLETE, '_stack_valid']:
        assert linked in code, '%s is not in the linked worker' % linked
    walks = {t: walk(LINKED[t], code, names) for t in SURVEY}
    walks[COMPLETE] = walk('_' + COMPLETE, code, names)
    for t in SURVEY:
        walks[t]['functions'] = sorted(l for l in walks[t]['literals'] if sym.get(l, (0, ''))[1] in ('t', 'T'))
    ordered = sorted(names)
    start = sym['_' + COMPLETE][0]
    end = next(a for a in ordered if a > start)
    heartbeat = sym['_' + HEARTBEAT][0]
    words = [vaddr + k for vaddr, blob in segments(elf) for k in range((-vaddr) % 4, len(blob) - 3, 4)
             if (struct.unpack_from('<I', blob, k)[0] ^ heartbeat) & 0x1fffffff == 0]
    stack = (sym['__vmu_native_worker_stack_bottom'][0], sym['__vmu_native_worker_stack_top'][0])
    hook = code.get('_kui_vmu_native_passive_complete_hook', [])
    top, entered = None, False
    for _, op, args in hook:
        m = LITERAL.match(args) if op == 'mov.l' else None
        if m and m.group(2) == 'r15':
            top = int(m.group(3), 16)
        if m and exact(m) == '_' + COMPLETE and top is not None:
            entered = True
            break
    return {'walks': walks, 'words': words, 'complete_range': (start, end), 'stack': stack,
            'hook_top': top if entered else None}


# ---- the sources ----------------------------------------------------------------

def code_only(text):
    return re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', ' ', text, flags=re.S))


def source_facts(survey, passive):
    survey = code_only(survey)
    body = re.search(r'static bool stack_valid\(void\) \{(.*?)\n\}', passive, re.S)
    assert body, 'stack_valid is not in its reviewed form'
    low = re.search(r'sp<low\+(\d+)u', body.group(1))
    guards = re.search(r'for\(unsigned i=0;i<(\d+)u;i\+\+\) if\(guard\(sp,i\)!=GUARD\) return false;', body.group(1))
    assert low and guards, 'stack_valid no longer checks room and guard words as reviewed'
    guard = re.search(r'static uint32_t guard\(uint32_t sp,unsigned i\) \{(.*?)\n\}', passive, re.S)
    assert guard and 'STACK_LOW' in guard.group(1) and 'address+=i*4u;' in guard.group(1), \
        'the guard words are no longer the stack\'s lowest words'
    assert 'static void fail(uint32_t code) __attribute__((noreturn));' in passive, 'fail may return'
    complete = re.search(r'\nvoid kui_vmu_native_passive_complete\(const struct kui_vmu_native_passive_frame \*f\) \{'
                         r'(.*?)\n\}\n', passive, re.S)
    assert complete, 'the completion is not where it was reviewed'
    text = complete.group(1)
    check, first = text.find('|| !stack_valid()) fail(8u);'), text.find('kui_toy_cdda_survey_heartbeat();')
    assert 0 <= check < first, 'the completion does not check its stack before it surveys'
    # What reaches the hardware: three reads, SR, nothing else.
    reads = re.findall(r'\*\(volatile const uint(32|16|8)_t \*\)\(uintptr_t\)address', survey)
    assert sorted(reads) == ['16', '32', '8'], 'the survey\'s hardware reads are %s' % reads
    assert survey.count('volatile') == len(reads) + 2 and survey.count('__asm__ __volatile__(') == 2, \
        'a volatile access or asm statement besides the three reads and SR'
    assert re.findall(r'__asm__ __volatile__\("([^"]*)"', survey) == ['stc sr,%0', 'ldc %0,sr'], \
        'the survey\'s instructions are no longer only stc sr and ldc sr'
    assert survey.count('(uintptr_t)') == len(reads), 'an address made from a number besides the three reads'
    casts = re.findall(r'\(\s*(?:(?:const|volatile)\s+)*(?:struct\s+\w+|u?int\d+_t|uintptr_t|char|void|bool|'
                       r'unsigned|int|long|short)\s*(?:const\s*)?\*+\s*\)', survey)
    assert len(casts) == len(reads), 'a pointer cast besides the three reads: %s' % casts
    for banned in ('memcpy', 'memset', 'memmove'):
        assert banned not in survey, 'the survey uses %s' % banned
    assert not re.search(r'\(\s*\*\s*\w+\s*\)\s*\(', survey), 'the survey has a function pointer'
    return {'room': int(low.group(1)), 'guard': 4 * int(guards.group(1))}


# ---- the check -----------------------------------------------------------------

def verify(graph, linked, sources):
    g = graph_facts(graph)
    walks = linked['walks']
    lines = []
    for t in SURVEY:
        w = walks[t]
        assert w['frame'] == g['frames'][t], '%s: linked frame %d, the compiler\'s %d' % (t, w['frame'], g['frames'][t])
        expected = sorted(g['calls'][t])
        made = []
        for address, op, sp, targets, _ in w['calls']:
            names = sorted(targets)
            assert len(names) == 1 and SURVEY_SOURCE + ':' + names[0] in SURVEY[1:], \
                '%s calls %s at %#x, not one of the survey\'s own functions' % (t, names, address)
            assert sp == -w['frame'], '%s calls at stack offset %d, inside its frame' % (t, sp)
            made.append(SURVEY_SOURCE + ':' + names[0])
        assert sorted(made) == expected, '%s: linked calls %s, the compiler\'s %s' % (t, sorted(made), expected)
        assert set(w['functions']) <= {'_stable', '_drain'}, '%s holds the addresses of %s' % (t, w['functions'])
        lines.append('  %-45s frame %3d, calls %s' % (t, w['frame'], ', '.join(sorted(set(made))) or 'nothing'))
    # The linked depth: the compiler's frames along the linked calls.
    depth = g['depth']
    c = walks[COMPLETE]
    assert c['frame'] == g['complete_frame'], 'the completion\'s linked frame %d, the compiler\'s %d' % (
        c['frame'], g['complete_frame'])
    sites = [(a, op, sp, chk) for a, op, sp, targets, chk in c['calls'] if HEARTBEAT in targets]
    for a, op, sp, targets, chk in c['calls']:
        if HEARTBEAT in targets:
            assert targets == frozenset([HEARTBEAT]), 'the call at %#x may go to %s' % (a, sorted(targets))
    assert len(sites) == len(g['callers']), 'the completion calls the survey %d times linked, %d in the graph' % (
        len(sites), len(g['callers']))
    assert all(chk for _, _, _, chk in sites), 'a call into the survey not after stack_valid on every path'
    # A call the walk cannot follow (a register kept across a call the compiler
    # knows leaves it alone) is no call into the survey: the survey's address
    # is loaded once for each call found, and nowhere else.
    loaded = len(c['loads'].get('_' + HEARTBEAT, ()))
    assert loaded == len(sites), 'the completion loads the survey\'s address %d times for %d calls' % (
        loaded, len(sites))
    unfollowed = sum(1 for _, _, _, targets, _ in c['calls'] if len(targets) != 1 or
                     any(x == '?' or x.startswith('#') for x in targets))
    valid = [sp for _, _, sp, targets, _ in c['calls'] if targets == frozenset(['stack_valid'])]
    assert valid, 'the completion never calls stack_valid'
    v = max(valid)
    start, end = linked['complete_range']
    assert linked['words'] and all(start <= w < end for w in linked['words']), \
        'the survey\'s address is in the linked image outside the completion\'s literals: %s' % [
            hex(w) for w in linked['words'] if not start <= w < end]
    room, guard = sources['room'], sources['guard']
    worst = None
    for a, op, sp, _ in sites:
        spare = room - v + sp - depth - guard
        lines.append('  call at %#x (%s, stack offset %d): lowest point %d above the low end, %d clear of the guard'
                     % (a, 'tail call' if op == 'jmp' else 'call', sp, room - v + sp - depth, spare))
        worst = spare if worst is None else min(worst, spare)
    usable = worst + depth
    assert usable >= MARGIN * depth, 'on the least stack admitted, %d bytes for a survey of %d (needs %.1f x)' % (
        usable, depth, MARGIN)
    bottom, top = linked['stack']
    assert linked['hook_top'] is not None and (linked['hook_top'] ^ top) & 0x1fffffff == 0, \
        'the completion hook does not enter the completion at the top of the private stack'
    deepest = -min(sp for _, _, sp, _ in sites) + depth
    return lines, {'depth': depth, 'worst': worst, 'usable': usable, 'stack': top - bottom, 'deepest': deepest,
                   'valid_offset': v, 'sites': len(sites), 'room': room, 'guard': guard,
                   'calls': len(c['calls']), 'unfollowed': unfollowed}


def self_test(graph, linked, sources, survey_text, passive_text):
    cases = []

    def case(name, change):
        g, l, s = copy.deepcopy(graph), copy.deepcopy(linked), dict(sources)
        texts = {'survey': survey_text, 'passive': passive_text}
        change(g, l, texts)
        try:
            s = source_facts(texts['survey'], texts['passive'])
            verify(g, l, s)
        except AssertionError as e:
            cases.append((name, 'refused', str(e).splitlines()[0][:90]))
            return
        cases.append((name, 'ACCEPTED', ''))

    case('a call out of the survey', lambda g, l, t: g['edges'].append((HEARTBEAT, 'memcpy', 'x')))
    case('a call through a pointer', lambda g, l, t: g['edges'].append((SURVEY[1], '__indirect_call', 'x')))
    case('a frame the linked code does not have', lambda g, l, t: g['nodes'].__setitem__(HEARTBEAT, (200, 'static')))

    def larger(g, l, t):
        g['nodes'][HEARTBEAT] = (200, 'static')
        w = l['walks'][HEARTBEAT]
        w['frame'] = 200
        w['calls'] = [(a, op, -200, tg, chk) for a, op, sp, tg, chk in w['calls']]
    case('a larger frame, linked and compiled', larger)
    case('a frame of no fixed size', lambda g, l, t: g['nodes'].__setitem__(SURVEY[2], (4, 'dynamic')))
    case('another caller', lambda g, l, t: g['edges'].append(('reader_cooperative_service', HEARTBEAT, 'x')))
    case('the survey\'s address in a table', lambda g, l, t: l['words'].append(0x8c000000))
    case('a linked call the compiler did not make', lambda g, l, t: l['walks'][HEARTBEAT]['calls'].append(
        (0, 'jsr', -l['walks'][HEARTBEAT]['frame'], frozenset(['?']), True)))
    case('the survey\'s address loaded for a call not found', lambda g, l, t: l['walks'][COMPLETE]['loads'].setdefault(
        '_' + HEARTBEAT, set()).add(1))
    case('a call into the survey before stack_valid', lambda g, l, t: l['walks'][COMPLETE].__setitem__(
        'calls', [(a, op, sp, tg, False if HEARTBEAT in tg else chk) for a, op, sp, tg, chk in
                  l['walks'][COMPLETE]['calls']]))
    case('a write in the source', lambda g, l, t: t.__setitem__('survey', t['survey'].replace(
        'static uint32_t sr_read(void) {',
        'static void poke(uint32_t a) { *(volatile uint32_t *)(uintptr_t)a = 0u; }\nstatic uint32_t sr_read(void) {')))
    case('less room in stack_valid', lambda g, l, t: t.__setitem__('passive', t['passive'].replace(
        'sp<low+256u', 'sp<low+160u')))
    for name, result, why in cases:
        print('  %-9s %s%s' % (result, name, (': ' + why) if why else ''))
    bad = [n for n, r, _ in cases if r != 'refused']
    assert not bad, 'the check accepted: ' + ', '.join(bad)
    print('PASS Test 84 survey linked check refuses all %d altered readings' % len(cases))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('elf')
    parser.add_argument('build')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    survey_text = (ROOT / SURVEY_SOURCE).read_text()
    passive_text = (ROOT / PASSIVE_SOURCE).read_text()
    graph = read_graphs(args.build)
    linked = linked_facts(args.elf)
    sources = source_facts(survey_text, passive_text)
    if args.self_test:
        self_test(graph, linked, sources, survey_text, passive_text)
        return
    lines, r = verify(graph, linked, sources)
    print('\n'.join(lines))
    print('PASS Test 84 survey linked: three fixed frames calling only each other, %d bytes deep; called from the '
          'completion only (%d sites, each after stack_valid, the survey\'s address loaded for them only; %d of the '
          'completion\'s %d calls through registers the walk does not follow); on the least stack stack_valid admits '
          '(%d bytes above '
          'the low end at its call, %d of guard) the survey stays %d bytes clear of the guard words (%d usable, '
          '%.2f x); entered at the top of the %d-byte stack, its deepest point is %d bytes down; its source reads '
          'the hardware and writes only SR' % (
              r['depth'], r['sites'], r['unfollowed'], r['calls'], r['room'], r['guard'], r['worst'], r['usable'],
              r['usable'] / r['depth'],
              r['stack'], r['deepest']))


if __name__ == '__main__':
    main()
