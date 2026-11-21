#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 84 (SURVEY) on the original sound drivers: the survey, unmodified, over
every driver image the corpus lists (docs/evidence/cd-audio-corpus-2026-11-20/
sound-files.json), each with its family's start-up stores
(tests/test_toy_cdda_survey_drivers.c).

usage: test_toy_cdda_survey_drivers.py HARNESS SOUND_FILES_JSON DRIVER_DIR

DRIVER_DIR holds the owner's files named by their SHA-256, as the CD-audio
sound-driver archive gives them; they are read and never copied. For each
image the survey's result is held against two readings that are not its own:
- the family, from the drivers' static reading of their code (the table
  below for the MANATEE lineage, by SHA-256; the corpus's family for the rest);
- the version word and the date's hash, from the image's own bytes at 0x20
  and 0x50..0x5B.
And no identity change, no timeout, every survey of a family with candidates
proved (its records at rest: none in use), none for the others. One line an
image with its full SHA-256: the input inventory. Run with python3 -I.
"""
import hashlib
import json
import os
import subprocess
import sys

GEN2, GEN1, GEN1_OLD, NAMIHEI, AUDIO64, HYBRID, UNKNOWN = 1, 2, 3, 4, 5, 6, 0
# The MANATEE lineage by build, as the drivers' code reads (Test 84's notes,
# docs/cd-audio-corpus.md): 1.01 builds before 1.01.37, from it, NAMIHEI, 2.x.
# 8a1433e941ec is the 1999.03.18 build damaged by a newline conversion: no
# driver runs from it, and it has no expectation.
MANATEE = {
    "4320e30aed03": GEN1_OLD, "afb9469debb8": GEN1_OLD, "fcf11377370b": GEN1_OLD, "1b1c71711bcc": GEN1_OLD,
    "8631470a329d": GEN1, "49e26ddf0d7e": GEN1, "5ab700a241e5": GEN1, "906e3b8a1336": GEN1,
    "12c461d78bcf": GEN1, "64c38a8d253c": GEN1, "8a1433e941ec": None,
    "5f0dff5a07b4": NAMIHEI, "ca87ecf864d0": NAMIHEI,
    "276264a7d4b0": GEN2, "b95396e19897": GEN2, "11c89eaa2b20": GEN2, "de9ab7ffa637": GEN2,
    "3c76d1dc9177": GEN2, "1c9cd13f9c6c": GEN2, "fecdab296c8b": GEN2, "416cbb4f5624": GEN2,
    "938069c72b0f": GEN2, "3f6aabe1fc73": GEN2, "f9c9175d7a9c": GEN2, "a70defb3d960": GEN2,
    "509589f6b96d": GEN2, "2806d17567c5": GEN2, "8c4a4d3b2b91": GEN2,
}
KIND = {GEN2: "gen2", GEN1: "gen1", GEN1_OLD: "gen1", NAMIHEI: "namihei", None: "gen1"}


def le32(b):
    return int.from_bytes(b, "little")


def date_hash(image):
    h = 0x811c9dc5
    for byte in image[0x50:0x5c]:
        h = ((h ^ byte) * 0x01000193) & 0xffffffff
    return (h ^ (h >> 16)) & 0xffff


def plan(entry):
    """(kind, expected family) of a corpus driver entry, or None for a file that is no driver."""
    family, names = entry.get("family"), " ".join(entry.get("names", []))
    if family == "MANATEE":
        expected = MANATEE[entry["sha256"][:12]]
        return KIND[expected], expected
    if family == "AUDIO64":
        return "audio64", AUDIO64
    if family == "AUDIO64-MIDIDA":
        # ARMCOM 1.69 (version 01 60, Windows CE discs only) is no family the survey takes.
        return ("other", UNKNOWN) if "ARMCOM" in names else ("midida", HYBRID)
    if family == "AICADRV":
        return "aicadrv", UNKNOWN
    return None


def main():
    harness, inventory, directory = sys.argv[1:4]
    entries = [e for e in json.load(open(inventory)) if plan(e)]
    jobs, lines, problems, missing = [], [], [], []
    for e in entries:
        path = os.path.join(directory, e["sha256"])
        if not os.path.isfile(path):
            missing.append(e["sha256"])
            continue
        data = open(path, "rb").read()
        if hashlib.sha256(data).hexdigest() != e["sha256"]:
            problems.append("%s: not the file the inventory names" % e["sha256"])
            continue
        jobs.append((e, path, data[e.get("image_offset", 0):]))
    out = subprocess.run([harness], input="".join("%s %d %s\n" % (p, e.get("image_offset", 0), plan(e)[0])
                                                for e, p, _ in jobs),
                         capture_output=True, text=True, check=True).stdout.splitlines()
    assert len(out) == len(jobs), "the harness answered %d of %d images" % (len(out), len(jobs))
    checked = 0
    for (e, _, image), line in zip(jobs, out):
        r = dict(kv.split("=") for kv in line.split())
        kind, expected = plan(e)
        got = {k: int(v, 16) if k in ("version", "date_hash", "state") else v for k, v in r.items()}
        family = int(got["family"])
        why = []
        if got["version"] != le32(image[0x20:0x24]):
            why.append("version %08x, the image's %08x" % (got["version"], le32(image[0x20:0x24])))
        if got["date_hash"] != date_hash(image):
            why.append("date hash %04x, the image's %04x" % (got["date_hash"], date_hash(image)))
        if got["changes"] != "0" or got["timeouts"] != "0":
            why.append("changes %s, timeouts %s" % (got["changes"], got["timeouts"]))
        if expected is not None:
            checked += 1
            if family != expected:
                why.append("family %d, the static reading's %d" % (family, expected))
            candidates = expected not in (UNKNOWN,)
            if candidates and (got["proved"] != "4" or got["unproved"] != "0" or got["busy"] != "0/0"):
                why.append("proved %s, unproved %s, busy %s" % (got["proved"], got["unproved"], got["busy"]))
            if not candidates and (got["proved"] != "0" or got["unproved"] != "0"):
                why.append("proved %s, unproved %s for a family without candidates" % (got["proved"], got["unproved"]))
            state = {GEN2: 0x43455845, NAMIHEI: le32(image[0x38:0x3c]),
                     GEN1: le32(image[0xf8:0xfc]), GEN1_OLD: le32(image[0xf8:0xfc])}.get(expected, 0)
            if expected == NAMIHEI and image[0x38:0x3c] != b"SEGA":
                why.append("no 'SEGA' at 0x38 to copy")
            if got["state"] != state:
                why.append("state %08x, expected %08x" % (got["state"], state))
            window = 4 * 64 if expected in (AUDIO64, HYBRID) else 0
            if int(got["window_reads"]) != window:
                why.append("window reads %s" % got["window_reads"])
        lines.append("%s %-8s family %d version %08x date %04x proved %s unproved %s %s" % (
            e["sha256"], kind, family, got["version"], got["date_hash"], got["proved"], got["unproved"],
            "no expectation (damaged)" if expected is None else ("ok" if not why else "FAIL: " + "; ".join(why))))
        if why:
            problems.append("%s %s" % (e["sha256"][:12], "; ".join(why)))
    print("\n".join(lines))
    for sha in missing:
        print("%s unavailable: not in %s" % (sha, directory))
    if problems:
        sys.exit("FAIL Test 84 survey on the original drivers: " + " | ".join(problems))
    print("PASS Test 84 survey on the original drivers: %d images run (%d unavailable), %d held against the "
          "static reading's family, every version and date the image's own" % (len(jobs), len(missing), checked))


if __name__ == "__main__":
    main()
