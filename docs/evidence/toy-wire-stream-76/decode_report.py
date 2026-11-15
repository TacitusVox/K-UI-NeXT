#!/usr/bin/env python3
"""Decode a Test 75/76 console report into the figures the documents quote."""
import sys
HZ = 781250.0
def rows(path):
    lines = [l.rstrip("\n") for l in open(path, encoding="latin-1")]
    out = {}
    i = 0
    while i < len(lines) - 1:
        head = lines[i].split()
        vals = lines[i + 1].split()
        if head and vals and all(c in "0123456789ABCDEF" for v in vals for c in v) and any(c not in "0123456789ABCDEF" for h in head for c in h):
            out[" ".join(head)] = [int(v, 16) for v in vals]
            i += 2
        else:
            i += 1
    return out
def pieces(v):
    s = "".join("%08X" % x for x in v)
    return [int(s[i:i + 2], 16) for i in range(0, 24, 2)]
def show(path):
    r = rows(path)
    def g(prefix):
        for k in r:
            if k.startswith(prefix): return r[k]
        return None
    print("==", path)
    sub = g("I SUBMIT"); first = g("I FIRST"); ip = g("IP ON"); bp = g("BP ON"); b = g("B SUBMIT")
    print("I: submitted %d, on time %d, longest gap %.3f s after frame %d; first frame %.3f s, last %d, skipped %d, first skip at %d" % (
        sub[0], sub[1], sub[2] / HZ, sub[3], first[0] / HZ, first[1], first[2], first[4] if len(first) > 4 else -1))
    print("B: submitted %d, on time %d, longest gap %.1f ms after %d" % (b[0], b[1], 1000 * b[2] / HZ, b[3]))
    print("IP: frame calls %d, %.2f s, %.1f ms each; turn %.2f s; reader interrupt in span %.2f s, %.2f ms each; skip calls %d, %.3f s" % (
        ip[0], ip[1] / HZ, 1000 * ip[1] / HZ / ip[0], ip[2] / HZ, ip[3] / HZ, 1000 * ip[3] / HZ / ip[0], ip[4], ip[5] / HZ))
    print("BP: frame calls %d, %.1f ms each; skip calls %d" % (bp[0], 1000 * bp[1] / HZ / bp[0], bp[4]))
    v = g("V SCI"); blk = g("W BLK"); p = g("P RING"); d = g("D N T"); w = g("W START"); wb = g("W BYTES"); s = g("S C S"); f = g("W FILL"); k = g("K LIGHT"); a = g("AHEAD"); pipe = g("PIPE")
    total = sum(v[3:6]); n = blk[0]; us = lambda t: 1e6 * t / HZ / n
    print("V: SCI %d visits %.2f s; timer %d, %.3f s; game %d, %.2f s; all %.2f s; blocks %d; %.1f us a block" % (v[0], v[3] / HZ, v[1], v[4] / HZ, v[2], v[5] / HZ, total / HZ, n, us(total)))
    if p:
        seek = d[-2] if "CAP" in " ".join(k2 for k2 in r if k2.startswith("D N T")) else d[-1]
        print("P us/block: ring %.1f, feed less checks %.1f, checks %.1f, service %.1f, seek %.1f, rest %.1f; out %.2f s (%.1f us a sector of %d); last P value %d ticks (%.1f us for each of %d streams)" % (
            us(p[0]), us(p[1] - p[2]), us(p[2]), us(p[4]), us(seek), us(total - p[0] - p[1] - p[4] - seek), p[3] / HZ, 1e6 * p[3] / HZ / (a[0] + a[1]), a[0] + a[1], p[5], 1e6 * p[5] / HZ / w[0], w[0]))
        print("seek %.2f s, %.2f ms for each stream begun" % (seek / HZ, 1000 * seek / HZ / w[0]))
    print("D: delivery visits %d, %.2f s" % (d[0], d[1] / HZ), d)
    print("W: begun %d, full runs %d, overruns in a run %d, timer waits %d of %.2f s, stops for room %d; bytes %d; timer %d" % (w[0], w[1], w[2], w[3], wb[1] / HZ, w[4], wb[0], wb[2]))
    print("BLK: %d, rebuilt %d, unexplained %d, holed %d, crc only %d, low reader %d" % tuple(blk))
    if s: print("S: closed %d, sector %d, could not follow %d: lost frame %d, ambiguous %d, bad crc %d; lost frame per overrun %.2f%%" % (s[0], s[1], s[2], s[3], s[4], s[5], 100.0 * s[3] / w[2]))
    print("FILL: %d, refused %d, bad %d;  K: light %d, edc %d, mend %d, MB %d, LB %d, ORD %d" % (f[0], f[1], f[2], k[0], k[1], k[2], k[3], k[4], k[5]))
    print("AHEAD: served %d / %d, requests %d (%.1f sectors each), into window %d, stop %d; PIPE windows %d / %d, calls %d, irq %d, stay %d, PT %X" % (a[0], a[1], a[2], (a[0] + a[1]) / a[2], a[3], a[4], pipe[0], pipe[1], pipe[2], pipe[3], pipe[4], pipe[5]))
    for name, scale, fmt in (("TF", 1, "%d"), ("TD", 16, "%d"), ("TW", 16384 / HZ, "%.2f"), ("TB", 4, "%d"), ("TP", 16384 / HZ, "%.2f"), ("TR", 16384 / HZ, "%.2f"), ("TI", 16384 / HZ, "%.2f"), ("TG", 16384 / HZ, "%.2f")):
        x = g(name)
        if x and len(x) == 3: print(name, " | ".join(fmt % (q * scale) for q in pieces(x)))
    if g("D N T") and "CAP" in " ".join(r): print("CAP", g("D N T")[-1])
for p in sys.argv[1:]: show(p)
