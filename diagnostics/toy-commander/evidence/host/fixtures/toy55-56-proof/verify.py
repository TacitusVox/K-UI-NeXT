"""Run the existing linked Toy probes plus report-format bounds against one build."""

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
BASE53 = ROOT / 'toy-test52-54-package/reader53/build/toy-fast-crc53-final'
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
    test_src = (ROOT / 'toy51-telemetry/report-host-51.c').read_text()
    for old, new in (
        ('MODE 51', f'MODE {mode}'),
        ('TEST 51', f'TEST {mode}'),
        (',51u);', f',{mode}u);'),
        ('Mode 51 report:', f'Mode {mode} report:'),
    ):
        assert old in test_src, old
        test_src = test_src.replace(old, new)
    if mode == 55:
        for clip, label in enumerate(('BMP', 'INTRO')):
            current = f'{label} WK WGD RSM RET EXT'
            replacement = f'{label} WAIT USED NEED SKIP LAST'
            assert test_src.count(current) == 2
            test_src = test_src.replace(current, replacement)
            start = test_src.index(f'    if(!strcmp(label,"{replacement}"))')
            end = test_src.index('    }', start) + len('    }')
            values = [0, 0xffffffff, 0xffffffff, 0, 0] if clip == 0 else [0x7c, 0x11000, 0x400, 0x95, 0x2fd]
            check = ' && '.join(f'values[{i}]=={value:#x}u' for i, value in enumerate(values))
            test_src = test_src[:start] + (
                f'    if(!strcmp(label,"{replacement}")) {{\n'
                f'        assert(count==5u && {check});\n'
                '        ++checked_window_rows;\n'
                '    }'
            ) + test_src[end:]
            start = test_src.index(f'    assert(strstr(text,"{replacement}')
            end = test_src.index('\n', start)
            hex_values = ' '.join(f'{value:08X}' for value in values)
            report_label = f'{label[0]} WAIT USED NEED SKIP LAST'
            test_src = test_src[:start] + (
                f'    assert(strstr(text,"{report_label}\\n{hex_values}\\n"));'
            ) + test_src[end:]
            anchor = '    char buffer[KUI_TOY_REPORT_FILE_BYTES+2u];'
            for field, value in zip(
                ('wait_frame', 'wait_used', 'wait_missing', 'skip_frame', 'last_out_frame'), values
            ):
                test_src = test_src.replace(
                    anchor, f'    ra.onset[{clip}].{field}={value:#x}u;\n' + anchor, 1
                )
        extra = 'INTRO SKIP USED NEED'
        extra_report = 'I SKIP USED NEED'
        label_anchor = '    retail_display_line(label);'
        test_src = test_src.replace(label_anchor, (
            f'    if(!strcmp(label,"{extra}")) {{\n'
            '        assert(count==2u && values[0]==0x11223344u && values[1]==0x55667788u);\n'
            '        ++checked_window_rows;\n'
            '    }\n'
        ) + label_anchor, 1)
        test_src = test_src.replace(anchor, (
            '    ra.onset[1].skip_used=0x11223344u;\n'
            '    ra.onset[1].skip_missing=0x55667788u;\n'
        ) + anchor, 1)
        # The extra row follows both movie rows and must be included in the bounded body.
        old = 'const char *target=strstr(text,"INTRO EXIT CODE FRAME SAMP LAG LIM'
        start = test_src.index(old)
        end = test_src.index('\n', start)
        test_src = test_src[:start] + (
            'assert(strstr(text,"INTRO EXIT CODE FRAME SAMP LAG LIM\\n000007DC 00000456 00000789 00000000 00000000\\n"));\n    '
            f'const char *target=strstr(text,"{extra_report}\\n11223344 55667788\\n");'
        ) + test_src[end:]
        test_src = test_src.replace('screen bodies 14 and 13 rows','screen bodies 14 and 15 rows')
        test_src = test_src.replace(
            'assert(display_lines==13u && checked_witness_rows==4u && checked_window_rows==2u)',
            'assert(display_lines==15u && checked_witness_rows==4u && checked_window_rows==3u)',
        )
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


def provenance(build, mode, final, logdir):
    info = json.loads((build / 'entry.compile.json').read_text())
    package = verify((build / f'retail-toy-test-{mode}.kui').read_bytes())
    assert len(package['build']) == 12
    assert info['elf_sha256'] == sha256((build / 'entry.elf').read_bytes()).hexdigest()
    if final:
        assert not info['source_dirty']
        assert info['commit'].startswith(package['build'])
    baseline = verify((BASE53 / 'retail-toy-test-53.kui').read_bytes())['build'].encode()
    build_id = package['build'].encode()
    for file in ('resident-sci.bin', 'resident-legacy.bin'):
        expected = (BASE53 / file).read_bytes()
        actual = (build / file).read_bytes()
        assert expected.count(baseline) == 1
        assert actual.count(build_id) == 1
        assert actual.replace(build_id, baseline) == expected, file
    if mode == 55:
        # Only the diagnostic observer in the auxiliary and loader/report metadata change.
        assert (build / 'native-ra-aux.bin').read_bytes() != (BASE53 / 'native-ra-aux.bin').read_bytes()
    (logdir / 'provenance.log').write_text(
        f"Test {mode}: {package['build']}, CRC {package['crc32']}; "
        f"provenance {info['commit']}, dirty={info['source_dirty']}; "
        "SCI and legacy residents byte-exact with Test 53 after build-label normalization.\n"
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', type=int, choices=(53, 55, 56))
    ap.add_argument('build', type=Path)
    ap.add_argument('--final', action='store_true')
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
        'stack': (reader, [ROOT / 'toy48-linked-stack.py', build]),
        'startup': (ROOT, [ROOT / 'toy44-startup-linked-proof.py', build, mode]),
        'lease': (ROOT, [ROOT / 'toy55-56-lease-cold-matrix.py', INPUT, build]),
        'original-gd': (ROOT, [READER / 'tests/test_toy_original_gd_poll.py', INPUT,
                               '--resident', low, '--stage', stage]),
        'original-chunk': (ROOT, [ROOT / 'toy51-linked-low-chunk-replay.py', INPUT, aux]),
        'original-output': (ROOT, [ROOT / 'toy49-output-proto/prove-output.py', INPUT, aux]),
        'linked-resume': (ROOT, [ROOT / 'toy51-linked-resume.py', low]),
        'linked-crc16': (ROOT, [ROOT / 'toy53-linked-crc16.py',
                               READER / 'build/toy-wake-resume-final/resident-sci.elf', low]),
    }
    if mode == 55:
        commands['original-chunk'] = (ROOT, [ROOT / 'toy55-onset-replay.py', INPUT, aux])
    if mode == 56:
        commands['original-output'] = (ROOT, [HERE / 'prove-output56.py', INPUT, aux])
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
