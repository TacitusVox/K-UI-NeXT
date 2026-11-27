#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 85 (CARD) in the linked worker: how deep the audio's consumer goes on
the worker's private stack, and where it is called from.

usage: test_toy_cdda_card_linked.py WORKER_ELF WORKER_BUILD_DIR AUX_ELF [--self-test]

The consumer (kui_toy_cdda_consumer_heartbeat) is called by the worker's
completion behind the sound chip's survey, on the stack the completion's own
check admits (stack_valid, src/loader/vmu_native_passive.c): at least 256 bytes
above the stack's low end, whose lowest 16 words are its guard. This check
holds the compiler's call graph (-fcallgraph-info=su), the linked code and
that source against each other, with tests/test_toy_cdda_survey_linked.py's
readings of them:

- Every function reached from the consumer has a fixed frame, the linked
  code's and the compiler's the same, and none calls itself.
- A call through a pointer is one of two. In kui_toy_cdda_card_tick it is the
  auxiliary's visit gate, which saves six registers on the caller's stack
  before it changes to the auxiliary's own (read from AUX_ELF: the pushes
  before the gate loads its stack pointer). In kui_toy_cdda_card_heartbeat it
  is the consumer's take(): charged the deepest of the functions whose
  address the consumer's own code loads without calling them.
- The completion alone calls the consumer, as often linked as compiled, each
  time after stack_valid on every path, and behind a call of the survey where
  the worker is built with one (TOY_CDDA_SURVEY); the consumer's address is in
  no word of the linked image but the completion's own literals.

The bound, as the survey's: on the least stack the completion's check admits,
the consumer's lowest stack pointer stays clear of the guard words with half
of its depth again to spare.

--self-test runs the same check on altered readings (a larger frame, a frame
of no fixed size, a deeper take, a gate that saves more, another caller, a
call before stack_valid, a call through a pointer elsewhere, less room) and
fails unless each is refused.
"""
import argparse
import copy
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_toy_cdda_survey_linked as survey  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CONSUMER = 'kui_toy_cdda_consumer_heartbeat'
HEARTBEAT = 'kui_toy_cdda_card_heartbeat'
TICK = 'kui_toy_cdda_card_tick'
GATE = '_kui_native_readahead_aux_visit'
INDIRECT = '__indirect_call'
MARGIN = 1.5


def linked_name(title):
    return '_' + title.rsplit(':', 1)[-1]


def gate_pushes(aux_elf):
    """What the visit gate leaves on its caller's stack: its pushes before it
    loads the auxiliary's stack pointer."""
    code, _ = survey.disassemble(aux_elf)
    assert GATE in code, 'the auxiliary has no visit gate'
    pushes = 0
    for _, op, args in code[GATE]:
        plain = args.split('!')[0].strip()
        if op == 'mov.l' and re.search(r',r15$', plain) and not plain.startswith('@r15+'):
            return 4 * pushes
        if '@-r15' in plain:
            pushes += 1
        assert not (op == 'add' and plain.endswith(',r15')), 'the gate moves its caller\'s stack pointer'
    raise AssertionError('the gate never loads its own stack pointer')


def readings(worker_elf, build, aux_elf):
    graph = survey.read_graphs(build)
    code, names = survey.disassemble(worker_elf)
    sym = survey.symbols(worker_elf)
    walks = {}

    def walk(title):
        if title not in walks:
            linked = linked_name(title)
            assert linked in code, '%s is not in the linked worker' % linked
            walks[title] = survey.walk(linked, code, names)
        return walks[title]

    # Everything the compiler has the consumer reach.
    calls = {}
    for source, target, _ in graph['edges']:
        calls.setdefault(source, []).append(target)
    reached, todo = [], [CONSUMER]
    while todo:
        title = todo.pop()
        if title in reached or title == INDIRECT:
            continue
        reached.append(title)
        todo.extend(calls.get(title, ()))
    frames = {t: walk(t)['frame'] for t in reached}
    # The functions whose address the consumer's code loads and does not call.
    called = {linked_name(t) for t in reached}
    handed = sorted(l for t in reached if t not in (HEARTBEAT, TICK) for l in walk(t)['literals']
                    if sym.get(l, (0, ''))[1] in ('t', 'T') and l not in called)
    complete = survey.walk('_' + survey.COMPLETE, code, names)
    ordered = sorted(names)
    start = sym['_' + survey.COMPLETE][0]
    end = next(a for a in ordered if a > start)
    address = sym['_' + CONSUMER][0]
    import struct
    words = [vaddr + k for vaddr, blob in survey.segments(worker_elf) for k in range((-vaddr) % 4, len(blob) - 3, 4)
             if (struct.unpack_from('<I', blob, k)[0] ^ address) & 0x1fffffff == 0]
    return {'graph': graph, 'reached': reached, 'frames': frames, 'handed': handed, 'complete': complete,
            'complete_range': (start, end), 'words': words, 'gate': gate_pushes(aux_elf),
            'survey': '_' + survey.HEARTBEAT in sym,
            'handed_frames': {h: survey.walk(h, code, names) for h in handed}}


