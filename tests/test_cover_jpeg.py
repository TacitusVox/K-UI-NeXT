#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Full-size baseline JPEG through the native block reader and K-UI scaling."""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="kui-cover-jpeg-") as temp:
        folder = Path(temp)
        runner = folder / "read-jpeg"
        subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-g",
                        "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", "-Iinclude", "-I.deps/fatfs/source",
                        "tests/cover_jpeg_host.c", "src/apps/cover_jpeg.c",
                        "third_party/tjpgd/tjpgd.c", "src/core/game_cover.c",
                        "src/core/data.c", "src/core/destination.c", "-lm", "-o", runner],
                       cwd=ROOT, check=True)
        source, pixels = folder / "source.jpg", folder / "pixels.rgb565"
        env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}

        def run(mode):
            subprocess.run([runner, source, pixels, mode], check=True, env=env)

        def tiled(width, height):
            pic = Image.new("RGB", (width, height), "red")
            draw = ImageDraw.Draw(pic)
            for bounds, color in [((width//2, 0, width, height//2), "lime"),
                                  ((0, height//2, width//2, height), "blue"),
                                  ((width//2, height//2, width, height), "white")]:
                draw.rectangle(bounds, fill=color)
            return pic

        for w, h in ((4096, 2160), (2160, 3840), (4096, 4096)):
            picture = tiled(w, h)
            picture.save(source, quality=91)
            run("ok")
            samples = struct.unpack("<39552H", pixels.read_bytes())
            offset = 0
            for edge in (160, 104, 56):
                plane = samples[offset:offset+edge**2]
                offset += edge**2
                dw = max(1, round(edge*min(1, w/h)))
                dh = max(1, round(edge*min(1, h/w)))
                left, top = (edge-dw)//2, (edge-dh)//2
                for x, y, target in ((1, 1, 0xf800), (3, 1, 0x07e0),
                                     (1, 3, 0x001f), (3, 3, 0xffff)):
                    actual = plane[(top+dh*y//4)*edge+left+dw*x//4]
                    for shift, mask in ((0, 31), (5, 63), (11, 31)):
                        assert abs(((actual >> shift) & mask)-((target >> shift) & mask)) <= 1
                if w != h:
                    assert plane[0] == 0x0864

        run("cancel")
        run("io")
        valid = source.read_bytes()
        source.write_bytes(valid[:len(valid)//2]+b"\xff\xd9")
        run("invalid")
        source.write_bytes(valid[:-2])
        run("invalid")
        frame = valid.index(b"\xff\xc0")
        source.write_bytes(valid[:frame+7] + b"\x10\x01" + valid[frame+9:])
        run("invalid")
        picture.save(source, progressive=True)
        run("invalid")
        picture.thumbnail((192, 192))
        picture.save(source, progressive=True)
        run("small")
    print("PASS streamed 4K JPEG portrait, landscape, square, bounds, incomplete, cancellation and SD errors")


if __name__ == "__main__":
    main()
