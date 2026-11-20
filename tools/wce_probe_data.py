#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The Windows CE data probe's data file (include/kui/wce_probe.h).

usage:
  wce_probe_data.py blank OUT [--slots N] [--id TEXT]   make an empty WCEDATA.BIN
  wce_probe_data.py read DATA [--json OUT] [--dumps DIR]  decode a filled one

`read` decodes every claimed slot: the stage report (game, kernel, card), the
terminal report (counters, registers) and, with --dumps, writes each slot's
RAM dump as a 16 MiB image (zero pages filled back in) named by slot and
product. It reads the owner's file only; nothing is sent anywhere.
"""
import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

SECTOR = 512
VERSION = 1
FIRST_SLOT = 2048
STAGE_SECTOR, STAGE_SECTORS = 1, 7
TERMINAL_SECTOR, TERMINAL_SECTORS = 8, 8
DUMP_SECTOR = 16
RAM_BEGIN, RAM_BYTES, PAGE = 0x8c000000, 0x01000000, 4096
PAGES = RAM_BYTES // PAGE
DUMP_SECTORS = RAM_BYTES // SECTOR
SLOT_SECTORS = DUMP_SECTOR + DUMP_SECTORS
PAGE_MAP_OFFSET, PAGE_MAP_BYTES = 3584, PAGES // 8
SLOT_RUNS = 8
STATES = {0: "empty", 1: "claimed", 2: "dumping", 3: "complete"}
# A reader fault names its message by length (src/loader/retail_resident.c,
# the probe's report_fault macro: sizeof the literal, so length + 1).
FAULT_REASONS = {12: "REQ REFUSED", 15: "GD UNSUPPORTED", 16: "CPU MAP REFUSED",
                 18: "IMAGE READ FAILED", 19: "TRANSITION REACHED", 20: "GD REQUEST REJECTED",
                 24: "GD FUNCTION UNSUPPORTED", 28: "VC2 VMU PREPARATION REFUSED",
                 31: "CE KERNEL INTERRUPTS NOT FOUND"}
REASONS = {0: "none", 1: "A+B+X+Y+Start", 2: "reader fault"}
FUNCTIONS = ["REQUEST", "CHECK", "EXEC", "INIT", "DRIVE", "DMA_CALLBACK", "DMA_TRANSFER",
             "DMA_CHECK", "ABORT", "RESET", "DATATYPE", "PIO_CALLBACK", "PIO_TRANSFER",
             "PIO_CHECK", "F14", "F15"]
COMMANDS = {16: "PIOREAD", 17: "DMAREAD", 18: "GETTOC", 19: "GETTOC2", 20: "PLAY", 21: "PLAY2",
            22: "PAUSE", 23: "RELEASE", 24: "INIT", 25: "DMA_ABORT", 26: "OPEN_TRAY", 27: "SEEK",
            28: "DMAREAD_STREAM", 29: "NOP", 30: "REQ_MODE", 31: "SET_MODE", 32: "SCAN_CD",
            33: "STOP", 34: "GETSCD", 35: "GETSES", 36: "REQ_STAT", 37: "PIOREAD_STREAM",
            38: "DMAREAD_STREAM_EX", 39: "PIOREAD_STREAM_EX", 40: "GET_VERS"}
MAPLE_UNITS = [f"{port}{unit}" for port in "ABCD" for unit in ("", "1", "2")]
MAPLE_COMMANDS = ["other", "device info", "condition", "media info", "block read",
                  "block write", "write complete", "set condition"]
# SR is read inside the hook, after it forced BL and IMASK15.
REGISTERS = ["SR (hook)", "VBR", "CCR", "MMUCR", "PTEH", "TTB", "MDSTAR", "ISTNRM",
             "SPG_CONTROL", "SPG_LOAD", "VO_CONTROL", "FB_R_CTRL", "FB_R_SOF1",
             "IML2NRM", "IML4NRM", "IML6NRM"]

FILE = struct.Struct("<8s6I16s4I")
SLOT = struct.Struct("<8s6I16s4I")
SEED = struct.Struct("<8I16s" + "2I" * SLOT_RUNS + "64s2I")
AUDIO = struct.Struct("<BBH3I")
COUNTERS_HEAD = struct.Struct("<8I16H32H4I4I4I2I")
AUDIO_FIRST, AUDIO_RING = 4, 8
STAGE = struct.Struct("<8s2I16s24I256s84s13I")
TERMINAL_HEAD = struct.Struct("<8s10I32I")
assert FILE.size == SLOT.size == 64 and SEED.size == 184 and STAGE.size == 520 and COUNTERS_HEAD.size == 184


def crc(data):
    return zlib.crc32(data) & 0xffffffff


def file_id(text):
    return hashlib.sha256(b"KUIWCED1/" + text.encode()).digest()[:16]


def blank(out, slots, ident):
    fid = file_id(ident)
    total = FIRST_SLOT + slots * SLOT_SECTORS
    head = FILE.pack(b"KUIWCED1", VERSION, 64, slots, SLOT_SECTORS, FIRST_SLOT, DUMP_SECTORS,
                     fid, 0, 0, 0, 0)
    head = head[:60] + struct.pack("<I", crc(head[:60]))
    note = (b"K-UI WinCE data file. Copy it to KUI/WCEDATA/ on the SD card, play games with the "
            b"test ce-boot.kui, end each with A+B+X+Y+Start, then upload this file.\n")
    zeros = bytes(1 << 20)

    def fill(f, count):
        # Every byte is written: no sparse holes, so an exFAT copy has
        # ValidDataLength equal to its size (the probe refuses anything else).
        while count:
            n = min(count, len(zeros))
            f.write(zeros[:n])
            count -= n

    with open(out, "wb") as f:
        f.write(head + note.ljust(SECTOR - len(head), b"\0"))
        fill(f, (FIRST_SLOT - 1) * SECTOR)
        for s in range(slots):
            body = SLOT.pack(b"KUIWCES1", VERSION, 64, s, 0, 0, 0, fid, 0, 0, 0, 0)
            body = body[:60] + struct.pack("<I", crc(body[:60]))
            f.write(body.ljust(SECTOR, b"\0"))
            fill(f, (SLOT_SECTORS - 1) * SECTOR)
        assert f.tell() == total * SECTOR
    return {"slots": slots, "sectors": total, "bytes": total * SECTOR, "file_id": fid.hex()}


def cstr(raw):
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def ip_fields(ip):
    if ip[:16] != b"SEGA SEGAKATANA ":
        return {"valid": False}
    ip = ip.replace(b"\0", b" ")
    return {"valid": True, "maker": ip[16:32].decode("ascii", "replace").strip(),
            "device": ip[32:48].decode("ascii", "replace").strip(),
            "area": ip[48:56].decode("ascii", "replace").strip(),
            "peripherals": ip[56:64].decode("ascii", "replace").strip(),
            "product": ip[64:74].decode("ascii", "replace").strip(),
            "version": ip[74:80].decode("ascii", "replace").strip(),
            "date": ip[80:96].decode("ascii", "replace").strip(),
            "boot": ip[96:112].decode("ascii", "replace").strip(),
            "company": ip[112:128].decode("ascii", "replace").strip(),
            "title": ip[128:256].decode("ascii", "replace").strip()}


def decode_seed(raw):
    v = SEED.unpack(raw[:SEED.size])
    runs = [{"lba": v[9 + 2 * i], "sectors": v[10 + 2 * i]} for i in range(SLOT_RUNS)]
    return {"magic_ok": v[0] == 0x53454357, "version": v[1], "slot": v[2], "run_count": v[3],
            "slot_header_lba": v[4], "sequence": v[5], "stage_rtc": v[6], "flags": v[7],
            "runs": runs[:v[3]], "crc_ok": crc(raw[:SEED.size - 4]) == v[-1]}


def decode_counters(raw):
    if len(raw) < 520:
        return None
    v = COUNTERS_HEAD.unpack(raw[:COUNTERS_HEAD.size])
    at = COUNTERS_HEAD.size
    if v[0] != 0x43454357:
        return {"magic_ok": False}
    functions, commands = v[8:24], v[24:56]
    refused, refused_last, mask0, mask1 = v[56:60]
    read_requests, read_sectors, largest_read, unknown_commands = v[60:64]
    audio_count, maple_samples, maple_lists, maple_frames = v[64:68]
    maple_bad, maple_last_list = v[68:70]
    audio = []
    for i in range(AUDIO_FIRST + AUDIO_RING):
        c, r, call, first, last, repeat = AUDIO.unpack_from(raw, at + i * AUDIO.size)
        # A refused command's slots hold the owning command's parameters.
        audio.append({"command": COMMANDS.get(c, c), "accepted": r == 1, "call": call,
                      "first": first if r == 1 else None, "last": last if r == 1 else None,
                      "repeat": repeat if r == 1 else None})
    at += (AUDIO_FIRST + AUDIO_RING) * AUDIO.size
    maple = struct.unpack_from("<96B", raw, at)
    at += 96
    maple_functions = struct.unpack_from("<12I", raw, at)
    # The first 4 audio commands, then a ring of the latest 8 (entry
    # (n-4) % 8 holds command n, counting from 0).
    first, ring = audio[:AUDIO_FIRST], audio[AUDIO_FIRST:]
    later = max(0, audio_count - AUDIO_FIRST)
    order = ring[:later] if later <= AUDIO_RING else ring[later % AUDIO_RING:] + ring[:later % AUDIO_RING]
    mask = mask0 | mask1 << 32
    def count16(n):
        return "65535+" if n == 0xffff else n  # the 16-bit counters saturate

    def count8(n):
        return "255+" if n == 0xff else n  # so do the 8-bit Maple counters

    return {"magic_ok": True, "calls": v[3],
            "rtc": {"first_call": v[4], "first_read": v[5], "first_audio": v[6], "last": v[7]},
            "functions": {FUNCTIONS[i]: count16(n) for i, n in enumerate(functions) if n},
            "commands": {COMMANDS.get(i + 16, str(i + 16)): count16(n) for i, n in enumerate(commands) if n},
            "refused": {"count": refused, "last": COMMANDS.get(refused_last, refused_last),
                        "commands": [COMMANDS.get(c, c) for c in range(64) if mask >> c & 1]},
            "reads": {"requests": read_requests, "sectors": read_sectors, "largest": largest_read},
            "unknown_commands": unknown_commands, "audio_commands": audio_count,
            "audio_log": first[:min(AUDIO_FIRST, audio_count)] + order,
            "audio_log_dropped": max(0, audio_count - AUDIO_FIRST - AUDIO_RING),
            "maple": {"samples": maple_samples, "lists": maple_lists, "frames": maple_frames,
                      "bad_lists": maple_bad, "last_list": f"{maple_last_list:#010x}",
                      "units": {MAPLE_UNITS[u]: {"functions": f"{maple_functions[u]:#010x}",
                                                 **{MAPLE_COMMANDS[c]: count8(maple[u * 8 + c])
                                                    for c in range(8) if maple[u * 8 + c]}}
                                for u in range(12) if any(maple[u * 8:(u + 1) * 8]) or maple_functions[u]}}}


def decode_stage(raw):
    v = STAGE.unpack(raw[:STAGE.size])
    if v[0] != b"KUIWCEST":
        return None
    names = ["rtc", "transport", "card_high_capacity", "card_blocks", "partition_start",
             "partition_end", "track_count", "extent_count", "map_slots", "kernel_bytes",
             "kernel_crc32", "kernel_head_crc32", "rom_header", "rom_header_method", "ram_start",
             "ram_free", "ram_end", "zero_begin", "zero_end", "file_runs", "slot", "slot_runs",
             "sequence", "flags"]
    out = {"version": v[1], "build": cstr(v[3])}
    out.update({n: v[4 + i] for i, n in enumerate(names)})
    out["zero_high_begin"], out["zero_high_end"] = v[30], v[31]
    for n in ("kernel_crc32", "kernel_head_crc32", "rom_header", "ram_start", "ram_free", "ram_end",
              "zero_begin", "zero_end", "zero_high_begin", "zero_high_end"):
        out[n] = f"{out[n]:#010x}"
    out["ip"] = ip_fields(v[28])
    out["rom_header_bytes"] = v[29].hex()
    return out


def decode_terminal(raw):
    v = TERMINAL_HEAD.unpack(raw[:TERMINAL_HEAD.size])
    if v[0] != b"KUIWCETR":
        return None
    at = TERMINAL_HEAD.size
    seed = decode_seed(raw[at:at + SEED.size])
    counters = decode_counters(raw[at + SEED.size:PAGE_MAP_OFFSET])
    fault = v[3] == 2
    out = {"version": v[1], "reason": REASONS.get(v[3], v[3])}
    if fault:  # the GD function and the message, named by its length
        out.update({"function": FUNCTIONS[v[4]] if v[4] < len(FUNCTIONS) else v[4],
                    "reason_code": v[5], "reason_text": FAULT_REASONS.get(v[5], f"unknown ({v[5]})")})
    else:
        out.update({"menu_command": v[4], "caller": f"{v[5]:#010x}", "stack": f"{v[6]:#010x}"})
    return {**out, "rtc": v[7], "hook_fault": v[8],
            "pages": v[9], "dump_sectors": v[10],
            "registers": {REGISTERS[i]: f"{v[11 + i]:#010x}" for i in range(len(REGISTERS))},
            "seed": seed, "counters": counters}


def page_map(raw):
    bits = raw[PAGE_MAP_OFFSET:PAGE_MAP_OFFSET + PAGE_MAP_BYTES]
    return [p for p in range(PAGES) if bits[p // 8] >> (p % 8) & 1]


def read(path, json_out=None, dumps=None):
    data = Path(path)
    with open(data, "rb") as f:
        head = f.read(SECTOR)
        v = FILE.unpack(head[:64])
        if v[0] != b"KUIWCED1" or crc(head[:60]) != v[-1]:
            raise SystemExit("not a WinCE data file (header)")
        slots, slot_sectors, first_slot, fid = v[3], v[4], v[5], v[7]
        result = {"file": {"slots": slots, "slot_sectors": slot_sectors, "first_slot": first_slot,
                           "file_id": fid.hex()}, "sessions": []}
        for s in range(slots):
            base = (first_slot + s * slot_sectors) * SECTOR
            f.seek(base)
            sh = f.read(SECTOR)
            if len(sh) < 64:
                result["sessions"].append({"slot": s, "error": "the file ends here (a truncated copy?)"})
                result["file"]["truncated"] = True
                break
            w = SLOT.unpack(sh[:64])
            if w[0] != b"KUIWCES1" or w[3] != s or w[7] != fid or crc(sh[:60]) != w[-1]:
                result["sessions"].append({"slot": s, "error": "slot header unreadable"})
                continue
            if w[4] == 0:
                continue
            # The slot header's pages and reason are always 0: the terminal
            # report holds them.
            session = {"slot": s, "state": STATES.get(w[4], w[4]), "sequence": w[5], "rtc": w[6]}
            f.seek(base + STAGE_SECTOR * SECTOR)
            session["stage"] = decode_stage(f.read(STAGE_SECTORS * SECTOR))
            f.seek(base + TERMINAL_SECTOR * SECTOR)
            terminal_raw = f.read(TERMINAL_SECTORS * SECTOR)
            session["terminal"] = decode_terminal(terminal_raw)
            terminal = session["terminal"]
            if terminal:
                pages = page_map(terminal_raw)[:terminal["pages"]]
                image = bytearray(RAM_BYTES)
                f.seek(base + DUMP_SECTOR * SECTOR)
                missing = 0
                for p in pages:
                    chunk = f.read(PAGE)
                    # Every page the probe dumps is non-zero: an all-zero or
                    # short page was never written (a cut-off dump).
                    if len(chunk) < PAGE or not any(chunk):
                        missing += 1
                    image[p * PAGE:p * PAGE + len(chunk)] = chunk
                complete = session["state"] == "complete" and not missing
                session["dump_complete"] = complete
                session["dump_pages"] = len(pages) - missing
                session["dump_missing_pages"] = missing
                if dumps:
                    product = (session["stage"] or {}).get("ip", {}).get("product", "") or ""
                    product = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in product).strip("_")
                    out_dir = Path(dumps)
                    out_dir.mkdir(parents=True, exist_ok=True)
                    name = out_dir / f"slot{s:03d}-{product or 'unknown'}{'' if complete else '-partial'}.ram"
                    name.write_bytes(bytes(image))
                    session["dump_file"] = str(name)
            result["sessions"].append(session)
    if json_out:
        Path(json_out).write_text(json.dumps(result, indent=1) + "\n")
    return result


def summary(result):
    print(f"{len([s for s in result['sessions'] if 'state' in s])} sessions in "
          f"{result['file']['slots']} slots")
    for s in result["sessions"]:
        if "state" not in s:
            print(f"slot {s['slot']:3d}: {s.get('error')}")
            continue
        ip = (s.get("stage") or {}).get("ip", {})
        t = s.get("terminal") or {}
        c = (t.get("counters") or {}) if t else {}
        end = t.get("reason", "none") + (f" ({t.get('reason_text')})" if t.get("reason_text") else "")
        dump = "" if not t else " dump complete" if s.get("dump_complete") else \
            f" dump cut off ({s.get('dump_missing_pages', 0)} pages missing)"
        print(f"slot {s['slot']:3d} {s['state']:8s} {ip.get('product', '?'):10s} "
              f"{ip.get('title', '?')[:32]:32s} end={end:14s} "
              f"pages={t.get('pages', 0):5d} calls={c.get('calls', 0)} audio={c.get('audio_commands', 0)}{dump}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    b = sub.add_parser("blank")
    b.add_argument("out")
    b.add_argument("--slots", type=int, default=64)
    b.add_argument("--id", default="KUI WinCE data file")
    r = sub.add_parser("read")
    r.add_argument("data")
    r.add_argument("--json")
    r.add_argument("--dumps")
    args = parser.parse_args()
    if args.command == "blank":
        if not 1 <= args.slots <= 255:  # the file must stay below 4 GiB
            raise SystemExit("slots must be 1..255")
        print(json.dumps(blank(args.out, args.slots, args.id)))
    else:
        summary(read(args.data, args.json, args.dumps))


if __name__ == "__main__":
    sys.exit(main())
