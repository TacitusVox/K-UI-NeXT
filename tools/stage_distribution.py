#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Stage the reviewed Universal Loader software and its matching GUI inputs."""
import argparse
from functools import lru_cache
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

import build_runtime_module_bundle as bundle
from make_loader_probe import fixture_bytes
import pack_theme
from update_package import MAX_ENTRIES, MAX_PAYLOAD, inventory_v2, member_info_v2, software_path_v2

ROOT = Path(__file__).resolve().parents[1]
CONFIG = "config/distribution.json"
SOFTWARE = "resources/distribution"
STANDARD = "apps/games/retail-boot.kui"
STANDARD_BINDING = "resources/updater-bindings/installed-standard-9e3295bcfaec"
SMASH = tuple("apps/games/retail-smash-" + name + ".kui" for name in
              ("menu", "native-menu", "swirl", "vc2"))
SMASH_HELPERS = tuple("apps/games/smash-" + name + "-vmu-helper.bin" for name in ("swirl", "vc2"))
SHA256 = re.compile(r"[0-9a-f]{64}")
GROUPS = (("wwp", "2026-11-21", 4, 0x8cf50000), ("bam4", "2026-11-22", 2, 0x8cf50000))
ARMADA_PRIVATE = "apps/games/ce-probe.kui"
# The publisher can verify historical schema-1 releases without executing a
# Git revision's fixture generator. Its original G3 wire fixture is fixed.
PROBE_SHA256 = "d61f6b4c30026542b0746cffd82e72c90c3f468c355af373b9107da8bfd0553f"
ROUTE_RECORD = "docs/evidence/reader-corrections/delivery.json"
LINK_INPUTS = ("tests/test_native_standard_manifest.c", "src/core/native_vmu_standard.c",
               "src/core/vmu_game_card.c", "src/core/vmu_persist_profile.c", "src/core/runtime_image.c",
               "src/core/retail_image.c", "src/core/hash.c", "src/core/data.c")


def unique_keys(items):
    result = {}
    for name, value in items:
        if name in result:
            raise ValueError("Duplicate distribution key: " + name)
        result[name] = value
    return result


def reader_input_paths(root=ROOT):
    """Return tracked freestanding build inputs, excluding GUI, documents and tests."""
    root = Path(root)
    tracked = set(os.fsdecode(path) for path in subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=root).split(b"\0") if path)
    recipes = sorted(p for p in tracked if p.startswith("Makefile.") and "test" not in p)
    inputs = set(recipes)
    inputs.update(p for p in tracked if p.startswith(("src/bootstage/", "src/loader/")))
    inputs.update({"dependencies.json", "tools/setup_kos.sh", "tools/runtime_package.py",
                   "tools/retail_package.py", "tools/loader_package.py",
                   "tools/rebuild_distribution_wince.py"} & tracked)
    inputs.update(p for p in tracked if p.startswith("tools/patches/"))
    # Reader recipes name their linked common code and generated inputs. Makefile.dc
    # also builds the GUI in its prologue; GUI objects are outside the freestanding build.
    for name in recipes:
        recipe = (root / name).read_text()
        if name == "Makefile.dc":
            _, marker, recipe = recipe.partition("# G3 contains no KOS")
            if not marker:
                raise ValueError("Cannot identify freestanding recipes in Makefile.dc")
        for path in re.findall(r"(?:src|include|tools|third_party|config)/[A-Za-z0-9_./-]+", recipe):
            if path in tracked and not path.startswith("tools/check_"):
                inputs.add(path)

    # Follow real includes in shared C/assembly sources. A new or renamed header
    # changes the fingerprint; a shell-only header or source does not.
    pending = sorted(p for p in inputs if p.endswith((".S", ".c", ".h", ".inc")))
    while pending:
        name = pending.pop()
        body = (root / name).read_text(errors="replace")
        for include in re.findall(r'^\s*(?:#\s*include|\.include)\s*["<]([\w./-]+)[">]', body, re.M):
            for parent in (Path(name).parent, Path("include"), Path(".")):
                candidate = os.path.normpath(parent / include)
                if candidate in tracked:
                    if candidate not in inputs:
                        inputs.add(candidate)
                        pending.append(candidate)
                    break
    return tuple(sorted(inputs))


def reader_digest(root=ROOT):
    digest = hashlib.sha256()
    for name in reader_input_paths(root):
        body = (root / name).read_bytes()
        digest.update(name.encode() + b"\0" + hashlib.sha256(body).digest())
    return digest.hexdigest()


