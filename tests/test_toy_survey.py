#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and run the stream survey's report-file tests against FatFs, and hold
tools/toy_survey.py to the same file: its blank file is the C sources' byte
for byte, it reads back every launch the C test played and every launch
record the stage's own text wrote, and each state and command of it is run.

The stage's test is run a second time as a copy whose report seed is patched
as Card Tools patches a module's: that run plays a module started from Card
Tools."""
import contextlib
import io
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
STAGE_ID = bytes.fromhex("5a112233445566778899aabbccddeeff")
STAGE_SLOTS = 12
BUILD = "0123456789ab"

# The launch records tests/test_toy_survey_stage.c expects of its three maps.
STAGE_RAW = {
    "survey": 83, "build": BUILD, "product": "T-12345N", "title": "EXAMPLE GAME", "bootfile": "1ST_READ.BIN",
    "region": "JUE", "ip_crc32": 0x12345678, "gdi_crc32": 0x0d0d0d0d, "boot_crc32": 0x0b0b0b0b,
    "boot_bytes": 1234567, "exec_crc32": 0x9abcdef0, "exec_bytes": 4057056, "reader": 1, "transport": 1,
    "flags": 4, "source": 1, "tracks": 5, "raw": 1, "cooked": 0, "audio": 4, "audio_high": 2, "audio_mapped": 1,
    "other": 0, "extents": 3, "session_lba": 45150, "boot_lba": 45200}
STAGE_COOKED = {**STAGE_RAW, "source": 0, "tracks": 3, "raw": 0, "cooked": 1, "audio": 2, "audio_high": 0,
                "audio_mapped": 0}
STAGE_RICH = {
    "survey": 83, "build": BUILD, "product": "T-1234567890123N",
    "title": "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCD", "bootfile": "ABCDEFGHIJKLMNOP",
    "region": "JUEJUEJU", "ip_crc32": 0xfedcba98, "gdi_crc32": 1, "boot_crc32": 0x80000000, "boot_bytes": 0xffffff,
    "exec_crc32": 0, "exec_bytes": 1, "reader": 1, "transport": 1, "flags": 7, "source": 1, "tracks": 12,
    "raw": 1, "cooked": 2, "audio": 6, "audio_high": 4, "audio_mapped": 5, "other": 3, "extents": 7,
    "session_lba": 45150, "boot_lba": 47150}


def fatfs_source():
    dependency = ROOT / ".deps/fatfs/source"
    return dependency if dependency.is_dir() else ROOT / "fatfs/source"


def played_launch(slot):
    """The launch record tests/test_toy_survey.c writes into a slot."""
    return {
        "survey": 83, "build": BUILD, "product": "T-12345N",
        "title": "SURVEY GAME NUMBER " + chr(ord("A") + slot % 26), "bootfile": "1ST_READ.BIN", "region": "JUE",
        "ip_crc32": 0x11111111 + slot, "gdi_crc32": 0x22222222, "boot_crc32": 0x33333333,
        "boot_bytes": 0x123456, "exec_crc32": 0x44444444 + slot, "exec_bytes": 0x123456,
        "reader": 1, "transport": 1, "flags": 4, "source": slot & 1, "tracks": 5 + slot, "raw": 1,
        "cooked": 1, "audio": 2 + slot, "audio_high": 1 + slot, "audio_mapped": slot // 2, "other": 1,
        "extents": 7, "session_lba": 45150, "boot_lba": 45200 + slot}


def played_report(slot):
    return "K-UI TOY COMMANDER REPORT\nTEST 81\nBUILD %s\nSLOT %d\n" % (BUILD, slot)


def tool(*arguments):
    """Run the tool's command line in this process: its exit status, what it printed, its complaints."""
    out, complaints = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(complaints):
        status = toy_survey.main([str(argument) for argument in arguments])
    return status, out.getvalue(), complaints.getvalue()


