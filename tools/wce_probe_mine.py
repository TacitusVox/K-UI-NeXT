#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Per-game findings from a filled WinCE data file (docs/wince-data-probe.md).

usage:
  wce_probe_mine.py DATA [--out DIR] [--evidence JSON] [--kernels JSON]

Reads WCEDATA.BIN with tools/wce_probe_data.py, rebuilds each slot's RAM dump
in memory and reports, per launch:
  - the game (IP.BIN) joined to the library scope: kernel, VMU family, sound
    group and CD-driver line (docs/evidence/wince-universal-scope-*.json);
  - how the launch ended, disc use, refused commands and CD audio commands;
  - controller ports: which port and slot the game polls, reads and writes;
  - from the RAM dump: the Windows CE process list, the module list with the
    processes that use each module (DirectSound, maple, mapledev), the kernel
    heap bounds and the kernel's own free-page counters.
Writes DIR/findings.json and DIR/findings.md. Reads the owner's file only.

Kernel structures are read only through offsets with this evidence:
  - KData from the stage's interrupt words in the dump (94/94 kernels:
    pending = KData+0x340, ring = +0x2a8, resched = +0x84);
  - module list head KData+0x324, current process +0x90, process array
    +0x8ee0 (CE3) or +0x5ec0 (CE2), stride 0x9c, and the module fields
    self +0, next +4, use bits +12, reference counts +16, base +80,
    TOC/file +96, kind +100, size +124 (include/kui/armada_ce_live_sound.h);
  - CE's software page walk for virtual addresses (include/kui/armada_ce_ram_bind.h).
Process names (+0x20), module names of disc DLLs (+8) and the KData aInfo
memory counters (free pages +0x310, total +0x32c, lowest free +0x364) follow
the standard CE layout and are labelled unreviewed; each is used only when
the reviewed fields around it check out.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wce_probe_data as wd  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/evidence/wince-universal-scope-2026-11-17.json"
KERNELS = ROOT / "docs/evidence/wince-kernel-index-2026-11-17.json"
CE_KERNEL_WORDS = 0x8c008324
OFFSETS = {"CE3": {"process_array": 0x8ee0, "heap_low": 0x7c30, "heap_high": 0x7c34},
           "CE2": {"process_array": 0x5ec0, "heap_low": 0x4c10, "heap_high": 0x4c14}}
MODULE_HEAD, CURRENT_PROCESS, PROCESS_STRIDE = 0x324, 0x90, 0x9c
INFO_PROCARRAY, INFO_PAGESIZE, INFO_PAGEFREE, INFO_NUMPAGES, INFO_MINPAGEFREE = 0x300, 0x304, 0x310, 0x32c, 0x364
WATCH = ("dsound.dll", "sndcore.dll", "maple.dll", "mapledev.dll", "wsegacd.dll", "ddraw.dll",
         "dinput.dll", "coredll.dll")


class Ram:
    """The console's 16 MiB as dumped; zero pages were not written and are zero."""

    def __init__(self, image, ttb=0):
        self.image, self.ttb = image, ttb

    def offset(self, address):
        physical = address & 0x1fffffff
        if address >= 0x80000000 and (address & 0xe0000000) in (0x80000000, 0xa0000000) and \
                0x0c000000 <= physical < 0x0d000000:
            return physical - 0x0c000000
        return None

    def word(self, address):
        if address & 3:
            return None
        at = self.offset(address)
        if at is None or at + 4 > len(self.image):
            return None
        return struct.unpack_from("<I", self.image, at)[0]

    def byte(self, address):
        at = self.offset(address)
        return None if at is None else self.image[at]

    def translate(self, va):
        """CE's software walk (include/kui/armada_ce_ram_bind.h): P1 tables only."""
        if va >= 0x80000000:
            return va if self.offset(va) is not None else None
        if not self.ttb or va < 0x02000000:
            return None
        slot = self.word(self.ttb + 4 * (va >> 25))
        if not slot or (slot & 0xe0000000) != 0x80000000:
            return None
        block = self.word(slot + 4 * ((va >> 16) & 0x1ff))
        if not block or not block & 0x80000000 or (block & 0xe0000000) != 0x80000000:
            return None
        page = self.word(block + 12 + 4 * ((va >> 12) & 15))
        if page is None or not page & 1:
            return None
        physical = (page & 0xfffff000) | (va & 0xfff) | 0x80000000
        return physical if self.offset(physical) is not None else None

    def vword(self, va):
        p = self.translate(va)
        return None if p is None else self.word(p)

    def string(self, address, wide, limit=64):
        out = []
        for i in range(limit):
            step = 2 if wide else 1
            p = self.translate(address + i * step)
            if p is None:
                return None
            at = self.offset(p)
            c = struct.unpack_from("<H", self.image, at)[0] if wide else self.image[at]
            if c == 0:
                break
            if c < 0x20 or c > 0x7e:
                return None
            out.append(chr(c))
        else:
            return None
        return "".join(out) or None


