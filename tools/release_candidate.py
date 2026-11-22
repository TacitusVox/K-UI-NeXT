#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build or publish an explicitly requested K-UI prerelease."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

from channel_feed import COMMIT, DIGEST, git
from package import release_metadata
from software_distribution import digest, verify_distribution, write_distribution


REVIEW_REF = "refs/heads/codex/3-0-rc1-full-nightlies"
REVIEW_TAG = "v3.0.0-rc.1"


def candidate():
    metadata = release_metadata()
    version = metadata["version"]
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)-rc\.[1-9][0-9]*", version):
        raise ValueError("The checked-out GUI version is not a release candidate")
    revision = git("rev-parse", "HEAD")
    if os.environ.get("GITHUB_SHA", revision) != revision or git("status", "--porcelain", "--untracked-files=normal"):
        raise ValueError("The candidate must come from a clean checkout of the requested revision")
    prefix = metadata["artifact_prefix"]
    return metadata, revision, prefix + "-full.kup", prefix + "-sd.zip"


def acceptance(tag, head, kup, sd):
    """A main promotion can pin the tested ancestor and its two official downloads."""
    path = Path("config/release-acceptance") / (tag + ".json")
    if not path.is_file():
        return None
    record = json.loads(path.read_text(encoding="utf-8"))
    if (not isinstance(record, dict) or set(record) != {"revision", "sha256", "workflow_run_id", "artifact_id"} or
            not isinstance(record["revision"], str) or not COMMIT.fullmatch(record["revision"]) or
            any(type(record[key]) is not int or record[key] < 1 for key in ("workflow_run_id", "artifact_id")) or
            not isinstance(record["sha256"], dict) or set(record["sha256"]) != {kup, sd} or
            not all(isinstance(value, str) and DIGEST.fullmatch(value) for value in record["sha256"].values())):
        raise ValueError("Invalid candidate acceptance record")
    if git("merge-base", record["revision"], head) != record["revision"]:
        raise ValueError("The accepted candidate is not an ancestor of this promotion")
    return record


def check_accepted_hashes(directory, accepted):
    for name, expected in accepted["sha256"].items():
        if digest(directory / name) != expected:
            raise ValueError("Candidate differs from the accepted SHA-256 for " + name)


def request_matches(message, tag, review):
    title = message.partition("\n")[0]
    return title == ("Draft release candidate: " if review else "Release candidate: ") + tag


def checksums(directory, names):
    return "".join(f"{digest(directory / name)}  {name}\n" for name in names)


def build(directory):
    from nightly import build_gui
    from stage_distribution import stage, verify_linked_gui
    metadata, revision, kup, sd = candidate()
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        software = Path(work) / "KUI"
        stage(software)
        build_gui(revision, software)
        verify_linked_gui(software / "runtime.kui", software)
        write_distribution(software, directory / kup, directory / sd,
                           revision=revision, label=metadata["name"])
    (directory / "SHA256SUMS.txt").write_text(checksums(directory, (kup, sd)), encoding="ascii")
    check_assets(directory, revision, kup, sd)


def check_assets(directory, revision, kup, sd):
    from nightly_recipe import read_recipe
    from stage_distribution import checked_inventory, verify_linked_gui, verify_routes, ROOT, SOFTWARE
    names = (kup, sd, "SHA256SUMS.txt")
    if set(p.name for p in directory.iterdir()) != set(names):
        raise ValueError("Candidate assets differ from the two distributions and their checksums")
    if (directory / names[2]).read_text(encoding="ascii") != checksums(directory, names[:2]):
        raise ValueError("Candidate SHA256SUMS differs from the supplied files")
    report = verify_distribution(directory / kup, directory / sd, revision=revision)
    inventory = checked_inventory()
    verify_routes(inventory)
    recipe = read_recipe(revision)
    if not recipe.distribution:
        raise ValueError("A release candidate requires the complete reviewed distribution")
    recipe.check_members(report, revision)
    with tempfile.TemporaryDirectory() as work, zipfile.ZipFile(directory / sd) as archive:
        gui = Path(work) / "runtime.kui"
        gui.write_bytes(archive.read("KUI/runtime.kui"))
        verify_linked_gui(gui, ROOT / SOFTWARE)
        if json.loads(archive.read("build.json"))["label"] != release_metadata()["name"]:
            raise ValueError("SD ZIP identifies another release candidate")
    return names


def view(tag):
    result = subprocess.run(["gh", "release", "view", tag, "--json",
                             "tagName,targetCommitish,isDraft,isPrerelease,assets"],
                            capture_output=True, text=True)
    if not result.returncode:
        return json.loads(result.stdout)
    if result.stderr.strip() != "release not found":
        raise RuntimeError(result.stderr.strip())
    return None


