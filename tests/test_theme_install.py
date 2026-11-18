#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Cold-load shipped art and mixed saved selections through real FAT32 and exFAT."""
import argparse
from array import array
import configparser
from dataclasses import dataclass
import shutil
import sys
import tempfile
import zlib
from pathlib import Path

from test_games_library_images import private_formatter
from test_images import run


ROOT = Path(__file__).resolve().parents[1]
FACTORY = "moonlit-muramasa"
V3_THEMES = {FACTORY, "chrome-muramasa", "neon-eclipse", "indigo-waves",
             "thanksgiving-samurai", "zombie-turkey", "gravy-racer"}
V3_BANNERS = {"midnight-forge", "vermilion-sun", "thanksgiving-pie"}


@dataclass
class Pack:
    directory: Path
    images: dict
    files: list
    banner_only: bool
    background: int


def pixel_id(pixels):
    return f"pixels-{zlib.crc32(pixels):08x}"


def installed_packs(directory):
    packs = {}
    for manifest in sorted(directory.glob("*/theme.ini")):
        data = configparser.ConfigParser(interpolation=None)
        with manifest.open(encoding="ascii") as stream:
            data.read_file(stream)
        assets = {}
        files = {manifest.name}
        for component in ("splash", "header", "background"):
            name = data["theme"].get(component)
            if not name or name == "none":
                assets[component] = name or "built-in"
                continue
            assert Path(name).name == name
            pixels = (manifest.parent / name).read_bytes()[32:]
            if sys.byteorder == "big":
                pixels = b"".join(pixels[i:i+2][::-1] for i in range(0, len(pixels), 2))
            assets[component] = pixel_id(pixels)
            files.add(name)
        background = int(data.get("colors", "background", fallback="#080f23").lstrip("#"), 16)
        rgb565 = ((background >> 8) & 0xf800) | ((background >> 5) & 0x07e0) | ((background >> 3) & 0x001f)
        packs[manifest.parent.name] = Pack(manifest.parent, assets, sorted(files),
                                          data["theme"].get("type") == "banner", rgb565)
    return packs


def test_card(image, packs, classic):
    def card(*arguments):
        return run(str(ROOT / "build/theme-image"), str(image), "installed", *arguments)

    def expect_boot(theme, splash, banner, frame=None):
        output = card("boot").splitlines()
        expected = (f"THEME {theme}", f"SPLASH {splash}", f"BANNER {banner}", f"FRAME {frame or splash}")
        assert all(line in output for line in expected), (expected, output)

    for pack, item in packs.items():
        card("install", str(item.directory), pack, *item.files)
    listed = {line[5:] for line in card("catalog").splitlines() if line.startswith("PACK ")}
    assert listed == {"default", *packs}, (listed, packs.keys())

    factory = packs[FACTORY].images
    assert factory["splash"].startswith("pixels-") and factory["header"].startswith("pixels-")
    expect_boot(FACTORY, factory["splash"], factory["header"])
    themes = {k:v for k,v in packs.items() if not v.banner_only}
    banners = {k:v for k,v in packs.items() if v.banner_only}
    assert V3_THEMES <= themes.keys(), f"missing 3.0 themes: {sorted(V3_THEMES - themes.keys())}"
    assert V3_BANNERS <= banners.keys(), f"missing 3.0 banners: {sorted(V3_BANNERS - banners.keys())}"
    other = next(k for k,v in themes.items() if v.images["splash"] != factory["splash"])
    alternate = themes[other].images
    banner = next(k for k,v in banners.items() if v.images["header"] != alternate["header"])

    # Save and boot each manifest individually, so a bad alternate outside the
    # two used for persistence checks cannot silently fall back to built-in art.
    for pack, item in themes.items():
        card("save", pack, "-", "-")
        assert item.images["splash"].startswith("pixels-")
        expect_boot(pack, item.images["splash"], item.images["header"])
    for pack, item in banners.items():
        card("save", other, "-", pack)
        assert item.images["header"].startswith("pixels-")
        expect_boot(other, alternate["splash"], item.images["header"])

    # Separate processes reproduce fresh mounts, reset all RAM and compare the
    # decoded pixels independently to the prepared files from the install tree.
    card("save", other, "-", banner)
    expect_boot(other, alternate["splash"], packs[banner].images["header"])
    card("save", FACTORY, other, banner)
    expect_boot(FACTORY, alternate["splash"], packs[banner].images["header"])
    card("save", "default", "-", "-")
    expect_boot("default", "built-in", "built-in", classic)
    card("save", FACTORY, "none", banner)
    fill = array("H", [packs[FACTORY].background]) * (640*480)
    expect_boot(FACTORY, "none", packs[banner].images["header"], pixel_id(fill))

    saves = len(packs) + 4
    newest, older = ("a", "b") if saves % 2 else ("b", "a")
    card("damage", newest)
    expect_boot("default", "built-in", "built-in", classic)
    card("damage", older)
    expect_boot(FACTORY, factory["splash"], factory["header"])
    card("save", other, "-", banner)
    splash_name = configparser.ConfigParser()
    splash_name.read(themes[other].directory / "theme.ini")
    card("remove", f"0:/KUI/themes/{other}/{splash_name['theme']['splash']}")
    expect_boot(other, "built-in", packs[banner].images["header"], factory["splash"])
    print(f"PASS {image.stem}: {len(packs)} shipped packs, factory startup, independent saved splash/banner, "
          "original, disabled, damaged newest/both slots and missing selected splash; exact first-frame CRCs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("themes", type=Path, nargs="?", default=ROOT / "resources/themes",
                        help="prepared themes directory from the source tree or the extracted install ZIP")
    packs = installed_packs(parser.parse_args().themes)
    assert FACTORY in packs, "factory theme missing from installation"
    sys.path.insert(0, str(ROOT / "tools"))
    from build_splash import decode_png
    classic = pixel_id(array("H", decode_png((ROOT / "resources/branding/startup.png").read_bytes())))
    with tempfile.TemporaryDirectory(prefix="kui-installed-themes-") as temp:
        base = Path(temp)
        external = shutil.which("mkfs.fat") and shutil.which("mkfs.exfat")
        formatter = None if external else private_formatter(base)
        for fs in ("fat32", "exfat"):
            image = base / f"{fs}.img"
            if formatter:
                run(str(formatter), fs, str(image))
            else:
                with image.open("wb") as stream:
                    stream.truncate(96 * 1024 * 1024)
                command = ("mkfs.fat", "-F", "32") if fs == "fat32" else ("mkfs.exfat",)
                run(*command, str(image))
            test_card(image, packs, classic)


if __name__ == "__main__":
    main()
