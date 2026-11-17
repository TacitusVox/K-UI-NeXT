#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the GUI's patched KOS BBA worker, DMA lifetime and TX path on the host."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
KOS = Path(os.environ.get("KOS_BASE", ROOT / ".deps/kos"))
FUNCTIONS = ("bba_dma_cb", "bba_link_is_stable", "bba_tx_dma", "bba_rtx",
             "bba_tx", "bba_stop_receive", "bba_if_start", "bba_if_stop")


def main():
    with tempfile.TemporaryDirectory() as temp:
        dest = Path(temp)
        driver = dest / "bba.c"
        subprocess.run(["patch", "--batch", "--fuzz=0", "-o", str(driver),
                        str(KOS / "kernel/arch/dreamcast/hardware/network/broadband_adapter.c"),
                        str(ROOT / "tools/patches/bba-session.patch")], check=True)
        source = driver.read_text()
        routines = []
        for name in FUNCTIONS:
            found = re.search(r"^(?:static )?\w+ " + name + r"\([^;{}]+\)\s*\{.*?^\}",
                              source, re.M | re.S)
            if not found:
                raise ValueError("Missing pinned BBA routine " + name)
            routines.append(found.group())
        (dest / "kos_bba_routines.h").write_text("\n".join(routines))
        binary = dest / "bba-driver-test"
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-I"+temp, str(ROOT / "tests/kos_bba_driver_host.c"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


if __name__ == "__main__":
    main()
