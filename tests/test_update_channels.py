#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the real release verifier, ancestry proof and atomic Git feed."""
import contextlib
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from channel_feed import git, remote_ref, update_info
from make_update import make_update
import publish_stable_feed as stable
from runtime_package import envelope
import update_package as kup


class UpdateChannels(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        (self.root / "checkout").mkdir()
        cwd = contextlib.chdir(self.root / "checkout")
        cwd.__enter__()
        self.addCleanup(cwd.__exit__, None, None, None)
        git("init", "-q", "-b", "main")
        git("config", "user.name", "tester")
        git("config", "user.email", "tester@example.invalid")
        git("init", "--bare", "-q", str(self.root / "origin"))
        git("remote", "add", "origin", str(self.root / "origin"))
        self.previous = self.commit("Add the previous GUI")

    def commit(self, subject, filename="gui.c"):
        path = Path(filename)
        path.write_text(subject)
        git("add", filename)
        git("commit", "-qm", subject)
        return git("rev-parse", "HEAD")

    def test_order_uses_git_ancestry_and_package_bound_notes(self):
        git("checkout", "-qb", "unrelated")
        foreign = self.commit("Side branch GUI")
        git("tag", "v90.0.0", foreign)
        git("checkout", "-q", "main")
        git("tag", "v2.0.0", self.previous)
        self.commit("Documentation housekeeping", "docs.md")
        current = self.commit("Support Japanese saves " + "x" * 100)
        fields = update_info(current, "1" * 64, self.previous, ["gui.c"]).splitlines()
        self.assertEqual(fields[:5], ["KUI-UPDATE-INFO-1", current, "1" * 64, "2", self.previous[:12]])
        self.assertNotIn(foreign[:12], fields)
        self.assertNotIn(current[:12], fields)
        self.assertEqual(fields[-2], "1")
        self.assertTrue(fields[-1].startswith("Support Japanese saves"))
        self.assertEqual(len(fields[-1]), 76)
        self.assertLessEqual(len("\n".join(fields)) + 1, 1350)
        self.assertNotIn("Documentation", "\n".join(fields))
        no_proof = update_info(current, "2" * 64, foreign)
        self.assertNotIn(foreign[:12], no_proof)
        self.assertIn("2" * 64, no_proof)

    def release(self, tag="v2.1.0"):
        revision = self.commit("Add release-only downloads")
        git("tag", "-am", "Release software", tag)
        git("push", "-q", "origin", "main", "refs/tags/" + tag)
        stage = self.root / "stage"
        (stage / "apps/games").mkdir(parents=True)
        payload = kup.CAPABILITY + kup.ENGINE + kup.HANDOFF_HEADER + bytes(192)
        (stage / "runtime.kui").write_bytes(envelope(payload, len(payload), revision[:12]))
        # Accepted game loaders retain their genuine component IDs.
        (stage / "apps/games/retail-boot.kui").write_bytes(envelope(b"loader00", 8, self.previous[:12]))
        (stage / "redump.db").write_bytes(b"catalogue")
        directory = self.root / "assets"
        directory.mkdir()
        pkg = directory / "kui-2.1.0-test-update.kup"
        make_update(stage, pkg, release=True, channel="stable")
        archive = directory / "kui-2.1.0-test-release.zip"
        with zipfile.ZipFile(archive, "w") as z:
            for f in stage.rglob("*"):
                if f.is_file():
                    z.write(f, "KUI/" + f.relative_to(stage).as_posix())
            z.write(stage / "runtime.kui", "KUI/boot.kui")
            z.writestr("build.json", json.dumps(dict(kind="release", commit=revision, release=dict(version=tag[1:]))))
        sums = directory / "SHA256SUMS.txt"
        sums.write_text("".join(hashlib.sha256(f.read_bytes()).hexdigest() + "  " + f.name + "\n"
                                for f in (pkg, archive)))
        assets = [dict(name=f.name, size=f.stat().st_size, id=i + 1) for i, f in enumerate((pkg, archive, sums))]
        return dict(id=1, tag_name=tag, draft=False, prerelease=False, published_at="today",
                    body="- Verified actions and saves", assets=assets), revision, directory

    def test_full_stable_publication_is_atomic_repeatable_and_bound(self):
        rel, rev, files = self.release()
        def download(tag, assets, out):
            for a in assets:
                shutil.copyfile(files / a["name"], out / a["name"])
        with patch.object(stable, "download_assets", side_effect=download), patch.object(stable, "latest", return_value=rel):
            self.assertTrue(stable.publish(rel))
            head = remote_ref(stable.CHANNEL)
            body = git("show", head + ":release.txt").splitlines()
            self.assertEqual(body[:2], ["KUI-RELEASE-1", rev])
            self.assertEqual(body[4:], [rel["tag_name"], rel["assets"][0]["name"]])
            info = git("show", head + ":update-info.txt")
            self.assertIn(rev + "\n" + body[3] + "\n", info)
            self.assertIn(self.previous[:12], info)
            self.assertIn("Verified actions and saves", info)
            self.assertFalse(stable.publish(rel))
            self.assertEqual(remote_ref(stable.CHANNEL), head)
            self.assertFalse(stable.publish(rel, expected_tag="v2.0.0"))
            broken = files / rel["assets"][0]["name"]
            data = bytearray(broken.read_bytes())
            data[-1] ^= 0xff
            broken.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "checksum"):
                stable.publish(rel)
            self.assertEqual(remote_ref(stable.CHANNEL), head)
            old = dict(rel, tag_name="v2.0.0")
            git("tag", old["tag_name"], self.previous)
            git("push", "-q", "origin", "refs/tags/v2.0.0")
            with self.assertRaisesRegex(ValueError, "roll"):
                stable.publish(old)

    def test_legacy_zip_has_no_feed_and_prereleases_are_never_promoted(self):
        common = dict(id=1, tag_name="v2.0.0", draft=False, prerelease=False, published_at="today", assets=[])
        self.assertFalse(stable.publish(common))
        self.assertFalse(remote_ref(stable.CHANNEL))
        for extra in (dict(draft=True), dict(prerelease=True)):
            with self.assertRaisesRegex(ValueError, "non-prerelease"):
                stable.assets_for(dict(common, **extra))
        for tag in ("v2.01.0", "v2.1.0-rc1", "nightly-abcd", "v4294967296.1.0"):
            with self.assertRaises(ValueError):
                stable.version(tag)
        for name in ("../K-UI.kup", "K-UI??.kup", "a" * 60 + ".kup"):
            assets = [dict(name=name, size=4096)]
            with self.assertRaisesRegex(ValueError, "name"):
                stable.assets_for(dict(common, assets=assets))

    def test_release_review_rejects_a_gui_only_or_mismatched_install(self):
        rel, rev, files = self.release()
        pkg, archive, sums = [files / a["name"] for a in rel["assets"]]
        def rehash():
            sums.write_text("".join(hashlib.sha256(f.read_bytes()).hexdigest() + "  " + f.name + "\n"
                                    for f in (pkg, archive)))
            for item in rel["assets"]:
                item["size"] = (files / item["name"]).stat().st_size
        with zipfile.ZipFile(archive, "a") as z:
            z.writestr("KUI/apps/games/extra-helper.bin", b"missing from KUP")
        rehash()
        with self.assertRaisesRegex(SystemExit, "inventory"):
            stable.verify_files(files, rel["assets"], rev, "v2.1.0")
        partial = self.root / "partial"
        partial.mkdir()
        shutil.copyfile(self.root / "stage/runtime.kui", partial / "runtime.kui")
        make_update(partial, pkg, channel="testing")
        rehash()
        with self.assertRaisesRegex(SystemExit, "exact replacements"):
            stable.verify_files(files, rel["assets"], rev, "v2.1.0")


if __name__ == "__main__":
    unittest.main()
