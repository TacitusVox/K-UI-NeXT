#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Optional public-origin test of the native cover resolver and image decoder."""
import argparse
import ctypes
from hashlib import sha1
from pathlib import Path
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MiB = 1024 * 1024


class DiscIdentity(ctypes.Structure):
    _fields_ = [
        ("title", ctypes.c_char * 129), ("product", ctypes.c_char * 11),
        ("version", ctypes.c_char * 7), ("region", ctypes.c_char * 9),
        ("number", ctypes.c_uint), ("total", ctypes.c_uint),
        ("format", ctypes.c_uint), ("media", ctypes.c_uint),
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--card", action="store_true",
                        help="Also verify installed thumbnails on disposable FAT32 and exFAT cards")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        library = Path(temp, "cover.so")
        subprocess.run(["cc", "-O2", "-shared", "-fPIC", "-Iinclude", "-I.deps/fatfs/source",
                        "src/apps/internet_cover.c", "src/apps/cover_image.c", "-o", library],
                       cwd=ROOT, check=True)
        api = ctypes.CDLL(str(library))
        api.kui_internet_cover_url.argtypes = [ctypes.POINTER(DiscIdentity), ctypes.c_char_p,
                                               ctypes.c_size_t]
        api.kui_internet_cover_url.restype = ctypes.c_bool
        api.kui_internet_cover_small_url.argtypes = api.kui_internet_cover_url.argtypes
        api.kui_internet_cover_small_url.restype = ctypes.c_bool
        uint_ptr = ctypes.POINTER(ctypes.c_uint)
        api.kui_cover_image_decode.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p),
            uint_ptr, uint_ptr, uint_ptr,
        ]
        api.kui_cover_image_free.argtypes = [ctypes.c_void_p]
        for release, product, git_blob, dimensions, small_hash in (
            ("crazy", b"MK-51035", "fc773be5f8da1762eb27a4895f960fd4af0d0f1e", (512, 510),
             "e0c6644bb19f079c017549c7dc2b55190a4f2766"),
            ("skies", b"MK-51052", "a18707ccea2e340bf037f8423d68649efd6bd8fa", (512, 506),
             "3166c55e32d69f4a9e9000212c44456cbe6ebc9a"),
        ):
            url = ctypes.create_string_buffer(768)
            id_ = DiscIdentity(product=product, region=b"U")
            assert api.kui_internet_cover_url(ctypes.byref(id_), url, len(url))
            with urllib.request.urlopen(url.value.decode("ascii"), timeout=30) as response:
                data = response.read(3 * MiB + 1)
            assert len(data) <= 3 * MiB
            assert sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest() == git_blob
            pixels = ctypes.c_void_p()
            w, h, channels = ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
            assert api.kui_cover_image_decode(data, len(data), ctypes.byref(pixels),
                    ctypes.byref(w), ctypes.byref(h), ctypes.byref(channels)) == 0
            try:
                assert (w.value, h.value) == dimensions
                print(f"{product.decode()}: {len(data)} bytes, {w.value} x {h.value}, "
                      f"{channels.value} channels")
            finally:
                api.kui_cover_image_free(pixels)
            small = ctypes.create_string_buffer(768)
            assert api.kui_internet_cover_small_url(ctypes.byref(id_), small, len(small))
            with urllib.request.urlopen(small.value.decode("ascii"), timeout=30) as response:
                reduced = response.read(256 * 1024 + 1)
            assert reduced.startswith(b"\xff\xd8") and reduced.endswith(b"\xff\xd9")
            assert sha1(reduced).hexdigest() == small_hash
            assert len(reduced) < 0.10 * len(data), (product, len(reduced), len(data))
            print(f"{product.decode()}: smaller website cover is {len(reduced)} bytes")
            if args.card:
                src = Path(temp, "origin.png")
                src.write_bytes(data)
                command = [sys.executable, ROOT / "tests/test_bba_media_integration.py", "--public-cover", src,
                           "--release", release]
                # First exercise a missing archived JPEG, then a real successful
                # primary transfer, including both on-card display paths.
                subprocess.run(command, cwd=ROOT, check=True)
                jpeg = Path(temp, "regional.jpg")
                jpeg.write_bytes(reduced)
                subprocess.run([*command, "--public-small", jpeg], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
