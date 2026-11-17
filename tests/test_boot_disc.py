#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/boot_disc.py: the copy table matches stage 1's C layout and loader,
MIL-CD scrambling round-trips, the placeholder patch is exact, the builder
refuses inputs and layouts that would verify but not protect or boot, and
(when the pinned mkdcdisc and stage 1 are built) a real CDI is laid out, read
back and loaded by the actual stage-1 C code from its own data track."""
import importlib.util
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("boot_disc", ROOT / "tools/boot_disc.py")
disc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(disc)
MKDCDISC = ROOT / ".deps/mkdcdisc/builddir/mkdcdisc"
STAGE1 = ROOT / "build/stage1/stage1.bin"


def compile_loader(work):
    binary = Path(work) / "boot-stage1"
    subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "include"), str(ROOT / "src/bootstage/stage1_load.c"),
                    str(ROOT / "tests/test_boot_stage1.c"), "-o", str(binary)], check=True)
    return binary


class BootDisc(unittest.TestCase):
    def test_table_layout_and_loader(self):
        """A Python-built table drives the real C loader over a flat track."""
        rng = random.Random(3)
        image = bytes(rng.randrange(256) for _ in range(300_000))
        base, offsets = 11702, (40, 500, 900)
        fads = [base + o + 150 for o in offsets]
        table = disc.build_table(image, fads)
        self.assertEqual(len(table), 1084)
        padded = image + bytes(-len(image) % 2048)
        track = bytearray(1100 * 2048)
        for o in offsets:
            track[o * 2048:o * 2048 + len(padded)] = padded
        with tempfile.TemporaryDirectory(prefix="kui-boot-disc-") as work:
            loader = compile_loader(work)
            paths = {name: Path(work) / name for name in ("table", "image", "track")}
            paths["table"].write_bytes(table)
            paths["image"].write_bytes(image)

            def load(data):
                paths["track"].write_bytes(data)
                return subprocess.run([str(loader), str(paths["table"]), str(paths["image"]),
                                       str(paths["track"]), str(base)], capture_output=True, text=True)
            self.assertEqual(load(bytes(track)).returncode, 0)
            damaged = bytearray(track)
            damaged[(40 + 16 * 3 + 2) * 2048 + 9] ^= 1           # copy 1, chunk 3
            damaged[(500 + 16 * 3 + 7) * 2048 + 9] ^= 1          # copy 2, same chunk
            self.assertEqual(load(bytes(damaged)).returncode, 0)
            damaged[(900 + 16 * 3 + 2) * 2048 + 9] ^= 1          # copy 3 agrees with copy 1's damage
            self.assertEqual(load(bytes(damaged)).returncode, 1)
            bad = bytearray(table)
            bad[40] ^= 1                                          # a copy address; table CRC now wrong
            paths["table"].write_bytes(bytes(bad))
            self.assertEqual(load(bytes(track)).returncode, 1)
        with self.assertRaises(ValueError):
            disc.build_table(image, [])
        with self.assertRaises(ValueError):
            disc.build_table(bytes(256 * 32768 + 1), fads)

    def test_scramble_round_trip(self):
        rng = random.Random(7)
        for size in (1, 31, 32, 33, 2048, 100_000, 2 * 1024 * 1024 + 12345, 5 * 1024 * 1024 + 7):
            data = rng.randbytes(size)
            scrambled = disc.scramble(data)
            self.assertEqual(disc.descramble(scrambled), data)
            if size >= 64:
                self.assertNotEqual(scrambled, data)

    def test_placeholder_patch(self):
        empty = disc.MAGIC + bytes(disc.TABLE_BYTES - 16)
        table = disc.build_table(b"x" * 5000, [11883])
        blob = b"\x11" * 64 + disc.MAGIC + b"\x22" * 40 + empty + b"\x33" * 8
        patched = disc.patch_stage1(blob, table)
        self.assertEqual(len(patched), len(blob))
        self.assertEqual(patched[64 + 16 + 40:64 + 16 + 40 + len(table)], table)
        for bad in (blob + empty, b"\x11" * 66 + empty):
            with self.assertRaises(ValueError):
                disc.patch_stage1(bad, table)

    def test_refuses_unbootable_inputs(self):
        """ELF files, an empty bootstrap and spacers under 1 MiB are refused
        before mkdcdisc runs."""
        with tempfile.TemporaryDirectory(prefix="kui-boot-disc-") as work:
            work = Path(work)
            elf, flat, empty = work / "a.elf", work / "a.bin", work / "empty.bin"
            elf.write_bytes(b"\x7fELF" + bytes(60))
            flat.write_bytes(b"\x00\x09" * 1024)
            empty.write_bytes(b"")

            class Args:
                output, mkdcdisc, badge = str(work / "x.cdi"), str(work / "no-such-tool"), None
                title, author, release, spacer_mib = "T", "A", "20260916", 1
            for stage1, bootstrap, spacer in ((elf, flat, 1), (flat, elf, 1), (flat, empty, 1), (flat, flat, 0)):
                Args.stage1, Args.bootstrap, Args.spacer_mib = str(stage1), str(bootstrap), spacer
                with self.assertRaises(SystemExit):
                    disc.build(Args)

    def test_layout_checks(self):
        """Copies must follow stage 1, lie in order and be a spacer apart."""
        copy_bytes, spacer = 300 * 2048, 1
        step = 300 + 512
        good = {"1ST_READ.BIN": (11729, 2896), "KUIBOOT1.BIN": (11731, 0),
                "KUIBOOT2.BIN": (11731 + step, 0), "KUIBOOT3.BIN": (11731 + 2 * step, 0)}
        disc.check_layout(good, copy_bytes, spacer)
        bad = [dict(good, **{"KUIBOOT1.BIN": (11740, 0)}),                    # a gap after stage 1
               dict(good, **{"KUIBOOT2.BIN": (11731 + 300, 0)}),              # no spacer
               dict(good, **{"KUIBOOT3.BIN": (11731 + step - 1, 0)}),         # out of order
               dict(good, **{"KUIBOOT2.BIN": (11731, 0), "KUIBOOT3.BIN": (11731, 0)})]  # one extent
        for layout in bad:
            with self.assertRaises(SystemExit):
                disc.check_layout(layout, copy_bytes, spacer)
        with self.assertRaises(SystemExit):
            disc.check_layout(good, copy_bytes, 2)

    def test_mkdcdisc_warning_refuses_disc(self):
        """A warning from mkdcdisc (an unmatched sort entry, say) fails the build."""
        with tempfile.TemporaryDirectory(prefix="kui-boot-disc-") as work:
            work = Path(work)
            tool = work / "fake-mkdcdisc"

            class Args:
                badge, title, author, release = None, "T", "A", "20260916"
            for text, refused in (("Wrote x.cdi", False), ("Warning: the sort file gives a weight to /X", True),
                                  ("Error: nothing", True)):
                tool.write_text("#!/bin/sh\necho '%s' >&2\n" % text)
                tool.chmod(0o755)
                call = lambda: disc.mkdcdisc(tool, work / "s.bin", [], work / "sort.txt", work / "x.cdi", Args)
                if refused:
                    with self.assertRaises(SystemExit):
                        call()
                else:
                    self.assertEqual(call(), work / "x.cdi")

    @unittest.skipUnless(MKDCDISC.exists() and STAGE1.exists(), "needs the pinned mkdcdisc and make -f Makefile.dc stage1")
    def test_real_cdi(self):
        """Build a CDI, read the layout back and load from its data track."""
        rng = random.Random(11)
        image = rng.randbytes(200_000)
        with tempfile.TemporaryDirectory(prefix="kui-boot-disc-") as work:
            work = Path(work)
            bootstrap = work / "bootstrap.bin"
            bootstrap.write_bytes(image)

            class Args:
                stage1, output, mkdcdisc = str(STAGE1), str(work / "disc.cdi"), str(MKDCDISC)
                badge = str(ROOT / "resources/branding/boot-disc-badge.mr")
                title, author, release, spacer_mib = "K-UI Bootloader v3", "TacitusVox", "20260916", 1
            Args.bootstrap = str(bootstrap)
            report = disc.build(Args)
            copies = [f - 150 for f in report["copy_fads"]]
            self.assertEqual(len(copies), 3)
            self.assertTrue(all(b - a >= 512 for a, b in zip(copies, copies[1:])))
            self.assertLess(report["stage1_lba"], copies[0])
            self.assertEqual(report["badge"]["bootstrap_offset"], "0x3820")
            cdi = (work / "disc.cdi").read_bytes()
            track = disc.CdiDataTrack(cdi, report["start_lba"])
            flat = b"".join(track.read(lba) for lba in range(report["start_lba"], copies[-1] + -(-len(image) // 2048)))
            loader = compile_loader(work)
            # The disc's own stage 1 carries exactly the table for this layout.
            stage1 = disc.descramble(track.file(*track.files()["1ST_READ.BIN"]))
            table = disc.build_table(image, report["copy_fads"])
            self.assertEqual(stage1.count(table), 1)
            (work / "table").write_bytes(table)
            (work / "track").write_bytes(flat)
            result = subprocess.run([str(loader), str(work / "table"), str(bootstrap), str(work / "track"),
                                     str(report["start_lba"])], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
