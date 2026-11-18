#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare and verify K-UI theme folders; image conversion happens on the PC.

    python3 tools/pack_theme.py build source.json output/theme-id
    python3 tools/pack_theme.py build source.json output/theme-id --check
    python3 tools/pack_theme.py verify output/theme-id
    python3 tools/pack_theme.py stage-all staging/KUI/themes

Build requires Pillow. Verification uses only the Python standard library.
The Dreamcast reads theme.ini, .kuiimg artwork and optional .thumb previews.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import zlib

MAGIC = b"KUITEX01"
HEADER = struct.Struct("<8sIHHII8s")
DIMENSIONS = {"splash": (640, 480), "header": (640, 100), "background": (640, 480)}
THUMB_DIMENSIONS = {kind: (width // 2, height // 2) for kind, (width, height) in DIMENSIONS.items()}
THUMB_HEADER = struct.Struct("<8sIHHIIII")
THUMB_MAGIC = b"KUITHM01"
COLOR_KEYS = (
    "background", "panel", "selection", "edge", "text", "muted", "accent",
    "accent2", "success", "warning", "selection_text",
)
MANIFEST_MAX = 4096
IMAGE_PIXELS_MAX = 16 * 1024 * 1024


def checked_name(value: object, label: str) -> str:
    if not isinstance(value, str) or not value or len(value) > 47:
        raise ValueError(f"{label} must contain 1..47 printable ASCII characters")
    if value != value.strip() or any(ord(c) < 32 or ord(c) > 126 for c in value):
        raise ValueError(f"{label} must be printable ASCII without surrounding whitespace")
    return value


def checked_id(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,30}", value):
        raise ValueError("Theme folder name must be 1..31 ASCII letters, digits, underscores or hyphens")
    if value.lower() in ("default", "none"):
        raise ValueError(f"'{value}' is reserved for the built-in theme/disabled choice")
    return value


def checked_color(value: object) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"#[0-9A-Fa-f]{6}", value):
        raise ValueError(f"Expected #RRGGBB color, got {value!r}")
    return value.upper()


def checked_filename(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,62}", value) or ".." in value:
        raise ValueError(f"Unsafe asset basename: {value!r}")
    return value


def json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate source key: {key}")
        result[key] = value
    return result


def source_definition(path: Path) -> dict:
    definition = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=json_object)
    if not isinstance(definition, dict):
        raise ValueError("Source definition must be a JSON object")
    unknown = set(definition) - {"version", "name", "author", "colors", "assets", "type"}
    if unknown:
        raise ValueError(f"Unknown source keys: {', '.join(sorted(unknown))}")
    if type(definition.get("version")) is not int or definition["version"] != 1:
        raise ValueError("Source version must be 1")
    checked_name(definition.get("name"), "Theme name")
    if "author" in definition:
        checked_name(definition["author"], "Theme author")
    colors = definition.get("colors", {})
    assets = definition.get("assets", {})
    if not isinstance(colors, dict) or set(colors) - set(COLOR_KEYS):
        raise ValueError("Colors must be an object containing only documented color roles")
    for color in colors.values():
        checked_color(color)
    if not isinstance(assets, dict) or set(assets) - set(DIMENSIONS):
        raise ValueError("Assets must be an object containing splash, header or background")
    check_pack_type(definition, assets)
    return definition


def check_pack_type(definition: dict, assets: dict) -> None:
    if "type" not in definition:
        return
    if definition["type"] != "banner":
        raise ValueError("The optional pack type must be banner")
    if set(assets) != {"header"} or assets["header"] == "none":
        raise ValueError("A banner pack must supply just its header image")


