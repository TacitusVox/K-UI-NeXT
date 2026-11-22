#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that a candidate can advance only its own prerelease after asset verification."""
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
