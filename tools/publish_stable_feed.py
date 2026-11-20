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

from channel_feed import DIGEST, checked_scope, fetch_feed, git, payload_scope, remote_ref, update_info, write_feed
from nightly_history import WINDOW, contents, utc
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
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if asset.get("digest") not in (None, "sha256:" + digest) or (path != sums and digest != digests.get(path.name)):
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
    return digests[package.name], payload_scope(report)


def source_for_tag(tag):
    # FETCH_HEAD^{commit} peels lightweight and annotated tags. Never execute
    # code from a release checkout; verification runs from trusted main.
    git("fetch", "--no-tags", "origin", "refs/tags/" + tag)
    return git("rev-parse", "FETCH_HEAD^{commit}")


def stable_releases():
    # The API sorts by creation time; sort by actual publication below. Filter
    # nightlies on the runner before retaining the (small) stable inventory.
    result = subprocess.check_output(["gh", "api", "--paginate", "repos/" + REPOSITORY + "/releases?per_page=100",
                                      "--jq", '.[] | select(.draft == false and .prerelease == false) | @json'],
                                     text=True)
    return [json.loads(line) for line in result.splitlines() if line]


def release_notes(release):
    bullets = []
    for line in (release.get("body") or "").splitlines():
        if line.startswith(("- ", "* ")):
            line = re.sub(r"\[([^]]+)\]\([^)]+\)", r"\1", line[2:])
            bullets.append(line.replace("**", "").replace("`", ""))
    return bullets


def verified_release(release, assets, parent, previous):
    tag = release["tag_name"]
    date = utc(release["published_at"])
    revision = source_for_tag(tag)
    prefix = "history/" + tag + "/"
    package = assets[0]
    evidence = json.dumps([release["id"], revision, date, asset_identity(assets)], separators=(",", ":")) + "\n"
    archived = contents(parent, prefix + "release.txt")
    scope_record = contents(parent, prefix + "scope.txt")
    if (archived and scope_record and contents(parent, prefix + "assets.json") == evidence and
            all(a.get("digest") for a in assets)):
        digest = archived.splitlines()[3]
        if package["digest"] != "sha256:" + digest:
            raise ValueError("Archived release digest differs from the published KUP")
        scope = checked_scope(scope_record.rstrip("\n"))
    else:
        with tempfile.TemporaryDirectory() as temp:
            download_assets(tag, assets, Path(temp))
            digest, scope = verify_files(Path(temp), assets, revision, tag)
    manifest = "\n".join(["KUI-RELEASE-1", revision, str(package["size"]), digest, tag, package["name"], ""])
    if archived and archived != manifest:
        raise ValueError("An already published stable version changed")
    if scope_record and scope_record != scope + "\n":
        raise ValueError("Archived package scope changed")
    info = contents(parent, prefix+"update-info.txt")
    legacy = contents(parent, "release.txt")
    if not info and legacy == manifest:
        info = contents(parent, "update-info.txt")
    info = info or update_info(revision, digest, previous, release_notes=release_notes(release))
    if not info.startswith(f"KUI-UPDATE-INFO-1\n{revision}\n{digest}\n"):
        raise ValueError("Stable release notes refer to a different package")
    files = {prefix+"release.txt": manifest, prefix+"scope.txt": scope+"\n",
             prefix+"update-info.txt": info, prefix+"assets.json": evidence}
    row = " ".join([revision, str(package["size"]), digest, date, tag, package["name"], scope])
    return row, manifest, info, files


def candidates(releases):
    accepted = []
    for release in releases:
        if release["draft"] or release["prerelease"] or not release.get("published_at"):
            continue
        try:
            assets = assets_for(release)
            if assets:
                accepted.append((release, assets, utc(release["published_at"]), version(release["tag_name"])))
        except ValueError as error:
            print(f"Skipping {release['tag_name']}: {error}")
    return sorted(accepted, key=lambda r: (r[2], r[3], r[0]["id"]), reverse=True)


def catalog_stamp(records):
    return [(r["id"], r["tag_name"], date, asset_identity(assets)) for r, assets, date, _ in records]


def publish(release, *, expected_tag=None):
    parent = remote_ref(CHANNEL)
    fetch_feed(parent)
    legacy = contents(parent, "release.txt")
    old = legacy.splitlines() if legacy else None
    records = candidates(stable_releases())
    approved = {r["tag_name"]: assets for r, assets, _, _ in records}
    latest_assets = assets_for(release)
    latest_tag = release["tag_name"] if latest_assets else None
    if latest_assets and asset_identity(approved.get(latest_tag)) != asset_identity(latest_assets):
        raise ValueError("Latest release changed during enumeration")
    if expected_tag and expected_tag not in approved:
        print("Published event has no compatible stable KUP.")
        return False
    if old and latest_tag and version(old[4]) > version(latest_tag):
        raise ValueError("Refusing to roll the stable channel back")
    files, rows, valid = {}, [], {}
    previous = old[1] if old else None
    # A pinned GitHub latest may be older than the ten newest publications.
    # Retain that separate compatible alias for older clients as well.
    for candidate, assets, _, _ in records:
        tag = candidate["tag_name"]
        if (len(rows) == WINDOW and tag != latest_tag) or tag in valid:
            continue
        try:
            row, receipt, info, archive = verified_release(candidate, assets, parent, previous)
        except (ValueError, KeyError, SystemExit, zipfile.BadZipFile) as error:
            if tag == latest_tag or tag == expected_tag:
                raise
            print(f"Omitting ineligible historical release {tag}: {error}")
            continue
        if len(rows) < WINDOW:
            rows.append(row)
        files.update(archive)
        valid[tag] = (receipt, info)
    if not rows:
        print("No verified stable KUP exists. Installation ZIPs remain available on GitHub.")
        return False
    alias = latest_tag or (old[4] if old and old[4] in valid else rows[0].split()[4])
    receipt, info = valid[alias]
    if old and old[4] == alias and legacy != receipt:
        raise ValueError("An already published stable version changed")
    if old and version(alias) < version(old[4]):
        raise ValueError("Refusing to roll the stable channel back")
    files.update({"release.txt": receipt, "update-info.txt": info,
                  "releases.txt": "\n".join(["KUI-RELEASES-1", str(len(rows)), *rows, ""])})
    readme = ["# Recent K-UI stable releases", "", "Published (UTC) | Release and downloads | Bytes",
              "--- | --- | ---"]
    for row in rows:
        _, size, _, date, tag, *_ = row.split()
        readme.append(f"{date} | [{tag}](https://github.com/{REPOSITORY}/releases/tag/{tag}) | {size}")
    files["README.md"] = "\n".join(readme) + "\n"
    current = latest()
    if (current["id"] != release["id"] or current["tag_name"] != release["tag_name"] or
            asset_identity(assets_for(current)) != asset_identity(latest_assets) or
            catalog_stamp(candidates(stable_releases())) != catalog_stamp(records) or
            any(source_for_tag(tag) != item[0].splitlines()[1] for tag, item in valid.items())):
        raise ValueError("Published releases changed during verification; retry")
    first = receipt.splitlines()[1]
    if parent and all(contents(parent, name) == text for name, text in files.items()):
        return False
    write_feed(CHANNEL, parent, files, first)
    return True


if __name__ == "__main__":
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise SystemExit("Expected the upstream K-UI repository")
    publish(latest(), expected_tag=os.environ.get("STABLE_RELEASE_TAG"))