def header_left(image, target, filtering):
    """Frame the selected portrait to the left of the standard menu text."""
    from PIL import Image, ImageChops, ImageOps

    width, height = target
    image = ImageOps.contain(image, target, method=filtering)
    # The version starts at x=176 and the URL crosses the footer at y=78.
    # Keep the face and cup bright and fade the art under those live labels.
    end = min(image.width, width * 7 // 20)
    image = image.crop((0, 0, end, image.height))
    start = end * 2 // 3
    horizontal = [min(1.0, (end - 1 - x) / max(1, end - 1 - start)) for x in range(end)]
    top = (height - image.height) // 2
    vertical = [1.0 - .78 * max(0.0, min(1.0, ((y + top) / height - .62) / .17))
                for y in range(image.height)]
    mask = Image.new("L", image.size)
    mask.putdata([round(255 * x * y) for y in vertical for x in horizontal])
    image.putalpha(ImageChops.multiply(image.getchannel("A"), mask))
    return image


def dither_channel(channel, levels: int) -> bytearray:
    """Round to the display levels and diffuse the residual across neighboring pixels."""
    source = channel.tobytes()
    width, height = channel.size
    output = bytearray(len(source))
    errors = [0] * (width + 2)
    for y in range(height):
        following = [0] * (width + 2)
        step = 1 if y % 2 == 0 else -1
        for x in (range(width) if step == 1 else reversed(range(width))):
            index = y * width + x
            slot = x + 1
            # 4080 is one display level (255 × the 16 diffusion weights).
            value = source[index] * levels * 16 + errors[slot]
            level = max(0, min(levels, (value + 2040) // 4080))
            output[index] = level
            residual = (value - level * 4080 + 8) // 16
            errors[slot + step] += residual * 7
            following[slot - step] += residual * 3
            following[slot] += residual * 5
            following[slot + step] += residual
        errors = following
    return output


def checked_rect(rect: object, size: tuple[int, int], label: str) -> list[int]:
    if (not isinstance(rect, list) or len(rect) != 4 or any(type(v) is not int for v in rect) or
            not (0 <= rect[0] < rect[2] <= size[0] and 0 <= rect[1] < rect[3] <= size[1])):
        raise ValueError(f"{label}: expected a rectangle inside {size[0]}×{size[1]}")
    return rect


def compose_banner_lettering(canvas, source, spec: dict, author: str, matte: str) -> None:
    from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

    # The standalone banner owns the left half of the native header only.
    artwork = canvas.crop((0, 0, canvas.width // 2, canvas.height))
    if "logo_detail" in spec:
        detail = spec["logo_detail"]
        if not isinstance(detail, dict) or set(detail) != {"crop", "box"}:
            raise ValueError("logo_detail requires a source crop and a destination box")
        crop = checked_rect(detail["crop"], source.size, "logo_detail.crop")
        x, y, right, bottom = checked_rect(detail["box"], artwork.size, "logo_detail.box")
        logo = ImageOps.contain(source.crop(crop), (right - x, bottom - y), method=Image.Resampling.LANCZOS)
        # Soften just the outer edge of the pasted source background.
        mask = Image.new("L", (logo.width + 8, logo.height + 8))
        ImageDraw.Draw(mask).rectangle((6, 6, logo.width + 1, logo.height + 1), fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(1.5)).crop((4, 4, logo.width + 4, logo.height + 4))
        artwork.paste(logo, (x, y), ImageChops.multiply(mask, logo.getchannel("A")))

    if "credit_at" in spec:
        author = checked_name(author, "Banner credit author")
        at = spec["credit_at"]
        if not isinstance(at, list) or len(at) != 2 or any(type(v) is not int for v in at):
            raise ValueError("credit_at requires [left, top] in the banner artwork")
        x, y = at
        text = f"by {author}"
        font_file = Path(__file__).resolve().parents[1] / "resources/fonts/DejaVuSans-Bold.ttf"
        font = ImageFont.truetype(str(font_file), 18, layout_engine=ImageFont.Layout.BASIC)
        left, top, right, bottom = font.getbbox(text)
        end_x, end_y = x + right - left, y + bottom - top
        checked_rect([x, y, end_x, end_y], artwork.size, "Banner credit")
        # A feathered matte replaces the original small text. Scale the source
        # artwork first, then draw the credit directly at its final pixel size.
        mask = Image.new("L", artwork.size)
        ImageDraw.Draw(mask).rectangle((x - 7, y - 3, end_x + 4, end_y + 3), fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(1.5))
        artwork.paste(matte, (0, 0, *artwork.size), mask)
        ImageDraw.Draw(artwork).text((x - left, y - top), text, font=font,
                                    fill="#FFF3DF", stroke_width=1, stroke_fill="#10101A")
    canvas.paste(artwork, (0, 0))


def image_pixels(path: Path, specification: object, kind: str, matte: str, banner_only: bool = False,
                 *, preview: bool = False, author: str = "") -> bytes:
    # Import here: verify does not require image tooling on the installed system.
    from PIL import Image, ImageEnhance, ImageOps

    if not isinstance(specification, dict):
        raise ValueError(f"{kind}: asset must be an object or the string 'none'")
    unknown = set(specification) - {"source", "sha256", "crop", "fit", "filter", "matte", "brightness", "dither",
                                     "credit_at", "logo_detail"}
    if unknown:
        raise ValueError(f"{kind}: unknown asset keys: {', '.join(sorted(unknown))}")
    source = specification.get("source")
    if not isinstance(source, str) or not source:
        raise ValueError(f"{kind}: source PNG path is required")
    source_path = path.parent / source
    if "sha256" in specification:
        expected = specification["sha256"]
        if not isinstance(expected, str) or not re.fullmatch("[0-9a-f]{64}", expected):
            raise ValueError(f"{kind}: sha256 must be 64 lowercase hexadecimal digits")
        if hashlib.sha256(source_path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"{kind}: source SHA-256 differs from the pinned image")
    fit = specification.get("fit", "contain")
    if fit not in ("contain", "cover", "header-left"):
        raise ValueError(f"{kind}: unsupported fit")
    if fit == "header-left" and (kind != "header" or banner_only):
        raise ValueError("header-left fits a full theme's menu header")
    filter_name = specification.get("filter", "nearest")
    if filter_name not in ("nearest", "lanczos"):
        raise ValueError(f"{kind}: filter must be nearest or lanczos")
    filtering = Image.Resampling.LANCZOS if preview or filter_name == "lanczos" else Image.Resampling.NEAREST
    dither = specification.get("dither", preview)
    if type(dither) is not bool:
        raise ValueError(f"{kind}: dither must be true or false")
    matte = checked_color(specification.get("matte", matte))
    brightness = specification.get("brightness", 1.0)
    if type(brightness) not in (int, float) or not 0 <= brightness <= 1:
        raise ValueError(f"{kind}: brightness must be between 0 and 1")
    with Image.open(source_path) as opened:
        if opened.format != "PNG":
            raise ValueError(f"{kind}: source must be a PNG")
        if opened.width * opened.height > IMAGE_PIXELS_MAX:
            raise ValueError(f"{kind}: source exceeds {IMAGE_PIXELS_MAX} pixels")
        image = opened.convert("RGBA")
    original = image if "logo_detail" in specification else None
    if "crop" in specification:
        image = image.crop(checked_rect(specification["crop"], image.size, "crop"))
    target = (THUMB_DIMENSIONS if preview else DIMENSIONS)[kind]
    area = (target[0] // 2, target[1]) if banner_only else target
    if fit == "header-left":
        image = header_left(image, target, filtering)
    elif fit == "contain":
        image = ImageOps.contain(image, area, method=filtering)
    else:
        image = ImageOps.fit(image, area, method=filtering)
    canvas = Image.new("RGBA", target, matte)
    left, top = (area[0] - image.width) // 2, (area[1] - image.height) // 2
    if fit == "header-left":
        left = 0
    canvas.alpha_composite(image, (left, top))
    artwork_box = (left, top, left + image.width, top + image.height)
    image = canvas.convert("RGB")
    if brightness != 1:
        image = ImageEnhance.Brightness(image).enhance(brightness)
    if "credit_at" in specification or "logo_detail" in specification:
        if kind != "header" or not banner_only or preview:
            raise ValueError("Native lettering is supported only on standalone banner artwork")
        compose_banner_lettering(image, original, specification, author, matte)
        artwork_box = (0, 0, area[0], area[1])
    result = bytearray(target[0] * target[1] * 2)
    if dither:
        # Keep letterboxing and the solid status panel a single, stable color.
        matte_rgb = tuple(int(matte[i:i + 2], 16) for i in (1, 3, 5))
        r, g, b = (int(c * brightness) for c in matte_rgb)
        result[:] = struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) * (target[0] * target[1])
        artwork = image.crop(artwork_box)
        red, green, blue = (dither_channel(band, levels) for band, levels in zip(artwork.split(), (31, 63, 31)))
        for y in range(artwork.height):
            at = y * artwork.width
            dest = ((y + top) * target[0] + left) * 2
            for x in range(artwork.width):
                color = (red[at + x] << 11) | (green[at + x] << 5) | blue[at + x]
                struct.pack_into("<H", result, dest + 2 * x, color)
        return bytes(result)
    rgb = image.tobytes()
    for i in range(target[0] * target[1]):
        r, g, b = rgb[i * 3:i * 3 + 3]
        struct.pack_into("<H", result, i * 2, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return bytes(result)


def encode_image(payload: bytes, kind: str) -> bytes:
    width, height = DIMENSIONS[kind]
    if len(payload) != width * height * 2:
        raise ValueError(f"{kind}: incorrect RGB565 payload length")
    return HEADER.pack(MAGIC, 1, width, height, len(payload), zlib.crc32(payload), b"\0" * 8) + payload


def verify_image(data: bytes, kind: str) -> dict:
    if len(data) < HEADER.size:
        raise ValueError(f"{kind}: truncated image header")
    magic, version, width, height, size, checksum, reserved = HEADER.unpack_from(data)
    if magic != MAGIC or version != 1 or reserved != b"\0" * 8:
        raise ValueError(f"{kind}: invalid image header")
    if (width, height) != DIMENSIONS[kind] or size != width * height * 2:
        raise ValueError(f"{kind}: incorrect image dimensions or payload size")
    if len(data) != HEADER.size + size:
        raise ValueError(f"{kind}: truncated image or trailing bytes")
    if zlib.crc32(data[HEADER.size:]) != checksum:
        raise ValueError(f"{kind}: image CRC mismatch")
    return {"width": width, "height": height, "pixel_bytes": size, "crc32": f"{checksum:08x}",
            "sha256": hashlib.sha256(data).hexdigest()}


def thumbnail_dimensions(kind: str, banner_only: bool) -> tuple[int, int]:
    if kind == "header" and banner_only:
        return THUMB_DIMENSIONS["header"][0], DIMENSIONS["header"][1]
    return THUMB_DIMENSIONS[kind]


def encode_thumbnail(payload: bytes, kind: str, source_crc: int, banner_only: bool = False) -> bytes:
    width, height = thumbnail_dimensions(kind, banner_only)
    if len(payload) != width * height * 2:
        raise ValueError(f"{kind}: incorrect thumbnail size")
    return THUMB_HEADER.pack(THUMB_MAGIC, 1, width, height, len(payload), zlib.crc32(payload),
                             source_crc, 0) + payload


def verify_thumbnail(data: bytes, kind: str, source_crc: int, banner_only: bool = False) -> None:
    width, height = thumbnail_dimensions(kind, banner_only)
    size = width * height * 2
    if len(data) != THUMB_HEADER.size + size:
        raise ValueError(f"{kind}: incorrect thumbnail size")
    expected = (THUMB_MAGIC, 1, width, height, size, zlib.crc32(data[THUMB_HEADER.size:]), source_crc, 0)
    if THUMB_HEADER.unpack_from(data) != expected:
        raise ValueError(f"{kind}: malformed, damaged or stale thumbnail")


def manifest_text(definition: dict) -> bytes:
    lines = ["[theme]", "version=1", "name=" + definition["name"]]
    if "type" in definition:
        lines.append("type=" + definition["type"])
    if "author" in definition:
        lines.append("author=" + definition["author"])
    for kind in DIMENSIONS:
        if kind in definition.get("assets", {}):
            value = definition["assets"][kind]
            lines.append(f"{kind}=" + ("none" if value == "none" else f"{kind}.kuiimg"))
    colors = definition.get("colors", {})
    if colors:
        lines += ["", "[colors]"]
        lines += [f"{key}={checked_color(colors[key])}" for key in COLOR_KEYS if key in colors]
    result = ("\n".join(lines) + "\n").encode("ascii")
    if len(result) > MANIFEST_MAX:
        raise ValueError("Manifest exceeds runtime size limit")
    return result


def prepare(source: Path) -> dict[str, bytes]:
    definition = source_definition(source)
    files = {}
    for kind, spec in definition.get("assets", {}).items():
        if spec == "none":
            continue
        matte = definition.get("colors", {}).get("background", "#080F23")
        banner = definition.get("type") == "banner"
        pixels = image_pixels(source, spec, kind, matte, banner_only=banner, author=definition.get("author", ""))
        data = encode_image(pixels, kind)
        verify_image(data, kind)
        files[f"{kind}.kuiimg"] = data
        if banner:
            # The right half is reserved for live menu status. Show the
            # artwork pixel for pixel at its actual on-console size.
            stride = DIMENSIONS["header"][0] * 2
            thumbnail = b"".join(pixels[row:row + stride // 2] for row in range(0, len(pixels), stride))
        else:
            thumbnail = image_pixels(source, spec, kind, matte, preview=True)
        files[f"{kind}.kuiimg.thumb"] = encode_thumbnail(thumbnail, kind, zlib.crc32(pixels), banner)
    files["theme.ini"] = manifest_text(definition)
    return files


def parse_manifest(data: bytes) -> dict:
    if not data or len(data) > MANIFEST_MAX:
        raise ValueError("Manifest is empty or larger than 4096 bytes")
    try:
        text = data.decode("ascii")
    except UnicodeError as error:
        raise ValueError("Manifest must contain ASCII text") from error
    if any((ord(c) < 32 and c not in "\t\r\n") or ord(c) > 126 for c in text):
        raise ValueError("Manifest contains a control character")
    # Match the small runtime parser, including CRLF line bounds and exact
    # section names. General-purpose INI readers accept extra syntax it cannot.
    sections = {"theme": {}, "colors": {}}
    section = None
    for raw in text.split("\n"):
        if len(raw) > 191:
            raise ValueError("Manifest line exceeds 191 bytes")
        line = raw.strip(" \t\r")
        if not line or line[0] in ";#":
            continue
        if line.startswith("["):
            if line not in ("[theme]", "[colors]"):
                raise ValueError("Unknown manifest section")
            section = sections[line[1:-1]]
            continue
        key, equals, value = line.partition("=")
        key, value = key.strip(" \t\r"), value.strip(" \t\r")
        if section is None or not equals or key in section:
            raise ValueError("Manifest key needs a section and cannot repeat")
        section[key] = value
    theme = sections["theme"]
    if set(theme) - {"version", "name", "author", "type", *DIMENSIONS} or theme.get("version") != "1":
        raise ValueError("Unknown manifest key or unsupported version")
    if "name" in theme:
        checked_name(theme["name"], "Theme name")
    if "author" in theme:
        checked_name(theme["author"], "Theme author")
    colors = sections["colors"]
    if set(colors) - set(COLOR_KEYS):
        raise ValueError("Unknown palette role")
    for value in colors.values():
        checked_color(value)
    for kind in DIMENSIONS:
        if kind in theme:
            checked_filename(theme[kind])
    check_pack_type(theme, {kind: theme[kind] for kind in DIMENSIONS if kind in theme})
    return theme


def verify_folder(folder: Path) -> dict:
    checked_id(folder.name)
    theme = parse_manifest((folder / "theme.ini").read_bytes())
    result = {"id": folder.name, "name": theme.get("name", folder.name), "assets": {}}
    for kind in DIMENSIONS:
        name = theme.get(kind, "")
        if not name or name == "none":
            continue
        asset = folder / name
        expected_size = HEADER.size + DIMENSIONS[kind][0] * DIMENSIONS[kind][1] * 2
        if asset.stat().st_size != expected_size:
            raise ValueError(f"{kind}: file has incorrect size")
        result["assets"][kind] = verify_image(asset.read_bytes(), kind)
        thumb = folder / (name + ".thumb")
        if thumb.exists():
            banner = theme.get("type") == "banner"
            width, height = thumbnail_dimensions(kind, banner)
            if thumb.stat().st_size != THUMB_HEADER.size + width * height * 2:
                raise ValueError(f"{kind}: incorrect thumbnail size")
            verify_thumbnail(thumb.read_bytes(), kind, int(result["assets"][kind]["crc32"], 16), banner)
    return result


def build(source: Path, output: Path, check: bool = False) -> dict:
    checked_id(output.name)
    files = prepare(source)
    if check:
        for name, contents in files.items():
            target = output / name
            if not target.is_file() or target.read_bytes() != contents:
                raise ValueError(f"Stale or missing theme output: {target}")
    else:
        output.mkdir(parents=True, exist_ok=True)
        # Publish the manifest last. Never emit or overwrite user preferences.
        for name, contents in files.items():
            target = output / name
            temporary = output / (name + ".tmp")
            temporary.write_bytes(contents)
            temporary.replace(target)
    return verify_folder(output)


def stage_all(output: Path) -> dict:
    """Assemble every shipped pack into a new manual-install staging directory."""
    collection = Path(__file__).resolve().parents[1] / "resources" / "themes"
    packs = sorted({p.parent for name in ("source.json", "theme.ini") for p in collection.glob("*/" + name)})
    if not packs:
        raise ValueError("No bundled theme packs found")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        staged = Path(temporary) / "themes"
        for pack in packs:
            verify_folder(pack)
            manifest = parse_manifest((pack / "theme.ini").read_bytes())
            destination = staged / pack.name
            destination.mkdir(parents=True)
            shutil.copyfile(pack / "theme.ini", destination / "theme.ini")
            for kind in DIMENSIONS:
                name = manifest.get(kind)
                if name and name != "none":
                    shutil.copyfile(pack / name, destination / name)
                    thumb = pack / (name + ".thumb")
                    if thumb.is_file():
                        shutil.copyfile(thumb, destination / thumb.name)
        staged.rename(output)
    return {"packs": [p.name for p in packs]}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    make = commands.add_parser("build", help="convert a source JSON definition and PNGs")
    make.add_argument("source", type=Path)
    make.add_argument("output", type=Path)
    make.add_argument("--check", action="store_true", help="compare prepared output without rewriting")
    verify = commands.add_parser("verify", help="check manifest, dimensions, lengths and payload CRCs")
    verify.add_argument("folder", type=Path)
    stage = commands.add_parser("stage-all", help="stage every bundled pack in a new KUI/themes directory")
    stage.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "stage-all":
            result = stage_all(args.output)
        elif args.command == "build":
            result = build(args.source, args.output, args.check)
        else:
            result = verify_folder(args.folder)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, ImportError) as error:
        print(f"Theme preparation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
