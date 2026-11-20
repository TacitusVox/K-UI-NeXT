# SPDX-License-Identifier: GPL-3.0-only
"""Keep the ten most recent published console nightlies tied to their original bytes."""
from dataclasses import dataclass
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import tempfile

from channel_feed import COMMIT, DIGEST, checked_scope, git, payload_scope, remote_ref, update_info
from update_package import HEADER_BYTES, MAX_ENTRIES, MAX_PAYLOAD, V2_ENTRY_BYTES, verify

REPO = "TacitusVox/K-UI-NeXT"
WINDOW = 10
ASSET = "K-UI.kup"
GUI_ONLY = "partial 1 0 0 1"
LIMIT = HEADER_BYTES + MAX_ENTRIES * V2_ENTRY_BYTES + MAX_PAYLOAD


def contents(commit, name):
    if commit and git("ls-tree", commit, "--", name):
        return subprocess.check_output(["git", "show", commit + ":" + name]).decode("ascii")
    return None


@dataclass(frozen=True)
class Entry:
    commit: str
    size: str
    digest: str

    def __post_init__(self):
        if (not COMMIT.fullmatch(self.commit) or not DIGEST.fullmatch(self.digest) or
                not re.fullmatch(r"[1-9][0-9]{0,8}", self.size) or not HEADER_BYTES <= int(self.size) <= LIMIT):
            raise ValueError("Invalid nightly identity, byte length or checksum")

    @classmethod
    def from_manifest(cls, text):
        magic, commit, size, digest, end = text.split("\n")
        if magic != "KUI-NIGHTLY-1" or end:
            raise ValueError("Invalid nightly manifest")
        return cls(commit, size, digest)

    @property
    def manifest(self):
        return f"KUI-NIGHTLY-1\n{self.commit}\n{self.size}\n{self.digest}\n"


