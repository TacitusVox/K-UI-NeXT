#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Whole-folder startup bundling (v2) and the sweep's part reader, on original
synthetic images: native raw GDI, duplicate, Windows CE, CD audio, cooked
2048-byte GDI, Redump-style CUE, already-sent, broken, CDI and a loose ISO."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

from games_fixture import DATA, RAW, SESSION, make_fixture, raw_sector, record

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('library_startup_bundle',
                                              ROOT / 'tools/kui-library-startup-bundle.py')
library = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(library)
sys.path[:0] = [str(ROOT / 'tools')]
import native_library_sweep as sweep  # noqa: E402


def patch(folder, sector, offset, content):
    with (folder / 'track03.bin').open('r+b') as track:
        track.seek(sector * RAW + 16 + offset)
        track.write(content)


def add_root_records(folder, *records):
    """Append directory records after the fixture's three root entries."""
    used = sum(len(r) for r in (record(b"\0", SESSION + 20, DATA, True), record(b"\1", SESSION + 20, DATA, True),
                                record(b"1ST_READ.BIN;1", SESSION + 21, DATA * 2)))
    patch(folder, 20, used, b''.join(records))


def cook(folder):
    """The 2048-byte ISO-track copy of a fixture, as common converters write it."""
    for raw_name, iso_name in (('track01.bin', 'track01.iso'), ('track03.bin', 'track03.iso')):
        raw = (folder / raw_name).read_bytes()
        (folder / iso_name).write_bytes(b''.join(raw[i + 16:i + 16 + DATA] for i in range(0, len(raw), RAW)))
        (folder / raw_name).unlink()
    (folder / 'disc.gdi').write_text(
        '3\n1 0 4 2048 track01.iso 0\n2 4 0 2352 track02.raw 0\n3 45000 4 2048 track03.iso 0\n',
        encoding='ascii')


def as_cue(folder):
    """A per-track BIN/CUE descriptor for the same fixture, without the GDI."""
    (folder / 'disc.gdi').unlink()
    (folder / 'game.cue').write_text('\n'.join([
        'REM SINGLE-DENSITY AREA', 'FILE "track01.bin" BINARY', '  TRACK 01 MODE1/2352',
        '    INDEX 01 00:00:00', 'FILE "track02.raw" BINARY', '  TRACK 02 AUDIO',
        '    INDEX 01 00:00:00', 'REM HIGH-DENSITY AREA', 'FILE "track03.bin" BINARY',
        '  TRACK 03 MODE1/2352', '    INDEX 01 00:00:00']) + '\n', encoding='ascii')


class LibraryStartupBundleTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)
        self.games = self.folder / 'Games'
        make_fixture(self.games / 'Alpha', title=b'Alpha Fixture')
        make_fixture(self.games / 'Alpha copy', title=b'Alpha Fixture')
        patch(self.games / 'Alpha copy', 0, 32, b'0000 GD-ROM2/2  ')
        make_fixture(self.games / 'Beta', title=b'Beta Fixture')
        patch(self.games / 'Beta', 0, 64, b'BETA-0001 ')
        patch(self.games / 'Beta', 21, 0, b'BETA')
        make_fixture(self.games / 'Gamma CE', title=b'Gamma Windows CE Fixture')
        patch(self.games / 'Gamma CE', 0, 56, b'0000001')
        make_fixture(self.games / 'Delta CDDA', title=b'Delta Audio Fixture')
        delta = self.games / 'Delta CDDA'
        patch(delta, 21, 0, b'DELTA')
        (delta / 'track04.raw').write_bytes(bytes(4 * RAW))
        (delta / 'track05.bin').write_bytes(b''.join(raw_sector(SESSION + 80 + i, bytes(2048))
                                                     for i in range(2)))
        (delta / 'disc.gdi').write_text(
            '5\n1 0 4 2352 track01.bin 0\n2 4 0 2352 track02.raw 0\n'
            '3 45000 4 2352 track03.bin 0\n4 45070 0 2352 track04.raw 0\n'
            '5 45080 4 2352 track05.bin 0\n', encoding='ascii')
        make_fixture(self.games / 'Zeta-2048', title=b'Zeta Cooked Fixture')
        patch(self.games / 'Zeta-2048', 21, 0, b'ZETA')
        cook(self.games / 'Zeta-2048')
        make_fixture(self.games / 'Eta (USA)', title=b'Eta Cue Fixture')
        patch(self.games / 'Eta (USA)', 21, 0, b'ETA')
        as_cue(self.games / 'Eta (USA)')
        (self.games / 'Broken').mkdir()
        (self.games / 'Broken' / 'disc.gdi').write_text('not a gdi\n')
        (self.games / 'Broken' / '._disc.gdi').write_bytes(b'\0\5\26\7')
        (self.games / 'Epsilon').mkdir()
        (self.games / 'Epsilon' / 'game.cdi').write_bytes(b'')
        (self.games / 'Loose').mkdir()
        (self.games / 'Loose' / 'game.iso').write_bytes(b'')
        (self.games / 'Loose' / 'stray.bin').write_bytes(b'')
        self.output = self.folder / 'out'
        self.output.mkdir()

    def hashes(self):
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in self.games.rglob('*') if path.is_file()}

    def build(self, **options):
        settings = dict(name='Library-startup', part_mb=0)
        settings.update(options)
        return library.build(self.games, self.output, log=lambda line: None, **settings)

    def test_rows_and_single_part(self):
        before = self.hashes()
        rows, created = self.build()
        self.assertEqual(self.hashes(), before, 'source tracks are read only')
        status = {row['source']: row['status'] for row in rows}
        self.assertEqual(status, {
            'Alpha/disc.gdi': 'bundled', 'Alpha copy/disc.gdi': 'duplicate',
            'Beta/disc.gdi': 'bundled', 'Broken/disc.gdi': 'error',
            'Delta CDDA/disc.gdi': 'bundled', 'Eta (USA)/game.cue': 'bundled',
            'Gamma CE/disc.gdi': 'bundled', 'Zeta-2048/disc.gdi': 'bundled',
            'Epsilon/game.cdi': 'other_format', 'Loose/game.iso': 'other_format'})
        by = {row['source']: row for row in rows}
        self.assertEqual(by['Alpha copy/disc.gdi']['duplicate_of'], by['Alpha/disc.gdi']['slug'])
        self.assertEqual(by['Alpha copy/disc.gdi']['disc'], '2/2')
        self.assertEqual(by['Alpha/disc.gdi']['disc'], '1/1')
        self.assertIs(by['Gamma CE/disc.gdi']['windows_ce'], True)
        self.assertIs(by['Alpha/disc.gdi']['windows_ce'], False)
        self.assertEqual(by['Delta CDDA/disc.gdi']['cdda_tracks'], 1)
        self.assertEqual(by['Zeta-2048/disc.gdi']['data_encoding'], 'cooked2048')
        self.assertEqual(by['Eta (USA)/game.cue']['descriptor_format'], 'cue')
        self.assertEqual(by['Beta/disc.gdi']['slug'], '003-BETA-0001-Beta-Fixture')
        self.assertEqual([path.name for path in created], ['Library-startup-01.zip'])
        self.assertEqual(list(self.output.iterdir()), created, 'no staging files remain')
        with zipfile.ZipFile(created[0]) as archive:
            names = archive.namelist()
            info = json.loads(archive.read('library.json'))
            inner = archive.read(by['Alpha/disc.gdi']['archive'])
            cue_inner = archive.read(by['Eta (USA)/game.cue']['archive'])
        self.assertEqual(names[0], 'library.json')
        self.assertEqual(len(names), 1 + sum(row['status'] == 'bundled' for row in rows))
        self.assertEqual((info['schema'], info['part'], info['parts']), (library.SCHEMA, 1, 1))
        self.assertEqual(info['games'], rows)
        with zipfile.ZipFile(io.BytesIO(inner)) as game:
            self.assertEqual(game.namelist(), ['1ST_READ.BIN', 'IP.BIN', 'disc.gdi', 'startup-info.json'])
            boot = game.read('1ST_READ.BIN')
            report = json.loads(game.read('startup-info.json'))
        self.assertEqual(boot, bytes((i * 17 + 3) & 255 for i in range(2048)) +
                         bytes((i * 29 + 7) & 255 for i in range(2048)))
        self.assertEqual(hashlib.sha256(boot).hexdigest(), by['Alpha/disc.gdi']['boot_sha256'])
        self.assertEqual([item['path'] for item in report['inventory']], ['1ST_READ.BIN'])
        self.assertIs(report['audio_payload_copied'], False)
        with zipfile.ZipFile(io.BytesIO(cue_inner)) as game:
            self.assertIn('disc.cue', game.namelist())
        with self.assertRaisesRegex(ValueError, 'already holds'):
            self.build()

    def test_unusual_names_elsewhere_on_the_disc_do_not_stop_extraction(self):
        theta = self.games / 'Theta'
        make_fixture(theta, title=b'Theta Names Fixture')
        patch(theta, 21, 0, b'THETA')
        add_root_records(theta, record(b'README.;1', SESSION + 22, 16), record(b'AUX.DAT;1', SESSION + 22, 16),
                         record(b'CON.DRV;1', SESSION + 22, 16))
        rows, created = self.build()
        row = next(row for row in rows if row['source'] == 'Theta/disc.gdi')
        self.assertEqual(row['status'], 'bundled', row.get('error'))
        with zipfile.ZipFile(created[0]) as archive:
            with zipfile.ZipFile(io.BytesIO(archive.read(row['archive']))) as game:
                self.assertEqual(game.namelist(), ['1ST_READ.BIN', 'IP.BIN', 'disc.gdi', 'startup-info.json'])
                report = json.loads(game.read('startup-info.json'))
        listed = {item['path']: item for item in report['inventory']}
        self.assertIn('README', listed, 'the separator dot of an extensionless name is dropped')
        self.assertTrue(listed['AUX.DAT'].get('unsafe_name'))
        self.assertEqual([(item['path'], item['reason']) for item in report['omitted_files']],
                         [('CON.DRV', 'unsafe_name')])

    def test_requested_program_files_are_copied_even_for_a_sent_boot(self):
        iota = self.games / 'Iota'
        make_fixture(iota, title=b'Iota Loader Fixture')
        patch(iota, 0, 64, b'IOTA-0001 ')
        patch(iota, 21, 0, b'IOTA')
        add_root_records(iota, record(b'GAME.BIN;1', SESSION + 22, 64))
        first, _ = self.build()
        boot = next(row for row in first if row['source'] == 'Iota/disc.gdi')['boot_sha256']
        known = self.folder / 'already-sent.json'
        known.write_text(json.dumps({'startups': [boot]}))
        requests = self.folder / 'requests.json'
        requests.write_text(json.dumps({'requests': [
            {'product': 'IOTA-0001', 'paths': ['GAME.BIN', 'MISSING.BIN']}]}))
        self.output = self.folder / 'out2'
        self.output.mkdir()
        rows, created = self.build(known_path=known, requests_path=requests)
        row = next(row for row in rows if row['source'] == 'Iota/disc.gdi')
        self.assertEqual((row['status'], row['requested_files']), ('bundled', ['GAME.BIN']),
                         'a requested program is new even when the boot file was sent')
        with zipfile.ZipFile(created[0]) as archive:
            with zipfile.ZipFile(io.BytesIO(archive.read(row['archive']))) as game:
                self.assertIn('GAME.BIN', game.namelist())
        known.write_text(json.dumps({'startups': [{'startup_key': row['startup_key']}]}))
        self.output = self.folder / 'out3'
        self.output.mkdir()
        rows, _ = self.build(known_path=known, requests_path=requests)
        self.assertEqual(next(r for r in rows if r['source'] == 'Iota/disc.gdi')['status'], 'already_sent')

    def test_already_sent_startups_are_listed_not_copied(self):
        first, _ = self.build()
        beta = next(row for row in first if row['source'] == 'Beta/disc.gdi')
        known = self.folder / 'already-sent.json'
        gamma = next(row for row in first if row['source'] == 'Gamma CE/disc.gdi')
        known.write_text(json.dumps({'startups': [{'title': 'Beta', 'boot_sha256': beta['boot_sha256']},
                                                  {'title': 'Gamma', 'startup_key': gamma['startup_key']},
                                                  {'title': 'not CE', 'boot_sha256': gamma['boot_sha256']}]}))
        self.output = self.folder / 'out2'
        self.output.mkdir()
        rows, created = self.build(known_path=known)
        row = next(row for row in rows if row['source'] == 'Beta/disc.gdi')
        self.assertEqual(row['status'], 'already_sent')
        self.assertNotIn('archive', row)
        self.assertEqual(row['boot_sha256'], beta['boot_sha256'])
        self.assertEqual(next(row for row in rows if row['source'] == 'Gamma CE/disc.gdi')['status'],
                         'already_sent', 'a CE startup matches by its startup key')
        with zipfile.ZipFile(created[0]) as archive:
            self.assertEqual(json.loads(archive.read('library.json'))['already_sent'], 3)
            self.assertNotIn(beta['archive'], archive.namelist())
        known.write_text(json.dumps({'startups': ['not-a-hash']}))
        self.output = self.folder / 'out3'
        self.output.mkdir()
        with self.assertRaisesRegex(ValueError, 'malformed SHA256'):
            self.build(known_path=known)

    def test_windows_ce_only_recopies_ce_startups_and_lists_native_games(self):
        first, _ = self.build()
        gamma = next(row for row in first if row['source'] == 'Gamma CE/disc.gdi')
        known = self.folder / 'already-sent.json'
        known.write_text(json.dumps({'startups': [{'title': 'Gamma', 'startup_key': gamma['startup_key']}]}))
        self.output = self.folder / 'out-ce'
        self.output.mkdir()
        before = self.hashes()
        rows, created = self.build(known_path=known, windows_ce_only=True)
        self.assertEqual(self.hashes(), before, 'source tracks are read only')
        status = {row['source']: row['status'] for row in rows}
        self.assertEqual(status, {
            'Alpha/disc.gdi': 'not_windows_ce', 'Alpha copy/disc.gdi': 'not_windows_ce',
            'Beta/disc.gdi': 'not_windows_ce', 'Broken/disc.gdi': 'error',
            'Delta CDDA/disc.gdi': 'not_windows_ce', 'Eta (USA)/game.cue': 'not_windows_ce',
            'Gamma CE/disc.gdi': 'bundled', 'Zeta-2048/disc.gdi': 'not_windows_ce',
            'Epsilon/game.cdi': 'other_format', 'Loose/game.iso': 'other_format'})
        native = next(row for row in rows if row['source'] == 'Alpha/disc.gdi')
        self.assertEqual((native['title'], native['windows_ce']), ('Alpha Fixture', False))
        self.assertNotIn('archive', native)
        ce = next(row for row in rows if row['source'] == 'Gamma CE/disc.gdi')
        self.assertEqual(ce['startup_key'], gamma['startup_key'], 'sent before, copied again')
        with zipfile.ZipFile(created[0]) as archive:
            self.assertIs(json.loads(archive.read('library.json'))['windows_ce_only'], True)
            self.assertEqual([name for name in archive.namelist() if name.startswith('games/')],
                             [ce['archive']])
        self.assertEqual(sweep.verdict(native)[1], 'not_native')

    def test_cd_audio_only_copies_regular_cd_audio_games_and_their_sound_drivers(self):
        delta = self.games / 'Delta CDDA'
        manatee = b'MANATEE driver bytes ' * 12
        audio64 = b'AUDIO64 driver bytes ' * 9
        add_root_records(delta, record(b'MANATEE.DRV;1', SESSION + 22, len(manatee)),
                         record(b'AUDIO64.DRV;1', SESSION + 23, len(audio64)))
        patch(delta, 22, 0, manatee)
        patch(delta, 23, 0, audio64)
        # Another regular game with CD audio and the same MANATEE.DRV, and a Windows CE game with CD audio.
        for name, title, mark in (('Kappa CDDA', b'Kappa Audio Fixture', b'KAPPA'), ('Lambda CE CDDA', b'Lambda CE Audio', b'LAMBD')):
            folder = self.games / name
            make_fixture(folder, title=title)
            patch(folder, 21, 0, mark)
            (folder / 'track04.raw').write_bytes(bytes(4 * RAW))
            (folder / 'disc.gdi').write_text(
                '4\n1 0 4 2352 track01.bin 0\n2 4 0 2352 track02.raw 0\n'
                '3 45000 4 2352 track03.bin 0\n4 45070 0 2352 track04.raw 0\n', encoding='ascii')
        add_root_records(self.games / 'Kappa CDDA', record(b'MANATEE.DRV;1', SESSION + 22, len(manatee)))
        patch(self.games / 'Kappa CDDA', 22, 0, manatee)
        patch(self.games / 'Lambda CE CDDA', 0, 56, b'0000001')
        first, _ = self.build()
        sent = next(row for row in first if row['source'] == 'Delta CDDA/disc.gdi')
        known = self.folder / 'already-sent.json'
        known.write_text(json.dumps({'startups': [{'title': 'Delta', 'startup_key': sent['startup_key']}]}))
        self.output = self.folder / 'out-audio'
        self.output.mkdir()
        before = self.hashes()
        extracted = []
        extract = library.disc.extract
        library.disc.extract = lambda descriptor, *more: extracted.append(descriptor.parent.name) or extract(descriptor, *more)
        try:
            rows, created = self.build(known_path=known, cd_audio_only=True, name='CD-audio')
        finally:
            library.disc.extract = extract
        self.assertEqual(self.hashes(), before, 'source tracks are read only')
        status = {row['source']: row['status'] for row in rows}
        self.assertEqual(status, {
            'Alpha/disc.gdi': 'not_cd_audio', 'Alpha copy/disc.gdi': 'not_cd_audio',
            'Beta/disc.gdi': 'not_cd_audio', 'Broken/disc.gdi': 'error',
            'Delta CDDA/disc.gdi': 'bundled', 'Eta (USA)/game.cue': 'not_cd_audio',
            'Gamma CE/disc.gdi': 'not_cd_audio', 'Kappa CDDA/disc.gdi': 'bundled',
            'Lambda CE CDDA/disc.gdi': 'not_cd_audio', 'Zeta-2048/disc.gdi': 'not_cd_audio',
            'Epsilon/game.cdi': 'other_format', 'Loose/game.iso': 'other_format'})
        # Only images whose descriptor lists a high-density audio track are read at all
        # (and the one whose descriptor cannot be read, which then fails).
        self.assertEqual(sorted(extracted), ['Broken', 'Delta CDDA', 'Kappa CDDA', 'Lambda CE CDDA'])
        by = {row['source']: row for row in rows}
        self.assertEqual(by['Alpha/disc.gdi']['title'], 'Alpha Fixture', 'a game that is not copied is still named')
        self.assertEqual(by['Delta CDDA/disc.gdi']['startup_key'], sent['startup_key'], 'sent before, copied again')
        self.assertIs(by['Lambda CE CDDA/disc.gdi']['windows_ce'], True)
        self.assertEqual([path.name for path in created], ['CD-audio-sound-drivers.zip', 'CD-audio-01.zip'])
        self.assertEqual(sorted(self.output.iterdir()), sorted(created), 'no staging files remain')
        with zipfile.ZipFile(created[0]) as drivers:
            info = json.loads(drivers.read('library.json'))
            self.assertEqual((info['cd_audio_only'], info['drivers_only'], info['files_left_out']), (True, True, 0))
            self.assertEqual(info['games'], rows)
            names = drivers.namelist()
            digests = {hashlib.sha256(data).hexdigest(): data for data in (audio64, manatee)}
            self.assertEqual(names, ['library.json', f"games/{by['Delta CDDA/disc.gdi']['slug']}.json",
                                     f"games/{by['Kappa CDDA/disc.gdi']['slug']}.json"] +
                             [f'files/{digest}' for digest in digests], 'each distinct driver file once')
            for digest, data in digests.items():
                self.assertEqual(drivers.read(f'files/{digest}'), data)
            delta_game = json.loads(drivers.read(f"games/{by['Delta CDDA/disc.gdi']['slug']}.json"))
            self.assertEqual([(item['path'], item['copied']) for item in delta_game['sound_files']],
                             [('AUDIO64.DRV', True), ('MANATEE.DRV', True)])
            self.assertEqual(delta_game['report']['boot_path'], '1ST_READ.BIN')
            kappa_game = json.loads(drivers.read(f"games/{by['Kappa CDDA/disc.gdi']['slug']}.json"))
            self.assertEqual([item['sha256'] for item in kappa_game['sound_files']],
                             [hashlib.sha256(manatee).hexdigest()])
            boot = bytes((i * 17 + 3) & 255 for i in range(2048))
            self.assertFalse(any(boot in drivers.read(name) for name in names), 'no program in the small ZIP')
        with zipfile.ZipFile(created[1]) as archive:
            self.assertIs(json.loads(archive.read('library.json'))['cd_audio_only'], True)
            self.assertEqual([name for name in archive.namelist() if name.startswith('games/')],
                             [by['Delta CDDA/disc.gdi']['archive'], by['Kappa CDDA/disc.gdi']['archive']])
            with zipfile.ZipFile(io.BytesIO(archive.read(by['Delta CDDA/disc.gdi']['archive']))) as game:
                self.assertEqual(game.namelist(), ['1ST_READ.BIN', 'AUDIO64.DRV', 'IP.BIN', 'MANATEE.DRV', 'disc.gdi',
                                                   'startup-info.json'])
        # A file over the small ZIP's bound is listed, not copied.
        self.output = self.folder / 'out-audio-bound'
        self.output.mkdir()
        limit, library.DRIVER_FILE_LIMIT = library.DRIVER_FILE_LIMIT, len(audio64)
        try:
            _, created = self.build(cd_audio_only=True, name='CD-audio')
        finally:
            library.DRIVER_FILE_LIMIT = limit
        with zipfile.ZipFile(created[0]) as drivers:
            self.assertEqual(json.loads(drivers.read('library.json'))['files_left_out'], 2)
            self.assertEqual([name for name in drivers.namelist() if name.startswith('files/')],
                             [f'files/{hashlib.sha256(audio64).hexdigest()}'])

    def test_cd_audio_only_passes_over_a_chd_without_audio_tracks_unextracted(self):
        fake = self.folder / 'chdman'
        fake.write_text('#!/bin/sh\n'
                        'case "$1 $3" in\n'
                        '  info*music*) printf "Metadata: Tag=CHGD\\n TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE\\n'
                        ' TRACK:2 TYPE:AUDIO SUBTYPE:NONE\\n TRACK:3 TYPE:MODE1_RAW\\n TRACK:4 TYPE:AUDIO\\n";;\n'
                        '  info*plain*) printf " TRACK:1 TYPE:MODE1_RAW\\n TRACK:2 TYPE:AUDIO\\n TRACK:3 TYPE:MODE1_RAW\\n";;\n'
                        '  info*silent*) printf " TRACK:1 TYPE:MODE1_RAW\\n";;\n'
                        '  info*) echo "chdman cannot say";;\n'
                        '  *) echo "extract refused" >&2; exit 1;;\n'
                        'esac\n')
        fake.chmod(0o755)
        chd_root = self.folder / 'CHD'
        chd_root.mkdir()
        for name in ('music.chd', 'plain.chd', 'silent.chd', 'odd.chd'):
            (chd_root / name).write_bytes(b'chd')
        self.assertEqual([library.chd_audio_tracks(str(fake), chd_root / name)
                          for name in ('music.chd', 'plain.chd', 'silent.chd', 'odd.chd')], [2, 1, 0, None])
        self.assertIsNone(library.chd_audio_tracks(str(self.folder / 'missing-chdman'), chd_root / 'music.chd'))
        rows, _ = library.build(chd_root, self.output, 'CD-audio', 0, log=lambda line: None,
                                chdman=str(fake), cd_audio_only=True)
        by = {row['source']: row for row in rows}
        # Written out only when the track list has CD music or does not say; this chdman then refuses.
        self.assertEqual({source: row['status'] for source, row in by.items()},
                         {'music.chd': 'error', 'odd.chd': 'error', 'plain.chd': 'not_cd_audio',
                          'silent.chd': 'not_cd_audio'})
        self.assertEqual(by['plain.chd']['converted_from'], 'chd')
        self.assertIn('extract refused', by['music.chd']['error'])
        # Without the CD audio run every CHD is written out, as before.
        self.output = self.folder / 'out-all'
        self.output.mkdir()
        rows, _ = library.build(chd_root, self.output, 'Library-startup', 0, log=lambda line: None, chdman=str(fake))
        self.assertEqual({row['status'] for row in rows}, {'error'})

    def test_chd_images_read_through_chdman_match_their_gdi(self):
        chdman = shutil.which('chdman')
        if not chdman:
            self.skipTest('needs MAME chdman')
        gdi_rows, _ = self.build()
        alpha = next(row for row in gdi_rows if row['source'] == 'Alpha/disc.gdi')
        chd_root = self.folder / 'CHD'
        (chd_root / 'Alpha').mkdir(parents=True)
        subprocess.run([chdman, 'createcd', '-i', str(self.games / 'Alpha' / 'disc.gdi'),
                        '-o', str(chd_root / 'Alpha' / 'game.chd')], check=True, capture_output=True)
        (chd_root / 'Broken').mkdir()
        (chd_root / 'Broken' / 'bad.chd').write_bytes(b'not a chd')
        self.output = self.folder / 'out-chd'
        self.output.mkdir()
        rows, _ = library.build(chd_root, self.output, 'Library-startup', 0, log=lambda line: None,
                                chdman=chdman)
        by = {row['source']: row for row in rows}
        self.assertEqual(by['Alpha/game.chd']['status'], 'bundled')
        self.assertEqual(by['Alpha/game.chd']['converted_from'], 'chd')
        self.assertEqual(by['Alpha/game.chd']['startup_key'], alpha['startup_key'])
        self.assertEqual(by['Broken/bad.chd']['status'], 'error')
        self.assertEqual(list(self.output.glob('.kui-chd-*')), [], 'temporary GDIs are removed')
        self.output = self.folder / 'out-plain'
        self.output.mkdir()
        rows, _ = library.build(chd_root, self.output, 'Library-startup', 0, log=lambda line: None)
        self.assertEqual({row['status'] for row in rows}, {'other_format'}, 'CHD needs --chd')

    def test_parts_split_and_every_part_lists_every_game(self):
        rows, created = self.build(part_mb=0.001)
        bundled = [row for row in rows if row['status'] == 'bundled']
        self.assertEqual(len(bundled), 6)
        self.assertEqual(len(created), 6, 'each game exceeds the tiny part limit alone')
        self.assertEqual(sorted(row['part'] for row in bundled), [1, 2, 3, 4, 5, 6])
        for number, path in enumerate(created, 1):
            with zipfile.ZipFile(path) as archive:
                info = json.loads(archive.read('library.json'))
                self.assertEqual((info['part'], info['parts'], info['games']), (number, 6, rows))
                self.assertEqual(len(archive.namelist()), 2)

    def test_plan_parts_packs_in_order(self):
        rows = [dict(status='bundled', archive_bytes=size) for size in (4, 4, 3, 9, 1)]
        parts = library.plan_parts(rows + [dict(status='error')], 8)
        self.assertEqual([[row['archive_bytes'] for row in part] for part in parts], [[4, 4], [3], [9], [1]])

    def test_trim_keeps_required_and_bounds_optional_files(self):
        entries = {'IP.BIN': b'i', 'disc.gdi': b'g', '1ST_READ.BIN': b'b', 'SOUND.DRV': b'd' * 10,
                   'BIG.DLL': b'x' * (library.OPTIONAL_BUDGET + 1), 'DATA.LIB': b'l'}
        report = {'required_files': ['1ST_READ.BIN'], 'omitted_files': [],
                  'files': [{'path': name, 'lba': None} for name in entries]}
        kept, report = library.trim_optional(dict(entries), report)
        self.assertEqual(list(kept), ['IP.BIN', 'disc.gdi', '1ST_READ.BIN', 'SOUND.DRV'])
        self.assertEqual([item['path'] for item in report['omitted_files']], ['BIG.DLL', 'DATA.LIB'])
        self.assertEqual([item['path'] for item in report['files']], list(kept))
        self.assertEqual((report['selected_file_count'], report['archive_file_count']), (2, 5))

    def test_sweep_reads_parts_and_refuses_tampering(self):
        rows, created = self.build(part_mb=0.001)
        info, merged, blobs, missing = sweep.read_parts(created)
        self.assertEqual((merged, missing), (rows, []))
        native = [row for row in merged if row['status'] == 'bundled' and not row['windows_ce']]
        boots = {row['slug']: sweep.open_game(blobs[row['archive']], row) for row in native}
        self.assertEqual({slug: hashlib.sha256(data).hexdigest() for slug, data in boots.items()},
                         {row['slug']: row['boot_sha256'] for row in native})
        info, merged, blobs, missing = sweep.read_parts(created[:2])
        self.assertEqual(missing, [3, 4, 5, 6])
        with self.assertRaisesRegex(sweep.SweepError, 'given twice'):
            sweep.read_parts([created[0], created[0]])
        evil = self.folder / 'evil.zip'
        with zipfile.ZipFile(created[0]) as source, zipfile.ZipFile(evil, 'w') as target:
            for name in source.namelist():
                target.writestr(name, source.read(name))
            target.writestr('../escape.zip', b'')
        with self.assertRaisesRegex(sweep.SweepError, 'unexpected entry'):
            sweep.read_parts([evil])
        row = next(row for row in merged if row['status'] == 'bundled' and row['part'] in (1, 2)
                   and not row['windows_ce'])
        changed = io.BytesIO()
        with zipfile.ZipFile(io.BytesIO(blobs[row['archive']])) as source, \
                zipfile.ZipFile(changed, 'w') as target:
            for name in source.namelist():
                data = source.read(name)
                target.writestr(name, data[:-1] + bytes([data[-1] ^ 1]) if name == row['boot_path'] else data)
        with self.assertRaisesRegex(sweep.SweepError, 'differ from its hashes'):
            sweep.open_game(changed.getvalue(), row)

    def test_sweep_classifies_every_row_of_a_mixed_library(self):
        rows, created = self.build()
        known = self.folder / 'known.json'
        known.write_text(json.dumps({'games': []}))
        output, markdown = self.folder / 'sweep.json', self.folder / 'sweep.md'
        report = sweep.sweep(created, self.folder / 'work', 0x8ce8d288, output, markdown, known=known)
        self.assertEqual(json.loads(output.read_text()), report)
        self.assertEqual([row['source'] for row in report['results']], [row['source'] for row in rows])
        for row in report['results']:
            if row['status'] == 'bundled' and not row['windows_ce']:
                self.assertEqual(row['class'], 'blocked', 'synthetic boot files resolve no launch record')
                self.assertIn(row['first_refusal'], sweep.GATE_NOTES)
            else:
                self.assertEqual(row['class'], 'not_native', row['source'])
        ce = next(row for row in report['results'] if row['source'] == 'Gamma CE/disc.gdi')
        self.assertEqual(ce['now'], 'Windows CE (separate reader)')
        self.assertEqual(report['counts'], {'blocked': 5, 'not_native': 5})
        self.assertEqual(len(markdown.read_text().splitlines()), len(rows) + 2)

    def test_verdicts_follow_launcher_order(self):
        base = dict(status='bundled')
        cases = [
            (dict(structural_384k_models_passed=True, combined_resolution=True, sdk_sci_family=1), 'vmu_and_new'),
            (dict(combined_resolution=False, first_refusal='heap', sdk_sci_family=1), 'new_only'),
            (dict(combined_resolution=False, first_refusal='heap', sdk_sci_family=0), 'blocked'),
            (dict(combined_resolution=True, first_refusal='placement_384k', sdk_sci_family=1), 'blocked'),
            (dict(structural_384k_models_passed=True, combined_resolution=True, sdk_sci_family=0), 'blocked'),
        ]
        for fields, expected in cases:
            self.assertEqual(sweep.verdict({**base, **fields})[1], expected, fields)
        self.assertEqual(sweep.verdict(dict(status='windows_ce'))[1], 'not_native')
        self.assertEqual(sweep.verdict(dict(status='bundled', windows_ce=True))[1], 'not_native')
        self.assertEqual(sweep.verdict(dict(status='already_sent'))[1], 'not_native')


if __name__ == '__main__':
    unittest.main()
