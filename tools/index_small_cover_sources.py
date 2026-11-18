#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Match published Dreamcast box-front JPEG derivatives to the existing release index."""
import argparse
import csv
from concurrent.futures import ThreadPoolExecutor
from collections import defaultdict
from hashlib import sha256
import json
from pathlib import Path
import re
import urllib.request

from build_internet_cover_catalog import box_group, release_fingerprint

ROOT = Path(__file__).resolve().parents[1]
ITEM = "advanceddaisenryakusturmubereuropajapan"
THUMB_SUFFIX = "_thumb.jpg"
JEWEL_SNAPSHOT = "d7a342095a26f95cca3dadfe904c0865ac671c7a27e9f741834aceb2bcaed22b"


def metadata(item):
    with urllib.request.urlopen("https://archive.org/metadata/" + item, timeout=40) as reply:
        return json.load(reply)


def jpeg_entries(data):
    files = {f["name"]: f for f in data["files"]}
    for name, f in files.items():
        original = files.get(f.get("original"))
        if (f["format"] == "JPEG Thumb" and f.get("source") == "derivative"
                and original and original.get("source") == "original"):
            yield name, original, int(f["size"]), f["sha1"]


def jewel_covers(data):
    rows = sorted([name, original["name"], size, digest]
                  for name, original, size, digest in jpeg_entries(data)
                  if original["format"] == "PNG" and "/" not in name
                  and name == original["name"][:-4] + THUMB_SUFFIX)
    serialized = json.dumps(rows, ensure_ascii=True, separators=(",", ":"))
    if sha256(serialized.encode()).hexdigest() != JEWEL_SNAPSHOT:
        raise ValueError("Jewel-case thumbnail inventory changed; review before updating the pin")
    return {name[:-len(THUMB_SUFFIX)]: digest for name, _, _, digest in rows}


def verify_scan(row):
    files = {f["name"]: f for f in metadata(row["item"])["files"]}
    name = row["filename"]
    f = files.get(name, {})
    if name == "__ia_thumb.jpg":
        kind_ok = f.get("format") == "Item Tile" and f.get("source") == "original"
    else:
        kind_ok = (f.get("format") == "JPEG Thumb" and f.get("source") == "derivative"
                   and f.get("original") in files)
    if (not kind_ok or f.get("size") != row["bytes"]
            or f.get("sha1") != row["sha1"] or f.get("crc32") != row["crc32"]):
        raise ValueError("Retail front scan changed: " + row["release"])


def specific_scans(tsv, verify):
    scans = {}
    with tsv.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    for row in rows:
        release, item, filename = row["release"], row["item"], row["filename"]
        if (release in scans or not re.fullmatch(r"[A-Za-z0-9_.-]+", item)
                or "/" in filename or not filename.endswith(THUMB_SUFFIX)
                or not re.fullmatch(r"[0-9a-f]{40}", row["sha1"])
                or not re.fullmatch(r"[0-9a-f]{8}", row["crc32"])
                or not 0 < int(row["bytes"]) <= 128 * 1024):
            raise ValueError("Invalid or duplicate retail scan: " + release)
        scans[release] = row
    if verify:
        with ThreadPoolExecutor(max_workers=3) as workers:
            list(workers.map(verify_scan, rows))
    return scans


def generate(releases, thumbs, scans):
    groups = defaultdict(list)
    for name in sorted(thumbs):
        groups[box_group(name)].append(name)
    unknown = scans.keys() - set(releases)
    if unknown:
        raise ValueError("Unknown Dreamcast release(s): " + ", ".join(sorted(unknown)))
    lines = [
        "/* SPDX-License-Identifier: CC-BY-SA-4.0",
        " * Source associations only. See docs/internet-covers.md. */",
        '#define KUI_JEWEL_CASE_ITEM "' + ITEM + '"',
        '_Static_assert(KUI_BOXART_RELEASES == UINT64_C(0x' + release_fingerprint(releases) +
        '), "Regenerate the small cover index after changing release names");',
        "static const struct small_cover small_covers[] = {",
    ]
    count = 0
    tiles = []
    for i, name in enumerate(releases):
        item = None
        if name in scans:
            row = scans[name]
            item, stem = row["item"], row["filename"][:-len(THUMB_SUFFIX)]
            if row["filename"] == "__ia_thumb.jpg":
                if int(row["bytes"]) > 65535:
                    raise ValueError("Item tile exceeds the compact signature size")
                tiles.append((i, int(row["bytes"]), row["crc32"]))
        elif name in thumbs:
            stem = name
        else:
            variants = groups.get(box_group(name), [])
            # Multiple discs may repeat the same box. If they have different
            # images, leave the selection to Libretro's original catalog.
            if not variants or len({thumbs[n] for n in variants}) != 1:
                continue
            stem = variants[0]
        lines.append("    {%d, %s, %s}," % (i, json.dumps(item) if item else "NULL",
                    json.dumps(stem) if stem != name else "NULL"))
        count += 1
    lines.extend(["};", "static const struct cover_tile cover_tiles[] = {"])
    for i, length, checksum in tiles:
        lines.append(f"    {{{i}, {length}, UINT32_C(0x{checksum})}},")
    return "\n".join(lines + ["};", ""]), count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jewel-metadata", type=Path,
                        help="Use a saved archive.org/metadata response")
    parser.add_argument("--verify-items", action="store_true",
                        help="Recheck individual front-scan metadata at their sources")
    parser.add_argument("--output", default=ROOT / "src/apps/internet_cover_small.inc", type=Path)
    args = parser.parse_args()
    data = json.loads(args.jewel_metadata.read_text()) if args.jewel_metadata else metadata(ITEM)
    catalog = (ROOT / "src/apps/internet_cover_catalog.inc").read_text()
    releases = [json.loads(m.group(1)) for m in re.finditer(
        r'^    \{("(?:\\.|[^"\\])+"), \d+, \d+, [01]\},$', catalog, re.M)]
    if not releases or releases != sorted(set(releases)):
        raise ValueError("Unexpected Dreamcast release index")
    scans = specific_scans(ROOT / "resources/game-media/cover-scans.tsv", args.verify_items)
    result, count = generate(releases, jewel_covers(data), scans)
    args.output.write_text(result)
    print(f"{count} matched thumbnail releases including {len(scans)} individually identified front scans")


if __name__ == "__main__":
    main()
