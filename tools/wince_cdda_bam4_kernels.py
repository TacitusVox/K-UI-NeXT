#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Derive the placement of each kernel of Bust-A-Move 4's CD music group from the kernel itself.

Bust-A-Move 4's group is the kernels that carry the reviewed Bust-A-Move 4 US
kernel's nk.exe build and its coredll.dll, sndcore.dll, dsound.dll and
wsegacd.dll with its RAM end. The method is World Party's group's
(tools/wince_cdda_wwp_kernels.py), with Bust-A-Move 4 as the reference:

- nk.exe: the reference's section layout and one data copy record with the
  same source and sizes. Every loaded word is the reference's or moved by
  exactly the shift of the kernel's data block, except the ROM header pointer;
- the four DLLs: the reference's section layout, and code equal apart from
  addresses that move with a module's base or with the data block. coredll's
  debug-directory TimeDateStamp (link metadata, located from the module's own
  debug directory, nothing the module reads) may differ and is reported. A
  kernel without sndcore.dll or dsound.dll in ROM is refused: Super Runabout's
  kernels, whose game plays its sound through its own driver without
  DirectSound;
- the ROM header: its address, the CRC32 of its 84 bytes, RAM start and free,
  and the reference's RAM end and OEM globals, so the pool and controller
  placements below RAM end hold unchanged;
