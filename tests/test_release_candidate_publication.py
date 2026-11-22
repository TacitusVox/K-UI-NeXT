#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that a candidate can advance only its own prerelease after asset verification."""
from pathlib import Path
import hashlib
import json
import tempfile
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import release_candidate as rc


class CandidatePublicationTests(unittest.TestCase):
    def setUp(self):
        self.sha = "1234567890abcdef1234567890abcdef12345678"
        self.tag = "v3.0.0-rc.1"
        self.existing = dict(tagName=self.tag, targetCommitish=self.sha, isDraft=False, isPrerelease=True)
        self.names = ("kui.kup", "sd.zip", "SHA256SUMS.txt")
        self.addCleanup(patch.stopall)
        patch.dict(rc.os.environ, GITHUB_REPOSITORY="TacitusVox/K-UI-NeXT", GITHUB_REF="refs/heads/main").start()
        patch.object(rc, "candidate", return_value=({"version": self.tag[1:], "name": "K-UI 3.0 RC1"},
                                                    self.sha, *self.names[:2])).start()
        self.acceptance = patch.object(rc, "acceptance", return_value=None).start()
        patch.object(rc, "check_assets", return_value=self.names).start()
        self.git = patch.object(rc, "git", return_value=self.sha + "\trefs/tags/" + self.tag).start()
        self.view = patch.object(rc, "view").start()
        self.command = patch.object(rc.subprocess, "run").start()
        self.verify = patch.object(rc, "verify_remote").start()

    def test_bad_tag_or_nonprerelease_is_never_modified(self):
        with self.assertRaisesRegex(ValueError, "matching upstream"):
            rc.publish(Path("."), "v3.0.0", draft=False)
        self.view.return_value = dict(self.existing, isPrerelease=False)
        with self.assertRaisesRegex(ValueError, "another release"):
            rc.publish(Path("."), self.tag, draft=False)
        self.git.return_value = "0" * 40 + "\trefs/tags/" + self.tag
        with self.assertRaisesRegex(ValueError, "another object"):
            rc.publish(Path("."), self.tag, draft=False)
        self.command.assert_not_called()

    def test_review_branch_can_create_only_the_rc1_draft(self):
        with patch.dict(rc.os.environ, GITHUB_REF=rc.REVIEW_REF):
            with self.assertRaisesRegex(ValueError, "matching upstream"):
                rc.publish(Path("."), self.tag, draft=False)
            self.view.return_value = self.existing
            with self.assertRaisesRegex(ValueError, "only create or verify a draft"):
                rc.publish(Path("."), self.tag, draft=True)
            self.command.assert_not_called()
            self.git.return_value = ""
            self.view.side_effect = [None, dict(self.existing, isDraft=True)]
            rc.publish(Path("."), self.tag, draft=True)
        self.assertEqual([call.args[0][2] for call in self.command.call_args_list], ["create", "upload"])
        self.verify.assert_called_once()

    def test_publication_stops_at_draft_on_remote_checksum_failure(self):
        self.git.return_value = ""
        self.view.return_value = None
        self.verify.side_effect = ValueError("bad digest")
        with self.assertRaisesRegex(ValueError, "bad digest"):
            rc.publish(Path("."), self.tag, draft=False)
        calls = [call.args[0] for call in self.command.call_args_list]
        self.assertEqual([args[2] for args in calls], ["create", "upload"])
        self.assertIn("--draft", calls[0])
        self.assertIn("--prerelease", calls[0])
        self.assertIn("--latest=false", calls[0])

    def test_rerun_on_published_candidate_only_verifies_original_assets(self):
        self.view.return_value = self.existing
        rc.publish(Path("."), self.tag, draft=False)
        self.command.assert_not_called()
        self.verify.assert_called_once_with(self.tag, Path("."), self.sha, self.names, compare_local=False)

    def accepted(self, directory):
        hashes = {}
        for name in self.names[:2]:
            (directory / name).write_bytes(name.encode())
            hashes[name] = hashlib.sha256(name.encode()).hexdigest()
        self.acceptance.return_value = {"revision": self.sha, "sha256": hashes}

    def test_main_retargets_only_unpublished_ancestor_and_checks_before_publishing(self):
        old = "2" * 40
        draft = dict(self.existing, isDraft=True)
        self.view.side_effect = [dict(draft, targetCommitish=old), draft, draft, self.existing]
        self.git.side_effect = ["", old, "", self.sha + "\trefs/tags/" + self.tag]
        events = []
        self.command.side_effect = lambda args, **kwargs: events.append(args)
        self.verify.side_effect = lambda *args, **kwargs: events.append(["verify"])
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            self.accepted(directory)
            rc.publish(directory, self.tag, draft=False)
        self.assertEqual([cmd[2] if len(cmd)>1 else cmd[0] for cmd in events],
                         ["edit", "upload", "verify", "edit", "verify"])
        self.assertIn("--draft", events[0])
        self.assertIn("--notes-file", events[0])
        self.assertEqual(events[0][events[0].index("--target")+1], self.sha)
        self.assertIn("--draft=false", events[3])
        self.assertIn("--latest=false", events[3])

    def test_different_public_existing_tag_or_unrelated_draft_cannot_be_retargeted(self):
        other = "2" * 40
        for is_draft, tag_ref, common in [(False, "", other), (True, self.sha, other), (True, "", "3"*40)]:
            with self.subTest(is_draft=is_draft, tag_ref=tag_ref, common=common), tempfile.TemporaryDirectory() as work:
                directory = Path(work)
                self.accepted(directory)
                self.git.side_effect = [tag_ref, common]
                self.view.return_value = dict(self.existing, targetCommitish=other, isDraft=is_draft)
                with self.assertRaisesRegex(ValueError, "another release"):
                    rc.publish(directory, self.tag, draft=False)
                self.command.assert_not_called()

    def test_incorrect_accepted_download_stops_before_remote_access(self):
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            self.accepted(directory)
            (directory / self.names[1]).write_bytes(b"another distribution")
            with self.assertRaisesRegex(ValueError, "accepted SHA-256"):
                rc.publish(directory, self.tag, draft=False)
        self.view.assert_not_called()
        self.command.assert_not_called()

    def test_draft_changed_after_upload_is_never_published(self):
        self.git.return_value = ""
        self.view.side_effect = [dict(self.existing, isDraft=True), self.existing]
        with self.assertRaisesRegex(ValueError, "state changed"):
            rc.publish(Path("."), self.tag, draft=False)
        self.assertEqual([call.args[0][2] for call in self.command.call_args_list], ["upload"])

    def test_tag_appearing_on_other_source_while_uploading_stops_publication(self):
        draft = dict(self.existing, isDraft=True)
        self.view.side_effect = [draft, draft]
        self.git.side_effect = ["", "2"*40 + "\trefs/tags/" + self.tag]
        with self.assertRaisesRegex(ValueError, "tag changed"):
            rc.publish(Path("."), self.tag, draft=False)
        self.assertEqual([call.args[0][2] for call in self.command.call_args_list], ["upload"])

    def test_public_accepted_release_verifies_exact_original_bytes_without_uploading(self):
        self.view.return_value = self.existing
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            self.accepted(directory)
            rc.publish(directory, self.tag, draft=False)
        self.verify.assert_called_once_with(self.tag, directory, self.sha, self.names, compare_local=True)
        self.command.assert_not_called()


