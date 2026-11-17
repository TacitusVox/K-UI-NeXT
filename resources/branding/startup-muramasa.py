#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the K-UI V3.0 "Muramasa" startup splash (640x480, exact RGB565).

A katana driven into a neon grid in front of a blood moon: polished steel
with a hamon and wrapped tsuka, glowing fissures and a crimson aura, misty
ridges with a torii, drifting sakura, the chrome K-UI wordmark, V3.0,
"Muramasa", a 村正 seal and "by TacitusVox". Fixed seeds make it repeatable;
tools/build_splash.py pins the committed startup.png, so a rerun with other
numpy/Pillow/FreeType versions cannot silently change a build. See
startup-README.md for the fonts (pinned by SHA-256) and the exact versions.

    python3 resources/branding/startup-muramasa.py --fonts FONT_DIR startup.png
"""
import argparse
import math
from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from kui_art import (blur, chrome, fbm, finish, font, lin, over, sample, save_png,  # noqa: E402
                     smoothstep, star_flare, text_mask, to_linear, wordmark_mask)

S = 2
W, H = 640 * S, 480 * S
AA = 0.6
HORIZON = 330.0
MX, MY, MR = 212.0, 186.0, 116.0


def band(lo, hi, x):
    return smoothstep(lo - AA, lo + AA, x) * (1 - smoothstep(hi - AA, hi + AA, x))


def at(xf, yf):
    return (xf * S, yf * S)


def render(fonts):
    kanit = font(fonts, "kanit-latin-900-italic.woff")
    michroma = font(fonts, "michroma-latin-400-normal.woff")
    zen_mura = font(fonts, "zen-antique-109-400-normal.woff")
    zen_masa = font(fonts, "zen-antique-117-400-normal.woff")
    rng = np.random.default_rng(1858)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    X, Y = (xx + 0.5) / S, (yy + 0.5) / S

    # Sky with moonlit wisps and stars.
    t = np.clip(Y / HORIZON, 0, 1)[..., None]
    img = lin('#020104') * (1 - t) ** 1.5 + lin('#14030a') * (1 - (1 - t) ** 1.5)
    img += lin('#30060f') * (np.exp(-(HORIZON - Y).clip(0) / 30.0) * 0.7)[..., None]
    dm = np.hypot(X - MX, Y - MY)
    near_moon = np.exp(-np.maximum(dm - MR, 0) / 80.0)
    n1, n2 = fbm(H, W, 2.5, 5, rng), fbm(H, W, 2.5, 5, rng)
    base = fbm(H, W, 3.5, 7, rng, gain=0.55)
    cl = sample(base, xx + (n1 - 0.5) * 260 * S, yy + (n2 - 0.5) * 90 * S)
    cl = smoothstep(0.52, 0.82, cl) * smoothstep(HORIZON - 10, HORIZON - 150, Y)
    img += (lin('#3a0610') * (0.15 + 0.85 * near_moon[..., None]) +
            lin('#ff4a2a') * (near_moon ** 5 * 0.35)[..., None]) * (cl * 0.5)[..., None]
    for _ in range(170):
        sx, sy = rng.uniform(0, W), rng.uniform(0, 250 * S)
        b = rng.uniform(0.05, 0.8) ** 2 * (1 - cl[int(sy), int(sx)]) * (1 - near_moon[int(sy), int(sx)])
        if b < 0.01:
            continue
        r2 = (xx - sx) ** 2 + (yy - sy) ** 2
        img += lin('#ffe8f0') * (b * np.exp(-r2 / (2 * (0.75 * S) ** 2)))[..., None]

    # Blood moon: limb darkening, maria, a few soft craters, thin cloud strands.
    inside = 1 - smoothstep(MR - 0.6, MR + 0.6, dm)
    mu = np.sqrt(np.clip(1 - np.clip(dm / MR, 0, 1) ** 2, 0, 1))
    mf = fbm(H, W, 2.2, 6, rng)
    maria = smoothstep(0.47, 0.56, sample(mf, xx + 60 * S * (fbm(H, W, 3, 3, rng) - 0.5),
                                          yy + 40 * S * (fbm(H, W, 3, 3, rng) - 0.5)))
    maria *= smoothstep(MX + 70, MX - 40, X + (Y - MY) * 0.4) * 0.8 + 0.2
    blotch = fbm(H, W, 9, 4, rng)
    grain = fbm(H, W, 34, 3, rng)
    crater = np.zeros((H, W), np.float32)
    for _ in range(9):
        a = rng.uniform(0, 2 * math.pi)
        d = MR * math.sqrt(rng.uniform(0, 0.9))
        cx, cy = MX + d * math.cos(a), MY + d * math.sin(a)
        cr = rng.uniform(2.5, 10) if rng.random() < 0.85 else rng.uniform(12, 18)
        ddx, ddy = (X - cx) / cr, (Y - cy) / cr
        dd = np.hypot(ddx, ddy)
        crater += -0.12 * smoothstep(1.0, 0.35, dd) + \
            0.16 * np.exp(-((dd - 1.0) / 0.2) ** 2) * np.clip(-(ddx * 0.6 + ddy * 0.8), 0, 1)
    tone = (0.42 + 0.58 * mu ** 0.6) * (1 - 0.55 * maria) * (0.80 + 0.30 * blotch + 0.12 * grain) * (1 + crater)
    tone = np.clip(tone, 0, 1.3)[..., None]
    moon = lin('#1a0305') * (1 - tone) + lin('#9a2a22') * tone
    moon += lin('#ff8a5a') * (np.clip(tone - 0.85, 0, 1) * 0.9)
    img = over(img, moon, inside)
    img += lin('#ff2418') * ((1 - inside) * (np.exp(-np.maximum(dm - MR, 0) / 9.0) * 0.30 +
                                           np.exp(-np.maximum(dm - MR, 0) / 55.0) * 0.08))[..., None]
    str_n = sample(fbm(H, W, 7, 5, rng, aspect=0.10), xx + (n1 - 0.5) * 80 * S, yy)
    wob = (fbm(H, W, 3, 3, rng) - 0.5) * 22
    lanes = (np.exp(-((Y - (MY + 58) + wob) / 7.0) ** 2) + 0.8 * np.exp(-((Y - (MY + 86) - wob * 0.6) / 5.0) ** 2) +
             0.55 * np.exp(-((Y - (MY - 40) + wob * 0.4) / 3.5) ** 2))
    strand = smoothstep(0.42, 0.66, str_n) * np.clip(lanes, 0, 1) * smoothstep(MR + 80, MR - 30, dm)
    img = over(img, lin('#080106'), strand * 0.9)
    lit_edge = np.clip(strand - sample(strand, xx, yy - 2.0 * S), 0, 1)
    img += lin('#ff6a48') * (lit_edge * np.clip(inside + near_moon * 0.3, 0, 1) * 0.9)[..., None]

    # Ridges, lower behind the moon; only slopes facing it catch the light.
    cols = np.arange(W, dtype=np.float32) / S

    def ridge(seed, level, amp, cells, octaves):
        n = fbm(1, W, cells, octaves, np.random.default_rng(seed), aspect=1.0)[0]
        return level - (1 - np.abs(n * 2 - 1)) * amp

    def lit_slope(top):
        return np.clip(-np.gradient(top) * np.sign(MX - cols) * 0.9, 0, 1) * np.exp(-np.abs(cols - MX) / 420.0)

    env = 0.35 + 0.65 * (1 - np.exp(-((cols - MX) / 190.0) ** 2))
    far = HORIZON - 6 - (HORIZON - 6 - ridge(5, HORIZON - 6, 74, 2.4, 7)) * env
    mid = HORIZON - 1 - (HORIZON - 1 - ridge(17, HORIZON - 1, 34, 4.2, 7)) * (0.5 + 0.5 * env)
    haze = smoothstep(HORIZON - 90, HORIZON, Y)[..., None]
    farm = smoothstep(far[None, :] - 0.6, far[None, :] + 0.6, Y)
    fcol = lin('#14050f') * (1 - haze) + lin('#2a0812') * haze
    fcol += lin('#ff4a36') * (np.exp(-(Y - far[None, :]).clip(0) / 2.0) *
                              (0.10 + 0.9 * lit_slope(far))[None, :])[..., None]
    img = over(img, fcol, farm)
    mist = smoothstep(0.35, 0.8, sample(fbm(H, W, 5, 5, rng, aspect=0.25), xx, yy)) * \
        np.exp(-((Y - (HORIZON - 16)) / 13.0) ** 2)
    img += lin('#7a1222') * (mist * 0.26)[..., None]
    midm = smoothstep(mid[None, :] - 0.6, mid[None, :] + 0.6, Y)
    mcol = lin('#060105') + lin('#16030a') * haze * 0.8
    mcol += lin('#ff4434') * (np.exp(-(Y - mid[None, :]).clip(0) / 1.6) *
                              (0.04 + 0.7 * lit_slope(mid))[None, :])[..., None]
    img = over(img, mcol, midm)
    # A torii on the right-hand ridge.
    tg = Image.new('L', (W * 2, H * 2), 0)
    td = ImageDraw.Draw(tg)
    tx, tb, z = 588.0, float(mid[int(588 * S)]) + 2.0, S * 2

    def tp(x, y):
        return ((tx + x) * z, (tb - y) * z)

    for side in (-1, 1):
        td.polygon([tp(side * 9.6 - 1.3, 0), tp(side * 9.6 + 1.3, 0), tp(side * 8.8 + 1.1, 26),
                    tp(side * 8.8 - 1.1, 26)], fill=255)
    td.polygon([tp(-12.5, 20.5), tp(12.5, 20.5), tp(12.5, 22.3), tp(-12.5, 22.3)], fill=255)
    kasagi = [tp(-17 + 34 * k / 30, 27.0 + 2.6 * (abs(-17 + 34 * k / 30) / 17) ** 2.2) for k in range(31)]
    kasagi += [tp(17 - 34 * k / 30, 29.6 + 3.2 * (abs(17 - 34 * k / 30) / 17) ** 2.2) for k in range(31)]
    td.polygon(kasagi, fill=255)
    td.polygon([tp(-1, 22.3), tp(1, 22.3), tp(1, 27), tp(-1, 27)], fill=255)
    tg = np.asarray(tg.resize((W, H), Image.BOX), np.float32) / 255
    img = over(img, lin('#040103'), tg)
    img += lin('#ff4a36') * (np.clip(tg - sample(tg, xx + 1.0 * S, yy + 0.6 * S), 0, 1) * 0.7)[..., None]

    # Floor: perspective grid with the moon's reflection.
    below = (Y > HORIZON).astype(np.float32)
    dy = np.maximum(Y - HORIZON, 0.25)
    z = 40.0 / dy
    u = (X - 320.0) * z / 40.0
    gu = np.abs(np.gradient(u, axis=1)) + np.abs(np.gradient(u, axis=0))
    gv = np.abs(np.gradient(z, axis=0)) + np.abs(np.gradient(z, axis=1))
    cu, cv = 0.46, 0.5
    lu = 1 - smoothstep(0, np.maximum(gu * 1.1, 1e-4), np.abs(u / cu - np.round(u / cu)) * cu)
    lv = 1 - smoothstep(0, np.maximum(gv * 1.1, 1e-4), np.abs(z / cv - np.round(z / cv)) * cv)
    fade = np.exp(-z / 3.5) * np.clip(1 - gv / cv * 2.2, 0, 1) * np.clip(1 - gu / cu * 2.2, 0, 1)
    grid = np.maximum(lu, lv) * fade * below
    floor = lin('#030104') + lin('#1c030a') * np.exp(-dy / 22.0)[..., None]
    img = img * (1 - below[..., None]) + floor * below[..., None]
    refl = np.exp(-((X - MX) / (30 + dy * 0.45)) ** 2) * np.exp(-dy / 80.0) * below
    ripple = 0.5 + 0.5 * np.sin(dy ** 1.25 * 2.2 + 4 * fbm(H, W, 24, 2, rng))
    img += lin('#ff3420') * (refl * ripple * 0.28)[..., None]
    img += lin('#ff2046') * (grid * 0.55 + blur(grid, 2.5 * S) * 0.55)[..., None]
    img += lin('#ff5a50') * (np.exp(-((Y - HORIZON) / 0.8) ** 2) * 0.8)[..., None]

    # The katana, in blade coordinates: s along the axis, n toward the edge.
    top = np.array([196.0, 28.0])
    entry = np.array([216.0, 384.0])
    axis = (entry - top) / np.linalg.norm(entry - top)
    nrm = np.array([axis[1], -axis[0]])
    s = (X - top[0]) * axis[0] + (Y - top[1]) * axis[1]
    n = (X - top[0]) * nrm[0] + (Y - top[1]) * nrm[1]
    se = float(np.linalg.norm(entry - top))
    b0 = 126.0
    n = n + 0.00014 * np.clip(s - b0, 0, None) ** 2
    # Glowing fissures where the blade split the ground.
    fis = Image.new('L', (W * 2, H * 2), 0)
    fd = ImageDraw.Draw(fis)
    frng = np.random.default_rng(77)
    for k in range(9):
        ang = k / 9 * 2 * math.pi + frng.uniform(-0.25, 0.25)
        px, py, width = entry[0], entry[1] + 1, 3.2
        for _ in range(int(frng.integers(9, 16))):
            ang += frng.uniform(-0.45, 0.45)
            step = frng.uniform(5, 11)
            nx, ny = px + math.cos(ang) * step, py + math.sin(ang) * step * 0.28
            fd.line([(px * S * 2, py * S * 2), (nx * S * 2, ny * S * 2)], fill=255,
                    width=max(1, int(width * S * 2)))
            px, py, width = nx, ny, width * 0.86
    fis = np.asarray(fis.resize((W, H), Image.BOX), np.float32) / 255 * below
    img = over(img, lin('#0a0204'), np.clip(blur(fis, 1.5 * S) * 2.0, 0, 1) * 0.8)
    img += lin('#ff3a1a') * (fis * 1.6)[..., None] + lin('#ff5a20') * (blur(fis, 4 * S) * 1.2)[..., None]
    img += lin('#ff3018') * (np.exp(-(((X - entry[0]) / 30.0) ** 2 + ((Y - entry[1]) / 6.0) ** 2)) * 0.6)[..., None]
    # Crimson aura drifting up and to the right.
    sm = fbm(H, W, 4, 6, rng)
    smoke = sample(fbm(H, W, 6, 6, rng, gain=0.6), xx + (sm - 0.5) * 70 * S - yy * 0.18, yy + (sm - 0.5) * 50 * S)
    reach = (8 + 22 * smoothstep(b0, se, s)) * (0.6 + 0.9 * smoke)
    aura = np.exp(-np.abs(n - 3) / reach) * smoothstep(b0 - 10, b0 + 60, s) * smoothstep(se + 30, se - 20, s)
    img += lin('#ff0c22') * (aura * smoothstep(0.35, 0.85, smoke + 0.12) * 0.30)[..., None]
    # Blade: dark polished steel, bright shinogi and hamon, red glint on the edge.
    bw = 19.0 - 3.0 * smoothstep(b0, se, s)
    q = (n + bw * 0.40) / bw
    blade = band(b0, se, s) * band(0.0, 1.0, q)
    hamon = 0.70 + 0.05 * np.sin(s * 0.075) + 0.025 * np.sin(s * 0.19 + 1.1) + 0.012 * (fbm(H, W, 30, 2, rng) - 0.5)
    al = smoothstep(b0, se, s)[..., None]
    c_sj = lin('#14131c') * (0.8 + 0.4 * fbm(H, W, 90, 2, rng)[..., None]) + lin('#2a050c') * al * 0.6
    c_hj = lin('#34343f') + lin('#5a1a22') * (0.4 * al) + lin('#9a9aac') * (0.3 * (1 - al))
    streak = np.exp(-((q - 0.50) / 0.03) ** 2) * \
        (0.55 + 0.45 * smoothstep(b0, b0 + 120, s) * smoothstep(se, se - 140, s))
    c_hj += lin('#ffffff') * (streak * 0.55)[..., None]
    nie = (fbm(H, W, 220, 1, rng) > 0.8).astype(np.float32)
    c_ha = lin('#7c788c') * (0.92 + 0.14 * nie[..., None]) + lin('#ff6a76') * al * 0.08
    bc = np.where((q < 0.08)[..., None], lin('#55556a'), c_sj)
    bc = np.where((q >= 0.36)[..., None], c_hj, bc)
    bc = np.where((q >= hamon)[..., None], c_ha, bc)
    bc += lin('#ffffff') * (np.exp(-((q - 0.36) / 0.016) ** 2) * 0.6)[..., None]
    bc += lin('#f0e8f4') * (np.exp(-((q - hamon) / 0.05) ** 2) * 0.22)[..., None]
    bc += lin('#ff3046') * (np.exp(-((q - 0.985) / 0.022) ** 2) * 0.8)[..., None]
    bc += lin('#ff7a60') * (np.exp(-(q / 0.03) ** 2) * inside * 0.9)[..., None]
    img = over(img, lin('#040102'), band(b0, se, s) * band(-0.09, 1.09, q) * 0.85)
    img = over(img, bc, blade)
    # Habaki.
    hab = band(b0 - 12, b0, s) * band(-bw * 0.40 - 1.5, bw * 0.60 + 1.5, n)
    hq = np.clip((n + bw * 0.4 + 1.5) / (bw + 3.0), 0, 1)
    hcol = lin('#5a2c0a') + lin('#ffb850') * (np.exp(-((hq - 0.3) / 0.1) ** 2) * 1.0)[..., None]
    hcol += lin('#ffd890') * (np.exp(-((s - (b0 - 9)) / 0.9) ** 2) * 0.5)[..., None]
    img = over(img, hcol, hab)
    # Tsuba.
    ts = b0 - 18
    ell = ((n - 1.5) / 34.0) ** 2 + ((s - ts) / 7.0) ** 2
    tsc = lin('#0e080a') * (0.7 + 0.6 * fbm(H, W, 60, 2, rng)[..., None])
    tsc += lin('#c8782c') * (smoothstep(0.74, 0.9, ell) * smoothstep(1.05, 0.95, ell) * 0.8)[..., None]
    tsc += lin('#ff4a3a') * (np.exp(-((s - (ts + 6.2)) / 0.9) ** 2) * 0.6)[..., None]
    img = over(img, tsc, 1 - smoothstep(0.96, 1.04, ell))
    # Tsuka with ito wrap over ray skin.
    h0, h1 = 7.0, ts - 8
    hw = 12.0 - 1.0 * np.sin(np.clip((s - h0) / (h1 - h0), 0, 1) * math.pi)
    tsuka = band(h0, h1, s) * (1 - smoothstep(hw - AA, hw + AA, np.abs(n - 1.0)))
    cyl = np.clip((n - 1.0) / hw, -1, 1)
    nz = np.sqrt(np.clip(1 - cyl ** 2, 0, 1))
    shade = 0.25 + 0.75 * np.clip(-0.45 * cyl + 0.8 * nz, 0, 1)
    a1 = np.abs(((s + n * 0.9) / 12.0) % 1.0 - 0.5)
    a2 = np.abs(((s - n * 0.9) / 12.0) % 1.0 - 0.5)
    cord = np.clip(np.maximum(smoothstep(0.21, 0.16, a1), smoothstep(0.21, 0.16, a2)), 0, 1)
    same = lin('#9c907e') * (0.7 + 0.45 * fbm(H, W, 160, 1, rng)[..., None])
    silk = lin('#0c0507') + lin('#6a1220') * (np.clip(1 - np.minimum(a1, a2) / 0.21, 0, 1) ** 2 * 0.9)[..., None]
    hc = (same * (1 - cord[..., None]) + silk * cord[..., None]) * shade[..., None]
    hc += lin('#ff3226') * ((1 - nz) ** 2.5 * 0.8)[..., None]
    img = over(img, hc, tsuka)
    fuchi = band(h1, ts - 5.5, s) * (1 - smoothstep(13.0 - AA, 13.0 + AA, np.abs(n - 1.0)))
    img = over(img, lin('#22100a') + lin('#ffa848') * (np.exp(-((cyl + 0.3) / 0.25) ** 2) * 0.8)[..., None], fuchi)
    kas = band(0.0, h0 + 0.5, s) * (1 - smoothstep(13.0 - AA, 13.0 + AA, np.abs(n - 1.0)))
    img = over(img, lin('#140a08') * (0.6 + 0.8 * nz[..., None]) +
               lin('#ffa848') * (np.exp(-((s - 1.5) / 1.2) ** 2) * 0.7)[..., None], kas)

    # Sakura: scattered petals, then a gust sweeping up past the blade.
    def petal_poly(d, cx, cy, sz, ang, col):
        pts = []
        for k in range(28):
            th = 2 * math.pi * k / 28
            rad = sz * (0.62 + 0.38 * abs(math.cos(th))) * (0.8 if math.cos(th) > 0.92 else 1.0)
            ex, ey = rad * math.cos(th), rad * 0.5 * math.sin(th)
            pts.append((cx + ex * math.cos(ang) - ey * math.sin(ang), cy + ex * math.sin(ang) + ey * math.cos(ang)))
        d.polygon(pts, fill=col + (255,))

    def petals(count, lo, hi, soft, alpha):
        m = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(m)
        for _ in range(count):
            cx, cy = rng.uniform(20, 340) * S, rng.uniform(40, 460) * S
            sz, ang = rng.uniform(lo, hi) * S, rng.uniform(0, math.pi)
            petal_poly(d, cx, cy, sz, ang,
                       [(240, 160, 184), (226, 132, 160), (250, 196, 212), (210, 104, 136)][int(rng.integers(4))])
        a = np.asarray(m, np.float32) / 255
        rgb, cov = to_linear(a[..., :3]), a[..., 3]
        if soft:
            cov_b = blur(cov, soft * S)
            rgb = blur(rgb * cov[..., None], soft * S) / np.maximum(cov_b[..., None], 1e-4)
            cov = cov_b
        return rgb, cov * alpha

    for count, lo, hi, soft, alpha, gain in ((26, 1.3, 2.0, 0.0, 0.55, 0.55), (10, 2.4, 3.4, 0.35, 0.7, 0.7),
                                            (3, 6.0, 8.0, 2.2, 0.45, 0.55)):
        prgb, pal = petals(count, lo, hi, soft, alpha)
        img = over(img, prgb * gain, pal)
    gust = []
    for _ in range(34):
        tt = rng.uniform(0, 1)
        spread = 10 + 26 * tt
        gust.append((40 + 300 * tt + 26 * math.sin(tt * 9.0) + rng.normal(0, spread),
                     430 - 330 * tt ** 0.85 - 30 * math.sin(tt * 3.2) + rng.normal(0, spread * 0.6)))
    m = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(m)
    for cx, cy in gust:
        sz, ang = rng.uniform(2.0, 3.8) * S, rng.uniform(0, math.pi)
        petal_poly(d, cx * S, cy * S, sz, ang, [(236, 150, 176), (222, 120, 152), (246, 186, 204)][int(rng.integers(3))])
    a = np.asarray(m, np.float32) / 255
    soft_a = blur(a[..., 3], 0.35 * S)
    soft_c = blur(to_linear(a[..., :3]) * a[..., 3:4], 0.35 * S) / np.maximum(soft_a[..., None], 1e-4)
    img = over(img, soft_c * 0.7, soft_a * 0.7)
    for _ in range(80):   # embers
        ss = rng.uniform(b0 + 60, se)
        off = rng.normal(6, 14)
        ex = top[0] + axis[0] * ss + nrm[0] * off + rng.uniform(0, 36)
        ey = top[1] + axis[1] * ss + nrm[1] * off - rng.uniform(0, 70)
        img += lin('#ff6a38') * (rng.uniform(0.15, 0.9) * np.exp(-((X - ex) ** 2 + ((Y - ey) / 2.0) ** 2) / 0.45))[..., None]

    # Title block.
    mark = wordmark_mask(H, W, 350 * S, 46 * S, 256 * S, skew=0.22)
    crgb, calpha = chrome(mark, [(0.0, '#ffffff'), (0.55, '#e4e8ff'), (1.0, '#98a4dc')], 0.52,
                          [(0.0, '#12051a'), (0.18, '#5a0c26'), (0.62, '#ff4a50'), (1.0, '#ffd8b8')],
                          bevel=2.2 * S, strength=0.9, outline=1.6 * S)
    img += lin('#ff1e3a') * (blur(mark, 14 * S) * 0.55)[..., None]
    img = over(img, crgb, calpha)
    ver = text_mask(H, W, 'V3.0', kanit, 40 * S, at(392, 152), anchor='lm')
    vrgb, valpha = chrome(ver, [(0.0, '#ffffff'), (1.0, '#bcc4ea')], 0.5,
                          [(0.0, '#2a0614'), (0.5, '#ff4a5a'), (1.0, '#ffe0c8')],
                          bevel=1.3 * S, strength=0.9, outline=1.3 * S)
    img += lin('#ff1e3a') * (blur(ver, 7 * S) * 0.4)[..., None]
    img = over(img, vrgb, valpha)
    name = text_mask(H, W, 'Muramasa', kanit, 55 * S, at(470, 202), anchor='mm')
    nrgb, nalpha = chrome(name, [(0.0, '#fff0f2'), (1.0, '#ffb0b8')], 0.5,
                          [(0.0, '#3a0410'), (0.4, '#d0142c'), (1.0, '#ff8a5a')],
                          bevel=1.6 * S, strength=1.0, outline=1.6 * S)
    img += lin('#ff1430') * (blur(name, 10 * S) * 0.6)[..., None]
    img = over(img, nrgb, nalpha)
    sx, sy, sz = 547.0, 128.0, 46.0
    rough = fbm(H, W, 90, 3, rng)
    edge_d = 1 - np.maximum(np.abs(X - sx - sz / 2), np.abs(Y - sy - sz / 2)) / (sz / 2)
    seal = band(sx, sx + sz, X) * band(sy, sy + sz, Y) * \
        smoothstep(0.16, 0.30, rough + 0.6 * np.clip(edge_d * 4, 0, 1) - 0.2)
    ink = np.clip(text_mask(H, W, '村', zen_mura, 21 * S, at(sx + sz / 2, sy + 12.5), anchor='mm') +
                  text_mask(H, W, '正', zen_masa, 21 * S, at(sx + sz / 2, sy + 34.0), anchor='mm'), 0, 1)
    img = over(img, lin('#a80e1c') * (0.8 + 0.4 * rough[..., None]), seal * 0.95)
    img = over(img, lin('#f0e2d8'), ink * np.clip(seal * 1.2, 0, 1))
    credit = text_mask(H, W, 'by TacitusVox', michroma, 12 * S, at(470, 244), anchor='mm', spacing=0.8 * S)
    img = over(img, lin('#05000a'), np.clip(blur(credit, 1.2 * S) * 2.5, 0, 1) * 0.85)
    img = over(img, lin('#f6eef2'), credit)
    ky, kx = np.nonzero(mark > 0.5)
    k_top = ky.min()
    glints = ((kx[ky <= k_top + 2].min() / S + 3, k_top / S + 2, 11.0, 0.75),
              tuple(top + axis * (b0 - 8) + nrm * 3) + (9.0, 0.5))
    for gx, gy, size, amount in glints:
        img += star_flare(H, W, gx * S, gy * S, size * S, '#fff4f8', amount)

    # Bloom and vignette.
    bright = np.clip(img - 0.75, 0, None)
    img = img + blur(bright, 5 * S) * 0.3 + blur(bright, 20 * S) * 0.2
    vig = np.hypot((X - 320) / 430.0, (Y - 230) / 330.0)
    img *= (1 - 0.5 * np.clip(vig - 0.55, 0, 1))[..., None]
    return finish(img, S)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--fonts', required=True, help='directory holding the pinned font files')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    save_png(render(args.fonts), args.output)
    print('wrote', args.output)


if __name__ == '__main__':
    main()
