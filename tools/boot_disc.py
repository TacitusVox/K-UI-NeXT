#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the K-UI Bootloader boot disc (MIL-CD .cdi) with a redundant layout.

The console BIOS reads only IP.BIN and 1ST_READ.BIN. Here 1ST_READ.BIN is
stage 1 (src/bootstage, about 2 KB), and the CD bootstrap itself is stored as
KUIBOOT1.BIN, KUIBOOT2.BIN and KUIBOOT3.BIN at three radii of the disc,
separated by zero-filled spacer files. Stage 1 reads each 32 KiB chunk from
the nearest copy whose CRC-32 matches the table this tool writes into it, and
rebuilds a chunk sector by sector when every copy is damaged there.

Placement is decided by mkdcdisc's sort file. The first pass builds the disc
with an empty table to learn where the copies landed; the table is written
into stage 1 and the disc is built again with an identical layout. The final
CDI is then read back: file positions, every copy's bytes, the descrambled
1ST_READ.BIN and the boot badge are all checked against what was intended.

    tools/boot_disc.py --stage1 build/stage1/stage1.bin \\
        --bootstrap kui-diagnostic.bin --output K-UI-Bootloader-v3.cdi
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

MAGIC = b"KUI-BOOT-COPIES\0"
VERSION = 1
CHUNK_SECTORS = 16
MAX_COPIES = 4
MAX_CHUNKS = 256
SECTOR = 2048
CDI_SECTOR = 2336          # mkdcdisc stores the data track as 2336-byte Mode 2 sectors
CDI_USER = 8               # 8-byte subheader before the 2048 user bytes
TABLE_FORMAT = "<16s5I4II%dII" % MAX_CHUNKS
TABLE_BYTES = struct.calcsize(TABLE_FORMAT)
COPY_NAMES = ("KUIBOOT1.BIN", "KUIBOOT2.BIN", "KUIBOOT3.BIN")
SPACER_NAMES = ("KUIPAD1.BIN", "KUIPAD2.BIN")
STAGE1_LIMIT = 0x8000      # src/bootstage/stage1.ld


