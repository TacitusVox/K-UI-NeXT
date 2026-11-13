#!/usr/bin/env python3
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
checks = root / "build/dma-bus-share-gui-checks"
base = root / "build/dma-priority-gui-checks"
reader_root = root.parent / "trace-reader/build"
previous = {
    37: "toy-slow-retry",
    38: "toy-refill-slice",
    39: "toy-movie-exit",
    40: "toy-recovery-window",
    41: "toy-pipelined-recovery-final",
    42: "toy-graphics-priority-final",
    43: "toy-continuous-pio-final",
    44: "toy-sequential-pio-final",
    45: "toy-bounded-dma-final",
    46: "toy-first-cut-pio-final",
}


def verify(module):
    return json.loads(subprocess.check_output(
        [sys.executable, "tools/runtime_package.py", "verify", str(module)], text=True
    ))


def run(fs, options):
    disk = checks / f"route-{fs}.img"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", base / f"route-{fs}.img", disk], check=True)
    log = checks / f"route-{fs}.log"
    with log.open("w") as out:
        subprocess.run([checks / "route", disk, *options], stdout=out, stderr=subprocess.STDOUT, check=True)
    return fs, [line for line in log.read_text().splitlines() if line.startswith(("Verified mode", "Exact readers"))]


def main():
    assert len(sys.argv) == 2
    original = root.parent / "toy34-inputs/K-UI/ToyCommander-1GUTH.BIN"
    module = Path(sys.argv[1]).resolve(strict=True)
    assert module.name == "retail-toy-test-47.kui"
    identity = verify(module)
    args = [original, module, identity["build"], identity["crc32"]]
    placeholder43 = checks / "report-43-placeholder.txt"
    placeholder43.write_bytes(b"".join(
        (f"K-UI Toy Commander test 43, run 1, reserved block {block + 1}/2\n"
         "Report pending. Hold both triggers and A+B+X+Y in the game to save.\n").encode().ljust(511, b" ") + b"\n"
        for block in range(2)
    ))
    for mode, directory in previous.items():
        report = root.parent / f"upload/{mode}.txt" if mode != 43 else placeholder43
        args.extend((reader_root / directory / f"retail-toy-test-{mode}.kui", report))
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
        for fs, results in workers.map(lambda fs: run(fs, args), ("fat32", "exfat-128k")):
            print(fs)
            print("\n".join(results))


if __name__ == "__main__":
    main()
