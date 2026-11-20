# SPDX-License-Identifier: GPL-3.0-only
"""Publish a small atomic Git feed and bounded metadata for console review."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unicodedata

COMMIT = re.compile(r"[0-9a-f]{40}")
DIGEST = re.compile(r"[0-9a-f]{64}")


def git(*args, **kwargs):
    return subprocess.check_output(["git", *args], text=True, **kwargs).strip()


def remote_ref(ref):
    result = git("ls-remote", "--refs", "origin", ref)
    return result.split()[0] if result else ""


def fetch_feed(head):
    if head:
        git("fetch", "--no-tags", "--depth=1", "origin", head)


def write_feed(ref, parent, files, revision):
    """Swap the manifest and its review metadata in a single leased update."""
    fetch_feed(parent)
    with tempfile.TemporaryDirectory() as directory:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / "index"))
        if parent:
            git("read-tree", parent, env=env)
        for name, contents in files.items():
            blob = git("hash-object", "-w", "--stdin", input=contents)
            git("update-index", "--add", "--cacheinfo", "100644", blob, name, env=env)
        tree = git("write-tree", env=env)
        if parent and tree == git("rev-parse", parent + "^{tree}"):
            return
        commit = git("-c", "user.name=github-actions[bot]", "-c",
                     "user.email=41898282+github-actions[bot]@users.noreply.github.com",
                     "commit-tree", tree, *(("-p", parent) if parent else ()),
                     "-m", "Publish " + ref.split("/")[-1] + " " + revision[:12])
    git("push", "--force-with-lease=" + ref + ":" + parent, "origin", commit + ":" + ref)


def note_line(text):
    plain = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    clean = " ".join(re.sub(r"[^ -~]", " ", plain).split())
    return clean if len(clean) <= 76 else clean[:73].rstrip() + "..."


def update_info(revision, digest, previous=None, paths=(), release_notes=()):
    if not COMMIT.fullmatch(revision) or not DIGEST.fullmatch(digest):
        raise ValueError("Invalid update identity")
    # Prior official builds remain recognizable across large batches of commits.
    # --merged also excludes tags on unrelated branches. Tag labels themselves
    # are never interpreted as an ordering or evidence of ancestry.
    # Reserve space for stable releases even after many intervening nightlies.
    releases = git("tag", "--merged", revision, "--sort=-version:refname", "--list", "v[0-9]*").splitlines()
    nightlies = git("tag", "--merged", revision, "--sort=-creatordate", "--list", "nightly-*").splitlines()
    ancestry = []
    seen = {revision[:12]}
    candidates = []
    if previous and COMMIT.fullmatch(previous):
        check = subprocess.run(["git", "merge-base", "--is-ancestor", previous, revision],
                               capture_output=True)
        if check.returncode == 0:
            candidates.append(previous)
        else:
            previous = None
    else:
        previous = None
    for tag in [*releases[:16], *nightlies[:32]]:
        candidates.append(git("rev-parse", "refs/tags/" + tag + "^{commit}"))
    candidates += git("rev-list", "--max-count=80", revision).splitlines()[1:]
    for source in candidates:
        short = source[:12]
        if short not in seen and git("rev-parse", "--short=12", source) == short:
            seen.add(short)
            ancestry.append(short)
            if len(ancestry) == 64:
                break

    notes = []
    span = previous + ".." + revision if previous else revision
    # Keep only changes to the GUI for a GUI package. Stable releases may
    # provide their own player-facing bullet points instead.
    subjects = git("log", "--no-merges", "-12", "--format=%s", span, "--", *paths).splitlines()
    for line in [*release_notes, *subjects]:
        value = note_line(line)
        if value and value not in notes:
            notes.append(value)
            if len(notes) == 4:
                break
    return "\n".join(["KUI-UPDATE-INFO-1", revision, digest, str(len(ancestry)), *ancestry,
                      str(len(notes)), *notes, ""])


def payload_scope(report):
    """Describe explicit actions in an already verified v2 KUP, before console-specific comparisons."""
    from update_package import INHERIT_ROOT, RAW_SOFTWARE, REMOVE, SELF_KUI
    replace = remove = keep = components = 0
    for member in report["members"]:
        policy, path = member["policy"], member["path"]
        if policy == INHERIT_ROOT:
            keep += 1
            continue
        if policy == REMOVE:
            remove += 1
        elif policy in (SELF_KUI, RAW_SOFTWARE):
            replace += 1
        else:
            raise ValueError("Unrecognized KUP member policy")
        if path == "runtime.kui":
            components |= 1
        elif path.startswith(("apps/games/", "apps/disc/")) and path.endswith(".kui"):
            components |= 2
        else:
            components |= 4
    return checked_scope(f"{'full' if report['full'] else 'partial'} {replace} {remove} {keep} {components}")


def checked_scope(text):
    from update_package import MAX_ENTRIES
    mode, *counts = text.split(" ")
    if mode not in ("full", "partial") or len(counts) != 4 or not all(
            re.fullmatch(r"0|[1-9][0-9]{0,2}", n) for n in counts):
        raise ValueError("Invalid package scope")
    replace, remove, keep, mask = map(int, counts)
    if (not 1 <= replace + remove + keep <= MAX_ENTRIES or mask > 7 or bool(mask) != bool(replace + remove) or
            (mode == "full" and (keep or not replace or not mask & 1))):
        raise ValueError("Invalid package action totals")
    return text
