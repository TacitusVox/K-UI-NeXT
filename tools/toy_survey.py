#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The stream survey's report file, KUI/SURVEY/STREAM.TXT (include/kui/toy_survey.h).

  toy_survey.py blank OUT [--slots N] [--id HEX]   make a blank file
  toy_survey.py read FILE [--json OUT]             list a returned file's slots

The file is text in 512-byte sectors: the first names the file, then three
for each slot: the launch record Test 83's cold stage writes, and the two
halves of the report its reader saves at the report gesture.

A slot's state, as `read` lists it:
  empty                       its three sectors are blank
  launched, no report saved   a launch record, the report's sectors blank
  launched, report cut off    a launch record, one of the report's sectors blank
  report saved                a launch record and the reader's report
  report damaged              a launch record; the other two sectors are neither blank nor a report
  damaged                     the first sector is neither blank nor this slot's launch record,
                              or it is blank and the others are not
A launch record whose lines are not all read is listed with the line it stops at.
"""
import argparse
import json
import re
import secrets
import sys
from pathlib import Path

SECTOR = 512
PARTS = 3
SLOTS_MAX = 999
DEFAULT_SLOTS = 100
# The reader's report (src/loader/toy_suite_report_save.inc): 1,024 bytes of text.
REPORT_HEAD = b'K-UI TOY COMMANDER REPORT\nTEST '
REPORT_TAIL = b'\nEND REPORT\n'


def sector(text):
    data = text.encode('ascii')
    if len(data) > SECTOR - 1:
        raise ValueError('text does not fit a sector')
    return data + b' ' * (SECTOR - 1 - len(data)) + b'\n'


def file_sector(file_id, slots):
    return sector('K-UI STREAM SURVEY FILE 1\nID %s\nSLOTS %d\n'
                  'EACH LAUNCH OF A GAME WITH THE SURVEY MODULE FILLS ONE SLOT BELOW.\n'
                  'SEND THIS FILE BACK AS IT IS.\n' % (file_id.hex().upper(), slots))


def slot_line(file_id, slot, part, state):
    return '==== SLOT %03d PART %d ==== ID %s ==== %s ====\n' % (slot, part, file_id.hex().upper(), state)


def blank_sector(file_id, slot, part):
    return sector(slot_line(file_id, slot, part, 'EMPTY'))


def blank_file(file_id, slots):
    if len(file_id) != 16 or not 1 <= slots <= SLOTS_MAX:
        raise ValueError('a file has a 16-byte ID and 1 to %d slots' % SLOTS_MAX)
    return file_sector(file_id, slots) + b''.join(
        blank_sector(file_id, slot, part) for slot in range(slots) for part in range(PARTS))


def parse_file_sector(data):
    match = re.match(rb'K-UI STREAM SURVEY FILE 1\nID ([0-9A-F]{32})\nSLOTS ([1-9][0-9]{0,2})\n', data)
    if not match:
        raise ValueError('not a stream survey file')
    file_id, slots = bytes.fromhex(match.group(1).decode()), int(match.group(2))
    if data != file_sector(file_id, slots):
        raise ValueError('the first sector is not what its ID and slot count give')
    return file_id, slots


LAUNCH_FIELDS = (
    ('survey', r'SURVEY (\d+) BUILD (\S*)\n', ('survey', 'build')),
    ('product', r'PRODUCT (.*)\n', ('product',)),
    ('title', r'TITLE (.*)\n', ('title',)),
    ('boot', r'BOOT (.*) REGION (.*)\n', ('bootfile', 'region')),
    ('crc', r'IPCRC ([0-9A-F]{8}) GDICRC ([0-9A-F]{8}) BOOTCRC ([0-9A-F]{8}) BOOTBYTES ([0-9A-F]{8})\n',
     ('ip_crc32', 'gdi_crc32', 'boot_crc32', 'boot_bytes')),
    ('exec', r'EXECCRC ([0-9A-F]{8}) EXECBYTES ([0-9A-F]{8})\n', ('exec_crc32', 'exec_bytes')),
    ('reader', r'READER ([0-9A-F]) TRANSPORT ([0-9A-F]) FLAGS ([0-9A-F]{8}) SOURCE ([0-9A-F])\n',
     ('reader', 'transport', 'flags', 'source')),
    ('tracks', r'TRACKS (\d+) RAW (\d+) COOKED (\d+) AUDIO (\d+) HDAUDIO (\d+) MAPPED (\d+) OTHER (\d+) EXTENTS (\d+)\n',
     ('tracks', 'raw', 'cooked', 'audio', 'audio_high', 'audio_mapped', 'other', 'extents')),
    ('session', r'SESSION ([0-9A-F]{8}) BOOTLBA ([0-9A-F]{8})\n', ('session_lba', 'boot_lba')),
)
DECIMAL = {'survey', 'tracks', 'raw', 'cooked', 'audio', 'audio_high', 'audio_mapped', 'other', 'extents'}
TEXT = {'build', 'product', 'title', 'bootfile', 'region'}


def parse_launch(file_id, slot, data):
    """The launch record's fields, or None if the sector is no launch record of this slot."""
    head = slot_line(file_id, slot, 0, 'LAUNCH').encode()
    if not data.startswith(head) or data[-1:] != b'\n':
        return None
    text, at, record = data.decode('ascii', 'replace'), len(head), {}
    for name, pattern, keys in LAUNCH_FIELDS:
        match = re.compile(pattern).match(text, at)
        if not match:
            record['damaged'] = name
            return record
        for key, value in zip(keys, match.groups()):
            record[key] = value if key in TEXT else int(value, 10 if key in DECIMAL else 16)
        at = match.end()
    return record


