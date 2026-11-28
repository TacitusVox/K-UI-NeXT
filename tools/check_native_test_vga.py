#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify that the packaged native test reader references the shared VGA boot stage."""
import argparse
import subprocess
from pathlib import Path

from retail_package import STAGE_BLOB_OFFSET
from runtime_package import verify


def symbols(*options):
    lines = subprocess.check_output(["sh-elf-nm", *options], text=True).splitlines()
    return {tuple(line.split()[-2:]) for line in lines if line.split()}


def check(build):
    # This reference must come from the relay assembly itself. A VGA helper
    # linked into the same ELF could otherwise remain unused at handoff.
    if ("U", "_kui_native_vga_handoff") not in symbols("-u", build / "stage_asm.o"):
        raise ValueError("Native test boot relay lacks the VGA handoff")
    required = {("T", "_kui_native_vga_handoff"), ("b", "_native_vga_profile"),
                ("r", "_kui_game_vga_profiles")}
    if not required.issubset(symbols("--defined-only", build / "stage.elf")):
        raise ValueError("Linked native test stage lacks the VGA handoff, candidate or catalog")
    package = (build / "retail-maple-observe.kui").read_bytes()
    verify(package)
    if package[64 + STAGE_BLOB_OFFSET:] != (build / "stage.bin").read_bytes():
        raise ValueError("Packaged native test module differs from the linked VGA boot stage")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    try:
        check(parser.parse_args().build)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    print("PASS native test reader VGA handoff, catalog and packaged boot stage")
