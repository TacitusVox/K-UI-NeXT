#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Derive the placement of each kernel of World Party's CD music group from the kernel itself.

World Party's group is the kernels that carry the reviewed Worms World Party US
kernel's nk.exe build and its coredll.dll, sndcore.dll, dsound.dll and wsegacd.dll
(docs/evidence/world-party-kernel-contracts-2026-10-15.md,
docs/evidence/world-party-sound-contracts-2026-10-15.md). For each kernel this
checks that, and derives what include/kui/wince_cdda_wwp_kernels.h records:

- nk.exe: World Party's section layout and one data copy record with the same
  source and sizes. Every loaded word is World Party's or moved by exactly the
  shift of the kernel's data block (the copy destination's difference), except
  the ROM header pointer. Stored padding past a section's loaded size is not
  part of the kernel and is not compared;
- the four DLLs: World Party's section layout, and code equal apart from
  addresses that move with a module's base or with the data block. Their
  compressed data sections are compared by layout: the module checks what it
  reads of them at run time (its section records, the DirectSound object, the
  import entries by the code they reach);
- the ROM header: its address, the CRC32 of its 84 bytes, RAM start and free,
  and World Party's RAM end and OEM globals, so the pool and controller
  placements below RAM end hold unchanged;
- the Win32 record and the callback sentinel, unchanged;
- the load prefix's and the body's CRC32 (the stage's admission), the size and
  the SHA-256.

Kernels are private inputs: only their SHA-256 and the derived values are
written. Usage:
  wince_cdda_wwp_kernels.py --reference WWP.BIN NAME=KERNEL.BIN ... [--output JSON] [--check-header]
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from armada_ce_rom_audit import ROMInventory  # noqa: E402

HEADER = ROOT / 'include/kui/wince_cdda_wwp_kernels.h'
REFERENCE_SHA256 = '189719d8d3763284f74c5e66704710b9f9968118cb58aeb0a0844bd121a629ab'
NAMES = ('WORMS_WORLD_PARTY', 'DUCATI_WORLD', 'CHAMPIONSHIP_SURFER', 'NEXT_TETRIS_ONLINE',
         'RAILROAD_TYCOON_II', 'FOUR_BY_FOUR_EVOLUTION', 'STARLANCER', 'HIDDEN_AND_DANGEROUS',
         'HIDDEN_AND_DANGEROUS_PAL', 'ATARI_ANNIVERSARY', 'HOYLE_CASINO', 'BEAT_UP_A_MILLIONAIRE')
DLLS = ('coredll.dll', 'sndcore.dll', 'dsound.dll', 'wsegacd.dll')
RAM_END = 0x8cfd0000
OEM = (0x8cfd0000, 0x30000)
WIN32_RECORD, CALLBACK_SENTINEL = 0x8c011b20, 0x8c011aa4


def crc32(data):
    return zlib.crc32(data) & 0xffffffff


def loaded(rom, section):
    """The section's stored words that the kernel loads (not its stored padding)."""
    count = min(section['stored_bytes'], section['virtual_bytes']) // 4
    data = rom.address_read(section['data_address'], count * 4) if count else b''
    return struct.unpack('<%dI' % count, data)


def layout(module):
    return [(s['rva'], s['virtual_bytes'], s['flags']) for s in module['sections']]


def same_place(reference, rom, wa, wb):
    """Both words point at the same offset into a module of the same name and span."""
    for name, r_mod in reference.modules.items():
        k_mod = rom.modules.get(name)
        if not k_mod or r_mod['virtual_bytes'] != k_mod['virtual_bytes']:
            continue
        offset = (wa - r_mod['nominal_virtual_base']) & 0xffffffff
        if offset < r_mod['virtual_bytes'] and (wb - k_mod['nominal_virtual_base']) & 0xffffffff == offset:
            return True
    return False


def compare_nk(reference, rom):
    r_nk, k_nk = reference.modules['nk.exe'], rom.modules['nk.exe']
    if [(s['rva'], s['virtual_bytes'], s['stored_bytes'], s['flags']) for s in r_nk['sections']] != \
       [(s['rva'], s['virtual_bytes'], s['stored_bytes'], s['flags']) for s in k_nk['sections']]:
        raise ValueError("nk.exe is not World Party's build (section layout differs)")
    if len(reference.copies) != 1 or len(rom.copies) != 1:
        raise ValueError('Expected one kernel data copy record')
    r_copy, k_copy = reference.copies[0], rom.copies[0]
    if (k_copy['source'], k_copy['copied_bytes'], k_copy['total_bytes']) != \
       (r_copy['source'], r_copy['copied_bytes'], r_copy['total_bytes']):
        raise ValueError("The kernel data copy record's source or sizes differ from World Party's")
    shift = (k_copy['destination'] - r_copy['destination']) & 0xffffffff
    if any(s['compressed'] for s in r_nk['sections'] + k_nk['sections']):
        raise ValueError('nk.exe has a compressed section')
    # World Party's nk.exe holds its ROM header's address in exactly one word.
    pointers = [(i, at) for i, s in enumerate(r_nk['sections'])
                for at, w in enumerate(loaded(reference, s)) if w == reference.header_address]
    if len(pointers) != 1:
        raise ValueError("World Party's nk.exe must hold one ROM header pointer, found %d" % len(pointers))
    differing = moved = 0
    for i, (r_sec, k_sec) in enumerate(zip(r_nk['sections'], k_nk['sections'])):
        k_words = loaded(rom, k_sec)
        for at, (wa, wb) in enumerate(zip(loaded(reference, r_sec), k_words)):
            if (i, at) == pointers[0]:
                if wb != rom.header_address:
                    raise ValueError("nk.exe's ROM header pointer is not the kernel's ROM header")
                continue
            if wa == wb:
                continue
            differing += 1
            if (wb - wa) & 0xffffffff != shift:
                raise ValueError('nk.exe word at rva %#x differs by more than the data block shift'
                                 % (r_sec['rva'] + 4 * at))
            moved += 1
    section, at = pointers[0]
    return dict(shift=shift if shift < 0x80000000 else shift - (1 << 32), differing=differing,
                moved=moved, romhdr_pointer_rva=r_nk['sections'][section]['rva'] + 4 * at)


def compare_dll(reference, rom, name, shift):
    r_mod, k_mod = reference.modules[name], rom.modules[name]
    if layout(r_mod) != layout(k_mod) or r_mod['virtual_bytes'] != k_mod['virtual_bytes']:
        raise ValueError(name + " is not World Party's build (section layout differs)")
    base_shift = (k_mod['nominal_virtual_base'] - r_mod['nominal_virtual_base']) & 0xffffffff

    def relocated(wa, wb):
        return ((wb - wa) & 0xffffffff in (base_shift, shift & 0xffffffff) or
                same_place(reference, rom, wa, wb))
    compared = 0
    for r_sec, k_sec in zip(r_mod['sections'], k_mod['sections']):
        if r_sec['compressed'] != k_sec['compressed']:
            raise ValueError(name + ' compresses a different section')
        if r_sec['compressed']:
            continue
        if r_sec['stored_bytes'] != k_sec['stored_bytes']:
            raise ValueError(name + ' stores a different amount of an uncompressed section')
        a, b = loaded(reference, r_sec), loaded(rom, k_sec)
        unexplained = []
        for at, (wa, wb) in enumerate(zip(a, b)):
            compared += 1
            if wa == wb or relocated(wa, wb):
                continue
            # An address packed at an odd offset straddles two aligned words.
            raw_a = struct.pack('<%dI' % len(a), *a)
            raw_b = struct.pack('<%dI' % len(b), *b)
            if any(0 <= 4 * at + d and 4 * at + d + 4 <= len(raw_a) and
                   relocated(struct.unpack_from('<I', raw_a, 4 * at + d)[0],
                             struct.unpack_from('<I', raw_b, 4 * at + d)[0])
                   for d in (-3, -2, -1, 1, 2, 3)):
                continue
            unexplained.append((wa, wb))
        if unexplained:
            raise ValueError("%s differs from World Party's in %d words" % (name, len(unexplained)))
    if not compared:
        raise ValueError(name + ' has no uncompressed code to compare')
    return dict(toc=k_mod['toc_address'], base=k_mod['nominal_virtual_base'],
                span=k_mod['virtual_bytes'], words_compared=compared)


def derive(reference, rom, raw):
    nk = compare_nk(reference, rom)
    shift = nk['shift']
    for address, count in ((WIN32_RECORD, 3), (CALLBACK_SENTINEL, 1)):
        if rom.address_read(address, 4 * count) != reference.address_read(address, 4 * count):
            raise ValueError('The Win32 record or callback sentinel differs from World Party\'s')
    h = rom.header
    if h['ram_end'] != RAM_END or (h['driver_globals_address'], h['driver_globals_bytes']) != OEM:
        raise ValueError("RAM end or OEM globals differ from World Party's: other pool placement")
    dlls = {name: compare_dll(reference, rom, name, shift) for name in DLLS}
    return dict(
        kernel_bytes=len(raw), prefix_crc=crc32(raw[:2048]), body_crc=crc32(raw[2048:]),
        data_shift=shift, nk_words_differing=nk['differing'], nk_words_moved=nk['moved'],
        romhdr=rom.header_address, romhdr_crc=crc32(rom.address_read(rom.header_address, 84)),
        ram_start=h['ram_start'], ram_free=h['ram_free'], ram_end=h['ram_end'],
        coredll_toc=dlls['coredll.dll']['toc'], sndcore_toc=dlls['sndcore.dll']['toc'],
        dsound_toc=dlls['dsound.dll']['toc'],
        dll_bases={name: dlls[name]['base'] for name in DLLS},
        dll_words_compared={name: dlls[name]['words_compared'] for name in DLLS})


HEADER_KEYS = ('data_shift', 'romhdr', 'romhdr_crc', 'ram_start', 'ram_free', 'coredll_toc',
               'sndcore_toc', 'dsound_toc', 'kernel_bytes', 'prefix_crc', 'body_crc')


def header_values(text=None):
    """The values the header records for each kernel."""
    text = text if text is not None else HEADER.read_text()
    out = {}
    for name in NAMES:
        macro = 'KUI_WINCE_CDDA_WWP_KERNEL_' + name
        match = re.search(r'#(?:el)?if KUI_WINCE_CDDA_WWP_KERNEL == ' + macro +
                          r'\n(.*?)(?=\n#(?:elif|else|endif))', text, re.S)
        if not match:
            raise ValueError('Header has no block for ' + name)
        block = match.group(1)
        values = {}
        for key, value in re.findall(r'#define KUI_WINCE_CDDA_WWP_(\w+) \(?(-?(?:0x[0-9a-f]+|\d+))\)?\n', block + '\n'):
            values[key.lower()] = int(value, 0)
        sha = re.search(r'#define KUI_WINCE_CDDA_WWP_SHA256_BYTES \\\n(.*?)(?:\n(?!\s)|\Z)', block, re.S)
        if not sha:
            raise ValueError('Header block for %s has no SHA-256' % name)
        values['kernel_sha256'] = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', sha.group(1))).hex()
        out[name] = values
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--reference', type=Path, required=True, help="World Party US 0WINCEOS.BIN")
    parser.add_argument('kernels', nargs='*', help='NAME=PATH, NAME one of ' + ', '.join(NAMES[1:]))
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check-header', action='store_true')
    args = parser.parse_args()
    reference_raw = args.reference.read_bytes()
    if hashlib.sha256(reference_raw).hexdigest() != REFERENCE_SHA256:
        parser.error('--reference is not the reviewed World Party US kernel')
    reference = ROMInventory(args.reference)
    inputs = [(NAMES[0], args.reference)] + [tuple(item.split('=', 1)) for item in args.kernels]
    report = {'schema': 'kui.wince.cdda-wwp-kernels.v1', 'reference': NAMES[0],
              'reference_sha256': REFERENCE_SHA256, 'kernels': {}}
    for name, path in inputs:
        if name not in NAMES:
            parser.error('Unknown World Party group kernel ' + name)
        raw = Path(path).read_bytes()
        values = derive(reference, ROMInventory(Path(path)), raw)
        values['kernel_sha256'] = hashlib.sha256(raw).hexdigest()
        report['kernels'][name] = values
        print('%-26s %s shift %+#8x  ROMHDR %08x crc %08x  TOCs %08x %08x %08x  nk.exe words moved %d of %d'
              % (name, values['kernel_sha256'][:12], values['data_shift'], values['romhdr'],
                 values['romhdr_crc'], values['coredll_toc'], values['sndcore_toc'], values['dsound_toc'],
                 values['nk_words_moved'], values['nk_words_differing']))
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
