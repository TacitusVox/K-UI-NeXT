#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/wce_probe_mine.py on an authored RAM image: the kernel's interrupt
words, a CE3 process array, a module list with a ROM module and a disc DLL
whose name sits in a process slot (reached through the software page walk),
the kernel heap bounds and the aInfo page counters. Also the refusals: a
broken self pointer, a cycle, an unmatched layout and a dump without the
stage's words. No kernel or game bytes."""
from pathlib import Path
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import wce_probe_mine as mine  # noqa: E402

KD = 0x8c145800  # an Armada-like CE3 KData
RAM = 0x8c000000


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL " + message)


class Image:
    def __init__(self):
        self.b = bytearray(0x1000000)

    def put(self, address, value):
        struct.pack_into("<I", self.b, (address & 0x1fffffff) - 0x0c000000, value)

    def text(self, address, s, wide):
        raw = s.encode("utf-16-le") + b"\0\0" if wide else s.encode() + b"\0"
        at = (address & 0x1fffffff) - 0x0c000000
        self.b[at:at + len(raw)] = raw


def build(cycle=False, self_bad=False, class_off=0x8ee0):
    m = Image()
    for i, w in enumerate((KD + 0x340, KD + 0x2a8, KD + 0x84, KD + 0x404)):
        m.put(mine.CE_KERNEL_WORDS + 4 * i, w)
    base = KD + class_off
    names = ["NK.EXE", "filesys.exe", "device.exe", "gwes.exe", "game.exe"]
    for i, n in enumerate(names):
        e = base + 0x9c * i
        m.put(e, i)
        m.put(e + 8, 0x40000 + i)
        m.put(e + 12, (i + 1) << 25)
        m.put(e + 0x20, 0x8c210000 + 0x100 * i)
        m.text(0x8c210000 + 0x100 * i, n, True)
    m.put(KD + 0x90, base + 0x9c * 4)
    heap = 0x7c30 if class_off == 0x8ee0 else 0x4c10
    m.put(KD + heap, 0x8c200000)
    m.put(KD + heap + 4, 0x8c300000)
    m.put(KD + 0x300, base)
    m.put(KD + 0x304, 4096)
    m.put(KD + 0x310, 500)
    m.put(KD + 0x32c, 3000)
    m.put(KD + 0x364, 120)
    # Module list: dsound.dll (ROM, used by slot 4), mapledev.dll (disc DLL,
    # its name in slot 5's space at 0x0a010040), maple.dll (ROM, slot 2).
    nodes = [0x8c220000, 0x8c220200, 0x8c220400]
    m.put(KD + 0x324, nodes[0])
    toc = 0x8c050000
    rom_names = {0: "dsound.dll", 2: "maple.dll"}
    for i, n in enumerate(nodes):
        m.put(n, n if not (self_bad and i == 1) else n + 4)
        m.put(n + 4, nodes[i + 1] if i + 1 < len(nodes) else (nodes[0] if cycle else 0))
        if i in rom_names:
            m.put(n + 100, 3)
            m.put(n + 96, toc + 32 * i)
            m.put(toc + 32 * i + 16, 0x8c060000 + 0x40 * i)
            m.text(0x8c060000 + 0x40 * i, rom_names[i], False)
        else:
            m.put(n + 100, 2)
            m.put(n + 96, 0x1234)
            m.put(n + 8, 0x0a010040)
        m.put(n + 12, 1 << (4 if i == 0 else 4 if i == 1 else 2))
        m.put(n + 16 + 4 * 2, 1 << 16 if i in (0, 1) else 0)  # slot 4 (or 5) count 1
        m.put(n + 16 + 4 * 1, 2 if i == 2 else 0)            # slot 2 count 2
        m.put(n + 80, 0x01e00000)
        m.put(n + 124, 0xb000)
    # Software page walk for 0x0a010040: TTB -> slot table -> block -> page.
    ttb = 0x8c400000
    m.put(ttb + 4 * (0x0a010040 >> 25), 0x8c401000)
    m.put(0x8c401000 + 4 * ((0x0a010040 >> 16) & 0x1ff), 0x8c402000)
    m.put(0x8c402000 + 12 + 4 * ((0x0a010040 >> 12) & 15), 0x0c403000 | 0x9)
    m.text(0x8c403040, "mapledev.dll", True)
    return m, ttb


