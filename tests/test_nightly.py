#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise nightly selection/promotion using a temporary git remote and real KUPs."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import nightly
import update_package


class NightlyTests(unittest.TestCase):
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)
        self.repo = self.root / "source"
        self.repo.mkdir()
        cwd = contextlib.chdir(self.repo)
        cwd.__enter__()
        self.addCleanup(cwd.__exit__, None, None, None)
        self.git = nightly.git
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("init", "-q", "--bare", str(self.root / "origin"))
        self.git("remote", "add", "origin", str(self.root / "origin"))
        env = mock.patch.dict(os.environ, GITHUB_OUTPUT=str(self.root / "outputs"))
        env.start()
        self.addCleanup(env.stop)
        self.commit("src/apps/menu.c", "one")

    def commit(self, name, content):
        path = Path(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        self.git("add", "--", name)
        self.git("commit", "-qm", "change " + name)
        self.git("push", "-q", "origin", "main")
        self.revision = os.environ["GITHUB_SHA"] = self.git("rev-parse", "HEAD")

    def selection(self):
        path = Path(os.environ["GITHUB_OUTPUT"])
        path.write_text("")
        nightly.select()
        return dict(line.split("=", 1) for line in path.read_text().splitlines())

    def asset(self):
        stage = self.root / "KUI"
        stage.mkdir()
        payload = update_package.CAPABILITY + update_package.ENGINE + update_package.HANDOFF_HEADER + bytes(176)
        (stage / "runtime.kui").write_bytes(nightly.envelope(payload, len(payload), self.revision[:12]))
        package = self.root / nightly.ASSET
        nightly.make_update(stage, package, channel="testing")
        return package

    def test_first_publish_doc_only_and_missed_nights(self):
        selected = self.selection()
        self.assertEqual(selected["build"], "true")
        nightly.advance_channel("", selected["inputs"], "first", self.revision)
        first = nightly.remote_ref(nightly.CHANNEL)
        self.assertEqual(self.git("show", first + ":nightly.txt"), "first")
        self.assertEqual(self.selection()["build"], "false")
        self.commit("docs/notes.md", "manual")
        self.commit("src/loader/standalone.c", "reader only")
        self.assertEqual(self.selection()["build"], "false")
        self.commit("src/apps/menu.c", "two")
        self.commit("docs/notes.md", "next day")
        self.assertEqual(self.selection()["build"], "true")
        # The selector is still at the prior successful build until publication.
        self.assertEqual(nightly.remote_ref(nightly.CHANNEL), first)
        nightly.advance_channel(first, self.selection()["inputs"], "second", self.revision)
        self.assertEqual(self.selection()["build"], "false")
        with self.assertRaises(subprocess.CalledProcessError):
            nightly.advance_channel(first, selected["inputs"], "stale", self.revision)
        self.assertEqual(self.git("show", nightly.remote_ref(nightly.CHANNEL) + ":nightly.txt"), "second")

    def test_stale_workflow_commit_is_skipped(self):
        old = self.revision
        self.commit("src/apps/menu.c", "new")
        self.git("checkout", "-q", "--detach", old)
        os.environ["GITHUB_SHA"] = old
        self.assertEqual(self.selection()["build"], "false")

    def test_release_recovers_interrupted_upload_and_does_not_replace_published_assets(self):
        package = self.asset()
        actual = nightly.manifest(package, self.revision)
        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        self.assertEqual(actual, f"KUI-NIGHTLY-1\n{self.revision}\n{package.stat().st_size}\n{digest}\n")
        download = self.root / "download"
        download.mkdir()
        uploaded = self.root / "uploaded"
        info = {}
        commands = []
        original_run = subprocess.run

        def run(args, **kwargs):
            if args[0] != "gh":
                return original_run(args, **kwargs)
            action = args[2]
            commands.append(action)
            if action == "view":
                if not info:
                    return subprocess.CompletedProcess(args, 1, "", "release not found")
                return subprocess.CompletedProcess(args, 0, json.dumps(info), "")
            if action == "create":
                self.assertIn("--draft", args)
                info.update(tagName=args[3], targetCommitish=self.revision, isDraft=True,
                            isPrerelease=True, body=args[args.index("--notes") + 1])
            elif action == "upload":
                self.assertTrue(info["isDraft"])
                if commands.count("upload") == 1:
                    raise subprocess.CalledProcessError(1, args)
                shutil.copyfile(package, uploaded)
            elif action == "edit":
                self.assertTrue(uploaded.exists())
                info.update(isDraft=False, body=args[args.index("--notes") + 1])
            else:
                self.assertEqual(action, "download")
                shutil.copyfile(uploaded, download / nightly.ASSET)
            return subprocess.CompletedProcess(args, 0)

        with mock.patch.object(nightly, "remote_ref", return_value=self.revision), \
                mock.patch.object(nightly.subprocess, "run", side_effect=run):
            with self.assertRaises(subprocess.CalledProcessError):
                nightly.release(self.revision, package, download, actual)
            self.assertTrue(info["isDraft"])
            self.assertEqual(nightly.release(self.revision, package, download, actual), actual)
            self.assertFalse(info["isDraft"])
            commands.clear()
            self.assertEqual(nightly.release(self.revision, package, download, actual), actual)
            self.assertEqual(commands, ["view", "download"])
            info["body"] = "SHA-256: " + "0" * 64
            with self.assertRaisesRegex(ValueError, "differs"):
                nightly.release(self.revision, package, download, actual)

    def test_wrong_source_or_corrupt_kup_never_qualifies(self):
        package = self.asset()
        with self.assertRaisesRegex(ValueError, "requested source"):
            nightly.manifest(package, "a" * 40)
        package.write_bytes(package.read_bytes()[:-1])
        with self.assertRaises(ValueError):
            nightly.manifest(package, self.revision)

    def test_nightly_rejects_a_valid_package_that_also_updates_a_game_loader(self):
        package = self.asset()
        stage = self.root / "KUI"
        loader = stage / "apps/games/ce-boot.kui"
        loader.parent.mkdir(parents=True)
        loader.write_bytes(nightly.envelope(b"loader00", 8, self.revision[:12]))
        nightly.make_update(stage, package, channel="testing")
        with self.assertRaisesRegex(ValueError, "GUI-only"):
            nightly.manifest(package, self.revision)

    def test_main_advancing_during_upload_keeps_previous_feed(self):
        selected = self.selection()
        package = self.asset()
        previous = self.revision

        def finish_release(*args):
            self.commit("src/apps/menu.c", "new during upload")
            return nightly.manifest(package, previous)

        with mock.patch.dict(os.environ, NIGHTLY_CHANNEL_BASE=selected["channel"],
                             NIGHTLY_INPUTS=selected["inputs"]), \
                mock.patch.object(nightly, "release", side_effect=finish_release):
            nightly.publish(package)
        self.assertEqual(nightly.remote_ref(nightly.CHANNEL), "")
        with mock.patch.dict(os.environ, GITHUB_SHA=self.revision):
            self.assertEqual(self.selection()["build"], "true")


if __name__ == "__main__":
    unittest.main()