def kdata_from(ram):
    words = [ram.word(CE_KERNEL_WORDS + 4 * i) for i in range(4)]
    if None in words or not words[0]:
        return None, words
    kd = words[0] - 0x340
    if words[1] - kd != 0x2a8 or words[2] - kd != 0x84 or (words[3] and words[3] - kd != 0x404):
        return None, words
    return kd, words


def processes(ram, kd):
    """Pick the build class whose process array holds this kernel's slots."""
    best = None
    for cls, off in OFFSETS.items():
        base = kd + off["process_array"]
        rows, bad = [], 0
        for i in range(32):
            e = base + PROCESS_STRIDE * i
            vm = ram.word(e + 12)
            num = ram.byte(e)
            if vm is None or num is None:
                bad += 1
                continue
            if not vm:
                continue
            if num != i or vm != (i + 1) << 25:
                bad += 1
                continue
            rows.append({"slot": i, "handle": f"{ram.word(e + 8):#010x}", "vm_base": f"{vm:#010x}",
                         "name": ram.string(ram.word(e + 0x20) or 0, True) if ram.word(e + 0x20) else None,
                         "process": e})
        current = ram.word(kd + CURRENT_PROCESS)
        current_ok = current is not None and base <= current < base + 32 * PROCESS_STRIDE and \
            (current - base) % PROCESS_STRIDE == 0
        score = (len(rows) if not bad else -bad) + (2 if current_ok else 0)
        if rows and not bad and (best is None or score > best[0]):
            best = (score, cls, base, rows, current)
    if not best:
        return None
    _, cls, base, rows, current = best
    current_slot = (current - base) // PROCESS_STRIDE if current is not None and \
        base <= current < base + 32 * PROCESS_STRIDE else None
    for r in rows:
        r.pop("process")
    return {"class": cls, "array": f"{base:#010x}", "current_slot": current_slot, "processes": rows}


