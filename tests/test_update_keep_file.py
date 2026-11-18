#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test the independent download limit and legacy settings on real FatFs volumes."""
import os
from pathlib import Path
import shlex
import tempfile

from test_games_library_images import fatfs_source, private_formatter
from test_update_images import run


def main():
    source = fatfs_source()
    with tempfile.TemporaryDirectory(prefix="kui-update-keep-") as temporary:
        work = Path(temporary)
        formatter = private_formatter(work)
        binary = work / "update-keep"
        run(*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-fsanitize=address,undefined",
            "-fno-sanitize-recover=all", "-fno-pie", "-no-pie", "-Iinclude", "-I" + str(source),
            "tests/update_keep_file_image.c", "src/core/update_keep_file.c", "src/core/data.c",
            "src/apps/system_settings.c", "src/apps/system_settings_file.c", "src/core/diskio.c",
            str(source / "ff.c"), str(source / "ffunicode.c"), "-o", str(binary))
        for kind in ("fat32", "exfat"):
            image = work / (kind + ".img")
            run(str(formatter), kind, str(image))
            print(kind + ": " + run(str(binary), str(image)).strip(), flush=True)


if __name__ == "__main__":
    main()
