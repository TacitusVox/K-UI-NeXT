#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare VGA-only companion GDIs for the K-UI Games launcher.

Install a publisher-patched GDI, apply a DCP using the rom-weaver CLI, or
match a folder of VGA DCPs to original GDIs by their disc headers. Originals
and previously installed companions are preserved.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from zipfile import BadZipFile, ZIP_DEFLATED, ZipFile

sys.dont_write_bytecode = True  # Running from the card must not leave a Python cache there.

from gdi_optimize import SYNC, file_signature, load_gdi, no_symlink_directories, publish_directory, require, safe_name


def ip_field(value):
    # Match the console's game_metadata.c field reader, including NUL padding.
    return bytes(c if 32 <= c <= 126 else ord("?") for c in value.split(b"\0", 1)[0]).strip(b" ")


def disc_key(ip):
    require(len(ip) >= 256 and ip[:16] == b"SEGA SEGAKATANA ", "Missing Dreamcast disc header")
    member = re.fullmatch(rb" (GD|CD)-ROM([1-9])/([1-9])  ", ip[0x24:0x30])
    require(member and member[2] <= member[3], "Invalid Dreamcast disc member")
    flags = ip[0x38:0x3f]
    require(re.fullmatch(rb"[0-9A-Fa-f]{7}", flags) and ip[0x3f] == 32, "Invalid peripheral flags")
    wince = bool(int(flags, 16) & 1)
    require(member[1] == b"GD" or wince, "VGA companions require native GD or Windows CE")
    product, revision = ip_field(ip[0x40:0x4a]), ip_field(ip[0x4a:0x50])
    date, boot = ip[0x50:0x58], ip_field(ip[0x60:0x70])
    require(product and revision and date.isdigit(), "Incomplete Dreamcast release identity")
    require(boot not in (b".", b"..") and re.fullmatch(rb"[A-Za-z0-9_.]+", boot), "Invalid boot filename")
    areas = frozenset(ip_field(ip[0x30:0x38]).replace(b" ", b""))
    require(areas and areas <= set(b"JUE"), "Invalid Dreamcast region flags")
    return (product, revision, date, boot, member[2], member[3], wince), areas


def compatible(original, other):
    return original[0] == other[0] and original[1] <= other[1]


