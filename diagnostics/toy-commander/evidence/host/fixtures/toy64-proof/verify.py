"""Qualify the linked Toy Commander frame-timing reader and its report."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
import json
import os
import sys
from pathlib import Path
from subprocess import run

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
READER = ROOT / 'trace-reader'
BASE62 = READER / 'build/toy-movie-ahead62-final'
sys.path.insert(0, str(READER / 'tools'))
from runtime_package import verify
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
    test_src = (HERE / 'report-host.c').read_text()
    test_src = test_src.replace('@MODE@', str(mode))
    c = logdir / 'report-host.c'
    c.write_text(test_src)
    exe = logdir / 'report-host'
    with (logdir / 'report.log').open('w') as log:
        cmd = [
            'cc', '-std=c11', '-O1', '-g', '-fno-pie', '-no-pie',
            '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
            '-DKUI_NATIVE_BACKGROUND_COMPACT=1', '-DKUI_NATIVE_GENERAL_LEAN_LAYOUT=1',
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


def provenance(build, mode, final, logdir):
    info = json.loads((build / 'entry.compile.json').read_text())
    package = verify((build / f'retail-toy-test-{mode}.kui').read_bytes())
    assert len(package['build']) == 12
    assert info['elf_sha256'] == sha256((build / 'entry.elf').read_bytes()).hexdigest()
    if final:
        assert not info['source_dirty']
        assert info['commit'].startswith(package['build'])
        worker = (build / 'native-worker.bin').read_bytes()
        assert b'AHEAD B I HIT EDC STOP\x00' in worker
        assert b'I DEFER WHY FIRST END\x00' in worker
        assert b'OPEN B I KICK WAIT FAIL\x00' in worker
        assert b'B I SECT FAIL CRC\x00' not in worker
    baseline = verify((BASE62 / 'retail-toy-test-62.kui').read_bytes())
    assert (baseline['build'], baseline['crc32']) == ('7beafbe8da44', 'ad53b6e7')
    baseline_id = baseline['build'].encode()
    build_id = package['build'].encode()
    for file in ('resident-sci.bin', 'resident-legacy.bin'):
        expected = (BASE62 / file).read_bytes()
        actual = (build / file).read_bytes()
        assert expected.count(baseline_id) == 1
        assert actual.count(build_id) == 1
        assert actual.replace(build_id, baseline_id) == expected, file
    (logdir / 'provenance.log').write_text(
        f"Test {mode}: {package['build']}, CRC {package['crc32']}; "
        f"provenance {info['commit']}, dirty={info['source_dirty']}; "
        "Both low residents exact to Test 62 after build-label normalization.\n"
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', type=int, choices=(64,))
    ap.add_argument('build', type=Path)
    ap.add_argument('--final', action='store_true')
    ap.add_argument('--present-replay', type=Path,
                    default=ROOT / 'toy62-present-replay.py')
    args = ap.parse_args()
    mode, build = args.mode, args.build.resolve(strict=True)
    reader = build.parent.parent
    assert (reader / 'src/loader/vmu_native_passive.c').is_file()
    logdir = HERE / build.name
    logdir.mkdir(exist_ok=True)
    provenance(build, mode, args.final, logdir)
    aux, low, stage = [str(build / f'{stem}.elf') for stem in
                       ('native-ra-aux', 'resident-sci', 'stage')]
    commands = {
        'stack': (reader, [ROOT / 'toy64-linked-stack.py', build]),
        'startup': (ROOT, [ROOT / 'toy44-startup-linked-proof.py', build, mode]),
        'lease': (ROOT, [ROOT / 'toy64-lease-cold-matrix.py', INPUT, build]),
        'original-gd': (ROOT, [READER / 'tests/test_toy_original_gd_poll.py', INPUT,
                               '--resident', low, '--stage', stage]),
        'original-kick': (ROOT, [ROOT / 'toy64-original-kick-replay.py', INPUT, aux]),
        'original-chunk': (ROOT, [ROOT / 'toy62-refill-replay.py', INPUT, aux]),
        'original-output': (ROOT, [ROOT / 'toy62-output-replay.py', INPUT, aux]),
        'original-present': (ROOT, [args.present_replay.resolve(), INPUT, aux]),
        'linked-resume': (ROOT, [ROOT / 'toy51-linked-resume.py', low]),
        'linked-crc16': (ROOT, [ROOT / 'toy53-linked-crc16.py',
                               READER / 'build/toy-wake-resume-final/resident-sci.elf', low]),
    }
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
