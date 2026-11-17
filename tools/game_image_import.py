#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Expand compressed game images into a new, atomically published SD folder.

CSO v1/v2 and ZSO preserve the exact uncompressed ISO bytes. Standard ZSO uses
LZ4; DreamShell's LZO dialect requires --zso-codec lzo and installed liblzo2.
CHD CD/GD images require an installed MAME chdman: verify, then extractcd.
GDI/CUE export preserves tracks and sessions but omits CHD subchannel data.
--output-format zso instead writes one standard LZ4 ZSO (2048-byte blocks,
4-byte aligned) from a CSO, ZSO or plain 2048-byte ISO data image, for K-UI's
native ZSO reader. Every block is decoded back under the console's rules
before publication. CHD and other multi-track images are refused in that mode.
The source is never edited. Conversion does not establish game compatibility.

Independently implemented from the format descriptions:
https://github.com/unknownbrackets/maxcso/blob/master/README_CSO.md
https://github.com/unknownbrackets/maxcso/blob/master/README_ZSO.md
https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
https://github.com/mamedev/mame/blob/master/src/lib/util/chd.h
https://docs.mamedev.org/tools/chdman.html
DreamShell LZO dialect confirmed by its historical GPL-2.0 isofs reader:
https://github.com/DC-SWAT/DreamShell/blob/4a2b898cbc244b2fb9bd1698b45e5325056232fb/modules/isofs/ciso.c
"""
import argparse
from array import array
import ctypes
import ctypes.util
import errno
import hashlib
from functools import lru_cache
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
import zlib

MAX_IMAGE_BYTES = 2 * 1024**3
MAX_BLOCK_BYTES = 1024**2
MAX_INDEX_BYTES = 4 * 1024**2 + 4
MAX_METADATA_BYTES = 1024**2
DESCRIPTOR_LIMIT = 32768
LBA_LIMIT = 719850
COPY_BYTES = 1024**2
PROFILE = "kui-game-image-import-v1"
# Container rules enforced by the console reader (include/kui/zso.h, src/core/zso.c).
ZSO_BLOCK_BYTES = 2048
ZSO_MAX_SPAN = 4096
ZSO_MAX_SHIFT = 9
ZSO_PREFERRED_SHIFT = 2  # Every block starts 4-byte aligned for word copies.
ZSO_MAX_FILE_BYTES = 0xFFFFFFFF
ZSO_RAW_FLAG = 0x80000000
ZSO_HEADER = struct.Struct("<4sIQIBB2s")
LZ4_SKIP_STRENGTH = 5  # Each 32 failed lookups lengthen the search step by one byte.
CD_SYNC = b"\0" + b"\xff" * 10 + b"\0"
MULTI_TRACK_SUFFIXES = (".gdi", ".cue", ".cdi", ".nrg", ".mds", ".ccd", ".m3u")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def signature(path):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode), f"Not a regular file: {path}")
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def read_exact(stream, size, label):
    data = stream.read(size)
    require(len(data) == size, f"Truncated {label}")
    return data


def hash_file(path):
    before = signature(path)
    require(before[2] <= MAX_IMAGE_BYTES + MAX_INDEX_BYTES + MAX_BLOCK_BYTES,
            "File exceeds hashing size limit")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while size < before[2]:
            block = read_exact(stream, min(COPY_BYTES, before[2] - size), "file while hashing")
            digest.update(block)
            size += len(block)
        require(not stream.read(1), "File grew while hashing")
    require(signature(path) == before, "File changed while hashing")
    return {"bytes": size, "sha256": digest.hexdigest()}


def safe_name(name):
    return (0 < len(name.encode("utf-8")) < 128
            and not name.startswith((".", " ")) and not name.endswith((".", " "))
            and all(ord(c) >= 32 and not 127 <= ord(c) <= 159
                    and c not in '\\/:*?"<>|' for c in name))


def publish_directory(source, destination):
    """Exclusive atomic rename, including when another process creates the target."""
    if os.name == "nt":
        os.rename(source, destination)
        return
    libc = ctypes.CDLL(None, use_errno=True)
    if sys.platform.startswith("linux") and hasattr(libc, "renameat2"):
        rename = libc.renameat2
        rename.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
        rename.restype = ctypes.c_int
        result = rename(-100, os.fsencode(source), -100, os.fsencode(destination), 1)
    elif sys.platform == "darwin" and hasattr(libc, "renamex_np"):
        rename = libc.renamex_np
        rename.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
        rename.restype = ctypes.c_int
        result = rename(os.fsencode(source), os.fsencode(destination), 4)
    else:
        raise OSError(errno.ENOTSUP, "Platform cannot publish atomically without overwriting")
    if result:
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code), str(destination))


def decode_lz4(data, expected, padding=None):
    """Decode one independent LZ4 block; return bytes and consumed input length.

    Output and every literal/match are bounded by the header's block size.
    The final sequence must carry at least five literals and the last match
    must start at least 12 bytes before the block end, as src/core/zso.c
    requires. Input can include alignment padding, which the container
    validates later, or here when padding gives its maximum length.
    The checks are written inline: this loop runs for every verified block.
    """
    result, offset, size = bytearray(), 0, len(data)
    last_match_start = None
    while offset < size:
        token = data[offset]
        offset += 1
        literals = token >> 4
        if literals == 15:
            while True:
                if offset >= size:
                    raise ValueError("Truncated LZ4 length")
                extra = data[offset]
                offset += 1
                literals += extra
                if literals > expected:
                    raise ValueError("LZ4 length exceeds output block")
                if extra != 255:
                    break
        done = len(result)
        if literals > expected - done:
            raise ValueError("LZ4 literals exceed output block")
        if literals > size - offset:
            raise ValueError("Truncated LZ4 literals")
        result += data[offset:offset + literals]
        offset += literals
        done += literals
        if done == expected:
            require(literals >= min(5, expected), "LZ4 block lacks final literals")
            require(last_match_start is None or last_match_start <= expected - 12,
                    "LZ4 final match is too close to block end")
            require(padding is None or size - offset <= padding, "LZ4 block has excess padding")
            return bytes(result), offset
        if offset + 2 > size:
            raise ValueError("Truncated LZ4 match offset")
        distance = data[offset] | data[offset + 1] << 8
        offset += 2
        if not 0 < distance <= done:
            raise ValueError("Invalid LZ4 match offset")
        count = token & 15
        if count == 15:
            while True:
                if offset >= size:
                    raise ValueError("Truncated LZ4 length")
                extra = data[offset]
                offset += 1
                count += extra
                if count > expected:
                    raise ValueError("LZ4 length exceeds output block")
                if extra != 255:
                    break
        count += 4
        if count > expected - done:
            raise ValueError("LZ4 match exceeds output block")
        last_match_start = done
        start = done - distance
        if count <= distance:
            result += result[start:start + count]
        else:
            # Repeating the preceding distance bytes implements overlapping matches
            # without a byte-at-a-time loop over a potentially megabyte-long run.
            result += (result[start:] * (count // distance + 1))[:count]
    raise ValueError("LZ4 block has incomplete output")


def _lz4_length_bytes(value):
    return b"\xff" * (value // 255) + bytes((value % 255,))


_LZ4_LENGTHS = tuple(map(_lz4_length_bytes, range(ZSO_BLOCK_BYTES)))


@lru_cache(maxsize=256)
def _lz4_run(value, size):
    """A block of one repeated byte: one literal, one overlapping distance-1
    match covering bytes 1..size-6, then the five mandatory final literals."""
    matched = size - 10  # Match length size-6, stored minus the 4-byte minimum.
    token, extension = (0x10 | matched, b"") if matched < 15 else (0x1F, _LZ4_LENGTHS[matched - 15])
    return bytes((token, value, 1, 0)) + extension + bytes((0x50,)) + bytes((value,)) * 5


def encode_lz4_block(block):
    """Encode one independent LZ4 block, or return None if not smaller.

    Written from the LZ4 block format rules plus the console's canonical end
    checks: every match starts at least 12 bytes before the block end and
    ends at least 5 bytes before it, so the final sequence holds 5+ literals.
    A block of one repeated byte (zero padding is common) takes a fixed path.
    Otherwise a greedy parse looks up each 4-byte sequence in a hash table
    (a dict of the latest position per sequence). Like LZ4's acceleration,
    each run of 32 failed lookups lengthens the search step by one byte and a
    match resets it, so a 2048-byte incompressible block costs about 300
    lookups before it is stored raw.
    """
    size = len(block)
    require(0 < size <= ZSO_BLOCK_BYTES, "LZ4 encoder blocks must be 1..2048 bytes")
    if size >= 13 and block.count(block[:1]) == size:
        return _lz4_run(block[0], size)
    lengths = _LZ4_LENGTHS
    limit, end = size - 12, size - 5  # Last match start; end bound for matches.
    table = {}
    remember = table.setdefault
    out = bytearray()
    append = out.append
    anchor = position = misses = 0
    while position <= limit:
        key = block[position:position + 4]
        reference = remember(key, position)
        if reference == position:
            misses += 1
            position += 1 + (misses >> LZ4_SKIP_STRENGTH)
            continue
        table[key] = position
        # Extend forward through growing slice comparisons; the first unequal
        # byte is the lowest set byte of the XOR of the two little-endian
        # integers. Overlapping references compare source bytes, which is
        # exactly what the decoder's byte-ordered copy reproduces.
        left, right, room = reference + 4, position + 4, end - position - 4
        count, width = 0, 16
        while count < room:
            if width > room - count:
                width = room - count
            first = block[left + count:left + count + width]
            second = block[right + count:right + count + width]
            if first != second:
                difference = int.from_bytes(first, "little") ^ int.from_bytes(second, "little")
                count += ((difference & -difference).bit_length() - 1) >> 3
                break
            count += width
            width <<= 2
        while position > anchor and reference and block[position - 1] == block[reference - 1]:
            position -= 1
            reference -= 1
            count += 1
        literals = position - anchor
        append((15 if literals > 15 else literals) << 4 | (15 if count > 15 else count))
        if literals >= 15:
            out += lengths[literals - 15]
        out += block[anchor:position]
        distance = position - reference
        append(distance & 255)
        append(distance >> 8)
        if count >= 15:
            out += lengths[count - 15]
        position += count + 4
        anchor = position
        misses = 0
        if len(out) >= size:
            return None
        if position <= limit:
            table[block[position - 2:position + 2]] = position - 2
    literals = size - anchor
    append((15 if literals > 15 else literals) << 4)
    if literals >= 15:
        out += lengths[literals - 15]
    out += block[anchor:]
    return bytes(out) if len(out) < size else None


ZSO_ENCODERS = {"builtin": "built-in LZ4 block encoder",
                "lz4-hc": "lz4 package, lz4.block high compression level 12"}


def zso_block_encoder(name):
    """Return the block encoder: built-in, or the opt-in lz4 package.

    The third-party package is only an accelerator. Its blocks are stored
    under the same raw-storage rule and pass the same verification as the
    built-in encoder's before anything is published.
    """
    require(name in ZSO_ENCODERS, "ZSO encoder must be builtin or lz4-hc")
    if name == "builtin":
        return encode_lz4_block
    try:
        import lz4.block
    except ImportError as error:
        raise ValueError("--zso-encoder lz4-hc needs the third-party lz4 Python package; "
                         "the default built-in encoder needs no package") from error
    compress = lz4.block.compress

    def encode(block):
        try:
            encoded = compress(block, mode="high_compression", compression=12, store_size=False)
        except (TypeError, ValueError, RuntimeError) as error:
            raise ValueError(f"lz4 package could not encode a block: {error}") from error
        return bytes(encoded) if len(encoded) < len(block) else None
    return encode


def decode_deflate(data, expected):
    inflater = zlib.decompressobj(-15)
    try:
        result = inflater.decompress(data, expected + 1)
    except zlib.error as error:
        raise ValueError(f"Invalid raw DEFLATE block: {error}") from error
    require(len(result) == expected, "DEFLATE block has incorrect output size")
    require(inflater.eof and not inflater.unconsumed_tail, "Incomplete or oversized DEFLATE block")
    return result, len(data) - len(inflater.unused_data)


@lru_cache(maxsize=4)
def load_lzo_decoder(library=None):
    name = str(library) if library else ctypes.util.find_library("lzo2")
    require(name is not None, "LZO ZSO requires installed liblzo2; install it or supply --lzo-library PATH")
    try:
        codec = ctypes.CDLL(name)
        decode = codec.lzo1x_decompress_safe
    except (OSError, AttributeError) as error:
        raise ValueError(f"Cannot load safe LZO decoder from {name}: {error}") from error
    # LZO's public ABI requires lzo_uint to match size_t (including Win64).
    decode.argtypes = (ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                       ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p)
    decode.restype = ctypes.c_int
    return decode


def decode_lzo(data, expected, padding=0, library=None):
    require(0 < expected <= MAX_BLOCK_BYTES and len(data) <= MAX_BLOCK_BYTES + 65536 + 8192,
            "LZO block exceeds memory limit")
    decode = load_lzo_decoder(library)
    source = ctypes.create_string_buffer(data, len(data))
    destination = ctypes.create_string_buffer(expected)

    def trial(size):
        written = ctypes.c_size_t(expected)
        status = decode(source, size, destination, ctypes.byref(written), None)
        return status, written.value

    status, written = trial(len(data))
    require(status in (0, -8) and written == expected,
            f"Invalid or oversized LZO1X block (decoder status {status}, output {written})")
    if status == 0:
        return destination.raw, len(data)
    # Safe LZO reports INPUT_NOT_CONSUMED (-8) after its end marker when CSO
    # alignment padding follows. Find the exact end in logarithmically many
    # bounded decodes, rather than guessing marker bytes inside literal data.
    low, high = max(1, len(data) - padding), len(data)
    while low < high:
        middle = (low + high) // 2
        status, written = trial(middle)
        if status in (0, -8) and written == expected:
            high = middle
        else:
            low = middle + 1
    status, written = trial(low)
    require(status == 0 and written == expected, "LZO block has excess padding or an invalid end marker")
    return destination.raw, low


def read_compressed_layout(stream, source, max_bytes, zso_codec="lz4"):
    """Validate a CSO/ZSO header and its whole index before any block is read."""
    header = read_exact(stream, 24, "CSO/ZSO header")
    magic, header_size, total, block_size, version, shift, reserved = ZSO_HEADER.unpack(header)
    require(magic in (b"CISO", b"ZISO"), "Unsupported compressed image signature")
    if magic == b"CISO" and version in (0, 1):
        # Several original v1 writers put zero in these unreliable fields.
        kind = "CSO v1"
    elif magic == b"CISO" and version == 2:
        require(header_size == 24 and reserved == b"\0\0", "Invalid CSO v2 header")
        kind = "CSO v2"
    elif magic == b"ZISO" and zso_codec == "lzo":
        require(version in (0, 1), "Unsupported LZO ZSO version")
        kind = "ZSO (LZO1X)"
    else:
        require(magic == b"ZISO" and version == 1 and header_size == 24
                and reserved == b"\0\0", "Unsupported ZSO version/header (standard LZ4 ZSO required)")
        kind = "ZSO (LZ4)"
    require(0 < total <= max_bytes and total % 2048 == 0,
            "Uncompressed image must be positive whole 2048-byte sectors within the size limit")
    require(2048 <= block_size <= MAX_BLOCK_BYTES and block_size % 2048 == 0,
            "Block size must be whole 2048-byte sectors from 2048 to 1048576")
    require(shift <= 16, "CSO/ZSO alignment shift exceeds 16")
    block_count = (total + block_size - 1) // block_size
    index_bytes = (block_count + 1) * 4
    require(index_bytes <= MAX_INDEX_BYTES, "Compressed image index exceeds memory limit")
    indices = array("I")
    require(indices.itemsize == 4, "Platform lacks 32-bit array entries")
    indices.frombytes(read_exact(stream, index_bytes, "CSO/ZSO index"))
    if sys.byteorder != "little":
        indices.byteswap()
    alignment = 1 << shift
    file_bytes = signature(source)[2]
    previous = (indices[0] & 0x7fffffff) << shift
    require(previous >= 24 + index_bytes, "Compressed index points into header/index")
    require(kind != "CSO v2" or not indices[-1] & 0x80000000,
            "Terminal CSO v2 index has a flag")
    for entry in indices[1:]:
        current = (entry & 0x7fffffff) << shift
        require(previous < current <= file_bytes, "Compressed indices must increase within the file")
        previous = current
    require(file_bytes - previous < alignment, "Unexpected bytes after final compressed block")
    return {"magic": magic, "kind": kind, "version": version, "total": total,
            "block_size": block_size, "shift": shift, "alignment": alignment,
            "block_count": block_count, "indices": indices, "file_bytes": file_bytes}


def iter_compressed_blocks(stream, layout, zso_codec="lz4", lzo_library=None):
    """Yield every decoded block in order after checking its span and padding."""
    magic, kind, total = layout["magic"], layout["kind"], layout["total"]
    block_size, shift, alignment = layout["block_size"], layout["shift"], layout["alignment"]
    indices = layout["indices"]
    for number in range(layout["block_count"]):
        start = (indices[number] & 0x7fffffff) << shift
        end = (indices[number + 1] & 0x7fffffff) << shift
        span = end - start
        expected = min(block_size, total - number * block_size)
        require(span <= block_size + block_size // 255 + 16 + alignment - 1,
                f"Block {number}: stored span exceeds limit")
        stream.seek(start)
        stored = read_exact(stream, span, f"compressed block {number}")
        flagged = bool(indices[number] & 0x80000000)
        raw = span >= block_size if kind == "CSO v2" else flagged
        if raw:
            require(span >= expected, f"Block {number}: short uncompressed block")
            require(span <= block_size + alignment - 1,
                    f"Block {number}: excess uncompressed-block padding")
            result = stored[:expected]
        else:
            try:
                if magic == b"ZISO" and zso_codec == "lzo":
                    result, consumed = decode_lzo(stored, expected, alignment - 1, lzo_library)
                elif magic == b"ZISO" or (kind == "CSO v2" and flagged):
                    result, consumed = decode_lz4(stored, expected)
                else:
                    result, consumed = decode_deflate(stored, expected)
                require(span - consumed < alignment, "Excess compressed-block padding")
            except ValueError as error:
                raise ValueError(f"Block {number}: {error}") from error
        yield result


def expand_cso(source, destination, max_bytes, progress=None, zso_codec="lz4", lzo_library=None):
    with source.open("rb") as stream:
        layout = read_compressed_layout(stream, source, max_bytes, zso_codec)
        kind, block_count, total = layout["kind"], layout["block_count"], layout["total"]
        digest = hashlib.sha256()
        with destination.open("xb") as output:
            blocks = iter_compressed_blocks(stream, layout, zso_codec, lzo_library)
            for number, result in enumerate(blocks):
                output.write(result)
                digest.update(result)
                if progress and (number % 4096 == 0 or number + 1 == block_count):
                    progress(f"Expanding {kind}: {number + 1}/{block_count} blocks")
            output.flush()
            os.fsync(output.fileno())
        return {"format": kind, "block_bytes": layout["block_size"], "blocks": block_count,
                "alignment_bytes": layout["alignment"], "uncompressed_bytes": total,
                "zso_codec": zso_codec if layout["magic"] == b"ZISO" else None}, {
                    "file": destination.name, "bytes": total, "sha256": digest.hexdigest()}

def inspect_chd(path, max_bytes):
    """Inspect bounded headers/metadata; chdman performs actual CHD validation."""
    size = signature(path)[2]
    with path.open("rb") as stream:
        prefix = read_exact(stream, 16, "CHD header")
        magic, length, version = struct.unpack(">8sII", prefix)
        require(magic == b"MComprHD" and version in (3, 4, 5), "CHD CD/GD import requires CHD v3, v4 or v5")
        require(length == {3: 120, 4: 108, 5: 124}[version], "Invalid CHD header length")
        header = prefix + read_exact(stream, length - 16, "CHD header")
        logical = struct.unpack_from(">Q", header, 32 if version == 5 else 28)[0]
        metadata = struct.unpack_from(">Q", header, 48 if version == 5 else 36)[0]
        hunk = struct.unpack_from(">I", header, {3: 76, 4: 44, 5: 56}[version])[0]
        require(0 < logical <= max_bytes, "CHD logical size exceeds image size limit")
        require(16 <= hunk <= MAX_BLOCK_BYTES, "CHD hunk size exceeds memory limit")
        tags, visited, metadata_bytes, subchannel, gd_layout = [], set(), 0, False, []
        while metadata:
            require(len(visited) < 256 and metadata not in visited, "CHD metadata chain is cyclic or too long")
            require(length <= metadata <= size - 16, "CHD metadata offset is outside the file")
            visited.add(metadata)
            stream.seek(metadata)
            tag, flags_length, next_offset = struct.unpack(">4sIQ", read_exact(stream, 16, "CHD metadata"))
            entry_bytes = flags_length & 0xffffff
            metadata_bytes += entry_bytes
            require(metadata_bytes <= MAX_METADATA_BYTES and entry_bytes <= size - metadata - 16,
                    "CHD metadata is truncated or exceeds memory limit")
            payload = read_exact(stream, entry_bytes, "CHD metadata payload")
            tags.append(tag.decode("ascii", errors="replace"))
            if tag in (b"CHTR", b"CHT2", b"CHGT", b"CHGD"):
                subtype = re.search(rb"(?:^|\s)SUBTYPE:([^\s\0]+)", payload)
                require(subtype is not None, "CHD track metadata lacks SUBTYPE")
                pregap_subtype = re.search(rb"(?:^|\s)PGSUB:([^\s\0]+)", payload)
                if subtype[1] != b"NONE" or (pregap_subtype and pregap_subtype[1] != b"NONE"):
                    subchannel = True
                if tag in (b"CHGT", b"CHGD"):
                    fields = {key.decode("ascii"): value.decode("ascii")
                              for key, value in re.findall(rb"(?:^|\s)([A-Z]+):([^\s\0]+)", payload)}
                    require(all(key in fields for key in ("TRACK", "TYPE", "FRAMES", "PAD", "PREGAP", "POSTGAP")),
                            "GD CHD lacks complete track geometry metadata")
                    for key in ("TRACK", "FRAMES", "PAD", "PREGAP", "POSTGAP"):
                        require(fields[key].isdigit(), "Invalid GD CHD track geometry")
                        fields[key] = int(fields[key])
                    require(0 < fields["FRAMES"] < LBA_LIMIT and 0 <= fields["PAD"] < fields["FRAMES"],
                            "GD CHD track has no captured frames or exceeds limits")
                    require(fields["PREGAP"] == 0 and fields["POSTGAP"] == 0,
                            "GD CHD virtual/stored pregaps or postgaps are unsupported; use a PAD-based image")
                    gd_layout.append(fields)
            elif tag == b"CHCD":
                if subchannel is not True:
                    subchannel = None  # Legacy binary metadata; no lossless-export promise.
            metadata = next_offset
        return {"version": version, "logical_bytes": logical, "metadata_tags": tags,
                "gdrom": any(tag in ("CHGT", "CHGD") for tag in tags),
                "cdrom": any(tag in ("CHCD", "CHTR", "CHT2") for tag in tags),
                "subchannel_omitted": subchannel, "gd_track_geometry": gd_layout}


def validate_gd_geometry(descriptor, stage, layout):
    """Confirm extractor addresses/counts against authoritative PAD metadata."""
    require(layout and len(layout) <= 99, "GD CHD lacks track geometry")
    rows = [shlex.split(line) for line in descriptor.read_text(encoding="utf-8-sig").splitlines()[1:] if line.strip()]
    require(len(rows) == len(layout), "Extracted GD track count differs from CHD metadata")
    offset = 0
    types = {"AUDIO": (0, 2352), "MODE1": (4, 2048), "MODE1_RAW": (4, 2352),
             "MODE2_FORM1": (4, 2048), "MODE2_RAW": (4, 2352)}
    for number, (row, track) in enumerate(zip(rows, layout), 1):
        require(track["TRACK"] == number and track["TYPE"] in types, "Unsupported GD CHD track type/order")
        control, stride = types[track["TYPE"]]
        require(tuple(map(int, row[:4])) == (number, offset, control, stride),
                "Extracted GDI track addresses/types differ from CHD metadata")
        require((stage / row[4]).stat().st_size == (track["FRAMES"] - track["PAD"]) * stride,
                "Extracted GDI track length differs from CHD captured-frame metadata")
        offset += track["FRAMES"]
        require(offset < LBA_LIMIT, "GD CHD geometry exceeds disc limits")


def normalize_legacy_gd_audio(descriptor, stage):
    """MAME GDI extraction leaves canonical BE audio for CHDv3/v4.

    cdrom_file also canonicalizes CHGT legacy LE storage before extraction.
    Pair-swapping staged AUDIO files restores the GDI little-endian convention.
    Data tracks are copied exactly and the original CHD is never changed.
    """
    swapped = []
    for line in descriptor.read_text(encoding="utf-8-sig").splitlines()[1:]:
        if not line.strip():
            continue
        row = shlex.split(line)
        if int(row[2]) != 0:
            continue
        path = stage / row[4]
        with path.open("r+b") as stream:
            position = 0
            while data := stream.read(65536):
                require(len(data) % 2 == 0, "Odd-length extracted GD audio")
                samples = array("H", data)
                require(samples.itemsize == 2, "Host cannot represent 16-bit CDDA samples")
                samples.byteswap()
                stream.seek(position)
                stream.write(samples.tobytes())
                position += len(data)
                stream.seek(position)
            stream.flush()
            os.fsync(stream.fileno())
        swapped.append(path.name)
    return swapped


def run_chdman(command, stage, timeout):
    # Keep potentially verbose progress on disk, not in a growing captured string.
    log = stage / ".chdman.log"
    with log.open("wb") as output:
        try:
            result = subprocess.run(command, cwd=stage, stdout=output, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
        except subprocess.TimeoutExpired as error:
            raise ValueError(f"chdman {command[1]} exceeded {timeout}s time limit") from error
    with log.open("rb") as stream:
        stream.seek(max(0, log.stat().st_size - 16384))
        tail = stream.read(16384).decode("utf-8", errors="replace")
    log.unlink()
    require(result.returncode == 0, f"chdman {command[1]} failed ({result.returncode}): {tail.strip()}")
    return tail


def validate_extracted(stage, descriptor, max_bytes):
    """Check output confinement, track references, and positive whole sectors."""
    files, total = {}, 0
    for path in stage.iterdir():
        require(safe_name(path.name) and path.name.isascii(), f"Unsafe chdman output name: {path.name}")
        byte_count = signature(path)[2]
        require(byte_count > 0, f"Empty chdman output: {path.name}")
        total += byte_count
        require(total <= max_bytes + DESCRIPTOR_LIMIT, "Extracted image exceeds size limit")
        require(path.name.casefold() not in files, "Ambiguous extracted filenames")
        files[path.name.casefold()] = (path.name, byte_count)
    require(descriptor.name.casefold() in files, "chdman did not produce a descriptor")
    require(descriptor.stat().st_size <= DESCRIPTOR_LIMIT, "Extracted descriptor exceeds 32768 bytes")
    text = descriptor.read_text(encoding="utf-8-sig")
    references, tracks = set(), 0
    if descriptor.suffix == ".gdi":
        previous_end = 0
        lines = [line.strip() for line in text.splitlines() if line.strip()]
        require(lines and lines[0].isdigit() and 1 <= int(lines[0]) <= 99
                and len(lines) == int(lines[0]) + 1, "Invalid extracted GDI track count")
        for number, line in enumerate(lines[1:], 1):
            match = re.fullmatch(r'(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(?:"([^"\r\n]+)"|(\S+))\s+(\d+)', line)
            require(match is not None and int(match[1]) == number, "Invalid extracted GDI track row")
            lba, control, sector, name, offset = int(match[2]), int(match[3]), int(match[4]), match[5] or match[6], int(match[7])
            require(control in (0, 4) and sector in (2048, 2352)
                    and (control == 4 or sector == 2352), "Unsupported extracted GDI sector layout")
            validate_reference(files, name, offset, sector)
            require(offset == 0 and name.casefold() not in references,
                    "Extracted GDI must use separate track files with zero offsets")
            sectors = files[name.casefold()][1] // sector
            require(previous_end <= lba < LBA_LIMIT and lba + sectors <= LBA_LIMIT,
                    "Extracted GDI tracks overlap or exceed LBA limits")
            previous_end = lba + sectors
            references.add(name.casefold())
            tracks += 1
    else:
        tracks, references = validate_cue(text, files)
    require(set(files) == references | {descriptor.name.casefold()}, "Unreferenced chdman output files")
    return tracks, [{"file": name, **hash_file(stage / name)} for name, _ in sorted(files.values())]


def validate_reference(files, name, offset, sector):
    require(safe_name(name) and name.isascii() and name.casefold() in files, "Unsafe or missing extracted track file")
    size = files[name.casefold()][1]
    require(0 <= offset < size and (size - offset) % sector == 0,
            "Extracted track is not a positive whole number of sectors")


def cue_frames(value):
    match = re.fullmatch(r"([0-9]{1,3}):([0-9]{1,2}):([0-9]{1,2})", value)
    require(match is not None, "Invalid extracted CUE MSF timestamp")
    minute, second, frame = map(int, match.groups())
    total = minute * 4500 + second * 75 + frame
    require(second < 60 and frame < 75 and total < LBA_LIMIT,
            "Extracted CUE MSF timestamp exceeds limits")
    return total


def validate_cue(text, files):
    """Validate the separate-track CUE contract requested by extractcd -sb.

    This deliberately accepts one track per FILE. Shared-file CUE geometry is
    handled by the console's general parser, not guessed during CHD export.
    """
    require(all(ch in "\t\r\n" or ord(ch) >= 32 for ch in text),
            "Extracted CUE contains control bytes")
    records, references = [], set()
    current_name, track, session, density, pending_leadin = None, None, 1, 0, None
    scramble_marker = False
    modes = {"AUDIO": 2352, "CDG": 2448, "MODE1/2048": 2048, "MODE1/2352": 2352,
             "MODE1/2448": 2448, "MODE2/2048": 2048, "MODE2/2336": 2336,
             "MODE2/2352": 2352, "MODE2/2448": 2448}
    for line in text.splitlines():
        line = line.strip(" \t")
        if not line:
            continue
        command = line.split(None, 1)[0].upper()
        if command == "FILE":
            require(current_name is None or track is not None and 1 in track["indices"],
                    "Extracted CUE track lacks INDEX 01")
            match = re.fullmatch(r'FILE\s+(?:"([^"\r\n]+)"|(\S+))\s+BINARY', line, re.IGNORECASE)
            require(match is not None, "Extracted CUE requires BINARY files")
            current_name = match[1] or match[2]
            require(safe_name(current_name) and current_name.isascii(), "Unsafe extracted CUE filename")
            require(current_name.casefold() in files, "Missing extracted CUE track file")
            require(current_name.casefold() not in references, "Extracted CUE repeats a FILE")
            references.add(current_name.casefold())
            track = None
        elif command == "TRACK":
            match = re.fullmatch(r'TRACK\s+([0-9]{1,2})\s+(\S+)', line, re.IGNORECASE)
            require(match is not None and current_name is not None and track is None
                    and match[2].upper() in modes, "Unsupported extracted CUE split-track layout")
            require(int(match[1]) == len(records) + 1 and len(records) < 99,
                    "Nonconsecutive extracted CUE tracks")
            sector = modes[match[2].upper()]
            validate_reference(files, current_name, 0, sector)
            track = {"file": current_name, "frames": files[current_name.casefold()][1] // sector,
                     "session": session, "density": density, "indices": {}, "pregap": None,
                     "postgap": None, "leadin": pending_leadin, "leadout": None}
            records.append(track)
            pending_leadin = None
        elif command == "INDEX":
            match = re.fullmatch(r'INDEX\s+([0-9]{1,2})\s+(\S+)', line, re.IGNORECASE)
            require(match is not None and track is not None, "Invalid extracted CUE INDEX")
            number, frame = int(match[1]), cue_frames(match[2])
            indices = track["indices"]
            require(number not in indices and (not indices or number > max(indices)),
                    "Duplicate or out-of-order extracted CUE INDEX")
            require(number in (0, 1) or 1 in indices, "Extracted CUE track lacks INDEX 01")
            require(not indices or frame >= indices[max(indices)], "Extracted CUE INDEX timestamps decrease")
            require(frame < track["frames"],
                    "Extracted CUE INDEX exceeds track file bounds")
            indices[number] = frame
        elif command in ("PREGAP", "POSTGAP"):
            match = re.fullmatch(r'(?:PREGAP|POSTGAP)\s+(\S+)', line, re.IGNORECASE)
            field = command.lower()
            require(match is not None and track is not None and track[field] is None,
                    "Invalid or duplicate extracted CUE gap")
            track[field] = cue_frames(match[1])
        elif command == "REM":
            parts = line.split()
            if len(parts) < 2:
                continue
            kind = parts[1].upper()
            if kind == "SESSION":
                require(len(parts) == 3 and re.fullmatch(r"[0-9]{1,2}", parts[2])
                        and session <= int(parts[2]) <= 2, "Unsupported extracted CUE session")
                session = int(parts[2])
            elif kind in ("LEAD-IN", "PREGAP", "LEAD-OUT"):
                require(len(parts) == 3, "Invalid extracted CUE session gap")
                frames = cue_frames(parts[2])
                if kind == "LEAD-OUT":
                    require(track is not None and track["leadout"] is None,
                            "Invalid extracted CUE LEAD-OUT")
                    track["leadout"] = frames
                else:
                    pending_leadin = (pending_leadin or 0) + frames
                    require(pending_leadin < LBA_LIMIT, "Extracted CUE lead-in exceeds limits")
            elif kind in ("SINGLE-DENSITY", "HIGH-DENSITY"):
                next_density = 1 if kind == "SINGLE-DENSITY" else 2
                require(len(parts) == 3 and parts[2].upper() == "AREA"
                        and not (density == 2 and next_density == 1), "Invalid extracted CUE density marker")
                density = next_density
            elif kind == "KUI":
                require(len(parts) == 4 and parts[2].upper() == "SCRAMBLED"
                        and parts[3] in ("0", "1") and not scramble_marker,
                        "Invalid extracted CUE SCRAMBLED marker")
                scramble_marker = True
        else:
            require(command in ("TITLE", "PERFORMER", "SONGWRITER", "CATALOG", "ISRC", "FLAGS", "CDTEXTFILE"),
                    "Unsupported extracted CUE command")
    require(records and track is not None and 1 in track["indices"], "Extracted CUE track lacks INDEX 01")
    require(pending_leadin is None, "Extracted CUE has an unused lead-in")
    logical, previous_end = 0, 0
    for index, track in enumerate(records):
        require(1 in track["indices"], "Extracted CUE track lacks INDEX 01")
        transition = index > 0 and track["session"] > records[index - 1]["session"]
        if transition:
            logical += track["leadin"] if track["leadin"] is not None else 4500
        else:
            require(track["leadin"] is None, "Extracted CUE lead-in requires a new session")
        logical += track["indices"][1] + (track["pregap"] or 0)
        if track["density"] == 2 and (index == 0 or records[index - 1]["density"] != 2):
            logical = 45000
        sectors = track["frames"] - track["indices"][1]
        require(previous_end <= logical < LBA_LIMIT and logical + sectors <= LBA_LIMIT,
                "Extracted CUE session geometry overlaps or exceeds LBA limits")
        previous_end = logical + sectors
        logical = previous_end + (track["postgap"] or 0)
        next_session = index + 1 < len(records) and records[index + 1]["session"] > track["session"]
        if next_session:
            logical += track["leadout"] if track["leadout"] is not None else 6750 if track["session"] == 1 else 2250
        else:
            require(track["leadout"] is None, "Extracted CUE lead-out requires a following session")
        require(logical <= LBA_LIMIT, "Extracted CUE gaps exceed LBA limits")
    return len(records), references


def expand_chd(source, stage, max_bytes, chdman, parent, timeout, drop_subchannels, progress=None):
    metadata = inspect_chd(source, max_bytes)
    if not metadata["cdrom"] and not metadata["gdrom"] and parent:
        parent_metadata = inspect_chd(parent, max_bytes)
        for key in ("cdrom", "gdrom", "metadata_tags", "subchannel_omitted", "gd_track_geometry"):
            metadata[key] = parent_metadata[key]
    require(metadata["cdrom"] or metadata["gdrom"], "CHD is not a CD/GD image (DVD, hard disk and laserdisc are unsupported)")
    require(not (metadata["cdrom"] and metadata["gdrom"]), "CHD has conflicting CD and GD track metadata")
    require(metadata["subchannel_omitted"] is False or drop_subchannels,
            "CHD contains subchannel data (or legacy metadata cannot prove its absence); "
            "GDI/CUE export omits it. Use --drop-subchannels to explicitly allow this copy")
    executable = shutil.which(str(chdman))
    require(executable is not None, "CHD import requires MAME chdman; install it or supply --chdman")
    common = ["-i", str(source)] + (["-ip", str(parent)] if parent else [])
    if progress:
        progress("Verifying CHD with chdman")
    run_chdman([executable, "verify", *common], stage, timeout)
    # GD requires GDI to preserve density/track addresses. GDI is always split,
    # including older MAME versions that do not implement the -sb option.
    descriptor = stage / ("disc.gdi" if metadata["gdrom"] else "disc.cue")
    if progress:
        progress(f"Extracting CHD to {descriptor.name} and separate tracks")
    command = [executable, "extractcd", *common, "-o", str(descriptor)]
    if metadata["gdrom"]:
        run_chdman(command, stage, timeout)
    else:
        try:
            run_chdman([*command, "-sb"], stage, timeout)
        except ValueError as error:
            # Legacy CD extraction may still produce a valid single-track CUE.
            # The bounded validator rejects unsupported shared/mixed shapes.
            if "Option '-sb' not valid" not in str(error):
                raise
            require(not any(stage.iterdir()), "Legacy chdman left partial output after option rejection")
            run_chdman(command, stage, timeout)
    tracks, outputs = validate_extracted(stage, descriptor, max_bytes)
    audio_swapped = []
    if metadata["gdrom"]:
        validate_gd_geometry(descriptor, stage, metadata["gd_track_geometry"])
        if metadata["version"] < 5:
            audio_swapped = normalize_legacy_gd_audio(descriptor, stage)
            tracks, outputs = validate_extracted(stage, descriptor, max_bytes)
    return {"format": "CHD", **metadata, "tracks": tracks, "chdman_verified": True,
            "subchannel_preserved": False, "legacy_gd_audio_pair_swapped": audio_swapped,
            "gd_geometry_verified": metadata["gdrom"]}, descriptor.name, outputs


def zso_shift_for(end_offset):
    """Return the index shift for a ZSO whose last block ends at end_offset.

    Shift 2 starts every block on a 4-byte boundary. A larger shift is chosen
    only when end_offset >> 2 would not fit a 31-bit index entry (bit 31 marks
    raw blocks); the console admits at most 9.
    """
    shift = ZSO_PREFERRED_SHIFT
    while end_offset >> shift > 0x7FFFFFFF:
        shift += 1
    require(shift <= ZSO_MAX_SHIFT, "ZSO offsets do not fit 31-bit index entries at shift 9")
    return shift


def zso_name(source, output):
    """The launch file: source stem, else output folder name, if console-safe."""
    for stem in (source.stem, output.name, "disc"):
        name = f"{stem}.zso"
        # src/core/game_image.c admits printable ASCII names below 128 bytes.
        if safe_name(name) and all(32 <= ord(character) <= 126 for character in name):
            return name
    return "disc.zso"


def inspect_data_image(head, sectors):
    """Check that the first 64 logical sectors form one 2048-byte ISO9660 track.

    Follows the console's standalone-ISO rules (single_parse in
    src/core/game_image.c): volume descriptors from sector 16, a valid primary
    root record, and the root's '.' record within the first 64 sectors, which
    gives the data session's base LBA.
    """
    require(head[:12] != CD_SYNC, "Image holds raw CD sectors (sync header found); "
            "ZSO output needs a single image of 2048-byte data sectors")
    require(sectors >= 18, "Image is too small for an ISO9660 data track (fewer than 18 sectors)")
    horizon, root, primary = min(sectors, 64), None, None
    for number in range(16, min(32, horizon)):
        descriptor = head[number * 2048:number * 2048 + 7]
        require(descriptor[1:] == b"CD001\x01", f"Sector {number} is not an ISO9660 volume descriptor; "
                "ZSO output needs a single 2048-byte data image")
        if descriptor[0] == 255:
            break
        if descriptor[0] == 1:
            root, primary = head[number * 2048 + 156:number * 2048 + 190], number
            break
    require(root is not None, "ISO9660 primary volume descriptor not found")
    root_lba = int.from_bytes(root[2:6], "little")
    require(root[0] == 34 and root[32] == 1 and root[33] == 0 and root[25] & 2
            and root_lba == int.from_bytes(root[6:10], "big"),
            "ISO9660 primary volume descriptor has an invalid root directory record")
    for number in range(primary + 1, horizon):
        record = head[number * 2048:number * 2048 + 34]
        if record[0] == 34 and record[2:] == root[2:]:
            require(root_lba >= number, "ISO9660 root directory precedes the image start")
            base = root_lba - number
            break
    else:
        raise ValueError("ISO9660 root directory is not within the first 64 sectors; "
                         "the console could not locate the data session")
    require(base < LBA_LIMIT and sectors <= LBA_LIMIT - base,
            "Image exceeds disc LBA limits at its session address")
    return {"iso9660_pvd_sector": primary, "session_lba": base}


class LogicalImage:
    """Split decoded chunks into 2048-byte blocks, hashing and checking them."""

    def __init__(self, chunks, sectors):
        self.chunks, self.sectors = chunks, sectors
        self.digest, self.data_image = hashlib.sha256(), None

    def blocks(self):
        head, wanted, count = bytearray(), min(self.sectors, 64) * ZSO_BLOCK_BYTES, 0
        for chunk in self.chunks:
            self.digest.update(chunk)
            if self.data_image is None:
                head += chunk[:wanted - len(head)]
                if len(head) == wanted:
                    self.data_image = inspect_data_image(bytes(head), self.sectors)
            for offset in range(0, len(chunk), ZSO_BLOCK_BYTES):
                count += 1
                yield chunk[offset:offset + ZSO_BLOCK_BYTES]
        require(count == self.sectors and self.data_image is not None,
                "Decoded image size differs from its header")


def read_chunks(stream, total, label):
    remaining = total
    while remaining:
        chunk = read_exact(stream, min(COPY_BYTES, remaining), label)
        remaining -= len(chunk)
        yield chunk
    require(not stream.read(1), f"{label} grew while reading")


def progress_due(number, count):
    return number % 4096 == 0 or number + 1 == count


def write_zso(blocks, destination, total, progress=None, label="Writing ZSO", encode=encode_lz4_block):
    """Write header, index and LZ4/raw blocks; return the layout written."""
    block_count = total // ZSO_BLOCK_BYTES
    index_bytes = (block_count + 1) * 4
    # Raw storage is the worst case: a smaller LZ4 block plus its padding
    # never exceeds 2048 bytes, because the alignment divides 2048.
    shift = zso_shift_for(ZSO_HEADER.size + index_bytes + total)
    alignment = 1 << shift
    offset = (ZSO_HEADER.size + index_bytes + alignment - 1) // alignment * alignment
    require(offset + total <= ZSO_MAX_FILE_BYTES, "ZSO would exceed the console's 32-bit file size")
    index = array("I", bytes(index_bytes))
    require(index.itemsize == 4, "Platform lacks 32-bit array entries")
    padding = [bytes(size) for size in range(alignment)]
    raw_blocks, number = 0, -1
    with destination.open("xb") as output:
        output.write(bytes(offset))  # Header and index are written last.
        for number, block in enumerate(blocks):
            require(number < block_count and len(block) == ZSO_BLOCK_BYTES,
                    "Decoded image size differs from its header")
            encoded = encode(block)
            if encoded is None:
                index[number] = offset >> shift | ZSO_RAW_FLAG
                output.write(block)
                offset += ZSO_BLOCK_BYTES
                raw_blocks += 1
            else:
                index[number] = offset >> shift
                output.write(encoded)
                pad = -len(encoded) % alignment
                output.write(padding[pad])
                offset += len(encoded) + pad
            if progress and progress_due(number, block_count):
                progress(f"{label}: {number + 1}/{block_count} blocks")
        require(number + 1 == block_count, "Decoded image size differs from its header")
        index[block_count] = offset >> shift
        if sys.byteorder != "little":
            index.byteswap()
        output.seek(0)
        output.write(ZSO_HEADER.pack(b"ZISO", ZSO_HEADER.size, total, ZSO_BLOCK_BYTES, 1, shift, b"\0\0"))
        output.write(index.tobytes())
        output.flush()
        os.fsync(output.fileno())
    return {"shift": shift, "alignment_bytes": alignment, "blocks": block_count,
            "raw_blocks": raw_blocks, "bytes": offset}


def verify_zso(path, progress=None):
    """Re-read a ZSO under the console reader's rules and decode every block.

    Mirrors src/core/zso.c for the LZ4 codec: kui_zso_header_parse_codec
    (ZISO, header 24, version 1, 2048-byte blocks, shift <= 9, zero reserved
    bytes, at most 2 GiB in a file below 4 GiB), kui_zso_validate_index
    (31-bit entries within 32-bit offsets, past the index, increasing spans
    of at most 4096 bytes, tail below 1 << shift) and kui_zso_read_block (raw
    spans of 2048 plus padding below 1 << shift; canonical LZ4 ends, padding
    after the stream below 1 << shift). Hashes the file and the decoded image.
    """
    file_bytes = signature(path)[2]
    require(file_bytes <= ZSO_MAX_FILE_BYTES, "ZSO exceeds the console's 32-bit file size")
    contents, logical = hashlib.sha256(), hashlib.sha256()
    with path.open("rb") as stream:
        header = read_exact(stream, ZSO_HEADER.size, "ZSO header")
        magic, header_size, total, block_size, version, shift, reserved = ZSO_HEADER.unpack(header)
        require(magic == b"ZISO" and header_size == ZSO_HEADER.size and version == 1
                and block_size == ZSO_BLOCK_BYTES and shift <= ZSO_MAX_SHIFT and reserved == b"\0\0",
                "ZSO header does not meet the console's standard LZ4 rules")
        require(0 < total <= MAX_IMAGE_BYTES and total % ZSO_BLOCK_BYTES == 0,
                "ZSO image must be whole 2048-byte blocks up to 2 GiB")
        block_count = total // ZSO_BLOCK_BYTES
        index_bytes = (block_count + 1) * 4
        lowest, alignment = ZSO_HEADER.size + index_bytes, 1 << shift
        require(file_bytes >= lowest, "Truncated ZSO index")
        stored_index = read_exact(stream, index_bytes, "ZSO index")
        indices = array("I", stored_index)
        if sys.byteorder != "little":
            indices.byteswap()
        contents.update(header)
        contents.update(stored_index)

        def position(entry):
            value = entry & 0x7FFFFFFF
            require(value <= ZSO_MAX_FILE_BYTES >> shift and lowest <= value << shift <= file_bytes,
                    "ZSO index entry is outside the file")
            return value << shift

        start = position(indices[0])
        contents.update(read_exact(stream, start - lowest, "ZSO alignment"))
        raw_blocks = 0
        for number in range(block_count):
            entry, end = indices[number], position(indices[number + 1])
            span = end - start
            require(0 < span <= ZSO_MAX_SPAN, f"Block {number}: ZSO spans must increase by 1..4096 bytes")
            stored = read_exact(stream, span, f"ZSO block {number}")
            contents.update(stored)
            if entry & ZSO_RAW_FLAG:
                require(ZSO_BLOCK_BYTES <= span < ZSO_BLOCK_BYTES + alignment,
                        f"Block {number}: raw ZSO span must be 2048 bytes plus padding below {alignment}")
                block = stored[:ZSO_BLOCK_BYTES]
                raw_blocks += 1
            else:
                try:
                    block = decode_lz4(stored, ZSO_BLOCK_BYTES, alignment - 1)[0]
                except ValueError as error:
                    raise ValueError(f"Block {number}: {error}") from error
            logical.update(block)
            start = end
            if progress and progress_due(number, block_count):
                progress(f"Verifying ZSO: {number + 1}/{block_count} blocks")
        require(file_bytes - start < alignment, "Unexpected bytes after final ZSO block")
        contents.update(read_exact(stream, file_bytes - start, "ZSO tail"))
        require(not stream.read(1), "ZSO grew during verification")
    return {"blocks": block_count, "raw_blocks": raw_blocks, "shift": shift,
            "alignment_bytes": alignment, "logical_bytes": total,
            "logical_sha256": logical.hexdigest(), "bytes": file_bytes, "sha256": contents.hexdigest()}


def convert_to_zso(source, destination, magic, max_bytes, zso_codec, lzo_library,
                   source_sha256, progress=None, encoder="builtin"):
    """Write one verified standard LZ4 ZSO from a CSO, ZSO or plain ISO image.

    An LZ4 ZSO that already has 2048-byte blocks and 4..512-byte alignment is
    copied unchanged; every other input is decoded and encoded. Either way the
    published file is decoded again under the console's rules and its image
    hash must equal the source's.
    """
    encode = zso_block_encoder(encoder)
    file_bytes = signature(source)[2]
    with source.open("rb") as stream:
        layout = None
        if magic[:4] in (b"CISO", b"ZISO"):
            layout = read_compressed_layout(stream, source, max_bytes, zso_codec)
            kind, total = layout["kind"], layout["total"]
            chunks = iter_compressed_blocks(stream, layout, zso_codec, lzo_library)
            details = {"format": kind, "block_bytes": layout["block_size"], "blocks": layout["block_count"],
                       "alignment_bytes": layout["alignment"], "uncompressed_bytes": total,
                       "zso_codec": zso_codec if magic[:4] == b"ZISO" else None}
        else:
            suffix = source.suffix.lower()
            require(suffix not in MULTI_TRACK_SUFFIXES, f"{suffix} images can hold several tracks; "
                    "ZSO output accepts a single 2048-byte data image (ISO, CSO or ZSO)")
            require(0 < file_bytes <= max_bytes and file_bytes % ZSO_BLOCK_BYTES == 0,
                    "ZSO output needs a single image of whole 2048-byte sectors within the size limit")
            kind, total = "ISO", file_bytes
            chunks = read_chunks(stream, total, "ISO image")
            details = {"format": kind, "uncompressed_bytes": total}
        sectors = total // ZSO_BLOCK_BYTES
        require(sectors <= LBA_LIMIT, f"Image has {sectors} sectors; the console admits at most {LBA_LIMIT}")
        image = LogicalImage(chunks, sectors)
        copy = (layout is not None and kind == "ZSO (LZ4)" and layout["block_size"] == ZSO_BLOCK_BYTES
                and ZSO_PREFERRED_SHIFT <= layout["shift"] <= ZSO_MAX_SHIFT and file_bytes <= ZSO_MAX_FILE_BYTES)
        if copy:
            for number, _ in enumerate(image.blocks()):
                if progress and progress_due(number, sectors):
                    progress(f"Checking {kind}: {number + 1}/{sectors} blocks")
            stream.seek(0)
            digest = hashlib.sha256()
            with destination.open("xb") as output:
                for chunk in read_chunks(stream, file_bytes, "ZSO source"):
                    output.write(chunk)
                    digest.update(chunk)
                output.flush()
                os.fsync(output.fileno())
            require(digest.hexdigest() == source_sha256, "Source changed while copying")
            written = None
        else:
            written = write_zso(image.blocks(), destination, total, progress, f"Converting {kind} to ZSO", encode)
    verified = verify_zso(destination, progress)
    require(verified["logical_bytes"] == total and verified["logical_sha256"] == image.digest.hexdigest(),
            "ZSO verification failed: the decoded image differs from the source")
    if written:
        require(all(verified[key] == written[key] for key in ("shift", "blocks", "raw_blocks", "bytes")),
                "ZSO verification failed: the file differs from the layout written")
    else:
        require(verified["sha256"] == source_sha256, "ZSO verification failed: the copy differs from the source")
    details.update(logical_sha256=verified["logical_sha256"], data_image=image.data_image)
    zso = {"codec": "lz4", "method": "copied" if copy else "encoded",
           "encoder": None if copy else ZSO_ENCODERS[encoder],
           "block_bytes": ZSO_BLOCK_BYTES, "blocks": verified["blocks"], "raw_blocks": verified["raw_blocks"],
           "compressed_blocks": verified["blocks"] - verified["raw_blocks"], "shift": verified["shift"],
           "alignment_bytes": verified["alignment_bytes"], "logical_bytes": total,
           "logical_sha256": verified["logical_sha256"], "file": destination.name,
           "bytes": verified["bytes"], "sha256": verified["sha256"],
           "ratio": round(verified["bytes"] / total, 6)}
    return details, [{"file": destination.name, "bytes": verified["bytes"], "sha256": verified["sha256"]}], zso


def import_image(source, output_directory, *, max_bytes=MAX_IMAGE_BYTES,
                 chdman="chdman", parent=None, timeout=1800, drop_subchannels=False,
                 zso_codec="lz4", lzo_library=None, progress=None, output_format="expanded",
                 zso_encoder="builtin"):
    """Convert into a new directory; return its saved report after publication."""
    started = time.monotonic()
    require(output_format in ("expanded", "zso"), "Output format must be expanded or zso")
    require(zso_encoder in ZSO_ENCODERS, "ZSO encoder must be builtin or lz4-hc")
    require(zso_encoder == "builtin" or output_format == "zso",
            "--zso-encoder applies only to --output-format zso")
    require(0 < max_bytes <= MAX_IMAGE_BYTES, "Size limit must be 1..2147483648 bytes")
    require(0 < timeout <= 86400, "chdman timeout must be 1..86400 seconds")
    require(zso_codec in ("lz4", "lzo"), "ZSO codec must be lz4 or lzo; automatic guessing is unsupported")
    source = Path(source).expanduser().absolute()
    source_signature = signature(source)
    require(source_signature[2] <= max_bytes + MAX_INDEX_BYTES + MAX_BLOCK_BYTES,
            "Compressed source exceeds size limit")
    source = source.resolve()
    output = Path(output_directory).expanduser().absolute()
    require(not os.path.lexists(output), f"Output already exists: {output}")
    require(output.parent.is_dir(), "Output parent directory does not exist")
    output = output.parent.resolve() / output.name
    require(output != source.parent and source.parent not in output.parents,
            "Output must be outside the original image directory")
    require(safe_name(output.name), "Output directory name is unsafe or exceeds 127 bytes")
    with source.open("rb") as stream:
        magic = stream.read(8)
    if output_format == "zso":
        require(magic != b"MComprHD", "CHD cannot be converted to ZSO: GD and multi-track CD images "
                "are out of scope for ZSO output; use --output-format expanded")
    else:
        require(magic[:4] in (b"CISO", b"ZISO") or magic == b"MComprHD",
                "Unsupported image: import accepts CSO, ZSO and CD/GD CHD")
    parent_record, parent_signature = None, None
    if parent is not None:
        require(magic == b"MComprHD", "--parent is only supported for CHD")
        parent = Path(parent).expanduser().absolute()
        parent_signature = signature(parent)
        require(parent_signature[2] <= max_bytes + MAX_INDEX_BYTES + MAX_BLOCK_BYTES,
                "CHD parent exceeds size limit")
        parent = parent.resolve()
        require(parent != source, "CHD parent must differ from the source")
        parent_record = {"path": str(parent), **hash_file(parent)}
    source_record = {"path": str(source), **hash_file(source)}
    require(signature(source) == source_signature, "Source changed while hashing")
    stage = Path(tempfile.mkdtemp(prefix=f".{output.name}.staging-", dir=output.parent))
    try:
        zso = None
        if output_format == "zso":
            launch = zso_name(source, output)
            details, outputs, zso = convert_to_zso(source, stage / launch, magic, max_bytes, zso_codec,
                                                   lzo_library, source_record["sha256"], progress, zso_encoder)
        elif magic == b"MComprHD":
            details, launch, outputs = expand_chd(source, stage, max_bytes, chdman, parent, timeout,
                                                 drop_subchannels, progress)
        else:
            launch = "disc.iso"
            details, record = expand_cso(source, stage / launch, max_bytes, progress, zso_codec, lzo_library)
            outputs = [record]
        require(signature(source) == source_signature, "Source changed during import")
        require(hash_file(source) == {key: source_record[key] for key in ("bytes", "sha256")},
                "Source content changed during import")
        if parent:
            require(signature(parent) == parent_signature and hash_file(parent) == {
                key: parent_record[key] for key in ("bytes", "sha256")}, "CHD parent changed during import")
        report = {"schema": 1, "profile": PROFILE, "tool": "tools/game_image_import.py", "complete": True}
        if zso is not None:
            report["output_format"] = "zso"
        report.update(source=source_record, parent=parent_record, image=details)
        if zso is not None:
            zso["elapsed_seconds"] = round(time.monotonic() - started, 3)
            report["zso"] = zso
        report.update(launch_file=launch, outputs=outputs, verification={
            "source_unchanged": True, "output_sha256_recorded": True,
            **({"zso_blocks_decoded": True, "logical_sha256_matches_source": True} if zso else {}),
            "catalogue_match_checked": False, "game_compatibility_checked": False})
        (stage / "import-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        publish_directory(stage, output)
        return report
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path,
                        help="CSO, ZSO or CD/GD CHD file; also a 2048-byte ISO with --output-format zso")
    parser.add_argument("output", type=Path, help="New output directory outside the source folder")
    parser.add_argument("--chdman", default="chdman", help="Installed MAME chdman executable")
    parser.add_argument("--parent", type=Path, help="Parent CHD required by a delta CHD")
    parser.add_argument("--drop-subchannels", action="store_true", help="Allow CHD export to omit stored subchannel data")
    parser.add_argument("--zso-codec", choices=("lz4", "lzo"), default="lz4", help="ZSO codec: standard LZ4 or DreamShell LZO (default: lz4)")
    parser.add_argument("--lzo-library", help="Optional explicit liblzo2 shared library/DLL path")
    parser.add_argument("--max-bytes", type=int, default=MAX_IMAGE_BYTES, help="Output limit (default: 2 GiB)")
    parser.add_argument("--timeout", type=int, default=1800, help="Seconds per chdman operation (default: 1800)")
    parser.add_argument("--output-format", choices=("expanded", "zso"), default="expanded",
                        help="expanded: ISO, GDI or CUE folder (default); zso: one standard LZ4 ZSO "
                             "for K-UI's native ZSO reader (CSO, ZSO or ISO input)")
    parser.add_argument("--zso-encoder", choices=tuple(ZSO_ENCODERS), default="builtin",
                        help="ZSO block encoder: built-in (default, no packages) or the optional "
                             "third-party lz4 package's high-compression mode")
    args = parser.parse_args(argv)
    try:
        report = import_image(args.source, args.output, max_bytes=args.max_bytes, chdman=args.chdman,
                              parent=args.parent, timeout=args.timeout, drop_subchannels=args.drop_subchannels,
                              zso_codec=args.zso_codec, lzo_library=args.lzo_library,
                              progress=lambda line: print(line, file=sys.stderr),
                              output_format=args.output_format, zso_encoder=args.zso_encoder)
    except (OSError, ValueError, UnicodeError) as error:
        print(f"Import failed: {error}", file=sys.stderr)
        return 1
    zso = report.get("zso")
    if zso:
        action = ("Copied verified standard ZSO" if zso["method"] == "copied"
                  else f"Converted {report['image']['format']} to ZSO")
        print(f"{action}: {args.output / report['launch_file']}")
        print(f"{zso['blocks']} blocks ({zso['raw_blocks']} stored raw), {zso['bytes']} bytes = "
              f"{zso['ratio']:.1%} of the image, shift {zso['shift']}; every block decoded and matched.")
    else:
        print(f"Imported {report['image']['format']}: {args.output / report['launch_file']}")
    print("Copy the complete output folder to SD. The original image is unchanged.")
    if report["image"].get("subchannel_omitted") is not False and report["image"]["format"] == "CHD":
        print("CHD subchannel data is omitted by GDI/CUE export; preserve the original CHD.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