def verify(r, sources):
    nodes, edges = r['graph']['nodes'], r['graph']['edges']
    calls = {}
    for source, target, _ in edges:
        calls.setdefault(source, []).append(target)
    lines = []
    for t in r['reached']:
        assert nodes.get(t) is not None, '%s has no frame in the compiler\'s graph' % t
        size, kind = nodes[t]
        assert kind == 'static', '%s has a %s frame, not a fixed one' % (t, kind)
        assert r['frames'][t] == size, '%s: linked frame %d, the compiler\'s %d' % (t, r['frames'][t], size)
    for h, w in r['handed_frames'].items():
        assert not w['calls'], 'the consumer\'s take %s calls on (%d calls)' % (h, len(w['calls']))
    take = max((w['frame'] for w in r['handed_frames'].values()), default=0)
    memo = {}

    def depth(t, path=()):
        assert t not in path, 'recursion through ' + ' -> '.join(path + (t,))
        if t in memo:
            return memo[t]
        best, via = 0, ''
        for c in calls.get(t, ()):
            if c == INDIRECT:
                assert t in (TICK, HEARTBEAT), '%s calls through a pointer' % t
                d, chain = (r['gate'], 'the gate\'s registers %d' % r['gate']) if t == TICK else (take, 'take %d' % take)
            else:
                d, chain = depth(c, path + (t,))
            if d > best:
                best, via = d, chain
        memo[t] = (nodes[t][0] + best, '%s %d' % (t.rsplit(':', 1)[-1], nodes[t][0]) + (' -> ' + via if via else ''))
        return memo[t]
    deepest, chain = depth(CONSUMER)
    assert TICK in r['reached'] and INDIRECT in calls.get(TICK, ()), 'the consumer makes no visit to the auxiliary'
    lines.append('  ' + chain)
    callers = [(s, w) for s, t, w in edges if t == CONSUMER]
    assert callers and all(s == survey.COMPLETE for s, _ in callers), 'the consumer is called by %s' % sorted(
        {s for s, _ in callers})
    c = r['complete']
    sites = [(a, op, sp, chk) for a, op, sp, targets, chk in c['calls'] if CONSUMER in targets]
    for a, op, sp, targets, chk in c['calls']:
        if CONSUMER in targets:
            assert targets == frozenset([CONSUMER]), 'the call at %#x may go to %s' % (a, sorted(targets))
    assert len(sites) == len(callers), 'the completion calls the consumer %d times linked, %d in the graph' % (
        len(sites), len(callers))
    assert all(chk for _, _, _, chk in sites), 'a call of the consumer not after stack_valid on every path'
    loaded = len(c['loads'].get('_' + CONSUMER, ()))
    assert loaded == len(sites), 'the completion loads the consumer\'s address %d times for %d calls' % (loaded, len(sites))
    surveys = sorted(a for a, _, _, targets, _ in c['calls'] if targets == frozenset([survey.HEARTBEAT]))
    if r['survey']:
        assert len(surveys) == len(sites), 'the consumer is called %d times, the survey %d' % (len(sites), len(surveys))
        for (a, _, _, _), s in zip(sorted(sites), surveys):
            assert s < a, 'the consumer\'s call at %#x is not behind the survey\'s at %#x' % (a, s)
    else:
        assert not surveys, 'a worker without the survey calls it'
    start, end = r['complete_range']
    assert r['words'] and all(start <= w < end for w in r['words']), \
        'the consumer\'s address is in the linked image outside the completion\'s literals'
    valid = [sp for _, _, sp, targets, _ in c['calls'] if targets == frozenset(['stack_valid'])]
    assert valid, 'the completion never calls stack_valid'
    v = max(valid)
    room, guard = sources['room'], sources['guard']
    worst = None
    for a, op, sp, _ in sorted(sites):
        spare = room - v + sp - deepest - guard
        lines.append('  call at %#x (stack offset %d): lowest point %d above the low end, %d clear of the guard' % (
            a, sp, room - v + sp - deepest, spare))
        worst = spare if worst is None else min(worst, spare)
    usable = worst + deepest
    assert usable >= MARGIN * deepest, 'on the least stack admitted, %d bytes for a consumer of %d (needs %.1f x)' % (
        usable, deepest, MARGIN)
    below = memo[HEARTBEAT][0]
    return lines, {'depth': deepest, 'worst': worst, 'usable': usable, 'sites': len(sites), 'gate': r['gate'],
                   'survey': r['survey'],
                   'take': take, 'below': below, 'left': int(usable / MARGIN) - below, 'room': room, 'guard': guard}


