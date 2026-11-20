#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Index a published, complete stable release in the console's release-only feed."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

from channel_feed import COMMIT, DIGEST, fetch_feed, git, remote_ref, update_info, write_feed
from publish_release import verify_update_asset
from update_package import HEADER_BYTES, MAX_ENTRIES, MAX_PAYLOAD, V2_ENTRY_BYTES

CHANNEL = "refs/heads/release-channel"
REPOSITORY = "TacitusVox/K-UI-NeXT"
ASSET_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,58}\.kup")


def version(tag):
    if not re.fullmatch(r"v(?:0|[1-9][0-9]{0,9})(?:\.(?:0|[1-9][0-9]{0,9})){2}", tag):
        raise ValueError("Stable channel requires a vMAJOR.MINOR.PATCH tag")
    fields = tuple(map(int, tag[1:].split(".")))
    if any(n > 0xffffffff for n in fields):
        raise ValueError("Release version exceeds the console's limit")
    return fields


def latest():
    return json.loads(subprocess.check_output(["gh", "api", "repos/" + REPOSITORY + "/releases/latest"]))


def assets_for(release):
    if release["draft"] or release["prerelease"] or not release.get("published_at"):
        raise ValueError("Stable channel accepts only a published, non-prerelease release")
    assets = release["assets"]
    packages = [a for a in assets if a["name"].lower().endswith(".kup")]
    if not packages:
        return None  # Published archive-only releases remain available for manual SD installs.
    version(release["tag_name"])
    if len(packages) != 1 or not ASSET_NAME.fullmatch(packages[0]["name"]) or ".." in packages[0]["name"]:
        raise ValueError("Expected one console-compatible KUP asset name")
    if not HEADER_BYTES < packages[0]["size"] <= HEADER_BYTES + MAX_PAYLOAD + MAX_ENTRIES * V2_ENTRY_BYTES:
        raise ValueError("Release KUP size exceeds console limits")
    zips = [a for a in assets if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*-release\.zip", a["name"])]
    sums = [a for a in assets if a["name"] == "SHA256SUMS.txt"]
    if len(zips) != 1 or len(sums) != 1:
        raise ValueError("Expected the matching installation ZIP and SHA256SUMS.txt")
    return (packages[0], zips[0], sums[0])


def asset_identity(assets):
    return [(a["id"], a["name"], a["size"], a.get("digest")) for a in assets or ()]


def download_assets(tag, assets, directory):
    command = ["gh", "release", "download", tag, "--repo", REPOSITORY, "--dir", str(directory)]
    for asset in assets:
        command += ["--pattern", asset["name"]]
    subprocess.run(command, check=True)


def verify_files(directory, assets, revision, tag):
    package, installation, sums = [directory / a["name"] for a in assets]
    digests = {}
    for line in sums.read_text(encoding="ascii").splitlines():
        digest, name = line.split("  ", 1)
        if not DIGEST.fullmatch(digest) or name in digests:
            raise ValueError("Invalid release checksum list")
        digests[name] = digest
    for asset, path in zip(assets, (package, installation, sums)):
        if path.stat().st_size != asset["size"]:
            raise ValueError("Release asset length changed")
        if path != sums:
            with path.open("rb") as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != digests.get(path.name):
                    raise ValueError("Asset differs from published release checksums: " + path.name)
    report = verify_update_asset(package, installation)
    gui = next((row for row in report["members"] if row["path"] == "runtime.kui"), {})
    if gui.get("build") != revision[:12] or not any(row["path"].startswith("apps/games/") and
                                                      row["path"].endswith(".kui") for row in report["members"]):
        raise ValueError("Release requires its own GUI and the staged Games software")
    with zipfile.ZipFile(installation) as archive:
        record = json.loads(archive.read("build.json"))
        if record.get("kind") != "release" or record.get("commit") != revision or \
                (record.get("release") or {}).get("version") != tag[1:]:
            raise ValueError("Installation ZIP does not identify this stable release and source")
    return digests[package.name]


def source_for_tag(tag):
    # FETCH_HEAD^{commit} peels lightweight and annotated tags. Never execute
    # code from a release checkout; verification runs from trusted main.
    git("fetch", "--no-tags", "origin", "refs/tags/" + tag)
    return git("rev-parse", "FETCH_HEAD^{commit}")


def publish(release, *, expected_tag=None):
    tag = release["tag_name"]
    if expected_tag and expected_tag != tag:
        print("A different release is marked latest; skipped this event.")
        return False
    assets = assets_for(release)
    if not assets:
        print("Latest stable release is ZIP-only. The verified console feed was left in place.")
        return False
    revision = source_for_tag(tag)
    parent = remote_ref(CHANNEL)
    previous = None
    old = None
    if parent:
        fetch_feed(parent)
        old = git("show", parent + ":release.txt").splitlines()
        if len(old) != 6 or old[0] != "KUI-RELEASE-1" or not COMMIT.fullmatch(old[1]):
            raise ValueError("Invalid existing stable feed")
        if version(old[4]) > version(tag):
            raise ValueError("Refusing to roll the stable channel back")
        previous = old[1]
    with tempfile.TemporaryDirectory() as temp:
        download_assets(tag, assets, Path(temp))
        digest = verify_files(Path(temp), assets, revision, tag)
    package = assets[0]
    manifest = "\n".join(["KUI-RELEASE-1", revision, str(package["size"]), digest, tag, package["name"], ""])
    if old and old[4] == tag:
        if old != manifest.splitlines():
            raise ValueError("An already published stable version changed")
        return False
    bullets = []
    for line in (release.get("body") or "").splitlines():
        if line.startswith(("- ", "* ")):
            line = re.sub(r"\[([^]]+)\]\([^)]+\)", r"\1", line[2:])
            bullets.append(line.replace("**", "").replace("`", ""))
    info = update_info(revision, digest, previous, release_notes=bullets)
    current = latest()
    current_assets = assets_for(current)
    if current["id"] != release["id"] or current["tag_name"] != tag or \
            asset_identity(current_assets) != asset_identity(assets) or source_for_tag(tag) != revision:
        raise ValueError("Published release changed during verification; retry")
    write_feed(CHANNEL, parent, {"release.txt": manifest, "update-info.txt": info}, revision)
    return True


if __name__ == "__main__":
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise SystemExit("Expected the upstream K-UI repository")
    publish(latest(), expected_tag=os.environ.get("STABLE_RELEASE_TAG"))