def check_boot_iso(tracks, boot_track):
    # Read only the IP, primary volume descriptor and bounded root directory.
    # Directory extents in Dreamcast GD images are absolute logical sector numbers.
    def data_track(lba):
        track = next((t for t in tracks if t.start_lba <= lba < t.start_lba + t.sectors), None)
        require(track and track.control == 4, f"ISO extent outside a data track at LBA {lba}")
        return track

    def sector(lba):
        t = data_track(lba)
        with t.path.open("rb") as f:
            f.seek((lba - t.start_lba) * t.sector_bytes)
            if t.sector_bytes == 2352:
                header = f.read(16)
                require(len(header) == 16 and header[:12] == SYNC and header[15] == 1,
                        f"Invalid Mode 1 sector at LBA {lba}")
            block = f.read(2048)
        require(len(block) == 2048, f"Incomplete ISO sector at LBA {lba}")
        return block

    def both(data, at, size):
        first = int.from_bytes(data[at:at + size], "little")
        require(first == int.from_bytes(data[at + size:at + size * 2], "big"), "Invalid ISO extent fields")
        return first

    def record(data):
        require(len(data) >= 34 and data[0] == len(data) and len(data) % 2 == 0 and data[32] and
                33 + data[32] + (1 - data[32] % 2) <= len(data) and both(data, 28, 2) == 1,
                "Malformed ISO directory record")
        return both(data, 2, 4), both(data, 10, 4), data[33:33 + data[32]]

    def check_extent(lba, size, limit):
        count = (size + 2047) // 2048
        require(0 < size <= limit and count <= volume, "ISO extent size out of range")
        end = lba + count
        while lba < end:
            t = data_track(lba)
            lba = t.start_lba + t.sectors

    key = disc_key(sector(boot_track.start_lba))
    boot = key[0][3].upper()
    for offset in range(16, 32):
        pvd = sector(boot_track.start_lba + offset)
        require(pvd[1:7] == b"CD001\1" and pvd[0] <= 3, "Invalid ISO volume descriptor")
        if pvd[0] == 1:
            break
    else:
        raise ValueError("Missing ISO primary volume descriptor")
    volume = both(pvd, 80, 4)
    require(not pvd[7] and pvd[881] == 1 and volume > offset and
            both(pvd, 120, 2) == both(pvd, 124, 2) == 1 and both(pvd, 128, 2) == 2048,
            "Unsupported ISO primary volume descriptor")
    root = pvd[156:190]
    start, length, identifier = record(root)
    require(identifier == b"\0" and root[25] & 2 and not root[25] & 0xfc and
            not (root[1] or root[26] or root[27]), "Invalid ISO root directory")
    check_extent(start, length, 256 * 1024)
    found = False
    for off in range(0, length, 2048):
        block = sector(start + off // 2048)[:length - off]
        at = 0
        while at < len(block) and block[at]:
            entry = block[at:at + block[at]]
            lba, size, name = record(entry)
            if name.upper() in (boot, boot + b";1"):
                require(not found and not (entry[1] or entry[25] & 0xfe or entry[26] or entry[27]),
                        "Duplicate or unsupported boot extent")
                check_extent(lba, size, 16 * 1024 * 1024)
                found = True
            at += len(entry)
        require(not any(block[at:]), "Malformed ISO directory padding")
    require(found, f"Boot file missing from ISO root: {boot.decode()}")
    return key


def device_lines(data):
    lines = data.replace(b"\r\n", b"\n").split(b"\n")
    if lines[-1] == b"":
        lines.pop()
    return all(line.strip(b" \t") and b"\r" not in line for line in lines)


def fat_name(name):
    stem = name.split(".", 1)[0].upper()
    return safe_name(name) and stem not in ("CON", "PRN", "AUX", "NUL") and not re.fullmatch(r"(COM|LPT)[1-9]", stem)


def gdi_source(path, original=False):
    require(path.suffix.lower() == ".gdi", f"Expected a GDI descriptor: {path}")
    require(not original or fat_name(path.name), f"Invalid Games source filename: {path.name}")
    data, signature, tracks = load_gdi(path)
    require(all(t.descriptor_name.isascii() and fat_name(t.descriptor_name) for t in tracks),
            "GDI track names must be ASCII, FAT-safe filenames")
    require(not original or device_lines(data), "Original descriptor contains unsupported blank lines or separators")
    if not device_lines(data):
        rows = [str(len(tracks))] + [f'{t.number} {t.start_lba} {t.control} {t.sector_bytes} "{t.descriptor_name}" 0'
                                     for t in tracks]
        data = ("\n".join(rows) + "\n").encode("ascii")
    boot = next((t for t in tracks if t.control == 4 and t.start_lba >= 45000), None)
    require(boot is not None, f"Missing high-density boot track: {path}")
    return check_boot_iso(tracks, boot), data, signature, tracks


def dcp_key(path):
    with ZipFile(path) as z:
        headers = [i for i in z.infolist() if i.filename.replace("\\", "/").casefold() == "bootsector/ip.bin"]
        if not headers:
            return None
        require(len(headers) == 1 and headers[0].file_size == 32768, f"Invalid DCP boot header: {path}")
        with z.open(headers[0]) as stream:
            return disc_key(stream.read())


def patch_for_backend(path, work):
    """rom-weaver 0.25.1 requires uncompressed VCDIFF sections in DCP files."""
    with ZipFile(path) as src:
        compressed = set()
        members = set()
        for item in src.infolist():
            name = item.filename.replace("\\", "/").rstrip("/")
            parts = name.split("/")
            require(all(part not in ("", ".", "..") and ":" not in part for part in parts)
                    and name.casefold() not in members, f"Invalid or duplicate DCP path: {name}")
            members.add(name.casefold())
            if not item.is_dir() and name.casefold().endswith(".xdelta"):
                with src.open(item) as f:
                    header = f.read(5)
                require(len(header) == 5 and header[:4] == bytes.fromhex("d6c3c400"),
                        f"Invalid DCP delta: {name}")
                if header[4] & 1:
                    compressed.add(item.filename)
        if not compressed:
            return path
        xdelta = shutil.which("xdelta3")
        require(xdelta, "This DCP uses secondary compression. Install xdelta3 or use --prepared.")
        # Recoding changes only the encoding of the patch's sections. Copy every
        # other member unchanged, preserving additions and the publisher's IP.BIN.
        converted = work / "normalized.dcp"
        raw, recoded = work / "input.delta", work / "output.delta"
        with ZipFile(converted, "w", compression=ZIP_DEFLATED) as dst:
            for item in src.infolist():
                if item.filename in compressed:
                    with src.open(item) as source, raw.open("wb") as dest:
                        shutil.copyfileobj(source, dest)
                    subprocess.run([xdelta, "recode", "-f", "-S", "none", str(raw), str(recoded)], check=True)
                    dst.write(recoded, item.filename)
                else:
                    with src.open(item) as source, dst.open(item.filename, "w") as dest:
                        shutil.copyfileobj(source, dest)
        return converted


def install(original, source, patch, patcher):
    require(".kui-vga" not in (p.casefold() for p in original.parts), "Select the original disc")
    parent = original.parent / ".kui-vga"
    destination = parent / original.name
    no_symlink_directories(parent)
    key, _, original_signature, original_tracks = gdi_source(original, original=True)
    require(not os.path.lexists(destination), f"Companion already exists: {destination}")
    if patch:
        identity = dcp_key(patch)
        require(identity is None or compatible(key, identity), f"DCP is for a different disc: {patch}")
        require(shutil.which(patcher), f"Cannot find {patcher}. Install rom-weaver or use --prepared.")
    with tempfile.TemporaryDirectory(prefix=".kui-vga-prepare-", dir=original.parent) as temporary:
        work = Path(temporary)
        if patch:
            rendered = work / "rendered"
            rendered.mkdir()
            source = rendered / "disc.gdi"
            patch = patch_for_backend(patch, work)
            subprocess.run([patcher, "patch", "apply", "--input", str(original),
                            "--patch", str(patch), "--output", str(source), "--no-compress"], check=True)
        require(not original.samefile(source), "Select the patched GDI as the companion")
        other, descriptor, signature, tracks = gdi_source(source)
        require(compatible(key, other), "Patched output is for a different release or disc")
        require(all(t.descriptor_name.casefold() != "disc.gdi" for t in tracks), "GDI/track filename collision")
        staged = work / "complete"
        staged.mkdir()
        for t in tracks:
            target = staged / t.descriptor_name
            if patch and t.path.parent == source.parent:
                t.path.rename(target)
            else:
                shutil.copyfile(t.path, target)
                require(file_signature(t.path) == t.signature, f"Input track changed during copy: {t.path}")
        require(file_signature(source) == signature, "Input descriptor changed during preparation")
        (staged / "disc.gdi").write_bytes(descriptor)
        require(gdi_source(staged / "disc.gdi")[0] == other, "Invalid staged disc")
        require(file_signature(original) == original_signature and
                all(file_signature(t.path) == t.signature for t in original_tracks),
                "Original changed during preparation")
        parent.mkdir(exist_ok=True)
        no_symlink_directories(parent)
        publish_directory(staged, destination)
    return destination / "disc.gdi"


def walk_files(root, suffix):
    for folder, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if not d.startswith(".") and not (Path(folder)/d).is_symlink())
        for name in sorted(files):
            path = Path(folder) / name
            if not name.startswith(".") and path.suffix.lower() == suffix and not path.is_symlink():
                yield path


