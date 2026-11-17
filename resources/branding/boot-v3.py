#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the version-agnostic K-UI Bootloader v3 boot-disc art (640x480).

Minimal retro-futurism: a black sky, a neon perspective grid fading into the
horizon haze, the chrome K-UI wordmark, BOOTLOADER and a neon "v3". No release
name or scene, so the same disc serves every K-UI release. The countdown panel
(y=360..456) sits over the grid. tools/build_splash.py pins the committed
boot-red.png (the bootstrap's historical asset name).

    python3 resources/branding/boot-v3.py --fonts FONT_DIR boot-red.png
"""
import argparse
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from kui_art import blur, chrome, finish, font, lin, over, save_png, smoothstep, text_mask, wordmark_mask  # noqa: E402

S = 2
W, H = 640 * S, 480 * S


def render(fonts):
    michroma = font(fonts, "michroma-latin-400-normal.woff")
    kanit = font(fonts, "kanit-latin-900-italic.woff")
    rng = np.random.default_rng(3)
    horizon, vpx = 286 * S, 320 * S
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)

    t = np.clip(yy / horizon, 0, 1)[..., None]
    img = lin('#040208') * (1 - t ** 2) + lin('#1c040c') * t ** 2
    img += lin('#ff2448') * (0.16 * np.exp(-((horizon - yy) / (46 * S)).clip(0)) * (yy <= horizon))[..., None]
    for _ in range(90):
        sx, sy = rng.uniform(0, W), rng.uniform(0, horizon * 0.75)
        b = rng.uniform(0.15, 0.6) * (1 - sy / horizon)
        img += lin('#ffd8e8') * (b * np.exp(-((xx - sx) ** 2 + (yy - sy) ** 2) / (2 * (0.9 * S) ** 2)))[..., None]

    below = yy > horizon
    dy = np.maximum(yy - horizon, 0.5)
    z = (60.0 * S) / dy
    u = (xx - vpx) * z / (60.0 * S)
    gu = np.abs(np.gradient(u, axis=1)) + np.abs(np.gradient(u, axis=0))
    gv = np.abs(np.gradient(z, axis=0)) + np.abs(np.gradient(z, axis=1))
    cu, cv = 0.55, 0.62
    lu = 1 - smoothstep(0, np.maximum(gu * 1.2, 1e-4), np.abs(u / cu - np.round(u / cu)) * cu)
    lv = 1 - smoothstep(0, np.maximum(gv * 1.2, 1e-4), np.abs(z / cv - np.round(z / cv)) * cv)
    fade = np.exp(-z / 6.0) * np.clip(1.0 - gv / cv * 2.2, 0, 1) * np.clip(1.0 - gu / cu * 2.2, 0, 1)
    lines = np.maximum(lu, lv) * fade * below
    floor = lin('#07020a') + lin('#2a0610') * np.exp(-dy / (40 * S))[..., None]
    img = np.where(below[..., None], floor, img)
    img += lin('#ff2a55') * (lines * 1.25 + blur(lines, 3 * S) * 1.2)[..., None]
    img += lin('#ff6080') * (np.exp(-((yy - horizon) / (1.2 * S)) ** 2) * 1.5)[..., None]
    img += lin('#ff2040') * (np.exp(-np.abs(yy - horizon) / (14 * S)) * 0.35)[..., None]
    img += lin('#ff3050') * (np.exp(-(((xx - vpx) / (170 * S)) ** 2 + ((yy - horizon) / (36 * S)) ** 2)) * 0.45)[..., None]

    mark = wordmark_mask(H, W, (W - 300 * S - 0.22 * 44 * 300 / 174 * S) / 2, 66 * S, 300 * S, skew=0.22)
    crgb, calpha = chrome(mark, [(0.0, '#ffffff'), (0.55, '#dfe6ff'), (1.0, '#8e9cd8')], 0.52,
                          [(0.0, '#14061c'), (0.18, '#5a0c2a'), (0.62, '#ff4a5a'), (1.0, '#ffd2b0')],
                          bevel=2.2 * S, strength=0.9, outline=1.6 * S)
    img += lin('#ff1e44') * (blur(mark, 16 * S) * 0.9)[..., None]
    img = over(img, crgb, calpha)
    boot = text_mask(H, W, 'BOOTLOADER', michroma, 25 * S, (W / 2 - 34 * S, 205 * S), anchor='mm', spacing=7 * S)
    img += lin('#ff2050') * (blur(boot, 5 * S) * 0.7)[..., None]
    img = over(img, lin('#f4ecf4'), boot)
    v3 = text_mask(H, W, 'v3', kanit, 58 * S, (W / 2 + 196 * S, 203 * S), anchor='mm')
    img += lin('#ff1a40') * (blur(v3, 10 * S) * 1.6 + blur(v3, 3 * S) * 1.2)[..., None]
    img = over(img, lin('#ffd6e0'), v3 * 0.95)
    r = np.hypot((xx - W / 2) / (W * 0.62), (yy - H * 0.45) / (H * 0.70))
    img *= (1 - 0.35 * np.clip(r - 0.55, 0, 1))[..., None]
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
