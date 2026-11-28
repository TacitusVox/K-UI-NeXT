#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rebuild the three installed GD-Play modules with only the VGA stage change.

Source .deps/kos/environ.sh first. The output directory must be new.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from runtime_package import verify

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_BASE = "3f757bc2ac0aa8cbb6e5e104afe8a2df83201c8e"
PLAIN = "eeed38bd540e7a1085ca53aabf832e5d7a835efa"
VMU = "ec04bb0be564015650d79b61fad85f914ad502b0"
ROLES = (
    ("native-disc.kui", "Makefile.disc-native", PLAIN, "disc_stage.c"),
    ("native-disc-vmu.kui", "Makefile.disc-vmu", VMU, "disc_vmu_stage.c"),
    ("native-disc-vmu-a1.kui", "Makefile.disc-vmu-a1", VMU, "disc_vmu_stage.c"),
)
RESIDENT = ("native-worker.bin", "native-worker.reloc", "support/support.bin", "support/support.reloc")


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(tree, makefile, build_id, directory, log, jobs, worker=None):
    targets = ["all", "audit"] if makefile == "Makefile.disc-native" else ["disc-vmu"]
    frozen = []
    if worker is not None:
        # The VMU worker carries its own HUD build label. Reuse the exact
        # verified worker and relocation table in the new, independently linked stage.
        (tree / directory).mkdir(parents=True)
        # Linked stack qualification also consumes the original worker's emitted call graphs.
        for graph in (worker / "native").rglob("*.ci"):
            target = tree / directory / graph.relative_to(worker)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(graph, target)
        for name in ("native-worker.elf", "native-worker.bin", "native-worker.reloc"):
            shutil.copyfile(worker / name, tree / directory / name)
            frozen.append("--old-file=" + directory + "/" + name)
    with log.open("wb") as f:
        run = subprocess.run(["make", "-f", makefile, f"-j{jobs}", *frozen, "BUILD=" + directory,
                              "BUILD_ID=" + build_id, *targets], cwd=tree, stdout=f, stderr=subprocess.STDOUT)
    if run.returncode:
        raise ValueError("GD-Play build failed:\n" + "\n".join(log.read_text().splitlines()[-20:]))
    return tree / directory


def qualify(revision, dest, jobs):
    headers = git("ls-tree", "-r", "--name-only", revision, "--", "include/kui").decode().splitlines()
    headers = ["include/kui/game_vga.h", "src/loader/game_vga_cable.h"] + [
        name for name in headers if name.startswith("include/kui/game_vga_") and name.endswith(".inc")]
    results = {}
    for filename, makefile, baseline, stage in ROLES:
        source = "src/loader/" + stage
        original = git("show", INTEGRATION_BASE + ":resources/distribution/apps/disc/" + filename)
        with tempfile.TemporaryDirectory(prefix="kui-vga-disc-") as temp:
            tree = Path(temp) / "source"
            git("worktree", "add", "-q", "--detach", str(tree), baseline)
            try:
                (tree / ".deps").symlink_to((ROOT / ".deps").resolve(), target_is_directory=True)
                log = dest / (filename + ".log")
                # Compile the integration-base stage against the qualified worker sources.
                # This proves that the stage we are replacing produces the installed module.
                (tree / source).write_bytes(git("show", INTEGRATION_BASE + ":" + source))
                prior = build(tree, makefile, verify(original)["build"], "build/original", log, jobs)
                if (prior / filename).read_bytes() != original:
                    raise ValueError("Original build differs from installed module: " + filename)
                (tree / source).write_bytes(git("show", revision + ":" + source))
                for header in headers:
                    (tree / header).write_bytes(git("show", revision + ":" + header))
                candidate = build(tree, makefile, revision[:12], "build/vga", log, jobs,
                                  worker=prior if baseline == VMU else None)
                data = (candidate / filename).read_bytes()
                info = verify(data)
                if info["build"] != revision[:12] or data[64:].count(b"T3101N\0") != 1:
                    raise ValueError("Missing VGA stage catalogue or incorrect build identity: " + filename)
                saved = {}
                if baseline == VMU:
                    for name in RESIDENT:
                        blob = (prior / name).read_bytes()
                        if ((candidate / name).read_bytes() != blob or
                                original.count(blob) != 1 or data.count(blob) != 1):
                            raise ValueError(f"{filename} failed resident preservation: {name}")
                        saved[name] = sha(blob)
                (dest / filename).write_bytes(data)
                results[filename] = dict(original_sha256=sha(original), source_base=baseline,
                                         sha256=sha(data), runtime=info, resident_sha256=saved)
            finally:
                git("worktree", "remove", "--force", str(tree))
    return dict(source=revision, integration_base=INTEGRATION_BASE, modules=results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--revision", default="HEAD", help="Committed VGA code and patch catalogue")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    out = args.output.resolve()
    if args.jobs < 1 or out.exists() or shutil.which("sh-elf-gcc") is None:
        parser.error("source .deps/kos/environ.sh, use a new output directory and positive job count")
    revision = git("rev-parse", "--verify", "--end-of-options", args.revision + "^{commit}").decode().strip()
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out.parent, prefix=".gd-play-build-") as tmp:
        dest = Path(tmp) / "qualified"
        dest.mkdir()
        result = qualify(revision, dest, args.jobs)
        (dest / "qualification.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        dest.rename(out)


if __name__ == "__main__":
    main()