def build_table(image, fads):
    """The struct kui_boot_copies stage 1 validates (include/kui/boot_copies.h)."""
    if not image or not 1 <= len(fads) <= MAX_COPIES:
        raise ValueError("need an image and 1..4 copies")
    chunk = CHUNK_SECTORS * SECTOR
    count = -(-len(image) // chunk)
    if count > MAX_CHUNKS:
        raise ValueError("bootstrap larger than %d bytes" % (MAX_CHUNKS * chunk))
    crcs = [zlib.crc32(image[i * chunk:(i + 1) * chunk]) for i in range(count)]
    crcs += [0] * (MAX_CHUNKS - count)
    padded = list(fads) + [0] * (MAX_COPIES - len(fads))
    body = struct.pack(TABLE_FORMAT[:-1], MAGIC, VERSION, len(image), CHUNK_SECTORS, count,
                       len(fads), *padded, zlib.crc32(image), *crcs)
    return body + struct.pack("<I", zlib.crc32(body))


def patch_stage1(stage1, table):
    """Write TABLE over the empty placeholder: the magic followed by zeros.

    The magic also appears in stage 1's own validator constant, which is
    followed by other data, so the whole empty placeholder must be unique."""
    if len(table) != TABLE_BYTES:
        raise ValueError("table does not match struct kui_boot_copies")
    placeholder = MAGIC + bytes(TABLE_BYTES - len(MAGIC))
    at = stage1.find(placeholder)
    if at < 0 or stage1.find(placeholder, at + 1) >= 0 or at % 4:
        raise ValueError("stage 1 must contain exactly one aligned, empty table placeholder")
    return stage1[:at] + table + stage1[at + TABLE_BYTES:]


# Dreamcast MIL-CD 1ST_READ.BIN scrambling (after Marcus Comstedt's scramble.c):
# the file is cut into 2 MiB chunks, then halving windows down to 32 bytes, and
# each chunk's 32-byte slices are permuted by a 16-bit LCG seeded with the size.
def _slices(size):
    seed = size & 0xffff

    def rand():
        nonlocal seed
        seed = (seed * 2109 + 9273) & 0x7fff
        return (seed + 0xc000) & 0xffff

    out, offset, chunk, remaining = [], 0, 2048 * 1024, size
    while chunk >= 32:
        while remaining >= chunk:
            n = chunk // 32
            idx = list(range(n))
            for i in range(n - 1, -1, -1):
                x = (rand() * i) >> 16
                idx[i], idx[x] = idx[x], idx[i]
                out.append(offset + 32 * idx[i])
            remaining -= chunk
            offset += chunk
        chunk >>= 1
    return out, offset


def descramble(data):
    """Scrambled file bytes -> the program bytes the BIOS loads into RAM."""
    slices, tail = _slices(len(data))
    out = bytearray(len(data))
    for k, target in enumerate(slices):
        out[target:target + 32] = data[32 * k:32 * k + 32]
    out[tail:] = data[tail:]
    return bytes(out)


def scramble(data):
    slices, tail = _slices(len(data))
    out = bytearray(len(data))
    for k, source in enumerate(slices):
        out[32 * k:32 * k + 32] = data[source:source + 32]
    out[tail:] = data[tail:]
    return bytes(out)


def directory(read_sector, root_lba, root_bytes):
    """Root directory of an ISO9660 volume: {NAME: (lba, size)}."""
    files = {}
    data = b"".join(read_sector(root_lba + i) for i in range(-(-root_bytes // SECTOR)))
    at = 0
    while at < len(data):
        length = data[at]
        if not length:
            at = (at // SECTOR + 1) * SECTOR
            continue
        record = data[at:at + length]
        name = record[33:33 + record[32]].decode("ascii", "replace").split(";")[0]
        if not record[25] & 2:
            files[name] = (struct.unpack_from("<I", record, 2)[0], struct.unpack_from("<I", record, 10)[0])
        at += length
    return files


class CdiDataTrack:
    """Read 2048-byte user data of the CDI's data track by absolute LBA."""

    def __init__(self, cdi, start_lba):
        marker = b"\x01CD001\x01"
        at = cdi.find(marker)
        if at < 0 or cdi.find(marker, at + 1) >= 0:
            raise ValueError("expected exactly one ISO9660 descriptor in the CDI")
        self.cdi, self.start = cdi, start_lba
        self.base = at - CDI_USER - 16 * CDI_SECTOR
        pvd = self.read(start_lba + 16)
        self.root = struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0]

    def read(self, lba):
        offset = self.base + (lba - self.start) * CDI_SECTOR + CDI_USER
        if lba < self.start or offset + SECTOR > len(self.cdi):
            raise ValueError("LBA %d outside the data track" % lba)
        return self.cdi[offset:offset + SECTOR]

    def files(self):
        return directory(self.read, *self.root)

    def file(self, lba, size):
        return b"".join(self.read(lba + i) for i in range(-(-size // SECTOR)))[:size]


def mkdcdisc(tool, stage1, files, sort, output, args):
    """Run mkdcdisc at its normal verbosity. A warning means the disc is not
    the one asked for (a sort-file entry that matched nothing, say, leaves
    the copies side by side), so any warning or error refuses the disc."""
    command = [str(tool), "-b", str(stage1), "-N", "-S", str(sort), "--allow-overwrite", "-v", "1",
               "-n", args.title, "-a", args.author, "-r", args.release, "-o", str(output)]
    if args.badge:
        command += ["-i", str(args.badge)]
    for path in files:
        command += ["-f", str(path)]
    run = subprocess.run(command, capture_output=True, text=True)
    if run.returncode or re.search(r"\b(Warning|Error):", run.stdout + run.stderr):
        raise SystemExit("mkdcdisc did not build the disc cleanly; refusing it:\n" + run.stderr + run.stdout)
    return output


def flat_input(path, what):
    """The BIOS and stage 1 both start a program at its first byte, so an ELF
    file here would build a disc that verifies but cannot boot."""
    data = Path(path).read_bytes()
    if data[:4] == b"\x7fELF":
        raise SystemExit("%s %s is an ELF file; give the flat binary (sh-elf-objcopy -O binary)" % (what, path))
    if not data:
        raise SystemExit("%s %s is empty" % (what, path))
    return data


def check_layout(layout, copy_bytes, spacer_mib):
    """Stage 1 must be followed directly by the first copy (a normal boot does
    not seek), and the copies must lie in order with a full spacer between
    them. Anything else means the sort file was not applied as intended."""
    lba, size = layout["1ST_READ.BIN"]
    copies = [layout[name][0] for name in COPY_NAMES]
    if lba + -(-size // SECTOR) != copies[0]:
        raise SystemExit("%s does not follow 1ST_READ.BIN (LBA %d, %d bytes) but is at LBA %d"
                         % (COPY_NAMES[0], lba, size, copies[0]))
    need = -(-copy_bytes // SECTOR) + spacer_mib * 1024 * 1024 // SECTOR
    for (a, b) in zip(copies, copies[1:]):
        if b - a < need:
            raise SystemExit("the copies at LBA %s are not spread apart: each must start at least %d "
                             "sectors after the previous one" % (copies, need))


def start_lba(tool):
    out = subprocess.run([str(tool), "-M", "-o", "/dev/null"], check=True, capture_output=True, text=True)
    return int(out.stdout.strip().splitlines()[-1])


def build(args):
    if args.spacer_mib < 1:
        raise SystemExit("--spacer-mib must be at least 1: the copies would otherwise sit side by side")
    stage1 = flat_input(args.stage1, "stage 1")
    if len(stage1) > STAGE1_LIMIT:
        raise SystemExit("stage 1 is %d bytes; it must fit in %d" % (len(stage1), STAGE1_LIMIT))
    image = flat_input(args.bootstrap, "bootstrap")
    padded = image + b"\0" * (-len(image) % SECTOR)
    tool = Path(args.mkdcdisc)
    with tempfile.TemporaryDirectory(prefix="kui-boot-disc-") as work:
        work = Path(work)
        files = []
        for name in COPY_NAMES:
            (work / name).write_bytes(padded)
        for name in SPACER_NAMES:
            with open(work / name, "wb") as out:
                out.truncate(args.spacer_mib * 1024 * 1024)
        order = ["1ST_READ.BIN", COPY_NAMES[0], SPACER_NAMES[0], COPY_NAMES[1], SPACER_NAMES[1], COPY_NAMES[2]]
        files = [work / name for name in order[1:]]
        sort = work / "sort.txt"
        # libisofs places HIGHER weights at LOWER addresses (towards the hub).
        sort.write_text("".join("/%s %d\n" % (name, 1000 - 10 * i) for i, name in enumerate(order)))
        # Pass 1 with the empty table: where did the copies land? Read the
        # positions from the CDI itself; mkdcdisc's standalone -I dump is laid
        # out for LBA 0 and differs from the multi-session track.
        first = work / "stage1-pass1.bin"
        first.write_bytes(stage1)
        base = start_lba(tool)
        pass1 = mkdcdisc(tool, first, files, sort, work / "pass1.cdi", args)
        layout = CdiDataTrack(pass1.read_bytes(), base).files()
        check_layout(layout, len(padded), args.spacer_mib)
        fads = [layout[name][0] + 150 for name in COPY_NAMES]
        patched = patch_stage1(stage1, build_table(image, fads))
        second = work / "stage1.bin"
        second.write_bytes(patched)
        output = Path(args.output)
        mkdcdisc(tool, second, files, sort, output, args)
        layout2 = CdiDataTrack(output.read_bytes(), base).files()
        if {k: layout[k] for k in order} != {k: layout2[k] for k in order}:
            raise SystemExit("the second pass moved files; refusing the disc")
    report = verify(output.read_bytes(), base, image, patched, fads, args.badge)
    report.update({"cdi": str(output), "cdi_bytes": output.stat().st_size,
                   "cdi_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                   "stage1_sha256": hashlib.sha256(patched).hexdigest(),
                   "bootstrap_sha256": hashlib.sha256(image).hexdigest(),
                   "spacer_mib": args.spacer_mib})
    return report


def verify(cdi, base, image, stage1, fads, badge=None):
    """Read the burned layout back out of the CDI and check every part."""
    track = CdiDataTrack(cdi, base)
    files = track.files()
    padded = image + b"\0" * (-len(image) % SECTOR)
    for name, fad in zip(COPY_NAMES, fads):
        lba, size = files[name]
        if lba + 150 != fad or size != len(padded):
            raise SystemExit("%s is at FAD %d (%d bytes), table says %d" % (name, lba + 150, size, fad))
        if track.file(lba, size) != padded:
            raise SystemExit("%s does not hold the bootstrap" % name)
    lba, size = files["1ST_READ.BIN"]
    loaded = descramble(track.file(lba, size))
    if loaded[:len(stage1)] != stage1 or loaded[len(stage1):].strip(b"\0"):
        raise SystemExit("1ST_READ.BIN does not descramble to the patched stage 1")
    copies = [files[n][0] for n in COPY_NAMES]
    report = {"start_lba": base, "stage1_lba": lba, "copy_fads": fads,
              "copy_spacing_sectors": [b - a for a, b in zip(copies, copies[1:])],
              "chunks": -(-len(image) // (CHUNK_SECTORS * SECTOR))}
    if badge:
        from boot_badge import verify_cdi_badge
        report["badge"] = verify_cdi_badge(cdi, Path(badge).read_bytes())
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--stage1", required=True, help="unpatched stage-1 binary (build/stage1/stage1.bin)")
    parser.add_argument("--bootstrap", required=True, help="flat CD bootstrap binary (objcopy of kui-diagnostic.elf)")
    parser.add_argument("--output", required=True)
    parser.add_argument("--mkdcdisc", default=str(ROOT / ".deps/mkdcdisc/builddir/mkdcdisc"))
    parser.add_argument("--badge", default=str(ROOT / "resources/branding/boot-disc-badge.mr"))
    parser.add_argument("--title", default="K-UI Bootloader v3")
    parser.add_argument("--author", default="TacitusVox")
    parser.add_argument("--release", default="20260916")
    parser.add_argument("--spacer-mib", type=int, default=64,
                        help="zero-filled MiB between copies (radial separation)")
    args = parser.parse_args()
    print(json.dumps(build(args), indent=2))


if __name__ == "__main__":
    main()
