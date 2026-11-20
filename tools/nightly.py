#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and publish the K-UI testing channel from a clean main checkout."""
import argparse
from channel_feed import fetch_feed, git, remote_ref, write_feed
from nightly_history import ASSET, Entry, contents, make_index
from nightly_recipe import RECIPE, SOFTWARE, read_recipe
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from make_update import make_update
from runtime_package import envelope, flatten_elf
from update_package import CONSUMER_INPUTS, current_consumer_check, REMOVE, verify

CHANNEL = "refs/heads/nightly-channel"

# Only inputs to the GUI, its dependencies and the nightly recipe. In
# particular a new standalone game reader or card theme is not a GUI update.
GUI_INPUTS = (
    "src/apps", "src/core", "src/dreamcast", "include", "config", "third_party",
    "src/loader/*.h", "src/loader/*.inc", "src/loader/sd_reader.c",
    "src/loader/sci_sd_bus.c", "src/loader/smash_fat_read_cache.c",
    "src/loader/vmu_backing.c", "firmware/kui-wifi/components/kwlink",
    "resources/updater-bindings", "resources/branding/startup.png",
    "resources/branding/boot-red.png", "resources/branding/startup-chime.ogg",
    "resources/themes/moonlit-muramasa/splash.kuiimg", "dependencies.json",
    "Makefile.dc", "tools/setup_kos.sh", "tools/fetch_deps.py", "tools/fetch_media_xiph.py",
    "tools/runtime_module_bundle_config.py", "tools/patches", "tools/build_splash.py",
    "tools/pack_theme.py", "tools/record_build.py", "tools/runtime_package.py",
    "tools/make_update.py", "tools/update_package.py", "tools/update_consumer_check.c",
    "tools/nightly.py", "tools/nightly_history.py", "tools/nightly_recipe.py", "tools/channel_feed.py",
    ".github/workflows/nightly.yml",
)


def input_paths(revision):
    recipe = read_recipe(revision)
    producer = ("tools/nightly*.py", "tools/channel_feed.py", "tools/make_update.py",
                "tools/update_package.py", "tools/runtime_package.py", "tools/update_consumer_check.c",
                ".github/workflows/nightly.yml", "dependencies.json", *CONSUMER_INPUTS)
    files = tuple(SOFTWARE + "/" + name for name in sorted(recipe.files))
    return (*(GUI_INPUTS if recipe.gui and not recipe.full else producer), RECIPE, *files)


def source_commit():
    sha = git("rev-parse", "HEAD")
    if not re.fullmatch("[0-9a-f]{40}", sha) or sha != os.environ["GITHUB_SHA"]:
        raise ValueError("Checkout does not match the requested source commit")
    return sha


def fingerprint():
    revision = source_commit()
    entries = subprocess.check_output(["git", "ls-files", "--stage", "-z", "--", *input_paths(revision)])
    return hashlib.sha256(entries).hexdigest()


def select():
    revision = source_commit()
    channel = remote_ref(CHANNEL)
    inputs = fingerprint()
    changed = True
    if channel:
        git("fetch", "--no-tags", "--depth=1", "origin", channel)
        changed = git("show", channel + ":inputs.sha256") != inputs
    # Rerunning an old Actions run must never move the feed backwards.
    current = remote_ref("refs/heads/main") == revision
    build = changed and current
    migrate = current and not changed and contents(channel, "nightlies-v2.txt") is None
    recipe = read_recipe(revision)
    with open(os.environ["GITHUB_OUTPUT"], "a") as out:
        print(f"gui={str(recipe.gui and not recipe.full).lower()}\nbuild={str(build).lower()}\nmigrate={str(migrate).lower()}\nchannel={channel}\ninputs={inputs}", file=out)
    print("Indexing existing nightly releases." if migrate else
          "Software inputs changed; building nightly." if build else
          "Skipped: matching software inputs already published or a newer main commit exists.")


def build_gui(revision, stage):
    elf = Path("build/kui-runtime.elf")
    data = elf.read_bytes()
    record = json.loads(elf.with_suffix(".compile.json").read_text())
    if (record["source_dirty"] or record["commit"] != revision or
            record["elf_sha256"] != hashlib.sha256(data).hexdigest()):
        raise ValueError("Native link record does not match this clean source and ELF")
    payload, memory = flatten_elf(data)
    (Path(stage) / "runtime.kui").write_bytes(envelope(payload, memory, revision[:12]))


def package(output):
    revision = source_commit()
    recipe = read_recipe(revision)
    with tempfile.TemporaryDirectory() as stage:
        recipe.stage(revision, stage)
        if recipe.gui and not recipe.full:
            build_gui(revision, stage)
        report = make_update(Path(stage), output, channel="testing", full=recipe.full, remove=recipe.remove)
        recipe.check_members(report, revision)


