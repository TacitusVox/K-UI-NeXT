#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 88: the CD-music output on the original sound drivers. The output,
unmodified, runs over sound memory as each MANATEE-lineage image of the corpus
leaves it after its own start-up (tests/test_toy_cdda_output_drivers.c).

usage: test_toy_cdda_output_drivers.py HARNESS SOUND_FILES_JSON DRIVER_DIR

DRIVER_DIR holds the owner's files named by their SHA-256, as the CD-audio
sound-driver archive gives them; they are read and never copied (the memory a
start-up leaves goes to the harness through a pipe). Run with python3 -I. Needs
the unicorn and capstone packages, as tools/manatee_read.py does, whose ARM7
model this uses.

What the output decides is held against readings that are not its own:
- the family: the drivers' static reading of their code, by SHA-256 (the table
  of tests/test_toy_cdda_survey_drivers.py, Test 84's);
- the count's word and its measure: the word the image's own timer interrupt
  writes in sound memory and how far 3,528 interrupts move it;
- the ring's place: from the first address above the highest ring the image
  indexes (the command history, MANATEE 2's MIDI ring) to 0x17FFF, which the
  harness allows a store in and nowhere else in sound memory;
- the two channels: the family's by the same static reading, the only ones
  whose registers the harness allows.
Then two minutes of play: one key-on, no ring begun again, the output's
reckoning of the frames played within a count and a heartbeat's own time of
the model chip's. One line an image with its full SHA-256: the input
inventory.

What rests on what: everything in sound memory is the image's own, written by
its start-up as run on the model; nothing is authored. The model is not a
console, and no host command of a game's reaches the driver: the records are
as a start-up leaves them, every port and pool entry free.
"""
import hashlib
import importlib.util
import json
import os
import subprocess
import sys

GEN2, GEN1, GEN1_OLD, NAMIHEI = 1, 2, 3, 4
# The MANATEE lineage by build, as the drivers' code reads (Test 84's table).
STATIC = {
    "4320e30aed03": GEN1_OLD, "afb9469debb8": GEN1_OLD, "fcf11377370b": GEN1_OLD, "1b1c71711bcc": GEN1_OLD,
    "8631470a329d": GEN1, "49e26ddf0d7e": GEN1, "5ab700a241e5": GEN1, "906e3b8a1336": GEN1,
    "12c461d78bcf": GEN1, "64c38a8d253c": GEN1, "8a1433e941ec": None,
    "5f0dff5a07b4": NAMIHEI, "ca87ecf864d0": NAMIHEI,
    "276264a7d4b0": GEN2, "b95396e19897": GEN2, "11c89eaa2b20": GEN2, "de9ab7ffa637": GEN2,
    "3c76d1dc9177": GEN2, "1c9cd13f9c6c": GEN2, "fecdab296c8b": GEN2, "416cbb4f5624": GEN2,
    "938069c72b0f": GEN2, "3f6aabe1fc73": GEN2, "f9c9175d7a9c": GEN2, "a70defb3d960": GEN2,
    "509589f6b96d": GEN2, "2806d17567c5": GEN2, "8c4a4d3b2b91": GEN2,
}
PAIR = {GEN2: (62, 63), GEN1: (61, 62), GEN1_OLD: (62, 63), NAMIHEI: (62, 63)}
INTERRUPTS = 8 * 441
# A count is 177 frames at most, and the model's heartbeat reads the count a
# little after the tick the output keeps for the look.
WITHIN = 177 + 64


def load_reader():
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools", "manatee_read.py")
    spec = importlib.util.spec_from_file_location("manatee_read", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def study(reader, image):
    """What the image shows by itself: memory after start-up, the count's word and measure, the spare memory's start."""
    run = reader.Run(image)
    if not run.go(0, 3000000) or run.fault:
        return None
    memory = bytes(run.mu.mem_read(0, 0x18000))
    run.phase = "timer"
    for _ in range(INTERRUPTS):
        if not run.interrupt(2) or not run.go(run.mu.reg_read(reader.UC_ARM_REG_PC), 400) or run.fault:
            return None
    words = sorted(a for a in run.fiq_writes if a >= 0x10000)
    if len(words) != 1:
        return None
    count_at = words[0]
    grew = (run.word(count_at) - reader.le32(memory, count_at)) & 0xffffffff
    if grew not in (INTERRUPTS // 4, INTERRUPTS * 440 // 441 // 4):
        return None
    values, masks = reader.named(image)
    if not values or set(values) != set(masks) or any(len(m) != 1 for m in masks.values()):
        return None
    spare = max(base + max(mask) + 1 for base, mask in masks.items())
    return memory, count_at, grew == INTERRUPTS // 4, spare


def main():
    harness, inventory, directory = sys.argv[1:4]
    reader = load_reader()
    entries = [e for e in json.load(open(inventory)) if e.get("family") == "MANATEE"]
    lines, problems, missing = [], [], []
    for entry in entries:
        key = entry["sha256"][:12]
        path = os.path.join(directory, entry["sha256"])
        if not os.path.isfile(path):
            missing.append(entry["sha256"])
            continue
        data = open(path, "rb").read()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            problems.append("%s: not the file the inventory names" % entry["sha256"])
            continue
        image = data[entry.get("image_offset", 0):]
        expected = STATIC[key]
        if expected is None:
            lines.append("%s damaged copy: not run, no expectation" % entry["sha256"])
            continue
        found = study(reader, image)
        if found is None:
            problems.append("%s: the image's own start-up, count or rings were not read" % key)
            continue
        memory, count_at, slow, spare = found
        pair = PAIR[expected]
        done = subprocess.run([harness, "%d" % count_at, "%d" % slow, "%d" % spare, "%d" % pair[0], "%d" % pair[1]],
                              input=memory, capture_output=True)
        if done.returncode:
            problems.append("%s: %s" % (key, done.stderr.decode("ascii", "replace").strip()))
            lines.append("%s FAIL: %s" % (entry["sha256"], done.stderr.decode("ascii", "replace").strip()))
            continue
        r = dict(kv.split("=") for kv in done.stdout.decode("ascii").split())
        why = []
        if int(r["family"]) != expected:
            why.append("family %s, the static reading's %d" % (r["family"], expected))
        if int(r["version"], 16) != reader.le32(image, 0x20) or int(r["hash"], 16) != reader.date_hash(image):
            why.append("not the image's version bytes and date hash")
        if r["channels"] != "%d,%d" % pair:
            why.append("channels %s" % r["channels"])
        if int(r["ring"], 16) != spare or int(r["frames"]) != 0x18000 - spare - 64 or r["spare"] != "1":
            why.append("the ring at %s, %s frames; the image's spare memory begins at %05x" % (r["ring"], r["frames"], spare))
        # (One key-on of the output's, which the model's chip sees as its two channels keyed on.)
        if r["phase"] != "2" or r["why"] != "0" or r["keys"] != "1" or r["key_ons"] != "2":
            why.append("phase %s, reason %s, %s key-ons, %s channels keyed on" % (r["phase"], r["why"], r["keys"], r["key_ons"]))
        for name in ("restarts", "underruns", "clock_faults", "takes", "faults", "timeouts"):
            if r[name] != "0":
                why.append("%s %s" % (name, r[name]))
        if int(r["looks"]) < 6000 or int(r["worst"]) > WITHIN:
            why.append("%s looks, the frames played reckoned %s out" % (r["looks"], r["worst"]))
        lines.append("%s %d.%02x.%02x family %d channels %s ring %s of %s frames; count %05x, %s frames a count; "
                     "2 minutes: %s key-on, %s looks, reckoning within %s frames [the image's own start-up] %s" % (
                         entry["sha256"], image[0x20], image[0x21], image[0x22], int(r["family"]), r["channels"], r["ring"],
                         r["frames"], count_at, "176" if slow else "176.4", r["keys"], r["looks"], r["worst"],
                         "ok" if not why else "FAIL: " + "; ".join(why)))
        if why:
            problems.append("%s %s" % (key, "; ".join(why)))
    print("\n".join(sorted(lines, key=lambda line: line.split()[1:2] + line.split()[0:1])))
    print("%d MANATEE-lineage images listed, %d absent" % (len(entries), len(missing)))
    if missing:
        print("absent: " + " ".join(sha[:12] for sha in missing))
    if problems or missing:
        sys.exit("FAIL\n" + "\n".join(problems))
    print("PASS the output on every original MANATEE-lineage driver: its family, its channels, its spare memory and its count")


if __name__ == "__main__":
    main()
