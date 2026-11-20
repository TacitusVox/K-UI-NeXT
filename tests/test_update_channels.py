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

    def test_full_nightly_uses_exact_qualified_release_and_installation(self):
        import nightly
        import nightly_recipe
        import os
        import struct
        import zlib
        tag = "v2.1.0"
        plan = Path(nightly.RECIPE)
        plan.parent.mkdir()
        plan.write_text(json.dumps({"full_release": tag}))
        git("add", str(plan))
        release, rev, assets = self.release(tag)
        def download(selected, entries, target):
            self.assertEqual(selected, tag)
            for f in entries:
                shutil.copyfile(assets / f["name"], target / f["name"])
        with patch.dict(os.environ, GITHUB_SHA=rev), \
                patch.object(nightly_recipe, "published_release", return_value=release), \
                patch.object(stable, "download_assets", side_effect=download), \
                patch.object(nightly, "build_gui", side_effect=AssertionError("unexpected rebuild")):
            full = self.root / "nightly.kup"
            nightly.package(full)
            result = kup.verify(full)
            approved = kup.verify(assets / release["assets"][0]["name"])
            self.assertTrue(result["full"])
            self.assertEqual(result["channel"], "testing")
            self.assertEqual(result["members"], approved["members"])
            self.assertEqual(nightly.manifest(full, rev).splitlines()[1], rev)
            # The consumer accepts other serialized orders with the same
            # managed paths and bytes. Exercise a real reordered KUP.
            rows = result["members"]
            data = full.read_bytes()
            header = bytearray(data[:kup.HEADER_BYTES])
            offset = kup.HEADER_BYTES + len(rows) * kup.V2_ENTRY_BYTES
            row_payloads = []
            for index, row in enumerate(rows):
                pos = kup.HEADER_BYTES + index * kup.V2_ENTRY_BYTES
                row_payloads.append((data[pos:pos + kup.V2_ENTRY_BYTES], data[offset:offset + row["bytes"]]))
                offset += row["bytes"]
            row_payloads.reverse()
            manifest = b"".join(record for record, _ in row_payloads)
            header[32:44] = hashlib.sha256(kup.V2_MAGIC + header[44:46] + manifest).hexdigest()[:12].encode()
            header[64:96] = hashlib.sha256(manifest).digest()
            struct.pack_into("<I", header, 124, zlib.crc32(header[:124]))
            reordered = self.root / "reordered.kup"
            reordered.write_bytes(header + manifest + b"".join(payload for _, payload in row_payloads))
            self.assertEqual(nightly.manifest(reordered, rev).splitlines()[1], rev)
            # A well-formed but incomplete FULL must also fail at promotion.
            stage = self.root / "incomplete"
            stage.mkdir()
            with zipfile.ZipFile(assets / release["assets"][1]["name"]) as original:
                (stage / "runtime.kui").write_bytes(original.read("KUI/runtime.kui"))
            incomplete = self.root / "incomplete.kup"
            make_update(stage, incomplete, channel="testing", full=True)
            with self.assertRaisesRegex(ValueError, "differs from the qualified"):
                nightly.manifest(incomplete, rev)
            # Rebuilding rechecks the published ZIP and its external digest.
            with (assets / release["assets"][1]["name"]).open("ab") as out:
                out.write(b"damaged")
            with self.assertRaisesRegex(ValueError, "length"):
                nightly.package(self.root / "corrupt.kup")

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

    def release(self, tag="v2.1.0", revision=None, published="2026-11-20T12:00:00Z"):
        revision = revision or self.commit("Add release-only downloads "+tag)
        git("tag", "-am", "Release software", tag)
        git("push", "-q", "origin", "main", "refs/tags/" + tag)
        stage = self.root / ("stage"+tag)
        (stage / "apps/games").mkdir(parents=True)
        payload = kup.CAPABILITY + kup.ENGINE + kup.HANDOFF_HEADER + bytes(192)
        (stage / "runtime.kui").write_bytes(envelope(payload, len(payload), revision[:12]))
        # Accepted game loaders retain their genuine component IDs.
        (stage / "apps/games/retail-boot.kui").write_bytes(envelope(b"loader00", 8, self.previous[:12]))
        (stage / "redump.db").write_bytes(b"catalogue")
        directory = self.root / ("assets"+tag)
        directory.mkdir()
        pkg = directory / (tag+"-test-update.kup")
        make_update(stage, pkg, release=True, channel="stable")
        archive = directory / (tag+"-test-release.zip")
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
        return dict(id=int(tag.split(".")[-1])+100, tag_name=tag, draft=False, prerelease=False, published_at=published,
                    body="- Verified actions and saves", assets=assets), revision, directory

    def test_full_stable_publication_is_atomic_repeatable_and_bound(self):
        rel, rev, files = self.release()
        def download(tag, assets, out):
            for a in assets:
                shutil.copyfile(files / a["name"], out / a["name"])
        with patch.object(stable, "download_assets", side_effect=download), patch.object(stable, "latest", return_value=rel), \
                patch.object(stable, "stable_releases", return_value=[rel]) as listing:
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
            listing.return_value = [old, rel]
            with self.assertRaisesRegex(ValueError, "roll"):
                stable.publish(old)

    def test_stable_picker_keeps_distinct_tags_notes_and_exact_selection(self):
        one, rev, first = self.release("v2.1.1")
        two, same, second = self.release("v2.1.2", revision=rev)
        one["body"], two["body"] = "- First note", "- Second note"
        legacy = dict(one, id=4, tag_name="v2.0.0", assets=[])
        rejected = dict(one, id=5, tag_name="v2.2.0", prerelease=True)
        directories = {one["tag_name"]: first, two["tag_name"]: second}
        def download(tag, assets, out):
            for a in assets:
                shutil.copyfile(directories[tag] / a["name"], out / a["name"])
        with patch.object(stable, "download_assets", side_effect=download), patch.object(stable, "latest", return_value=two), \
                patch.object(stable, "stable_releases", return_value=[one, legacy, rejected, two]):
            self.assertTrue(stable.publish(two, expected_tag=one["tag_name"]))
            head = remote_ref(stable.CHANNEL)
            rows = git("show", head+":releases.txt").splitlines()
            self.assertEqual(rows[:2], ["KUI-RELEASES-1", "2"])
            self.assertEqual([line.split()[4] for line in rows[2:]], ["v2.1.2", "v2.1.1"])
            for row, release in zip(rows[2:], (two, one)):
                fields = row.split()
                self.assertEqual(fields[0], rev)
                self.assertEqual(fields[6:], ["full", "3", "0", "0", "7"])
                self.assertEqual(fields[5], release["assets"][0]["name"])
                archived = head+":history/"+release["tag_name"]+"/"
                self.assertIn(release["body"][2:], git("show", archived+"update-info.txt").splitlines())
                self.assertIn(fields[2], git("show", archived+"release.txt").splitlines())
            self.assertFalse(stable.publish(two))
            self.assertEqual(remote_ref(stable.CHANNEL), head)

    def test_last_ten_stables_and_separately_pinned_latest(self):
        datasets = [self.release(f"v2.5.{i}", published=f"2026-11-{i+1:02d}T00:00:01Z") for i in range(12)]
        stable_records = [record for record, _, _ in datasets]
        by_tag = {record["tag_name"]: directory for record, _, directory in datasets}
        pinned = stable_records[0]
        def download(tag, assets, out):
            for asset in assets:
                shutil.copyfile(by_tag[tag] / asset["name"], out / asset["name"])
        with patch.object(stable, "download_assets", side_effect=download), \
                patch.object(stable, "latest", return_value=pinned), \
                patch.object(stable, "stable_releases", return_value=stable_records):
            self.assertTrue(stable.publish(pinned))
        head = remote_ref(stable.CHANNEL)
        rows = git("show", head+":releases.txt").splitlines()
        self.assertEqual(rows[:2], ["KUI-RELEASES-1", "10"])
        self.assertEqual([r.split()[4] for r in rows[2:]], [r["tag_name"] for r in stable_records[:1:-1]])
        self.assertEqual(git("show", head+":release.txt").splitlines()[4], pinned["tag_name"])

    def test_payload_scope_counts_declared_actions_and_components(self):
        from channel_feed import payload_scope, checked_scope
        report = dict(full=False,members=[dict(path="runtime.kui", policy=kup.INHERIT_ROOT),
            dict(path="apps/disc/read.kui", policy=kup.SELF_KUI), dict(path="redump.db", policy=kup.REMOVE)])
        self.assertEqual(payload_scope(report),"partial 1 1 1 6")
        for bad in ("full 257 0 0 3", "partial 1 0 1 8", "partial 0 0 0 0", "partial 01 0 0 1",
                    "full 0 0 2 1", "full 3 0 0 2", "full 1 0 1 1"):
            with self.assertRaises(ValueError):
                checked_scope(bad)

    def test_legacy_zip_has_no_feed_and_prereleases_are_never_promoted(self):
        common = dict(id=1, tag_name="v2.0.0", draft=False, prerelease=False, published_at="today", assets=[])
        with patch.object(stable, "stable_releases", return_value=[common]):
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
        shutil.copyfile(self.root / "stagev2.1.0/runtime.kui", partial / "runtime.kui")
        make_update(partial, pkg, channel="testing")
        rehash()
        with self.assertRaisesRegex(SystemExit, "exact replacements"):
            stable.verify_files(files, rel["assets"], rev, "v2.1.0")


if __name__ == "__main__":
    unittest.main()