def tool_states(used):
    """Every state the tool tells apart, made from the file the C test played."""
    data = bytearray(used)

    def at(slot, part):
        return toy_survey.SECTOR * (1 + toy_survey.PARTS * slot + part)

    def put(slot, part, sector):
        assert len(sector) == toy_survey.SECTOR
        data[at(slot, part):at(slot, part) + toy_survey.SECTOR] = sector

    def get(slot, part):
        return bytes(data[at(slot, part):at(slot, part) + toy_survey.SECTOR])

    def blank(slot, part):
        return toy_survey.blank_sector(FILE_ID, slot, part)

    expect = {slot: "launched, no report saved" if slot % 5 == 3 else "report saved" for slot in range(SLOTS)}
    damaged_launch = {}
    put(1, 2, blank(1, 2)); expect[1] = "launched, report cut off"          # the second half never written
    put(2, 1, blank(2, 1)); expect[2] = "launched, report cut off"          # the first half gone
    put(4, 1, bytes(512)); put(4, 2, bytes(512)); expect[4] = "report damaged"     # nothing of a report
    first, second = get(5, 1), get(5, 2)
    put(5, 1, second); put(5, 2, first); expect[5] = "report damaged"       # its halves the wrong way round
    put(6, 2, get(7, 2)[:500] + b"X" * 12); expect[6] = "report damaged"    # its end marker gone
    put(7, 1, b"k" + get(7, 1)[1:]); expect[7] = "report damaged"           # its first line not a report's
    put(9, 0, blank(9, 0)); expect[9] = "damaged"                           # a report and no launch record
    put(10, 0, bytes(512)); expect[10] = "damaged"                          # no text at all
    put(11, 0, get(12, 0)); expect[11] = "damaged"                          # another slot's launch record
    put(14, 0, get(14, 0)[:-1] + b" "); expect[14] = "damaged"              # its last byte no line feed
    put(15, 0, get(15, 0).replace(b"\nTRACKS ", b"\nTRACKZ ")); damaged_launch[15] = "tracks"
    put(16, 0, get(16, 0).replace(b"\nIPCRC 1", b"\nIPCRC g")); damaged_launch[16] = "crc"
    put(13, 0, get(13, 0).replace(b"\nSESSION ", b"\nSESSIOM ")); damaged_launch[13] = "session"  # and no report
    put(17, 1, blank(17, 1)); put(17, 2, blank(17, 2)); expect[17] = "launched, no report saved"
    put(19, 0, blank(19, 0)); put(19, 1, blank(19, 1)); put(19, 2, blank(19, 2)); expect[19] = "empty"
    result = toy_survey.read_file(bytes(data))
    for slot, entry in enumerate(result["entries"]):
        assert entry["slot"] == slot and entry["state"] == expect[slot], (entry, expect[slot])
        assert ("launch" in entry) == (expect[slot] not in ("empty", "damaged")), entry
        assert ("report" in entry) == (expect[slot] in ("report saved", "report damaged", "launched, report cut off")), entry
        if "launch" in entry:
            assert entry["launch"].get("damaged") == damaged_launch.get(slot), entry
            if slot not in damaged_launch:
                assert entry["launch"] == played_launch(slot), entry
    # A launch record that stops at a line keeps the lines before it.
    assert result["entries"][15]["launch"]["title"] == "SURVEY GAME NUMBER P" and "tracks" not in result["entries"][15]["launch"]
    assert "ip_crc32" not in result["entries"][16]["launch"] and result["entries"][16]["launch"]["region"] == "JUE"
    # The listing names every slot that is not empty, its state, and a launch record that is damaged.
    lines = toy_survey.listing("FILE", result)
    assert lines[0] == "FILE: ID %s, %d of %d slots used" % (FILE_ID.hex().upper(), SLOTS - 1, SLOTS), lines[0]
    assert len(lines) == SLOTS, lines
    listed = {int(line[5:8]): line for line in lines[1:]}
    assert sorted(listed) == [slot for slot in range(SLOTS) if slot != 19]
    for slot, line in listed.items():
        assert line.startswith("slot %03d: %s" % (slot, expect[slot])), line
        assert ("LAUNCH RECORD DAMAGED at its %s line" % damaged_launch.get(slot) in line) == (slot in damaged_launch), line
        assert ("LAUNCH RECORD DAMAGED" in line) == (slot in damaged_launch), line
        if expect[slot] == "damaged":
            assert line == "slot %03d: damaged" % slot, line
    assert listed[0] == ("slot 000: report saved: T-12345N SURVEY GAME NUMBER A (build %s; source 0; tracks 5, "
                         "CD music tracks 1)" % BUILD), listed[0]
    assert listed[15].startswith("slot 015: report saved: T-12345N SURVEY GAME NUMBER P (build %s; source 1; tracks ?, "
                                 "CD music tracks ?); LAUNCH RECORD DAMAGED" % BUILD), listed[15]
    return bytes(data), result


