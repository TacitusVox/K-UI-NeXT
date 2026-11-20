#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/wce_probe_mine.py on an authored RAM image laid out as the kernels of
the first data session keep it: the kernel's interrupt words, a CE3 process
array with 8-bit names (one UTF-16 name for the fallback), a module list with
a ROM module and a disc DLL whose 8-bit name sits in a process slot (reached
through the software page walk), the kernel heap bounds, the aInfo page
counters with no low-water mark (KData+0x364 = 0), KData's own address at
+0x334 and a ROMHDR at pTOC (+0x330). Also the refusals: a broken self
pointer, a cycle, an unmatched layout, an implausible low-water mark or
ROMHDR, a KData self-check mismatch (recorded, not fatal) and a dump without
the stage's words; the Maple write evidence and the decoder's saturated
8-bit counters. No kernel or game bytes."""
from pathlib import Path
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import wce_probe_data as wd  # noqa: E402
import wce_probe_mine as mine  # noqa: E402

KD = 0x8c145800  # an Armada-like CE3 KData
RAM = 0x8c000000
ROMHDR = 0x8c140000  # pTOC: a 2-module image 0x8c010000..0x8c1400a4
UTF16_PROCESS = 3  # gwes.exe keeps a UTF-16 name: the fallback


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


def build(cycle=False, self_bad=False, class_off=0x8ee0, lowest=0):
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
        m.text(0x8c210000 + 0x100 * i, n, i == UTF16_PROCESS)
    m.put(KD + 0x90, base + 0x9c * 4)
    heap = 0x7c30 if class_off == 0x8ee0 else 0x4c10
    m.put(KD + heap, 0x8c200000)
    m.put(KD + heap + 4, 0x8c300000)
    m.put(KD + 0x300, base)
    m.put(KD + 0x304, 4096)
    m.put(KD + 0x310, 500)
    m.put(KD + 0x32c, 3000)
    m.put(KD + 0x364, lowest)  # KINX_MINPAGEFREE: 0 on all 17 kernels of the first session
    m.put(KD + 0x334, KD)      # KINX_KDATA_ADDR
    m.put(KD + 0x330, ROMHDR)  # KINX_PTOC
    # ROMHDR: dllfirst, dlllast, physfirst, physlast (the header, 2 TOC
    # entries and a copy entry end the image), nummods, RAM start/free/end.
    for i, w in enumerate((0x01e00000, 0x02000000, 0x8c010000, ROMHDR + 84 + 2 * 32 + 16, 2,
                           0x8c141000, 0x8c150000, 0x8cfd0000)):
        m.put(ROMHDR + 4 * i, w)
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
    m.text(0x8c403040, "mapledev.dll", False)  # the 8-bit export name in the DLL's image
    return m, ttb


def main():
    stage = {"zero_begin": "0x8c400000", "zero_end": "0x8ce00000"}
    image, ttb = build()
    pages = sorted({a // 4096 for a in range(0, 0x1000000, 4096) if any(image.b[a:a + 4096])})
    out = mine.analyse(image.b, ttb, stage, pages)
    check(out["kdata"] == f"{KD:#010x}", "KData from the interrupt words")
    check(out["kdata_self_check"] == {"value": f"{KD:#010x}", "matches": True, "unreviewed": True},
          "KData+0x334 names KData")
    procs = out["processes"]
    check(procs and procs["class"] == "CE3" and procs["current_slot"] == 4, "CE3 process array and current slot")
    check([p["name"] for p in procs["processes"]] == ["NK.EXE", "filesys.exe", "device.exe", "gwes.exe", "game.exe"],
          "process names: 8-bit, and the UTF-16 one whole (an 8-bit read stops after its first character)")
    check(mine.Ram(image.b).string(0x8c210000 + 0x100 * UTF16_PROCESS, False) == "g",
          "the UTF-16 name reads as one character 8-bit, so its form is chosen first")
    # A UTF-16 name that does not read as one gives no name, never its first
    # character; an 8-bit name is read only as 8-bit.
    odd = mine.Ram(bytearray(0x1000000))
    odd.image[0x210000:0x210000 + 10] = "gaméx".encode("utf-16-le")
    odd.image[0x210100:0x210100 + 6] = b"XY\0Z\0\0"[:6]
    check(mine.name_at(odd, 0x8c210000) is None and mine.name_at(odd, 0x8c210100) == "XY",
          "an invalid UTF-16 name has no name; an 8-bit name stays 8-bit")
    mods = out["modules"]
    check(not mods["error"] and [m["name"] for m in mods["modules"]] == ["dsound.dll", "mapledev.dll", "maple.dll"],
          f"module walk {mods}")
    check(mods["modules"][1]["source"] == "disc/file" and mods["modules"][0]["source"] == "ROM", "module sources")
    w = out["watched_modules"]
    check(w["dsound.dll"]["used_by"] == ["game.exe"] and w["maple.dll"]["used_by"] == ["device.exe"],
          f"module users {w}")
    check(w["mapledev.dll"]["used_by"] == ["game.exe"], "disc DLL users")
    mem = out["memory"]
    check(mem["kernel_pages"]["free_kib"] == 2000 and "lowest_free_kib" not in mem["kernel_pages"] and
          mem["kernel_pages"]["total_kib"] == 12000, f"page counters {mem}")
    check(mem["kernel_pages"]["lowest_free"] == "not kept by this kernel (KData+0x364 = 0)",
          "a zero low-water mark is labelled, not published")
    check(mem["cleared_ranges"][0]["written_by_game"] == 4 and mem["cleared_written_kib"] == 16 and
          mem["kernel_heap"]["low"] == "0x8c200000", f"cleared range and heap {mem}")
    check(out["rom_header"] == {"source": "KData+0x330", "unreviewed": True, "address": f"{ROMHDR:#010x}", "modules": 2,
                                "ram_start": "0x8c141000", "ram_free": "0x8c150000", "ram_end": "0x8cfd0000"},
          f"ROMHDR from KData+0x330 when the stage report has none {out['rom_header']}")
    reported = dict(stage, rom_header=f"{ROMHDR:#010x}", ram_start="0x8c141000", ram_free="0x8c150000",
                    ram_end="0x8cfd0000")
    rom = mine.analyse(image.b, ttb, reported, pages)["rom_header"]
    check(rom["source"] == "stage report" and rom["ram_free"] == "0x8c150000" and rom["kdata_ptoc_agrees"],
          f"the stage report's ROMHDR, and KData+0x330 agrees {rom}")
    rom = mine.analyse(image.b, ttb, dict(reported, ram_free="0x8c151000"), pages)["rom_header"]
    check(rom["source"] == "stage report" and rom["ram_free"] == "0x8c151000" and not rom["kdata_ptoc_agrees"],
          "the stage report stays the source; a disagreeing KData+0x330 is recorded")
    print("PASS miner: KData and its self-check, CE3 processes with 8-bit and UTF-16 names, module list with ROM "
          "and disc DLLs, page walk, users, memory counters without a low-water mark, ROMHDR from either source")

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
    for lowest, kib in ((120, 480), (500, 2000), (501, None)):  # 500 pages free
        image, ttb = build(lowest=lowest)
        k = mine.analyse(image.b, ttb, stage, pages)["memory"]["kernel_pages"]
        check(k.get("lowest_free_kib") == kib and ("lowest_free" in k) == (kib is None),
              f"a low-water mark of {lowest} pages is published only up to the free pages: {k}")
    image, ttb = build()
    image.put(KD + 0x334, KD + 4)
    out = mine.analyse(image.b, ttb, stage, pages)
    check(out["kdata_self_check"] == {"value": f"{KD + 4:#010x}", "matches": False, "unreviewed": True} and out["processes"] and
          not out["modules"]["error"] and out["memory"]["kernel_pages"]["free_kib"] == 2000,
          "a KData+0x334 mismatch is recorded, and the kernel structures are still read")
    for at, value, what in ((24, 0x8cfe0000, "RAM free above RAM end"), (16, 0, "no modules"),
                            (16, 200, "more TOC entries than the image holds"),
                            (8, ROMHDR + 4, "the header outside its image")):
        image, ttb = build()
        image.put(ROMHDR + at, value)
        rom = mine.analyse(image.b, ttb, stage, pages)["rom_header"]
        check(rom["source"] is None and "not a consistent ROMHDR" in rom["note"], f"ROMHDR refused: {what} {rom}")
    image, ttb = build()
    image.put(KD + 0x330, ROMHDR & 0x1fffffff)          # pTOC outside P1 RAM
    check(mine.analyse(image.b, ttb, stage, pages)["rom_header"]["source"] is None, "ROMHDR refused: pTOC outside RAM")
    print("PASS miner refusals: cycle, self pointer, CE2 layout, missing words, inconsistent process array, "
          "low-water marks above the free pages, inconsistent ROMHDRs; a KData self-check mismatch is recorded")

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
                "maple": {"units": {"A": {"functions": "0x01000000", "condition": "255+"},
                                    "A1": {"functions": "0x0e000000", "device info": 3, "block read": 5, "block write": 2},
                                    "A2": {"functions": "0x02000000", "device info": 3, "write complete": "255+"}}}}
    p = mine.ports(counters)
    check(p["saved_to"] == ["A1"] and p["polled_slots"] == ["A1", "A2"] and "storage" in p["units"]["A1"]["functions"],
          f"ports {p}")
    check(p["completion_frames"] == ["A2"] and p["addressed_slots"] == ["A1", "A2"] and p["read_from"] == ["A1"] and
          p["units"]["A2"]["write_complete"] == "255+", f"a (saturated) completion frame is card activity, not a save {p}")
    q = mine.ports({"maple": {"units": {"B1": {"functions": "0x04000000", "media info": 3, "other": 9},
                                        "B2": {"functions": "0x00000000", "device info": 3}}}})
    check(q["polled_slots"] == ["B2"] and not q["addressed_slots"] and not q["completion_frames"] and not q["saved_to"],
          f"an LCD frame or a device-info poll addresses no card {q}")
    # The decoder marks a saturated 8-bit Maple counter "255+", as it marks the 16-bit ones "65535+".
    raw = bytearray(520)
    struct.pack_into("<I", raw, 0, 0x43454357)
    at = wd.COUNTERS_HEAD.size + (wd.AUDIO_FIRST + wd.AUDIO_RING) * wd.AUDIO.size
    raw[at + 2] = 255                                    # A: condition
    raw[at + 8 + 4], raw[at + 8 + 6] = 7, 255            # A1: block read, write complete
    struct.pack_into("<I", raw, at + 96 + 4, 0x02000000)  # A1: storage
    units = wd.decode_counters(bytes(raw))["maple"]["units"]
    check(units == {"A": {"functions": "0x00000000", "condition": "255+"},
                    "A1": {"functions": "0x02000000", "block read": 7, "write complete": "255+"}},
          f"saturated Maple counters {units}")
    p8 = mine.ports({"maple": {"units": units}})
    check(p8["completion_frames"] == ["A1"] and p8["addressed_slots"] == ["A1"] and not p8["saved_to"],
          f"the miner reads the decoder's 255+ as a count seen {p8}")
    a = mine.audio(counters)
    check(a["commands"] == {"PLAY": 3, "STOP": 1} and a["track_ranges"] == [[2, 2], [3, 3]] and
          a["status_polls"] == {"REQ_STAT": 40}, f"audio {a}")
    print("PASS miner counters: saves, completion frames, card slots addressed and polled, saturated 8-bit Maple "
          "counters, CD audio commands, track ranges, status polls")

    image, ttb = build()
    launch = {"slot": 0, "state": "complete", "complete": True, "end": "A+B+X+Y+Start", "title": "WILD METAL",
              "area": "U", "product": "T42101N 00", "version": "V1.003", **ident,
              "disc": {"reads": {"requests": 40}, "refused": {"count": 1, "commands": ["OPEN_TRAY"]}},
              "ports": p, "cd_audio": a,
              "dump": mine.analyse(image.b, ttb, {"zero_begin": "0x8c400000", "zero_end": "0x8ce00000"}, pages)}
    # A reader fault leaves a terminal report and a complete dump: not an end-of-game record.
    fault = {"slot": 1, "state": "complete", "complete": True, "title": "KISS", "area": "U", "cd_driver_line": 2,
             "end": "reader fault: GD FUNCTION UNSUPPORTED in DMA_TRANSFER", "disc": {"reads": {"requests": 41}}}
    unended = {"slot": 2, "state": "claimed", "title": "QUIET", "area": "J", "cd_driver_line": 2, "end": mine.NO_END}
    report = mine.text({"file": {"slots": 64}, "launches": [launch, fault, unended]})
    for needle in ("| Line | Launches | Ended by A+B+X+Y+Start | Reader fault | No end record | Read the disc after start |",
                   "| 2 | 3 | 1 | 1 | 1 | 2 | KISS (U), QUIET (J), WILD METAL (U) |",
                   "| Family | Game | Card slots addressed | Reads from | Saved to | Completion frames |",
                   "| 7 | WILD METAL (U) | A1, A2 | A1 | A1 | A2 |",
                   "samples the Maple list only during GD calls", "is not proof of absence",
                   "card activity, not a save", "summed over the sampled lists",
                   "| End | Saved to (block write seen) | Card slots addressed |", "| A+B+X+Y+Start | A1 | A1, A2 |",
                   "| game.exe | 2000 KiB |", "game.exe (ROM)", "OPEN_TRAY (1 times)",
                   "| WILD METAL (U) | not kept by this kernel | 2000 KiB | NK-CE3-A |", "disc/file, used by game.exe"):
        check(needle in report, f"report shows {needle!r}")
    print("PASS miner report: roadmap rollups for boot (combination, fault, no record), card slots, saves and "
          "completion frames, DirectSound, refused commands and memory without a low-water mark")


if __name__ == "__main__":
    main()
