# SPDX-License-Identifier: GPL-3.0-only
"""Read the software selected for a nightly from its exact source revision."""
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile

from channel_feed import COMMIT, DIGEST
import publish_stable_feed as stable
from update_package import verify, MAX_ENTRIES, MAX_PAYLOAD, RAW_SOFTWARE, REMOVE, SELF_KUI, software_path_v2

RECIPE = "config/nightly-package.json"
SOFTWARE = "resources/nightly-software"


def blob(revision, path):
    entry = subprocess.check_output(["git", "ls-tree", "-z", revision, "--", path]).decode("ascii")
    if not entry:
        return None
    meta, name = entry.rstrip("\0").split("\t")
    mode, kind, oid = meta.split()
    if mode not in ("100644", "100755") or kind != "blob" or name != path:
        raise ValueError("Nightly input must be a regular tracked file: " + path)
    return oid


def unique_keys(items):
    obj = {}
    for key, value in items:
        if key in obj:
            raise ValueError("Duplicate nightly recipe key: " + key)
        obj[key] = value
    return obj


@dataclass
class Recipe:
    gui: bool
    full: bool
    files: dict
    remove: list
    release_tag: str = ""

    def stage(self, revision, directory):
        if self.full:
            qualified_release(revision, self.release_tag, stage=directory)
            return
        total = 0
        for name, expected in self.files.items():
            oid = blob(revision, SOFTWARE + "/" + name)
            if not oid:
                raise ValueError("Missing reviewed nightly software: " + name)
            size = int(subprocess.check_output(["git", "cat-file", "-s", oid]))
            total += size
            if total > MAX_PAYLOAD:
                raise ValueError("Reviewed nightly software exceeds the package limit")
            data = subprocess.check_output(["git", "cat-file", "blob", oid])
            if len(data) != size or hashlib.sha256(data).hexdigest() != expected:
                raise ValueError("Reviewed nightly software checksum differs: " + name)
            target = Path(directory) / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    def check_members(self, report, revision):
        members = {r["path"]: r for r in report["members"]}
        if self.full:
            expected = {r["path"]: r for r in qualified_release(revision, self.release_tag)}
            if not report["full"] or members != expected:
                raise ValueError("Full nightly differs from the qualified complete release")
            return
        expected = set(self.files) | set(self.remove)
        if self.gui:
            expected.add("runtime.kui")
        if report["full"] != self.full or expected != set(members):
            raise ValueError("Nightly inventory differs from the requested source's recipe")
        if self.gui:
            gui = members["runtime.kui"]
            if gui["policy"] != SELF_KUI or gui["build"] != revision[:12]:
                raise ValueError("Nightly GUI does not match the requested source")
        for name, sha in self.files.items():
            policy = SELF_KUI if name.endswith(".kui") else RAW_SOFTWARE
            if members[name]["policy"] != policy or members[name]["sha256"] != sha:
                raise ValueError("Nightly differs from the reviewed software: " + name)
        if any(members[name]["policy"] != REMOVE for name in self.remove):
            raise ValueError("Nightly removal differs from the reviewed software")


_full_members = {}


def published_release(tag):
    endpoint = "repos/" + stable.REPOSITORY + "/releases/tags/" + tag
    return json.loads(subprocess.check_output(["gh", "api", endpoint]))


def qualified_release(revision, tag, *, stage=None):
    key = (revision, tag)
    if stage is None and key in _full_members:
        return _full_members[key]
    release = published_release(tag)
    assets = stable.assets_for(release)
    if release["tag_name"] != tag or not assets or stable.source_for_tag(tag) != revision:
        raise ValueError("Full nightly needs this commit's published complete release and matching installation ZIP")
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        stable.download_assets(tag, assets, directory)
        stable.verify_files(directory, assets, revision, tag)
        members = verify(directory / assets[0]["name"])["members"]
        # verify_files checked every owned ZIP path, its exact hash, the
        # GUI build and the release record against this source revision.
        if stage is not None:
            with zipfile.ZipFile(directory / assets[1]["name"]) as archive:
                for member in members:
                    path = Path(stage) / member["path"]
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(archive.read("KUI/" + member["path"]))
    current = published_release(tag)
    if (current["id"] != release["id"] or stable.asset_identity(stable.assets_for(current)) !=
            stable.asset_identity(assets) or stable.source_for_tag(tag) != revision):
        raise ValueError("Complete release changed during nightly verification")
    _full_members[key] = members
    return members


def read_recipe(revision):
    if not COMMIT.fullmatch(revision):
        raise ValueError("Invalid nightly source revision")
    oid = blob(revision, RECIPE)
    if oid is None:
        return Recipe(True, False, {}, [])
    if int(subprocess.check_output(["git", "cat-file", "-s", oid])) > 65536:
        raise ValueError("Nightly recipe is oversized")
    data = json.loads(subprocess.check_output(["git", "cat-file", "blob", oid]), object_pairs_hook=unique_keys)
    if isinstance(data, dict) and set(data) == {"full_release"}:
        tag = data["full_release"]
        if not isinstance(tag, str):
            raise ValueError("Expected a published full-release tag")
        stable.version(tag)
        return Recipe(True, True, {}, [], tag)
    if (not isinstance(data, dict) or set(data) != {"gui", "full", "files", "remove"} or
            type(data["gui"]) is not bool or type(data["full"]) is not bool or
            not isinstance(data["files"], dict) or not isinstance(data["remove"], list)):
        raise ValueError("Invalid nightly package recipe")
    if data["full"]:
        raise ValueError("For complete software specify full_release with a qualified tag; use full=false for a partial")
    recipe = Recipe(**data)
    names = [*recipe.files, *recipe.remove]
    if (any(not software_path_v2(n) or n == "runtime.kui" for n in names) or
            any(not isinstance(v, str) or not DIGEST.fullmatch(v) for v in recipe.files.values()) or
            len(set(names)) != len(names) or not 1 <= len(names) + int(recipe.gui) <= MAX_ENTRIES):
        raise ValueError("Invalid nightly software paths, hashes or scope")
    return recipe
