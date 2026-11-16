#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Derive the placement of each maple.dll family 7 kernel from the kernel itself.

Family 7 (docs/wince-vmu-driver-families.md) is Armada's: the same nk.exe build
and the same maple.dll and mapledev.dll. For each kernel this checks that, and
derives what include/kui/ce_maple_family7.h records:

- the shift of the kernel's data block from Armada's (its copy destination);
  nearly every nk.exe word that differs from Armada's must differ by exactly it;
- maple.dll's and mapledev.dll's ROM table entries and nominal bases, with
  their code equal to Armada's apart from addresses that move with a module's
  base (their compressed data sections only need equal stored sizes);
- the RAM end, which the Armada reservations below it rely on.

Kernels are private inputs: only their SHA-256 and the derived values are
written. Usage:
  wince_family7_profiles.py --armada ARMADA.BIN NAME=KERNEL.BIN ... [--output JSON] [--check-header]
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from armada_ce_rom_audit import ROMInventory

HEADER = ROOT / 'include/kui/ce_maple_family7.h'
NAMES = {'ARMADA': 'KUI_CE_FAMILY7_ARMADA', 'WILD_METAL': 'KUI_CE_FAMILY7_WILD_METAL',
         'BIOHAZARD2_VALUE_PLUS': 'KUI_CE_FAMILY7_BIOHAZARD2_VALUE_PLUS',
         'HELLO_KITTY_LOVELY_FRUIT_PARK': 'KUI_CE_FAMILY7_HELLO_KITTY_LOVELY_FRUIT_PARK'}
RAM_END = 0x8cfd0000
# At most this many of nk.exe's differing words may move by anything but the
# data shift: the ROM header pointer and four values that follow where RAM
# starts. Every family 7 kernel has exactly these five.
OTHER_LIMIT = 5


