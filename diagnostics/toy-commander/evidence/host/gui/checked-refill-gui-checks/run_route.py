from pathlib import Path
import os
import subprocess
import sys

sys.path.insert(0, 'tools')
from runtime_package import verify

checks = Path(__file__).resolve().parent
previous = Path('/workspace/scratch/bf5994e80308')
previous_reader = previous / 'toy-burst-reader/build'
modules = [
    previous_reader / 'toy-burst-final/retail-toy-test-26.kui',
    previous_reader / 'toy-compare-final/retail-toy-test-27.kui',
    previous_reader / 'toy-recovery-final/retail-toy-test-28.kui',
    Path('../trace-reader/build/trace-sci/retail-toy-test-29.kui'),
    Path(sys.argv[1]),
]
arguments = []
for module in modules:
    info = verify(module.read_bytes())
    arguments += [str(module), info['build'], info['crc32']]
for fs in ('fat32', 'exfat-128k'):
    image = checks / f'route-{fs}.img'
    with image.open('wb') as f:
        f.truncate(1536 * 1024 * 1024)
    subprocess.run([previous / 'crc-work/build/mkfs-fixture', fs, image], check=True)
    with (checks / f'route-{fs}.log').open('w') as log:
        subprocess.run([checks / 'route', image, previous / 'kui-build-inputs/toy-report-route-fixture', *arguments],
                       env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, stdout=log, stderr=log, check=True)
