# SPDX-License-Identifier: GPL-3.0-only
"""Shared numpy/Pillow helpers for the K-UI startup and boot-disc artwork.

Scenes are rendered in linear RGB at twice the output size, tone mapped,
box-downsampled to 640x480 and ordered-dithered to exact RGB565 values, so the
committed PNG is what the console shows. Nothing here runs during a build:
tools/build_splash.py pins the committed PNG bytes.
"""
import hashlib
import math
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# The K-UI wordmark paths from the boot-disc badge (startup-muramasa-wordmark.svg).
WORDMARK = (
    "M98 14 H111 V31 L131 14 H149 L124 36 L151 58 H132 L111 41 V58 H98 Z",
    "M155 33 H179 V41 H155 Z",
    "M187 14 H201 V43 Q201 47 206 47 H230 Q235 47 235 43 V14 H249 V44 Q249 58 233 58 H203 Q187 58 187 44 Z",
    "M258 14 H272 V58 H258 Z",
)

# Pinned font inputs (glyphs are rendered into pixels only; no font ships).
FONTS = {
    "kanit-latin-900-italic.woff":
        "9d7c9601801de8760f1c41172ddaf6b71d622e63dadf127c05bd1ab82e832fb7",
    "michroma-latin-400-normal.woff":
        "bd6218f6b39b65298b2eff876d99f15bbb0181c813f1fa9bb823bc095fc80c9c",
    "zen-antique-109-400-normal.woff":
        "34cb18e5be7efac93c225196899e4e3518b1aae0341f084f45778d9067fd6a6f",
    "zen-antique-117-400-normal.woff":
        "cbc6471c8fd043a5593c039160377a96c45a2c073615e2b02309e064d25478b4",
}


def font(directory, name):
    """Return a pinned font path from DIRECTORY, refusing different bytes."""
    path = Path(directory) / name
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != FONTS[name]:
        raise SystemExit(f"{path}: SHA-256 {digest} is not the pinned {FONTS[name]}")
    return str(path)


def hexrgb(h):
    h = h.lstrip('#')
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], np.float32)


