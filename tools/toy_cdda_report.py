#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The saved reports of a CD-music test's STREAM.TXT (Tests 84 to 89), each
with its CD rows spelled out.

usage: python3 -I tools/toy_cdda_report.py STREAM.TXT

The file is the survey file of tools/toy_survey.py: a header sector and
three sectors a slot (the launch record and two of report text). Every
slot that was launched is printed as it stands, and under it what the rows
CD P, CD ST, CD O and CD C say (docs/cdda-background-test-85.md to
docs/cdda-background-test-89.md, The report). Nothing is written."""
import sys

TICK = 781250.0
PHASE = {0: 'off', 1: 'priming', 2: 'playing', 3: 'silent'}
WHY = {0: 'none', 1: 'no admitted driver', 2: 'the sound CPU held', 3: 'another driver came', 4: 'records prove nothing',
       5: 'the ring', 6: 'a channel taken', 7: 'a key dropped too often', 8: 'the timer'}
RING = {1: 'no place among the table\'s pairs', 2: 'a pair came to lie in it', 3: 'a guard line overwritten'}
FAMILY = {0: 'none', 1: 'MANATEE 2', 2: 'MANATEE 1 from 1.01.37', 3: 'older MANATEE 1', 4: 'NAMIHEI', 5: 'AUDIO64'}
# Reason 4's detail (Test 88): the records are proved and leave no pair.
PROOF = {1: 'the split leaves no two stream ports'}
STATE = {0: 'idle', 1: 'playing', 2: 'paused', 3: 'ended', 4: 'clock lost'}
KIND = {0: '16-bit samples in a place of the table\'s', 1: 'ADPCM in a place of the table\'s',
        2: '16-bit samples in the driver\'s spare memory', 3: 'ADPCM in the driver\'s spare memory'}


def ms(ticks):
    return '%.2f ms' % (ticks * 1000.0 / TICK)


def read(rows, v):
    if 'CD P P2 PA RE END LAP' in v:
        p = v['CD P P2 PA RE END LAP']
        print('   the game: %d PLAY, %d PLAY2, %d PAUSE, %d RELEASE; %d plays ended, %d laps' % tuple(p))
    if 'CD ST RD KEEP REF LOST PJ' in v:
        s = v['CD ST RD KEEP REF LOST PJ']
        print('   the drive at the save: %s, first track %d, repeat byte %02X, generation byte %02X; %d data reads during a '
              'play, %d read steps kept at 3; %d plays refused, %d lost; %d positions from the play' %
              (STATE.get(s[0] & 255, '?'), s[0] >> 24, s[0] >> 16 & 255, s[0] >> 8 & 255, s[1], s[2], s[3], s[4], s[5]))
    if 'CD O ST V ID RING N' in v:
        o = v['CD O ST V ID RING N']
        why, detail = o[0] >> 4 & 15, o[0] >> 16
        print('   the output: %s; reason: %s%s; family %s; keys dropped in the play %d' %
              (PHASE.get(o[0] & 15, '?'), WHY.get(why, '?'),
               (' (%s)' % RING.get(detail, 'detail 0x%X' % detail)) if why == 5 else
               (' (%s)' % PROOF[detail]) if why == 4 and detail in PROOF else (' (detail 0x%X)' % detail if detail else ''),
               FAMILY.get(o[0] >> 8 & 15, '?'), o[0] >> 12 & 15))
        print('   the driver seen: version word %08X (%d.%02X.%02X), date hash %04X; channels %d and %d' %
              (o[1], o[1] & 255, o[1] >> 8 & 255, o[1] >> 16 & 255, o[2] & 0xffff, o[2] >> 16 & 255, o[2] >> 24))
        frames = o[4] & 0xffff
        if frames:
            print('   the ring: at 0x%06X, %d frames a plane (%.0f ms); places taken %d; %s' %
                  (o[3], frames, frames / 44.1, o[4] >> 16 & 255, KIND.get(o[4] >> 24, 'kind %d' % (o[4] >> 24))))
        else:
            print('   the ring: none placed; places taken %d' % (o[4] >> 16 & 255))
    for row, fourth in (('CD O KEY RST UND ANC DRP TK', 'for the clocks\' margin'),             # Tests 85 and 86
                        ('CD O KEY RST UND CLK DRP TK', 'for a count TMU0 did not bear out')):    # Tests 87 and 88
        if row in v:
            k = v[row]
            print('   %d key-ons; %d rings begun again (%d for a ring run dry, %d %s); %d keys dropped by the '
                  'driver, %d channels found keyed on; %d channels taken, %d registers put back' %
                  (k[0], k[1], k[2], k[3], fourth, k[4] & 0xffff, k[4] >> 16, k[5] & 0xffff, k[5] >> 16))
    if 'CD O HB LONG TICK KB SKIP LV' in v:
        h = v['CD O HB LONG TICK KB SKIP LV']
        music = h[3] * 1024 / 176400.0
        print('   %d heartbeats used the bus: %s in all, %s each on average, the longest %s; %d KiB of audio taken '
              '(%.1f s of music)' % (h[0], ms(h[2]), ms(h[2] / h[0]) if h[0] else '-', ms(h[1]), h[3], music))
        if h[0] and h[3]:
            print('   so %.0f bytes of audio a heartbeat, and %.1f heartbeats a second of music' %
                  (h[3] * 1024.0 / h[0], h[0] / music))
        print('   heartbeats not made for SR.BL %d, for a G2 transfer %d; bus waits run out %d; stores refused %d; levels '
              '%04X and %04X' % (h[4] >> 24, h[4] >> 16 & 255, h[4] >> 8 & 255, h[4] & 255, h[5] & 0xffff, h[5] >> 16))
    lost = None
    if 'CD O MX FB PRIME SIL LOST' in v:      # Test 85
        m = v['CD O MX FB PRIME SIL LOST']
        print('   the CD mixers as last read: %04X and %04X; MANATEE 2\'s level bytes %02X %02X, stood in %d times; priming '
              '%s; %d frames of silence' % (m[0] & 0xffff, m[0] >> 16, m[1] & 255, m[1] >> 8 & 255, m[1] >> 16, ms(m[2]), m[3]))
        lost = m[4]
    m = v.get('CD O MX M1 FB AT TM LOST') or v.get('CD O MX LD FB AT TM LOST') or v.get('CD O MX LD L2 AT TM LOST')
    if 'CD O MX M1 FB AT TM LOST' in v:       # Test 86
        print('   the CD mixers as last read: %04X and %04X; as first read with something in them: %04X and %04X; MANATEE '
              '2\'s level bytes %02X %02X, stood in %d times' %
              (m[0] & 0xffff, m[0] >> 16, m[1] & 0xffff, m[1] >> 16, m[2] & 255, m[2] >> 8 & 255, m[2] >> 16))
    if 'CD O MX LD FB AT TM LOST' in v:       # Tests 87 and 88
        lead, gap = m[1] & 0xffff, m[1] >> 16
        print('   the CD mixers as last read: %04X and %04X; MANATEE 2\'s level bytes %02X %02X, stood in %d times' %
              (m[0] & 0xffff, m[0] >> 16, m[2] & 255, m[2] >> 8 & 255, m[2] >> 16))
        print('   the least a look found the ring ahead of the chip: %s frames (%.0f ms; under 241 the ring has run dry); '
              'the longest wait between two looks at a running ring: %.1f ms' %
              ('65535 or more' if lead == 0xffff else str(lead), lead / 44.1, gap * 64 * 1000.0 / TICK))
    if 'CD O MX LD L2 AT TM LOST' in v:       # Test 89
        lead, gap, run = m[1] & 0xffff, m[1] >> 16, m[2] & 0xffff
        print('   the CD mixers as last read: %04X and %04X; MANATEE 2\'s level bytes stood in %d times' %
              (m[0] & 0xffff, m[0] >> 16, m[2] >> 16))
        print('   the least a look found the ring ahead of the chip: %s frames (%.0f ms; under 241 the ring has run dry); '
              'a second and more into its ring: %s; the longest wait between two looks at a running ring: %.1f ms' %
              ('65535 or more' if lead == 0xffff else str(lead), lead / 44.1,
               'no such look' if run == 0xffff else '%d frames (%.0f ms)' % (run, run / 44.1), gap * 64 * 1000.0 / TICK))
    if 'CD W BL AH CU' in v or 'CD W BL AH CU TB' in v:   # Test 89
        w = v.get('CD W BL AH CU') or v.get('CD W BL AH CU TB')
        print('   why the audio\'s stream ended at a block: a CRC16 that fails %d, a lost start token %d, two readings %d, '
              'lost bytes the CRC16 does not mend %d; overruns inside a run %d, runs not started again for want of room %d' %
              (w[0] & 0xffff, w[0] >> 16, w[1] & 0xffff, w[1] >> 16, w[2] & 0xffff, w[2] >> 16))
        if len(w) > 3:
            first, vary = (w[3] >> 24, w[3] >> 8 & 255), (w[3] >> 16 & 255, w[3] & 255)
            print('   the sound chip\'s timers B and C, read only: B first read %02X, bits seen to change %02X; C first read '
                  '%02X, bits seen to change %02X (%s)' % (first[0], vary[0], first[1], vary[1],
                  'neither read as a count' if not (vary[0] | vary[1]) else 'a count reads back'))
    if m is not None:
        if m[3]:
            # (A count is 176.4 frames; in MANATEE 1.01 before build 0x32, 176.)
            o = v.get('CD O ST V ID RING N')
            slow = o is not None and (o[1] & 0xffff) == 0x0101 and (o[1] >> 16 & 255) < 0x32
            ratio = m[4] / (m[3] * 3125.0 * (176.0 / 176.4 if slow else 1.0))
            print('   the two clocks: %d counts of the driver\'s (%.1f s) in %d of TMU0\'s ticks: TMU0 runs %+.0f parts in '
                  'a million against the sound chip\'s clock, give or take %.0f' %
                  (m[3], m[3] / 250.0, m[4], (ratio - 1.0) * 1e6, 2e6 / m[3]))
        else:
            print('   the two clocks: no count of the driver\'s seen (AT 0, TM %d)' % m[4])
        lost = m[5]
    if lost is not None:
        print('   rings lost to a pair of the table %d, to a guard %d; segments the card refused %d; heartbeats without the '
              'stack %d' % (lost & 255, lost >> 16 & 255, lost >> 8 & 255, lost >> 24))
    if 'CD C BLK RB RR PIO WAIT YLD' in v:
        c = v['CD C BLK RB RR PIO WAIT YLD']
        print('   the card: %d blocks handed on (%d rebuilt from their CRC, %.1f%%), %d read again, %d by programmed '
              'transfers, %d visits left one waiting, %d windows given up' %
              (c[0], c[1], 100.0 * c[1] / c[0] if c[0] else 0, c[2], c[3], c[4], c[5]))
    if 'CD C HD HO HX REF DCL ERR' in v:
        c = v['CD C HD HO HX REF DCL ERR']
        print('   streams ended for a read of the game\'s %d, by the audio %d, found ended %d; segments refused %d (reason '
              '%d); streams not begun %d; first error %04X detail %04X' %
              (c[0], c[1], c[2], c[3] & 0xffff, c[3] >> 16, c[4], c[5] & 0xffff, c[5] >> 16))
    if 'CD C VIS TICK HB LONG RND SEEK' in v:
        c = v['CD C VIS TICK HB LONG RND SEEK']
        print('   %d visits for the audio in %d heartbeats (%.2f each): %s in all, %s a visit with the bytes\' way into the '
              'ring, the longest heartbeat %s, the longest reading visit %s; programmed reads %s' %
              (c[0], c[2], c[0] / c[2] if c[2] else 0, ms(c[1]), ms(c[1] / c[0]) if c[0] else '-', ms(c[3]), ms(c[4]),
               ms(c[5])))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    d = open(sys.argv[1], 'rb').read()

    def sector(i):
        return d[512 * i:512 * (i + 1)].decode('ascii', 'replace')

    for slot in range((len(d) // 512 - 1) // 3):
        launch, a, b = sector(1 + 3 * slot), sector(2 + 3 * slot), sector(3 + 3 * slot)
        if 'LAUNCH' not in launch.split('\n')[0]:
            continue
        print('=' * 78)
        for line in launch.split('\n'):
            if line.strip():
                print(line.rstrip())
        if 'EMPTY' in a.split('\n')[0]:
            print('(no report saved)')
            continue
        rows = [line.strip() for line in (a + b).split('\n') if line.strip()]
        for line in rows:
            print(line)
        v = {}
        for i in range(len(rows) - 1):
            if rows[i].startswith('CD '):
                try:
                    v[rows[i]] = [int(x, 16) for x in rows[i + 1].split()]
                except ValueError:
                    pass
        print('-- read:')
        read(rows, v)


if __name__ == '__main__':
    main()
