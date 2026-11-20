#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and run the stream survey's report-file test against FatFs, and hold
tools/toy_survey.py to the same file: its blank file is the C sources' byte
for byte, and it reads back every launch the C test played."""
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import toy_survey

FILE_ID = bytes.fromhex("0123456789abcdeffedcba9876543210")
SLOTS = 20


def fatfs_source():
    dependency = ROOT / ".deps/fatfs/source"
    return dependency if dependency.is_dir() else ROOT / "fatfs/source"


def main():
    config = (ROOT / "config/ffconf.h").read_text()
    marker = "#define FF_USE_MKFS 0"
    assert config.count(marker) == 1
    with tempfile.TemporaryDirectory() as scratch:
        base = Path(scratch)
        header = base / "survey-ffconf.h"
        header.write_text(config.replace(marker, "#define FF_USE_MKFS 1"))
        compiler = [*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-include", str(header),
                    "-I" + str(ROOT / "include"), "-I" + str(fatfs_source())]
        shared = [str(ROOT / "src/core/toy_survey.c"), str(ROOT / "src/core/toy_survey_fat.c"),
                  str(ROOT / "src/core/retail_image.c"), str(ROOT / "src/core/hash.c"), str(ROOT / "src/core/data.c"),
                  str(fatfs_source() / "ff.c"), str(fatfs_source() / "ffunicode.c")]
        environment = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        binary = base / "test-toy-survey"
        subprocess.run([*compiler, str(ROOT / "tests/test_toy_survey.c"), *shared, "-o", str(binary)], check=True)
        out = base / "out"
        out.mkdir()
        subprocess.run([str(binary), str(out)], check=True, env=environment)
        # The cold stage's own text, on the same kind of card.
        stage = base / "test-toy-survey-stage"
        subprocess.run([*compiler, "-I" + str(ROOT / "src/loader"), '-DKUI_BUILD_ID="0123456789ab"',
                        "-DKUI_VMU_NATIVE_PERSISTENT_PROBE=1", str(ROOT / "tests/test_toy_survey_stage.c"), *shared,
                        "-o", str(stage)], check=True)
        subprocess.run([str(stage)], check=True, env=environment)
        # The tool's blank file is the stage's, byte for byte.
        blank = (out / "blank.txt").read_bytes()
        assert blank == toy_survey.blank_file(FILE_ID, SLOTS), "the tool's blank file differs from the C sources'"
        assert toy_survey.read_file(blank) == {
            "id": FILE_ID.hex().upper(), "slots": SLOTS,
            "entries": [{"slot": slot, "state": "empty"} for slot in range(SLOTS)]}
        # The tool reads every launch the C test played.
        used = toy_survey.read_file((out / "used.txt").read_bytes())
        assert used["id"] == FILE_ID.hex().upper() and used["slots"] == SLOTS
        for slot, entry in enumerate(used["entries"]):
            launch = entry["launch"]
            assert entry["slot"] == slot and "damaged" not in launch, entry
            assert launch == {
                "survey": 83, "build": "0123456789ab", "product": "T-12345N",
                "title": "SURVEY GAME NUMBER " + chr(ord("A") + slot % 26), "bootfile": "1ST_READ.BIN", "region": "JUE",
                "ip_crc32": 0x11111111 + slot, "gdi_crc32": 0x22222222, "boot_crc32": 0x33333333,
                "boot_bytes": 0x123456, "exec_crc32": 0x44444444 + slot, "exec_bytes": 0x123456,
                "reader": 1, "transport": 1, "flags": 4, "source": slot & 1, "tracks": 5 + slot, "raw": 1,
                "cooked": 1, "audio": 2 + slot, "audio_high": 1 + slot, "audio_mapped": slot // 2, "other": 1,
                "extents": 7,
                "session_lba": 45150, "boot_lba": 45200 + slot}, launch
            if slot % 5 == 3:
                assert entry["state"] == "launched, no report saved" and "report" not in entry, entry
            else:
                assert entry["state"] == "report saved", entry
                report = entry["report"]
                assert len(report) == 1024 and report.endswith("\nEND REPORT\n")
                assert report.startswith("K-UI TOY COMMANDER REPORT\nTEST 81\nBUILD 0123456789ab\nSLOT %d\n" % slot)
        # A truncated upload and an edited first sector are refused, not guessed at.
        for bad in (blank[:-512], blank[:40] + b"X" + blank[41:], b"\0" * len(blank)):
            try:
                toy_survey.read_file(bad)
            except ValueError:
                continue
            raise AssertionError("a damaged file was read")
        print("PASS tools/toy_survey.py: its blank file is the stage's; %d launches read back" % SLOTS)


if __name__ == "__main__":
    sys.exit(main())
