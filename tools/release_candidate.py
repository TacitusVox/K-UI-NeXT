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

from channel_feed import git
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


def publish(directory, tag, *, draft):
    metadata, revision, kup, sd = candidate()
    ref = os.environ.get("GITHUB_REF")
    review_draft = ref == REVIEW_REF and draft and tag == REVIEW_TAG
    if os.environ.get("GITHUB_REPOSITORY") != "TacitusVox/K-UI-NeXT" or \
            (ref != "refs/heads/main" and not review_draft) or tag != "v" + metadata["version"]:
        raise ValueError("Candidate publication requires the matching upstream source, mode and tag")
    names = check_assets(directory, revision, kup, sd)
    existing_ref = git("ls-remote", "--refs", "origin", "refs/tags/" + tag)
    if existing_ref and existing_ref.split()[0] != revision:
        raise ValueError("The requested candidate tag already identifies another object")
    info = view(tag)
    if review_draft and info and not info["isDraft"]:
        raise ValueError("A review branch can only create or verify a draft prerelease")
    if info and (info["tagName"] != tag or info["targetCommitish"] != revision or not info["isPrerelease"]):
        raise ValueError("Existing candidate identifies another release or source")
    if not info:
        source = Path("docs") / ("release-v" + metadata["version"].split("-rc.")[0] + "-notes.md")
        subprocess.run(["gh", "release", "create", tag, "--target", revision, "--title", metadata["name"],
                        "--draft", "--prerelease", "--latest=false", "--notes-file", str(source)], check=True)
    if not info or info["isDraft"]:
        subprocess.run(["gh", "release", "upload", tag, *(str(directory / name) for name in names),
                        "--clobber"], check=True)
        verify_remote(tag, directory, revision, names)
        if draft:
            checked = view(tag)
            if not checked or not checked["isDraft"] or not checked["isPrerelease"] or \
                    checked["targetCommitish"] != revision or checked["tagName"] != tag:
                raise ValueError("Draft prerelease target or state changed during upload")
            return
        subprocess.run(["gh", "release", "edit", tag, "--draft=false", "--prerelease", "--latest=false"], check=True)
    # Public assets are immutable to this workflow. Also verify an already-published retry.
    published = view(tag)
    if not published or published["isDraft"] or not published["isPrerelease"] or \
            git("ls-remote", "--refs", "origin", "refs/tags/" + tag).split()[:1] != [revision]:
        raise ValueError("The published prerelease does not identify the requested commit")
    verify_remote(tag, directory, revision, names, compare_local=not info or info["isDraft"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    commands.add_parser("build").add_argument("directory", type=Path)
    promoter = commands.add_parser("publish")
    promoter.add_argument("directory", type=Path)
    promoter.add_argument("--tag", required=True)
    promoter.add_argument("--draft", action="store_true")
    args = parser.parse_args()
    if args.operation == "build":
        build(args.directory)
    else:
        publish(args.directory, args.tag, draft=args.draft)
