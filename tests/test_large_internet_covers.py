#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run full-resolution PNG and JPEG sources through the Games scan on two card formats."""
from pathlib import Path
import random
import tempfile

from PIL import Image, ImageChops, ImageDraw

from games_fixture import make_fixture
from test_bba_media_integration import check_scaled_cover, run
from test_games_covers_images import pvr, write_manifest
from test_games_library_images import private_formatter

ROOT = Path(__file__).resolve().parents[1]


def make_cover(path, size, fmt):
    width, height = size
    base = Image.new("RGB", size)
    draw = ImageDraw.Draw(base)
    for x in range(6):
        for y in range(5):
            draw.rectangle((x*width//6, y*height//5, (x+1)*width//6-1, (y+1)*height//5-1),
                           fill=((x*31+y*7)%190, (x*19+y*21)%190, (x*13+y*33)%190))
    draw.polygon([(width//4, height//6), (width//2, height//5), (width//3, height//2)], (186, 25, 30))
    # High-frequency noise makes the actual compressed payload exceed 3 MiB.
    # The asymmetric colour field is still visible after reduction to 56 px.
    noise = Image.frombytes("L", size, random.Random(1940).randbytes(width*height))
    noise = noise.point([i//4 for i in range(256)]).convert("RGB")
    image = ImageChops.add(base, noise)
    if fmt == "PNG":
        alpha = Image.new("L", size, 255)
        mask = ImageDraw.Draw(alpha)
        mask.rectangle((0, height//2, width//3, height), fill=80)
        mask.polygon([(0, 0), (width//2, 0), (0, height//2)], fill=0)
        image.putalpha(alpha)
        image.save(path, fmt, compress_level=1)
    else:
        image.save(path, fmt, quality=97, subsampling=2, progressive=False)
    with Image.open(path) as saved:
        assert saved.format == fmt and saved.size == size
    assert path.stat().st_size > 3 * 1024 * 1024


def main():
    with tempfile.TemporaryDirectory(prefix="kui-large-box-art-") as tmp:
        work = Path(tmp)
        seed = work / "card"
        game = seed / "Games" / "Local Game"
        make_fixture(game, b"CRAZY TAXI", art=pvr(1, 9, 64, 32, b"\x1f\x00" * 64 * 32))
        with (game / "track03.bin").open("r+b") as data:
            data.seek(16+48)
            data.write(b"U       ")
            data.seek(16+64)
            data.write(b"MK-51035  ")
        (seed / "KUI" / "covers").mkdir(parents=True)
        write_manifest(seed)
        formatter = private_formatter(work)
        origin = work / "origin"  # Never infer the actual payload format from its name.
        for fmt, size in (("PNG", (3840, 2160)), ("JPEG", (2160, 3840))):
            make_cover(origin, size, fmt)
            reference = None
            for fs in ("fat32", "exfat"):
                card = work / f"{fmt}-{fs}.img"
                run(formatter, fs, card)
                run(ROOT / "build/games-covers-image", card, seed, "seed", "network-media")
                result = work / f"{fmt}-{fs}.rgb565"
                run(ROOT / "build/bba-media-integration", card, work, result, "4k")
                check_scaled_cover(origin, result, max_mean_error=5)
                pixels = result.read_bytes()
                if reference is None:
                    reference = pixels
                else:
                    assert pixels == reference, f"{fmt} thumbnail pixels differ by filesystem"
                print(f"{fs} {size[0]}x{size[1]} {fmt}: all three display sizes, borders and repeat scan passed",
                      flush=True)


if __name__ == "__main__":
    main()
