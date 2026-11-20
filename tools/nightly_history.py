# SPDX-License-Identifier: GPL-3.0-only
"""Keep the ten most recent published console nightlies tied to their original bytes."""
from dataclasses import dataclass
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import tempfile

from channel_feed import COMMIT, DIGEST, git, remote_ref, update_info
from update_package import HEADER_BYTES, MAX_ENTRIES, MAX_PAYLOAD, V2_ENTRY_BYTES

REPO = "TacitusVox/K-UI-NeXT"
WINDOW = 10
ASSET = "K-UI.kup"
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


def parse_index(text):
    magic, count, *lines, end = text.split("\n")
    if end or magic != "KUI-NIGHTLIES-1" or count != str(len(lines)) or not 1 <= len(lines) <= WINDOW:
        raise ValueError("Invalid nightly history header")
    rows = []
    seen = set()
    for line in lines:
        commit, size, digest, published = line.split(" ")
        if commit in seen or (rows and published > rows[-1][1]):
            raise ValueError("Repeated nightly or incorrect chronological order")
        rows.append((Entry(commit, size, digest), utc(published)))
        seen.add(commit)
    return rows


def publication(entry, verify_manifest, *, check_bytes):
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
    if check_bytes or not asset.get("digest"):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run(["gh", "release", "download", tag, "--repo", REPO, "--pattern", ASSET,
                            "--dir", directory], check=True)
            try:
                if verify_manifest(Path(directory) / ASSET, entry.commit) != entry.manifest:
                    return None
            except ValueError as error:
                print(f"Skipping {tag}: {error}")
                return None
    return utc(release["published_at"])


def previous_entries(parent):
    """Use only builds promoted to nightly-channel; an abandoned draft/upload cannot enter the picker."""
    index = contents(parent, "nightlies.txt")
    if index is not None:
        for entry, date in parse_index(index):
            root = "history/" + entry.commit + "/"
            info = contents(parent, root + "update-info.txt")
            if contents(parent, root + "nightly.txt") != entry.manifest or not info:
                raise ValueError("Indexed nightly has incomplete archive metadata")
            yield entry, date, info
    elif parent:
        # The legacy branch stores one verified manifest per promotion.
        # This deployment reads prior promotions and confirms the release
        # and exact bytes still exist before adding any to the new picker.
        git("fetch", "--no-tags", "--depth=64", "origin", parent)
        for commit in git("log", "-64", "--format=%H", parent, "--", "nightly.txt").splitlines():
            yield Entry.from_manifest(contents(commit, "nightly.txt")), None, contents(commit, "update-info.txt")


def make_index(parent, entry, paths, verify_manifest):
    prior = list(previous_entries(parent))
    old = contents(parent, "nightly.txt")
    if old and (not prior or prior[0][0].manifest != old):
        raise ValueError("Latest nightly and nightly index disagree")
    rows = []
    files = {}
    seen = {entry.commit}
    same = bool(prior and prior[0][0].commit == entry.commit)
    if same and prior[0][0] != entry:
        raise ValueError("Published nightly bytes changed for the same source")
    date = publication(entry, verify_manifest, check_bytes=same and not prior[0][1])
    if date is None:
        raise ValueError("Published nightly is unavailable or does not match its verified bytes")
    if same and prior[0][1] and prior[0][1] != date:
        raise ValueError("Current nightly release date changed")
    previous = next((row[0].commit for row in prior if row[0].commit != entry.commit), None)
    info = prior[0][2] if same else contents(parent, "history/" + entry.commit + "/update-info.txt")
    rows.append((entry, date, info or update_info(entry.commit, entry.digest, previous, paths)))
    for n, (last, expected_date, info) in enumerate(prior):
        if len(rows) == WINDOW:
            break
        if last.commit in seen:
            continue
        seen.add(last.commit)
        last_date = publication(last, verify_manifest, check_bytes=not expected_date)
        if last_date is None or (expected_date and expected_date != last_date):
            print("Omitting unavailable or changed historical release nightly-" + last.commit)
            continue
        if last_date > date:
            raise ValueError("A newer release has already been promoted to the nightly channel")
        previous = prior[n + 1][0].commit if n + 1 < len(prior) else None
        rows.append((last, last_date, info or update_info(last.commit, last.digest, previous, paths)))
    rows.sort(key=lambda row: row[1], reverse=True)
    index = ["KUI-NIGHTLIES-1", str(len(rows))]
    readme = ["# Recent K-UI GUI nightlies", "", "Published (UTC) | Build and downloads | Bytes",
              "--- | --- | ---"]
    for current, published, info in rows:
        prefix = "history/" + current.commit + "/"
        if not info.startswith(f"KUI-UPDATE-INFO-1\n{current.commit}\n{current.digest}\n"):
            raise ValueError("Archived review metadata has a different identity")
        for filename, body in (("nightly.txt", current.manifest), ("update-info.txt", info)):
            old_file = contents(parent, prefix + filename)
            if old_file is not None and old_file != body:
                raise ValueError("Refusing to change archived nightly metadata")
            files[prefix + filename] = body
        index.append(f"{current.commit} {current.size} {current.digest} {published}")
        url = f"https://github.com/{REPO}/releases/tag/nightly-{current.commit}"
        readme.append(f"{published} | [{current.commit[:12]}]({url}) | {current.size}")
    files["nightlies.txt"] = "\n".join(index) + "\n"
    files["README.md"] = "\n".join(readme) + "\n"
    files["update-info.txt"] = rows[0][2]
    return files