def to_linear(c):
    c = np.asarray(c, np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(np.float32)


def to_srgb(c):
    c = np.clip(c, 0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def lin(h):
    """A #rrggbb colour in linear RGB."""
    return to_linear(hexrgb(h))


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def _box_axis(a, r, axis):
    pad = [(0, 0)] * a.ndim
    pad[axis] = (r + 1, r)
    c = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis, dtype=np.float64)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return ((hi - lo) / (2 * r + 1)).astype(np.float32)


def blur(a, sigma):
    """Gaussian approximation: three box passes per axis."""
    if sigma <= 0.3:
        return a.astype(np.float32)
    r = max(1, int(round((math.sqrt(4 * sigma * sigma + 1) - 1) / 2)))
    out = a.astype(np.float32)
    for _ in range(3):
        out = _box_axis(_box_axis(out, r, 0), r, 1)
    return out


def value_noise(h, w, cy, cx, rng):
    g = rng.random((cy + 3, cx + 3)).astype(np.float32)
    im = Image.fromarray(g, mode='F').resize((w, h), Image.BICUBIC, box=(1, 1, cx + 1, cy + 1))
    return np.asarray(im, np.float32)


def fbm(h, w, cells, octaves, rng, lac=2.0, gain=0.5, aspect=None):
    """Fractal value noise; CELLS is the horizontal lattice count of octave one."""
    total = np.zeros((h, w), np.float32)
    amp, norm, cx = 1.0, 0.0, cells
    ar = aspect if aspect else h / w
    for _ in range(octaves):
        total += amp * value_noise(h, w, max(1, int(round(cx * ar))), max(1, int(round(cx))), rng)
        norm += amp
        amp *= gain
        cx *= lac
    return total / norm


def sample(img, x, y):
    """Bilinear lookup of IMG at fractional pixel positions."""
    h, w = img.shape[:2]
    x = np.clip(x, 0, w - 1.001)
    y = np.clip(y, 0, h - 1.001)
    x0, y0 = x.astype(np.int32), y.astype(np.int32)
    fx, fy = x - x0, y - y0
    if img.ndim == 3:
        fx, fy = fx[..., None], fy[..., None]
    top = img[y0, x0] * (1 - fx) + img[y0, x0 + 1] * fx
    bottom = img[y0 + 1, x0] * (1 - fx) + img[y0 + 1, x0 + 1] * fx
    return top * (1 - fy) + bottom * fy


def over(dst, color, alpha):
    a = np.clip(alpha, 0, 1)[..., None]
    return dst * (1 - a) + color * a


def path_polys(d, samples=24):
    """Absolute M/H/V/L/Q/Z SVG path data as polygons."""
    toks = re.findall(r'[MHVLQZ]|-?\d+(?:\.\d+)?', d)
    polys, cur, x, y, i = [], [], 0.0, 0.0, 0
    while i < len(toks):
        t = toks[i]
        i += 1
        if t == 'M':
            x, y = float(toks[i]), float(toks[i + 1])
            i += 2
            cur = [(x, y)]
        elif t == 'H':
            x = float(toks[i])
            i += 1
            cur.append((x, y))
        elif t == 'V':
            y = float(toks[i])
            i += 1
            cur.append((x, y))
        elif t == 'L':
            x, y = float(toks[i]), float(toks[i + 1])
            i += 2
            cur.append((x, y))
        elif t == 'Q':
            cx, cy, ex, ey = (float(v) for v in toks[i:i + 4])
            i += 4
            for k in range(1, samples + 1):
                s = k / samples
                cur.append(((1 - s) ** 2 * x + 2 * (1 - s) * s * cx + s * s * ex,
                            (1 - s) ** 2 * y + 2 * (1 - s) * s * cy + s * s * ey))
            x, y = ex, ey
        elif t == 'Z':
            polys.append(cur)
            cur = []
    if cur:
        polys.append(cur)
    return polys


def wordmark_mask(h, w, x0, y0, width, skew=0.0, ss=4):
    """Rasterize the K-UI wordmark (optionally italic) as a 0..1 coverage mask."""
    vx0, vy0, vw, vh = 98.0, 14.0, 174.0, 44.0
    s = width / vw
    img = Image.new('L', (w * ss, h * ss), 0)
    d = ImageDraw.Draw(img)
    for path in WORDMARK:
        for poly in path_polys(path):
            pts = []
            for px, py in poly:
                yy = (py - vy0) * s
                pts.append(((x0 + (px - vx0) * s + skew * (vh * s - yy)) * ss, (y0 + yy) * ss))
            d.polygon(pts, fill=255)
    return np.asarray(img.resize((w, h), Image.BOX), np.float32) / 255.0


def text_mask(h, w, text, font_path, size, xy, anchor='la', spacing=0.0, ss=4):
    """Coverage mask of TEXT at working-resolution position XY."""
    face = ImageFont.truetype(font_path, int(round(size * ss)))
    img = Image.new('L', (w * ss, h * ss), 0)
    d = ImageDraw.Draw(img)
    x, y = xy[0] * ss, xy[1] * ss
    if spacing:
        widths = [face.getlength(ch) for ch in text]
        total = sum(widths) + spacing * ss * (len(text) - 1)
        x -= total / 2 if anchor[0] == 'm' else total if anchor[0] == 'r' else 0
        for ch, advance in zip(text, widths):
            d.text((x, y), ch, font=face, fill=255, anchor='l' + anchor[1])
            x += advance + spacing * ss
    else:
        d.text((x, y), text, font=face, fill=255, anchor=anchor)
    return np.asarray(img.resize((w, h), Image.BOX), np.float32) / 255.0


def _ramp(stops, t):
    out = np.zeros(t.shape + (3,), np.float32)
    for (a, ca), (b, cb) in zip(stops, stops[1:]):
        f = np.clip((t - a) / max(1e-6, b - a), 0, 1)[..., None]
        seg = lin(ca) * (1 - f) + lin(cb) * f
        out = np.where(((t >= a) & (t <= b))[..., None], seg, out)
    return out


def chrome(mask, top, horizon, bottom, light=(-0.6, -0.8), bevel=3.0, strength=1.0,
           outline=2.0, outline_color='#06000a'):
    """1980s chrome lettering: sky ramp, hard horizon, ground ramp and a bevel.

    Returns (linear RGB, alpha) for compositing; alpha includes a dark outline.
    """
    h, w = mask.shape
    ys, xs = np.nonzero(mask > 0.02)
    y0, y1 = ys.min(), ys.max() + 1
    t = np.broadcast_to(np.clip((np.arange(h, dtype=np.float32) - y0) / max(1, y1 - y0), 0, 1)[:, None], (h, w))
    col = np.where((t < horizon)[..., None], _ramp(top, np.clip(t / horizon, 0, 1)),
                   _ramp(bottom, np.clip((t - horizon) / (1 - horizon), 0, 1)))
    height = blur(mask, bevel)
    gy, gx = np.gradient(height)
    norm = math.hypot(*light)
    shade = -(gx * light[0] / norm + gy * light[1] / norm) * (6.0 * bevel) * strength
    edge = np.clip(1 - height * 2.0, 0, 1)[..., None]
    col = col * (1 + np.clip(shade, -0.75, 1.5)[..., None] * edge) + np.clip(shade, 0, None)[..., None] * edge * 0.6
    ring = np.clip(blur(mask, outline) * 3.0, 0, 1)
    rgb = col * mask[..., None] + lin(outline_color) * (1 - mask[..., None])
    return rgb, np.maximum(mask, ring)


def star_flare(h, w, cx, cy, size, color, intensity=1.0, rays=4):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dx, dy = xx - cx, yy - cy
    r = np.hypot(dx, dy) + 1e-3
    spikes = np.abs(np.cos(np.arctan2(dy, dx) * rays / 2)) ** 60 * np.exp(-r / (size * 0.9))
    v = (np.exp(-(r / (size * 0.08)) ** 2) + np.exp(-r / (size * 0.35)) * 0.35 + spikes) * intensity
    return lin(color) * v[..., None]


def tonemap(c, exposure=1.0):
    """Narkowicz's ACES fit."""
    c = c * exposure
    return np.clip((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0, 1)


BAYER8 = np.array([
    [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]], np.float32)


def finish(img, scale, exposure=1.0):
    """Linear HDR at SCALE x 640x480 -> 640x480 8-bit RGB of exact RGB565 values.

    An 8x8 ordered dither hides RGB565 banding and keeps the repeating pattern
    that deflate stores compactly in the executable.
    """
    srgb = to_srgb(tonemap(img, exposure))
    h, w = srgb.shape[:2]
    small = srgb.reshape(h // scale, scale, w // scale, scale, 3).mean(axis=(1, 3))
    h, w = small.shape[:2]
    levels = np.array([31, 63, 31], np.float32)
    threshold = (BAYER8[np.arange(h)[:, None] % 8, np.arange(w)[None, :] % 8] + 0.5) / 64.0 - 0.5
    q = np.clip(np.round(np.clip(small, 0, 1) * levels + threshold[..., None]), 0, levels).astype(np.int32)
    rgb = np.stack([(q[..., 0] << 3) | (q[..., 0] >> 2), (q[..., 1] << 2) | (q[..., 1] >> 4),
                    (q[..., 2] << 3) | (q[..., 2] >> 2)], -1)
    return rgb.astype(np.uint8)


def save_png(rgb, path):
    Image.fromarray(rgb, 'RGB').save(path, optimize=True)
