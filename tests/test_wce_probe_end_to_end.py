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


def fault_reasons():
    # The decoder's table is the reader's fault messages keyed by length + 1,
    # and no two messages may share a length.
    import re
    text = (ROOT / "src/loader/retail_resident.c").read_text()
    literals = sorted(set(re.findall(r'report_fault\("([^"]+)"', text)))
    by_code = {}
    for s in literals:
        by_code.setdefault(len(s) + 1, []).append(s)
    check(all(len(v) == 1 for v in by_code.values()), f"fault messages share a length: {by_code}")
    check({k: v[0] for k, v in by_code.items()} == wce_probe_data.FAULT_REASONS,
          "the decoder's fault table matches the reader's messages")
    print(f"PASS {len(literals)} reader fault messages have distinct lengths and decode by length")


PER_MS = 12500  # session()'s TMU0: Pphi/4, a 125000-tick (10 ms) period


def scenario_ticks(i):
    """Read i of session(): issued 34999 ticks into a tick of CE's count, and
    ended 10 + i ms later by that count, 64999 + 125 (i + 1) ticks into a tick
    (a whole period more for read 5, which ends with an underflow pending).
    Returns its begin and its time to the end, in ticks."""
    begin = (1000 + 100 * i) * PER_MS + 34999
    took = (10 + i) * PER_MS + 64999 + 125 * (i + 1) + (125000 if i == 5 else 0) - 34999
    return begin, took


def us(ticks):
    return round(ticks * 1000 / PER_MS)


def wrapped_ring_checks(path):
    """The ring the C test's last block leaves (256 reads, 190 kept),
    through the decoder: order, saturations, flags and carried totals."""
    image = bytearray(wce_probe_data.TIMING_ADDRESS - wce_probe_data.RAM_BEGIN + wce_probe_data.TIMING_BYTES)
    image[wce_probe_data.TIMING_ADDRESS - wce_probe_data.RAM_BEGIN:] = Path(path).read_bytes()
    t = wce_probe_data.decode_timing(image)
    r = t["requests"]
    check(t["requests_begun"] == 256 and t["requests_kept"] == 190 and len(r) == 190 and not t["open_at_end"],
          f"wrapped ring counts {t['requests_begun']} {t['requests_kept']}")
    # Oldest first: reads 66 to 249, then the six after the loop.
    check([q["lba"] for q in r[:184]] == list(range(66, 250)), "wrapped ring order")
    check(r[247 - 66]["sectors"] == "65535+" and all(q["sectors"] == 1 for q in r[:184] if q is not r[247 - 66]),
          "sectors saturate")
    replaced, ended_by_new, aborted, busy, reset, long_read = r[184:]
    check(replaced["replaced"] and not replaced["ended"] and replaced["latency_ms"] is None, f"replaced {replaced}")
    check(ended_by_new["replaced"] and ended_by_new["ended"] and not ended_by_new["failed"], "ended by another token")
    check(aborted["failed"] and aborted["ended"] and not aborted["replaced"], "aborted read failed")
    check(busy["calls"] == "65535+" and busy["ended"] and not busy["failed"], f"calls saturate {busy}")
    check(reset["failed"] and reset["ended"] and not reset["replaced"], "a read ended by a reset failed")
    check(long_read["latency_ms"] == "65535+" and long_read["latency_us"] is None and long_read["end_us"] is None
          and long_read["issued_ms"] == 100000, f"latency saturates {long_read}")
    # Totals carried past 32 bits: every call after the call seed is one tick
    # (the 70000 saturating calls, their read's end, the reset's, the long
    # read's) on top of 2^32 + 4; the interrupt seed alone makes 2^32 + 4.
    check(t["call_us"] == us((1 << 32) + 4 + 70000 + 1 + 1 + 1) and t["irq_us"] == us((1 << 32) + 4)
          and t["end_ms"] == 170500, f"carried totals {t['call_us']} {t['irq_us']}")
    print("PASS a wrapped ring decodes: order, saturated sectors, calls and latency, replaced, aborted and reset "
          "reads, carried totals")


