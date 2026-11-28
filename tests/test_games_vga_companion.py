#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise VGA game and multidisc preparation on real, synthetic FAT32 and exFAT cards."""
import shutil
import tempfile
import zlib
from pathlib import Path

from games_fixture import dual32
from test_games_library_images import private_formatter
from test_games_retail import BINARY, make_retail_fixture
from test_images import run
from test_loader_probe_images import partition_image


def stage(fixture, case):
    make_retail_fixture(fixture, "launch-report")
    track = fixture / "track03.bin"
    original = bytearray(track.read_bytes())
    original[16 + 48:16 + 56] = b"U       "
    if case == "ce-valid":
        original[16 + 62] = ord("1")
    if case == "cue":
        (fixture / "title.cue").write_text(
            'REM SINGLE-DENSITY AREA\nFILE "track01.bin" BINARY\n TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n'
            'FILE "music track02.raw" BINARY\n TRACK 02 AUDIO\n  INDEX 01 00:00:00\n'
            'REM HIGH-DENSITY AREA\nFILE "track03.bin" BINARY\n TRACK 03 MODE1/2352\n  INDEX 01 00:00:00\n')
    track.write_bytes(original)
    candidate = bytearray(original)
    candidate[16 + 48:16 + 56] = b"JUE     "
    candidate[16 + 56:16 + 64] = b"0799810 "
    candidate[21 * 2352 + 16] ^= 42  # main executable
    candidate[40 * 2352 + 16] ^= 73  # secondary game data
    if case == "region":
        candidate[16 + 48:16 + 56] = b"JE      "
    for key, offset in (("date", 87), ("product", 64), ("revision", 78), ("disc", 45), ("boot", 96)):
        if case == key:
            candidate[16 + offset] = ord("2") if key != "boot" else ord("B")
    if case == "boot":
        candidate[20 * 2352 + 16 + 68 + 33] = ord("B")
    if case in ("ce", "ce-valid"):
        candidate[16 + 62] = ord("1")
    if case == "bad-iso":
        candidate[16 * 2352 + 17] ^= 1
    if case == "late":
        # The ISO admits the extent. The retail reader later refuses a boot below 128 bytes.
        dual32(candidate, 20 * 2352 + 16 + 68 + 10, 64)
    (fixture / "vga-track03.bin").write_bytes(candidate)
    for name in ("track01.bin", "music track02.raw"):
        shutil.copyfile(fixture / name, fixture / ("vga-" + name))
    gdi = (fixture / "disc.gdi").read_bytes().replace(b"\n", b"\r\n")
    (fixture / "vga-disc.gdi").write_bytes(b"malformed\n" if case == "malformed" else gdi)


def verify(fixture, case, output):
    if case in ("peer", "cancel"):
        return
    prepared = case in ("valid", "cue", "ce-valid")
    original = "title.cue" if case == "cue" else "disc.gdi"
    path = "/games/reader test/" + (f".kui-vga/{original}/disc.gdi" if prepared else original)
    data, checksum = (fixture / "after-last-launch-a.txt").read_bytes().rsplit(b"crc32: ", 1)
    assert int(checksum, 16) == zlib.crc32(data)
    fields = dict(line.split(": ", 1) for line in data.decode().splitlines()[1:])
    assert fields["image"] == path, fields["image"]
    assert fields["reader"] == "Standard" and fields["product"] == "KUI-TEST"
    assert fields["encoding"] == ("Windows CE prefix + body" if case == "ce-valid" else "Plain")
    source = (fixture / ("vga-track03.bin" if prepared else "track03.bin")).read_bytes()
    logical = b"".join(source[i + 16:i + 2064] for i in range(0, len(source), 2352))
    assert int(fields["source executable crc32"], 16) == zlib.crc32(logical[21 * 2048:21 * 2048 + 3001])
    assert int(fields["ip crc32"], 16) == zlib.crc32(logical[:32768])
    if case in ("tv", "missing"):
        assert "VGA companion" not in output
    elif prepared:
        assert f"VGA companion: /Games/Reader Test/.kui-vga/{original}/disc.gdi" in output
    elif case == "late":
        assert "VGA companion could not be prepared" in output
    else:
        assert "using selected image" in output


def main():
    cases = ("valid", "cue", "ce-valid", "peer", "tv", "missing", "incomplete", "malformed", "bad-iso", "region",
             "date", "product", "revision", "disc", "ce", "boot", "late", "cancel")
    with tempfile.TemporaryDirectory(prefix="kui-vga-media-") as workspace:
        base = Path(workspace)
        formatter = private_formatter(base)
        fixture = base / "fixture"
        for kind in ("fat32", "exfat"):
            volume, clean, working = (base / name for name in ("volume.img", "mbr.img", "card.img"))
            run(str(formatter), kind, str(volume))
            partition_image(volume, clean, kind)
            for case in cases:
                stage(fixture, case)
                shutil.copyfile(clean, working)
                name = "launch-vga-" + case
                run(BINARY, str(working), str(fixture), "seed", name)
                output = run(BINARY, str(working), str(fixture), "check", name)
                verify(fixture, case, output)
                print(f"PASS {kind} {name}", flush=True)
            volume.unlink()
            clean.unlink()


if __name__ == "__main__":
    main()
