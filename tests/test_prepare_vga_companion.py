#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the PC companion tool with generated GDIs. No retail files or patcher needed."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

from games_fixture import make_fixture

TOOL = Path(__file__).resolve().parents[1] / "tools" / "prepare_vga_companion.py"


def snapshot(root):
    return {p.relative_to(root): hashlib.sha256(p.read_bytes()).digest() if p.is_file() else None
            for p in root.rglob("*")}


def fixture(directory, name, member=b"1/2", region=b"U", product=b"T-TEST", revision=b"V1.000",
            date=b"20001231", flags=b"0000000", nul=False):
    make_fixture(directory, gdi_name=name)
    track = directory / "track03.bin"
    data = bytearray(track.read_bytes())
    header = bytearray(data[16:272])
    pad = b"\0" if nul else b" "
    header[43:46] = member
    header[48:56] = region.ljust(8, pad)
    header[56:63] = flags
    header[64:74] = product.ljust(10, pad)
    header[74:80] = revision.ljust(6, pad)
    header[80:88] = date
    header[96:112] = b"1ST_READ.BIN".ljust(16, pad)
    data[16:272] = header
    track.write_bytes(data)
    return directory / name, bytes(header)


def command(*args, ok=True):
    result = subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, text=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def dcp(path, header):
    with ZipFile(path, "w") as archive:
        archive.writestr("Bootsector/IP.BIN", header.ljust(32768, b"\0"))


class CompanionTool(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.games = self.root / "Games"
        self.original, self.ip = fixture(self.games / "Retail", "my release.gdi")
        self.patches = self.root / "patches"
        self.patches.mkdir()

    def test_prepared_raw_cooked_and_no_clobber(self):
        prepared, _ = fixture(self.root / "publisher", "published.gdi", region=b"JUE", flags=b"0799810", nul=True)
        track = prepared.parent / "track03.bin"
        raw = bytearray(track.read_bytes())
        raw[21 * 2352 + 16] ^= 0x5a
        cooked = b"".join(raw[i + 16:i + 2064] for i in range(0, len(raw), 2352))
        track.write_bytes(cooked)
        # The source's on-disk case can differ from its descriptor spelling.
        track.rename(prepared.parent / "TRACK03.BIN")
        prepared.write_bytes(prepared.read_bytes().replace(b"3 45000 4 2352", b"3 45000 4 2048").replace(b"\n", b"\r"))
        before, input_before = snapshot(self.games), snapshot(prepared.parent)
        command(self.original, "--prepared", prepared)
        dest = self.original.parent / ".kui-vga" / self.original.name
        self.assertEqual((dest / "track03.bin").read_bytes(), cooked)
        self.assertIn(b"3 45000 4 2048", (dest / "disc.gdi").read_bytes())
        self.assertNotIn(b"\r", (dest / "disc.gdi").read_bytes())
        self.assertEqual(snapshot(prepared.parent), input_before)
        self.assertTrue(all(snapshot(self.games)[key] == value for key, value in before.items()))
        after = snapshot(self.games)
        self.assertIn("already exists", command(self.original, "--prepared", prepared, ok=False))
        self.assertEqual(snapshot(self.games), after)

    def test_wrong_disc_identity_and_malformed_data_are_never_installed(self):
        def reject(path, *args):
            before = snapshot(self.games)
            output = command(path, *args, ok=False)
            self.assertNotIn("Cannot find", output)
            self.assertEqual(snapshot(self.games), before)

        for key, value in (("member", b"2/2"), ("member", b"3/2"), ("product", b"T-OTHER"), ("product", b"TTEST"),
                           ("revision", b"V1.001"), ("date", b"20010101"), ("date", b"BAD-DATE"),
                           ("flags", b"0000001"), ("flags", b"xxx0000"), ("region", b"JE")):
            with self.subTest(key=key, value=value):
                directory = self.root / ("bad-" + key + "-" + str(value).replace("/", "-"))
                other, ip = fixture(directory, "disc.gdi", **{key: value})
                patch = self.patches / "foreign.dcp"
                dcp(patch, ip)
                reject(self.original, "--dcp", patch, "--patcher", "/unused")
                reject(self.original, "--prepared", other)
        other, _ = fixture(self.root / "damaged", "disc.gdi")
        for source in (other, self.original):
            with self.subTest(malformed=source):
                track = source.parent / "track03.bin"
                prior = track.read_bytes()
                damaged = bytearray(prior)
                damaged[16 * 2352 + 17] = 0
                track.write_bytes(damaged)
                reject(self.original, "--prepared", other)
                track.write_bytes(prior)
        (other.parent / "track02.raw").unlink()
        reject(self.original, "--prepared", other)
        reject(self.original, "--prepared", self.original)

    def test_rejects_unexpected_patcher_output(self):
        patch_file = self.patches / "publisher.dcp"
        dcp(patch_file, self.ip)
        wrong_dir = self.root / "rendered"
        wrong, _ = fixture(wrong_dir, "disc.gdi", member=b"2/2")
        sys.path.insert(0, str(TOOL.parent))
        self.addCleanup(sys.path.pop, 0)
        from prepare_vga_companion import install

        def render(args, check):
            self.assertTrue(check)
            destination = Path(args[args.index("--output") + 1]).parent
            shutil.copytree(wrong_dir, destination, dirs_exist_ok=True)

        before = snapshot(self.games)
        with patch("prepare_vga_companion.subprocess.run", side_effect=render):
            with self.assertRaisesRegex(ValueError, "Patched output is for a different release or disc"):
                install(self.original, None, patch_file, sys.executable)
            self.assertEqual(snapshot(self.games), before)
            wrong.write_text("malformed output")
            with self.assertRaisesRegex(ValueError, "Invalid GDI track count"):
                install(self.original, None, patch_file, sys.executable)
            self.assertEqual(snapshot(self.games), before)

    def test_dry_run_matches_correct_member_and_skips_hidden_sources(self):
        second, second_ip = fixture(self.games / "Disc Two", "disc.gdi", member=b"2/2")
        _, padded = fixture(self.root / "published", "disc.gdi", region=b"JUE", nul=True, flags=b"0799810")
        dcp(self.patches / "disc-one.dcp", padded)
        dcp(self.patches / "disc-two.dcp", second_ip)
        # Valid hidden trees and capture descriptors are private to Games; never batch-select them.
        private = self.original.parent / ".kui-vga" / "other.gdi"
        private.mkdir(parents=True)
        shutil.copytree(self.original.parent, private, dirs_exist_ok=True, ignore=shutil.ignore_patterns(".kui-vga"))
        shutil.copyfile(self.original, self.original.parent / ".capture.gdi")
        before = snapshot(self.games)
        out = command(self.games, "--patch-dir", self.patches, "--dry-run")
        self.assertIn(f"{self.original} <- {self.patches / 'disc-one.dcp'}", out)
        self.assertIn(f"{second} <- {self.patches / 'disc-two.dcp'}", out)
        self.assertIn("Matched 2 original GDI(s)", out)
        self.assertNotIn(".kui-vga", out)
        self.assertNotIn(".capture", out)
        self.assertEqual(snapshot(self.games), before)
        dcp(self.patches / "ambiguous.dcp", padded)
        self.assertIn("multiple DCPs", command(self.games, "--patch-dir", self.patches, "--dry-run", ok=False))
        self.assertEqual(snapshot(self.games), before)


if __name__ == "__main__":
    unittest.main()
