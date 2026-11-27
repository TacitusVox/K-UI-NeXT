#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Routes of a Background launch whose executable the SDK check answers for.

The actual Games launcher runs on FAT32 and exFAT card images. Synthetic
executables never pass the real SDK check, so the check's answer is authored
(tests/test_games_native_route.c): a 448 KiB pool, or the real refusal. Each
row names the installed modules (the fixture case), the answer, what the card
already holds, and what the launcher must do or say. A refusal leaves the card
byte for byte as it was. Host decisions only; no console or game claim.

"other-game" is a card whose cache directory holds exactly one saved record,
another game's: the state in which a scan offers that record as this game's
hint. A combined module must not take it, or the launcher's own answer would
never be asked.

A "large" launch is refused the memory for the whole program, as the console
refuses it for a program larger than K-UI's free memory (Plasma Sword, 9.07
MiB, 27 November 2026). The launcher then reads the program once in pieces:
without the Maple discovery's witness in it the combined module is refused,
whatever else is installed, and the reader-only module is not asked; with the
witness, planted here in the fixture's program across its two sectors, there
is no answer and the combined module starts unseeded with the CRC of that one
read. That start is the host launcher's, which has no virtual-card code. On
the console it is what a Virtual VMU Off launch does; with Virtual VMU On the
combined module gets no prepared card without the launcher's answer, and its
stage stops at storage start (a known limit, docs/background-launch-checks.md).

