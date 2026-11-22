#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/check_gui_lines.py on a small repository of its own: a build that has
every line passes; a line's docs-only commits warn; a line's code commit, a
merge that brings code, and a line that cannot be found fail; the installed
K-UI check answers for an ancestor, for the commits of a source recorded as
an equivalent K-UI (with evidence), and refuses a commit it lacks."""
import contextlib
import io
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_gui_lines  # noqa: E402


def sh(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True, capture_output=True, text=True).stdout.strip()


def commit(repo, path, text, message):
    target = Path(repo) / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text)
    sh(repo, 'add', path)
    sh(repo, '-c', 'user.name=t', '-c', 'user.email=t@localhost', 'commit', '-q', '-m', message)
    return sh(repo, 'rev-parse', 'HEAD')


def run(repo, *argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = check_gui_lines.main(['--repo', str(repo), '--no-fetch', *argv])
    return code, out.getvalue()


def main():
    with tempfile.TemporaryDirectory(prefix='gui-lines-') as tmp:
        repo = Path(tmp)
        sh(repo, 'init', '-q', '-b', 'main')
        base = commit(repo, 'src/apps/a.c', 'int a;\n', 'base')
        ledger = {'schema': 'kui.gui-lines.v1',
                  'lines': [{'branch': 'main', 'thread': 'Main'}, {'branch': 'cd', 'thread': 'CD music'},
                            {'branch': 'ce', 'thread': 'Windows CE'}],
                  'installed': {'build': 'feedfacecafe', 'source': base}}
        commit(repo, 'docs/gui-lines.json', json.dumps(ledger), 'ledger')
        sh(repo, 'branch', 'cd'); sh(repo, 'branch', 'ce')
        sh(repo, 'checkout', '-q', 'ce')
        ce_gui = commit(repo, 'src/apps/ce.c', 'int ce;\n', 'CE: a K-UI route')
        sh(repo, 'checkout', '-q', 'cd')
        cd_gui = commit(repo, 'src/apps/cd.c', 'int cd;\n', 'CD: the 160-slot map')
        for branch in ('main', 'cd', 'ce'):
            sh(repo, 'update-ref', 'refs/remotes/origin/' + branch, sh(repo, 'rev-parse', branch))

        # The CD line alone lacks the CE line's K-UI code: refused, naming the branch.
        code, out = run(repo, '--commit', 'cd')
        assert code == 1 and 'MISS  ce' in out and 'merge origin/ce first' in out, out
        # Merged, it passes.
        sh(repo, '-c', 'user.name=t', '-c', 'user.email=t@localhost', 'merge', '-q', '--no-edit', 'ce')
        merged = sh(repo, 'rev-parse', 'HEAD')
        code, out = run(repo, '--commit', merged)
        assert code == 0 and 'PASS' in out, out
        # The CE line then adds only a docs commit: a warning, still passing.
        sh(repo, 'checkout', '-q', 'ce')
        commit(repo, 'docs/ce.md', 'notes\n', 'Docs: CE result')
        sh(repo, 'update-ref', 'refs/remotes/origin/ce', sh(repo, 'rev-parse', 'ce'))
        code, out = run(repo, '--commit', merged)
        assert code == 0 and 'docs  ce' in out and 'WARN' in out, out
        # A code commit in a merge the CE line makes counts as code.
        sh(repo, 'checkout', '-q', '-b', 'side', 'ce')
        commit(repo, 'src/core/x.c', 'int x;\n', 'CE: shared code')
        sh(repo, 'checkout', '-q', 'ce')
        sh(repo, '-c', 'user.name=t', '-c', 'user.email=t@localhost', 'merge', '-q', '--no-ff', '--no-edit', 'side')
        sh(repo, 'update-ref', 'refs/remotes/origin/ce', sh(repo, 'rev-parse', 'ce'))
        code, out = run(repo, '--commit', merged)
        assert code == 1 and 'CE: shared code' in out, out
        # A line that is not here fails.
        ledger['lines'].append({'branch': 'gone', 'thread': 'x'})
        sh(repo, 'checkout', '-q', 'cd')
        (repo / 'docs/gui-lines.json').write_text(json.dumps(ledger))
        code, out = run(repo, '--commit', merged)
        assert code == 1 and 'gone: no refs/remotes/origin/gone' in out, out
        # The installed K-UI: its own source and ancestors pass; a later commit fails.
        code, out = run(repo, '--installed-has', base)
        assert code == 0 and 'PASS' in out, out
        code, out = run(repo, '--installed-has', cd_gui)
        assert code == 1 and 'lacks' in out, out
        ledger['installed']['source'] = merged
        (repo / 'docs/gui-lines.json').write_text(json.dumps(ledger))
        code, out = run(repo, '--installed-has', cd_gui)
        assert code == 0, out
        code, out = run(repo, '--installed-has', ce_gui)
        assert code == 0, out
        # A later line commit: refused until a source containing it is recorded as an
        # equivalent K-UI, with its evidence; without evidence the record is refused.
        sh(repo, 'checkout', '-q', 'ce')
        later = commit(repo, 'src/loader/reader.c', 'int reader;\n', 'CE: a module-only change')
        sh(repo, 'checkout', '-q', '-b', 'merged2', merged)
        sh(repo, '-c', 'user.name=t', '-c', 'user.email=t@localhost', 'merge', '-q', '--no-edit', 'ce')
        merged2 = sh(repo, 'rev-parse', 'HEAD')
        code, out = run(repo, '--installed-has', later)
        assert code == 1 and 'lacks' in out, out
        ledger['installed']['equivalent'] = [{'source': merged2}]
        (repo / 'docs/gui-lines.json').write_text(json.dumps(ledger))
        code, out = run(repo, '--installed-has', later)
        assert code == 1 and 'needs its commit and its evidence' in out, out
        ledger['installed']['equivalent'] = [{'source': merged2, 'evidence': 'docs/x.txt'}]
        (repo / 'docs/gui-lines.json').write_text(json.dumps(ledger))
        code, out = run(repo, '--installed-has', later)
        assert code == 0 and 'equals the one built from' in out, out
        code, out = run(repo, '--installed-has', commit(repo, 'src/apps/z.c', 'int z;\n', 'not merged'))
        assert code == 1, out
    print('PASS check_gui_lines: a build lacking a line\'s code is refused and names the branch; merged it passes; '
          'docs-only commits warn; code inside a merge counts; a missing line fails; the installed K-UI check '
          'answers for its ancestors and a proven-equivalent source\'s, and refuses what it lacks')


if __name__ == '__main__':
    main()
