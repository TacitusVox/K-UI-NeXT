#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The deepest use of the auxiliary's private stack in a linked Test 84 build.

Test 84 (ROOM) gives 2,304 bytes of the auxiliary's 4 KiB private stack to
its code and keeps the stack's top (include/kui/native_readahead_layout.h).
This check bounds what the linked auxiliary can need there, from its own
instructions and the compiler's frame sizes (the .su files the build writes),
and fails unless the stack holds the bound with half of it again to spare.

The rule, which charges every call it cannot follow at its most:
- A frame is the largest the compiler gives the function (.su).
- A call is direct only when the register it jumps through was last written,
  in that function, by a PC-relative load of a word naming a function (or by
  a copy of such a register); bsr is direct. Anything else is indirect.
- An indirect call is the low resident's operations (src/loader/retail_async.c
  ops table) or a function the auxiliary was handed: it is charged the low
  resident's whole private stack (352 bytes, include/kui/retail_loader_layout.h)
  plus the deepest auxiliary function whose address the auxiliary hands out
  (a cursor's write callback, which the low resident's feed calls back).
- With --player, an indirect call is also charged the deepest path of the
  worker's function the stage hands the auxiliary (Test 84's HOOK), whichever
  is the more.
- The visit gate (src/loader/native_readahead_aux_entry.S) starts the stack 32
  bytes below its top and saves six registers before it calls the dispatcher;
  the stage's init call starts at the same place. The guard words take the
  stack's lowest 16 bytes.

usage: test_toy_cdda_stack_bound.py BUILD [--player WORKER_ELF WORKER_SU_DIR SYMBOL]
"""
import argparse
import re
import subprocess
from pathlib import Path

LOW = 352
GATE = 32 + 6 * 4
GUARD = 16
MARGIN = 1.5
ROOTS = ('_kui_native_readahead_aux_dispatch', '_kui_native_readahead_aux_init')
HEAD = re.compile(r'^([0-9a-f]{8}) <([^>]+)>:')
INSN = re.compile(r'^\s*([0-9a-f]{8}):\s+(?:[0-9a-f]{2} )+\s*(\S+)\s*(.*)$')
NO_DEST = {'jsr', 'jmp', 'tst', 'tst.b', 'cmp/eq', 'cmp/hi', 'cmp/hs', 'cmp/ge', 'cmp/gt', 'cmp/pl',
           'cmp/pz', 'cmp/str', 'bt', 'bf', 'bt.s', 'bf.s', 'bra', 'braf', 'bsr', 'bsrf', 'rts', 'nop',
           'pref', 'ocbp', 'ocbi', 'ocbwb', 'movca.l', 'lds', 'lds.l', 'ldc', 'ldc.l', 'clrt', 'sett',
           'clrmac', 'sleep', 'trapa', 'rte'}


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def frames(su_dir):
    sizes = {}
    for su in Path(su_dir).rglob('*.su'):
        for line in su.read_text().splitlines():
            parts = line.split('\t')
            if len(parts) >= 2 and parts[1].isdigit():
                name = parts[0].rsplit(':', 1)[-1]
                sizes[name] = max(sizes.get(name, 0), int(parts[1]))
    return sizes


def frame_of(sizes, symbol):
    name = symbol[1:] if symbol.startswith('_') else symbol
    if name in sizes:
        return sizes[name]
    # GCC names a clone "f.isra.0" in the object and "f.isra" in the .su file.
    m = re.match(r'^(.*?\.(?:isra|constprop|part|cold))(?:\.\d+)*$', name)
    if m and m.group(1) in sizes:
        return sizes[m.group(1)]
    return sizes.get(name.split('.')[0])


def pushes(code):
    """An assembler function's frame: what it pushes (it has no .su line)."""
    return 4 * sum(1 for _, op, args in code
                   if (op in ('mov.l', 'sts.l', 'stc.l', 'fmov.s', 'fmov') and '@-r15' in args))


def functions(elf):
    out, current = {}, None
    for line in run('sh-elf-objdump', '-d', elf).splitlines():
        m = HEAD.match(line)
        if m:
            current = m.group(2)
            out[current] = []
            continue
        m = INSN.match(line)
        if m and current:
            out[current].append((int(m.group(1), 16), m.group(2), m.group(3)))
    return out


def destination(op, args):
    if op in NO_DEST or not args:
        return None
    dest = args.split('!')[0].split(',')[-1].strip()
    return None if dest.startswith('@') else dest


def literal_symbol(op, args):
    """The function a PC-relative literal load names, or None."""
    if op != 'mov.l' or '! ' not in args or '@(' in args.split('!')[0]:
        return None
    m = re.search(r'<([^>+]+)>\s*$', args)
    return m.group(1) if m else None