def words(data):
    return struct.unpack('<%dI' % (len(data) // 4), data[:len(data) // 4 * 4])


def sections(rom, name):
    module = rom.modules[name]
    return module, [rom.address_read(s['data_address'], s['stored_bytes']) for s in module['sections']]


def layout(module):
    return [(s['rva'], s['virtual_bytes'], s['stored_bytes'], s['flags']) for s in module['sections']]


def same_place(armada, rom, wa, wb):
    """Both words point at the same offset into a module of the same name."""
    for name, a_mod in armada.modules.items():
        k_mod = rom.modules.get(name)
        if not k_mod or a_mod['virtual_bytes'] != k_mod['virtual_bytes']:
            continue
        offset = (wa - a_mod['nominal_virtual_base']) & 0xffffffff
        if offset < a_mod['virtual_bytes'] and (wb - k_mod['nominal_virtual_base']) & 0xffffffff == offset:
            return True
    return False


def derive(armada, rom):
    a_nk, a_data = sections(armada, 'nk.exe')
    k_nk, k_data = sections(rom, 'nk.exe')
    if layout(a_nk) != layout(k_nk):
        raise ValueError('nk.exe is not Armada\'s build (section layout differs)')
    if len(armada.copies) != 1 or len(rom.copies) != 1:
        raise ValueError('Expected one kernel data copy record')
    shift = rom.copies[0]['destination'] - armada.copies[0]['destination']
    differing = moved = 0
    for x, y in zip(a_data, k_data):
        for wa, wb in zip(words(x), words(y)):
            if wa != wb:
                differing += 1
                moved += (wb - wa) & 0xffffffff == shift & 0xffffffff
    if differing - moved > OTHER_LIMIT:
        raise ValueError('nk.exe differs by more than its data block shift')
    drivers = {}
    for name in ('maple.dll', 'mapledev.dll'):
        a_mod, a_secs = sections(armada, name)
        k_mod, k_secs = sections(rom, name)
        if layout(a_mod) != layout(k_mod) or a_mod['virtual_bytes'] != k_mod['virtual_bytes']:
            raise ValueError(name + ' is not Armada\'s driver')
        base_shift = (k_mod['nominal_virtual_base'] - a_mod['nominal_virtual_base']) & 0xffffffff
        def relocated(wa, wb):
            return ((wb - wa) & 0xffffffff in (base_shift, shift & 0xffffffff) or
                    same_place(armada, rom, wa, wb))
        unexplained = []
        # Compressed sections (initialized data) only need equal stored sizes:
        # their bytes cannot be lined up word by word. The code is compared.
        plain = [(x, y) for x, y, sec in zip(a_secs, k_secs, a_mod['sections']) if not sec['compressed']]
        if [s['stored_bytes'] for s in a_mod['sections']] != [s['stored_bytes'] for s in k_mod['sections']]:
            raise ValueError(name + ' stored section sizes differ from Armada\'s')
        for x, y in plain:
            for at in range(0, min(len(x), len(y)) // 4 * 4, 4):
                wa, wb = struct.unpack_from('<I', x, at)[0], struct.unpack_from('<I', y, at)[0]
                if wa == wb or relocated(wa, wb):
                    continue
                # An address packed at an odd offset (import data) straddles two aligned words.
                if any(0 <= at + d and at + d + 4 <= min(len(x), len(y)) and
                       relocated(struct.unpack_from('<I', x, at + d)[0], struct.unpack_from('<I', y, at + d)[0])
                       for d in (-3, -2, -1, 1, 2, 3)):
                    continue
                unexplained.append((wa, wb))
        if unexplained:
            raise ValueError('%s code differs from Armada\'s in %d words: %s' % (
                name, len(unexplained), ', '.join('%08x/%08x' % pair for pair in unexplained[:6])))
        drivers[name] = dict(toc=k_mod['toc_address'], base=k_mod['nominal_virtual_base'],
                             span=k_mod['virtual_bytes'])
    if rom.header['ram_end'] != RAM_END:
        raise ValueError('RAM end differs from Armada\'s')
    return dict(data_shift=shift, nk_words_differing=differing, nk_words_moved=moved,
                maple_base=drivers['maple.dll']['base'], maple_toc=drivers['maple.dll']['toc'],
                mapledev_base=drivers['mapledev.dll']['base'], mapledev_toc=drivers['mapledev.dll']['toc'],
                maple_span=drivers['maple.dll']['span'], mapledev_span=drivers['mapledev.dll']['span'],
                ram_end=rom.header['ram_end'])


def header_values(text=None):
    """The values the header records for each kernel."""
    text = text if text is not None else HEADER.read_text()
    out = {}
    for name, macro in NAMES.items():
        match = re.search(r'#(?:el)?if KUI_CE_FAMILY7_KERNEL == ' + macro + r'\n(.*?)(?=\n#(?:elif|else|endif))',
                          text, re.S)
        if not match:
            raise ValueError('Header has no block for ' + name)
        values = dict(re.findall(r'#define KUI_CE_FAMILY7_(\w+) \(?(-?0x[0-9a-f]+)\)?', match.group(1)))
        out[name] = {key.lower(): int(value, 16) for key, value in values.items()}
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--armada', type=Path, required=True)
    parser.add_argument('kernels', nargs='*', help='NAME=PATH, NAME one of ' + ', '.join(NAMES))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check-header', action='store_true')
    args = parser.parse_args()
    armada = ROMInventory(args.armada)
    inputs = [('ARMADA', args.armada)] + [tuple(item.split('=', 1)) for item in args.kernels]
    report = {'schema': 'kui.wince.family7-profiles.v1', 'reference': 'ARMADA', 'kernels': {}}
    for name, path in inputs:
        if name not in NAMES:
            parser.error('Unknown family 7 kernel ' + name)
        path = Path(path)
        values = derive(armada, ROMInventory(path))
        values['kernel_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        report['kernels'][name] = values
        print('%-30s shift %+#x  maple %08x toc %08x  mapledev %08x toc %08x  nk.exe words moved %d of %d'
              % (name, values['data_shift'], values['maple_base'], values['maple_toc'],
                 values['mapledev_base'], values['mapledev_toc'], values['nk_words_moved'],
                 values['nk_words_differing']))
    if args.check_header:
        recorded = header_values()
        for name, values in report['kernels'].items():
            want = {key: values[key] for key in recorded[name]}
            if recorded[name] != want:
                raise SystemExit('Header values for %s differ: %s against %s' % (name, recorded[name], want))
        print('Header agrees with the kernels')
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
