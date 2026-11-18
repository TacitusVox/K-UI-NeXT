#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare and verify K-UI theme folders; image conversion happens on the PC.

    python3 tools/pack_theme.py build source.json output/theme-id
    python3 tools/pack_theme.py build source.json output/theme-id --check
    python3 tools/pack_theme.py verify output/theme-id
    python3 tools/pack_theme.py stage-all staging/KUI/themes

Build requires Pillow. Verification uses only the Python standard library.
The Dreamcast consumes theme.ini and .kuiimg files, never source.json or PNGs.
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


def image_pixels(path: Path, specification: object, kind: str, matte: str, banner_only: bool = False) -> bytes:
    # Import here: verify does not require image tooling on the installed system.
    from PIL import Image, ImageEnhance, ImageOps

    if not isinstance(specification, dict):
        raise ValueError(f"{kind}: asset must be an object or the string 'none'")
    unknown = set(specification) - {"source", "sha256", "crop", "fit", "filter", "matte", "brightness"}
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
    if fit not in ("contain", "cover"):
        raise ValueError(f"{kind}: fit must be contain or cover")
    filter_name = specification.get("filter", "nearest")
    if filter_name not in ("nearest", "lanczos"):
        raise ValueError(f"{kind}: filter must be nearest or lanczos")
    filtering = Image.Resampling.NEAREST if filter_name == "nearest" else Image.Resampling.LANCZOS
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
    if "crop" in specification:
        crop = specification["crop"]
        if (not isinstance(crop, list) or len(crop) != 4 or
                any(type(v) is not int for v in crop) or
                not (0 <= crop[0] < crop[2] <= image.width and 0 <= crop[1] < crop[3] <= image.height)):
            raise ValueError(f"{kind}: crop must be [left, top, right, bottom] inside the source PNG")
        image = image.crop(crop)
    target = DIMENSIONS[kind]
    area = (target[0] // 2, target[1]) if banner_only else target
    image = (ImageOps.contain(image, area, method=filtering) if fit == "contain" else
             ImageOps.fit(image, area, method=filtering))
    canvas = Image.new("RGBA", target, matte)
    canvas.alpha_composite(image, ((area[0] - image.width) // 2, (area[1] - image.height) // 2))
    image = canvas.convert("RGB")
    if brightness != 1:
        image = ImageEnhance.Brightness(image).enhance(brightness)
    rgb = image.tobytes()
    result = bytearray(target[0] * target[1] * 2)
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
        pixels = image_pixels(source, spec, kind, definition.get("colors", {}).get("background", "#080F23"),
                              banner_only=definition.get("type") == "banner")
        data = encode_image(pixels, kind)
        verify_image(data, kind)
        files[f"{kind}.kuiimg"] = data
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
