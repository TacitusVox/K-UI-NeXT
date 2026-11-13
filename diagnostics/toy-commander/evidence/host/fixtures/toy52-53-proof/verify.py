"""Run the existing linked Toy probes plus report-format bounds against one build."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
from subprocess import run

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
READER = ROOT / 'trace-reader'
INPUT = ROOT / 'toy34-inputs' / 'K-UI' / 'ToyCommander-1GUTH.BIN'
TOOLCHAIN = ROOT / 'toy34-inputs' / '.deps' / 'sh-elf' / 'bin'
ENV = os.environ | {'PATH': f'{TOOLCHAIN}:{os.environ["PATH"]}'}


def report(reader, mode, logdir):
    source = (reader / 'src/loader/vmu_native_passive.c').read_text()
    start = source.index('static void toy_playback_page(')
    end = source.index(
        '#if !defined(KUI_VMU_NATIVE_PASSIVE_HOST_TEST)\nstatic void toy_report_pause(',
        start,
    )
    (logdir / 'screen-extract.inc').write_text(source[start:end] + '\n#endif\n')
    test_src = (ROOT / 'toy51-telemetry/report-host-51.c').read_text()
    for old, new in (
        ('MODE 51', f'MODE {mode}'),
        ('TEST 51', f'TEST {mode}'),
        (',51u);', f',{mode}u);'),
        ('Mode 51 report:', f'Mode {mode} report:'),
    ):
        assert old in test_src, old
        test_src = test_src.replace(old, new)
    c = logdir / 'report-host.c'
    c.write_text(test_src)
    exe = logdir / 'report-host'
    with (logdir / 'report.log').open('w') as log:
        cmd = [
            'cc', '-std=c11', '-O1', '-g', '-fno-pie', '-no-pie',
            '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
            '-Wall', '-Wextra', '-Werror', f'-I{reader / "include"}',
            f'-I{reader / "src/loader"}', str(c), str(reader / 'src/core/hash.c'),
            str(reader / 'src/core/data.c'), '-Wl,--gc-sections', '-o', str(exe),
        ]
        result = run(cmd, stdout=log, stderr=log, env=ENV)
        if result.returncode != 0:
            return result.returncode
        return run([str(exe), str(logdir / f'report-{mode}.txt')], stdout=log,
                   stderr=log, env=ENV | {'ASAN_OPTIONS': 'detect_leaks=0'}).returncode


def probe(cmd, cwd, log):
    with log.open('w') as out:
        return run(cmd, cwd=cwd, stdout=out, stderr=out, env=ENV).returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', type=int, choices=(51, 52, 53, 54))
    ap.add_argument('build', type=Path)
    args = ap.parse_args()
    mode, build = args.mode, args.build.resolve(strict=True)
    reader = build.parent.parent
    assert (reader / 'src/loader/vmu_native_passive.c').is_file()
    logdir = HERE / build.name
    logdir.mkdir(exist_ok=True)
    aux, low, stage = [str(build / f'{stem}.elf') for stem in
                       ('native-ra-aux', 'resident-sci', 'stage')]
    commands = {
        'stack': (reader, [ROOT / 'toy48-linked-stack.py', build]),
        'startup': (ROOT, [ROOT / 'toy44-startup-linked-proof.py', build, mode]),
        'lease': (ROOT, [ROOT / 'toy52-53-lease-cold-matrix.py', INPUT, build]),
        'original-gd': (ROOT, [READER / 'tests/test_toy_original_gd_poll.py', INPUT,
                               '--resident', low, '--stage', stage]),
        'original-chunk': (ROOT, [ROOT / 'toy51-linked-low-chunk-replay.py', INPUT, aux]),
        'original-output': (ROOT, [ROOT / 'toy49-output-proto/prove-output.py', INPUT, aux]),
        'linked-resume': (ROOT, [ROOT / 'toy51-linked-resume.py', low]),
    }
    if mode == 52:
        baseline = READER / 'build/toy-wake-resume-final/native-ra-aux.elf'
        commands['linked-edc'] = (ROOT, [HERE / 'edc52.py', baseline, aux])
    if mode == 54:
        commands['opening-buffer'] = (ROOT, [ROOT / 'toy54-opening-replay.py', INPUT, aux])
    with ThreadPoolExecutor(max_workers=5) as pool:
        checks = {
            name: pool.submit(
                probe, ['python3', *map(str, command)], cwd, logdir / f'{name}.log',
            )
            for name, (cwd, command) in commands.items()
        }
        checks['report'] = pool.submit(report, reader, mode, logdir)
        failed = []
        for name, job in checks.items():
            status = job.result()
            if status:
                failed.append(name)
            print(f'Test {mode} {name}: {"PASS" if not status else "FAIL"} '
                  f'({logdir / f"{name}.log"})', flush=True)
    return int(bool(failed))


if __name__ == '__main__':
    raise SystemExit(main())
