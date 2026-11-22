#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One K-UI for every thread: check a K-UI build's source against every active line.

Several threads change K-UI at the same time (docs/gui-lines.json names them:
Main and each thread's branch). A K-UI built from one line alone drops the
other lines' K-UI work, and the owner's next test of that work fails for a
reason no test of it can show (Test 84, 21 November 2026: the Windows CE
line's K-UI 1c71ba3afa17 had no 160-slot map, so the CD music line's module
was refused for Bangai-O, MDK2 and Plasma Sword).

    python3 tools/check_gui_lines.py [--commit REV] [--no-fetch]

fetches each active line (unless --no-fetch) and checks that REV (default
HEAD) contains its head. A line whose missing commits touch only docs/ is a
warning; any other missing commit fails the check, with the branch to merge.
Run it from the clean checkout a K-UI is built from, before building it.

    python3 tools/check_gui_lines.py --installed-has REV

checks that the K-UI the owner runs (docs/gui-lines.json, "installed")
contains REV: for a module-only delivery, REV is the commit of the K-UI
change the module needs (for example the 160-slot map). If it does not, the
delivery must bring a K-UI that has it, or the module is refused.

    python3 tools/check_gui_lines.py --installed

prints the installed K-UI the ledger records.

Exit status: 0 when every check passes (warnings allowed), 1 otherwise.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEDGER = 'docs/gui-lines.json'


def git(repo, *args, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], capture_output=True, text=True)
    if check and result.returncode:
        raise RuntimeError('git %s: %s' % (' '.join(args), result.stderr.strip()))
    return result


def is_ancestor(repo, older, newer):
    return git(repo, 'merge-base', '--is-ancestor', older, newer, check=False).returncode == 0


def load_ledger(repo):
    data = json.loads((Path(repo) / LEDGER).read_text())
    if data.get('schema') != 'kui.gui-lines.v1' or not data.get('lines'):
        raise ValueError(LEDGER + ' is not a kui.gui-lines.v1 ledger with lines')
    for line in data['lines']:
        if not isinstance(line.get('branch'), str) or not line['branch']:
            raise ValueError(LEDGER + ': a line has no branch')
    return data


def missing_commits(repo, commit, head):
    """Commits of head that commit lacks, each with whether it touches only docs/."""
    out = git(repo, 'log', '--format=%x00%H %s', '--name-only', '-m', '%s..%s' % (commit, head)).stdout
    rows, seen = [], set()
    for block in out.split('\x00')[1:]:
        lines = [l for l in block.splitlines() if l.strip()]
        sha, _, subject = lines[0].partition(' ')
        paths = lines[1:]
        if sha in seen:
            # A merge is listed once per parent; the paths of every listing count.
            rows[[r[0] for r in rows].index(sha)][2].extend(paths)
            continue
        seen.add(sha)
        rows.append([sha, subject, list(paths)])
    return [(sha, subject, bool(paths) and all(p.startswith('docs/') for p in paths)) for sha, subject, paths in rows]


def check(repo, commit, fetch):
    ledger = load_ledger(repo)
    target = git(repo, 'rev-parse', '--verify', commit + '^{commit}').stdout.strip()
    failures, warnings = [], []
    for line in ledger['lines']:
        branch = line['branch']
        if fetch:
            got = git(repo, 'fetch', '-q', 'origin', '+refs/heads/%s:refs/remotes/origin/%s' % (branch, branch),
                      check=False)
            if got.returncode:
                failures.append('%s: could not fetch (%s)' % (branch, got.stderr.strip()))
                continue
        ref = 'refs/remotes/origin/' + branch
        head = git(repo, 'rev-parse', '--verify', '-q', ref + '^{commit}', check=False).stdout.strip()
        if not head:
            failures.append('%s: no %s here; fetch it' % (branch, ref))
            continue
        if is_ancestor(repo, head, target):
            print('ok    %-36s %s contained' % (branch, head[:12]))
            continue
        rows = missing_commits(repo, target, head)
        code = [r for r in rows if not r[2]]
        docs = [r for r in rows if r[2]]
        for sha, subject, _ in code:
            print('MISS  %-36s %s %s' % (branch, sha[:12], subject[:80]))
        for sha, subject, _ in docs:
            print('docs  %-36s %s %s' % (branch, sha[:12], subject[:80]))
        if code:
            failures.append('%s: %d commit(s) with code this build lacks; merge origin/%s first (%s)' % (
                branch, len(code), branch, line.get('thread', '')))
        else:
            warnings.append('%s: only %d docs commit(s) missing' % (branch, len(docs)))
    for w in warnings:
        print('WARN  ' + w)
    if failures:
        for f in failures:
            print('FAIL  ' + f)
        print('FAIL K-UI build %s lacks another line\'s work: merge it, rebuild each line\'s delivered modules '
              'from the merge and compare their bytes, then build K-UI' % target[:12])
        return 1
    print('PASS K-UI build %s contains every active line of %s' % (target[:12], LEDGER))
    return 0


def installed_has(repo, commit):
    installed = load_ledger(repo).get('installed') or {}
    source = installed.get('source')
    if not source:
        print('FAIL %s records no installed K-UI source' % LEDGER)
        return 1
    want = git(repo, 'rev-parse', '--verify', commit + '^{commit}').stdout.strip()
    if git(repo, 'cat-file', '-e', source + '^{commit}', check=False).returncode:
        print('FAIL the installed K-UI\'s source %s is not in this repository; fetch its line' % source[:12])
        return 1
    if is_ancestor(repo, want, source):
        print('PASS the installed K-UI %s contains %s' % (installed.get('build', source[:12]), want[:12]))
        return 0
    print('FAIL the installed K-UI %s lacks %s: deliver a K-UI that has it, or the module that needs it is '
          'refused' % (installed.get('build', source[:12]), want[:12]))
    return 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--repo', default=str(ROOT))
    parser.add_argument('--commit', default='HEAD')
    parser.add_argument('--no-fetch', action='store_true')
    parser.add_argument('--installed-has', metavar='REV')
    parser.add_argument('--installed', action='store_true')
    args = parser.parse_args(argv)
    if args.installed:
        print(json.dumps(load_ledger(args.repo).get('installed'), indent=2))
        return 0
    if args.installed_has:
        return installed_has(args.repo, args.installed_has)
    return check(args.repo, args.commit, not args.no_fetch)


if __name__ == '__main__':
    sys.exit(main())
