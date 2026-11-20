# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the GUI's negotiated map against both fixed reader decoders."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="kui-track-capacity-") as workspace:
        tmp = Path(workspace)
        fatfs = ROOT / "fatfs/source"
        config = (fatfs / "ffconf.h").read_text()
        assert "#define FF_USE_MKFS 0" in config
        cfg = tmp / "ffconf.h"
        cfg.write_text(config.replace("#define FF_USE_MKFS 0", "#define FF_USE_MKFS 1"))
        cc = shlex.split(os.environ.get("CC", "cc"))
        common = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                  "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                  "-fsanitize=undefined", "-fno-sanitize-recover=all",
                  "-Iinclude", "-Isrc/dreamcast", "-Itests", "-I" + str(fatfs)]
        source = "tests/test_native_track_capacity.c"
        image = "src/core/retail_image.c"
        subprocess.run(cc + common + ["-include", str(cfg), "-DTEST_GUI=1",
                       "-DKUI_RETAIL_MANIFEST_CAPACITY=1", "-DKUI_RETAIL_EXTENDED_MAP=1",
                       source, image, "src/core/destination.c", str(fatfs / "ff.c"),
                       str(fatfs / "ffunicode.c"), "-o", str(tmp / "gui")], cwd=ROOT, check=True)
        for capacity in (64, 160):
            subprocess.run(cc + common + ["-DTEST_GUI=0", f"-DKUI_RETAIL_ASYNC_SLOTS={capacity}",
                           source, image,
                           "src/core/retail_cursor.c", "src/core/retail_gd.c",
                           "-o", str(tmp / f"stage{capacity}")], cwd=ROOT, check=True)
        for tracks, runs in ((37, 49), (99, 59), (99, 60), (37, 25), (37, 26)):
            for stride in (2048, 2352):
                card, wire = tmp / "card.img", tmp / "wire.bin"
                for cap in (64, 160):
                    (tmp / f"wire.bin-{cap}").unlink(missing_ok=True)
                subprocess.run([tmp / "gui", card, wire, str(tracks), str(runs), str(stride)], check=True)
                for producer in (64, 160):
                    selected = tmp / f"wire.bin-{producer}"
                    assert selected.exists() == (tracks + runs + 2 <= producer)
                    if selected.exists():
                        for cap in (64, 160):
                            subprocess.run([tmp / f"stage{cap}", card, selected, str(tracks), str(stride)], check=True)
            print(f"PASS {tracks} tracks, {runs} physical T03 extents, raw and cooked", flush=True)


if __name__ == "__main__":
    main()