- the Win32 record and the callback sentinel, unchanged;
- the load prefix's and the body's CRC32 (the stage's admission), the size and
  the SHA-256.

Kernels are private inputs: only their SHA-256 and the derived values are
written. Usage:
  wince_cdda_bam4_kernels.py --reference BAM4.BIN NAME=KERNEL.BIN ... [--output JSON] [--check-header]
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from armada_ce_rom_audit import ROMInventory  # noqa: E402
from wince_cdda_wwp_kernels import (  # noqa: E402
    CALLBACK_SENTINEL, DLLS, HEADER_KEYS, OEM, RAM_END, WIN32_RECORD, compare_dll, compare_nk, crc32)

HEADER = ROOT / 'include/kui/wince_cdda_bam4_kernels.h'
REFERENCE_SHA256 = '2a8d24738bb5ba0687fa56ad33e814fe798158f9cb8cacfd4a8a20dea6bbb35d'
LABEL = "Bust-A-Move 4's"
NAMES = ('BUST_A_MOVE_4', 'PUZZLE_BOBBLE_4', 'VIRTUA_COP_2', 'CAESARS_PALACE_2000',
         'CAESARS_PALACE_2000_PAL', 'GET_COLONY', 'MARIONETTE_COMPANY_2', 'HELLO_KITTY_MAGICAL_BLOCK')


def derive(reference, rom, raw):
    nk = compare_nk(reference, rom, LABEL)
    shift = nk['shift']
    for address, count in ((WIN32_RECORD, 3), (CALLBACK_SENTINEL, 1)):
        if rom.address_read(address, 4 * count) != reference.address_read(address, 4 * count):
            raise ValueError("The Win32 record or callback sentinel differs from " + LABEL)
    h = rom.header
    if h['ram_end'] != RAM_END or (h['driver_globals_address'], h['driver_globals_bytes']) != OEM:
        raise ValueError("RAM end or OEM globals differ from %s: other pool placement" % LABEL)
    for name in ('sndcore.dll', 'dsound.dll'):
        if name not in rom.modules:
            raise ValueError(name + ' is not in the kernel ROM: no DirectSound to bind')
    dlls = {name: compare_dll(reference, rom, name, shift, LABEL, allow_stamps=name == 'coredll.dll')
            for name in DLLS}
    return dict(
        kernel_bytes=len(raw), prefix_crc=crc32(raw[:2048]), body_crc=crc32(raw[2048:]),
        data_shift=shift, nk_words_differing=nk['differing'], nk_words_moved=nk['moved'],
        romhdr=rom.header_address, romhdr_crc=crc32(rom.address_read(rom.header_address, 84)),
        ram_start=h['ram_start'], ram_free=h['ram_free'], ram_end=h['ram_end'],
        coredll_toc=dlls['coredll.dll']['toc'], sndcore_toc=dlls['sndcore.dll']['toc'],
        dsound_toc=dlls['dsound.dll']['toc'],
        coredll_debug_stamps=dlls['coredll.dll']['stamps'],
        dll_bases={name: dlls[name]['base'] for name in DLLS},
        dll_words_compared={name: dlls[name]['words_compared'] for name in DLLS})


def header_values(text=None):
    """The values the header records for each kernel."""
    text = text if text is not None else HEADER.read_text()
    out = {}
    for name in NAMES:
        macro = 'KUI_WINCE_CDDA_BAM4_KERNEL_' + name
        match = re.search(r'#(?:el)?if KUI_WINCE_CDDA_BAM4_KERNEL == ' + macro +
                          r'\n(.*?)(?=\n#(?:elif|else|endif))', text, re.S)
        if not match:
            raise ValueError('Header has no block for ' + name)
        block = match.group(1)
        values = {}
        for key, value in re.findall(r'#define KUI_WINCE_CDDA_BAM4_(\w+) \(?(-?(?:0x[0-9a-f]+|\d+))\)?\n',
                                     block + '\n'):
            values[key.lower()] = int(value, 0)
        sha = re.search(r'#define KUI_WINCE_CDDA_BAM4_SHA256_BYTES \\\n(.*?)(?:\n(?!\s)|\Z)', block, re.S)
        if not sha:
            raise ValueError('Header block for %s has no SHA-256' % name)
        values['kernel_sha256'] = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', sha.group(1))).hex()
        out[name] = values
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--reference', type=Path, required=True, help='Bust-A-Move 4 US 0WINCEOS.BIN')
    parser.add_argument('kernels', nargs='*', help='NAME=PATH, NAME one of ' + ', '.join(NAMES[1:]))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check-header', action='store_true')
    args = parser.parse_args()
    reference_raw = args.reference.read_bytes()
    if hashlib.sha256(reference_raw).hexdigest() != REFERENCE_SHA256:
        parser.error('--reference is not the reviewed Bust-A-Move 4 US kernel')
    reference = ROMInventory(args.reference)
    inputs = [(NAMES[0], args.reference)] + [tuple(item.split('=', 1)) for item in args.kernels]
    report = {'schema': 'kui.wince.cdda-bam4-kernels.v1', 'reference': NAMES[0],
              'reference_sha256': REFERENCE_SHA256, 'kernels': {}}
    for name, path in inputs:
        if name not in NAMES:
            parser.error("Unknown Bust-A-Move 4 group kernel " + name)
        raw = Path(path).read_bytes()
        values = derive(reference, ROMInventory(Path(path)), raw)
        values['kernel_sha256'] = hashlib.sha256(raw).hexdigest()
        report['kernels'][name] = values
        stamp = values['coredll_debug_stamps']
        print('%-26s %s shift %+#8x  ROMHDR %08x crc %08x  TOCs %08x %08x %08x  nk.exe words moved %d of %d%s'
              % (name, values['kernel_sha256'][:12], values['data_shift'], values['romhdr'],
                 values['romhdr_crc'], values['coredll_toc'], values['sndcore_toc'], values['dsound_toc'],
                 values['nk_words_moved'], values['nk_words_differing'],
                 '  coredll debug stamp %#010x' % stamp[0]['kernel'] if stamp else ''))
    if args.check_header:
        recorded = header_values()
        for name, values in report['kernels'].items():
            want = {key: values[key] for key in HEADER_KEYS}
            want['kernel_sha256'] = values['kernel_sha256']
            if recorded[name] != want:
                raise SystemExit('Header values for %s differ: %s against %s' % (name, recorded[name], want))
        print('Header agrees with the kernels')
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
