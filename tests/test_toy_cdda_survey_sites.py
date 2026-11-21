#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whether the gesture test notices the survey's heartbeat call moved: each of
a list of defects put into src/loader/vmu_native_passive.c in a copy of the
sources, and the gesture test (toy-survey-mixed-pad-host-test) built and run
there with Test 84's survey on. The repository's own files are never written.

usage: test_toy_cdda_survey_sites.py [--work DIR]

Each mutation must still compile and must make the survey's variant of the
gesture test fail (Makefile.vmu-native-toy-suite,
toy-cdda-survey-site-mutation-check).
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ("TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 "
          "TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 "
          "TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 "
          "TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1 TOY_CDDA_SURVEY=1").split()
FAST = ("        if(!kui_vmu_native_early_server(NATIVE_HOT_CONFIG.driver.family) ||\n"
        "           (controller_pair.frames[controller_pair.parent].header&255u)==9u)\n"
        "            kui_toy_cdda_survey_heartbeat();")
MIXED = "        if(pad_sample>=0) kui_toy_cdda_survey_heartbeat();"
COMPLETE = "        if(!controller_complete(f)) return;"
MUTATIONS = [
    ("the fast path surveys early discovery batches", [(FAST, "        kui_toy_cdda_survey_heartbeat();")]),
    ("the mixed path surveys batches without the controller", [(MIXED, "        kui_toy_cdda_survey_heartbeat();")]),
    ("the fast path surveys before the controller's completion",
     [(FAST, ""), (COMPLETE, "#if KUI_TOY_CDDA_SURVEY\n" + FAST + "\n#endif\n" + COMPLETE)]),
    ("no survey on the fast path", [(FAST, "")]),
    ("no survey on the mixed path", [(MIXED, "")]),
]


def copy_tree(work):
    for name in ("include", "src", "tests"):
        shutil.copytree(ROOT / name, work / name, ignore=shutil.ignore_patterns("__pycache__"))
    for makefile in ROOT.glob("Makefile*"):
        shutil.copy2(makefile, work / makefile.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work")
    args = parser.parse_args()
    base = Path(args.work or tempfile.mkdtemp(prefix="toy-cdda-survey-sites-"))
    print("mutations in", base)
    env = dict(os.environ)
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)
    results = []
    for index, (name, edits) in enumerate(MUTATIONS):
        work = base / ("m%02d" % index)
        if work.exists():
            shutil.rmtree(work)
        work.mkdir(parents=True)
        copy_tree(work)
        source = work / "src/loader/vmu_native_passive.c"
        text = source.read_text()
        applied = True
        for old, new in edits:
            if text.count(old) != 1:
                applied = False
                break
            text = text.replace(old, new)
        if not applied:
            results.append((name, "not applied"))
            continue
        source.write_text(text)
        run = subprocess.run(["make", "-f", "Makefile.vmu-native-toy-suite", *RECIPE,
                              "HOST_CC=" + env.get("HOST_CC", "cc"), "BUILD=" + str(work / "build"),
                              "toy-survey-mixed-pad-host-test"], cwd=work, env=env, capture_output=True, text=True)
        log = run.stdout + run.stderr
        (work / "log.txt").write_text(log)
        if run.returncode == 0:
            results.append((name, "SURVIVED"))
        elif "test-survey-mixed-pad-cdda" in log and ("Assertion" in log or "test_toy_survey_mixed_pad.c" in log):
            results.append((name, "caught"))
        else:
            results.append((name, "did not build"))
        shutil.rmtree(work / "src")
        shutil.rmtree(work / "include")
    for name, result in results:
        print("%-9s %s" % (result, name))
    bad = [(n, r) for n, r in results if r != "caught"]
    if bad:
        sys.exit("FAIL %d of %d heartbeat-site mutations not caught" % (len(bad), len(results)))
    print("PASS Test 84 survey sites: all %d mutations of the heartbeat's calls caught by the gesture test" % len(results))


if __name__ == "__main__":
    main()
