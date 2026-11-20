#!/usr/bin/env python3
"""Decode a Test 78 to 81 report into the figures the documents quote.

A report of another title than Toy Commander has no movie: its movie rows
are zero and are passed over. python3 decode_report.py <report>...

The rows are Test 77's (docs/evidence/toy-wire-stream-77/decode_report.py)
without the rows by piece of the second movie's course, and with:
  BIG N S BACK [TOY]   requests of more than sixty sectors a window served
                       to their end, their sectors, those that went back
                       to the low reader; Test 79: 1 the verified Toy
                       Commander, 0 any other title
  SD BLK PIO OVR CRC DECL   Test 79: the low reader's blocks by DMA, its
                       blocks by programmed transfers, its receive
                       overruns (each a block read again), its CRC16
                       failures; the stream's refused take-overs
  XC KEPT LO HI        Test 80: commands that are no reads a window stood
                       through, and every such command a window met, a
                       bit to a command number
  VMAX SCI TMR GAME D  Test 80: the longest single pump of the reader from
                       the SCI's interrupt, from the stream's timer and
                       from one of the game's calls, and the longest whole
                       delivery visit from one of the game's disc calls
  W ... TMR            Test 81: the timer the last stream took at its start
                       (2 TMU2, 1 TMU1, 0 none); V's second count and time
                       are the timer's visits
The row V has three counts and then three times: its label's last two
words stand for the times of the SCI's visits, the timer's and the game's.
"""
import sys

HZ = 781250.0
HEX = "0123456789ABCDEF"
COMMANDS = {18: "GETTOC", 19: "GETTOC2", 20: "PLAY", 21: "PLAY2", 22: "PAUSE", 23: "RELEASE", 24: "INIT",
            27: "SEEK", 29: "NOP", 30: "REQ_MODE", 31: "SET_MODE", 33: "STOP", 34: "GETSCD", 36: "REQ_STAT",
            40: "GET_VERS"}


def rows(path):
    lines = [l.rstrip("\n") for l in open(path, encoding="latin-1")]
    out = {}
    i = 0
    while i < len(lines) - 1:
        head = lines[i].split()
        vals = lines[i + 1].split()
        if (head and vals and all(c in HEX for v in vals for c in v)
                and any(c not in HEX for h in head for c in h)):
            out[" ".join(head)] = [int(v, 16) for v in vals]
            i += 2
        else:
            i += 1
    out["_test"] = next((l.split()[1] for l in lines if l.startswith("TEST ")), "?")
    out["_build"] = next((l.split()[1] for l in lines if l.startswith("BUILD ")), "?")
    return out


