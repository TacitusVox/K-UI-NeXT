#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and run the WinCE data probe's FAT32/exFAT lookup test against FatFs."""
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def fatfs_source():
    dependency = ROOT / ".deps/fatfs/source"
    return dependency if dependency.is_dir() else ROOT / "fatfs/source"


def main():
    config = (ROOT / "config/ffconf.h").read_text()
    marker = "#define FF_USE_MKFS 0"
    assert config.count(marker) == 1
    with tempfile.TemporaryDirectory() as scratch:
        base = Path(scratch)
        header = base / "probe-ffconf.h"
        header.write_text(config.replace(marker, "#define FF_USE_MKFS 1"))
        binary = base / "test-wce-probe-fat"
        subprocess.run([*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1", "-g",
                        "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", "-include", str(header),
                        "-I" + str(ROOT / "include"), "-I" + str(fatfs_source()),
                        str(ROOT / "tests/test_wce_probe_fat.c"), str(ROOT / "src/core/wce_probe_fat.c"),
                        str(fatfs_source() / "ff.c"), str(fatfs_source() / "ffunicode.c"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    sys.exit(main())
