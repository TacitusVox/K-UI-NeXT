# SPDX-License-Identifier: GPL-3.0-only
"""Resolve a synthetic disc and install native internet media on FAT32 and exFAT."""
import argparse
from hashlib import sha256
import os
from pathlib import Path
import struct
import subprocess
import tempfile

from PIL import Image, ImageChops, ImageStat
from games_fixture import make_fixture
from test_games_covers_images import pvr, write_manifest
from test_games_library_images import private_formatter

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT, timeout=100, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


# Hashes from pinned provider PNGs and an independent integer-area RGB565 resample.
RELEASES = {
    "crazy": (b"CRAZY TAXI", b"MK-51035  ", "8e07bc902b28445319bd5630e6df4aab0e8ca2c232dae559f6e1379b4c8f5eac",
              "984b5b4f853beff0b24be1dd0258a8854a08ed7e47fc85c03bd7315ade99096b"),
    "skies": (b"SKIES OF ARCADIA", b"MK-51052  ", "52e96096f9acce49d0a29bdfcbd2fe1e2c3e837382f2bffa8ac1f95e1bdb0475",
              "2d68e87b551a150faa7addf2b692bea29a0be73b477c5fb31c47c354793f140f"),
}


def check_scaled_cover(source, dumped, max_mean_error=12):
    # Compare the on-card RGB565 to an independent decoder and area resampler.
    # DCT, integer filtering and RGB565 quantization can introduce small errors.
    data = memoryview(dumped.read_bytes())
    with Image.open(source) as image:
        if image.mode == "RGBA":
            background = Image.new("RGBA", image.size, (8, 12, 33, 255))
            original = Image.alpha_composite(background, image).convert("RGB")
        else:
            original = image.convert("RGB")
    width, height = original.size
    for edge in (160, 104, 56):
        dw = max(1, (width * edge + max(width, height) // 2) // max(width, height))
        dh = max(1, (height * edge + max(width, height) // 2) // max(width, height))
        expected = Image.new("RGB", (edge, edge), (8, 12, 33))
        expected.paste(original.resize((dw, dh), Image.Resampling.BOX),
                       ((edge - dw) // 2, (edge - dh) // 2))
        samples = data[:edge*edge*2]
        data = data[len(samples):]
        rgb = bytearray()
        for lo, hi in zip(samples[::2], samples[1::2]):
            value = lo | hi << 8
            rgb.extend(((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
        assert len(samples) == 2*edge*edge
        left, top = (edge-dw)//2, (edge-dh)//2
        for y in range(edge):
            for x in range(edge):
                if left <= x < left+dw and top <= y < top+dh:
                    continue
                at = 2*(y*edge+x)
                assert bytes(samples[at:at+2]) == b"\x64\x08", (edge, x, y)
        error = ImageStat.Stat(ImageChops.difference(Image.frombytes("RGB", (edge, edge), bytes(rgb)), expected)).mean
        assert max(error) < max_mean_error, (edge, error)
    assert not data


def multidisc_fixture(folder):
    source = folder / "multidisc"
    (source / "KUI").mkdir(parents=True)
    for disc in (1, 2):
        game = source / "Games" / f"Skies of Arcadia Disc {disc}"
        make_fixture(game, b"SKIES OF ARCADIA")
        with (game / "track03.bin").open("r+b") as data:
            data.seek(16 + 32)
            data.write(f"0000 GD-ROM{disc}/2  ".encode())
            data.write(b"U       0000000 MK-51052  ")
    make_fixture(source / "Games" / "Standalone", b"Unrelated Test Game")
    write_manifest(source)
    # One legacy frame deliberately recognisable after an interrupted upgrade.
    (folder / "legacy.kpv").write_bytes(struct.pack("<4sHHHHI",b"KPV1",160,120,10,0,1)
                                        + b"\x5a\xa5"*(160*120))
    (folder / "original.pdf").write_bytes(b"%PDF-1.4\n" + b" " * 64 + b"\n%%EOF\n")
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--public-cover", type=Path, help="Use the pinned public cover PNG for --release")
    parser.add_argument("--public-small", type=Path, help="Use this downloaded matched JPEG as the first origin")
    parser.add_argument("--release", choices=RELEASES, default="crazy")
    args = parser.parse_args()
    title, product, png_hash, thumbnail_hash = RELEASES[args.release]
    if args.public_small and not args.public_cover:
        parser.error("--public-small also requires the fallback --public-cover")
    with tempfile.TemporaryDirectory(prefix="kui-internet-media-") as tmp:
        folder = Path(tmp)
        source = folder / "card"
        game = source / "Games" / "Local Game"
        disc_art = pvr(1, 9, 64, 32, b"\x1f\x00" * (64 * 32))
        make_fixture(game, title, art=disc_art)
        make_fixture(source / "KUI" / "Regional" / "Local Game", b"OTHER REGION",
                     art=pvr(1, 9, 64, 32, b"\xe0\xff" * (64 * 32)))
        # Only the product/region describe a real release; all sectors and
        # artwork, manual pages and video frames are locally authored fixtures.
        track = game / "track03.bin"
        with track.open("r+b") as data:
            data.seek(16 + 48)
            data.write(b"U       ")
            data.seek(16 + 64)
            data.write(product)
        (source / "KUI" / "covers").mkdir(parents=True)
        Image.new("RGB", (32, 32), "#00ff00").save(folder / "custom.png")
        (source / "KUI" / "owner.png").write_bytes((folder / "custom.png").read_bytes())
        write_manifest(source)
        if args.public_cover:
            if args.public_small:
                (folder / "small.jpg").write_bytes(args.public_small.read_bytes())
            data = args.public_cover.read_bytes()
            assert sha256(data).hexdigest() == png_hash
            (folder / "box.png").write_bytes(data)
        else:
            Image.new("RGB", (64, 64), "#ff0000").save(folder / "box.png")
        Image.new("RGB", (160, 120), "blue").save(folder / "page.jpg")
        run("ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=160x120:r=10",
            "-f", "lavfi", "-i", "sine=frequency=850:sample_rate=32000", "-t", "24",
            "-threads", "1", "-c:v", "libtheora", "-c:a", "libvorbis", str(folder / "clip.ogv"))
        disc_source = multidisc_fixture(folder)
        formatter = private_formatter(folder)
        for kind in ("fat32", "exfat"):
            if kind == "exfat":
                # Exercise the JPEG-first path on the ordinary offline suite too.
                if not args.public_cover:
                    Image.new("RGB", (240, 210), "#ff0000").save(folder / "small.jpg")
                # IP.BIN uses the first peripheral bit to identify Windows CE.
                with track.open("r+b") as data:
                    data.seek(16 + 56)
                    data.write(b"0000001 ")
            card = folder / (kind + ".img")
            run(formatter, kind, card)
            run(ROOT / "build/games-covers-image", card, source, "seed", "network-media")
            thumbs = folder / (kind + "-thumbs.rgb565")
            run(ROOT / "build/bba-media-integration", card, folder, thumbs if args.public_cover else "-", args.release)
            if args.public_small:
                check_scaled_cover(args.public_small, thumbs)
                print(f"{kind}: all three regional JPEG cover thumbnails match the independent resample")
            elif args.public_cover:
                # Independently calculated from the public RGBA pixels using integer area
                # resampling, alpha blending and RGB565 quantization at 160/104/56 square.
                digest = sha256(thumbs.read_bytes()).hexdigest()
                assert digest == thumbnail_hash, digest
                print(f"{kind}: all three retrieved public cover thumbnails match {digest}")
            cleanup_card = folder / (kind + "-cleanup.img")
            run(formatter, kind, cleanup_card)
            run(ROOT / "build/bba-media-integration", cleanup_card, folder, "-", "cleanup")
            group_card = folder / (kind + "-discs.img")
            run(formatter, kind, group_card)
            run(ROOT / "build/games-covers-image", group_card, disc_source, "seed", "network-media")
            run(ROOT / "build/bba-media-integration", group_card, folder, "-", "discs")


if __name__ == "__main__":
    main()
