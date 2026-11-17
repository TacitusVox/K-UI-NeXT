#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Find each function of a GUI build (an ELF with symbols) in another GUI's payload.

A function matches when its bytes occur once in the other payload with
absolute addresses (words 0x8c000000-0x8cffffff) and branches that leave the
function masked. Used to tell a stripped GUI's own changes from its base:
build the base with the same flags, and every unmatched function, and every
uncovered stretch of the other payload, is where the two differ.

usage: gui_function_match.py ELF PAYLOAD [--json OUT]
PAYLOAD is a runtime.kui or its bare payload, loaded at 0x8c010000.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

BASE = 0x8c010000
sys.path.insert(0, str(Path(__file__).resolve().parent))
from runtime_package import verify  # noqa: E402


def functions(elf):
    out = subprocess.run(['sh-elf-nm', '-S', '--defined-only', str(elf)], capture_output=True, text=True, check=True)
    found = []
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2] in 'tT' and int(parts[1], 16) >= 8:
            found.append((int(parts[0], 16), int(parts[1], 16), parts[3]))
    return sorted(found)


def pattern(image, addr, size):
    code = image[addr - BASE:addr - BASE + size]
    mask = bytearray(b'\xff' * size)
    for off in range(0, size - 3, 2):
        if (addr + off) % 4 == 0 and 0x8c000000 <= struct.unpack_from('<I', code, off)[0] < 0x8d000000:
            mask[off:off + 4] = b'\0\0\0\0'
    for off in range(0, size - 1, 2):
        half = struct.unpack_from('<H', code, off)[0]
        if half >> 12 in (0xa, 0xb):  # bra, bsr
            disp = half & 0xfff
            target = addr + off + 4 + 2 * (disp - 0x1000 if disp & 0x800 else disp)
            if not addr <= target < addr + size:
                mask[off] = 0
                mask[off + 1] &= 0xf0
    return bytes(code), bytes(mask)


def occurrences(other, code, mask):
    best, run_start = (0, 0), None
    for i in range(len(mask) + 1):
        if i < len(mask) and mask[i] == 0xff:
            run_start = i if run_start is None else run_start
        else:
            if run_start is not None and i - run_start > best[1] - best[0]:
                best = (run_start, i)
            run_start = None
    if best[1] - best[0] < 6:
        return []
    anchor, hits = code[best[0]:best[1]], []
    at = other.find(anchor)
    while at != -1:
        start = at - best[0]
        if start >= 0 and start % 2 == 0 and start + len(code) <= len(other):
            if all((x ^ y) & m == 0 for x, y, m in zip(other[start:start + len(code)], code, mask)):
                hits.append(start)
        at = other.find(anchor, at + 1)
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('elf', type=Path)
    parser.add_argument('payload', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as scratch:
        flat = Path(scratch) / 'text.bin'
        subprocess.run(['sh-elf-objcopy', '-O', 'binary', '-j', '.text', str(args.elf), str(flat)], check=True)
        image = flat.read_bytes()
    data = args.payload.read_bytes()
    if data[:8] == b'KUIRUN1\0':
        data = data[len(data) - verify(data)['payload_bytes']:]
    matched, unmatched, shift = [], [], 0
    for addr, size, name in functions(args.elf):
        if not BASE <= addr < BASE + len(image):
            continue
        hits = occurrences(data, *pattern(image, addr, size))
        if not hits:
            unmatched.append((name, addr, size))
            continue
        hit = min(hits, key=lambda h: abs(h - (addr - BASE + shift)))
        shift = hit - (addr - BASE)
        matched.append((name, addr, size, hit + BASE))
    print('functions %d: matched %d, unmatched %d' % (len(matched) + len(unmatched), len(matched), len(unmatched)))
    for name, addr, size in unmatched:
        print('UNMATCHED %08x %6d %s' % (addr, size, name))
    if args.json:
        args.json.write_text(json.dumps({'matched': matched, 'unmatched': unmatched}, indent=1) + '\n')


if __name__ == '__main__':
    main()