def is_report(data):
    """Whether two sectors hold a report as the reader saves it."""
    return len(data) == 2 * SECTOR and data.startswith(REPORT_HEAD) and data.endswith(REPORT_TAIL)


def read_file(data):
    file_id, slots = parse_file_sector(data[:SECTOR])
    if len(data) != SECTOR * (1 + PARTS * slots):
        raise ValueError('the file has %d bytes, not the %d its %d slots take'
                         % (len(data), SECTOR * (1 + PARTS * slots), slots))
    out = []
    for slot in range(slots):
        parts = [data[SECTOR * (1 + PARTS * slot + part):][:SECTOR] for part in range(PARTS)]
        blank = [parts[part] == blank_sector(file_id, slot, part) for part in range(PARTS)]
        entry = {'slot': slot}
        if all(blank):
            entry['state'] = 'empty'
        else:
            launch = None if blank[0] else parse_launch(file_id, slot, parts[0])
            if launch is None:
                entry['state'] = 'damaged'
            else:
                entry['launch'] = launch
                if blank[1] and blank[2]:
                    entry['state'] = 'launched, no report saved'
                elif blank[1] or blank[2]:
                    entry['state'] = 'launched, report cut off'
                elif is_report(parts[1] + parts[2]):
                    entry['state'] = 'report saved'
                else:
                    entry['state'] = 'report damaged'
                if not (blank[1] and blank[2]):
                    entry['report'] = (parts[1] + parts[2]).decode('ascii', 'replace')
        out.append(entry)
    return {'id': file_id.hex().upper(), 'slots': slots, 'entries': out}


def listing(name, result):
    """The lines `read` prints: the file, then every slot that is not empty."""
    used = [entry for entry in result['entries'] if entry['state'] != 'empty']
    lines = ['%s: ID %s, %d of %d slots used' % (name, result['id'], len(used), result['slots'])]
    for entry in used:
        launch = entry.get('launch', {})
        line = 'slot %03d: %s' % (entry['slot'], entry['state'])
        if launch:
            line += ': %s %s (build %s; source %s; tracks %s, CD music tracks %s)' % (
                launch.get('product', '?'), launch.get('title', '?'), launch.get('build', '?'),
                launch.get('source', '?'), launch.get('tracks', '?'), launch.get('audio_high', '?'))
        if 'damaged' in launch:
            line += '; LAUNCH RECORD DAMAGED at its %s line' % launch['damaged']
        lines.append(line)
    return lines


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    blank = commands.add_parser('blank', help='make a blank file')
    blank.add_argument('out', type=Path)
    blank.add_argument('--slots', type=int, default=DEFAULT_SLOTS)
    blank.add_argument('--id', help='32 hex digits (default: random)')
    read = commands.add_parser('read', help="list a returned file's slots")
    read.add_argument('file', type=Path)
    read.add_argument('--json', type=Path)
    args = parser.parse_args(arguments)
    try:
        if args.command == 'blank':
            if args.id is not None and not re.fullmatch('[0-9A-Fa-f]{32}', args.id):
                raise ValueError('--id takes 32 hex digits')
            file_id = bytes.fromhex(args.id) if args.id else secrets.token_bytes(16)
            data = blank_file(file_id, args.slots)
            args.out.write_bytes(data)
            print('%s: %d slots, %d bytes, ID %s' % (args.out, args.slots, len(data), file_id.hex().upper()))
            return 0
        result = read_file(args.file.read_bytes())
    except (ValueError, OSError) as error:
        print('toy_survey.py: %s' % error, file=sys.stderr)
        return 1
    if args.json:
        args.json.write_text(json.dumps(result, indent=1) + '\n')
    print('\n'.join(listing(args.file, result)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
