# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic compressed bytes and controlled chdman; originals stay untouched."""
from array import array
import ctypes
import ctypes.util
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("game_image_import", ROOT / "tools/game_image_import.py")
importer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(importer)


def deflate(data):
    stream = zlib.compressobj(level=9, wbits=-15)
    return stream.compress(data) + stream.flush()


def lz4_run(data):
    """One literal, one overlapping match, five final literals; original fixture."""
    assert len(data) >= 25 and data == data[:1] * len(data)
    extra = len(data) - 25
    extension = bytearray()
    while extra >= 255:
        extension.append(255)
        extra -= 255
    extension.append(extra)
    return b"\x1f" + data[:1] + b"\x01\x00" + extension + b"\x50" + data[-5:]


def lz4_run_late(data):
    """Another valid parse of a 2048-byte one-byte block: five literals, one
    distance-1 match, five final literals. 22 bytes; the importer emits 18."""
    assert len(data) == 2048 and data == data[:1] * 2048
    return b"\x5f" + data[:5] + b"\x01\x00" + b"\xff" * 7 + bytes((2019 - 7 * 255,)) + b"\x50" + data[-5:]


def lz4_literals(data):
    """Valid LZ4 holding only literals: 2057 bytes for a 2048-byte block."""
    extra = len(data) - 15
    return b"\xf0" + b"\xff" * (extra // 255) + bytes((extra % 255,)) + data


def boundary_blocks():
    """Random blocks with one early repeat; the importer's encoder gives LZ4
    forms of 2047, 2046, 2045 and 2044 bytes."""
    rng, blocks = random.Random(10), []
    for repeat in (14, 15, 16, 17):
        block = bytearray(rng.randbytes(2048))
        block[20:20 + repeat] = block[:repeat]
        blocks.append(bytes(block))
    return blocks


def cso(payloads, methods, *, magic=b"CISO", version=1, shift=0, block_size=2048):
    align = 1 << shift
    start = 24 + 4 * (len(payloads) + 1)
    start = (start + align - 1) // align * align
    indices, stored = [], bytearray(b"\xad" * (start - 24 - 4 * (len(payloads) + 1)))
    for data, method in zip(payloads, methods):
        flagged = method == "raw" if version != 2 else method == "lz4"
        indices.append(start >> shift | (0x80000000 if flagged else 0))
        block = (method if isinstance(method, bytes) else data if method == "raw"
                 else deflate(data) if method == "deflate" else lz4_run(data))
        if version == 2 and method == "raw":
            block = block.ljust(block_size, b"\0")
        stored.extend(block)
        padding = (-len(block)) % align
        stored.extend(b"\xad" * padding)
        start += len(block) + padding
    indices.append(start >> shift)
    header = struct.pack("<4sIQIBB2s", magic, 24, sum(map(len, payloads)), block_size, version, shift, b"\0\0")
    return header + struct.pack(f"<{len(indices)}I", *indices) + stored


WORDS = (b"sector", b"block", b"index", b"padding", b"literal", b"match", b"offset", b"track",
         b"session", b"volume", b"record", b"stream", b"cache", b"loader", b"texture", b"audio")


def iso_image(kinds, *, base=45000, seed=0):
    """Original synthetic ISO9660 data track: system area, primary volume
    descriptor at sector 16, terminator at 17, root directory at 18, then one
    2048-byte sector of each requested kind."""
    rng = random.Random(seed)
    sectors = [bytearray(2048) for _ in range(19)]
    sectors[0][:32] = b"KUI SYNTHETIC SYSTEM AREA 0123\0\0"
    root = bytearray(34)
    root[0], root[25], root[32] = 34, 2, 1
    root[2:10] = (base + 18).to_bytes(4, "little") + (base + 18).to_bytes(4, "big")
    root[10:18] = (2048).to_bytes(4, "little") + (2048).to_bytes(4, "big")
    sectors[16][:7], sectors[16][156:190] = b"\x01CD001\x01", root
    sectors[17][:7], sectors[18][:34] = b"\xffCD001\x01", root
    for kind in kinds:
        if kind == "zero":
            sectors.append(bytes(2048))
        elif kind == "run":
            sectors.append(bytes((rng.randrange(1, 256),)) * 2048)
        elif kind == "random":
            sectors.append(rng.randbytes(2048))
        elif kind == "text":
            sectors.append(b" ".join(rng.choice(WORDS) for _ in range(400))[:2048])
        else:
            period = rng.randbytes(rng.randrange(2, 9))
            sectors.append((period * 1024)[:2048])
    return b"".join(sectors)


MIXED = ("zero", "random", "text", "run", "pattern", "random", "text", "zero", "pattern", "run")


def zso_layout(data):
    fields = struct.unpack_from("<4sIQIBB2s", data)
    return fields, struct.unpack_from(f"<{fields[2] // 2048 + 1}I", data, 24)


def zso_file(blocks, *, shift=2, total=None):
    """Hand-built ZSO from (stored bytes, raw flag) pairs; every block starts aligned."""
    align = 1 << shift
    start = (24 + 4 * (len(blocks) + 1) + align - 1) // align * align
    body, indices = bytearray(start), []
    for stored, raw in blocks:
        indices.append(len(body) >> shift | (0x80000000 if raw else 0))
        body += stored + bytes(-len(stored) % align)
    indices.append(len(body) >> shift)
    struct.pack_into("<4sIQIBB2s", body, 0, b"ZISO", 24, total or 2048 * len(blocks), 2048, 1, shift, b"\0\0")
    struct.pack_into(f"<{len(indices)}I", body, 24, *indices)
    return bytes(body)


def lz4_reference(stored, size=2048):
    """Independent byte-ordered LZ4 decoder for one block. Returns the output,
    the input bytes used (any rest is padding), the final literal count and the
    start of the last match: the facts behind the console's end checks."""
    at, out, last_match = 0, bytearray(), None

    def length(value):
        nonlocal at
        extra = 255 if value == 15 else 0
        while extra == 255:
            extra = stored[at]
            at += 1
            value += extra
        return value

    while True:
        token = stored[at]
        at += 1
        literals = length(token >> 4)
        out += stored[at:at + literals]
        at += literals
        if len(out) >= size:
            assert len(out) == size, "decoded size"
            return bytes(out), at, literals, last_match
        distance = stored[at] | stored[at + 1] << 8
        at += 2
        count = length(token & 15) + 4
        assert 0 < distance <= len(out), "distance"
        last_match = len(out)
        for _ in range(count):
            out.append(out[-distance])


# Host program for the repository's own C reader. It decodes every block via
# 512-byte physical-sector views (as the console's card cache returns them)
# and via whole-file views; both must agree. KUI_TEST_VIEW_API selects the
# zero-copy view callback; 0 builds against the older read callback.
ZSO_HOST_CHECK_C = r"""
#include "kui/zso.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct source {
    const uint8_t *bytes;
    uint32_t size, window;
    uint32_t sector[128];
};
#if KUI_TEST_VIEW_API
static enum kui_zso_result input(void *context, uint32_t offset, uint32_t wanted,
                                 const uint8_t **data, uint32_t *available) {
    struct source *s = context;
    if(!wanted || offset >= s->size || wanted > s->size - offset) return KUI_ZSO_RANGE;
    uint32_t left = s->size - offset;
    if(s->window) {
        uint32_t base = offset - offset % s->window;
        uint32_t count = s->size - base < s->window ? s->size - base : s->window;
        memcpy(s->sector, s->bytes + base, count);
        *data = (const uint8_t *)s->sector + (offset - base);
        left = base + count - offset;
    } else {
        *data = s->bytes + offset;
    }
    *available = left < wanted ? left : wanted;
    return KUI_ZSO_OK;
}
#else
static enum kui_zso_result input(void *context, uint32_t offset, void *out, size_t size) {
    struct source *s = context;
    if(offset > s->size || size > s->size - offset) return KUI_ZSO_RANGE;
    memcpy(out, s->bytes + offset, size);
    return KUI_ZSO_OK;
}
#endif
static int decode(struct source *s, uint8_t *image, uint32_t *blocks) {
    struct kui_zso_info info;
    enum kui_zso_result result = s->size < KUI_ZSO_HEADER_BYTES ? KUI_ZSO_RANGE :
        kui_zso_header_parse_codec(s->bytes, s->size, KUI_ZSO_LZ4, &info);
    if(result == KUI_ZSO_OK) result = kui_zso_validate_index(&info, input, s);
    if(result != KUI_ZSO_OK) {
        fprintf(stderr, "header/index: %s\n", kui_zso_result_name(result));
        return 1;
    }
    for(uint32_t block = 0; block < info.block_count; ++block) {
        result = kui_zso_read_block(&info, block, input, s, image + (size_t)block * KUI_ZSO_BLOCK_BYTES);
        if(result != KUI_ZSO_OK) {
            fprintf(stderr, "block %lu: %s\n", (unsigned long)block, kui_zso_result_name(result));
            return 1;
        }
    }
    *blocks = info.block_count;
    return 0;
}
int main(int argc, char **argv) {
    if(argc != 3) return 2;
    FILE *in = fopen(argv[1], "rb");
    if(!in || fseek(in, 0, SEEK_END)) return 3;
    long length = ftell(in);
    if(length < 0 || length > 256L * 1024L * 1024L || fseek(in, 0, SEEK_SET)) return 3;
    uint8_t *bytes = calloc((size_t)length + 8u, 1);
    if(!bytes || fread(bytes, 1, (size_t)length, in) != (size_t)length) return 3;
    fclose(in);
    uint64_t total = 0;
    for(unsigned i = 0; i < 8u && length >= (long)KUI_ZSO_HEADER_BYTES; ++i)
        total |= (uint64_t)bytes[8 + i] << (8u * i);
    if(total > 256u * 1024u * 1024u) return 3;
    uint8_t *sectors = malloc((size_t)total + 1u), *whole = malloc((size_t)total + 1u);
    struct source s = {bytes, (uint32_t)length, 512u, {0}};
    uint32_t blocks = 0, again = 0;
    if(!sectors || !whole) return 3;
    if(decode(&s, sectors, &blocks)) return 1;
    s.window = 0;
    if(decode(&s, whole, &again)) return 1;
    size_t size = (size_t)blocks * KUI_ZSO_BLOCK_BYTES;
    if(again != blocks || memcmp(sectors, whole, size)) {
        fprintf(stderr, "sector and whole-file views decoded differently\n");
        return 1;
    }
    FILE *out = fopen(argv[2], "wb");
    if(!out || fwrite(sectors, 1, size, out) != size || fclose(out)) return 3;
    free(bytes);
    free(sectors);
    free(whole);
    return 0;
}
"""


def chd(*, version=5, tag=b"CHGD", subtype=b"NONE", logical=19584, cycle=False):
    length = {3: 120, 4: 108, 5: 124}[version]
    header = bytearray(length)
    struct.pack_into(">8sII", header, 0, b"MComprHD", length, version)
    struct.pack_into(">Q", header, 32 if version == 5 else 28, logical)
    struct.pack_into(">Q", header, 48 if version == 5 else 36, length)
    struct.pack_into(">I", header, {3: 76, 4: 44, 5: 56}[version], 19584)
    payload = b"TRACK:1 TYPE:MODE1_RAW SUBTYPE:" + subtype + b" FRAMES:1\0"
    if tag in (b"CHGD", b"CHGT"):
        payload = payload[:-1] + b" PAD:0 PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0\0"
    return header + struct.pack(">4sIQ", tag, len(payload), length if cycle else 0) + payload


class GameImageImportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="kui-import-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.original = self.root / "original"
        self.original.mkdir()
        self.source = self.original / "Synthetic.cso"
        self.output = self.root / "expanded"
        self.calls = self.root / "calls.jsonl"
        self.payloads = [b"A" * 2048, bytes(range(256)) * 8, b"Z" * 2048]

    def assert_not_published(self):
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob(".*.staging-*")), [])

    def write_cso(self, **kwargs):
        self.source.write_bytes(cso(self.payloads, ["deflate", "raw", "deflate"], **kwargs))

    def fake_chdman(self, *, fail=None, gdrom=True, unsafe=False, missing=False, extra=False):
        executable = self.root / "fake-chdman"
        executable.write_text(f'''#!{sys.executable}
import json
from pathlib import Path
import sys
args = sys.argv[1:]
with Path({str(self.calls)!r}).open("a") as stream:
    stream.write(json.dumps(args) + "\\n")
if args[0] == {fail!r}:
    print("synthetic failure")
    sys.exit(7)
if args[0] == "extractcd":
    output = Path(args[args.index("-o") + 1])
    name = "../outside.bin" if {unsafe!r} else "track01.bin"
    if {gdrom!r} and output.suffix == ".gdi":
        output.write_text('1\\n1 0 4 2352 "' + name + '" 0\\n')
    else:
        output.write_text('REM SESSION 01\\nFILE "' + name + '" BINARY\\n'
                          '  TRACK 01 MODE1/2352\\n    INDEX 01 00:00:00\\n')
    if not {missing!r}:
        (output.parent / "track01.bin").write_bytes(bytes([71]) * 2352)
    if {extra!r}:
        (output.parent / "unexpected.bin").write_bytes(bytes([72]) * 2352)
print("synthetic chdman completed")
''', encoding="utf-8")
        executable.chmod(0o755)
        return executable

    def test_cso_v1_mixed_exact_bytes_original_hash_and_saved_report(self):
        self.write_cso(shift=4)
        original = self.source.read_bytes()
        report = importer.import_image(self.source, self.output)
        data = b"".join(self.payloads)
        self.assertEqual((self.output / "disc.iso").read_bytes(), data)
        self.assertEqual(self.source.read_bytes(), original)
        self.assertEqual(report["source"]["sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual(report["outputs"], [{"file": "disc.iso", "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}])
        self.assertEqual(json.loads((self.output / "import-report.json").read_text()), report)
        self.assertFalse(report["verification"]["game_compatibility_checked"])

    def test_cso_v1_unreliable_header_and_reserved_fields_are_ignored(self):
        self.write_cso(version=0)
        data = bytearray(self.source.read_bytes())
        struct.pack_into("<I", data, 4, 0xdeadbeef)
        data[22:24] = b"xy"
        self.source.write_bytes(data)
        importer.import_image(self.source, self.output)
        self.assertEqual((self.output / "disc.iso").read_bytes(), b"".join(self.payloads))

    def test_cso_v2_deflate_lz4_and_uncompressed_with_flag_ignored_for_raw(self):
        self.payloads = [b"A" * 2048, b"B" * 2048, bytes(range(256)) * 8]
        data = bytearray(cso(self.payloads, ["deflate", "lz4", "raw"], version=2, shift=3))
        # v2 rawness comes from stored length; its high bit is ignored.
        struct.pack_into("<I", data, 32, struct.unpack_from("<I", data, 32)[0] | 0x80000000)
        self.source.write_bytes(data)
        report = importer.import_image(self.source, self.output)
        self.assertEqual(report["image"]["format"], "CSO v2")
        self.assertEqual((self.output / "disc.iso").read_bytes(), b"".join(self.payloads))

    def test_zso_lz4_overlapping_match_padding_and_raw(self):
        self.payloads = [b"Q" * 2048, bytes(range(256)) * 8]
        self.source = self.original / "Synthetic.zso"
        self.source.write_bytes(cso(self.payloads, ["lz4", "raw"], magic=b"ZISO", shift=5))
        report = importer.import_image(self.source, self.output)
        self.assertEqual(report["image"]["format"], "ZSO (LZ4)")
        self.assertEqual((self.output / "disc.iso").read_bytes(), b"".join(self.payloads))

    def test_partial_last_block_and_non_power_of_two_block_size(self):
        self.payloads = [b"a" * 6144, b"z" * 2048]
        self.source.write_bytes(cso(self.payloads, ["deflate", "deflate"], block_size=6144))
        importer.import_image(self.source, self.output)
        self.assertEqual((self.output / "disc.iso").read_bytes(), b"".join(self.payloads))

    def test_system_lz4_encoder_interoperability(self):
        library = ctypes.util.find_library("lz4")
        if not library:
            self.skipTest("system liblz4 is unavailable")
        codec = ctypes.CDLL(library)
        codec.LZ4_compressBound.argtypes = (ctypes.c_int,)
        codec.LZ4_compressBound.restype = ctypes.c_int
        codec.LZ4_compress_default.argtypes = (ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int)
        codec.LZ4_compress_default.restype = ctypes.c_int
        for size in (2048, 6144, 65536, 1024 * 1024):
            data = (b"interoperability" + bytes(range(256))) * (size // 272 + 1)
            data = data[:size]
            destination = ctypes.create_string_buffer(codec.LZ4_compressBound(size))
            actual = codec.LZ4_compress_default(data, destination, size, len(destination))
            self.assertGreater(actual, 0)
            decoded, consumed = importer.decode_lz4(destination.raw[:actual], size)
            self.assertEqual(decoded, data)
            self.assertEqual(consumed, actual)

    def test_invalid_lz4_offsets_lengths_and_end_conditions(self):
        for data, message in ((b"\x00\x00\x00", "offset"), (b"\x00\x01\x00", "offset"),
                              (b"\xf0\xff", "length"), (b"\xf0\xff" * 10, "length"),
                              (b"\x10x\x01\x00", "incomplete|Truncated")):
            with self.subTest(data=data):
                with self.assertRaisesRegex(ValueError, message):
                    importer.decode_lz4(data, 32)
        with self.assertRaisesRegex(ValueError, "incorrect output"):
            importer.decode_deflate(deflate(b"x" * 4096), 2048)

    def test_lzo_dependency_failure_is_clear_and_does_not_publish(self):
        self.source.write_bytes(cso([b"Q" * 2048], ["lz4"], magic=b"ZISO"))
        importer.load_lzo_decoder.cache_clear()
        self.addCleanup(importer.load_lzo_decoder.cache_clear)
        with patch.object(importer.ctypes.util, "find_library", return_value=None):
            with self.assertRaisesRegex(ValueError, "installed liblzo2"):
                importer.import_image(self.source, self.output, zso_codec="lzo")
        self.assert_not_published()
        with self.assertRaisesRegex(ValueError, "Cannot load safe LZO decoder"):
            importer.import_image(self.source, self.output, zso_codec="lzo",
                                  lzo_library="nonexistent-liblzo2-kui-test")
        self.assert_not_published()
        with self.assertRaisesRegex(ValueError, "automatic guessing"):
            importer.import_image(self.source, self.output, zso_codec="auto")
        self.assert_not_published()

    def test_lzo_safe_encoder_interoperability_padding_and_explicit_dialect(self):
        library = os.environ.get("KUI_TEST_LZO_LIBRARY") or ctypes.util.find_library("lzo2")
        if not library:
            self.skipTest("liblzo2 unavailable; KUI_TEST_LZO_LIBRARY can select a test oracle")
        codec = ctypes.CDLL(library)
        codec.lzo1x_1_compress.argtypes = (ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                          ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p)
        codec.lzo1x_1_compress.restype = ctypes.c_int
        for size in (2048, 6144, 65536, 1024 * 1024):
            data = ((b"LZO dialect" + bytes(range(256))) * (size // 267 + 1))[:size]
            destination = ctypes.create_string_buffer(size + size // 16 + 67)
            written = ctypes.c_size_t(len(destination))
            workspace = ctypes.create_string_buffer(1024 * 1024)
            status = codec.lzo1x_1_compress(data, size, destination, ctypes.byref(written), workspace)
            self.assertEqual(status, 0)
            encoded = destination.raw[:written.value]
            for padding in (0, 1, 15, 65535):
                decoded, consumed = importer.decode_lzo(encoded + b"\xad" * padding, size, padding, library)
                self.assertEqual(decoded, data)
                self.assertEqual(consumed, len(encoded))
            with self.assertRaisesRegex(ValueError, "oversized"):
                importer.decode_lzo(encoded, size - 1, library=library)
            with self.assertRaisesRegex(ValueError, "excess padding"):
                importer.decode_lzo(encoded + b"padding", size, 1, library)
            if size == 2048:
                self.source.write_bytes(cso([data], [encoded], magic=b"ZISO", shift=4))
                with self.assertRaisesRegex(ValueError, "LZ4"):
                    importer.import_image(self.source, self.output)
                self.assert_not_published()
                report = importer.import_image(self.source, self.output, zso_codec="lzo", lzo_library=library)
                self.assertEqual(report["image"]["zso_codec"], "lzo")
                self.assertEqual(report["image"]["format"], "ZSO (LZO1X)")
                self.assertEqual((self.output / "disc.iso").read_bytes(), data)
                shutil.rmtree(self.output)

    def test_indices_into_header_repeated_backward_and_past_eof_are_rejected(self):
        self.write_cso()
        original = self.source.read_bytes()
        first, second = struct.unpack_from("<II", original, 24)
        for offset, value, message in ((24, 0, "header/index"), (28, first, "increase"),
                                       (28, first - 1, "increase"), (36, len(original) + 1, "within the file")):
            with self.subTest(offset=offset, value=value):
                data = bytearray(original)
                struct.pack_into("<I", data, offset, value)
                self.source.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    importer.import_image(self.source, self.output)
                self.assert_not_published()

    def test_bad_headers_size_limits_truncation_and_unsupported_extensions(self):
        self.write_cso()
        original = self.source.read_bytes()
        cases = [(8, "<Q", 0, "Uncompressed"), (8, "<Q", 2**40, "Uncompressed"),
                 (16, "<I", 0, "Block size"), (16, "<I", 2049, "Block size"),
                 (21, "<B", 17, "shift"), (20, "<B", 7, "ZSO")]
        for offset, encoding, value, message in cases:
            with self.subTest(offset=offset, value=value):
                data = bytearray(original)
                struct.pack_into(encoding, data, offset, value)
                self.source.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    importer.import_image(self.source, self.output)
                self.assert_not_published()
        self.source.write_bytes(original[:26])
        with self.assertRaisesRegex(ValueError, "index"):
            importer.import_image(self.source, self.output)
        self.source.write_bytes(b"unsupported nrg contents")
        with self.assertRaisesRegex(ValueError, "Unsupported image"):
            importer.import_image(self.source, self.output)
        self.assert_not_published()

    def test_deflate_damage_and_extra_padding_do_not_publish_partial_output(self):
        self.write_cso()
        data = bytearray(self.source.read_bytes())
        first = struct.unpack_from("<I", data, 24)[0]
        data[first:first + 4] = b"\xff" * 4
        self.source.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "Block 0"):
            importer.import_image(self.source, self.output)
        self.assert_not_published()
        self.write_cso()
        self.source.write_bytes(self.source.read_bytes() + b"trailing")
        with self.assertRaisesRegex(ValueError, "after final"):
            importer.import_image(self.source, self.output)
        self.assert_not_published()

    def test_source_mutation_aborts_and_original_directory_remains_unwritten(self):
        self.write_cso()
        names = sorted(path.name for path in self.original.iterdir())

        def change_source(_line):
            with self.source.open("r+b") as stream:
                stream.seek(-1, 2)
                stream.write(b"!")

        with self.assertRaisesRegex(ValueError, "Source changed"):
            importer.import_image(self.source, self.output, progress=change_source)
        self.assert_not_published()
        self.assertEqual(sorted(path.name for path in self.original.iterdir()), names)

    def test_existing_target_source_descendant_and_symlink_refused(self):
        self.write_cso()
        self.output.mkdir()
        with self.assertRaisesRegex(ValueError, "already exists"):
            importer.import_image(self.source, self.output)
        self.output.rmdir()
        with self.assertRaisesRegex(ValueError, "outside"):
            importer.import_image(self.source, self.original / "copy")
        link = self.root / "source-link.cso"
        link.symlink_to(self.source)
        with self.assertRaisesRegex(ValueError, "regular file"):
            importer.import_image(link, self.output)
        self.assert_not_published()

    def test_target_created_during_import_is_not_overwritten(self):
        self.write_cso()
        publish = importer.publish_directory

        def concurrent_create(stage, target):
            target.mkdir()
            publish(stage, target)

        with patch.object(importer, "publish_directory", concurrent_create):
            with self.assertRaises(FileExistsError):
                importer.import_image(self.source, self.output)
        self.assertTrue(self.output.is_dir())
        self.assertEqual(list(self.output.iterdir()), [])
        self.assertEqual(list(self.root.glob(".*.staging-*")), [])

    def test_chd_gd_versions_keep_gdi_addresses_and_hash_normalized_outputs(self):
        executable = self.fake_chdman()
        for version in (3, 4, 5):
            with self.subTest(version=version):
                self.source.write_bytes(chd(version=version))
                original = self.source.read_bytes()
                report = importer.import_image(self.source, self.output, chdman=executable)
                self.assertEqual(report["launch_file"], "disc.gdi")
                self.assertTrue(report["image"]["gd_geometry_verified"])
                self.assertTrue(report["image"]["chdman_verified"])
                self.assertEqual(report["image"]["version"], version)
                self.assertEqual(self.source.read_bytes(), original)
                self.assertEqual((self.output / "track01.bin").read_bytes(), b"G" * 2352)
                for record in report["outputs"]:
                    data = (self.output / record["file"]).read_bytes()
                    self.assertEqual(record["sha256"], hashlib.sha256(data).hexdigest())
                calls = [json.loads(line) for line in self.calls.read_text().splitlines()]
                self.assertEqual([call[0] for call in calls[-2:]], ["verify", "extractcd"])
                self.assertNotIn("-sb", calls[-1])
                self.assertEqual(Path(calls[-1][calls[-1].index("-o") + 1]).suffix,
                                 ".gdi")
                self.assertNotIn("--fix", calls[-2])
                shutil.rmtree(self.output)

    def test_chd_cd_cue_session_and_parent_are_preserved_in_workflow(self):
        self.source.write_bytes(chd(tag=b"CHT2"))
        parent = self.original / "Parent.chd"
        parent.write_bytes(chd(tag=b"CHT2"))
        executable = self.fake_chdman(gdrom=False)
        report = importer.import_image(self.source, self.output, chdman=executable, parent=parent)
        self.assertEqual(report["launch_file"], "disc.cue")
        self.assertIn("REM SESSION 01", (self.output / "disc.cue").read_text())
        self.assertEqual(report["parent"]["sha256"], hashlib.sha256(parent.read_bytes()).hexdigest())
        for call in map(json.loads, self.calls.read_text().splitlines()):
            self.assertEqual(call[call.index("-ip") + 1], str(parent))

    def test_chd_subchannel_drop_requires_explicit_opt_in(self):
        self.source.write_bytes(chd(subtype=b"RW_RAW"))
        executable = self.fake_chdman()
        with self.assertRaisesRegex(ValueError, "--drop-subchannels"):
            importer.import_image(self.source, self.output, chdman=executable)
        self.assertFalse(self.calls.exists())
        self.assert_not_published()
        report = importer.import_image(self.source, self.output, chdman=executable, drop_subchannels=True)
        self.assertTrue(report["image"]["subchannel_omitted"])
        self.assertFalse(report["image"]["subchannel_preserved"])

    def test_chd_verify_and_extract_failures_do_not_publish(self):
        self.source.write_bytes(chd())
        for operation in ("verify", "extractcd"):
            with self.subTest(operation=operation):
                executable = self.fake_chdman(fail=operation)
                with self.assertRaisesRegex(ValueError, f"{operation} failed.*synthetic failure"):
                    importer.import_image(self.source, self.output, chdman=executable)
                self.assert_not_published()

    def test_chd_invalid_metadata_non_cd_missing_tool_and_size_limit(self):
        for data, message in ((chd(cycle=True), "cyclic"), (chd(tag=b"GDDD"), "not a CD/GD"),
                              (chd(logical=importer.MAX_IMAGE_BYTES + 1), "logical size")):
            with self.subTest(message=message):
                self.source.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    importer.import_image(self.source, self.output)
                self.assert_not_published()
        self.source.write_bytes(chd())
        with self.assertRaisesRegex(ValueError, "requires MAME chdman"):
            importer.import_image(self.source, self.output, chdman="nonexistent-chdman-kui-test")
        self.assert_not_published()

    def test_chd_unsafe_missing_and_unreferenced_files_rejected(self):
        self.source.write_bytes(chd())
        for options, message in (({"unsafe": True}, "Unsafe or missing"),
                                  ({"missing": True}, "Unsafe or missing"),
                                  ({"extra": True}, "Unreferenced")):
            with self.subTest(options=options):
                executable = self.fake_chdman(**options)
                with self.assertRaisesRegex(ValueError, message):
                    importer.import_image(self.source, self.output, chdman=executable)
                self.assert_not_published()

    def test_chd_gdi_unsupported_stride_and_geometry_are_not_published(self):
        self.source.write_bytes(chd())
        executable = self.fake_chdman()
        run = importer.run_chdman
        for row, size in (("1 45000 4 2336 track01.bin 0", 2336),
                          ("1 45000 4 2448 track01.bin 0", 2448),
                          ("1 719850 4 2352 track01.bin 0", 2352)):
            with self.subTest(row=row):
                def malformed(command, stage, timeout):
                    result = run(command, stage, timeout)
                    if command[1] == "extractcd":
                        (stage / "disc.gdi").write_text("1\n" + row + "\n")
                        (stage / "track01.bin").write_bytes(b"G" * size)
                    return result
                with patch.object(importer, "run_chdman", malformed):
                    with self.assertRaises(ValueError):
                        importer.import_image(self.source, self.output, chdman=executable)
                self.assert_not_published()

    def test_chd_cue_missing_duplicate_reversed_and_out_of_bounds_indices_rejected(self):
        self.source.write_bytes(chd(tag=b"CHT2"))
        executable = self.fake_chdman(gdrom=False)
        run = importer.run_chdman
        prefix = 'FILE "track01.bin" BINARY\n TRACK 01 MODE1/2352\n'
        suffixes = ("", "INDEX 01\n", "INDEX 01 invalid\n", "INDEX 01 00:60:00\n",
                    "INDEX 01 00:00:75\n", "INDEX 01 00:00:01\n",
                    "INDEX 02 00:00:00\n", "INDEX 01 00:00:00\nINDEX 01 00:00:00\n",
                    "INDEX 01 00:00:00\nINDEX 00 00:00:00\n",
                    "INDEX 01 00:00:00\nINDEX 02 00:00:01\n",
                    "INDEX 01 00:00:00\nREM SESSION 03\n",
                    "INDEX 01 00:00:00\nREM SESSION 02\nREM SESSION 01\n",
                    "INDEX 01 00:00:00\nPREGAP 159:57:74\nPOSTGAP 00:00:02\n",
                    "INDEX 01 00:00:00\nPREGAP 00:00:00\nPREGAP 00:00:00\n",
                    "INDEX 01 00:00:00\nREM LEAD-IN 00:00:01\n",
                    "INDEX 01 00:00:00\nREM LEAD-OUT 00:00:01\n",
                    "INDEX 01 00:00:00\nBOGUS data\n",
                    "INDEX 01 00:00:00\nREM nul\0\n")
        for suffix in suffixes:
            with self.subTest(suffix=suffix):
                def malformed(command, stage, timeout):
                    result = run(command, stage, timeout)
                    if command[1] == "extractcd":
                        (stage / "disc.cue").write_text(prefix + suffix)
                    return result
                with patch.object(importer, "run_chdman", malformed):
                    with self.assertRaises(ValueError):
                        importer.import_image(self.source, self.output, chdman=executable)
                self.assert_not_published()

    def test_extracted_cue_valid_indices_gaps_and_two_sessions(self):
        text = ('REM SESSION 01\nTITLE "metadata café"\nREM\n'
                'FILE "first.bin" BINARY\nTRACK 01 AUDIO\nINDEX 00 00:00:00\n'
                'INDEX 01 00:00:01\nINDEX 02 00:00:02\nPOSTGAP 00:00:01\n'
                'REM LEAD-OUT 00:00:01\nREM SESSION 02\nREM LEAD-IN 00:00:01\n'
                'FILE "second.bin" BINARY\nTRACK 02 MODE2/2336\nPREGAP 00:00:01\n'
                'INDEX 01 00:00:00\n')
        count, references = importer.validate_cue(text, {"first.bin": ("first.bin", 2352 * 3),
                                                       "second.bin": ("second.bin", 2336 * 2)})
        self.assertEqual(count, 2)
        self.assertEqual(references, {"first.bin", "second.bin"})
        with self.assertRaisesRegex(ValueError, "split-track"):
            importer.validate_cue(text + "TRACK 03 AUDIO\nINDEX 01 00:00:00\n",
                                  {"first.bin": ("first.bin", 2352 * 3),
                                   "second.bin": ("second.bin", 2336 * 2)})

    def test_chd_timeout_cleanup(self):
        self.source.write_bytes(chd())
        executable = self.fake_chdman()
        with patch.object(importer.subprocess, "run", side_effect=subprocess.TimeoutExpired("chdman", 1)):
            with self.assertRaisesRegex(ValueError, "time limit"):
                importer.import_image(self.source, self.output, chdman=executable, timeout=1)
        self.assert_not_published()

    def test_real_chdman_round_trip_when_installed(self):
        executable = shutil.which("chdman")
        if not executable:
            self.skipTest("MAME chdman unavailable; controlled subprocess integration is tested separately")
        payload = b"".join(self.payloads)
        (self.original / "track.bin").write_bytes(payload)
        cue = self.original / "input.cue"
        cue.write_text('FILE "track.bin" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n')
        self.source = self.original / "Synthetic.chd"
        created = subprocess.run([executable, "createcd", "-i", str(cue), "-o", str(self.source),
                                  "-c", "none"], capture_output=True, text=True, check=False)
        self.assertEqual(created.returncode, 0, created.stdout + created.stderr)
        report = importer.import_image(self.source, self.output, chdman=executable)
        self.assertEqual(report["launch_file"], "disc.cue")
        bins = [record for record in report["outputs"] if record["file"].endswith(".bin")]
        self.assertEqual(len(bins), 1)
        self.assertEqual((self.output / bins[0]["file"]).read_bytes(), payload)

    def test_cli_success_and_failure(self):
        self.write_cso()
        command = [sys.executable, str(ROOT / "tools/game_image_import.py"), str(self.source), str(self.output)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("disc.iso", result.stdout)
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Output already exists", result.stderr)

    # --output-format zso: one standard LZ4 ZSO for the console's native reader.

    def write_iso(self, kinds=MIXED * 3, name="Synthetic Game.iso"):
        data = iso_image(kinds)
        self.source = self.original / name
        self.source.write_bytes(data)
        return data

    def assert_zso_matches(self, report, expected, output=None):
        """Independent container checks and reference decode of every block."""
        data = ((output or self.output) / report["launch_file"]).read_bytes()
        fields, index = zso_layout(data)
        self.assertEqual(fields, (b"ZISO", 24, len(expected), 2048, 1, 2, b"\0\0"))
        self.assertEqual(len(index), len(expected) // 2048 + 1)
        self.assertEqual((index[0] & 0x7fffffff) << 2, 24 + 4 * len(index))
        self.assertEqual(index[-1] << 2, len(data))
        blocks, decoded = [], bytearray()
        for number in range(len(index) - 1):
            start, end = (index[number] & 0x7fffffff) << 2, (index[number + 1] & 0x7fffffff) << 2
            raw, stored = bool(index[number] & 0x80000000), data[start:end]
            if raw:
                self.assertEqual(len(stored), 2048)
                decoded += stored
            else:
                block, used, literals, last_match = lz4_reference(stored)
                # Compressed only when smaller than raw storage, padding included.
                self.assertLess(len(stored), 2048)
                self.assertEqual(len(stored), (used + 3) // 4 * 4)
                self.assertGreaterEqual(literals, 5)
                self.assertTrue(last_match is None or last_match <= 2048 - 12)
                decoded += block
            blocks.append((raw, len(stored)))
        self.assertEqual(bytes(decoded), expected)
        return blocks

    def test_zso_from_iso_mixed_blocks_alignment_progress_and_report(self):
        expected = self.write_iso()
        original = self.source.read_bytes()
        lines = []
        report = importer.import_image(self.source, self.output, output_format="zso", progress=lines.append)
        self.assertEqual(sorted(path.name for path in self.output.iterdir()),
                         ["Synthetic Game.zso", "import-report.json"])
        self.assertEqual(report["launch_file"], "Synthetic Game.zso")
        blocks = self.assert_zso_matches(report, expected)
        kinds = ["system"] * 19 + list(MIXED * 3)
        # Incompressible blocks are stored raw; one-byte blocks take the
        # 18-byte run encoding plus two bytes of 4-byte alignment padding.
        self.assertEqual([raw for raw, _ in blocks], [kind == "random" for kind in kinds])
        self.assertEqual({span for (_, span), kind in zip(blocks, kinds) if kind in ("zero", "run")}, {20})
        data = (self.output / "Synthetic Game.zso").read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        zso = report["zso"]
        self.assertEqual(report["output_format"], "zso")
        self.assertEqual((zso["codec"], zso["method"], zso["shift"], zso["alignment_bytes"], zso["block_bytes"]),
                         ("lz4", "encoded", 2, 4, 2048))
        self.assertEqual((zso["blocks"], zso["raw_blocks"], zso["compressed_blocks"]),
                         (len(kinds), kinds.count("random"), len(kinds) - kinds.count("random")))
        self.assertEqual((zso["file"], zso["bytes"], zso["sha256"]), ("Synthetic Game.zso", len(data), digest))
        self.assertEqual((zso["logical_bytes"], zso["logical_sha256"]),
                         (len(expected), hashlib.sha256(expected).hexdigest()))
        self.assertAlmostEqual(zso["ratio"], len(data) / len(expected), places=5)
        self.assertLess(zso["ratio"], 0.6)
        self.assertGreaterEqual(zso["elapsed_seconds"], 0)
        self.assertEqual(report["outputs"], [{"file": "Synthetic Game.zso", "bytes": len(data), "sha256": digest}])
        self.assertEqual(report["source"]["sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual(report["image"], {"format": "ISO", "uncompressed_bytes": len(expected),
                                           "logical_sha256": zso["logical_sha256"],
                                           "data_image": {"iso9660_pvd_sector": 16, "session_lba": 45000}})
        self.assertEqual(report["verification"], {
            "source_unchanged": True, "output_sha256_recorded": True, "zso_blocks_decoded": True,
            "logical_sha256_matches_source": True, "catalogue_match_checked": False,
            "game_compatibility_checked": False})
        self.assertEqual(json.loads((self.output / "import-report.json").read_text()), report)
        self.assertEqual(self.source.read_bytes(), original)
        count = len(kinds)
        self.assertEqual(lines, [f"Converting ISO to ZSO: 1/{count} blocks",
                                 f"Converting ISO to ZSO: {count}/{count} blocks",
                                 f"Verifying ZSO: 1/{count} blocks", f"Verifying ZSO: {count}/{count} blocks"])
        verified = importer.verify_zso(self.output / "Synthetic Game.zso")
        self.assertEqual((verified["logical_sha256"], verified["sha256"], verified["raw_blocks"]),
                         (zso["logical_sha256"], digest, zso["raw_blocks"]))

    def test_zso_from_cso_v1_v2_and_larger_source_blocks(self):
        expected = iso_image(MIXED * 3, seed=3)
        sectors = [expected[offset:offset + 2048] for offset in range(0, len(expected), 2048)]
        large = [expected[offset:offset + 8192] for offset in range(0, len(expected), 8192)]
        self.assertNotEqual(len(large[-1]), 8192)  # Partial final source block.
        v2_methods = ["lz4" if len(set(sector)) == 1 else "raw" if len(set(sector)) > 200 else "deflate"
                      for sector in sectors]
        cases = (("CSO v1", cso(sectors, ["deflate"] * len(sectors))),
                 ("CSO v2", cso(sectors, v2_methods, version=2, shift=3)),
                 ("CSO v1", cso(large, ["deflate"] * len(large), block_size=8192, shift=4)))
        for number, (kind, data) in enumerate(cases):
            with self.subTest(kind=kind, number=number):
                self.source = self.original / f"Synthetic {number}.cso"
                self.source.write_bytes(data)
                output = self.root / f"converted-{number}"
                report = importer.import_image(self.source, output, output_format="zso")
                self.assertEqual((report["image"]["format"], report["zso"]["method"]), (kind, "encoded"))
                self.assertEqual(report["image"]["logical_sha256"], hashlib.sha256(expected).hexdigest())
                self.assertEqual(report["launch_file"], f"Synthetic {number}.zso")
                blocks = self.assert_zso_matches(report, expected, output)
                self.assertEqual(report["zso"]["raw_blocks"], sum(raw for raw, _ in blocks))

    def test_zso_from_lz4_zso_is_copied_or_repacked_at_shift_2_without_reencoding(self):
        expected = iso_image(MIXED * 2, seed=4)
        sectors = [expected[offset:offset + 2048] for offset in range(0, len(expected), 2048)]
        runs = [len(set(sector)) == 1 for sector in sectors]
        # The 22-byte parse differs from the encoder's 18 bytes, so the output
        # shows whether the source's own LZ4 streams were kept.
        methods = [lz4_run_late(sector) if run else "raw" for sector, run in zip(sectors, runs)]
        self.source = self.original / "Synthetic.zso"
        # 2048-byte blocks at shift 2, each LZ4 block under 2048 bytes: the
        # file is copied unchanged, then decoded and compared like any output.
        ready = cso(sectors, methods, magic=b"ZISO", shift=2)
        self.source.write_bytes(ready)
        report = importer.import_image(self.source, self.output, output_format="zso")
        self.assertEqual((report["image"]["format"], report["zso"]["method"], report["zso"]["encoder"]),
                         ("ZSO (LZ4)", "copied", None))
        self.assertEqual((self.output / "Synthetic.zso").read_bytes(), ready)
        self.assertEqual(report["zso"]["sha256"], report["source"]["sha256"])
        self.assertEqual(report["zso"]["raw_blocks"], runs.count(False))
        self.assertEqual(report["zso"]["logical_sha256"], hashlib.sha256(expected).hexdigest())
        # Any other shift is rewritten at shift 2 from the same streams, with
        # zero padding: a shift-9 source shrinks to the shift-2 layout.
        repacked = zso_file([(stream, False) if run else (sector, True)
                             for stream, sector, run in zip(methods, sectors, runs)])
        self.assertEqual(len(repacked), len(ready))
        count = len(sectors)
        for shift in (0, 1, 3, 9):
            with self.subTest(shift=shift):
                source = cso(sectors, methods, magic=b"ZISO", shift=shift)
                self.source.write_bytes(source)
                output, lines = self.root / f"repacked-{shift}", []
                report = importer.import_image(self.source, output, output_format="zso", progress=lines.append)
                self.assertEqual((report["zso"]["method"], report["zso"]["encoder"], report["zso"]["shift"]),
                                 ("repacked", None, 2))
                self.assertEqual(report["image"]["alignment_bytes"], 1 << shift)
                self.assertEqual(self.assert_zso_matches(report, expected, output),
                                 [(False, 24) if run else (True, 2048) for run in runs])
                self.assertEqual((output / "Synthetic.zso").read_bytes(), repacked)
                self.assertEqual(report["zso"]["bytes"], len(repacked))
                if shift == 9:
                    self.assertLess(len(repacked), len(source))
                self.assertEqual(lines, [f"Repacking ZSO (LZ4) at shift 2: 1/{count} blocks",
                                         f"Repacking ZSO (LZ4) at shift 2: {count}/{count} blocks",
                                         f"Verifying ZSO: 1/{count} blocks", f"Verifying ZSO: {count}/{count} blocks"])
        # At shift 2, an LZ4 block that is not smaller than raw storage also
        # rules out the copy: the file is repacked and that block stored raw.
        wasteful = list(methods)
        wasteful[20] = lz4_literals(sectors[20])
        self.assertEqual(len(wasteful[20]), 2057)
        self.source.write_bytes(cso(sectors, wasteful, magic=b"ZISO", shift=2))
        report = importer.import_image(self.source, self.root / "wasteful", output_format="zso")
        self.assertEqual(report["zso"]["method"], "repacked")
        self.assertEqual((self.root / "wasteful" / "Synthetic.zso").read_bytes(), repacked)
        shutil.rmtree(self.output)
        # 4096-byte blocks are split into 2048-byte blocks and encoded.
        pairs = [expected[offset:offset + 4096] for offset in range(0, len(expected), 4096)]
        self.source.write_bytes(cso(pairs, ["lz4" if len(set(pair)) == 1 else "raw" for pair in pairs],
                                    magic=b"ZISO", shift=2, block_size=4096))
        report = importer.import_image(self.source, self.output, output_format="zso")
        self.assertEqual(report["zso"]["method"], "encoded")
        self.assert_zso_matches(report, expected)

    def test_zso_from_historical_lzo_zso(self):
        library = os.environ.get("KUI_TEST_LZO_LIBRARY") or ctypes.util.find_library("lzo2")
        if not library:
            self.skipTest("liblzo2 unavailable; KUI_TEST_LZO_LIBRARY can select a test oracle")
        codec = ctypes.CDLL(library)
        codec.lzo1x_1_compress.argtypes = (ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                          ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p)
        codec.lzo1x_1_compress.restype = ctypes.c_int
        expected = iso_image(MIXED * 2, seed=5)
        sectors = [expected[offset:offset + 2048] for offset in range(0, len(expected), 2048)]
        methods = []
        for sector in sectors:
            destination = ctypes.create_string_buffer(2048 + 2048 // 16 + 67)
            written = ctypes.c_size_t(len(destination))
            workspace = ctypes.create_string_buffer(1024 * 1024)
            self.assertEqual(codec.lzo1x_1_compress(sector, 2048, destination, ctypes.byref(written), workspace), 0)
            methods.append("raw" if written.value >= 2048 else destination.raw[:written.value])
        self.source = self.original / "Historical.zso"
        self.source.write_bytes(cso(sectors, methods, magic=b"ZISO", shift=2))
        report = importer.import_image(self.source, self.output, output_format="zso",
                                       zso_codec="lzo", lzo_library=library)
        self.assertEqual((report["image"]["format"], report["image"]["zso_codec"], report["zso"]["method"]),
                         ("ZSO (LZO1X)", "lzo", "encoded"))
        self.assert_zso_matches(report, expected)

    def test_lz4_encoder_fast_paths_end_rules_and_independent_decoders(self):
        self.assertEqual(importer.encode_lz4_block(bytes(2048)), lz4_run(bytes(2048)))
        self.assertEqual(len(importer.encode_lz4_block(b"\xff" * 2048)), 18)
        rng = random.Random(6)
        self.assertIsNone(importer.encode_lz4_block(rng.randbytes(2048)))
        system = None
        library = ctypes.util.find_library("lz4")
        if library:
            system = ctypes.CDLL(library).LZ4_decompress_safe
            system.argtypes = (ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int)
            system.restype = ctypes.c_int
        text = b" ".join(rng.choice(WORDS) for _ in range(400))[:2048]
        blocks = [text, b"\xab" * 2047 + b"\xcd", bytes(700) + rng.randbytes(600) + text[:748]]
        blocks += [(rng.randbytes(period) * 2048)[:2048] for period in range(2, 10)]
        optional = set()  # Mostly random literals: raw storage is allowed.
        for tail in range(4, 60, 5):
            prefix = rng.randbytes(2048 - tail)
            optional.add(prefix + prefix[:tail])  # Repeats that reach the block end.
            blocks.append(prefix + prefix[:tail])
        for start in (2030, 2036, 2037, 2040):
            body = bytes(1000) + rng.randbytes(start - 1000)
            blocks.append(body + body[1000:1000 + 2048 - start])  # Match candidates near the end.
        for _ in range(200):
            block = bytearray()
            while len(block) < 2048:
                choice, size = rng.randrange(4), rng.choice((1, 3, 4, 5, 12, 40, 300))
                if choice == 0:
                    block += rng.randbytes(size)
                elif choice == 1 or not block:
                    block += bytes((rng.randrange(256),)) * size
                else:
                    distance = rng.randrange(1, len(block) + 1)
                    for _ in range(size):
                        block.append(block[-distance])
            blocks.append(bytes(block[:2048]))
        for block in blocks:
            encoded = importer.encode_lz4_block(block)
            if encoded is None and block in optional:
                continue
            self.assertIsNotNone(encoded)
            self.assertLess(len(encoded), 2048)
            decoded, used, literals, last_match = lz4_reference(encoded)
            self.assertEqual((decoded, used), (block, len(encoded)))
            self.assertGreaterEqual(literals, 5)
            self.assertTrue(last_match is None or last_match <= 2048 - 12)
            self.assertEqual(importer.decode_lz4(encoded + b"\xad" * 3, 2048, 3), (block, len(encoded)))
            with self.assertRaisesRegex(ValueError, "excess padding"):
                importer.decode_lz4(encoded + bytes(4), 2048, 3)
            if system:
                output = ctypes.create_string_buffer(2048)
                self.assertEqual(system(encoded, output, len(encoded), 2048), 2048)
                self.assertEqual(output.raw, block)

    def test_zso_block_is_raw_unless_its_padded_lz4_form_is_smaller(self):
        extra = boundary_blocks()
        self.assertEqual([len(importer.encode_lz4_block(block)) for block in extra], [2047, 2046, 2045, 2044])
        expected = iso_image(MIXED) + b"".join(extra)
        self.source = self.original / "Boundary.iso"
        self.source.write_bytes(expected)
        report = importer.import_image(self.source, self.output, output_format="zso")
        # Padded to 4 bytes, 2045..2047 bytes take as much room as raw storage.
        self.assertEqual(self.assert_zso_matches(report, expected)[-4:], [(True, 2048)] * 3 + [(False, 2044)])
        self.assertEqual(report["zso"]["raw_blocks"], MIXED.count("random") + 3)

    def test_zso_shift_selection_and_launch_name(self):
        self.assertEqual(importer.zso_shift_for(24 + 8 + 2048), 2)
        self.assertEqual(importer.zso_shift_for(0x7FFFFFFF << 2), 2)
        # The writer pads the end up to the alignment, so one byte more would
        # make the terminal entry 0x80000000, the raw flag, at shift 2.
        self.assertEqual(importer.zso_shift_for((0x7FFFFFFF << 2) + 1), 3)
        self.assertEqual(importer.zso_shift_for((0x7FFFFFFF << 2) + 4), 3)
        self.assertEqual(importer.zso_shift_for(0x7FFFFFFF << 9), 9)
        with self.assertRaisesRegex(ValueError, "shift 9"):
            importer.zso_shift_for((0x7FFFFFFF << 9) + 1)
        # The largest admitted image stays at shift 2 even if every block is raw.
        largest = 24 + 4 * (importer.LBA_LIMIT + 1) + importer.LBA_LIMIT * 2048
        self.assertEqual(importer.zso_shift_for(largest), 2)
        name = importer.zso_name
        self.assertEqual(name(Path("/in/Game (Disc 1).cso"), Path("/out/Ready")), "Game (Disc 1).zso")
        self.assertEqual(name(Path("/in/Gâme.cso"), Path("/out/Ready")), "Ready.zso")
        self.assertEqual(name(Path("/in/Gâme.cso"), Path("/out/Prêt")), "disc.zso")
        self.assertEqual(name(Path("/in/" + "x" * 130 + ".iso"), Path("/out/Ready")), "Ready.zso")

    def test_zso_refuses_partial_sectors_chd_multitrack_raw_and_non_iso_images(self):
        valid = iso_image(MIXED)
        raw_sectors = (importer.CD_SYNC + bytes(2340)) * 128  # 2352-byte sectors, 147 * 2048 bytes.
        missing_root = bytearray(valid)
        missing_root[18 * 2048:18 * 2048 + 34] = bytes(34)
        cases = [("Game.iso", valid + b"x", "whole 2048-byte sectors"),
                 ("Game.iso", valid[:-1024], "whole 2048-byte sectors"),
                 ("Game.chd", chd(), "CHD cannot be converted to ZSO.*out of scope"),
                 ("Game.gdi", b"3\n1 0 4 2352 track01.bin 0\n".ljust(2048, b" "), "several tracks"),
                 ("Game.cue", b"FILE track.bin BINARY".ljust(4096, b" "), "several tracks"),
                 ("Game.bin", raw_sectors, "raw CD sectors"),
                 ("Game.iso", random.Random(8).randbytes(40 * 2048), "volume descriptor"),
                 ("Game.iso", valid[:17 * 2048], "too small"),
                 ("Game.iso", bytes(missing_root), "first 64 sectors"),
                 ("Game.cso", cso(self.payloads, ["deflate", "raw", "deflate"]), "too small")]
        for name, data, message in cases:
            with self.subTest(name=name, message=message):
                self.source = self.original / name
                self.source.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    importer.import_image(self.source, self.output, output_format="zso")
                self.assert_not_published()
                self.source.unlink()
        self.write_iso(MIXED)
        with self.assertRaisesRegex(ValueError, "Output format"):
            importer.import_image(self.source, self.output, output_format="chd")
        with self.assertRaisesRegex(ValueError, "--parent is only supported for CHD"):
            importer.import_image(self.source, self.output, output_format="zso", parent=self.source)
        with self.assertRaisesRegex(ValueError, "outside"):
            importer.import_image(self.source, self.original / "copy", output_format="zso")
        self.output.mkdir()
        with self.assertRaisesRegex(ValueError, "already exists"):
            importer.import_image(self.source, self.output, output_format="zso")
        self.assertEqual(list(self.output.iterdir()), [])
        self.output.rmdir()
        # Plain ISO input stays refused by the default expanded mode.
        with self.assertRaisesRegex(ValueError, "Unsupported image"):
            importer.import_image(self.source, self.output)
        self.assert_not_published()

    def test_zso_refuses_more_sectors_than_the_console_admits(self):
        count = importer.LBA_LIMIT + 1
        first = 24 + 4 * (count + 1)
        index = array("I", range(first, first + count + 1))
        if sys.byteorder != "little":
            index.byteswap()
        self.source.write_bytes(struct.pack("<4sIQIBB2s", b"CISO", 24, count * 2048, 2048, 1, 0, b"\0\0")
                                + index.tobytes() + bytes(count))
        with self.assertRaisesRegex(ValueError, "admits at most 719850"):
            importer.import_image(self.source, self.output, output_format="zso")
        self.assert_not_published()

    def test_zso_failed_block_verification_and_source_change_do_not_publish(self):
        self.write_iso()
        encode = importer.encode_lz4_block

        def short_final_literals(block):
            if block != bytes(2048):
                return encode(block)
            # Valid LZ4 output, but its final sequence has 4 literals; the
            # console rejects that, so verification must stop publication.
            return b"\x1f\x00\x01\x00" + b"\xff" * 7 + b"\xef" + b"\x40" + bytes(4)

        with patch.object(importer, "encode_lz4_block", short_final_literals):
            with self.assertRaisesRegex(ValueError, "Block 1: LZ4 block lacks final literals"):
                importer.import_image(self.source, self.output, output_format="zso")
        self.assert_not_published()

        def change_source(line):
            if line.startswith("Verifying"):
                with self.source.open("r+b") as stream:
                    stream.seek(-1, 2)
                    stream.write(b"!")

        with self.assertRaisesRegex(ValueError, "Source changed"):
            importer.import_image(self.source, self.output, output_format="zso", progress=change_source)
        self.assert_not_published()

    def c_reader(self):
        """Build a host program from the repository's own C ZSO reader."""
        compiler = shlex.split(os.environ.get("CC", "")) or [
            shutil.which(name) for name in ("cc", "gcc", "clang") if shutil.which(name)][:1]
        if not compiler:
            self.skipTest("no C compiler; the C reader cross-check needs one")
        view = "kui_zso_view_fn" in (ROOT / "include/kui/zso.h").read_text(encoding="utf-8")
        source = self.root / "zso_host_check.c"
        source.write_text(ZSO_HOST_CHECK_C, encoding="utf-8")
        executable = self.root / "zso_host_check"
        command = ["-std=c11", "-O1", "-Wall", "-Wextra", f"-I{ROOT / 'include'}", "-DKUI_ZSO_CODECS=7",
                   f"-DKUI_TEST_VIEW_API={int(view)}", str(source),
                   *(str(ROOT / "src/core" / name) for name in ("zso.c", "lzo1x.c", "deflate.c")),
                   "-o", str(executable)]
        errors = []
        for flags in (["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"], []):
            built = subprocess.run([*compiler, *flags, *command], capture_output=True, text=True, check=False)
            if built.returncode == 0:
                return executable
            errors.append(built.stderr)
        self.fail("Cannot build the C reader host check:\n" + "\n".join(errors))

    def run_c_reader(self, executable, path):
        decoded = path.with_suffix(".decoded")
        # Sanitizer findings exit 86; reader rejections exit 1.
        result = subprocess.run([str(executable), str(path), str(decoded)], capture_output=True, text=True,
                                check=False, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:exitcode=86",
                                                  "UBSAN_OPTIONS": "exitcode=86"})
        data = decoded.read_bytes() if result.returncode == 0 else None
        if decoded.exists():
            decoded.unlink()
        return result, data

    def test_zso_output_decodes_bit_exactly_through_repository_c_reader(self):
        executable = self.c_reader()
        # Encoded from ISO and CSO (with LZ4 forms of 2044..2047 bytes at the
        # end), and a shift-9 standard ZSO repacked at shift 2.
        expected = iso_image(MIXED * 4) + b"".join(boundary_blocks())
        sectors = [expected[offset:offset + 2048] for offset in range(0, len(expected), 2048)]
        streams = [lz4_run_late(sector) if len(set(sector)) == 1 else "raw" for sector in sectors]
        sources = [("Synthetic Game.iso", expected, "encoded"),
                   ("Synthetic.cso", cso(sectors, ["deflate"] * len(sectors), shift=2), "encoded"),
                   ("Synthetic.zso", cso(sectors, streams, magic=b"ZISO", shift=9), "repacked")]
        for number, (name, source, method) in enumerate(sources):
            with self.subTest(source=name):
                self.source = self.original / name
                self.source.write_bytes(source)
                output = self.root / f"converted-{number}"
                report = importer.import_image(self.source, output, output_format="zso")
                self.assertEqual((report["zso"]["method"], report["zso"]["shift"]), (method, 2))
                result, data = self.run_c_reader(executable, output / report["launch_file"])
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(data, expected)
        # The importer's verifier and the C reader accept and reject the same
        # hand-built containers.
        zero = lz4_run(bytes(2048))
        short = b"\x1f\x00\x01\x00" + b"\xff" * 7 + b"\xef" + b"\x40" + bytes(4)
        # Its last match starts at 2039, after the 2036 limit; five final literals.
        late = b"\x1f\x00\x01\x00" + b"\xff" * 7 + b"\xe9" + b"\x10\x00\x01\x00" + b"\x50" + bytes(5)
        noise = random.Random(9).randbytes(2048)
        cases = [(zso_file([(zero, False), (noise, True)]), True),
                 (zso_file([(zero + bytes(4), False)]), False),
                 (zso_file([(short, False)]), False),
                 (zso_file([(late, False)]), False),
                 (zso_file([(noise + bytes(4), True)]), False),
                 (zso_file([(noise[:2044], True)]), False),
                 (zso_file([(zero, False)], shift=10), False),
                 (zso_file([(zero, False)]) + bytes(4), False)]
        for number, (data, accepted) in enumerate(cases):
            with self.subTest(case=number):
                path = self.root / f"case-{number}.zso"
                path.write_bytes(data)
                result, decoded = self.run_c_reader(executable, path)
                self.assertEqual(result.returncode == 0, accepted, result.stderr)
                if accepted:
                    self.assertEqual(importer.verify_zso(path)["logical_sha256"],
                                     hashlib.sha256(decoded).hexdigest())
                else:
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertRegex(result.stderr, r"^(header/index|block \d+): ")
                    with self.assertRaises(ValueError):
                        importer.verify_zso(path)

    def test_zso_optional_lz4_package_encoder_is_opt_in_and_verified(self):
        self.write_iso()
        with patch.dict(sys.modules, {"lz4": None, "lz4.block": None}):
            with self.assertRaisesRegex(ValueError, "third-party lz4 Python package"):
                importer.import_image(self.source, self.output, output_format="zso", zso_encoder="lz4-hc")
        self.assert_not_published()
        with self.assertRaisesRegex(ValueError, "only to --output-format zso"):
            importer.import_image(self.source, self.output, zso_encoder="lz4-hc")
        library = ctypes.util.find_library("lz4")
        if not library:
            self.skipTest("system liblz4 unavailable to stand in for the lz4 package")
        codec = ctypes.CDLL(library)
        codec.LZ4_compress_HC.argtypes = (ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int)
        codec.LZ4_compress_HC.restype = ctypes.c_int
        calls = []

        def compress(source, mode="default", compression=9, store_size=True):
            calls.append((mode, compression, store_size))
            destination = ctypes.create_string_buffer(4096)
            size = codec.LZ4_compress_HC(bytes(source), destination, len(source), len(destination), compression)
            self.assertGreater(size, 0)
            return destination.raw[:size]

        package, block = types.ModuleType("lz4"), types.ModuleType("lz4.block")
        block.compress, package.block = compress, block
        expected = self.source.read_bytes()
        with patch.dict(sys.modules, {"lz4": package, "lz4.block": block}):
            report = importer.import_image(self.source, self.output, output_format="zso", zso_encoder="lz4-hc")
        # LZ4 HC's default level 9: level 12 was much slower for no real gain.
        self.assertEqual(set(calls), {("high_compression", 9, False)})
        self.assertEqual(report["zso"]["encoder"], "lz4 package, lz4.block high compression level 9")
        blocks = self.assert_zso_matches(report, expected)
        self.assertEqual(report["zso"]["raw_blocks"], sum(raw for raw, _ in blocks))

    def test_cli_zso_success_progress_and_existing_output(self):
        self.write_iso(MIXED)
        command = [sys.executable, str(ROOT / "tools/game_image_import.py"), str(self.source), str(self.output),
                   "--output-format", "zso"]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Converted ISO to ZSO:", result.stdout)
        self.assertIn("Synthetic Game.zso", result.stdout)
        self.assertIn("every block decoded and matched", result.stdout)
        self.assertIn("Converting ISO to ZSO: 29/29 blocks", result.stderr)
        self.assertIn("Verifying ZSO: 29/29 blocks", result.stderr)
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Output already exists", result.stderr)
        data = self.source.read_bytes()
        sectors = [data[offset:offset + 2048] for offset in range(0, len(data), 2048)]
        streams = [lz4_run_late(sector) if len(set(sector)) == 1 else "raw" for sector in sectors]
        self.source = self.original / "Synthetic.zso"
        self.source.write_bytes(cso(sectors, streams, magic=b"ZISO", shift=9))
        command[2:4] = [str(self.source), str(self.root / "repacked")]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Repacked standard ZSO at shift 2:", result.stdout)
        self.assertIn("Repacking ZSO (LZ4) at shift 2: 29/29 blocks", result.stderr)


if __name__ == "__main__":
    unittest.main()
