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
import zipfile
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import nightly
import nightly_history as history
from channel_feed import write_feed
import update_package


class NightlyTests(unittest.TestCase):
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)
        self.repo = self.root / "source"
        self.repo.mkdir()
        sources = mock.patch("software_distribution.ROOT", self.repo)
        sources.start()
        self.addCleanup(sources.stop)
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
        # The temporary repo has no factory catalog; individual tests supply its assets.
        derived = mock.patch("nightly_recipe.derived_members", return_value={})
        self.derived = derived.start()
        self.addCleanup(derived.stop)

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

    @mock.patch("nightly_history.publication", return_value=("2026-11-20T03:33:31Z", history.GUI_ONLY))
    def test_first_publish_doc_only_and_missed_nights(self, checked_release):
        selected = self.selection()
        self.assertEqual(selected["build"], "true")
        first_body = nightly.manifest(self.asset(), self.revision)
        nightly.advance_channel("", selected["inputs"], first_body, self.revision)
        first = nightly.remote_ref(nightly.CHANNEL)
        self.assertEqual(self.git("show", first + ":nightly.txt"), first_body.strip())
        self.assertEqual(self.selection()["build"], "false")
        self.commit("docs/notes.md", "manual")
        self.commit("src/loader/standalone.c", "reader only")
        self.assertEqual(self.selection()["build"], "false")
        self.commit("src/apps/menu.c", "two")
        self.commit("docs/notes.md", "next day")
        self.assertEqual(self.selection()["build"], "true")
        # The selector is still at the prior successful build until publication.
        self.assertEqual(nightly.remote_ref(nightly.CHANNEL), first)
        second_body = first_body.replace(first_body.splitlines()[1], self.revision)
        nightly.advance_channel(first, self.selection()["inputs"], second_body, self.revision)
        self.assertEqual(self.selection()["build"], "false")
        with self.assertRaises(subprocess.CalledProcessError):
            nightly.advance_channel(first, selected["inputs"], second_body, self.revision)
        self.assertEqual(self.git("show", nightly.remote_ref(nightly.CHANNEL) + ":nightly.txt"), second_body.strip())

    def test_picker_ten_newest_first_and_exact_closure(self):
        dates = {}

        def published(entry, check, *, known_scope=None):
            return (dates[entry.commit], history.GUI_ONLY) if entry.commit in dates else None

        stages = []
        with mock.patch.object(history, "publication", side_effect=published):
            for day in range(1, 13):
                self.commit("src/apps/menu.c", f"menu {day}")
                dates[self.revision] = f"2026-11-{day:02d}T03:33:31Z"
                entry = history.Entry(self.revision, "500", hashlib.sha256(str(day).encode()).hexdigest())
                nightly.advance_channel(nightly.remote_ref(nightly.CHANNEL), nightly.fingerprint(),
                                        entry.manifest, self.revision)
                stages.append(entry)
            ref = nightly.remote_ref(nightly.CHANNEL)
            rows = history.parse_index(history.contents(ref, "nightlies.txt"))
            self.assertEqual([row for row, _ in rows], stages[:-11:-1])
            self.assertEqual(history.contents(ref, "nightly.txt"), stages[-1].manifest)
            for entry, timestamp in rows:
                prefix = "history/" + entry.commit + "/"
                self.assertEqual(history.contents(ref, prefix + "nightly.txt"), entry.manifest)
                info = history.contents(ref, prefix + "update-info.txt")
                self.assertEqual(info.splitlines()[1:3], [entry.commit, entry.digest])
                self.assertIn("/releases/tag/nightly-" + entry.commit + ")", history.contents(ref, "README.md"))
                self.assertEqual(dates[entry.commit], timestamp)
            nightly.advance_channel(ref, nightly.fingerprint(), stages[-1].manifest, self.revision)
            self.assertEqual(nightly.remote_ref(nightly.CHANNEL), ref)
            modified = history.Entry(self.revision, "510", stages[-1].digest)
            with self.assertRaisesRegex(ValueError, "bytes changed"):
                nightly.advance_channel(ref, nightly.fingerprint(), modified.manifest, self.revision)
            self.assertEqual(nightly.remote_ref(nightly.CHANNEL), ref)

    def test_reader_updates_keep_original_gui_feed_while_advancing_new_index(self):
        verified = {}
        def publish(entry, check, *, known_scope=None):
            return verified[entry.commit]
        with mock.patch.object(history, "publication", side_effect=publish):
            for day in range(1, 13):
                self.commit("src/apps/menu.c", f"menu {day}")
                entry = history.Entry(self.revision, "500", hashlib.sha256(bytes(day)).hexdigest())
                mode = history.GUI_ONLY if day == 1 else "partial 1 0 0 2"
                verified[entry.commit] = (f"2026-11-{day:02d}T00:00:00Z", mode)
                nightly.advance_channel(nightly.remote_ref(nightly.CHANNEL), nightly.fingerprint(),
                                        entry.manifest, self.revision)
                if day == 1:
                    first_gui = entry
            ref = nightly.remote_ref(nightly.CHANNEL)
            self.assertEqual(history.contents(ref, "nightly.txt"), first_gui.manifest)
            self.assertIn(first_gui.digest, history.contents(ref, "update-info.txt"))
            self.assertEqual(len(history.parse_index(history.contents(ref, "nightlies.txt"))), 1)
            full = history.parse_index(history.contents(ref, "nightlies-v2.txt"), rich=True)
            self.assertEqual(len(full), 10)
            self.assertEqual(full[0], (entry, *verified[entry.commit]))
            self.assertTrue(all(scope == "partial 1 0 0 2" for _, _, scope in full))

    def test_backfill_verified_promotions_skips_unavailable_preserves_latest(self):
        dates = {}
        adopted = []

        def published(entry, check, *, known_scope=None):
            self.assertIsNone(known_scope)
            adopted.append(entry.commit)
            return (dates[entry.commit], history.GUI_ONLY) if entry.commit in dates else None

        parent = ""
        entries = []
        for day in range(1, 4):
            self.commit("src/apps/menu.c", f"menu {day}")
            entry = history.Entry(self.revision, "400", hashlib.sha256(bytes(day)).hexdigest())
            inputs = nightly.fingerprint()
            write_feed(nightly.CHANNEL, parent,
                       {"nightly.txt": entry.manifest, "inputs.sha256": inputs + "\n"}, self.revision)
            parent = nightly.remote_ref(nightly.CHANNEL)
            entries.append(entry)
            if day != 2:
                dates[self.revision] = f"2026-11-{day:02d}T03:33:31Z"
        self.assertEqual(self.selection()["build"], "false")
        self.assertEqual(self.selection()["migrate"], "true")
        with mock.patch.dict(os.environ, NIGHTLY_CHANNEL_BASE=parent, NIGHTLY_INPUTS=inputs), \
                mock.patch.object(history, "publication", side_effect=published):
            nightly.migrate()
        head = nightly.remote_ref(nightly.CHANNEL)
        self.assertNotEqual(parent, head)
        self.assertEqual(history.contents(head, "nightly.txt"), entries[-1].manifest)
        self.assertEqual([r for r, _ in history.parse_index(history.contents(head, "nightlies.txt"))],
                         [entries[2], entries[0]])
        self.assertEqual(set(adopted), {entry.commit for entry in entries})
        self.assertEqual(self.selection()["migrate"], "false")
        self.assertEqual(self.selection()["build"], "false")

    def test_legacy_asset_is_downloaded_and_verified_against_original_receipt(self):
        package = self.asset()
        entry = history.Entry.from_manifest(nightly.manifest(package, self.revision))
        tag = "nightly-" + entry.commit
        asset = dict(name=nightly.ASSET, state="uploaded", size=int(entry.size), digest="sha256:" + entry.digest)
        release = dict(tag_name=tag, prerelease=True, draft=False, target_commitish=entry.commit,
                       published_at="2026-11-20T03:33:31Z", body="SHA-256: " + entry.digest, assets=[asset])
        original_run = subprocess.run
        transfers = []

        def gh(args, **kwargs):
            if args[0] != "gh":
                return original_run(args, **kwargs)
            if args[1] == "api":
                return subprocess.CompletedProcess(args, 0, json.dumps(release), "")
            self.assertEqual(args[:4], ["gh", "release", "download", tag])
            shutil.copyfile(package, Path(args[-1]) / nightly.ASSET)
            transfers.append(tag)
            return subprocess.CompletedProcess(args, 0)

        with mock.patch.object(history, "remote_ref", return_value=entry.commit), \
                mock.patch.object(history.subprocess, "run", side_effect=gh):
            self.assertEqual(history.publication(entry, nightly.manifest), (release["published_at"], history.GUI_ONLY))
            self.assertEqual(transfers, [tag])
            self.assertEqual(history.publication(entry, nightly.manifest, known_scope=history.GUI_ONLY), (release["published_at"], history.GUI_ONLY))
            self.assertEqual(transfers, [tag])
            asset["digest"] = "sha256:" + "0" * 64
            self.assertIsNone(history.publication(entry, nightly.manifest))
            asset["digest"] = None
            package.write_bytes(package.read_bytes()[:-1])
            self.assertIsNone(history.publication(entry, nightly.manifest))
            release["draft"] = True
            self.assertIsNone(history.publication(entry, nightly.manifest, known_scope=history.GUI_ONLY))

    def test_picker_index_rejects_malformed_duplicate_unordered_dates(self):
        a = history.Entry("a"*40, "600", "0"*64)
        b = history.Entry("b"*40, "700", "1"*64)
        def row(entry, stamp):
            return f"{entry.commit} {entry.size} {entry.digest} {stamp}\n"
        x = row(a, "2026-11-20T00:00:02Z")
        y = row(b, "2026-11-20T00:00:01Z")
        for text in ("KUI-NIGHTLIES-1\n1\n" + x.rstrip(), "KUI-NIGHTLIES-1\n2\n" + x + x,
                     "KUI-NIGHTLIES-1\n2\n" + y + x, "KUI-NIGHTLIES-1\n1\n" + x.replace("-20T", "-32T"),
                     "KUI-NIGHTLIES-1\n11\n" + x*11, "KUI-NIGHTLIES-1\n1\n" + x.replace(" 600 ", " 0600 ")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                history.parse_index(text)
        self.assertEqual(len(history.parse_index("KUI-NIGHTLIES-1\n2\n" + x + y)), 2)

    def test_stale_workflow_commit_is_skipped(self):
        old = self.revision
        self.commit("src/apps/menu.c", "new")
        self.git("checkout", "-q", "--detach", old)
        os.environ["GITHUB_SHA"] = old
        self.assertEqual(self.selection()["build"], "false")

    def test_release_recovers_interrupted_upload_and_does_not_replace_published_assets(self):
        package = self.distribution_assets()
        actual = nightly.manifest(package, self.revision)
        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        self.assertEqual(actual, f"KUI-NIGHTLY-1\n{self.revision}\n{package.stat().st_size}\n{digest}\n")
        download = self.root / "download"
        download.mkdir()
        uploaded = self.root / "uploaded"
        uploaded.mkdir()
        info = {}
        commands = []
        corrupt_first_download = True
        original_run = subprocess.run

        def run(args, **kwargs):
            nonlocal corrupt_first_download
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
                for path in args[4:-1]:
                    path = Path(path)
                    shutil.copyfile(path, uploaded / path.name)
            elif action == "edit":
                self.assertIn("download", commands[:-1])
                self.assertTrue((uploaded / nightly.SD_ZIP).is_file())
                info.update(isDraft=False, body=args[args.index("--notes") + 1])
            else:
                self.assertEqual(action, "download")
                for name in (nightly.ASSET, nightly.SD_ZIP, nightly.SUMS):
                    shutil.copyfile(uploaded / name, download / name)
                if corrupt_first_download:
                    corrupt_first_download = False
                    (download / nightly.SD_ZIP).write_bytes(b"incomplete asset")
            return subprocess.CompletedProcess(args, 0)

        with mock.patch.object(nightly, "remote_ref", return_value=self.revision), \
                mock.patch.object(nightly.subprocess, "run", side_effect=run):
            with self.assertRaises(subprocess.CalledProcessError):
                nightly.release(self.revision, package, download, actual)
            self.assertTrue(info["isDraft"])
            with self.assertRaisesRegex(ValueError, "SHA256SUMS"):
                nightly.release(self.revision, package, download, actual)
            self.assertTrue(info["isDraft"])
            self.assertNotIn("edit", commands)
            self.assertEqual(nightly.release(self.revision, package, download, actual), actual)
            self.assertFalse(info["isDraft"])
            commands.clear()
            self.assertEqual(nightly.release(self.revision, package, download, actual), actual)
            self.assertEqual(commands, ["view", "download"])
            info["body"] = "SHA-256: " + "0" * 64
            with self.assertRaisesRegex(ValueError, "differ"):
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
        with self.assertRaisesRegex(ValueError, "inventory"):
            nightly.manifest(package, self.revision)

    def build_legacy_fixture(self, output):
        # Regenerate historical artifacts only to verify the archived feed.
        recipe = nightly.read_recipe(self.revision)
        with tempfile.TemporaryDirectory() as stage:
            recipe.stage(self.revision, stage)
            if recipe.gui and not recipe.full:
                nightly.build_gui(self.revision, stage)
            report = nightly.make_update(Path(stage), output, channel="testing",
                                        full=recipe.full, remove=recipe.remove)
            recipe.check_members(report, self.revision)

    def distribution(self):
        name = "apps/games/retail-boot.kui"
        data = nightly.envelope(b"reader00", 8, "abcdef012345")
        model = {"schema": 1, "reader_inputs_sha256": "a" * 64,
                 "files": {name: hashlib.sha256(data).hexdigest()}}
        self.commit("config/distribution.json", json.dumps(model))
        self.commit(nightly.RECIPE, json.dumps({"distribution": "config/distribution.json"}))
        return name, data

    def distribution_assets(self):
        name, data = self.distribution()
        stage = self.root / "KUI"
        target = stage / name
        target.parent.mkdir(parents=True)
        target.write_bytes(data)
        payload = update_package.CAPABILITY + update_package.ENGINE + update_package.HANDOFF_HEADER + bytes(176)
        (stage / "runtime.kui").write_bytes(nightly.envelope(payload, len(payload), self.revision[:12]))
        package = self.root / nightly.ASSET
        from software_distribution import write_distribution
        write_distribution(stage, package, self.root / nightly.SD_ZIP, revision=self.revision, label="GUI test")
        (self.root / nightly.SUMS).write_text(nightly.checksum_text(package))
        return package

    def test_new_nightly_requires_complete_distribution(self):
        with self.assertRaisesRegex(ValueError, "complete reviewed distribution"):
            nightly.package(self.root / nightly.ASSET)
        self.prepare_recipe(gui=True)
        with self.assertRaisesRegex(ValueError, "complete reviewed distribution"):
            nightly.package(self.root / nightly.ASSET)

    def test_complete_nightly_detects_reader_sources_and_verifies_inventory(self):
        asset = self.distribution_assets()
        revision = self.revision
        first = nightly.fingerprint()
        body = nightly.verify_assets(asset, revision)
        self.assertEqual(body.splitlines()[1], revision)
        start = self.selection()
        self.assertEqual(start["gui"], "true")
        self.assertEqual(start["build"], "true")
        with mock.patch.object(history, "publication", return_value=(
                "2026-11-22T00:00:00Z", "full 2 0 0 3")):
            nightly.advance_channel("", first, body, revision)
        self.assertEqual(self.selection()["build"], "false")
        self.commit("docs/guide.md", "notes")
        self.assertEqual(nightly.fingerprint(), first)
        self.assertEqual(self.selection()["build"], "false")
        self.commit("src/loader/standalone.c", "updated reader")
        self.assertNotEqual(nightly.fingerprint(), first)
        self.assertEqual(self.selection()["build"], "true")
        self.assertEqual(nightly.manifest(asset, revision), body)
        self.assertTrue(update_package.verify(asset)["full"])
        self.assertEqual({m["build"] for m in update_package.verify(asset)["members"]},
                         {"abcdef012345", revision[:12]})
        # A partial package with the identical members cannot qualify.
        nightly.make_update(self.root / "KUI", asset, channel="testing")
        with self.assertRaisesRegex(ValueError, "inventory"):
            nightly.manifest(asset, revision)
        with self.assertRaisesRegex(ValueError, "checksums|SHA256SUMS"):
            nightly.verify_assets(asset, revision)

    def test_factory_assets_follow_their_original_revision(self):
        name, data = self.distribution()
        # Derive a canonical asset from each revision in the temporary repository.
        def factory(revision):
            body = subprocess.check_output(["git", "show", revision + ":data/catalog"])
            return {"redump.db": hashlib.sha256(body).hexdigest()}
        self.derived.side_effect = factory
        self.commit("data/catalog", "first catalog")
        first = self.revision
        stage = self.root / "factory"
        (stage / name).parent.mkdir(parents=True)
        (stage / name).write_bytes(data)
        (stage / "redump.db").write_text("first catalog")
        payload = update_package.CAPABILITY + update_package.ENGINE + update_package.HANDOFF_HEADER + bytes(176)
        def gui(revision):
            (stage / "runtime.kui").write_bytes(nightly.envelope(payload, len(payload), revision[:12]))
        gui(first)
        package = self.root / nightly.ASSET
        nightly.make_update(stage, package, full=True, channel="testing")
        receipt = nightly.manifest(package, first)
        self.commit("data/catalog", "second catalog")
        self.assertEqual(nightly.manifest(package, first), receipt)
        gui(self.revision)
        nightly.make_update(stage, package, full=True, channel="testing")
        with self.assertRaisesRegex(ValueError, "reviewed software"):
            nightly.manifest(package, self.revision)
        (stage / "redump.db").write_text("second catalog")
        nightly.make_update(stage, package, full=True, channel="testing")
        self.assertEqual(nightly.manifest(package, self.revision).splitlines()[1], self.revision)

    def test_nightly_producer_checks_linked_gui_before_building_both_assets(self):
        name, data = self.distribution()
        output = self.root / nightly.ASSET

        def stage_reader(recipe, revision, stage):
            path = stage / name
            path.parent.mkdir(parents=True)
            path.write_bytes(data)

        def build_gui(revision, stage):
            payload = update_package.CAPABILITY + update_package.ENGINE + update_package.HANDOFF_HEADER + bytes(176)
            (stage / "runtime.kui").write_bytes(nightly.envelope(payload, len(payload), revision[:12]))

        def validate_gui(gui, stage):
            self.assertEqual(gui, stage / "runtime.kui")
            self.assertTrue(gui.is_file())
            self.assertEqual((stage / name).read_bytes(), data)

        with mock.patch("nightly_recipe.Recipe.stage", stage_reader), \
                mock.patch.object(nightly, "build_gui", side_effect=build_gui), \
                mock.patch("stage_distribution.verify_linked_gui", side_effect=validate_gui) as admission:
            nightly.package(output)
        admission.assert_called_once()
        nightly.verify_assets(output, self.revision)
        zip_path = self.root / nightly.SD_ZIP
        zip_path.write_bytes(zip_path.read_bytes()[:-300])
        (self.root / nightly.SUMS).write_text(nightly.checksum_text(output))
        with self.assertRaises((ValueError, zipfile.BadZipFile)):
            nightly.verify_assets(output, self.revision)

    def prepare_recipe(self, *, gui=False, full=False):
        name = "apps/games/ce-boot.kui"
        data = nightly.envelope(b"settled0", 16, "0123456789ab")
        path = Path(nightly.SOFTWARE) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        self.git("add", "--", str(path))
        recipe = {"gui": gui, "full": full, "files": {name: hashlib.sha256(data).hexdigest()},
                  "remove": [] if full else ["apps/games/old-helper.bin"]}
        self.commit(nightly.RECIPE, json.dumps(recipe))
        return recipe, path

    def test_reader_only_uses_reviewed_git_bytes_and_source_without_native_gui(self):
        recipe, path = self.prepare_recipe()
        original = self.revision
        package = self.root / nightly.ASSET
        path.write_bytes(b"changed in workspace")
        with mock.patch.object(nightly, "build_gui", side_effect=AssertionError("unexpected link")):
            self.build_legacy_fixture(package)
        report = update_package.verify(package)
        self.assertEqual(report["members"][0]["build"], "0123456789ab")
        self.assertEqual(report["members"][1]["policy"], update_package.REMOVE)
        self.assertEqual(nightly.manifest(package, original).splitlines()[1], original)
        inputs = nightly.fingerprint()
        self.commit("src/apps/menu.c", "unrelated GUI update")
        self.assertEqual(nightly.fingerprint(), inputs)
        self.assertEqual(self.selection()["gui"], "false")
        recipe["files"]["apps/games/ce-boot.kui"] = "0" * 64
        self.commit(nightly.RECIPE, json.dumps(recipe))
        self.assertNotEqual(nightly.fingerprint(), inputs)
        with self.assertRaisesRegex(ValueError, "reviewed software"):
            nightly.manifest(package, self.revision)
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.build_legacy_fixture(self.root / "unexpected.kup")
        self.assertFalse((self.root / "unexpected.kup").exists())
        self.assertEqual(nightly.manifest(package, original).splitlines()[1], original)

    def test_gui_and_reader_preserve_independent_build_ids(self):
        self.prepare_recipe(gui=True)
        payload = update_package.CAPABILITY + update_package.ENGINE + update_package.HANDOFF_HEADER + bytes(176)
        def fixture_gui(revision, stage):
            (Path(stage) / "runtime.kui").write_bytes(nightly.envelope(payload, len(payload), revision[:12]))
        with mock.patch.object(nightly, "build_gui", side_effect=fixture_gui):
            package = self.root / nightly.ASSET
            self.build_legacy_fixture(package)
        reported = update_package.verify(package)
        self.assertFalse(reported["full"])
        self.assertEqual([m["build"] for m in reported["members"][:2]], [self.revision[:12], "0123456789ab"])
        self.assertEqual(nightly.manifest(package, self.revision).splitlines()[1], self.revision)

    def test_full_flag_cannot_promote_a_selection_of_individual_files(self):
        self.prepare_recipe(gui=True, full=True)
        with self.assertRaisesRegex(ValueError, "qualified tag"):
            self.build_legacy_fixture(self.root / "invalid.kup")

    def test_denies_user_data_and_duplicate_or_nonregular_inputs(self):
        recipe, path = self.prepare_recipe()
        original = recipe["files"]
        for name in ("../private", "apps/games/selection-a.bin", "vmu/card.vmd", "runtime.kui"):
            recipe["files"] = {name: "0" * 64}
            self.commit(nightly.RECIPE, json.dumps(recipe))
            with self.assertRaisesRegex(ValueError, "paths"):
                nightly.read_recipe(self.revision)
        recipe["files"] = original
        recipe["remove"] = list(original)
        self.commit(nightly.RECIPE, json.dumps(recipe))
        with self.assertRaisesRegex(ValueError, "paths"):
            nightly.read_recipe(self.revision)
        recipe["remove"] = []
        path.unlink()
        path.symlink_to("elsewhere")
        self.git("add", "--", str(path))
        self.commit(nightly.RECIPE, json.dumps(recipe))
        with self.assertRaisesRegex(ValueError, "regular tracked file"):
            self.build_legacy_fixture(self.root / "unexpected.kup")

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
