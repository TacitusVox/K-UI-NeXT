#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Production text of the shared reader sources with the Toy test switches off.

The Toy Commander test modules (Tests 26 to 82) share reader sources with the
installed loaders. With every test switch off (tests/toy_test_switches.py),
each shared source must still give the production text recorded here:

- exact: the complete text, for the sources the pinned SCI history tests
  compare in full;
- code: the tokens a compiler sees, ignoring comments, layout and equivalent
  #ifdef spellings, for the other shared sources.

A change of test-module code under a test switch or KUI_TOY_TEST_BUILD keeps
these fingerprints. A failure means production text changed: either put the
change under a test switch, or, for a reviewed production change, record the
new fingerprint with --write and say so in the commit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from toy_test_switches import code_tokens, resolve_toy_switches

FIXTURE = ROOT / 'tests/fixtures/toy-switch-production-text.json'


def fingerprint(path, mode):
    text = resolve_toy_switches((ROOT / path).read_text())
    if mode == 'exact':
        return hashlib.sha256(text.encode()).hexdigest()
    return hashlib.sha256('\n'.join(code_tokens(text)).encode()).hexdigest()


def resolver_cases():
    cases = [
        ('#if KUI_TOY_SUITE_MODE == 31\nA\n#endif\nB\n', 'B\n'),
        ('#if X && !KUI_TOY_SUITE_ASYNC_DEMAND\nA\n#endif\n', '#if X\nA\n#endif\n'),
        ('#if KUI_TOY_WIRE_STREAM\nA\n#else\nB\n#endif\n', 'B\n'),
        ('#if KUI_TOY_WIRE_STREAM\nA\n#elif defined(KUI_RETAIL_CE)\nB\n#else\nC\n#endif\n',
         '#ifdef KUI_RETAIL_CE\nB\n#else\nC\n#endif\n'),
        ('#if X\nA\n#elif KUI_TOY_WIRE_STREAM\nB\n#else\nC\n#endif\n', '#if X\nA\n#else\nC\n#endif\n'),
        ('#if X\nA\n#elif !KUI_TOY_WIRE_STREAM\nB\n#else\nC\n#endif\n', '#if X\nA\n#else\nB\n#endif\n'),
        ('#ifndef KUI_TOY_WIRE_SEQ\n#define KUI_TOY_WIRE_SEQ 0\n#endif\n#if defined(KUI_TOY_WIRE_SEQ)\nA\n#endif\n',
         'A\n'),
        ('#define STRICT (KUI_TOY_SUITE_MODE == 26)\n#if STRICT\nA\n#else\nB\n#endif\n', 'B\n'),
        ('#include "kui/toy_test_build.h"\n#if KUI_TOY_TEST_BUILD\nT\n#else\nP\n#endif\n', 'P\n'),
        ('#if KUI_RETAIL_TOY_PILOT\nA\n#endif\n', '#if KUI_RETAIL_TOY_PILOT\nA\n#endif\n'),
        ('#if KUI_TOY_SUITE_MODE\n#if Y\nA\n#endif\n#endif\nB\n', 'B\n'),
    ]
    for source, expected in cases:
        got = resolve_toy_switches(source)
        if got != expected:
            raise AssertionError('Resolver case %r gave %r, not %r' % (source, got, expected))
    if code_tokens('#ifdef A\nx /* c */ = 1;\n') != code_tokens('#if defined (A)\nx=1;'):
        raise AssertionError('Equivalent code is not recognized as such')
    return len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true',
                        help='record the current fingerprints (only after a reviewed production change)')
    args = parser.parse_args()
    cases = resolver_cases()
    record = json.loads(FIXTURE.read_text())
    if args.write:
        for path, entry in record['files'].items():
            entry['sha256'] = fingerprint(path, entry['mode'])
        FIXTURE.write_text(json.dumps(record, indent=2) + '\n')
        print('Recorded', len(record['files']), 'production fingerprints')
        return
    wrong = [path for path, entry in sorted(record['files'].items())
             if fingerprint(path, entry['mode']) != entry['sha256']]
    if wrong:
        raise AssertionError('Production text changed with the Toy test switches off: ' + ', '.join(wrong))
    print('PASS toy test switches: %d resolver cases; %d shared sources keep their production text'
          % (cases, len(record['files'])))


if __name__ == '__main__':
    main()
