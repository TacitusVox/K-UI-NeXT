#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Replay each native SD reader's published baseline, then rebuild its temporary VGA stage.

Source .deps/kos/environ.sh first. Outputs include the seven stock-release readers
and the Standard reader's build-time GUI binding.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from runtime_package import envelope, flatten_elf, verify

ROOT = Path(__file__).resolve().parents[1]
BASE = "3f757bc2ac0aa8cbb6e5e104afe8a2df83201c8e"
SOURCES = ("src/loader/retail_stage.S", "src/loader/retail_stage.c")
# Historical build strings and reproducible public source commits can differ.
# Each tuple names the installed file, original makefile, build directory,
# and make target (relative to that directory unless the Makefile.dc target is phony).
GROUPS = (
    ("ec04bb0be564015650d79b61fad85f914ad502b0", "9e3295bcfaec", (
        ("retail-boot", "Makefile.vmu-native-vmu-lite-paced", "native-vmu-lite-paced-sci", "retail-native-vmu-lite-paced.kui"),
        ("retail-native-dual-a1", "Makefile.vmu-native-background-compact-a1", "native-background-compact-a1", "retail-native-dual.kui"))),
    ("e79da8cee44d95cda518721df7a97a83965ee2c3", "f8807cb16561", (
        ("retail-native-dual", "Makefile.vmu-native-background-compact", "native-background-compact", "retail-native-dual.kui"),
        ("retail-physical", "Makefile.dc", "retail-physical", "retail-physical"),
        ("zso-boot", "Makefile.dc", "retail-zso", "native-zso"),
        ("lzo-boot", "Makefile.dc", "retail-lzo", "native-lzo"))),
    ("bcbd183be2f94b56ae257b0b2b3e0175cc39a9c2", "e5af01888606", (
        ("retail-native-standalone", "Makefile.native-standalone-general", "native-standalone-general", "retail-native-standalone.kui"),)),
)


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def resident_files(directory):
    return {p.name: sha(p.read_bytes()) for p in directory.iterdir()
            if p.suffix in (".bin", ".reloc") and p.name != "stage.bin"}


def image_stage_rules(revision):
    # These three release variants come from Makefile.dc. Reapply only their
    # changed rules, leaving unrelated GUI, bench and development rules alone.
    makefile = git("show", revision + ":Makefile.dc").decode()
    begin = makefile.index("\nZSO_STAGE_SRC :=") + 1
    end = makefile.index("\n$(RETAIL_PHYSICAL_DIR)/stage.elf:", begin) + 1
    return makefile[begin:end].splitlines(keepends=True)


def source_update(tree, revision, uses_image_stage_rules):
    headers = git("ls-tree", "-r", "--name-only", revision, "--",
                  "include/kui", "src/loader").decode().splitlines()
    for name in headers:
        if name.startswith("include/kui/game_vga") or name in (
                "src/loader/game_vga_cable.h", "src/loader/native_game_vga_policy.h",
                "src/loader/native_game_vga_stage.inc"):
            (tree / name).write_bytes(git("show", revision + ":" + name))
    # Stage code surrounding the two added calls differs between baselines.
    # The changed lines anchor both joins to the original CRC and relay. Check
    # ordering explicitly, including the call before the cache/CPU restore.
    patch = git("diff", BASE, revision, "--", *SOURCES)
    if uses_image_stage_rules:
        patch += "".join(difflib.unified_diff(image_stage_rules(BASE), image_stage_rules(revision),
                                              "a/Makefile.dc", "b/Makefile.dc")).encode()
    subprocess.run(["git", "apply", "-C0", "-"], input=patch, cwd=tree, check=True)
    stage = (tree / SOURCES[1]).read_text()
    asm = (tree / SOURCES[0]).read_text()
    if ("boot_crc = kui_retail_crc32(0, boot, exec_bytes);\n#if KUI_NATIVE_GAME_VGA_STAGE\n"
        "    native_vga_identify(ip,boot);" not in stage
            or not asm.index("    mov.l .Lrelaymain,r0") < asm.index("    mov.l .Lvgahandoff,r0")
            < asm.index("    or r9,r0")):
        raise ValueError("VGA proof or final handoff outside the audited stage boundary")


def compile_roles(tree, roles, label, jobs, log):
    products = {}
    with log.open("wb") as f:
        for name, recipe, folder, target in roles:
            directory = tree / "build" / folder
            flags = ["BUILD_ID=" + label]
            goal = target if recipe == "Makefile.dc" else "build/" + folder + "/" + target
            subprocess.run(["make", "-j" + str(jobs), "-f", recipe, *flags, goal],
                           cwd=tree, stdout=f, stderr=subprocess.STDOUT, check=True)
            payload, memory = flatten_elf((directory / "entry.elf").read_bytes())
            products[name] = (payload, memory, directory)
    return products