def show(path):
    r = rows(path)

    def g(prefix):
        for k in r:
            if k.startswith(prefix):
                return r[k]
        return None

    def ms(ticks, n):
        return 1000.0 * ticks / HZ / n if n else 0.0

    print("==", path)
    print("test %s, build %s" % (r["_test"], r["_build"]))
    big = g("BIG N S"); sd = g("SD BLK"); xc = g("XC KEPT")
    toy = big[3] if big and len(big) > 3 else 1
    print("title: %s" % ("the verified Toy Commander" if toy else "another title (no movie rows)"))
    sub = g("I SUBMIT"); first = g("I FIRST"); ip = g("IP ON"); bp = g("BP ON"); b = g("B SUBMIT")
    if sub and sub[0]:
        print("I: submitted %d, on time %d, longest gap %.3f s after frame %d; first frame %.3f s, "
              "last %d, skipped %d, exit %d, first skip at %d" % (
                  sub[0], sub[1], sub[2] / HZ, sub[3], first[0] / HZ, first[1], first[2], first[3], first[4]))
        print("B: submitted %d, on time %d, longest gap %.1f ms after %d" % (b[0], b[1], 1000 * b[2] / HZ, b[3]))
        print("IP: frame calls %d, %.2f s, %.1f ms each; reader interrupt in span %.2f s, %.2f ms each; "
              "skip calls %d" % (ip[0], ip[1] / HZ, ms(ip[1], ip[0]), ip[3] / HZ, ms(ip[3], ip[0]), ip[4]))
        if bp and bp[0]:
            print("BP: frame calls %d, %.1f ms each; skip calls %d" % (bp[0], ms(bp[1], bp[0]), bp[4]))
    v = g("V SCI"); blk = g("W BLK"); p = g("P RING"); d = g("D N T"); w = g("W START")
    wb = g("W BYTES"); s = g("S C S"); f = g("W FILL"); k = g("K LIGHT"); a = g("AHEAD")
    pipe = g("PIPE"); x = g("X OW")
    served = a[0] + a[1] + a[5]
    total = sum(v[3:6]); n = blk[0]
    us = lambda t: 1e6 * t / HZ / n if n else 0.0
    print("V: SCI %d visits %.2f s; timer %d, %.3f s; game %d, %.2f s; all %.2f s; stream blocks %d; "
          "%.1f us a block" % (v[0], v[3] / HZ, v[1], v[4] / HZ, v[2], v[5] / HZ, total / HZ, n, us(total)))
    seek = d[-1]
    print("P us/block: ring %.1f, feed less checks %.1f, checks %.1f, service %.1f, seek %.1f, rest %.1f; "
          "out %.2f s (%.1f us a sector of %d); ending streams %.3f s" % (
              us(p[0]), us(p[1] - p[2]), us(p[2]), us(p[4]), us(seek), us(total - p[0] - p[1] - p[4] - seek),
              p[3] / HZ, 1e6 * p[3] / HZ / served if served else 0.0, served, p[5] / HZ))
    print("seek %.2f s, %.2f ms for each stream begun" % (seek / HZ, ms(seek, w[0])))
    print("D: delivery visits %d, %.2f s, %.2f ms each" % (d[0], d[1] / HZ, ms(d[1], d[0])))
    print("W: begun %d, full runs %d, runs cut %d, waits %d of %.2f s (%.2f ms each), stops for room %d; "
          "bytes %d (%.1f a block); timer %d; errors %d, detail %X" % (
              w[0], w[1], w[2], w[3], wb[1] / HZ, ms(wb[1], w[3]), w[4], wb[0], wb[0] / n if n else 0.0,
              wb[2], wb[3], wb[4]))
    print("BLK: %d, rebuilt %d, unexplained %d, holed %d, crc only %d; brought by the low reader for a "
          "window %d" % tuple(blk))
    print("S: closed %d, sector %d, could not follow %d: lost frame %d, ambiguous %d, bad crc %d" % tuple(s))
    print("FILL: %d, refused %d, bad %d;  K: light %d, edc %d, mend %d, MB %d, LB %d, ORD %d" % (
        f[0], f[1], f[2], k[0], k[1], k[2], k[3], k[4], k[5]))
    print("  check: light + edc - bad = %d against sectors into the window %d (the difference is sectors "
          "refused for too many lost bytes once whole)" % (k[0] + k[1] - f[2], a[3]))
    print("AHEAD: sectors served from the movies' windows %d, from other windows %d, all %d; requests "
          "served %d (%.1f sectors each); sectors into the window %d, of which not served %d (%.1f%%); "
          "stop %d" % (a[0] + a[1], a[5], served, a[2], served / a[2] if a[2] else 0.0, a[3], a[3] - served,
                       100.0 * (a[3] - served) / a[3] if a[3] else 0.0, a[4]))
    opened = pipe[0] + pipe[1] + x[0]
    gone = sum(x[1:6])
    print("PIPE: movie windows %d; game's calls %d, interrupt visits %d, stay %d, PT %X" % (
        pipe[0] + pipe[1], pipe[2], pipe[3], pipe[4], pipe[5]))
    print("X: other windows %d; opened in all %d (%.1f sectors, %.2f requests each); given up: error %d, "
          "request state %d, command %d, waited %d, other read %d, together %d%s" % (
              x[0], opened, served / opened if opened else 0.0, a[2] / opened if opened else 0.0,
              x[1], x[2], x[3], x[4], x[5], gone,
              "" if gone == opened else " (%d not given up by the report's end)" % (opened - gone)))
    if big:
        print("BIG: requests of more than sixty sectors %d, %d sectors (%.1f each), back to the low "
              "reader %d" % (big[0], big[1], big[1] / big[0] if big[0] else 0.0, big[2]))
    if sd:
        own = sd[0] * 512.0 / 2352.0
        print("SD: low reader's blocks by DMA %d (%.0f sectors' worth), by programmed transfers %d, "
              "overruns %d (%.2f per hundred blocks), CRC16 failures %d; take-overs refused %d" % (
                  sd[0], own, sd[1], sd[2], 100.0 * sd[2] / sd[0] if sd[0] else 0.0, sd[3], sd[4]))
        print("  the windows' share of the sectors read: %.0f%% (%d of about %.0f)" % (
            100.0 * served / (served + own) if served + own else 0.0, served, served + own))
    vmax = g("VMAX SCI")
    if vmax:
        print("VMAX: longest pump from the SCI's interrupt %.2f ms, from the timer %.3f ms, from a call of "
              "the game's %.2f ms; longest delivery visit %.2f ms (a frame is %.1f ms)" % (
                  1000 * vmax[0] / HZ, 1000 * vmax[1] / HZ, 1000 * vmax[2] / HZ, 1000 * vmax[3] / HZ,
                  1000 * 0x32DD / HZ))
    if xc:
        seen = [n for n in range(64) if xc[1 + n // 32] >> (n % 32) & 1]
        print("XC: commands a window stood through %d; commands that are no reads met with a window "
              "open: %s" % (xc[0], ", ".join("%d %s" % (n, COMMANDS.get(n, "?")) for n in seen) or "none"))


for path in sys.argv[1:]:
    show(path)