def graph(funcs):
    calls, indirect, handed = {}, {}, set()
    for name, code in funcs.items():
        calls[name], indirect[name] = set(), []
        jumped = set()
        for i, (address, op, args) in enumerate(code):
            if op == 'bsr':
                m = re.search(r'<([^>+]+)(\+0x[0-9a-f]+)?>', args)
                # A branch to a label inside the same function is no call.
                if m and not (m.group(1) == name and m.group(2)):
                    calls[name].add(m.group(1))
            if op not in ('jsr', 'jmp'):
                continue
            reg, target, j = args.strip().strip('@'), None, i - 1
            while j >= 0:
                _, op2, args2 = code[j]
                if destination(op2, args2) == reg or re.search(r'@%s\+' % reg, args2):
                    symbol = literal_symbol(op2, args2)
                    if symbol:
                        target = symbol
                        jumped.add(j)
                    elif op2 == 'mov' and re.match(r'^r\d+,r\d+$', args2.strip()):
                        reg = args2.split(',')[0].strip()
                        j -= 1
                        continue
                    break
                j -= 1
            if target:
                calls[name].add(target)
            else:
                indirect[name].append(address)
        # An address loaded and not jumped through is handed out.
        for j, (_, op, args) in enumerate(code):
            symbol = literal_symbol(op, args)
            if symbol and j not in jumped and symbol in funcs:
                handed.add(symbol)
    return calls, indirect, handed


def bound(funcs, sizes, calls, indirect, handed, player_depth):
    memo = {}
    missing = set()
    # What the low resident calls back is C code. An entry that reaches the
    # visit gate is an interrupt's own visit (the stream's timer callback),
    # which the gate refuses while a visit runs: not a nested call.
    callbacks = [c for c in handed if frame_of(sizes, c) is not None and
                 '_kui_native_readahead_aux_visit' not in reach(calls, c)]

    def depth(name, path=()):
        if name in path:
            raise AssertionError('recursion through ' + ' -> '.join(path + (name,)))
        if name in memo:
            return memo[name]
        own = frame_of(sizes, name)
        if own is None:
            code = funcs.get(name, [])
            # Assembler code (the gate, the word loops): its pushes, and it may
            # not move the stack otherwise.
            if any(re.search(r'(add|sub)\s+\S+,r15$', args) or re.search(r',r15$', args) and op == 'mov'
                   for _, op, args in code):
                missing.add(name)
            own = pushes(code)
        best, via = 0, ''
        for callee in calls.get(name, ()):
            d, chain = depth(callee, path + (name,))
            if d > best:
                best, via = d, chain
        if indirect.get(name):
            callback = max((depth(c, path + (name,))[0] for c in callbacks if c not in path and c != name),
                           default=0)
            charge = max(LOW + callback, player_depth)
            if charge > best:
                best, via = charge, 'indirect(%d)' % charge
        memo[name] = (own + best, '%s %d' % (name, own) + (' -> ' + via if via else ''))
        return memo[name]

    results = {root: depth(root) for root in ROOTS if root in funcs}
    assert results, 'no visit root in the auxiliary'
    assert not missing, 'functions without a frame size: ' + ', '.join(sorted(missing))
    return results


def player_bound(elf, su_dir, symbol):
    funcs = functions(elf)
    sizes = frames(su_dir)
    calls, indirect, handed = graph(funcs)
    assert symbol in funcs, symbol + ' is not in the worker'
    # The player may call nothing it was not linked with: it is handed no operation.
    assert not any(indirect.get(name) for name in reach(calls, symbol)), 'the player makes an indirect call'
    return bound_of(funcs, sizes, calls, symbol)


def reach(calls, root):
    seen, todo = set(), [root]
    while todo:
        name = todo.pop()
        if name not in seen:
            seen.add(name)
            todo.extend(calls.get(name, ()))
    return seen


def bound_of(funcs, sizes, calls, root):
    memo = {}

    def depth(name, path=()):
        if name in path:
            raise AssertionError('recursion through ' + ' -> '.join(path + (name,)))
        if name not in memo:
            own = frame_of(sizes, name) or 0
            memo[name] = own + max((depth(c, path + (name,)) for c in calls.get(name, ())), default=0)
        return memo[name]
    return depth(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('build')
    parser.add_argument('--player', nargs=3, metavar=('WORKER_ELF', 'WORKER_SU_DIR', 'SYMBOL'))
    args = parser.parse_args()
    build = Path(args.build)
    elf = build / 'native-ra-aux.elf'
    symbols = {}
    for line in run('sh-elf-nm', str(elf)).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    stack = symbols['__native_ra_stack_top'] - symbols['__native_ra_stack_bottom']
    funcs = functions(str(elf))
    sizes = frames(build / 'ra')
    calls, indirect, handed = graph(funcs)
    player = player_bound(*args.player) if args.player else 0
    results = bound(funcs, sizes, calls, indirect, handed, player)
    deepest = max(results.values())
    need = deepest[0] + GATE
    usable = stack - GUARD
    for root, (d, chain) in sorted(results.items()):
        print('  %-40s %5d  %s' % (root, d, chain))
    assert usable >= MARGIN * need, 'stack %d (usable %d) holds less than %.1f x the bound %d' % (
        stack, usable, MARGIN, need)
    print('PASS auxiliary stack bound: %d bytes deepest with every indirect call charged %d%s, '
          '%d with the gate; %d usable of %d (%.2f x)' % (
              deepest[0], LOW, ' or the player %d' % player if player else '', need, usable, stack,
              usable / need))


if __name__ == '__main__':
    main()
