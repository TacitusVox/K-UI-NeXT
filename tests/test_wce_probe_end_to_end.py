#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The WinCE data probe end to end on the host (tests/test_wce_probe_end_to_end.c),
then tools/wce_probe_data.py decodes what it left: on FAT32 and exFAT, with a
contiguous and a fragmented data file."""
import hashlib
import json
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import wce_probe_data  # noqa: E402
import wce_probe_mine  # noqa: E402


def fatfs_source():
    dependency = ROOT / ".deps/fatfs/source"
    return dependency if dependency.is_dir() else ROOT / "fatfs/source"


def check(ok, what):
    if not ok:
        raise SystemExit("FAIL " + what)


def main():
    config = (ROOT / "config/ffconf.h").read_text()
    marker = "#define FF_USE_MKFS 0"
    assert config.count(marker) == 1
    with tempfile.TemporaryDirectory() as scratch:
        base = Path(scratch)
        header = base / "probe-ffconf.h"
        header.write_text(config.replace(marker, "#define FF_USE_MKFS 1"))
        binary = base / "test-wce-probe-end-to-end"
        subprocess.run([*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1", "-g",
                        "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-include", str(header),
                        "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src/loader"), "-I" + str(fatfs_source()),
                        str(ROOT / "tests/test_wce_probe_end_to_end.c"), str(ROOT / "src/core/wce_probe_fat.c"),
                        str(fatfs_source() / "ff.c"), str(fatfs_source() / "ffunicode.c"),
                        "-o", str(binary)], check=True)
        blank = base / "blank.bin"
        wce_probe_data.blank(blank, 5, "host test")
        for fs, fragment in (("exfat", 0), ("fat32", 0), ("exfat", 3), ("fat32", 3)):
            data, ram, info = base / f"data-{fs}-{fragment}.bin", base / "ram.bin", base / "info.json"
            subprocess.run([str(binary), fs, str(fragment), str(blank), str(data), str(ram), str(info)], check=True)
            dumps = base / f"dumps-{fs}-{fragment}"
            result = wce_probe_data.read(data, None, dumps)
            sessions = {s["slot"]: s for s in result["sessions"] if "state" in s}
            check(set(sessions) == {0, 1, 2, 3, 4}, f"five claimed slots, got {sorted(sessions)}")
            s0, s1, s2 = sessions[0], sessions[1], sessions[2]
            check(sessions[3]["state"] == "claimed" and sessions[3]["stage"] is None,
                  "an interrupted claim: CLAIMED header, no stage report")
            check(sessions[4]["state"] == "claimed" and sessions[4]["stage"]["ip"]["product"] == "T12509N",
                  "the launch after an interrupted claim takes the next slot")
            check(s0["state"] == "complete" and s1["state"] == "claimed" and s2["state"] == "claimed",
                  "states: complete, claimed (failed dump), claimed (stage only)")
            check(s0["stage"]["ip"]["product"] == "T13004N" and s1["stage"]["ip"]["product"] == "T42101N",
                  "stage reports name their games")
            t = s0["terminal"]
            check(t and t["reason"] == "A+B+X+Y+Start" and t["caller"] == "0x8c123456", "terminal report")
            check(t["registers"]["MDSTAR"] == "0x0c100000" and t["registers"]["SPG_CONTROL"] == "0x00000150",
                  "registers")
            c = t["counters"]
            expected = json.loads(info.read_text())
            check(c["calls"] == 1 + 80 + 14 + 1 + 8, "call count")
            check(c["functions"] == {"REQUEST": 55, "CHECK": 40, "INIT": 1, "DRIVE": 8}, "calls by function")
            check(c["commands"] == {"DMAREAD": 40, "PLAY": 7, "PAUSE": 7}, "accepted commands")
            check(c["refused"] == {"count": 1, "last": "OPEN_TRAY", "commands": ["OPEN_TRAY"]}, "refused commands")
            check(c["reads"] == {"requests": 40, "sectors": sum(32 + i for i in range(40)), "largest": 71}, "reads")
            log = c["audio_log"]
            check(c["audio_commands"] == 14 and len(log) == 12 and c["audio_log_dropped"] == 2, "audio log size")
            check([a["first"] for a in log] == [2, 3, 4, 5] + list(range(8, 16)), "audio log keeps first 4, last 8")
            check(log[0]["command"] == "PLAY" and log[1]["command"] == "PAUSE" and log[0]["repeat"] == 15,
                  "audio commands and parameters")
            m = c["maple"]
            check(m["samples"] == c["calls"] // 4 and m["frames"] == 3 * m["samples"] and not m["bad_lists"],
                  "Maple samples and frames")
            units = m["units"]
            check(units["A"]["condition"] == m["samples"] and units["A"]["functions"] == "0x01000000", "port A")
            check(units["A1"]["block write"] == min(255, m["samples"]) and units["A1"]["functions"] == "0x02000000", "A1 writes")
            check(units["B2"]["media info"] == min(255, m["samples"]), "B2 media info")
            # The dump: every non-zero page, byte for byte.
            image = Path(s0["dump_file"]).read_bytes()
            original = ram.read_bytes()
            check(hashlib.sha256(image).digest() == hashlib.sha256(original).digest(), "dump reproduces RAM")
            pages = sum(1 for p in range(4096) if any(original[p * 4096:(p + 1) * 4096]))
            check(t["pages"] == pages == s0["dump_pages"], "page counts")
            check(s1["terminal"] is None or s1["terminal"]["reason"] in ("reader fault", "A+B+X+Y+Start"), "slot 1")
            # The miner on the same file: launches, ends, ports and CD audio.
            mined = {r["slot"]: r for r in wce_probe_mine.mine(data)["launches"]}
            check(set(mined) == {0, 1, 2, 3, 4} and mined[0]["complete"] and mined[0]["end"] == "A+B+X+Y+Start",
                  "miner: launches and the completed end")
            check(mined[0]["ports"]["saved_to"] == ["A1"] and mined[0]["cd_audio"]["commands"] == {"PLAY": 7, "PAUSE": 7},
                  "miner: saves and CD audio")
            check("not in the dump" in mined[0]["dump"]["note"] and mined[0]["dump"]["memory"]["dumped_pages"] == pages,
                  "miner: dump without CE structures")
            check(mined[2]["end"].startswith("no end-of-game record"), "miner: stage-only launch")
            print(f"PASS decode {fs} fragment {fragment}: slot 0 complete with {pages} pages, slot 1 claimed "
                  "after a failed dump, slot 2 claimed, slot 3 interrupted, slot 4 next; miner agrees")
    return 0


if __name__ == "__main__":
    sys.exit(main())