def batch(games, patches, patcher, preview):
    require(games.is_dir() and patches.is_dir(), "Supply a Games folder and a folder of VGA DCP files")
    index = {}
    warnings = 0
    for patch in walk_files(patches, ".dcp"):
        try:
            key = dcp_key(patch)
            if key:
                index.setdefault(key[0], []).append((key, patch))
            else:
                print(f"Manual selection required (DCP has no disc header): {patch}", file=sys.stderr)
        except (OSError, ValueError, BadZipFile) as exc:
            warnings += 1
            print(f"Skipping {patch}: {exc}", file=sys.stderr)
    matched = 0
    for original in walk_files(games, ".gdi"):
        try:
            key = gdi_source(original, original=True)[0]
            options = [p for k,p in index.get(key[0], ()) if compatible(key,k)]
            if not options:
                continue
            matched += 1
            if len(options) != 1:
                raise ValueError("multiple DCPs match, choose one explicitly with --dcp")
            if os.path.lexists(original.parent/".kui-vga"/original.name):
                print(f"Already staged: {original}")
            elif preview:
                print(f"{original} <- {options[0]}")
            else:
                print(install(original, None, options[0], patcher))
        except (OSError, ValueError, BadZipFile, subprocess.CalledProcessError) as exc:
            warnings += 1
            print(f"Skipping {original}: {exc}", file=sys.stderr)
    print(f"Matched {matched} original GDI(s).")
    return 1 if warnings else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("original", type=Path, help="Original GDI, or the Games directory with --patch-dir")
    action = ap.add_mutually_exclusive_group(required=True)
    action.add_argument("--prepared", type=Path, help="GDI already patched using the author's instructions")
    action.add_argument("--dcp", type=Path, help="Apply one DCP to the specified original")
    action.add_argument("--patch-dir", type=Path, help="Match a directory of unpacked VGA .dcp files")
    ap.add_argument("--dry-run", action="store_true", help="Show batch matches without writing any files")
    ap.add_argument("--patcher", default="rom-weaver", help="Path to the rom-weaver CLI for applying DCP files")
    args = ap.parse_args()
    original = args.original.absolute()
    try:
        if args.patch_dir:
            return batch(original, args.patch_dir.absolute(), args.patcher, args.dry_run)
        if args.dry_run:
            ap.error("--dry-run is for batch matching with --patch-dir")
        result = install(original, args.prepared and args.prepared.absolute(),
                         args.dcp and args.dcp.absolute(), args.patcher)
        print(result)
        return 0
    except (OSError, ValueError, BadZipFile, subprocess.CalledProcessError) as exc:
        ap.exit(1, f"{ap.prog}: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