def manifest(path, revision):
    report = verify(path)
    if report["format"] != 2 or report["channel"] != "testing":
        raise ValueError("Expected the requested source's current-format testing package")
    gui = next((row for row in report["members"] if row["path"] == "runtime.kui"), None)
    if gui and gui["build"] != revision[:12]:
        raise ValueError("Nightly GUI does not match the requested source")
    read_recipe(revision).check_members(report, revision)
    current_consumer_check(path)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return f"KUI-NIGHTLY-1\n{revision}\n{path.stat().st_size}\n{digest}\n"


def release(revision, asset, directory, checked):
    tag = "nightly-" + revision
    target = remote_ref("refs/tags/" + tag)
    if target and target != revision:
        raise ValueError("Nightly tag already points to another commit")
    report = verify(asset)
    scope = ("Complete software package. Managed software omitted from this version becomes unavailable."
             if report["full"] else "Partial software update. Other installed software is kept.")
    changes = ["- " + ("Remove" if row["policy"] == REMOVE else "Install") + " `" + row["path"] + "`"
               for row in report["members"]]
    notes = "\n".join([scope, "", *changes, "", "SHA-256: " + checked.splitlines()[3]])
    existing = subprocess.run(["gh", "release", "view", tag, "--json",
                               "tagName,targetCommitish,isDraft,isPrerelease,body"],
                              text=True, capture_output=True)
    if existing.returncode:
        if existing.stderr.strip() != "release not found":
            raise RuntimeError(existing.stderr.strip())
        subprocess.run(["gh", "release", "create", tag, "--target", revision, "--draft",
                        "--prerelease", "--latest=false", "--title", f"K-UI nightly {revision[:12]}",
                        "--notes", notes], check=True)
        draft = True
    else:
        info = json.loads(existing.stdout)
        if info["tagName"] != tag or info["targetCommitish"] != revision or not info["isPrerelease"]:
            raise ValueError("Existing release does not match the nightly source")
        draft = info["isDraft"]

    if draft:
        # A failed upload may leave a draft or a partial asset. Only drafts
        # may be repaired. Published release assets are never replaced.
        subprocess.run(["gh", "release", "upload", tag, str(asset), "--clobber"], check=True)
        subprocess.run(["gh", "release", "edit", tag, "--draft=false", "--latest=false", "--notes", notes], check=True)
    if remote_ref("refs/tags/" + tag) != revision:
        raise ValueError("Published tag does not point to the requested source")
    published = directory / ASSET
    subprocess.run(["gh", "release", "download", tag, "--pattern", ASSET,
                    "--dir", str(directory)], check=True)
    published_body = manifest(published, revision)
    if draft:
        matches = published_body == checked
    else:
        matches = "SHA-256: " + published_body.splitlines()[3] in info["body"].splitlines()
    if not matches:
        raise ValueError("Downloaded asset differs from the verified nightly")
    return published_body


def advance_channel(parent, inputs, body, revision):
    entry = Entry.from_manifest(body)
    if entry.commit != revision:
        raise ValueError("Nightly manifest and publication target disagree")
    fetch_feed(parent)
    files = make_index(parent, entry, input_paths(revision), manifest)
    # Keep latest-only GUIs and every review/selection in sync in one commit.
    files["inputs.sha256"] = inputs + "\n"
    write_feed(CHANNEL, parent, files, revision)


def migrate():
    source = source_commit()
    parent, inputs = os.environ["NIGHTLY_CHANNEL_BASE"], os.environ["NIGHTLY_INPUTS"]
    if not parent or parent != remote_ref(CHANNEL) or source != remote_ref("refs/heads/main"):
        raise ValueError("Source or channel changed before nightly history migration")
    fetch_feed(parent)
    if contents(parent, "inputs.sha256") != inputs + "\n" or inputs != fingerprint():
        raise ValueError("Migration requires matching software inputs")
    if contents(parent, "nightlies-v2.txt") is None:
        body = contents(parent, "nightly.txt")
        advance_channel(parent, inputs, body, Entry.from_manifest(body).commit)


def publish(asset):
    revision = source_commit()
    expected, inputs = os.environ["NIGHTLY_CHANNEL_BASE"], os.environ["NIGHTLY_INPUTS"]
    if inputs != fingerprint() or expected != remote_ref(CHANNEL):
        raise ValueError("Nightly inputs or channel head changed since selection")
    if remote_ref("refs/heads/main") != revision:
        print("A newer main commit is queued for the next nightly; leaving the feed unchanged.")
        return
    checked = manifest(asset, revision)
    with tempfile.TemporaryDirectory() as directory:
        # On a retry a published asset can predate the failed feed update.
        # Validate those exact bytes with the current console parser and point
        # the manifest at them, even if a subsequent native link differs.
        body = release(revision, asset, Path(directory), checked)
    if remote_ref("refs/heads/main") != revision:
        print("Main changed during the release upload; keeping the previous feed.")
        return
    advance_channel(expected, inputs, body, revision)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("select")
    commands.add_parser("migrate")
    for name in ("package", "publish"):
        commands.add_parser(name).add_argument("asset", type=Path)
    args = parser.parse_args()
    if args.command == "select":
        select()
    elif args.command == "migrate":
        migrate()
    elif args.command == "package":
        package(args.asset)
    else:
        publish(args.asset)