def cut_ring_checks(data, base):
    """A dump cut off inside the ring's last dumped page leaves no ring to
    decode; one cut after it still has its ring. (A ring page that is all
    zero is not dumped at all: with 40 reads only the first is.)"""
    decoded = wce_probe_data.read(data)
    s0 = decoded["sessions"][0]
    check(s0["dump_complete"] and "error" not in s0["timing"], "slot 0's ring before the cut")
    raw = bytearray(Path(data).read_bytes())
    slot = (decoded["file"]["first_slot"]) * wce_probe_data.SECTOR
    terminal = raw[slot + wce_probe_data.TERMINAL_SECTOR * wce_probe_data.SECTOR:
                   slot + (wce_probe_data.TERMINAL_SECTOR + wce_probe_data.TERMINAL_SECTORS) * wce_probe_data.SECTOR]
    pages = wce_probe_data.page_map(bytes(terminal))[:s0["terminal"]["pages"]]
    dump = slot + wce_probe_data.DUMP_SECTOR * wce_probe_data.SECTOR
    ring_page = max(p for p in pages if p in (14, 15))
    at = dump + pages.index(ring_page) * wce_probe_data.PAGE + 2048
    for cut, whole in ((at, False), (dump + (len(pages) - 1) * wce_probe_data.PAGE, True)):
        copy = bytearray(raw)
        copy[cut:dump + len(pages) * wce_probe_data.PAGE] = bytes(dump + len(pages) * wce_probe_data.PAGE - cut)
        path = base / "cut.bin"
        path.write_bytes(copy)
        s = wce_probe_data.read(path)["sessions"][0]
        check(not s["dump_complete"] and (("error" not in s["timing"]) == whole)
              and (whole or "cut off" in s["timing"]["error"]), f"ring after a cut ({whole}): {s.get('timing', {}).get('error')}")
    print("PASS a dump cut inside the ring decodes no ring; one cut after it keeps it")


def timing_checks(s0):
    """The timing reader's ring for session() in tests/test_wce_probe_end_to_end.c."""
    t = s0.get("timing")
    check(t and not t.get("error") and t["ticks_per_ms"] == PER_MS and t["period"] == 125000
          and t["tick_ms"] == 10.0, f"ring header {t}")
    check(t["requests_begun"] == 40 and t["requests_kept"] == 40 and t["open_at_end"], "40 reads, the last open")
    check(t["first_ms"] == 1000 and t["end_ms"] == 9000, "the ring began once CE ran and ended at the end")
    r = t["requests"]
    for i, q in enumerate(r):
        begin, took = scenario_ticks(i)
        check(q["issued_ms"] == 1000 + 100 * i and q["begin_us"] == us(begin) and q["lba"] == 45000 + 64 * i
              and q["sectors"] == 32 + i and q["command"] == "DMAREAD" and q["calls"] == 2
              and q["longest_call_us"] == max(100, 10 * (i + 1)), f"read {i} identity and calls {q}")
        if i < 39:
            check(q["ended"] and q["latency_ms"] == 10 + i and q["latency_us"] == us(took)
                  and q["end_us"] == us(begin + took) and q["call_us"] == 100 + 10 * (i + 1)
                  and q["irq_us"] == 400 and q["overruns"] == i % 3 and q["slow_blocks"] == i % 5
                  and q["failed"] == (i == 7) and not q["replaced"], f"read {i} timing {q}")
        else:
            # Its time and counts are taken when it ends.
            check(not q["ended"] and q["latency_ms"] is None and q["latency_us"] is None and q["end_us"] is None
                  and q["call_us"] is None and q["irq_us"] is None, f"the open read {q}")
    check(r[5]["latency_us"] == r[5]["end_us"] - r[5]["begin_us"] and r[5]["latency_us"] > 25000,
          "read 5 counts the tick CE had not yet counted")
    # Since the ring began: 40 REQUESTs of 1250 ticks, CHECKs of 125 * (i + 1);
    # 80 interrupts of 2500, one of 151 across TMU0's reload and one of 125050
    # across an underflow still pending at its end.
    check(t["call_us"] == us(40 * 1250 + 125 * 820) and t["irq_us"] == us(80 * 2500 + 151 + 125050),
          f"ring totals {t}")