def standard_binding(dest, blob, directory, revision):
    old = json.loads(git("show", BASE + ":resources/updater-bindings/installed-standard-9e3295bcfaec.json"))
    prior_vga = json.loads(git("show", revision + ":resources/updater-bindings/previous-vga-standard.json"))
    preserved = json.loads(git("show", BASE + ":resources/updater-bindings/preserved-standard-manifest.json"))
    info = verify(blob)
    profile = (directory / "vmu-sd-profile.bin").read_bytes()
    payload = blob[64:]
    slot = payload.find(profile)
    if len(profile) != 4096 or slot < 0 or payload.find(profile, slot + 1) != -1:
        raise ValueError("The linked Standard module must have exactly one canonical VMU profile")
    digest = sha(payload[:slot] + bytes(len(profile)) + payload[slot + len(profile):])
    record = dict(old, build=info["build"], module_sha256=sha(blob), module_bytes=len(blob),
                  payload_bytes=len(payload), payload_crc32=info["crc32"],
                  profile_payload_offset=slot, normalized_payload_sha256=digest,
                  source=revision, previous_build=old["build"], prior_vga_build=prior_vga["build"],
                  preserved_build=preserved["build"])
    for key in ["gui", "gui_runtime_sha256", "evidence"]:
        record.pop(key, None)
    (dest / "vga-standard.json").write_text(json.dumps(record, indent=2) + "\n")
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only */",
             "/* Generated by tools/build_vga_sd.py from the matched linked binaries. */",
             "#define KUI_NATIVE_STANDARD_MANIFEST_VERSION 1"]
    for prefix, item, digest_name in [("MANIFEST", record, "manifest"), ("PREVIOUS", old, "previous"),
                                     ("PRIOR_VGA", prior_vga, "prior_vga"),
                                     ("PRESERVED", preserved, "preserved")]:
        lines.extend([f'#define KUI_NATIVE_STANDARD_{prefix}_BUILD "{item["build"]}"',
                      f'#define KUI_NATIVE_STANDARD_{prefix}_PAYLOAD_BYTES {item["payload_bytes"]}u',
                      f'#define KUI_NATIVE_STANDARD_{prefix}_PROFILE_OFFSET {item["profile_payload_offset"]}u',
                      f'static const uint8_t kui_native_standard_{digest_name}_sha256[32]={{'])
        octets = [f"0x{b:02x}" for b in bytes.fromhex(item["normalized_payload_sha256"])]
        lines.extend(["    " + ",".join(octets[:16]) + ",", "    " + ",".join(octets[16:]) + "};"])
    (dest / "vga-standard.h").write_text("\n".join(lines) + "\n")


def qualify(dest, revision, jobs):
    results = {}
    for baseline, label, roles in GROUPS:
        with tempfile.TemporaryDirectory(prefix="sd-vga-") as folder:
            tree = Path(folder) / "source"
            git("worktree", "add", "-q", "--detach", str(tree), baseline)
            try:
                (tree / ".deps").symlink_to((ROOT / ".deps").resolve(), target_is_directory=True)
                before = compile_roles(tree, roles, label, jobs, dest / (label + "-original.log"))
                workers = {}
                originals = {}
                for name, (payload, mem, directory) in before.items():
                    path = "resources/distribution/apps/games/" + name + ".kui"
                    old = git("show", BASE + ":" + path)
                    if envelope(payload, mem, label) != old:
                        raise ValueError("Rebuilt baseline differs from the shipped " + path)
                    originals[name] = sha(old)
                    workers[name] = resident_files(directory)
                source_update(tree, revision, any(recipe == "Makefile.dc" for _, recipe, _, _ in roles))
                after = compile_roles(tree, roles, label, jobs, dest / (label + "-vga.log"))
                for name, (payload, mem, directory) in after.items():
                    if workers[name] != resident_files(directory):
                        raise ValueError("Game-time reader, worker or relocation table changed: " + name)
                    symbols = subprocess.check_output(["sh-elf-nm", str(directory / "stage.elf")])
                    if b" T _kui_native_vga_handoff" not in symbols or b" b _native_vga_profile" not in symbols:
                        raise ValueError("Missing VGA stage implementation: " + name)
                    # Game-time reports retain the baseline worker's build ID.
                    # The launch report identifies the new component by envelope and payload CRC.
                    blob = envelope(payload, mem, revision[:12])
                    path = "release/" + name + ".kui"
                    (dest / path).write_bytes(blob)
                    results[path] = dict(source=baseline, baseline_sha256=originals[name],
                                         sha256=sha(blob), runtime=verify(blob), retained=workers[name])
                    if name == "retail-boot":
                        standard_binding(dest, blob, directory, revision)
            finally:
                git("worktree", "remove", "--force", str(tree))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--revision", default="HEAD")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    dest = args.output.resolve()
    if dest.exists() or args.jobs <= 0 or not shutil.which("sh-elf-gcc"):
        parser.error("use a new output directory, a positive jobs count and the KOS environment")
    revision = git("rev-parse", "--verify", "--end-of-options", args.revision + "^{commit}").decode().strip()
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest.parent) as tmp:
        out = Path(tmp) / "qualified"
        (out / "release").mkdir(parents=True)
        results = qualify(out, revision, args.jobs)
        (out / "qualification.json").write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")
        out.rename(dest)


if __name__ == "__main__":
    main()
