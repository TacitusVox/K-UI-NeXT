#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the full distribution's input lock, exact inventory and runtime contracts."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import stage_distribution as dist


class Distribution(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        self.write("Makefile.worker", b"worker: src/core/common.c\n")
        self.write("src/core/common.c", b'#include "kui/layout.h"\n')
        self.write("include/kui/layout.h", b"#define SLOTS 128\n")
        self.write("src/loader/reader.S", b".text\n")
        self.write("src/dreamcast/main.c", b"/* GUI */\n")
        self.write("tests/test_reader.c", b"/* host test */\n")
        self.write("docs/guide.md", b"Use the release ZIP.")
        subprocess.run(["git", "add", "."], cwd=self.root, check=True)
        self.member = "apps/games/test-helper.bin"
        self.write(dist.SOFTWARE + "/" + self.member, b"test helper")
        self.config = {"schema": 1, "reader_inputs_sha256": dist.reader_digest(self.root),
                       "files": {self.member: hashlib.sha256(b"test helper").hexdigest()}}
        self.write(dist.CONFIG, json.dumps(self.config).encode())

    def write(self, path, data):
        dest = self.root / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    def test_source_coverage_and_unrelated_changes(self):
        baseline = dist.reader_digest(self.root)
        for path in ("src/dreamcast/main.c", "docs/guide.md", "tests/test_reader.c"):
            self.write(path, b"new")
            self.assertEqual(baseline, dist.reader_digest(self.root))
        for path in ("src/loader/reader.S", "Makefile.worker", "include/kui/layout.h", "src/core/common.c"):
            with self.subTest(path=path):
                original = (self.root / path).read_bytes()
                self.write(path, original + b"\n")
                with self.assertRaisesRegex(ValueError, "Reader or build inputs changed"):
                    dist.checked_inventory(self.root)
                self.write(path, original)

    def test_only_qualified_bytes(self):
        self.assertEqual(dist.checked_inventory(self.root), {self.member: b"test helper"})
        self.write(dist.SOFTWARE + "/" + self.member, b"damaged")
        with self.assertRaisesRegex(ValueError, "checksum differs"):
            dist.checked_inventory(self.root)
        (self.root / dist.SOFTWARE / self.member).unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            dist.checked_inventory(self.root)
        self.write(dist.SOFTWARE + "/" + self.member, b"test helper")
        self.write(dist.SOFTWARE + "/apps/disc/unplanned-helper.bin", b"unexpected")
        with self.assertRaisesRegex(ValueError, "unexpected"):
            dist.checked_inventory(self.root)

    def test_user_data_cannot_be_staged(self):
        self.config["files"] = {"apps/games/vmu-session.bin": "0" * 64}
        self.write(dist.CONFIG, json.dumps(self.config).encode())
        with self.assertRaisesRegex(ValueError, "Invalid distribution software"):
            dist.checked_inventory(self.root)

    def test_kernel_modules_need_every_matching_record(self):
        modules = {}
        for family, date, profile, pool in dist.GROUPS:
            record = json.loads((ROOT / f"docs/evidence/wince-cdda-{family}-kernels-{date}.json").read_text())
            for key, kernel in record["kernels"].items():
                if key == record["reference"]:
                    continue
                sha = kernel["kernel_sha256"]
                modules["apps/games/ce-cdda-" + sha[:12] + ".kui"] = bytes(64+0x300) + struct.pack(
                    "<8s6I32s", b"KUICDK01", 1, 64, profile, kernel["kernel_bytes"], pool, 0,
                    bytes.fromhex(sha))
        self.assertEqual(18, len(modules))
        dist.verify_kernel_routes(modules)
        removed, wrong = list(modules)[:2]
        old = modules.pop(removed)
        with self.assertRaisesRegex(ValueError, removed):
            dist.verify_kernel_routes(modules)
        modules[removed] = modules[wrong]
        with self.assertRaisesRegex(ValueError, removed):
            dist.verify_kernel_routes(modules)
        modules[removed] = old
        dist.verify_kernel_routes(modules)

    def test_personalized_armada_adapter_is_excluded(self):
        with self.assertRaisesRegex(ValueError, "Personalized Armada"):
            dist.verify_routes({dist.ARMADA_PRIVATE: b"personalized"}, self.root)

    def test_historical_assets_are_read_from_the_requested_commit(self):
        # Use a real prepared theme pack and the project's fixed G3 fixture.
        import shutil
        themes = self.root / "resources/themes"
        shutil.copytree(ROOT / "resources/themes/chrome-muramasa", themes / "chrome-muramasa")
        self.write("data/known-dumps/redump.db", b"old catalog")
        self.write("data/known-dumps/tosec.db", b"another catalog")
        self.write("resources/music/manifest.json", b'{"tracks":[]}')
        subprocess.run(["git", "add", "."], cwd=self.root, check=True)
        subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                        "commit", "-qm", "fixture"], cwd=self.root, check=True)
        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=self.root, text=True).strip()
        self.write("data/known-dumps/redump.db", b"new uncommitted catalog")
        hashes = dist.derived_hashes(revision, self.root)
        self.assertEqual(hashes["redump.db"], hashlib.sha256(b"old catalog").hexdigest())
        self.assertEqual(hashes["assets/themes/index.txt"], hashlib.sha256(b"chrome-muramasa\n").hexdigest())
        self.assertEqual(hashes["apps/games/probe.dat"], dist.PROBE_SHA256)
        self.assertNotIn("assets/themes/chrome-muramasa/source.json", hashes)

    def test_empty_output_cannot_overwrite_an_install(self):
        with self.assertRaisesRegex(ValueError, "empty"):
            dist.empty_output(self.root)


if __name__ == "__main__":
    unittest.main()
