#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fetch pinned Xiph release sources for the portable SH-4 preview decoder."""
from pathlib import Path
import hashlib
import shutil
import tarfile
import tempfile

from fetch_deps import DEPS, download

PACKAGES = (
    ("ogg", "libogg-1.3.6", "5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061"),
    ("theora", "libtheora-1.2.0", "ebdf77a8f5c0a8f7a9e42323844fa09502b34eb1d1fece7b5f54da41fe2122ec"),
)


def main():
    cache = DEPS / "downloads"
    cache.mkdir(parents=True, exist_ok=True)
    for subdir, version, digest in PACKAGES:
        archive = cache / (version + ".tar.xz")
        if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
            data = download(f"https://downloads.xiph.org/releases/{subdir}/{archive.name}")
            if hashlib.sha256(data).hexdigest() != digest:
                raise ValueError(f"Unexpected digest for {archive.name}")
            archive.write_bytes(data)
        with tarfile.open(archive, "r:xz") as tar, tempfile.TemporaryDirectory(dir=DEPS) as tmp:
            for member in tar:
                path = Path(member.name)
                if (member.isfile() and path.parts[0] == version and ".." not in path.parts
                        and len(path.parts) > 2 and path.parts[1] in ("src", "lib", "include")
                        and path.suffix in (".c", ".h")):
                    dest = Path(tmp) / path
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    with tar.extractfile(member) as src, dest.open("wb") as out:
                        shutil.copyfileobj(src, out)
            shutil.copytree(Path(tmp) / version, DEPS / version, dirs_exist_ok=True)


if __name__ == "__main__":
    main()