def modules(ram, kd, low=None, high=None):
    head = ram.word(kd + MODULE_HEAD)
    out, seen, node, error = [], set(), head, None
    while node:
        if node in seen:
            error = f"cycle at {node:#010x}"
            break
        if len(out) >= 160:
            error = "more than 160 modules"
            break
        seen.add(node)
        if ram.word(node) != node:
            error = f"self pointer mismatch at {node:#010x}"
            break
        if low is not None and high is not None and not (low <= node < high):
            error = f"node {node:#010x} outside the kernel heap"
            break
        kind = (ram.word(node + 100) or 0) & 255
        toc = ram.word(node + 96) or 0
        name = None
        if kind == 3 and toc:
            pointer = ram.word(toc + 16)
            name = ram.string(pointer, False) if pointer else None
            source = "ROM"
        else:
            pointer = ram.word(node + 8)
            name = ram.string(pointer, True) if pointer else None
            source = "disc/file" if kind == 2 else f"kind {kind}"
        use = ram.word(node + 12) or 0
        refs = []
        for i in range(32):
            w = ram.word(node + 16 + 4 * (i // 2)) or 0
            n = (w >> (16 * (i & 1))) & 0xffff
            if n:
                refs.append({"slot": i, "references": n})
        out.append({"node": f"{node:#010x}", "name": name, "source": source,
                     "base": f"{ram.word(node + 80) or 0:#010x}", "bytes": ram.word(node + 124) or 0,
                     "used_by": [i for i in range(32) if use >> i & 1], "references": refs})
        node = ram.word(node + 4)
        if node is None:
            error = "next pointer outside RAM"
            break
    return {"head": f"{head or 0:#010x}", "modules": out, "error": error}


def memory(ram, kd, cls, stage, page_list):
    info = {}
    if kd and cls:
        low, high = ram.word(kd + OFFSETS[cls]["heap_low"]), ram.word(kd + OFFSETS[cls]["heap_high"])
        info["kernel_heap"] = {"low": f"{low or 0:#010x}", "high": f"{high or 0:#010x}"}
        size = ram.word(kd + INFO_PAGESIZE)
        procarray = ram.word(kd + INFO_PROCARRAY)
        valid = size == 4096 and procarray == kd + OFFSETS[cls]["process_array"]
        free, total, lowest = (ram.word(kd + x) for x in (INFO_PAGEFREE, INFO_NUMPAGES, INFO_MINPAGEFREE))
        info["kernel_pages"] = {
            "layout_check": "page size 4096 and process array match" if valid else "not matched; values not used",
            "unreviewed": True}
        if valid:
            info["kernel_pages"].update({"free_kib": (free or 0) * 4, "total_kib": (total or 0) * 4,
                                         "lowest_free_kib": (lowest or 0) * 4})
    pages = set(page_list)
    info["dumped_pages"] = len(pages)
    if stage:
        # The stage cleared these ranges before Windows CE started: a non-zero
        # page there was written by CE or the game. Other dumped pages of CE's
        # RAM may still hold launcher bytes (the stage image, its stack area).
        cleared, total, used = [], 0, 0
        for b_key, e_key in (("zero_begin", "zero_end"), ("zero_high_begin", "zero_high_end")):
            zb, ze = int(stage.get(b_key, "0x0"), 16), int(stage.get(e_key, "0x0"), 16)
            if ze <= zb:
                continue
            first, last = (zb - wd.RAM_BEGIN) // wd.PAGE, (ze - wd.RAM_BEGIN) // wd.PAGE
            n = sum(1 for p in range(first, last) if p in pages)
            cleared.append({"begin": f"{zb:#010x}", "end": f"{ze:#010x}", "pages": last - first,
                            "written_by_game": n})
            total += last - first
            used += n
        if cleared:
            inside = set()
            for c in cleared:
                f0 = (int(c["begin"], 16) - wd.RAM_BEGIN) // wd.PAGE
                inside.update(range(f0, f0 + c["pages"]))
            body_end = 0x8c010000 + (int(stage.get("kernel_bytes", 0)) + 0xfff) // 0x1000 * 0x1000
            first_ce = (body_end - wd.RAM_BEGIN) // wd.PAGE
            info["cleared_ranges"] = cleared
            info["cleared_written_kib"] = used * 4
            info["cleared_untouched_kib"] = (total - used) * 4
            info["not_cleared_dumped_kib"] = sum(4 for p in pages if p >= first_ce and p not in inside)
    if pages:
        info["highest_page"] = f"{wd.RAM_BEGIN + max(pages) * wd.PAGE:#010x}"
    return info


def ports(counters):
    units = (counters or {}).get("maple", {}).get("units", {})
    out = {}
    for unit, row in units.items():
        label = unit
        fn = int(row.get("functions", "0x0"), 16)
        kinds = []
        if fn & 0x01000000:
            kinds.append("controller")
        if fn & 0x02000000:
            kinds.append("storage")
        if fn & 0x04000000:
            kinds.append("LCD")
        if fn & 0x08000000:
            kinds.append("clock")
        if fn & 0x00010000:
            kinds.append("vibration")
        out[label] = {"functions": kinds, "device_info": row.get("device info", 0),
                      "condition": row.get("condition", 0), "media_info": row.get("media info", 0),
                      "block_reads": row.get("block read", 0), "block_writes": row.get("block write", 0),
                      "write_complete": row.get("write complete", 0)}
    saves = [u for u, r in out.items() if r["block_writes"]]
    reads = [u for u, r in out.items() if r["block_reads"]]
    polled = [u for u, r in out.items() if r["device_info"] and len(u) == 2]
    # Positive evidence only: the probe reads the Maple list during GD calls,
    # so a frame it did not see may still have happened.
    samples = (counters or {}).get("maple", {}).get("samples", 0)
    return {"units": out, "saved_to": saves, "read_from": reads, "polled_slots": polled, "samples": samples}


def audio(counters):
    if not counters or not counters.get("magic_ok"):
        return None
    cmd = counters.get("commands", {})
    plays = [a for a in counters.get("audio_log", []) if a["command"] in ("PLAY", "PLAY2") and a["accepted"]]
    tracks = sorted({(a["first"], a["last"]) for a in plays if a["command"] == "PLAY"})
    return {"commands": {k: cmd[k] for k in ("PLAY", "PLAY2", "PAUSE", "RELEASE", "STOP") if k in cmd},
            "status_polls": {k: cmd[k] for k in ("REQ_STAT", "GETSCD", "GETTOC", "GETTOC2", "GETSES") if k in cmd},
            "logged": counters.get("audio_commands", 0), "track_ranges": [list(t) for t in tracks],
            "log": counters.get("audio_log", [])}


def load_json(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError):
        return None


def identify(stage, evidence, kernels):
    out = {}
    if not stage:
        return out
    by_body = {}
    if kernels:
        for sha, k in kernels["kernels"].items():
            by_body[(k.get("body_bytes"), k.get("body_crc32"))] = (sha, k)
    hit = by_body.get((stage["kernel_bytes"], stage["kernel_crc32"][2:]))
    if hit:
        sha, k = hit
        out["kernel"] = {"sha256": sha, "class": k["nk_class"], "kdata": k["kdata"], "ram_end": k["ram_end"]}
    if evidence:
        ek = {k["kernel_sha256"]: k for k in evidence["kernels"]}
        if hit and hit[0] in ek:
            k = ek[hit[0]]
            out.update({"maple_family": k["maple_family"], "cd_driver_line": k["cd_driver_line"],
                        "sound_group": k["sound_group"], "sound_adapter": k["sound_adapter"],
                        "mapledev_in_rom": k["mapledev_in_rom"], "bridge_tail": k["bridge_tail"]})
        ip = stage.get("ip", {})
        images = [i for i in evidence["images"] if i["product"].replace(" ", "") == ip.get("product", "").replace(" ", "")
                  and i["version"] == ip.get("version") and (not hit or i["kernel"] == hit[0][:12])]
        if images:
            out["library"] = [{"id": i["id"], "game": i["game"], "region": i["region"], "disc": i["disc"],
                               "format": i["format"], "tracks": i["tracks"], "audio_tracks_30s": i["audio_tracks_30s"]}
                              for i in images]
    return out


def mine(path, evidence_path=EVIDENCE, kernels_path=KERNELS):
    evidence, kernels = load_json(evidence_path), load_json(kernels_path)
    decoded = wd.read(path)
    rows = []
    with open(path, "rb") as f:
        for session in decoded["sessions"]:
            if "state" not in session:
                rows.append({"slot": session["slot"], "error": session.get("error")})
                continue
            stage, terminal = session.get("stage"), session.get("terminal")
            ip = (stage or {}).get("ip", {})
            row = {"slot": session["slot"], "sequence": session["sequence"], "state": session["state"],
                   "title": ip.get("title"), "product": ip.get("product"), "version": ip.get("version"),
                   "area": ip.get("area"), "module_build": (stage or {}).get("build"),
                   "probe_armed": bool((stage or {}).get("flags", 0) & 1)}
            row.update(identify(stage, evidence, kernels))
            if stage:
                row["kernel_loaded"] = {"bytes": stage["kernel_bytes"], "crc32": stage["kernel_crc32"],
                                        "rom_header": stage["rom_header"], "map_slots": stage["map_slots"],
                                        "tracks": stage["track_count"], "extents": stage["extent_count"]}
            if not terminal:
                row["end"] = "no end-of-game record (combination not reached, power off, or probe not armed)"
                rows.append(row)
                continue
            counters = terminal.get("counters") or {}
            row["end"] = terminal["reason"] + (f": {terminal['reason_text']}" if terminal.get("reason_text") else "") + \
                (f" in {terminal['function']}" if terminal.get("function") else "")
            row["complete"] = bool(session.get("dump_complete"))
            row["disc"] = {"calls": counters.get("calls"), "reads": counters.get("reads"),
                           "commands": counters.get("commands"), "refused": counters.get("refused"),
                           "unknown_commands": counters.get("unknown_commands"),
                           "hook_fault": terminal.get("hook_fault")}
            row["cd_audio"] = audio(counters)
            row["ports"] = ports(counters)
            row["rtc"] = counters.get("rtc")
            base = (decoded["file"]["first_slot"] + session["slot"] * decoded["file"]["slot_sectors"]) * wd.SECTOR
            f.seek(base + wd.TERMINAL_SECTOR * wd.SECTOR)
            page_list = wd.page_map(f.read(wd.TERMINAL_SECTORS * wd.SECTOR))
            written = min(len(page_list), terminal["pages"]) if session.get("dump_complete") else 0
            image = bytearray(wd.RAM_BYTES)
            f.seek(base + wd.DUMP_SECTOR * wd.SECTOR)
            for p in page_list[:written]:
                image[p * wd.PAGE:(p + 1) * wd.PAGE] = f.read(wd.PAGE)
            ttb = int(terminal["registers"]["TTB"], 16)
            row["dump"] = analyse(image, ttb, stage, page_list[:written]) if written else \
                {"note": f"no complete dump in this slot ({session.get('dump_missing_pages', 0)} pages missing)"}
            rows.append(row)
    return {"file": decoded["file"], "launches": rows}


def analyse(image, ttb, stage, page_list):
    ram = Ram(image, ttb)
    kd, words = kdata_from(ram)
    out = {"kernel_words": [f"{w or 0:#010x}" for w in words]}
    if kd is None:
        out["note"] = "the stage's interrupt words are not in the dump: kernel structures not read"
        out["memory"] = memory(ram, None, None, stage, page_list)
        return out
    out["kdata"] = f"{kd:#010x}"
    procs = processes(ram, kd)
    cls = procs["class"] if procs else None
    out["processes"] = procs
    low = high = None
    if cls:
        low, high = ram.word(kd + OFFSETS[cls]["heap_low"]), ram.word(kd + OFFSETS[cls]["heap_high"])
        # Bound the module walk by the kernel heap only when both words look like one.
        if not (low and high and ram.offset(low) is not None and ram.offset(high - 4) is not None and low < high):
            low = high = None
    mods = modules(ram, kd, low, high)
    out["modules"] = mods
    names = {p["slot"]: p.get("name") or f"slot {p['slot']}" for p in (procs or {}).get("processes", [])}
    watch = {}
    for m in mods["modules"]:
        if m["name"] and m["name"].lower() in WATCH:
            watch[m["name"].lower()] = {"source": m["source"], "used_by": [names.get(i, f"slot {i}") for i in m["used_by"]]}
    out["watched_modules"] = watch
    out["memory"] = memory(ram, kd, cls, stage, page_list)
    return out


def rollup(launches):
    """Roadmap answers across launches (docs/wince-roadmap.md, 'What the data session answers')."""
    def name(r):
        return f"{r.get('title') or '?'} ({r.get('area') or '?'})"
    out = ["## Roadmap answers", ""]
    out += ["### Boot by CD-driver line (B0)", "", "| Line | Launches | End-of-game record | Read the disc after start | Games |",
            "|---|---|---|---|---|"]
    lines = {}
    for r in launches:
        lines.setdefault(r.get("cd_driver_line", "?"), []).append(r)
    for line in sorted(lines, key=str):
        rs = lines[line]
        ended = [r for r in rs if r.get("complete")]
        read = [r for r in rs if ((r.get("disc") or {}).get("reads") or {}).get("requests")]
        out.append(f"| {line} | {len(rs)} | {len(ended)} | {len(read)} | {', '.join(sorted({name(r) for r in rs}))} |")
    out += ["", "### Virtual VMU: slots and saves by maple.dll family (S1, S5-S9, S16)", "",
            "Frames are sampled during GD calls: a slot listed is evidence; \"not seen\" is not proof of absence.", "",
            "| Family | Game | Polls | Reads from | Saves to |", "|---|---|---|---|---|"]
    for r in sorted(launches, key=lambda r: (str(r.get("maple_family", "?")), name(r))):
        p = r.get("ports") or {}
        if not p:
            continue
        unseen = f"not seen ({p.get('samples', 0)} samples)"
        out.append(f"| {r.get('maple_family', '?')} | {name(r)} | {', '.join(p.get('polled_slots', [])) or unseen} | "
                   f"{', '.join(p.get('read_from', [])) or unseen} | {', '.join(p.get('saved_to', [])) or unseen} |")
    out += ["", "### DirectSound by sound group (S3, S11, S15, R1)", "",
            "| Sound group | Game | dsound.dll | sndcore.dll | CD audio commands |", "|---|---|---|---|---|"]
    for r in sorted(launches, key=lambda r: (str(r.get("sound_group", "?")), name(r))):
        w = (r.get("dump") or {}).get("watched_modules")
        if w is None:
            continue
        def users(m):
            return ", ".join(w[m]["used_by"]) + f" ({w[m]['source']})" if m in w else "not loaded"
        cd = (r.get("cd_audio") or {}).get("commands") or {}
        out.append(f"| {r.get('sound_group', '?')} | {name(r)} | {users('dsound.dll')} | {users('sndcore.dll')} | "
                   f"{', '.join(f'{k} {v}' for k, v in cd.items()) or '-'} |")
    out += ["", "### Where maple and mapledev live (S16, S17)", "", "| Game | maple.dll | mapledev.dll |", "|---|---|---|"]
    for r in sorted(launches, key=name):
        w = (r.get("dump") or {}).get("watched_modules")
        if w is None:
            continue
        cell = lambda m: f"{w[m]['source']}, used by {', '.join(w[m]['used_by']) or 'none'}" if m in w else "not loaded"
        out.append(f"| {name(r)} | {cell('maple.dll')} | {cell('mapledev.dll')} |")
    refused = [r for r in launches if ((r.get("disc") or {}).get("refused") or {}).get("count")]
    out += ["", "### Refused commands (B1)", ""]
    out += [f"- {name(r)}: {', '.join(r['disc']['refused']['commands'])} ({r['disc']['refused']['count']} times)"
            for r in refused] or ["- none"]
    out += ["", "### Memory left (F5 and every CD music step)", "", "| Game | Lowest free | Free at the end | Kernel class |",
            "|---|---|---|---|"]
    def lowest(r):
        k = ((r.get("dump") or {}).get("memory") or {}).get("kernel_pages") or {}
        return k.get("lowest_free_kib")
    for r in sorted([r for r in launches if lowest(r) is not None], key=lowest):
        k = r["dump"]["memory"]["kernel_pages"]
        out.append(f"| {name(r)} | {k['lowest_free_kib']} KiB | {k['free_kib']} KiB | {(r.get('kernel') or {}).get('class', '?')} |")
    return out


def text(result):
    lines = ["# WinCE data session findings", ""]
    launches = [r for r in result["launches"] if "state" in r]
    lines.append(f"{len(launches)} launches in {result['file']['slots']} slots.")
    lines.append("")
    lines.append("| Slot | Game | Kernel class | VMU family | CD line | Sound group | End | Saves to | Polls | CD audio | DirectSound used by | Lowest free |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in launches:
        dump = r.get("dump") or {}
        mem = (dump.get("memory") or {}).get("kernel_pages") or {}
        ds = (dump.get("watched_modules") or {}).get("dsound.dll", {}).get("used_by")
        cd = r.get("cd_audio") or {}
        ports_ = r.get("ports") or {}
        lines.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
            r["slot"], f"{r.get('title') or '?'} ({r.get('product')} {r.get('version')} {r.get('area')})",
            (r.get("kernel") or {}).get("class", "?"), r.get("maple_family", "?"), r.get("cd_driver_line", "?"),
            r.get("sound_group", "?"), r.get("end") if r.get("complete", True) else f"{r.get('end')} (incomplete)",
            ", ".join(ports_.get("saved_to", [])) or "not seen", ", ".join(ports_.get("polled_slots", [])) or "not seen",
            ", ".join(f"{k} {v}" for k, v in (cd.get("commands") or {}).items()) or "-",
            ", ".join(ds) if ds else ("not loaded" if dump.get("modules") else "?"),
            f"{mem['lowest_free_kib']} KiB" if "lowest_free_kib" in mem else "?"))
    lines += [""] + rollup(launches)
    for r in launches:
        lines += ["", f"## Slot {r['slot']}: {r.get('title') or '?'}", ""]
        lines.append("```")
        lines.append(json.dumps({k: v for k, v in r.items() if k not in ("title",)}, indent=1)[:20000])
        lines.append("```")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("data")
    parser.add_argument("--out", default="wce-findings")
    parser.add_argument("--evidence", default=str(EVIDENCE))
    parser.add_argument("--kernels", default=str(KERNELS))
    args = parser.parse_args()
    result = mine(args.data, args.evidence, args.kernels)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "findings.json").write_text(json.dumps(result, indent=1) + "\n")
    (out / "findings.md").write_text(text(result))
    print(f"{len([r for r in result['launches'] if 'state' in r])} launches; wrote {out / 'findings.md'}")


if __name__ == "__main__":
    sys.exit(main())