def verify_remote(tag, directory, revision, names, *, compare_local=True):
    with tempfile.TemporaryDirectory() as work:
        received = Path(work)
        subprocess.run(["gh", "release", "download", tag, "--dir", work], check=True)
        check_assets(received, revision, *names[:2])
        if compare_local and any(digest(received / name) != digest(directory / name) for name in names):
            raise ValueError("Remote candidate assets differ from the uploaded inputs")


def check_draft(tag, revision):
    checked = view(tag)
    if (not checked or not checked["isDraft"] or not checked["isPrerelease"] or
            checked["targetCommitish"] != revision or checked["tagName"] != tag):
        raise ValueError("Draft prerelease target or state changed during upload")


def publish(directory, tag, *, draft):
    metadata, revision, kup, sd = candidate()
    ref = os.environ.get("GITHUB_REF")
    review_draft = ref == REVIEW_REF and draft and tag == REVIEW_TAG
    if os.environ.get("GITHUB_REPOSITORY") != "TacitusVox/K-UI-NeXT" or \
            (ref != "refs/heads/main" and not review_draft) or tag != "v" + metadata["version"]:
        raise ValueError("Candidate publication requires the matching upstream source, mode and tag")
    accepted = None if review_draft else acceptance(tag, revision, kup, sd)
    if accepted:
        revision = accepted["revision"]
        check_accepted_hashes(directory, accepted)
    names = check_assets(directory, revision, kup, sd)
    existing_ref = git("ls-remote", "--refs", "origin", "refs/tags/" + tag)
    if existing_ref and existing_ref.split()[0] != revision:
        raise ValueError("The requested candidate tag already identifies another object")
    info = view(tag)
    if review_draft and info and not info["isDraft"]:
        raise ValueError("A review branch can only create or verify a draft prerelease")
    if info and (info["tagName"] != tag or not info["isPrerelease"]):
        raise ValueError("Existing candidate identifies another release or source")
    notes = Path("docs") / ("release-v" + metadata["version"].split("-rc.")[0] + "-notes.md")
    if info and info["targetCommitish"] != revision:
        previous = info["targetCommitish"]
        if (not accepted or not info["isDraft"] or existing_ref or not COMMIT.fullmatch(previous) or
                git("merge-base", previous, revision) != previous):
            raise ValueError("Existing candidate identifies another release or source")
    if not info:
        subprocess.run(["gh", "release", "create", tag, "--target", revision, "--title", metadata["name"],
                        "--draft", "--prerelease", "--latest=false", "--notes-file", str(notes)], check=True)
    elif accepted and info["isDraft"]:
        subprocess.run(["gh", "release", "edit", tag, "--draft", "--prerelease", "--latest=false",
                        "--target", revision, "--notes-file", str(notes)], check=True)
        check_draft(tag, revision)
    if not info or info["isDraft"]:
        subprocess.run(["gh", "release", "upload", tag, *(str(directory / name) for name in names),
                        "--clobber"], check=True)
        verify_remote(tag, directory, revision, names)
        check_draft(tag, revision)
        if draft:
            return
        # GitHub ignores --target when a tag exists, so repeat the tag check
        # after the network upload and immediately before making assets public.
        tag_ref = git("ls-remote", "--refs", "origin", "refs/tags/" + tag)
        if tag_ref and tag_ref.split()[0] != revision:
            raise ValueError("The requested candidate tag changed during upload")
        subprocess.run(["gh", "release", "edit", tag, "--draft=false", "--prerelease", "--latest=false"], check=True)
    # Public assets are immutable to this workflow. Also verify an already-published retry.
    published = view(tag)
    if not published or published["isDraft"] or not published["isPrerelease"] or \
            git("ls-remote", "--refs", "origin", "refs/tags/" + tag).split()[:1] != [revision]:
        raise ValueError("The published prerelease does not identify the requested commit")
    verify_remote(tag, directory, revision, names, compare_local=bool(accepted) or not info or info["isDraft"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    commands.add_parser("build").add_argument("directory", type=Path)
    commands.add_parser("verify-accepted").add_argument("directory", type=Path)
    promoter = commands.add_parser("publish")
    promoter.add_argument("directory", type=Path)
    promoter.add_argument("--tag", required=True)
    promoter.add_argument("--draft", action="store_true")
    args = parser.parse_args()
    if args.operation == "build":
        build(args.directory)
    elif args.operation == "verify-accepted":
        metadata, head, kup, sd = candidate()
        approved = acceptance("v" + metadata["version"], head, kup, sd)
        if not approved:
            raise ValueError("Missing candidate acceptance record")
        check_accepted_hashes(args.directory, approved)
        check_assets(args.directory, approved["revision"], kup, sd)
    else:
        publish(args.directory, args.tag, draft=args.draft)
