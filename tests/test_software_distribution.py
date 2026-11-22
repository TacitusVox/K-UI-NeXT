#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import gzip
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from runtime_package import envelope
import software_distribution as distribution
from update_package import CAPABILITY, ENGINE, HANDOFF_HEADER


class SoftwareDistributionTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        work = Path(temp.name)
        self.stage = work / "KUI"
        readers = self.stage / "apps/games"
        readers.mkdir(parents=True)
        self.revision = "0123456789abcdef0123456789abcdef01234567"
        gui = CAPABILITY + ENGINE + HANDOFF_HEADER + bytes(180)
        (self.stage / "runtime.kui").write_bytes(envelope(gui, len(gui), self.revision[:12]))
        (readers / "reader.kui").write_bytes(envelope(bytes(16), 16, "222222222222"))
        boot = work / "disc.cdi.gz"
        boot.write_bytes(gzip.compress(b"boot disc"))
        self.addCleanup(patch.stopall)
        patch.multiple(distribution, BOOT_FILE=boot, BOOT_BYTES=9,
                       BOOT_SHA=hashlib.sha256(b"boot disc").hexdigest()).start()
        self.package, self.sd = work / "full.kup", work / "sd.zip"

    def produce(self):
        return distribution.write_distribution(self.stage, self.package, self.sd,
                                               revision=self.revision, label="RC fixture")

    def test_pair_is_reproducible_and_preserves_independent_reader_builds(self):
        report = self.produce()
        self.assertTrue(report["full"])
        self.assertEqual(report["consumer"]["engine_minimum"], 3)
        self.assertEqual([r["build"] for r in report["members"]], [self.revision[:12], "222222222222"])
        with zipfile.ZipFile(self.sd) as z:
            self.assertEqual(z.read("KUI/boot.kui"), z.read("KUI/runtime.kui"))
        first = distribution.digest(self.sd)
        self.produce()
        self.assertEqual(distribution.digest(self.sd), first)

    def test_alias_omission_cannot_pass_even_with_valid_kup(self):
        self.produce()
        changed = self.sd.with_name("missing-alias.zip")
        with zipfile.ZipFile(self.sd) as src, zipfile.ZipFile(changed, "w") as dst:
            for entry in src.infolist():
                if entry.filename != "KUI/boot.kui":
                    dst.writestr(entry, src.read(entry))
        with self.assertRaisesRegex(ValueError, "inventory"):
            distribution.verify_distribution(self.package, changed, revision=self.revision)

    def test_supplying_a_different_cdi_is_detected_before_outputs(self):
        wrong = self.sd.with_name("other.cdi")
        wrong.write_bytes(b"bad bytes")
        with self.assertRaisesRegex(ValueError, "ZIP checksum"):
            distribution.write_distribution(self.stage, self.package, self.sd,
                                            revision=self.revision, label="RC fixture", boot_cdi=wrong)
        self.assertFalse(self.sd.exists())
        self.assertFalse(self.package.exists())


if __name__ == "__main__":
    unittest.main()
