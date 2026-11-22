#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rebuild the distributed generic and CD-music WinCE readers from a committed source revision."""
import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from retail_package import inspect_retail
from runtime_package import envelope, flatten_elf, verify
from vmu_persist_profile import template_profile

ROOT = Path(__file__).resolve().parents[1]
GROUP_RECORDS = {
    "wwp": "docs/evidence/wince-cdda-wwp-kernels-2026-11-21.json",
    "bam4": "docs/evidence/wince-cdda-bam4-kernels-2026-11-22.json",
}


@dataclass(frozen=True)
class Recipe:
    name: str
    makefile: str
    target: str
    options: tuple = ()


def recipes(source):
    generic = Recipe("ce-boot.kui", "Makefile.dc", "build/retail-ce/entry.elf",
                     ("KUI_RETAIL_CE_ASYNC_SLOTS=96", "KUI_RETAIL_CE_MAP_RECORD=1",
                      "KUI_CE_VRAM_DMA=1", "KUI_CE_SLOW_WINDOW=16", "KUI_CE_SLOW_ADAPT=2",
                      "KUI_CE_POOL_KIB=0", "KUI_CE_READAHEAD=0"))
    roles = {generic.name: generic}
    for family in ("armada", "worms", "wwp", "bam4"):
        name = f"ce-cdda-{family}.kui"
        roles[name] = Recipe(name, f"Makefile.{family}-stream", f"build/{family}-ce-stream/entry.elf")
    name = "ce-armada-all-on.kui"
    roles[name] = Recipe(name, "Makefile.armada-cdda-vmu", "build/armada-ce-cdda-vmu/entry.elf")
    for family, filename in GROUP_RECORDS.items():
        record = json.loads((source / filename).read_text())
        for key, kernel in record["kernels"].items():
            if key == record["reference"]:
                continue
            sha = kernel["kernel_sha256"]
            if not re.fullmatch(r"[A-Z][A-Z0-9_]*", key) or not re.fullmatch(r"[a-f0-9]{64}", sha):
                raise ValueError(f"Invalid kernel identity in {filename}")
            name = f"ce-cdda-{sha[:12]}.kui"
            if name in roles:
                raise ValueError(f"Duplicate WinCE module name {name}")
            directory = f"build/{family}-ce-stream-{key.lower().replace('_', '-')}"
            roles[name] = Recipe(name, f"Makefile.{family}-stream", f"{directory}/entry.elf",
                                 (f"KUI_WINCE_CDDA_{family.upper()}_KERNEL={key}",))
    return roles


def compile_role(source, recipe, build_id, jobs, log_path, environment):
    # Fresh temporary source trees have no inherited flags/objects. -B also
    # forces complete recompilation when relabeling a changed candidate.
    cmd = ["make", "-B", f"-j{jobs}", "-f", recipe.makefile, recipe.target,
           f"BUILD_ID={build_id}", *recipe.options]
    with log_path.open("w") as log:
        result = subprocess.run(cmd, cwd=source, env=environment, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        tail = log_path.read_text(errors="replace").splitlines()[-12:]
        raise RuntimeError(f"{recipe.name} failed its linked build\n" + "\n".join(tail))
    payload, memory = flatten_elf((source / recipe.target).read_bytes())
    output = envelope(payload, memory, build_id)
    inspect_retail(output, ce=True)
    if recipe.name == "ce-armada-all-on.kui" and output.count(template_profile()) != 1:
        raise ValueError("Armada all-on requires the canonical public VMU placeholder")
    return output


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="new directory outside the source tree")
    parser.add_argument("--revision", default="HEAD", help="commit containing the source to build")
    parser.add_argument("--only", action="append", metavar="FILENAME", help="repeat to restrict the build")
    parser.add_argument("--candidate", action="store_true",
                        help="emit changed modules with the source commit's build ID for qualification")
    parser.add_argument("--jobs", type=int, default=4, help="make jobs per module")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    commit = git("rev-parse", "--verify", "--end-of-options", args.revision + "^{commit}")
    output = args.output.resolve()
    if output.exists() or output.is_relative_to(ROOT):
        parser.error("use a new output directory outside the repository")
    if not os.environ.get("KOS_BASE"):
        parser.error("source the pinned KallistiOS environment before building")
    output.parent.mkdir(parents=True, exist_ok=True)
    baseline = ROOT / "resources/distribution/apps/games"
    pins = json.loads((ROOT / "config/distribution.json").read_text())["files"]
    environment = {
        name: value for name, value in os.environ.items()
        if not name.startswith(("KUI_", "ARMADA_"))
        and name not in {"MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "GNUMAKEFLAGS", "SD_PROFILE", "BUILD_ID"}
    }
    with tempfile.TemporaryDirectory(prefix="kui-wince-rebuild-") as tmp, tempfile.TemporaryDirectory(
            prefix=".kui-wince-output-", dir=output.parent) as staging:
        source = Path(tmp) / "source"
        dest = Path(staging) / "results"
        dest.mkdir()
        git("worktree", "add", "--quiet", "--detach", str(source), commit)
        try:
            if (ROOT / ".deps").is_dir():
                (source / ".deps").symlink_to((ROOT / ".deps").resolve(), target_is_directory=True)
            roles = recipes(source)
            expected = {n.rsplit("/", 1)[-1] for n in pins
                        if n.startswith("apps/games/ce-cdda-") or n in {"apps/games/ce-boot.kui", "apps/games/ce-armada-all-on.kui"}}
            if expected - roles.keys() or (not args.candidate and expected != roles.keys()):
                raise ValueError("Recorded kernel families and the shipped WinCE reader inventory differ")
            names = sorted(set(args.only) if args.only else roles.keys())
            if set(names) - roles.keys():
                raise ValueError("Unknown module requested: " + ", ".join(sorted(set(names) - roles.keys())))
            report = {"source_commit": commit, "candidate": args.candidate, "modules": {}}
            for name in names:
                recipe = roles[name]
                pin = pins.get("apps/games/" + name)
                old = (baseline / name).read_bytes() if pin else None
                if old is not None and hashlib.sha256(old).hexdigest() != pin:
                    raise ValueError("Shipped file differs from the release lock: " + name)
                old_id = verify(old)["build"] if old is not None else None
                log_path = dest / (name + ".log")
                rebuilt = compile_role(source, recipe, old_id or commit[:12], args.jobs, log_path, environment)
                changed = rebuilt != old
                if changed and not args.candidate:
                    raise ValueError(f"{name} differs from its shipped bytes; run --candidate to qualify it")
                if changed and old_id and old_id != commit[:12]:
                    rebuilt = compile_role(source, recipe, commit[:12], args.jobs, log_path, environment)
                (dest / name).write_bytes(rebuilt)
                report["modules"][name] = dict(sha256=hashlib.sha256(rebuilt).hexdigest(),
                                               prior_sha256=pin, bytes=len(rebuilt), **verify(rebuilt))
                print(f"{'CANDIDATE' if changed else 'IDENTICAL'} {name}", flush=True)
            (dest / "rebuild.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
            dest.rename(output)
        finally:
            git("worktree", "remove", "--force", str(source))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
