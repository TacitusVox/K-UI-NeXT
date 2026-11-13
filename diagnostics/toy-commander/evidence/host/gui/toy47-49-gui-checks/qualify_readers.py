#!/usr/bin/env python3
import concurrent.futures
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys
from zipfile import ZipFile

root = Path.cwd()
checks = root / "build/toy47-49-gui-checks"
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
    assert len(sys.argv) == 4
    original = root.parent / "toy34-inputs/K-UI/ToyCommander-1GUTH.BIN"
    args = [original]
    identities = []
    for mode, name in enumerate(sys.argv[1:], start=47):
        module = Path(name).resolve(strict=True)
        assert module.name == f"retail-toy-test-{mode}.kui"
        identity = verify(module)
        identities.append(identity)
        args.extend((module, identity["build"], identity["crc32"]))
    assert len({identity["crc32"] for identity in identities}) == 3
    baseline, = (root.parent / "toy-test47-48-package").glob("*.zip")
    with ZipFile(baseline) as published:
        for mode, name in enumerate(sys.argv[1:3], start=47):
            exact = Path(name).read_bytes()
            assert exact == published.read(f"KUI/apps/games/retail-toy-test-{mode}.kui")
            print(f"Published Test {mode} preserved byte-for-byte: {sha256(exact).hexdigest()}")
    args.extend(root.parent / f"upload/{mode}.txt" for mode in (47,48))
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
