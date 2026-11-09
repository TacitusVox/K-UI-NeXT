#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Patch the background reader's slow refetch into the delivered Armada
Virtual VMU On reader (ce-armada-all-on.kui, build 67b0df9469cc).

That reader's source was not recovered; a rebuild from the published source
and the surviving reconstruction failed on the console. This keeps every
byte of the delivered module except two spans of its background reader
(src/loader/armada_all_on_slow_refetch.S): start_dma, rewritten to the same
register writes plus the 6.25 MHz window, and finish's two late diagnostic
snapshots, replaced by the window's update. The build ID strings change so
the module cannot be mistaken for 67b0df9. The input must be the exact
delivered module; the original bytes at both spans are checked first.
"""
import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from runtime_package import envelope, verify  # noqa: E402

ORIGINAL_SHA256 = "3061d5eed0c4d648323fe3f7ed33aac8bc39b18a812e5b590cfc7ea1cce609fb"
ORIGINAL_BUILD = b"67b0df9469cc"
RESIDENT = 0x8c008300
RESIDENT_PAYLOAD = 0x1a920          # the background reader inside the stage blob
RESIDENT_HEAD = bytes.fromhex("05d006d100e21230038922200470faaf")
SPANS = {
    # section: (address, original bytes replaced, SHA-256 of those bytes)
    ".start_dma": (0x8c009028, 152, "88dae05d24ff1d98975ce5291cc67d631ac7f4c4ced9efd277fdee7deaf9c710"),
    ".overrun": (0x8c00965c, 38, "92d6f7b8d7df4af92363e311cfef4e5748de6b4f12804866704c81d6feb04d53"),
}
STATS_LITERAL = (0x8c009738, 0x8c00d0bc)   # finish's literal the overrun span loads
CAPABILITY = b"KUIAVM01" + struct.pack("<2I", 1, 4096)


def elf_sections(data):
    """Allocated sections of a little-endian ELF32: name -> (address, bytes)."""
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("patch is not a little-endian ELF32")
    shoff, = struct.unpack_from("<I", data, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
    def header(i):
        return struct.unpack_from("<10I", data, shoff + i * shentsize)
    names = header(shstrndx)
    out = {}
    for i in range(shnum):
        name, kind, flags, addr, offset, size = header(i)[:6]
        if not flags & 2 or kind != 1:
            continue
        end = data.index(b"\0", names[4] + name)
        out[data[names[4] + name:end].decode()] = (addr, data[offset:offset + size])
    return out


def patch(original, patch_elf, build):
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError("input is not the delivered 67b0df9469cc all-on module")
    if len(build) != 12 or any(c not in "0123456789abcdef" for c in build):
        raise ValueError("build ID must be 12 lowercase hexadecimal characters")
    info = verify(original)
    payload = bytearray(original[64:])
    if payload[RESIDENT_PAYLOAD:RESIDENT_PAYLOAD + 16] != RESIDENT_HEAD:
        raise ValueError("background reader not at its delivered offset")
    if payload[0x150:0x160] != CAPABILITY:
        raise ValueError("all-on capability record missing")

    def at(address):
        return RESIDENT_PAYLOAD + (address - RESIDENT)

    literal_address, literal = STATS_LITERAL
    if struct.unpack_from("<I", payload, at(literal_address))[0] != literal:
        raise ValueError("finish's stats literal changed")
    sections = elf_sections(patch_elf)
    if set(sections) != set(SPANS):
        raise ValueError("patch must hold exactly .start_dma and .overrun")
    report = {}
    for name, (address, replaced, digest) in SPANS.items():
        base, code = sections[name]
        if base != address or len(code) > replaced:
            raise ValueError("patch section placed or sized wrongly: " + name)
        if name == ".overrun" and len(code) != replaced:
            raise ValueError("overrun span must be replaced exactly")
        old = bytes(payload[at(address):at(address) + replaced])
        if hashlib.sha256(old).hexdigest() != digest:
            raise ValueError("original bytes differ at " + name)
        payload[at(address):at(address) + len(code)] = code
        report[name] = {"address": hex(address), "bytes": len(code), "replaced_bytes": replaced,
                        "original_sha256": digest, "patched_sha256": hashlib.sha256(code).hexdigest()}
    count = payload.count(ORIGINAL_BUILD)
    if count != 5:
        raise ValueError("expected five embedded build IDs, found %d" % count)
    payload = bytearray(bytes(payload).replace(ORIGINAL_BUILD, build.encode()))
    out = envelope(bytes(payload), info["memory_bytes"], build)
    verify(out)
    # Everything else is the delivered module's bytes.
    changed = [i for i in range(len(payload)) if payload[i] != original[64 + i]]
    allowed = set()
    for name, (address, replaced, _) in SPANS.items():
        allowed.update(range(at(address), at(address) + len(sections[name][1])))
    builds = [i for i in range(len(original) - 64) if original[64 + i:64 + i + 12] == ORIGINAL_BUILD]
    for i in builds:
        allowed.update(range(i, i + 12))
    if not set(changed) <= allowed:
        raise ValueError("bytes outside the two spans and build IDs changed")
    report.update(
        input_sha256=ORIGINAL_SHA256, input_build=ORIGINAL_BUILD.decode(), build=build,
        payload_bytes=len(payload), payload_crc32="%08X" % (zlib.crc32(bytes(payload)) & 0xffffffff),
        sha256=hashlib.sha256(out).hexdigest(), changed_payload_bytes=len(changed),
        build_id_strings=len(builds), capability_kept=bytes(payload[0x150:0x160]) == CAPABILITY)
    return out, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path, help="delivered ce-armada-all-on.kui (67b0df9469cc)")
    parser.add_argument("patch", type=Path, help="linked armada_all_on_slow_refetch ELF")
    parser.add_argument("output", type=Path)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    out, report = patch(args.original.read_bytes(), args.patch.read_bytes(), args.build_id)
    args.output.write_bytes(out)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
