#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Theme producer checks: actual image bytes, malformed packs and preservation."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pack_theme", ROOT / "tools/pack_theme.py")
pack = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pack)


class ThemePackingTests(unittest.TestCase):
    def test_thumbnail_uses_original_source_and_rejects_stale_or_damaged_data(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            # The legacy full-size nearest filter aliases every 2-pixel stripe.
            # A preview sampled from that packed image could never restore gray.
            stripes = Image.frombytes("L", (1280, 960), (b"\0\xff" * 640) * 960)
            stripes.save(directory / "stripes.png")
            source = directory / "source.json"
            source.write_text(json.dumps({"name": "Stripes", "version": 1, "assets": {
                "splash": {"source": "stripes.png", "filter": "nearest"}, "background": "none"}}))
            output = directory / "stripes"
            pack.build(source, output)
            data = (output / "splash.kuiimg").read_bytes()
            self.assertEqual(len(set(struct.iter_unpack("<H", data[32:]))), 1)
            crc = zlib.crc32(data[32:])
            path = output / "splash.kuiimg.thumb"
            thumb = path.read_bytes()
            self.assertEqual(len(thumb), 32 + 320 * 240 * 2)
            gray = struct.unpack_from("<H", thumb, 32 + 160 * 2)[0]
            self.assertIn(gray >> 11, range(14, 18))
            pack.verify_thumbnail(thumb, "splash", crc)
            for at in (0, 8, 12, 14, 16, 20, 24, 28, len(thumb)-1):
                bad = bytearray(thumb)
                bad[at] ^= 1
                with self.subTest(index=at), self.assertRaises(ValueError):
                    pack.verify_thumbnail(bad, "splash", crc)
            path.write_bytes(thumb[:-1])
            with self.assertRaises(ValueError):
                pack.verify_folder(output)
            path.unlink()
            pack.verify_folder(output)  # Existing third-party packs need no sidecars.
            self.assertFalse((output / "background.kuiimg.thumb").exists())

    def test_wire_format_and_rgb565(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGB", (640, 480), (255, 0, 0)).save(directory / "red.png")
            payload = pack.image_pixels(directory / "source.json", {"source": "red.png"}, "splash", "#000000")
            self.assertEqual(payload, b"\0\xf8" * (640 * 480))
            image = pack.encode_image(payload, "splash")
            self.assertEqual(image[:8], b"KUITEX01")
            self.assertEqual(struct.unpack_from("<IHHII", image, 8),
                             (1, 640, 480, 614400, zlib.crc32(payload)))
            self.assertEqual(image[24:32], b"\0" * 8)
            self.assertEqual(pack.verify_image(image, "splash")["pixel_bytes"], 614400)

    def test_dimension_crc_and_header_rejection(self):
        for kind, (width, height) in pack.DIMENSIONS.items():
            good = pack.encode_image(b"\x12\x34" * width * height, kind)
            self.assertEqual(pack.verify_image(good, kind)["height"], height)
            for bad in (good[:31], good[:-1], good + b"\0",
                        b"X" + good[1:], good[:8] + b"\x02" + good[9:],
                        good[:24] + b"\x01" + good[25:], good[:-1] + b"\x35"):
                with self.subTest(kind=kind, damaged=len(bad)):
                    with self.assertRaises(ValueError):
                        pack.verify_image(bad, kind)
            wrong = bytearray(good)
            struct.pack_into("<H", wrong, 12, 320)
            with self.assertRaises(ValueError):
                pack.verify_image(wrong, kind)

    def test_build_contain_crop_dim_and_preferences(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            # Square input must be letterboxed, not stretched: opaque white square.
            Image.new("RGBA", (100, 100), (255, 255, 255, 255)).save(directory / "square.png")
            definition = {"version": 1, "name": "Test pack", "author": "TacitusVox",
                          "colors": {"background": "#000000", "accent": "#FF0000"},
                          "assets": {"splash": {"source": "square.png"},
                                     "header": {"source": "square.png", "crop": [0, 0, 100, 50],
                                                "fit": "cover", "brightness": 0.0},
                                     "background": "none"}}
            source = directory / "source.json"
            source.write_text(json.dumps(definition))
            output = directory / "test-pack"
            output.mkdir()
            preferences = output / "theme-a.bin"
            preferences.write_bytes(b"never overwrite preferences")
            result = pack.build(source, output)
            self.assertEqual(set(result["assets"]), {"splash", "header"})
            self.assertEqual(preferences.read_bytes(), b"never overwrite preferences")
            pixels = (output / "splash.kuiimg").read_bytes()[32:]
            self.assertEqual(pixels[:160], b"\0" * 160)
            self.assertEqual(pixels[160:1120], b"\xff" * 960)
            self.assertEqual(pixels[1120:1280], b"\0" * 160)
            self.assertEqual((output / "header.kuiimg").read_bytes()[32:], b"\0" * 128000)
            self.assertIn("background=none", (output / "theme.ini").read_text())
            pack.build(source, output, check=True)
            original = (output / "theme.ini").read_bytes()
            (output / "theme.ini").write_bytes(original + b"\n")
            with self.assertRaises(ValueError):
                pack.build(source, output, check=True)

    def test_banner_pack_reserves_the_right_half_for_status(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGB", (300, 100), "#FFFFFF").save(directory / "logo.png")
            source = directory / "source.json"
            source.write_text(json.dumps({"name": "Logo", "version": 1, "type": "banner",
                                         "assets": {"header": {"source": "logo.png", "matte": "#000000"}}}))
            files = pack.prepare(source)
            self.assertEqual(pack.parse_manifest(files["theme.ini"])["type"], "banner")
            row = files["header.kuiimg"][32:32 + 1280]
            self.assertEqual(row, bytes(20) + b"\xff" * 600 + bytes(660))
            header = files["header.kuiimg"][32:]
            thumb = files["header.kuiimg.thumb"]
            pack.verify_thumbnail(thumb, "header", zlib.crc32(header), banner_only=True)
            self.assertEqual(thumb[32:], b"".join(header[y*1280:y*1280+640] for y in range(100)))
            with self.assertRaises(ValueError):
                pack.verify_thumbnail(thumb, "header", zlib.crc32(header))
            for invalid in (b"", b"header=none", b"header=logo.kuiimg\nsplash=splash.kuiimg"):
                with self.assertRaises(ValueError):
                    pack.parse_manifest(b"[theme]\nversion=1\ntype=banner\n" + invalid)

    def test_dither_preserves_dark_art_average_and_solid_status_area(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGB", (320, 100), (4, 2, 4)).save(directory / "dark.png")
            spec = {"source": "dark.png", "dither": True}
            data = pack.image_pixels(directory / "source.json", spec, "header", "#000000", banner_only=True)
            lit = [0, 0, 0]
            for row in range(100):
                line = data[row * 1280:(row + 1) * 1280]
                self.assertEqual(line[640:], bytes(640))
                for (color,) in struct.iter_unpack("<H", line[:640]):
                    lit[0] += color >> 11
                    lit[1] += (color >> 5) & 63
                    lit[2] += color & 31
            for value, levels, expected in zip(lit, (31, 63, 31), (4, 2, 4)):
                self.assertAlmostEqual(value * 255 / (320 * 100 * levels), expected, delta=0.15)
            with self.assertRaises(ValueError):
                pack.image_pixels(directory / "source.json", {**spec, "dither": "yes"}, "header", "#000000")

    def test_alpha_is_precomposed(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGBA", (640, 100), (255, 0, 0, 0)).save(directory / "transparent.png")
            pixels = pack.image_pixels(directory / "source.json", {"source": "transparent.png"}, "header", "#00FF00")
            self.assertEqual(pixels, b"\xe0\x07" * (640 * 100))

    def test_manifest_paths_and_schema(self):
        good = b"[theme]\nversion=1\nname=Example\nauthor=TacitusVox\nsplash=splash.kuiimg\n[colors]\naccent=#AABBCC\n"
        self.assertEqual(pack.parse_manifest(good)["splash"], "splash.kuiimg")
        bad = [good.replace(b"splash.kuiimg", path) for path in
               (b"../outside.kuiimg", b"/absolute", b"0:/KUI/test", b"a\\b", b"double..dot", b"x" * 64)]
        bad += [good.replace(b"version=1", b"version=2"), good.replace(b"version=1", b"version=1\nversion=1"),
                good.replace(b"[colors]", b"[unknown]"), good.replace(b"accent=", b"unrecognized="),
                good.replace(b"#AABBCC", b"#AABBCCD"), good + b"\x00", b"x" * 4097,
                good + b"# DEL\x7f", good.replace(b"[theme]", b"[theme]extra"),
                good + b"#" + b"x" * 190 + b"\r\n",
                good.replace(b"name=Example", b"name=" + b"n" * 48)]
        for manifest in bad:
            with self.subTest(manifest=manifest[:100]):
                with self.assertRaises(ValueError):
                    pack.parse_manifest(manifest)
        for identifier in ("../escape", "a/b", "x" * 32, "default", "Default", "NONE", "none", "-start", ""):
            with self.assertRaises(ValueError):
                pack.checked_id(identifier)

    def test_bad_source_never_publishes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "test-pack"
            source = directory / "source.json"
            base = {"version": 1, "name": "Example", "colors": {"text": "#FFFFFF"}}
            cases = []
            bad = copy.deepcopy(base); bad["version"] = True; cases.append(bad)
            bad = copy.deepcopy(base); bad["colors"]["wrong"] = "#FFFFFF"; cases.append(bad)
            bad = copy.deepcopy(base); bad["colors"]["text"] = "#GGGGGG"; cases.append(bad)
            bad = copy.deepcopy(base); bad["assets"] = {"wrong": "none"}; cases.append(bad)
            bad = copy.deepcopy(base); bad["assets"] = {"header": {"source": "missing.png"}}; cases.append(bad)
            for definition in cases:
                source.write_text(json.dumps(definition))
                with self.assertRaises((ValueError, OSError)):
                    pack.build(source, output)
                self.assertFalse(output.exists())
            source.write_text('{"version":1,"version":1,"name":"Example"}')
            with self.assertRaises(ValueError):
                pack.build(source, output)

    def test_shipped_moonlit_pack_is_reproducible(self):
        folder = ROOT / "resources/themes/moonlit-muramasa"
        result = pack.build(folder / "source.json", folder, check=True)
        self.assertEqual(set(result["assets"]), {"splash", "header"})
        self.assertEqual(result["assets"]["header"]["pixel_bytes"], 128000)

    def test_collection_stages_all_prepared_artwork(self):
        collection = ROOT / "resources/themes"
        with tempfile.TemporaryDirectory() as temporary:
            staged = Path(temporary) / "KUI/themes"
            pack.stage_all(staged)
            expected = {source.parent.name for source in collection.glob("*/source.json")}
            self.assertEqual({p.name for p in staged.iterdir()}, expected)
            for name in sorted(expected):
                with self.subTest(name=name):
                    folder = collection / name
                    prepared = pack.prepare(folder / "source.json")
                    self.assertEqual({p.name: p.read_bytes() for p in (staged / name).iterdir()}, prepared)

    def test_actual_pack_passes_production_c_consumer(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            reader = directory / "theme-pack-reader"
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-Iinclude", "-I.deps/fatfs/source",
                "tests/theme_pack_reader.c", "src/core/theme.c", "src/core/data.c", "-o", str(reader)]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
            source = ROOT / "resources/themes/moonlit-muramasa"
            result = subprocess.run([str(reader), str(source)], check=True, capture_output=True, text=True)
            self.assertIn("accepted 2 prepared assets", result.stdout)
            damaged = directory / "damaged-theme"
            damaged.mkdir()
            for name in ("theme.ini", "splash.kuiimg", "header.kuiimg"):
                shutil.copyfile(source / name, damaged / name)
            content = bytearray((damaged / "header.kuiimg").read_bytes())
            content[-1] ^= 1
            (damaged / "header.kuiimg").write_bytes(content)
            self.assertNotEqual(subprocess.run([str(reader), str(damaged)], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
