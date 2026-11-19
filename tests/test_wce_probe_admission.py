#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The GUI's production Windows CE admission against an actual ce-boot.kui.

tests/test_games_retail.py runs src/apps/games_retail.c (build/games-retail)
over synthetic CE packages. This runs the same cases with a real module's
exact bytes as 0:/KUI/apps/games/ce-boot.kui, on MBR FAT32 and exFAT, so a
test build such as the WinCE data probe (docs/wince-data-probe.md) is shown
to be admitted, mapped and refused exactly as the GUI treats ce-boot.kui:
the KUIRCE01 layout words, the KUICEM01 96-slot record at payload 0x140 and
the background or Standard reader the launch asks for. No game bytes."""
from pathlib import Path
import argparse
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_games_retail as games  # noqa: E402
from test_images import run  # noqa: E402
from test_loader_probe_images import digest, partition_image, check_fs  # noqa: E402

# Case: the GUI's expected log lines (present, absent).
CASES = {
    "ce-probe": (("Windows CE boot test prepared",), ("background SCI stream",)),
    "ce-probe-cooked": (("Windows CE boot test prepared", "checking exact executable CRC"), ("background SCI stream",)),
    "ce-probe-async": (("Windows CE boot test prepared",
                        "Retail boot reader: background SCI stream (test, Windows CE interrupts)"), ()),
    "ce-probe-record-96": (("Windows CE boot test prepared",
                            "Retail boot reader: background SCI stream (test, Windows CE interrupts)"), ()),
    "ce-probe-native": (("the Windows CE boot test needs a Windows CE image",), ("Windows CE boot test prepared",)),
    "ce-probe-small": (("larger than its 2048-byte prefix",), ("Windows CE boot test prepared",)),
    "ce-probe-scif": (("the Windows CE boot test needs SCI microSD",), ("Windows CE boot test prepared",)),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path, help="the ce-boot.kui file to admit")
    args = parser.parse_args()
    module = args.module.read_bytes()
    for binary in ("mkfs.fat", "mkfs.exfat", "fsck.fat", "fsck.exfat"):
        if not shutil.which(binary):
            raise SystemExit(f"Missing test prerequisite: {binary}")
    if not Path(games.BINARY).exists():
        raise SystemExit(f"Missing {games.BINARY}: run make build/games-retail")
    synthetic = games.synthetic_package

    def real_ce(ce=False, **kwargs):
        # The harness writes its CE package file to ce-boot.kui verbatim.
        return module if ce else synthetic(ce=ce, **kwargs)

    games.synthetic_package = real_ce
    with tempfile.TemporaryDirectory(prefix="kui-wce-admission-") as temp:
        base = Path(temp)
        fixture = base / "fixture"
        for kind in ("fat32", "exfat"):
            volume = base / f"{kind}-volume.img"
            with volume.open("wb") as out:
                out.truncate(96 * 1024 * 1024)
            run("mkfs.fat", "-F", "32", str(volume)) if kind == "fat32" else run("mkfs.exfat", str(volume))
            clean = base / f"{kind}-mbr.img"
            partition_image(volume, clean, kind)
            for case, (present, absent) in CASES.items():
                games.make_retail_fixture(fixture, case, 0x47000)
                assert (fixture / "ce-probe.kui").read_bytes() == module
                image = base / "working.img"
                shutil.copyfile(clean, image)
                run(games.BINARY, str(image), str(fixture), "seed", case)
                before = digest(image)
                output = run(games.BINARY, str(image), str(fixture), "check", case)
                assert f"PASS retail preparation check {case}; no active-operation writes" in output, case
                assert digest(image) == before, f"{case} changed the card"
                for line in present:
                    assert line in output, (case, line)
                for line in absent:
                    assert line not in output, (case, line)
                check_fs(image, base / "check-volume.img", kind, True)
                image.unlink()
                print(f"PASS {kind} {case}: actual module admitted as the GUI admits ce-boot.kui", flush=True)
    print(f"PASS WinCE module admission: {len(module)} bytes, {len(CASES)} production cases on FAT32 and exFAT")


if __name__ == "__main__":
    main()