def utc(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z", value):
        raise ValueError("Invalid nightly publication timestamp")
    datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    return value


def parse_index(text, rich=False):
    magic, count, *lines, end = text.split("\n")
    if end or magic != ("KUI-NIGHTLIES-2" if rich else "KUI-NIGHTLIES-1") or count != str(len(lines)) or not 1 <= len(lines) <= WINDOW:
        raise ValueError("Invalid nightly history header")
    rows = []
    seen = set()
    for line in lines:
        fields = line.split(" ", 4)
        if len(fields) != (5 if rich else 4):
            raise ValueError("Invalid nightly history fields")
        commit, size, digest, published = fields[:4]
        if commit in seen or (rows and published > rows[-1][1]):
            raise ValueError("Repeated nightly or incorrect chronological order")
        row = (Entry(commit, size, digest), utc(published))
        rows.append((*row, checked_scope(fields[4])) if rich else row)
        seen.add(commit)
    return rows


def publication(entry, verify_manifest, *, known_scope=None):
    tag = "nightly-" + entry.commit
    proc = subprocess.run(["gh", "api", "repos/" + REPO + "/releases/tags/" + tag], capture_output=True, text=True)
    if proc.returncode:
        if "(HTTP 404)" in proc.stderr:
            return None
        raise RuntimeError(proc.stderr.strip() or "Cannot read published nightly")
    release = json.loads(proc.stdout)
    assets = [a for a in release["assets"] if a["name"] == ASSET and a["state"] == "uploaded"]
    if (release["tag_name"] != tag or release["draft"] or not release["prerelease"] or
            release["target_commitish"] != entry.commit or not release.get("published_at") or
            remote_ref("refs/tags/" + tag) != entry.commit or len(assets) != 1):
        return None
    asset = assets[0]
    if (asset["size"] != int(entry.size) or asset.get("digest") not in (None, "sha256:" + entry.digest) or
            "SHA-256: " + entry.digest not in (release.get("body") or "").splitlines()):
        return None
    # First-time adoption of a legacy entry gets the same real KUP decoder
    # as a new publication. Previously indexed entries can use GitHub's own
    # current asset digest, or re-download if the API does not provide one.
    scope = known_scope
    if not scope or not asset.get("digest"):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run(["gh", "release", "download", tag, "--repo", REPO, "--pattern", ASSET,
                            "--dir", directory], check=True)
            try:
                path = Path(directory) / ASSET
                if verify_manifest(path, entry.commit) != entry.manifest:
                    return None
                scope = payload_scope(verify(path))
                if known_scope and known_scope != scope:
                    return None
            except ValueError as error:
                print(f"Skipping {tag}: {error}")
                return None
    return utc(release["published_at"]), scope


def previous_entries(parent):
    """Use only builds promoted to nightly-channel; an abandoned draft/upload cannot enter the picker."""
    rich = contents(parent, "nightlies-v2.txt")
    index = rich or contents(parent, "nightlies.txt")
    if index is not None:
        for row in parse_index(index, rich=rich is not None):
            entry, date = row[:2]
            root = "history/" + entry.commit + "/"
            info = contents(parent, root + "update-info.txt")
            if contents(parent, root + "nightly.txt") != entry.manifest or not info:
                raise ValueError("Indexed nightly has incomplete archive metadata")
            yield entry, date, info, row[2] if rich else None
    elif parent:
        git("fetch", "--no-tags", "--depth=64", "origin", parent)
        for commit in git("log", "-64", "--format=%H", parent, "--", "nightly.txt").splitlines():
            yield Entry.from_manifest(contents(commit, "nightly.txt")), None, contents(commit, "update-info.txt"), None


def make_index(parent, entry, paths, verify_manifest):
    prior = list(previous_entries(parent))
    rows, files = [], {}
    same = bool(prior and prior[0][0].commit == entry.commit)
    if same and prior[0][0] != entry:
        raise ValueError("Published nightly bytes changed for the same source")
    candidates = prior if same else [(entry, None, None, None), *prior]
    seen = set()
    for n, (current, expected_date, info, known_scope) in enumerate(candidates):
        if len(rows) == WINDOW:
            break
        if current.commit in seen:
            continue
        seen.add(current.commit)
        verified = publication(current, verify_manifest, known_scope=known_scope)
        if not verified or (expected_date and expected_date != verified[0]):
            if n == 0:
                raise ValueError("Published nightly is unavailable or changed")
            print("Omitting unavailable or changed historical release nightly-" + current.commit)
            continue
        date, scope = verified
        if rows and date > rows[0][1]:
            raise ValueError("A newer release has already been promoted to the nightly channel")
        previous = candidates[n+1][0].commit if n+1 < len(candidates) else None
        info = info or contents(parent, "history/" + current.commit + "/update-info.txt")
        rows.append((current, date, info or update_info(current.commit, current.digest, previous,
                                                        paths if scope == GUI_ONLY else ()), scope))
    rows.sort(key=lambda row: row[1], reverse=True)
    index = ["KUI-NIGHTLIES-2", str(len(rows))]
    readme = ["# Recent K-UI nightlies", "", "Published (UTC) | Build and downloads | Bytes | Scope | Replaced | Removed | Kept",
              "--- | --- | --- | --- | --- | --- | ---"]
    for current, published, info, scope in rows:
        prefix = "history/" + current.commit + "/"
        if not info.startswith(f"KUI-UPDATE-INFO-1\n{current.commit}\n{current.digest}\n"):
            raise ValueError("Archived review metadata has a different identity")
        for filename, body in (("nightly.txt", current.manifest), ("update-info.txt", info), ("scope.txt", scope+"\n")):
            old_file = contents(parent, prefix + filename)
            if old_file is not None and old_file != body:
                raise ValueError("Refusing to change archived nightly metadata")
            files[prefix + filename] = body
        index.append(f"{current.commit} {current.size} {current.digest} {published} {scope}")
        url = f"https://github.com/{REPO}/releases/tag/nightly-{current.commit}"
        mode, replaced, removed, kept, _ = scope.split()
        readme.append(f"{published} | [{current.commit[:12]}]({url}) | {current.size} | "
                      f"{mode} | {replaced} | {removed} | {kept}")
    files["nightlies-v2.txt"] = "\n".join(index) + "\n"
    files["README.md"] = "\n".join(readme) + "\n"
    # Old clients accept only a partial GUI-only package. Keep these exact
    # choices and their sidecar even after ten non-GUI packages are published.
    legacy = [(e, d) for e, d, _, scope in rows if scope == GUI_ONLY]
    prior_legacy = contents(parent, "nightlies.txt")
    if prior_legacy:
        legacy_seen = {e.commit for e, _ in legacy}
        for old, date in parse_index(prior_legacy):
            if old.commit not in seen and old.commit not in legacy_seen:
                legacy.append((old, date))
                legacy_seen.add(old.commit)
    legacy = sorted(legacy, key=lambda row: row[1], reverse=True)[:WINDOW]
    if legacy:
        latest = legacy[0][0]
        prefix = "history/" + latest.commit + "/"
        info = files.get(prefix+"update-info.txt") or contents(parent, prefix+"update-info.txt")
        if not info:
            raise ValueError("Missing legacy release notes")
        files["update-info.txt"], files["nightly.txt"] = info, latest.manifest
        legacy_rows = [f"{e.commit} {e.size} {e.digest} {date}" for e, date in legacy]
        files["nightlies.txt"] = "\n".join(["KUI-NIGHTLIES-1", str(len(legacy)), *legacy_rows, ""])
    return files