Virtual VMU is On in every row: the host launcher is built without the
virtual-card code, and that build refuses a Background launch with Virtual VMU
Off before any of this. The Off answers, with the reader-only module's own
launch, are held by native_cache_refusal's table in test_games_native_cache.c.
"""
import json, os, re, shlex, shutil, subprocess, tempfile, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
X = "native-dual-standalone-report"                # New (X), SCI family, reader-only module installed
X_ABSENT = "native-dual-standalone-absent"         # New (X), SCI family, reader-only module not installed
X_NOFAMILY = "native-dual-standalone-nofamily"     # New (X), no SCI family
Y = "native-dual-standalone-y"                     # Legacy (Y)

POOL = "This game's memory pool is too small for the Background worker."
OFF_BOTH = " Set Background loading Off and Virtual VMU Off; saves then need a real VMU"
OFF_VMU = " Set Virtual VMU Off; saves then need a real VMU"
SDK = "Background loading cannot recognize this game's system library."
READER_ONLY = "reader-only"   # prepared with the reader-only module, nothing seeded
UNSEEDED = "combined-unseeded"  # prepared with the combined module, nothing seeded: its stage decides

# (fixture case, SDK answer, card, program) -> the refusal's two log lines, READER_ONLY or UNSEEDED.
# program: "whole" is held and checked; "large" and "large-witness" are refused the
# memory and read in pieces, without and with the Maple witness.
ROWS = (
    (X, "small", "empty", "whole", POOL + OFF_VMU),
    (X_ABSENT, "small", "empty", "whole", POOL + OFF_BOTH),
    (X_NOFAMILY, "small", "empty", "whole", POOL + OFF_BOTH),
    (Y, "small", "empty", "whole", POOL + OFF_BOTH),
    (X, "small", "other-game", "whole", POOL + OFF_VMU),
    (X_NOFAMILY, "small", "other-game", "whole", POOL + OFF_BOTH),
    (Y, "small", "other-game", "whole", POOL + OFF_BOTH),
    (X, "unresolved", "other-game", "whole", READER_ONLY),
    (X_ABSENT, "unresolved", "other-game", "whole", SDK + OFF_BOTH),
    (X_NOFAMILY, "unresolved", "other-game", "whole", SDK + OFF_BOTH),
    (Y, "unresolved", "other-game", "whole", SDK + OFF_BOTH),
    (X, "unresolved", "empty", "large", SDK + OFF_BOTH),
    (X_ABSENT, "unresolved", "other-game", "large", SDK + OFF_BOTH),
    (X_NOFAMILY, "unresolved", "empty", "large", SDK + OFF_BOTH),
    (Y, "unresolved", "other-game", "large", SDK + OFF_BOTH),
    (X, "unresolved", "other-game", "large-witness", UNSEEDED),
    (X, "unresolved", "empty", "large-witness", UNSEEDED),
)
FIRST_LINE = 76 - len("Retail boot: ")
WITNESS_SITE = 2040   # its 32 words lie in both sectors of the fixture's program


def witness_words():
    """The Maple discovery's own table (test_games_native_witness.py holds the launcher's copy to it)."""
    core = (ROOT / "src/core/vmu_native_discovery.c").read_text()
    body = re.search(r"static const struct witness words\[\]=\{(.*?)\};", core, re.S).group(1)
    words = [(int(offset), int(value, 16)) for offset, value in re.findall(r"\{(-?\d+),0x([0-9a-f]{4})\}", body)]
    assert len(words) == 32
    return words


def program_offset(at):
    """Where byte `at` of the fixture's boot program lies in its raw 2352-byte data track."""
    return (21 + at // 2048) * 2352 + 16 + at % 2048


def fixture_program(fixture, plant):
    """The boot program's bytes, after planting the witness when asked."""
    track = fixture / "track03.bin"
    data = bytearray(track.read_bytes())
    assert "4 2352 track03.bin" in (fixture / "disc.gdi").read_text()
    size = int.from_bytes(data[20 * 2352 + 16 + 68 + 10:20 * 2352 + 16 + 68 + 14], "little")
    assert size in (3001, 4096) and 0x53c <= WITNESS_SITE <= size - 0x1b8
    if plant:
        for offset, value in witness_words():
            at = program_offset(WITNESS_SITE + offset)
            data[at:at + 2] = value.to_bytes(2, "little")
        assert {(WITNESS_SITE + offset) // 2048 for offset, _ in witness_words()} == {0, 1}
        track.write_bytes(data)
    return bytes(data[program_offset(at)] for at in range(size))


def refusal(output):
    lines = [line[len("Retail boot: "):] for line in output.splitlines()
             if line.startswith("Retail boot: ") and "; product=" not in line]
    return lines[-1] if lines else None


def audit():
    from test_games_library_images import private_formatter
    from test_games_retail import make_retail_fixture
    from test_loader_probe_images import digest, partition_image
    plan = subprocess.check_output(["make", "-n", "-B", "build/games-retail"], cwd=ROOT, text=True)
    commands = [shlex.split(line) for line in plan.splitlines() if "tests/games_retail.c" in line and " -o " in line]
    assert len(commands) == 1
    env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1"}
    rows = []
    for text in {row[4] for row in ROWS} - {READER_ONLY, UNSEEDED}:
        # The first sentence fills the 76-column log line; the setting is the next line.
        assert text[FIRST_LINE] == " " and text[FIRST_LINE - 1] == "." and len(text) - FIRST_LINE <= 76, text
    with tempfile.TemporaryDirectory(prefix="kui-native-route-") as folder:
        work = Path(folder)
        command = commands[0].copy()
        command[command.index("tests/games_retail.c")] = "tests/test_games_native_route.c"
        binary = work / "route"
        command[command.index("-o") + 1] = str(binary)
        command += ["-Isrc/apps", "-fno-pie", "-no-pie", "-Wl,--wrap=kui_vmu_native_resolution_cache_build_prefixed",
                    "-DKUI_NATIVE_CACHE_BOOT_ALLOC=route_boot_alloc"]
        built = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
        assert not built.returncode, built.stderr
        formatter = private_formatter(work)
        for kind in ("fat32", "exfat"):
            volume = work / (kind + ".volume")
            clean = work / (kind + ".clean")
            subprocess.run([str(formatter), kind, str(volume)], env=env, check=True, capture_output=True, text=True)
            partition_image(volume, clean, kind)
            for number, (case, answer, saved, program, expected) in enumerate(ROWS):
                fixture = work / f"{kind}-{number}"
                make_retail_fixture(fixture, case, 0x47000)
                executable = fixture_program(fixture, program == "large-witness")
                memory = "whole" if program == "whole" else "large"
                card = work / f"{kind}-{number}.img"
                shutil.copyfile(clean, card)
                seeded = subprocess.run([str(binary), str(card), str(fixture), "seed", case, answer, saved, memory],
                                        cwd=ROOT, env=env, capture_output=True, text=True)
                assert not seeded.returncode, seeded.stdout + seeded.stderr
                before = digest(card)
                ran = subprocess.run([str(binary), str(card), str(fixture), "check", case, answer, saved, memory],
                                     cwd=ROOT, env=env, capture_output=True, text=True)
                assert not ran.returncode, ran.stdout + ran.stderr
                found = re.search(r"ROUTE prepared=(\d) standalone=(\d) seeded=(\d) family=(\d) standalone-opens=(\d) writes=(\d)", ran.stdout)
                assert found, ran.stdout
                prepared, standalone, seeded_module, family, opens, writes = map(int, found.groups())
                # A prepared launch logs other "Retail boot:" lines; only a refused one ends on its reason.
                said = None if prepared else refusal(ran.stdout)
                assert "Native VMU emulation: On" in ran.stdout
                assert "hint selected" not in ran.stdout, "another game's record was taken as this game's"
                pieces = f"Native reader: {len(executable)}-byte program is too large to hold; reading in pieces"
                if memory == "large":
                    # Read once in pieces; the whole-program check and the reader-only module's family are never asked.
                    assert family == 0 and pieces in ran.stdout, (case, program, ran.stdout)
                    assert "Native VMU mapping: reading" not in ran.stdout and "scanning SDK families" not in ran.stdout
                    assert "checking exact executable CRC" not in ran.stdout, "the program was read a second time"
                    assert ("Native reader: the SDK's Maple code is not in it; the stage would stop" in ran.stdout) == \
                        (program == "large"), (case, program, ran.stdout)
                    assert ("Native reader: the SDK's Maple code is in it; the full check needs it whole" in ran.stdout) == \
                        (program == "large-witness"), (case, program, ran.stdout)
                else:
                    assert family == (0 if case == Y else 1), (case, answer, family)
                    assert pieces not in ran.stdout
                    if answer == "small":
                        assert "Native reader: SDK pool 448 KiB cannot spare the 284 KiB lease" in ran.stdout
                    else:
                        assert "Native reader: SDK not recognized; the stage would stop" in ran.stdout
                if expected == READER_ONLY:
                    assert (prepared, standalone, seeded_module, opens) == (1, 1, 0, 1), (case, answer, saved, ran.stdout)
                    assert "using standalone New without virtual VMU" in ran.stdout
                    assert POOL not in ran.stdout and SDK not in ran.stdout
                elif expected == UNSEEDED:
                    assert (prepared, standalone, seeded_module, opens) == (1, 0, 0, 0), (case, program, saved, ran.stdout)
                    # The launch's CRC is that of the one read in pieces, independently recomputed here.
                    assert f"Retail boot: executable CRC32={zlib.crc32(executable):08x}" in ran.stdout, (case, ran.stdout)
                    assert POOL not in ran.stdout and SDK not in ran.stdout and "using standalone New" not in ran.stdout
                else:
                    assert (prepared, standalone, seeded_module, opens, writes) == (0, 0, 0, 0, 0), (case, answer, saved, ran.stdout)
                    assert said == expected, (case, answer, saved, said)
                    assert digest(card) == before, "a refused launch changed the card"
                    assert "using standalone New" not in ran.stdout
                rows.append(dict(filesystem=kind, installed=case, sdk_answer=answer, card=saved, virtual_vmu="on",
                                 program={"whole": "held whole", "large": "read in pieces, no Maple witness",
                                          "large-witness": "read in pieces, Maple witness present"}[program],
                                 prepared=bool(prepared), reader_only_module=bool(standalone),
                                 refusal=said, card_unchanged=not prepared))
    return dict(schema="kui.gui.native-route.v1", result="PASS", cases=rows,
                sdk_check="authored answer after the actual check refuses the synthetic executable; "
                          "the launcher, FatFs, module admission and messages are the production code")


if __name__ == "__main__":
    report = audit()
    print(json.dumps(report, indent=2))
    print(f"PASS native Background routes: {len(report['cases'])} launches on FAT32 and exFAT; "
          "each refusal names its setting and leaves the card unchanged; another game's saved record is never taken; "
          "a program too large to hold is read once in pieces and refused where the SDK's Maple code is not in it")
