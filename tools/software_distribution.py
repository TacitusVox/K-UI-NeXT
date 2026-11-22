#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the same managed software for the Dreamcast updater and a physical SD card."""
import argparse
import gzip
import hashlib
from io import BytesIO
import json
from pathlib import Path
import shutil
import tempfile
import zipfile

from channel_feed import COMMIT
from make_update import make_update
from update_package import ENGINE_CURRENT, RAW_SOFTWARE, SELF_KUI, current_consumer_check, verify

ROOT = Path(__file__).resolve().parents[1]
BOOT_FILE = ROOT / "resources/boot-disc/v3-0e4ab5a.cdi.gz"
BOOT_PATH = "boot-cd/kui-bootloader-v3.cdi"
BOOT_BYTES = 156085481
BOOT_SHA = "9cc3b808fef4a2bdc7f8c446d3a21ab34ce9ed127a12014ef25b89eddb5a1b0a"


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def auxiliary_files():
    return {p.relative_to(ROOT).as_posix(): p for p in
            [ROOT / "LICENSE", ROOT / "THIRD_PARTY.md", *sorted((ROOT / "LICENSES").glob("*"))]
            if p.is_file()}


def require_software(report, revision):
    if not COMMIT.fullmatch(revision) or report["format"] != 2 or not report["full"] or \
            report["channel"] != "testing" or report["engine_minimum"] != ENGINE_CURRENT:
        raise ValueError("A full, current-engine testing package and exact source commit are required")
    members = {member["path"]: member for member in report["members"]}
    if any(row["policy"] not in (SELF_KUI, RAW_SOFTWARE) for row in members.values()):
        raise ValueError("A self-contained distribution must supply all its managed files")
    if members["runtime.kui"]["build"] != revision[:12]:
        raise ValueError("GUI build differs from distribution source")
    if not any(p.startswith("apps/games/") and p.endswith(".kui") for p in members):
        raise ValueError("Missing Games software")
    return members


def install_guide(label, revision):
    return f"""# {label}

For an SD card installation, power down and merge the supplied KUI folder into
the card root. Replace the supplied files. Keep your games, virtual memory cards,
and preferences. Cold boot with your working independent K-UI boot disc.
For a new disc, burn boot-cd/kui-bootloader-v3.cdi as a disc image.

For a console with the current software updater, install the separate full
testing KUP in Settings → Updates, then choose Restart K-UI to use its bank.
After a manual copy, a previously selected bank can still start from Restart
K-UI. To use this build on both cold boot and Restart, also install the matching
full KUP from the new GUI.

Diagnostics shows build {revision[:12]}. The managed software inventory and the
individual component builds are in build.json.
"""


def record(label, revision, report):
    return {"label": label, "commit": revision, "source": "https://github.com/TacitusVox/K-UI-NeXT/tree/" + revision,
            "updater": {"channel": report["channel"], "full": report["full"], "build": report["build"],
                        "manifest_sha256": report["manifest_sha256"]},
            "software": report["members"],
            "boot_disc": {"build": "0e4ab5a", "bytes": BOOT_BYTES, "sha256": BOOT_SHA}}


def put(archive, name, stream):
    # Fix timestamps and permissions so identical release inputs yield identical ZIPs.
    entry = zipfile.ZipInfo(name)
    entry.create_system = 3
    entry.external_attr = 0o100644 << 16
    entry.compress_type = zipfile.ZIP_DEFLATED
    with archive.open(entry, "w") as dest:
        shutil.copyfileobj(stream, dest, 1024 * 1024)


def verify_distribution(package_path, sd_zip_path, *, revision):
    report = verify(package_path)
    members = require_software(report, revision)
    current_consumer_check(package_path)
    notices = auxiliary_files()
    owned = {"KUI/" + name: (item["bytes"], item["sha256"]) for name, item in members.items()}
    owned["KUI/boot.kui"] = owned["KUI/runtime.kui"]
    expected = dict(owned)
    expected[BOOT_PATH] = BOOT_BYTES, BOOT_SHA
    expected.update({name: (path.stat().st_size, digest(path)) for name, path in notices.items()})
    with zipfile.ZipFile(sd_zip_path) as archive:
        names = archive.namelist()
        if set(names) != set(expected) | {"build.json", "START-HERE.md"} or len(names) != len(set(names)):
            raise ValueError("SD ZIP inventory differs from the KUP and physical boot files")
        for name, (size, sha) in expected.items():
            entry = archive.getinfo(name)
            if entry.file_size != size or entry.external_attr >> 16 != 0o100644:
                raise ValueError("Unexpected ZIP member size or file type: " + name)
            with archive.open(entry) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != sha:
                    raise ValueError("SD ZIP checksum mismatch: " + name)
        if archive.getinfo("build.json").file_size > 262144 or archive.getinfo("START-HERE.md").file_size > 8192:
            raise ValueError("Oversized distribution metadata")
        actual = json.loads(archive.read("build.json"))
        label = actual["label"]
        if not isinstance(label, str) or not label.isprintable() or not 1 <= len(label) <= 120 or \
                actual != record(label, revision, report) or \
                archive.read("START-HERE.md") != install_guide(label, revision).encode():
            raise ValueError("SD ZIP metadata differs from the checked KUP")
    return report


def write_distribution(stage, package_path, sd_zip_path, *, revision, label, boot_cdi=None):
    stage, package_path, sd_zip_path = map(Path, (stage, package_path, sd_zip_path))
    if not isinstance(label, str) or not label.isprintable() or not 1 <= len(label) <= 120:
        raise ValueError("Invalid distribution label")
    if package_path.suffix != ".kup" or sd_zip_path.suffix != ".zip" or \
            package_path.parent.resolve() != sd_zip_path.parent.resolve() or \
            package_path.resolve().is_relative_to(stage.resolve()):
        raise ValueError("Use sibling .kup and .zip outputs outside the stage")
    parent = package_path.parent
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".kui-distribution-", dir=parent) as work:
        kup, sd_zip = Path(work) / "software.kup", Path(work) / "sd.zip"
        report = make_update(stage, kup, full=True, channel="testing")
        members = require_software(report, revision)
        sources = {"KUI/" + name: stage / name for name in members}
        sources["KUI/boot.kui"] = stage / "runtime.kui"
        sources.update(auxiliary_files())
        with zipfile.ZipFile(sd_zip, "x", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, path in sorted(sources.items()):
                with path.open("rb") as stream:
                    put(archive, name, stream)
            with (Path(boot_cdi).open("rb") if boot_cdi else gzip.open(BOOT_FILE)) as stream:
                put(archive, BOOT_PATH, stream)
            put(archive, "START-HERE.md", BytesIO(install_guide(label, revision).encode()))
            put(archive, "build.json", BytesIO((json.dumps(record(label, revision, report), indent=2) + "\n").encode()))
        verify_distribution(kup, sd_zip, revision=revision)
        kup.replace(package_path)
        sd_zip.replace(sd_zip_path)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", type=Path)
    parser.add_argument("package", type=Path)
    parser.add_argument("sd_zip", type=Path)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    write_distribution(args.stage, args.package, args.sd_zip, revision=args.revision, label=args.label)
