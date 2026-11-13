#!/usr/bin/env python3
import concurrent.futures
import json
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path.cwd()
checks = root / "build/continuous-pio-gui-checks"
base = root / "build/dma-priority-gui-checks"
module = pathlib.Path(sys.argv[1]).resolve(strict=True)
image = json.loads(subprocess.check_output(
    [sys.executable, "tools/runtime_package.py", "verify", str(module)], text=True
))
original = root.parent / "toy34-inputs/K-UI/ToyCommander-1GUTH.BIN"
prior_module = root.parent / "trace-reader/build/toy-slow-retry/retail-toy-test-37.kui"
prior_report = root.parent / "upload/37.txt"
last_module = root.parent / "trace-reader/build/toy-refill-slice/retail-toy-test-38.kui"
last_report = root.parent / "upload/38.txt"
exit_module = root.parent / "trace-reader/build/toy-movie-exit/retail-toy-test-39.kui"
exit_report = root.parent / "upload/39.txt"
window_module = root.parent / "trace-reader/build/toy-recovery-window/retail-toy-test-40.kui"
window_report = root.parent / "upload/40.txt"
pipelined_module = root.parent / "trace-reader/build/toy-pipelined-recovery-final/retail-toy-test-41.kui"
pipelined_report = root.parent / "upload/41.txt"
priority_module = root.parent / "trace-reader/build/toy-graphics-priority-final/retail-toy-test-42.kui"
priority_report = root.parent / "upload/42.txt"

def run(fs):
    disk = checks / f"route-{fs}.img"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", base / f"route-{fs}.img", disk], check=True)
    log = checks / f"route-{fs}.log"
    with log.open("w") as out:
        subprocess.run(
            [checks / "route", disk, original, module, image["build"], image["crc32"], prior_module, prior_report, last_module, last_report, exit_module, exit_report, window_module, window_report, pipelined_module, pipelined_report, priority_module, priority_report],
            stdout=out, stderr=subprocess.STDOUT, check=True,
        )
    return fs, [line for line in log.read_text().splitlines() if line.startswith(("Verified mode", "Exact reader"))]

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
    for fs, results in workers.map(run, ("fat32", "exfat-128k")):
        print(fs)
        print("\n".join(results))