def main():
    stage = {"zero_begin": "0x8c400000", "zero_end": "0x8ce00000"}
    image, ttb = build()
    pages = sorted({a // 4096 for a in range(0, 0x1000000, 4096) if any(image.b[a:a + 4096])})
    out = mine.analyse(image.b, ttb, stage, pages)
    check(out["kdata"] == f"{KD:#010x}", "KData from the interrupt words")
    procs = out["processes"]
    check(procs and procs["class"] == "CE3" and procs["current_slot"] == 4, "CE3 process array and current slot")
    check([p["name"] for p in procs["processes"]] == ["NK.EXE", "filesys.exe", "device.exe", "gwes.exe", "game.exe"],
          "process names")
    mods = out["modules"]
    check(not mods["error"] and [m["name"] for m in mods["modules"]] == ["dsound.dll", "mapledev.dll", "maple.dll"],
          f"module walk {mods}")
    check(mods["modules"][1]["source"] == "disc/file" and mods["modules"][0]["source"] == "ROM", "module sources")
    w = out["watched_modules"]
    check(w["dsound.dll"]["used_by"] == ["game.exe"] and w["maple.dll"]["used_by"] == ["device.exe"],
          f"module users {w}")
    check(w["mapledev.dll"]["used_by"] == ["game.exe"], "disc DLL users")
    mem = out["memory"]
    check(mem["kernel_pages"]["free_kib"] == 2000 and mem["kernel_pages"]["lowest_free_kib"] == 480 and
          mem["kernel_pages"]["total_kib"] == 12000, f"page counters {mem}")
    check(mem["cleared_range"]["written_by_game"] == 4 and mem["kernel_heap"]["low"] == "0x8c200000",
          f"cleared range and heap {mem}")
    print("PASS miner: KData, CE3 processes, module list with ROM and disc DLLs, page walk, users, memory counters")

    image, ttb = build(cycle=True)
    check(mine.analyse(image.b, ttb, stage, pages)["modules"]["error"].startswith("cycle"), "cycle refused")
    image, ttb = build(self_bad=True)
    m2 = mine.analyse(image.b, ttb, stage, pages)["modules"]
    check(m2["error"].startswith("self pointer") and len(m2["modules"]) == 1, "self pointer refused")
    image, ttb = build(class_off=0x5ec0)
    out = mine.analyse(image.b, ttb, stage, pages)
    check(out["processes"]["class"] == "CE2" and out["memory"]["kernel_pages"]["free_kib"] == 2000 and
          not out["modules"]["error"], "CE2 class chosen from the process array; its heap and counters read")
    image.put(KD + 0x300, KD + 0x8ee0)  # aInfo names another array: counters not used
    out = mine.analyse(image.b, ttb, stage, pages)
    check("not matched" in out["memory"]["kernel_pages"]["layout_check"] and
          "free_kib" not in out["memory"]["kernel_pages"], "aInfo check refuses mismatched counters")
    blank = bytearray(0x1000000)
    out = mine.analyse(blank, 0, stage, [])
    check("kdata" not in out and "not in the dump" in out["note"], "no interrupt words")
    image, ttb = build()
    image.put(KD + 0x8ee0 + 0x9c * 3, 7)                # wrong procnum for slot 3
    check(mine.analyse(image.b, ttb, stage, pages)["processes"] is None, "inconsistent process array refused")
    print("PASS miner refusals: cycle, self pointer, CE2 layout, missing words, inconsistent process array")

    # The library join, from the committed derived indexes only.
    kernels = json.loads(mine.KERNELS.read_text())
    evidence = json.loads(mine.EVIDENCE.read_text())
    sha = "4b01a13620c9882b7d8653223c7a5aae9d5fa9079b14fb3f163694568bc127dd"  # Wild Metal
    k = kernels["kernels"][sha]
    stage = {"kernel_bytes": k["body_bytes"], "kernel_crc32": "0x" + k["body_crc32"],
             "ip": {"product": "T42101N 00", "version": "V1.003"}}
    ident = mine.identify(stage, evidence, kernels)
    check(ident["kernel"]["sha256"] == sha and ident["maple_family"] == 7 and ident["cd_driver_line"] == 2 and
          ident["library"][0]["game"] == "Wild Metal", f"library join {ident}")
    bodies = {(v["body_bytes"], v["body_crc32"]) for v in kernels["kernels"].values()}
    check(len(bodies) == 94, "body size and CRC identify every kernel")
    print("PASS miner library join: body size/CRC names the kernel; IP names the image; families from the scope")

    counters = {"magic_ok": True, "commands": {"PLAY": 3, "REQ_STAT": 40, "STOP": 1},
                "audio_commands": 4, "audio_log": [{"command": "PLAY", "accepted": True, "call": 9, "first": 2, "last": 2, "repeat": 15},
                                                     {"command": "PLAY", "accepted": True, "call": 99, "first": 3, "last": 3, "repeat": 0}],
                "maple": {"units": {"A": {"functions": "0x01000000", "condition": 200},
                                    "A1": {"functions": "0x0e000000", "device info": 3, "block read": 5, "block write": 2},
                                    "A2": {"functions": "0x00000000", "device info": 3}}}}
    p = mine.ports(counters)
    check(p["saved_to"] == ["A1"] and p["polled_slots"] == ["A1", "A2"] and "storage" in p["units"]["A1"]["functions"],
          f"ports {p}")
    a = mine.audio(counters)
    check(a["commands"] == {"PLAY": 3, "STOP": 1} and a["track_ranges"] == [[2, 2], [3, 3]] and
          a["status_polls"] == {"REQ_STAT": 40}, f"audio {a}")
    print("PASS miner counters: saves and polled slots, CD audio commands, track ranges, status polls")

    image, ttb = build()
    launch = {"slot": 0, "state": "complete", "complete": True, "end": "A+B+X+Y+Start", "title": "WILD METAL",
              "area": "U", "product": "T42101N 00", "version": "V1.003", **ident,
              "disc": {"reads": {"requests": 40}, "refused": {"count": 1, "commands": ["OPEN_TRAY"]}},
              "ports": p, "cd_audio": a,
              "dump": mine.analyse(image.b, ttb, {"zero_begin": "0x8c400000", "zero_end": "0x8ce00000"}, pages)}
    report = mine.text({"file": {"slots": 64}, "launches": [launch]})
    for needle in ("| 2 | 1 | 1 | 1 | WILD METAL (U) |", "| 7 | WILD METAL (U) | A1, A2 | A1 | A1 |",
                   "game.exe (ROM)", "OPEN_TRAY (1 times)", "| WILD METAL (U) | 480 KiB | 2000 KiB | NK-CE3-A |",
                   "disc/file, used by game.exe"):
        check(needle in report, f"report shows {needle!r}")
    print("PASS miner report: roadmap rollups for boot, VMU slots, DirectSound, refused commands and memory")


if __name__ == "__main__":
    main()
