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
        uint_ptr = ctypes.POINTER(ctypes.c_uint)
        api.kui_cover_image_decode.argtypes = [
            ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p),
            uint_ptr, uint_ptr, uint_ptr,
        ]
        api.kui_cover_image_free.argtypes = [ctypes.c_void_p]
        for product, git_blob, dimensions in (
            (b"MK-51035", "fc773be5f8da1762eb27a4895f960fd4af0d0f1e", (512, 510)),
            (b"MK-51052", "a18707ccea2e340bf037f8423d68649efd6bd8fa", (512, 506)),
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
            if args.card and product == b"MK-51035":
                src = Path(temp, "origin.png")
                src.write_bytes(data)
                subprocess.run([sys.executable, ROOT / "tests/test_bba_media_integration.py",
                                "--public-cover", src], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
