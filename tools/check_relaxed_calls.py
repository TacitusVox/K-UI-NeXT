#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Refuse a relaxed SH image in which a branch enters another function
anywhere but its first instruction.

With -mrelax/--relax and LTRANS section+addend relocations, sh-elf ld 2.45.1
can delete a literal word that sits immediately before a function and leave
branches to that function four bytes late, past the start of its
register-saving prologue. The callee then pops registers it never pushed.
Chat's Armada 67b0df9 VMU-Off reader shipped eleven such calls and stopped at
Entering Windows CE; 4d893b3 changed only those displacements and booted.
Assembling LTRANS with -Wa,--renesas keeps named callee relocations, which
relax correctly (the WWP adapter already does this).

The ELF must be linked with -Wl,-q so its R_SH_IND12W relocations identify
real bra/bsr instructions; literal pools are never mistaken for branches.
The kept relocations cannot supply the intended target (ld shifts them with
the encoding), so the rule is structural: compiled code enters another
function only at its first instruction. Function bounds run from one FUNC
symbol to the next, because relaxation does not update st_size.
"""
import argparse
import bisect
import json
import re
import struct
import subprocess
import sys


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def code_sections(elf):
    sections = []
    for line in run('sh-elf-readelf', '-SW', elf).splitlines():
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+'
                     r'([0-9a-f]+)\s+\S+\s+(\S*)', line)
        if m and 'X' in m.group(5) and 'A' in m.group(5):
            sections.append((int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
    return sections


def functions(elf):
    """FUNC symbols only: local line labels (.LM*) are not entry points."""
    starts = {}
    for line in run('sh-elf-readelf', '-sW', elf).splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[3] == 'FUNC' and parts[6] != 'UND':
            starts.setdefault(int(parts[1], 16), parts[7])
    return sorted(starts.items())


def branch_sites(elf):
    sites, current = set(), ''
    for line in run('sh-elf-readelf', '-rW', elf).splitlines():
        if line.startswith('Relocation section'):
            current = line
            continue
        if 'debug' in current:
            continue
        m = re.match(r'([0-9a-f]+)\s+[0-9a-f]+\s+R_SH_IND12W\b', line)
        if m:
            sites.add(int(m.group(1), 16))
    return sorted(sites)


def scan(elf):
    blob = open(elf, 'rb').read()
    sections, funcs = code_sections(elf), functions(elf)
    keys = [address for address, _ in funcs]

    def offset(address):
        for start, file_offset, size in sections:
            if start <= address < start + size:
                return file_offset + address - start
        return None

    def owner(address):
        if offset(address) is None:
            return None, None
        i = bisect.bisect_right(keys, address) - 1
        return funcs[i] if i >= 0 else (None, None)

    sites = branch_sites(elf)
    found = []
    for site in sites:
        at = offset(site)
        if at is None:
            continue
        insn = struct.unpack_from('<H', blob, at)[0]
        if insn >> 12 not in (0xa, 0xb):  # bra, bsr
            continue
        disp = insn & 0xfff
        disp = disp - 0x1000 if disp & 0x800 else disp
        target = site + 4 + 2 * disp
        source_start, source = owner(site)
        target_start, callee = owner(target)
        if target_start is not None and target_start != source_start and target != target_start:
            found.append(dict(site=hex(site), kind='bsr' if insn >> 12 == 0xb else 'bra',
                              caller=source, target='%s+%d' % (callee, target - target_start)))
    return len(sites), found


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('elf', nargs='+')
    args = parser.parse_args()
    failed = False
    for elf in args.elf:
        count, found = scan(elf)
        print(json.dumps(dict(elf=elf, branch_sites=count, entering_mid_function=found)))
        if not count:
            print(f'{elf}: no R_SH_IND12W relocations; link with -Wl,-q', file=sys.stderr)
        failed |= bool(found) or not count
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