def main():
    fault_reasons()
    config = (ROOT / "config/ffconf.h").read_text()
    marker = "#define FF_USE_MKFS 0"
    assert config.count(marker) == 1
    with tempfile.TemporaryDirectory() as scratch:
        base = Path(scratch)
        header = base / "probe-ffconf.h"
        header.write_text(config.replace(marker, "#define FF_USE_MKFS 1"))
        binaries = {}
        for build in (1, 2):
            binaries[build] = base / f"test-wce-probe-end-to-end-{build}"
            subprocess.run([*shlex.split(os.environ.get("HOST_CC", "cc")), "-std=c11", "-O1", "-g",
                            "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", f"-DKUI_WCE_PROBE={build}",
                            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-include", str(header),
                            "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src/loader"), "-I" + str(fatfs_source()),
                            str(ROOT / "tests/test_wce_probe_end_to_end.c"), str(ROOT / "src/core/wce_probe_fat.c"),
                            str(fatfs_source() / "ff.c"), str(fatfs_source() / "ffunicode.c"),
                            "-o", str(binaries[build])], check=True)
        blank = base / "blank.bin"
        wce_probe_data.blank(blank, 7, "host test")
        for fs, fragment, build in (("exfat", 0, 1), ("fat32", 0, 1), ("exfat", 3, 1), ("fat32", 3, 1),
                                    ("exfat", 0, 2), ("fat32", 3, 2)):
            binary = binaries[build]
            data, ram, info = base / f"data-{fs}-{fragment}.bin", base / "ram.bin", base / "info.json"
            ring = base / "ring.bin"
            subprocess.run([str(binary), fs, str(fragment), str(blank), str(data), str(ram), str(info)]
                           + ([str(ring)] if build == 2 else []), check=True)
            if build == 2:
                wrapped_ring_checks(ring)
                cut_ring_checks(data, base)
            dumps = base / f"dumps-{fs}-{fragment}"
            result = wce_probe_data.read(data, None, dumps)
            sessions = {s["slot"]: s for s in result["sessions"] if "state" in s}
            check(set(sessions) == set(range(7)), f"seven claimed slots, got {sorted(sessions)}")
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
            # The timing session counts from CE's MMU on: not the first INIT.
            check(c["calls"] == (1 if build == 1 else 0) + 80 + 14 + 1 + 8, "call count")
            check(c["functions"] == dict({"REQUEST": 55, "CHECK": 40, "DRIVE": 8}, **({"INIT": 1} if build == 1 else {})),
                  "calls by function")
            check(c["commands"] == {"DMAREAD": 40, "PLAY": 7, "PAUSE": 7}, "accepted commands")
            check(c["refused"] == {"count": 1, "last": "OPEN_TRAY", "commands": ["OPEN_TRAY"]}, "refused commands")
            check(c["reads"] == {"requests": 40, "sectors": sum(32 + i for i in range(40)), "largest": 71}, "reads")
            log = c["audio_log"]
            if build == 1:
                check(c["audio_commands"] == 14 and len(log) == 12 and c["audio_log_dropped"] == 2, "audio log size")
                check([a["first"] for a in log] == [2, 3, 4, 5] + list(range(8, 16)), "audio log keeps first 4, last 8")
                check(log[0]["command"] == "PLAY" and log[1]["command"] == "PAUSE" and log[0]["repeat"] == 15,
                      "audio commands and parameters")
            else:
                check(not c["audio_commands"] and not log, "the timing session keeps no CD audio log")
            m = c["maple"]
            if build == 1:
                check(m["samples"] == c["calls"] and m["frames"] == 4 * m["samples"] and not m["bad_lists"],
                      "Maple sampled on every GD call: samples and frames")
                units = m["units"]
                check(units["A"]["condition"] == m["samples"] and units["A"]["functions"] == "0x01000000", "port A")
                # The 8-bit Maple counters stop at 255, which the decoder shows as "255+".
                capped = m["samples"] if m["samples"] < 255 else "255+"
                check(units["A1"]["block write"] == capped and units["A1"]["other"] == capped
                      and units["A1"]["functions"] == "0x06000000", "A1: storage writes count, LCD writes do not")
                check(units["B2"]["media info"] == capped, "B2 media info")
                check("timing" not in s0 and not s0["stage"]["flags"], "the data session's module keeps no request ring")
            else:
                check(not m["samples"] and not m["units"], "the timing session's module samples no Maple list")
                check(s0["stage"]["flags"] == wce_probe_data.STAGE_TIMING_BUILD, "the stage report marks a timing build")
                timing_checks(s0)
            # The dump: every non-zero page, byte for byte.
            image = Path(s0["dump_file"]).read_bytes()
            original = ram.read_bytes()
            check(hashlib.sha256(image).digest() == hashlib.sha256(original).digest(), "dump reproduces RAM")
            pages = sum(1 for p in range(4096) if any(original[p * 4096:(p + 1) * 4096]))
            check(t["pages"] == pages == s0["dump_pages"] and s0["dump_complete"], "page counts")
            s5, s6 = sessions[5], sessions[6]
            t5 = s5["terminal"]
            check(s5["state"] == "complete" and t5["reason"] == "reader fault" and t5["function"] == "REQUEST"
                  and t5["reason_text"] == "GD REQUEST REJECTED" and s5["dump_complete"], "a fault names its reason")
            check(s6["state"] == "claimed" and s6["terminal"]["reason"] == "A+B+X+Y+Start"
                  and not s6["dump_complete"] and s6["dump_missing_pages"] > 0
                  and Path(s6["dump_file"]).name.endswith("-partial.ram"), "a cut-off dump is flagged")
            check(s1["terminal"] is None or s1["terminal"]["reason"] in ("reader fault", "A+B+X+Y+Start"), "slot 1")
            # The miner on the same file: launches, ends, ports and CD audio.
            mined = {r["slot"]: r for r in wce_probe_mine.mine(data)["launches"]}
            check(set(mined) == set(range(7)) and mined[0]["complete"] and mined[0]["end"] == "A+B+X+Y+Start",
                  "miner: launches and the completed end")
            check(mined[0]["ports"]["saved_to"] == (["A1"] if build == 1 else [])
                  and mined[0]["cd_audio"]["commands"] == {"PLAY": 7, "PAUSE": 7}, "miner: saves and CD audio")
            check("not in the dump" in mined[0]["dump"]["note"] and mined[0]["dump"]["memory"]["dumped_pages"] == pages,
                  "miner: dump without CE structures")
            check(mined[2]["end"].startswith("no end-of-game record"), "miner: stage-only launch")
            check(mined[5]["end"] == "reader fault: GD REQUEST REJECTED in REQUEST" and mined[5]["complete"],
                  "miner: the fault and its reason")
            check(not mined[6]["complete"] and "pages missing" in mined[6]["dump"]["note"], "miner: a cut-off dump")
            if build == 2:
                t = mined[0]["timing"]
                took = sorted(us(scenario_ticks(i)[1]) / 1000 for i in range(39))
                share = {"median": round(took[int(0.5 * 39)], 1), "p90": round(took[int(0.9 * 39)], 1),
                         "max": round(took[-1], 1)}
                gap = us(scenario_ticks(1)[0] - scenario_ticks(0)[0] - scenario_ticks(0)[1]) / 1000
                check(t["requests_kept"] == 40 and t["latency_ms"] == share
                      and t["slowest"][0]["latency_ms"] == share["max"]
                      and t["open_at_end"][0]["pending_ms"] == 9000 - 4900 and t["open_at_end"][0]["kib_s"] is None
                      and t["longest_call_us"] == 400 and t["ring_s"] == 8.0
                      and max(g["gap_ms"] for g in t["gaps"]) == round(gap, 1), f"miner timing {t}")
                report = wce_probe_mine.text(wce_probe_mine.mine(data))
                check("## Read timing (timing session)" in report and "Still open at the end" in report,
                      "miner: the timing section")
            print(f"PASS decode {fs} fragment {fragment} probe {build}: slot 0 complete with {pages} pages, "
                  "slot 1 claimed after a failed dump, slot 2 claimed, slot 3 interrupted, slot 4 next, "
                  "slot 5 a fault, slot 6 cut off; miner agrees")
    return 0


if __name__ == "__main__":
    sys.exit(main())
