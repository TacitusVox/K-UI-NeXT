#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Test 82 is a composition of parts that other builds made. Hold it by their bytes.

usage: test_toy_wire_vmu_parts.py <Test 82 build> <the same recipe without TOY_WIRE_VMU> <the installed recipe>

All three are built with one BUILD_ID (Makefile.vmu-native-toy-suite, toy-wire-vmu-parts-check).
"""
import sys
from pathlib import Path

vmu, reader, installed = (Path(p) for p in sys.argv[1:4])


def same(name, other, whose):
    a, b = (vmu / name).read_bytes(), (other / name).read_bytes()
    if a != b:
        sys.exit(f"FAIL {name} is not {whose}: {len(a)} bytes against {len(b)}")
    return len(a)


# The worker: the installed recipe's, to the byte, with its relocations.
worker = same("native-worker.bin", installed, "the installed recipe's")
same("native-worker.reloc", installed, "the installed recipe's")
same("worker.bin", installed, "the installed recipe's")
# The low residents and the auxiliary: the reader's own recipe's, which is this one without the switch.
sci = same("resident-sci.bin", reader, "the reader's recipe's")
legacy = same("resident-legacy.bin", reader, "the reader's recipe's")
aux = same("native-ra-aux.bin", reader, "the reader's recipe's")
same("native-ra-aux.reloc", reader, "the reader's recipe's")
# The parts that must differ: the worker from the reader's recipe's, the auxiliary from the installed recipe's.
if (vmu / "native-worker.bin").read_bytes() == (reader / "native-worker.bin").read_bytes():
    sys.exit("FAIL the worker is the Toy tests' worker")
if (vmu / "native-ra-aux.bin").read_bytes() == (installed / "native-ra-aux.bin").read_bytes():
    sys.exit("FAIL the auxiliary is the installed block cache")
# The installed worker was compiled for the installed auxiliary, whose image is 0x3000 bytes at most. It asks an
# auxiliary's size before it calls one, and refuses a larger image: that refusal is what keeps its controller ticks
# and its capture gesture away from the stream's auxiliary, which would take the second for a reset. The stream's
# image must stay larger than that bound.
if aux <= 0x3000:
    sys.exit(f"FAIL the auxiliary's image is {aux} bytes: the installed worker would call into it")
stage = (vmu / "stage.bin").read_bytes()
# The stage: it names the test at launch, and it has no stop for the owner's VMU setting.
if b"TEST 82 / STREAM READER / INSTALLED VMU" not in stage:
    sys.exit("FAIL the stage does not name the test at launch")
if b"THIS TEST BUILD NEEDS VIRTUAL VMU OFF" in stage:
    sys.exit("FAIL the stage still stops a launch that has the virtual VMU on")
if b"THIS TEST BUILD NEEDS VIRTUAL VMU OFF" not in (reader / "stage.bin").read_bytes():
    sys.exit("FAIL the reader's own recipe no longer has the stop this check looks for")
print(f"PASS Test 82's parts: the worker ({worker:,} bytes) is the installed recipe's and the low residents "
      f"({sci:,} and {legacy:,}) and the auxiliary ({aux:,}, larger than the installed worker will call into) are "
      "the reader's own recipe's, byte for byte with one label; the stage names the test and does not stop for the "
      "virtual VMU")