def tool_commands(base, blank, used):
    """The tool's two commands, as they are typed."""
    # blank: the named ID and slot count; by default 100 slots and an ID of its own each time.
    named = base / "named.txt"
    status, out, complaints = tool("blank", named, "--slots", SLOTS, "--id", FILE_ID.hex())
    assert status == 0 and not complaints and named.read_bytes() == blank, (status, complaints)
    assert out == "%s: %d slots, %d bytes, ID %s\n" % (named, SLOTS, len(blank), FILE_ID.hex().upper()), out
    files = []
    for name in ("one.txt", "two.txt"):
        status, out, complaints = tool("blank", base / name)
        data = (base / name).read_bytes()
        assert status == 0 and not complaints and len(data) == 512 * (1 + 3 * 100), (status, complaints)
        parsed = toy_survey.read_file(data)
        assert parsed["slots"] == 100 and all(entry["state"] == "empty" for entry in parsed["entries"])
        assert out == "%s: 100 slots, %d bytes, ID %s\n" % (base / name, len(data), parsed["id"]), out
        files.append(parsed["id"])
    assert files[0] != files[1], "two blank files share an ID"
    for slots in (1, 999):
        status, out, complaints = tool("blank", base / "edge.txt", "--slots", slots)
        assert status == 0 and len((base / "edge.txt").read_bytes()) == 512 * (1 + 3 * slots)
        assert toy_survey.read_file((base / "edge.txt").read_bytes())["slots"] == slots
    # blank: what it refuses, with a line that says why and no file left.
    for arguments, why in ((("--slots", 0), "1 to 999 slots"), (("--slots", 1000), "1 to 999 slots"),
                           (("--id", "0123"), "32 hex digits"), (("--id", "g" * 32), "32 hex digits"),
                           (("--id", FILE_ID.hex() + "00"), "32 hex digits")):
        refused = base / "refused.txt"
        status, out, complaints = tool("blank", refused, *arguments)
        assert status == 1 and not out and why in complaints and not refused.exists(), (arguments, status, complaints)
    # read: the listing and, asked for, the same result as JSON.
    damaged, result = tool_states(used)
    returned = base / "returned.txt"
    returned.write_bytes(damaged)
    listed = base / "listed.json"
    status, out, complaints = tool("read", returned, "--json", listed)
    assert status == 0 and not complaints and out == "\n".join(toy_survey.listing(returned, result)) + "\n", out
    import json
    assert json.loads(listed.read_text()) == result
    status, out, complaints = tool("read", named)
    assert status == 0 and out == "%s: ID %s, 0 of %d slots used\n" % (named, FILE_ID.hex().upper(), SLOTS), out
    # read: a file that is not the survey's, cut short, or not there.
    for name, data, why in (("short.txt", blank[:-512], "not the"), ("edited.txt", blank[:40] + b"X" + blank[41:],
                            "not a stream survey file"), ("empty.txt", b"", "not a stream survey file"),
                            ("slots.txt", blank.replace(b"SLOTS 20\n", b"SLOTS 21\n", 1), "its 21 slots take"),
                            ("words.txt", blank.replace(b"SEND THIS FILE BACK", b"SEND THIS FILE BACk", 1),
                             "first sector is not what its ID and slot count give")):
        (base / name).write_bytes(data)
        status, out, complaints = tool("read", base / name)
        assert status == 1 and not out and why in complaints, (name, status, complaints)
    status, out, complaints = tool("read", base / "absent.txt")
    assert status == 1 and not out and "absent.txt" in complaints, complaints
    # And as a program of its own.
    run = subprocess.run([sys.executable, str(ROOT / "tools/toy_survey.py"), "read", str(returned)],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert run.returncode == 0 and run.stdout == "\n".join(toy_survey.listing(returned, result)) + "\n" and not run.stderr
    run = subprocess.run([sys.executable, str(ROOT / "tools/toy_survey.py"), "read", str(base / "short.txt")],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert run.returncode == 1 and not run.stdout and run.stderr.startswith("toy_survey.py: ") and "Traceback" not in run.stderr


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
        # The cold stage's own text, on the same kinds of card.
        stage = base / "test-toy-survey-stage"
        subprocess.run([*compiler, "-I" + str(ROOT / "src/loader"), '-DKUI_BUILD_ID="%s"' % BUILD,
                        "-DKUI_VMU_NATIVE_PERSISTENT_PROBE=1", str(ROOT / "tests/test_toy_survey_stage.c"), *shared,
                        "-o", str(stage)], check=True)
        subprocess.run([str(stage), str(out)], check=True, env=environment)
        # The same program with its report seed patched, as Card Tools patches a module's: the blank
        # seed's 64 bytes are in the program once, and the patched copy plays a start from Card Tools.
        seeds = subprocess.run([str(stage), "--seed"], check=True, env=environment, stdout=subprocess.PIPE).stdout
        assert len(seeds) == 128 and seeds[:64] != seeds[64:], "the stage test gives its two report seeds"
        program = stage.read_bytes()
        assert program.count(seeds[:64]) == 1, "the stage test holds its blank report seed once"
        patched = base / "test-toy-survey-stage-card-tools"
        patched.write_bytes(program.replace(seeds[:64], seeds[64:]))
        patched.chmod(0o755)
        subprocess.run([str(patched), "--card-tools"], check=True, env=environment)
        refused = subprocess.run([str(stage), "--card-tools"], env=environment, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
        assert refused.returncode != 0, "the unpatched stage test played a start from Card Tools"
        # The tool's blank file is the stage's, byte for byte.
        blank = (out / "blank.txt").read_bytes()
        assert blank == toy_survey.blank_file(FILE_ID, SLOTS), "the tool's blank file differs from the C sources'"
        assert toy_survey.read_file(blank) == {
            "id": FILE_ID.hex().upper(), "slots": SLOTS,
            "entries": [{"slot": slot, "state": "empty"} for slot in range(SLOTS)]}
        # So is the largest there can be: slot numbers of three digits.
        assert (out / "blank-999.txt").read_bytes() == toy_survey.blank_file(FILE_ID, toy_survey.SLOTS_MAX), \
            "the tool's largest blank file differs from the C sources'"
        # The tool reads every launch the C test played.
        used_bytes = (out / "used.txt").read_bytes()
        used = toy_survey.read_file(used_bytes)
        assert used["id"] == FILE_ID.hex().upper() and used["slots"] == SLOTS
        for slot, entry in enumerate(used["entries"]):
            launch = entry["launch"]
            assert entry["slot"] == slot and "damaged" not in launch, entry
            assert launch == played_launch(slot), launch
            if slot % 5 == 3:
                assert entry["state"] == "launched, no report saved" and "report" not in entry, entry
            else:
                assert entry["state"] == "report saved", entry
                report = entry["report"]
                assert len(report) == 1024 and report.endswith("\nEND REPORT\n")
                assert report.startswith(played_report(slot))
        # The tool reads the launch records the stage's own text wrote, for each map of its test:
        # slots 0 to 2 the two plain maps, then the fuller one, the raw one and the cooked one in turn.
        staged = toy_survey.read_file((out / "stage-used.txt").read_bytes())
        assert staged["id"] == STAGE_ID.hex().upper() and staged["slots"] == STAGE_SLOTS
        maps = [STAGE_RAW, STAGE_COOKED, STAGE_RAW] + [(STAGE_RICH, STAGE_RAW, STAGE_COOKED)[slot % 3]
                                                       for slot in range(3, STAGE_SLOTS)]
        for slot, entry in enumerate(staged["entries"]):
            assert entry == {"slot": slot, "state": "launched, no report saved", "launch": maps[slot]}, entry
        # A truncated upload and an edited first sector are refused, not guessed at.
        for bad in (blank[:-512], blank[:40] + b"X" + blank[41:], b"\0" * len(blank), b""):
            try:
                toy_survey.read_file(bad)
            except ValueError:
                continue
            raise AssertionError("a damaged file was read")
        # Every state the tool tells apart, and its two commands.
        tool_commands(base, blank, used_bytes)
        print("PASS tools/toy_survey.py: its blank file is the stage's; %d launches played and %d the stage wrote "
              "read back; every state and both commands" % (SLOTS, STAGE_SLOTS))


if __name__ == "__main__":
    sys.exit(main())