class CandidateAcceptanceTests(unittest.TestCase):
    def test_exact_subject_and_merge_body(self):
        tag = "v3.0.0-rc.1"
        self.assertTrue(rc.request_matches("Release candidate: " + tag + "\n\nMerge changes from PR 15.", tag, False))
        self.assertFalse(rc.request_matches("Release candidate: " + tag + " temporary", tag, False))
        self.assertFalse(rc.request_matches("Release candidate: " + tag, tag, True))

    def test_accepted_revision_must_be_on_the_checked_out_main_history(self):
        source, head = "1"*40, "2"*40
        record = {"revision":source, "sha256":{"a.kup":"a"*64,"a.zip":"b"*64}, "workflow_run_id":5, "artifact_id":6}
        with patch.object(Path, "is_file", return_value=True), patch.object(Path, "read_text", return_value=json.dumps(record)), patch.object(rc, "git") as git:
            git.return_value = source
            self.assertEqual(rc.acceptance("v1", head, "a.kup", "a.zip"), record)
            git.assert_called_with("merge-base", source, head)
            git.return_value = head
            with self.assertRaisesRegex(ValueError, "not an ancestor"):
                rc.acceptance("v1", head, "a.kup", "a.zip")
            for changed in (dict(record, revision="main"), dict(record, artifact_id=0), dict(record, sha256={"a.kup":"a"*64})):
                with patch.object(Path, "read_text", return_value=json.dumps(changed)), self.assertRaisesRegex(ValueError, "Invalid candidate acceptance"):
                    rc.acceptance("v1", head, "a.kup", "a.zip")


if __name__ == "__main__":
    unittest.main()
