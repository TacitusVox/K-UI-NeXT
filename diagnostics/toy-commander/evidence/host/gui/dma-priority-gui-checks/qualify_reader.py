#!/usr/bin/env python3
import concurrent.futures
import json
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path.cwd()
checks = root / "build/dma-priority-gui-checks"
base = root / "build/ram-window-gui-checks"
module = pathlib.Path(sys.argv[1]).resolve(strict=True)
image = json.loads(subprocess.check_output(
    [sys.executable, "tools/runtime_package.py", "verify", str(module)], text=True
))
original = root.parent / "toy34-inputs/K-UI/ToyCommander-1GUTH.BIN"

def run(fs):
    disk = checks / f"route-{fs}.img"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", base / f"route-{fs}.img", disk], check=True)
    log = checks / f"route-{fs}.log"
    with log.open("w") as out:
        subprocess.run(
            [checks / "route", disk, original, module, image["build"], image["crc32"]],
            stdout=out, stderr=subprocess.STDOUT, check=True,
        )
    return fs, [line for line in log.read_text().splitlines() if line.startswith(("Verified mode", "Exact readers"))]

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
    for fs, results in workers.map(run, ("fat32", "exfat-128k")):
        print(fs)
        print("\n".join(results))