def self_test(r, sources):
    cases = []

    def case(name, change):
        altered, s = copy.deepcopy(r), dict(sources)
        change(altered, s)
        try:
            verify(altered, s)
        except AssertionError as e:
            cases.append((name, 'refused', str(e).splitlines()[0][:90]))
            return
        cases.append((name, 'ACCEPTED', ''))

    def frame(title, size, kind='static', linked=True):
        def change(a, s):
            a['graph']['nodes'][title] = (size, kind)
            if linked:
                a['frames'][title] = size
        return change
    case('a larger heartbeat frame, linked and compiled', frame(HEARTBEAT, 120))
    case('a frame the linked code does not have', frame(HEARTBEAT, 60, linked=False))
    case('a frame of no fixed size', frame(CONSUMER, 44, 'dynamic'))

    def deeper_take(a, s):
        for w in a['handed_frames'].values():
            w['frame'] = 100
    case('a take that goes deep', deeper_take)

    def calling_take(a, s):
        for w in a['handed_frames'].values():
            w['calls'] = [(0, 'jsr', 0, frozenset(['memcpy']), True)]
    case('a take that calls on', calling_take)
    case('a gate that saves more', lambda a, s: a.__setitem__('gate', 96))
    case('another caller', lambda a, s: a['graph']['edges'].append(('reader_cooperative_service', CONSUMER, 'x')))
    case('a call through a pointer elsewhere', lambda a, s: a['graph']['edges'].append((CONSUMER, INDIRECT, 'x')))
    case('a call before stack_valid', lambda a, s: a['complete'].__setitem__(
        'calls', [(x, op, sp, tg, False if CONSUMER in tg else chk) for x, op, sp, tg, chk in a['complete']['calls']]))
    if r['survey']:
        case('a call ahead of the survey', lambda a, s: a['complete'].__setitem__(
            'calls', [(x - 0x10000 if CONSUMER in tg else x, op, sp, tg, chk)
                      for x, op, sp, tg, chk in a['complete']['calls']]))
    case('the consumer\'s address in a table', lambda a, s: a['words'].append(0x8c000000))
    case('less room in stack_valid', lambda a, s: s.__setitem__('room', 200))
    case('recursion', lambda a, s: a['graph']['edges'].append((HEARTBEAT, CONSUMER, 'x')))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('worker_elf')
    parser.add_argument('worker_build')
    parser.add_argument('aux_elf')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    passive = (ROOT / survey.PASSIVE_SOURCE).read_text()
    sources = survey.source_facts((ROOT / survey.SURVEY_SOURCE).read_text(), passive)
    r = readings(args.worker_elf, args.worker_build, args.aux_elf)
    lines, facts = verify(r, sources)
    if args.self_test:
        cases = self_test(r, sources)
        for name, result, why in cases:
            print('  %-8s %s%s' % (result, name, ': ' + why if why else ''))
        bad = [name for name, result, _ in cases if result != 'refused']
        if bad:
            sys.exit('FAIL altered readings accepted: ' + ', '.join(bad))
        print('PASS Test 85 consumer linked, self-test: all %d altered readings refused' % len(cases))
        return
    print('\n'.join(lines))
    print('PASS Test 85 consumer linked: %d bytes deep on the worker\'s stack, fixed frames throughout, the gate\'s '
          '%d bytes and a take of %d among them; called from the completion only (%d sites, each %safter '
          'stack_valid); on the least stack stack_valid admits (%d bytes above the low end, %d of '
          'guard) it stays %d bytes clear of the guard words (%d usable, %.2f x). Below a consumer\'s own frame the '
          'card\'s heartbeat takes %d bytes: by the same rule a consumer\'s frame may be %d' % (
              facts['depth'], facts['gate'], facts['take'], facts['sites'],
              'behind the survey and ' if facts['survey'] else '', facts['room'], facts['guard'],
              facts['worst'], facts['usable'], facts['usable'] / facts['depth'], facts['below'], facts['left']))


if __name__ == '__main__':
    main()