def checked_inventory(root=ROOT):
    config = json.loads((root / CONFIG).read_text(), object_pairs_hook=unique_keys)
    if set(config) != {"schema", "reader_inputs_sha256", "files"} or type(config["schema"]) is not int or config["schema"] != 1:
        raise ValueError("Unknown distribution manifest schema")
    if config["reader_inputs_sha256"] != reader_digest(root):
        raise ValueError("Reader or build inputs changed. Rebuild or requalify the affected modules, "
                         "then update config/distribution.json with their hashes and --print-reader-inputs.")
    files = config["files"]
    if (not isinstance(files, dict) or not 0 < len(files) < MAX_ENTRIES or
            any(not software_path_v2(name) or name == "runtime.kui" or
                not name.endswith((".kui", "-helper.bin")) or
                not isinstance(sha, str) or not SHA256.fullmatch(sha) for name, sha in files.items())):
        raise ValueError("Invalid distribution software paths, hashes or count")
    provisioned = set(inventory_v2(root / SOFTWARE))
    if provisioned != set(files):
        raise ValueError(f"Distribution inventory differs: missing={sorted(set(files)-provisioned)}, "
                         f"unexpected={sorted(provisioned-set(files))}")
    owned = {}
    total = 0
    for name, expected in sorted(files.items()):
        path = root / SOFTWARE / name
        total += path.stat().st_size
        if total > MAX_PAYLOAD:
            raise ValueError("Staged distribution exceeds update payload limit")
        data = path.read_bytes()
        member = member_info_v2(name, data)
        if member["sha256"] != expected:
            raise ValueError("Distribution checksum differs: " + name)
        owned[name] = data
    return owned


def verify_routes(owned, root=ROOT):
    if ARMADA_PRIVATE in owned:
        raise ValueError("Personalized Armada VMU worker cannot be distributed: " + ARMADA_PRIVATE)
    for role in ("probe", "image-probe", "ce-boot", "ce-armada-all-on", "ce-cdda-armada", "ce-cdda-bam4",
                 "ce-cdda-worms", "ce-cdda-wwp", "retail-native-dual", "retail-native-dual-a1",
                 "retail-native-standalone"):
        if "apps/games/" + role + ".kui" not in owned:
            raise ValueError("Missing distribution reader: " + role)
    bindings = json.loads((root / (STANDARD_BINDING + ".json")).read_text())
    known = json.loads((root / ROUTE_RECORD).read_text())["all_member_hash_inventory"]
    # All public reader roles of the previous complete native cohort must
    # still exist. Later readers can have new build IDs and qualified bytes.
    required = {path[4:] for path in known if path.startswith("KUI/apps/")} | {"tools.kui"}
    missing = required - set(owned)
    if missing:
        raise ValueError("Missing public runtime roles: " + ", ".join(sorted(missing)))
    pin = {STANDARD: bindings["module_sha256"]}
    pin.update({name: known["KUI/" + name]["sha256"] for name in (*SMASH, *SMASH_HELPERS)})
    for name, expected in pin.items():
        if name not in owned or hashlib.sha256(owned[name]).hexdigest() != expected:
            raise ValueError("Distribution does not contain the qualified module or helper: " + name)
    verify_kernel_routes(owned, root)


def verify_kernel_routes(owned, root=ROOT):
    for family, date, profile, pool in GROUPS:
        record = json.loads((root / f"docs/evidence/wince-cdda-{family}-kernels-{date}.json").read_text())
        for key, kernel in record["kernels"].items():
            if key == record["reference"]:
                continue
            sha = kernel["kernel_sha256"]
            path = "apps/games/ce-cdda-" + sha[:12] + ".kui"
            expected = struct.pack("<8s6I32s", b"KUICDK01", 1, 64, profile, kernel["kernel_bytes"],
                                   pool, 0, bytes.fromhex(sha))
            if path not in owned or owned[path][64+0x300:64+0x340] != expected:
                raise ValueError("Missing or mismatched WinCE kernel module: " + path)


def verify_linked_gui(runtime, stage):
    runtime, stage = Path(runtime), Path(stage)
    gui = runtime.read_bytes()
    member_info_v2("runtime.kui", gui)
    _, embedded = bundle.locate(gui[64:])
    expected = {name: (stage / name).read_bytes() for name in SMASH}
    if bundle.decode(embedded) != expected:
        raise ValueError("Linked GUI's Smash bundle differs from its external readers")
    binding = json.loads((ROOT / (STANDARD_BINDING + ".json")).read_text())
    digest = bytes.fromhex(binding["normalized_payload_sha256"])
    # The pinned SH compiler emits an inlined memcmp literal pool (adjacent
    # swapped pairs) or the digest intact; inspect the linked GUI as well as
    # exercising its selected recognizer source below.
    paired = b"".join(digest[i+4:i+8] + digest[i:i+4] for i in range(0, 32, 8))
    if (binding["build"].encode() + b"\0" not in gui or
            all(value not in gui for value in (digest, paired))):
        raise ValueError("Linked GUI does not identify its selected Standard VMU binding")
    with tempfile.TemporaryDirectory() as tmp:
        check = Path(tmp) / "check-standard"
        subprocess.run([*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1",
                        "-Iinclude", "-I.deps/fatfs/source",
                        '-DKUI_NATIVE_STANDARD_MANIFEST="' + str(ROOT / (STANDARD_BINDING + ".h")) + '"',
                        *LINK_INPUTS, "-o", str(check)], cwd=ROOT, check=True)
        subprocess.run([str(check), str((stage / STANDARD).resolve())], check=True)


