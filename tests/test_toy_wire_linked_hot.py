#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check which address a linked Test 76 auxiliary uses for the window's state.

Test 76 (HOT) reaches the verified movie window's own state through the
cache. The state is the last member of the auxiliary's context, which is
linked at its uncached (P2) address; the reader's C names the member only
through KUI_TOY_AHEAD (src/loader/native_readahead_aux_internal.h), which is
the member's linked address less the distance between the two views of the
same memory. No host test can see this: a host build has one view.

Mixing the two views would be the fault to fear. A write through the cache
stays in the cache until its line is written back; a read of the same bytes
at the uncached address in between sees what memory held before. So this
reads the module as linked and checks that

- the context's size is a whole number of cache lines and the state is its
  last three (the C asserts the same of the struct);
- the code holds address constants into the state at its cached address;
- it holds none into the state at its uncached address;
- with --without, for a build that leaves HOT out: the reverse, and no
  cached address into the context at all.

It checks constants, which is how this compiler addresses a member of a
global. It does not prove that no address is computed some other way; the
source keeps that from being written by giving the member another name in
the linked module. And it does not check the Dreamcast.

usage: test_toy_wire_linked_hot.py native-ra-aux.elf [--without]
"""
import struct
import subprocess
import sys

CONTEXT = "_kui_native_readahead_aux_context"
STATE_BYTES = 96  # three cache lines, the context's last
VIEWS = 0x20000000  # P2 less P1


def symbol(elf, name, nm="sh-elf-nm"):
    for line in subprocess.run([nm, "-S", elf], check=True, capture_output=True, text=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == name:
            return int(parts[0], 16), int(parts[1], 16)
    raise SystemExit(f"{elf}: no symbol {name}")


def text(elf):
    """The .text section's address and bytes, read from the ELF itself."""
    data = open(elf, "rb").read()
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2E)
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    names = sections[shstrndx]
    for name, _type, _flags, address, offset, size, *_ in sections:
        end = data.index(b"\0", names[4] + name)
        if data[names[4] + name:end] == b".text":
            return address, data[offset:offset + size]
    raise SystemExit(f"{elf}: no .text")


def main():
    arguments = [a for a in sys.argv[1:] if a != "--without"]
    without = "--without" in sys.argv[1:]
    if len(arguments) != 1:
        raise SystemExit(__doc__)
    elf = arguments[0]
    context, size = symbol(elf, CONTEXT)
    if context >> 29 != 0b101:
        raise AssertionError(f"the context is linked at {context:#x}, not at an uncached address")
    # Without HOT the member is not padded to whole lines: its own 72 bytes.
    state = context + size - (72 if without else STATE_BYTES)
    if not without and (size % 32 or state % 32):
        raise AssertionError(f"context {context:#x}+{size:#x}: the window's state does not own whole cache lines")
    address, code = text(elf)
    cached = uncached = elsewhere = 0
    for at in range(0, len(code) - 3, 4):
        value, = struct.unpack_from("<I", code, at)
        if state <= value < context + size:
            uncached += 1
        elif state - VIEWS <= value < context + size - VIEWS:
            cached += 1
        elif context - VIEWS <= value < state - VIEWS:
            elsewhere += 1
    if elsewhere:
        raise AssertionError(f"{elsewhere} constants reach the rest of the context at its cached address")
    if without:
        if cached or not uncached:
            raise AssertionError(f"without HOT: {cached} constants at the cached address, {uncached} at the uncached one")
        print(f"PASS without HOT the window's state is used at its linked address {state:#x}: {uncached} constants, "
              "none at the cached one")
        return 0
    if uncached or not cached:
        raise AssertionError(f"{uncached} constants reach the window's state at its uncached address, {cached} at the cached one")
    print(f"PASS the window's state, the context's last three cache lines at {state:#x}, is used at its cached address "
          f"{state - VIEWS:#x} only: {cached} constants in the linked code, none at the uncached address")
    return 0


if __name__ == "__main__":
    sys.exit(main())
