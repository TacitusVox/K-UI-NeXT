# SPDX-License-Identifier: GPL-3.0-only
"""Resolve a synthetic disc and install native internet media on FAT32 and exFAT."""
import argparse
from hashlib import sha256
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image
from games_fixture import make_fixture
from test_games_covers_images import pvr, write_manifest
from test_games_library_images import private_formatter

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT, timeout=100, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--public-cover", type=Path, help="Use the pinned public Crazy Taxi cover PNG")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="kui-internet-media-") as tmp:
        folder = Path(tmp)
        source = folder / "card"
        game = source / "Games" / "Local Game"
        disc_art = pvr(1, 9, 64, 32, b"\x1f\x00" * (64 * 32))
        make_fixture(game, b"CRAZY TAXI", art=disc_art)
        # Only the product/region describe a real release; all sectors and
        # artwork, manual pages and video frames are locally authored fixtures.
        track = game / "track03.bin"
        with track.open("r+b") as data:
            data.seek(16 + 48)
            data.write(b"U       ")
            data.seek(16 + 64)
            data.write(b"MK-51035  ")
        (source / "KUI" / "covers").mkdir(parents=True)
        write_manifest(source)
        if args.public_cover:
            data = args.public_cover.read_bytes()
            assert sha256(data).hexdigest() == "8e07bc902b28445319bd5630e6df4aab0e8ca2c232dae559f6e1379b4c8f5eac"
            (folder / "box.png").write_bytes(data)
        else:
            Image.new("RGB", (64, 64), "#ff0000").save(folder / "box.png")
        Image.new("RGB", (160, 120), "blue").save(folder / "page.jpg")
        run("ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=160x120:r=10",
            "-t", "24", "-threads", "1", "-c:v", "libtheora", "-an", str(folder / "clip.ogv"))
        formatter = private_formatter(folder)
        for kind in ("fat32", "exfat"):
            if kind == "exfat":
                # IP.BIN uses the first peripheral bit to identify Windows CE.
                with track.open("r+b") as data:
                    data.seek(16 + 56)
                    data.write(b"0000001 ")
            card = folder / (kind + ".img")
            run(formatter, kind, card)
            run(ROOT / "build/games-covers-image", card, source, "seed", "network-media")
            thumbs = folder / (kind + "-thumbs.rgb565")
            run(ROOT / "build/bba-media-integration", card, folder, *([thumbs] if args.public_cover else []))
            if args.public_cover:
                # Independently calculated from the public RGBA pixels using integer area
                # resampling, alpha blending and RGB565 quantization at 160/104/56 square.
                digest = sha256(thumbs.read_bytes()).hexdigest()
                assert digest == "984b5b4f853beff0b24be1dd0258a8854a08ed7e47fc85c03bd7315ade99096b", digest
                print(f"{kind}: all three retrieved public cover thumbnails match {digest}")


if __name__ == "__main__":
    main()
