#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The family 7 placements in include/kui/ce_maple_family7.h match the record
derived from the kernels themselves (tools/wince_family7_profiles.py; the
kernels are private, the record holds their SHA-256 and derived values)."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from wince_family7_profiles import RAM_END, header_values

RECORD = ROOT / 'docs/evidence/wince-family7-profiles-2026-11-17.json'


def main():
    record = json.loads(RECORD.read_text())['kernels']
    header = header_values()
    if set(header) != set(record):
        raise AssertionError('Header and record name different kernels')
    for name, values in header.items():
        derived = record[name]
        want = {key: derived[key] for key in values}
        if values != want:
            raise AssertionError('%s: header %s, derived %s' % (name, values, want))
        if derived['ram_end'] != RAM_END or derived['maple_span'] != 0xa000 or derived['mapledev_span'] != 0x10000:
            raise AssertionError(name + ': RAM end or driver spans differ from Armada\'s')
    print('PASS family 7 placements: %d kernels agree with their derivation' % len(header))


if __name__ == '__main__':
    main()
