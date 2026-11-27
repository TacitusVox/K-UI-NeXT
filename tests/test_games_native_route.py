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

Virtual VMU is On in every row: the host launcher is built without the
virtual-card code, and that build refuses a Background launch with Virtual VMU
Off before any of this. The Off answers, with the reader-only module's own
launch, are held by native_cache_refusal's table in test_games_native_cache.c.
"""
import json, os, re, shlex, shutil, subprocess, tempfile
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

# (fixture case, SDK answer, card) -> the refusal's two log lines, or READER_ONLY.
ROWS = (
    (X, "small", "empty", POOL + OFF_VMU),
    (X_ABSENT, "small", "empty", POOL + OFF_BOTH),
    (X_NOFAMILY, "small", "empty", POOL + OFF_BOTH),
    (Y, "small", "empty", POOL + OFF_BOTH),
    (X, "small", "other-game", POOL + OFF_VMU),
    (X_NOFAMILY, "small", "other-game", POOL + OFF_BOTH),
    (Y, "small", "other-game", POOL + OFF_BOTH),
    (X, "unresolved", "other-game", READER_ONLY),
    (X_ABSENT, "unresolved", "other-game", SDK + OFF_BOTH),
    (X_NOFAMILY, "unresolved", "other-game", SDK + OFF_BOTH),
    (Y, "unresolved", "other-game", SDK + OFF_BOTH),
)
FIRST_LINE = 76 - len("Retail boot: ")


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
    for text in {row[3] for row in ROWS} - {READER_ONLY}:
        # The first sentence fills the 76-column log line; the setting is the next line.
        assert text[FIRST_LINE] == " " and text[FIRST_LINE - 1] == "." and len(text) - FIRST_LINE <= 76, text
    with tempfile.TemporaryDirectory(prefix="kui-native-route-") as folder:
        work = Path(folder)
        command = commands[0].copy()
        command[command.index("tests/games_retail.c")] = "tests/test_games_native_route.c"
        binary = work / "route"
        command[command.index("-o") + 1] = str(binary)
        command += ["-Isrc/apps", "-fno-pie", "-no-pie", "-Wl,--wrap=kui_vmu_native_resolution_cache_build_prefixed"]
        built = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
        assert not built.returncode, built.stderr
        formatter = private_formatter(work)
        for kind in ("fat32", "exfat"):
            volume = work / (kind + ".volume")
            clean = work / (kind + ".clean")
            subprocess.run([str(formatter), kind, str(volume)], env=env, check=True, capture_output=True, text=True)
            partition_image(volume, clean, kind)
            for number, (case, answer, saved, expected) in enumerate(ROWS):
                fixture = work / f"{kind}-{number}"
                make_retail_fixture(fixture, case, 0x47000)
                card = work / f"{kind}-{number}.img"
                shutil.copyfile(clean, card)
                seeded = subprocess.run([str(binary), str(card), str(fixture), "seed", case, answer, saved],
                                        cwd=ROOT, env=env, capture_output=True, text=True)
                assert not seeded.returncode, seeded.stdout + seeded.stderr
                before = digest(card)
                ran = subprocess.run([str(binary), str(card), str(fixture), "check", case, answer, saved],
                                     cwd=ROOT, env=env, capture_output=True, text=True)
                assert not ran.returncode, ran.stdout + ran.stderr
                found = re.search(r"ROUTE prepared=(\d) standalone=(\d) seeded=(\d) family=(\d) standalone-opens=(\d) writes=(\d)", ran.stdout)
                assert found, ran.stdout
                prepared, standalone, seeded_module, family, opens, writes = map(int, found.groups())
                # A prepared launch logs other "Retail boot:" lines; only a refused one ends on its reason.
                said = None if prepared else refusal(ran.stdout)
                assert family == (0 if case == Y else 1), (case, answer, family)
                assert "Native VMU emulation: On" in ran.stdout
                assert "hint selected" not in ran.stdout, "another game's record was taken as this game's"
                if answer == "small":
                    assert "Native reader: SDK pool 448 KiB cannot spare the 284 KiB lease" in ran.stdout
                else:
                    assert "Native reader: SDK not recognized; the stage would stop" in ran.stdout
                if expected == READER_ONLY:
                    assert (prepared, standalone, seeded_module, opens) == (1, 1, 0, 1), (case, answer, saved, ran.stdout)
                    assert "using standalone New without virtual VMU" in ran.stdout
                    assert POOL not in ran.stdout and SDK not in ran.stdout
                else:
                    assert (prepared, standalone, seeded_module, opens, writes) == (0, 0, 0, 0, 0), (case, answer, saved, ran.stdout)
                    assert said == expected, (case, answer, saved, said)
                    assert digest(card) == before, "a refused launch changed the card"
                    assert "using standalone New" not in ran.stdout
                rows.append(dict(filesystem=kind, installed=case, sdk_answer=answer, card=saved, virtual_vmu="on",
                                 prepared=bool(prepared), reader_only_module=bool(standalone),
                                 refusal=said, card_unchanged=not prepared))
    return dict(schema="kui.gui.native-route.v1", result="PASS", cases=rows,
                sdk_check="authored answer after the actual check refuses the synthetic executable; "
                          "the launcher, FatFs, module admission and messages are the production code")


if __name__ == "__main__":
    report = audit()
    print(json.dumps(report, indent=2))
    print(f"PASS native Background routes: {len(report['cases'])} launches on FAT32 and exFAT; "
          "each refusal names its setting and leaves the card unchanged; another game's saved record is never taken")