def stage_assets(directory, root=ROOT):
    music = root / "resources/music"
    tracks = json.loads((music / "manifest.json").read_text())["tracks"]
    probe = fixture_bytes()
    if hashlib.sha256(probe).hexdigest() != PROBE_SHA256:
        raise ValueError("G3 wire fixture differs from the distribution's probe contract")
    assets = {"apps/games/probe.dat": probe}
    for name in ("redump.db", "tosec.db"):
        assets[name] = (root / "data/known-dumps" / name).read_bytes()
    for track in tracks:
        ogg = track["ogg"]
        name = ogg["file"]
        path = "apps/music/" + name
        if not re.fullmatch(r"[a-z0-9][a-z0-9._-]*\.ogg", name) or path in assets:
            raise ValueError("Invalid bundled music member: " + name)
        data = (music / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != ogg["sha256"] or len(data) != ogg["bytes"]:
            raise ValueError("Music differs from the release manifest: " + name)
        assets[path] = data
    for path, data in assets.items():
        target = directory / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    pack_theme.stage_all(directory / "assets/themes", managed=True, collection=root / "resources/themes")


@lru_cache(maxsize=16)
def derived_hashes(revision, root=None):
    """Reconstruct schema-1 asset hashes at an exact Git revision using trusted code."""
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("A full Git commit is required for distribution asset hashes")
    repository = Path.cwd() if root is None else Path(root)
    prefixes = ("resources/music", "resources/themes", "data/known-dumps")
    entries = subprocess.check_output(["git", "ls-tree", "-r", "-z", revision, "--", *prefixes], cwd=repository)
    tree = {}
    for entry in entries.split(b"\0"):
        if entry:
            meta, name = os.fsdecode(entry).split("\t", 1)
            tree[name] = meta.split()
    pack_dirs = {Path(name).parent.as_posix() for name in tree
                 if re.fullmatch(r"resources/themes/[a-z0-9][a-z0-9_-]*/(?:theme\.ini|source\.json)", name)}
    asset_path = re.compile(r"(?:data/known-dumps/(?:redump|tosec)\.db|resources/music/"
                            r"(?:manifest\.json|[a-z0-9._-]+\.ogg)|resources/themes/[a-z0-9][a-z0-9_-]*/"
                            r"(?:[a-z0-9][a-z0-9._-]{0,62}(?:\.thumb)?))")
    with tempfile.TemporaryDirectory() as temporary:
        source = Path(temporary) / "source"
        output = Path(temporary) / "KUI"
        total = 0
        for name, (mode, kind, oid) in tree.items():
            if not asset_path.fullmatch(name):
                continue
            if name.startswith("resources/themes/") and Path(name).parent.as_posix() not in pack_dirs:
                continue
            if mode not in ("100644", "100755") or kind != "blob":
                raise ValueError("Expected regular tracked release asset: " + name)
            size = int(subprocess.check_output(["git", "cat-file", "-s", oid], cwd=repository))
            total += size
            if total > MAX_PAYLOAD:
                raise ValueError("Historical distribution assets exceed package limits")
            body = subprocess.check_output(["git", "cat-file", "blob", oid], cwd=repository)
            dest = source / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(body)
        stage_assets(output, source)
        return {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
                for name in inventory_v2(output)}


def empty_output(directory):
    directory = Path(directory).absolute()
    if directory.is_symlink() or (directory.exists() and (not directory.is_dir() or any(directory.iterdir()))):
        raise ValueError("Use an empty output directory: " + str(directory))
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def stage(output, gui_inputs=False):
    members = checked_inventory()
    verify_routes(members)
    directory = empty_output(output)
    if gui_inputs:
        bundle.build({name: members[name] for name in SMASH}, "cc81e63ca4d2", directory)
    else:
        # Build the whole new tree beside its destination. An asset validation
        # failure leaves neither a partial stage nor an old software overlay.
        with tempfile.TemporaryDirectory(dir=directory.parent) as tmp:
            staged = Path(tmp) / "KUI"
            staged.mkdir()
            for name, data in members.items():
                target = staged / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            stage_assets(staged)
            paths = inventory_v2(staged)
            if len(paths) >= MAX_ENTRIES or sum((staged / n).stat().st_size for n in paths) >= MAX_PAYLOAD:
                raise ValueError("Distribution cannot fit an update package with its GUI")
            staged.rename(directory)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    mode = cli.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path, help="Empty directory for validated non-GUI managed software")
    mode.add_argument("--gui-inputs", type=Path, help="Empty RUNTIME_MODULE_BUNDLE directory")
    mode.add_argument("--verify-gui", type=Path, help="Check the packaged GUI against its external readers")
    mode.add_argument("--print-reader-inputs", action="store_true", help="Print the current reader source fingerprint")
    cli.add_argument("--stage", type=Path, help="Non-GUI software directory for --verify-gui")
    args = cli.parse_args()
    if bool(args.verify_gui) != bool(args.stage):
        cli.error("--verify-gui and --stage are used together")
    try:
        if args.print_reader_inputs:
            print(reader_digest())
        elif args.verify_gui:
            verify_linked_gui(args.verify_gui, args.stage)
        else:
            stage(args.gui_inputs or args.output, bool(args.gui_inputs))
    except (KeyError, OSError, ValueError, subprocess.CalledProcessError) as error:
        cli.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
