#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the nightly downloader with real FAT32/exFAT storage and an authored HTTPS origin."""
import os
from pathlib import Path
import shlex
import tempfile

from test_games_library_images import fatfs_source, private_formatter
from test_update_images import run


def main():
    source = fatfs_source()
    with tempfile.TemporaryDirectory(prefix="kui-nightly-") as temporary:
        base = Path(temporary)
        formatter = private_formatter(base)
        for review in (False, True):
            channel = "review" if review else "production"
            binary = base / channel
            run(*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-pie", "-no-pie",
                "-Iinclude", "-I" + str(source), "-DKUI_UPDATE_REVIEW_FEED=" + str(int(review)),
                "tests/update_nightly_image.c", "src/core/update_nightly.c", "src/core/update_retention.c",
                "src/core/hash.c",
                "src/core/data.c", "src/core/diskio.c", str(source/"ff.c"), str(source/"ffunicode.c"),
                "-o", str(binary))
            for kind in ("fat32", "exfat"):
                image = base / (channel + "-" + kind + ".img")
                run(str(formatter), kind, str(image))
                print(channel, kind + ": " + run(str(binary), str(image)).strip(), flush=True)


if __name__ == "__main__":
    main()
