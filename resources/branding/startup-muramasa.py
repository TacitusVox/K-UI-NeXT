#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generate the K-UI 3.0 "Muramasa" startup and boot artwork.

Procedural pixel art: the scene is drawn at 288x162 and scaled 2x (nearest
neighbour) into the 576x324 art window used by the splash and boot-background
pipeline, pasted at (32,24) on a 640x480 #090102 canvas, so the bootstrap
countdown at y=360..456 sits over the dark lower canvas. The K-UI wordmark
paths come from the project's boot-disc badge (startup-muramasa-wordmark.svg,
rasterized to the .png beside it). The deployed startup.png/boot-red.png are
this script's output; tools/build_splash.py pins those bytes, so rerunning
with different Pillow or font versions cannot silently change a build. See
startup-README.md for the exact versions that produced the pinned file.

    python3 resources/branding/startup-muramasa.py out.png
"""
import math
import random
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = Path(__file__).resolve().parent
W, H = 288, 162
rng = random.Random(1858)  # fixed seed: reproducible artwork
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
FONT = "/usr/share/fonts/truetype/dejavu/"


def hexrgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def lerp(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def dither_band(img, y0, y1, stops):
    """Vertical gradient through palette stops with 4x4 ordered dithering."""
    px = img.load()
    cols = [hexrgb(c) for c in stops]
    for y in range(y0, y1):
        f = (y - y0) / max(1, y1 - y0 - 1) * (len(cols) - 1)
        i = min(int(f), len(cols) - 2)
        frac = f - i
        for x in range(W):
            px[x, y] = cols[i + 1] if frac * 16 > BAYER[y & 3][x & 3] + 0.5 else cols[i]


def ridge(seed, base, amp, rough, peaks=()):
    """Midpoint-displacement skyline, optionally with conical peaks."""
    r = random.Random(seed)
    n = 513
    h = [0.0] * n
    h[0], h[-1] = r.uniform(-1, 1), r.uniform(-1, 1)
    step, scale = n - 1, 1.0
    while step > 1:
        half = step // 2
        for i in range(half, n, step):
            h[i] = (h[i - half] + h[i + half]) / 2 + r.uniform(-1, 1) * scale
        step, scale = half, scale * rough
    out = []
    for x in range(W):
        v = base - h[int(x / W * (n - 1))] * amp
        for cx, top, width in peaks:
            d = abs(x - cx) / width
            if d < 1:
                v = min(v, top + (base - top) * d ** 1.15)
        out.append(int(v))
    return out


def main(out_path):
    img = Image.new("RGB", (W, H))
    horizon = 112
    dither_band(img, 0, horizon, ["#05030f", "#0b0620", "#160a2c", "#260b32",
                                  "#3d0c34", "#5a0f33", "#7a142f", "#9c1f2b"])
    d = ImageDraw.Draw(img)
    # Stars: sparse, brighter toward the top.
    for _ in range(70):
        x, y = rng.randrange(W), rng.randrange(0, 70)
        c = rng.choice(["#ffffff", "#ffd8e8", "#c8b8ff", "#8f7fb0"])
        d.point((x, y), fill=c)
        if rng.random() < 0.08 and y > 2:
            d.point((x, y - 1), fill="#6a5a8a"); d.point((x, y + 1), fill="#6a5a8a")
    # Blood moon with dithered glow, limb shading and craters.
    mx, my, mr = 214, 50, 40
    px = img.load()
    for y in range(max(0, my - mr - 14), min(horizon, my + mr + 14)):
        for x in range(mx - mr - 14, mx + mr + 15):
            if not 0 <= x < W:
                continue
            dist = math.hypot(x - mx, y - my)
            if dist <= mr:
                limb = dist / mr
                shade = 0.55 + 0.45 * (1 - limb ** 2.2) - 0.18 * ((x - mx) / mr + 1) / 2
                base = lerp(hexrgb("#3c0612"), hexrgb("#e2403c"), max(0, min(1, shade)))
                n = math.sin(x * 0.37 + y * 0.21) + math.sin(x * 0.13 - y * 0.29) * 1.3
                if n > 1.35 and BAYER[y & 3][x & 3] < 9:
                    base = lerp(base, hexrgb("#4a0616"), 0.45)
                px[x, y] = base
            elif dist <= mr + 14:
                glow = 1 - (dist - mr) / 14
                if glow * 16 > BAYER[y & 3][x & 3] + 2:
                    px[x, y] = lerp(px[x, y], hexrgb("#ff5a46"), 0.22 + 0.25 * glow)
    # Far and near ranges; a snow-capped cone behind the near ridge.
    far = ridge(7, 100, 9, 0.55, peaks=[(118, 58, 34)])
    near = ridge(19, 110, 6, 0.6, peaks=[(250, 92, 26)])
    for x in range(W):
        for y in range(far[x], horizon):
            px[x, y] = hexrgb("#2b0a26") if y > far[x] + 1 else hexrgb("#4a1236")
        if far[x] < 72 and 102 < x < 134:  # snow cap on the cone
            for y in range(far[x], min(horizon, far[x] + 2 + (72 - far[x]) // 3)):
                px[x, y] = hexrgb("#f2c8d6") if (x + y) % 5 else hexrgb("#c48aa4")
        for y in range(near[x], horizon):
            px[x, y] = hexrgb("#150510") if y > near[x] else hexrgb("#3a0c22")
    # City-free horizon line and a reflective lake with a perspective grid.
    for x in range(W):
        px[x, horizon] = hexrgb("#ff6a50")
    for y in range(horizon + 1, H):
        t = (y - horizon) / (H - horizon)
        for x in range(W):
            px[x, y] = lerp(hexrgb("#24060f"), hexrgb("#0c0207"), t)
    for k in range(1, 12):  # horizontal grid lines, denser near the horizon
        y = horizon + int(round((k / 11) ** 1.9 * (H - horizon - 1)))
        for x in range(W):
            px[x, y] = lerp(px[x, y], hexrgb("#ff2a4a"), 0.55 - 0.25 * (k / 11))
    for k in range(-14, 15):  # vanishing lines toward the moon's x
        x0 = mx + k * 2.2
        for y in range(horizon + 1, H):
            t = (y - horizon) / (H - horizon)
            x = int(round(x0 + (k * 26) * t))
            if 0 <= x < W:
                px[x, y] = lerp(px[x, y], hexrgb("#c01838"), 0.45)
    for y in range(horizon + 2, H, 2):  # glitter path: broken dashes
        fade = 1 - (y - horizon) / (H - horizon)
        half = 6 + int((y - horizon) * 0.22)
        x = mx - half + rng.randrange(0, 3)
        while x < mx + half:
            dash = rng.randrange(2, 7)
            if rng.random() < 0.55 + 0.3 * fade:
                c = lerp(hexrgb("#a8243a"), hexrgb("#ff9a70"), 0.35 + 0.6 * fade)
                for k in range(dash):
                    if 0 <= x + k < W:
                        px[x + k, y] = c
            x += dash + rng.randrange(1, 5)
    # Rocky outcrop (left) with crimson rim light.
    rock = [(0, 162), (0, 128), (14, 120), (30, 117), (44, 106), (58, 101), (70, 104),
            (84, 110), (98, 120), (112, 130), (126, 162)]
    d.polygon(rock, fill="#0d0308")
    for x in range(0, 126):
        top = next((y for y in range(96, H) if px[x, y] == hexrgb("#0d0308")), None)
        if top is not None:
            px[x, top] = hexrgb("#ff5040") if x > 40 else hexrgb("#a8243a")
            if top + 1 < H:
                px[x, top + 1] = hexrgb("#5a0c1c")
    for _ in range(40):  # facets
        x, y = rng.randrange(6, 118), rng.randrange(112, 160)
        if px[x, y] == hexrgb("#0d0308"):
            d.line([(x, y), (x + rng.randrange(2, 6), y + rng.randrange(-2, 3))], fill="#2a0812")
    # The katana: handle up, blade into the stone, tilted with a slight sori.
    top, bottom = (70.0, 6.0), (56.0, 116.0)
    ax, ay = bottom[0] - top[0], bottom[1] - top[1]
    length = math.hypot(ax, ay)
    ux, uy = ax / length, ay / length
    nx, ny = -uy, ux  # normal: +n is the cutting edge (faces right)

    def at(t, off):
        bend = 2.6 * math.sin(math.pi * min(1, max(0, (t - 0.25) / 0.75)))
        return (top[0] + ux * length * t + nx * (off + bend), top[1] + uy * length * t + ny * (off + bend))
    # Aura around the blade.
    aura = Image.new("L", (W, H))
    ad = ImageDraw.Draw(aura)
    ad.line([at(0.27, 0), at(1.0, 0)], fill=255, width=15)
    aura = aura.filter(ImageFilter.GaussianBlur(6))
    ap = aura.load()
    for y in range(H):
        for x in range(W):
            a = ap[x, y] / 255
            if a > 0.05 and a * 16 > BAYER[y & 3][x & 3] * 0.9:
                px[x, y] = lerp(px[x, y], hexrgb("#ff1e3c"), min(0.7, a * 0.85))
    # Blade body, steel shading, hamon and glowing edge.
    for i in range(0, 400):
        t = 0.255 + 0.745 * i / 399
        if at(t, 0)[1] > 112:
            break
        for j in range(-3, 5):
            o = j * 0.95
            x, y = at(t, o)
            xi, yi = int(round(x)), int(round(y))
            if 0 <= xi < W and 0 <= yi < H:
                if j == -3:
                    c = "#1a1420"  # spine outline
                elif j == -2:
                    c = "#5a5a74"
                elif j == -1:
                    c = "#9a9ab6"
                elif j in (0, 1):
                    c = "#d6d6e6" if (i // 11) % 4 else "#f6f4ff"
                elif j == 2:
                    c = "#ffffff" if (i // 5) % 2 else "#ffd6de"  # hamon
                elif j == 3:
                    c = "#ff3a56"  # edge glow
                else:
                    c = "#7a0c22"
                px[xi, yi] = hexrgb(c)
    # Habaki, tsuba, wrapped tsuka and kashira.
    hx, hy = at(0.245, 0)
    d.rectangle([hx - 3, hy - 2, hx + 4, hy + 2], fill="#ffc56a", outline="#7a4a16")
    tx, ty = at(0.225, 0.5)
    d.ellipse([tx - 12, ty - 4, tx + 12, ty + 4], fill="#120806", outline="#ffc56a")
    d.ellipse([tx - 9, ty - 2, tx + 9, ty + 2], outline="#7a4a16")
    for k in (-6, 6):
        d.point((tx + k, ty), fill="#ffc56a")
    for i in range(0, 160):
        t = 0.02 + 0.19 * i / 159
        for j in range(-3, 4):
            x, y = at(t, j * 0.95)
            xi, yi = int(round(x)), int(round(y))
            if abs(j) == 3:
                c = "#0a0306"
            else:
                k = (i // 4 + abs(j)) % 4
                c = "#c8283e" if k == 0 else "#8a1a2c" if k == 1 else "#140810"
            px[xi, yi] = hexrgb(c)
    kx, ky = at(0.012, 0)
    d.rectangle([kx - 3, ky - 2, kx + 3, ky + 1], fill="#ffc56a", outline="#7a4a16")
    # Sageo cord drifting from the handle.
    for _ in range(26):
        t = rng.uniform(0.3, 0.95)
        x, y = at(t, rng.uniform(-10, 14))
        x, y = int(x), int(y - rng.uniform(0, 18))
        if 0 <= x < W and 0 <= y < H:
            px[x, y] = hexrgb(rng.choice(["#ffb070", "#ff6a40", "#ff3a3a"]))
            if y + 1 < H and rng.random() < 0.5:
                px[x, y + 1] = hexrgb("#7a1020")
    # Sakura petals across the scene.
    for _ in range(30):
        x, y = rng.randrange(W), rng.randrange(4, H - 4)
        if 150 <= x <= 266 and 26 <= y <= 104:
            continue
        c1, c2 = rng.choice([("#ffb8d0", "#ff7aa6"), ("#ffd0e0", "#e86a96")])
        px[x, y] = hexrgb(c1)
        if rng.random() < 0.7 and x + 1 < W:
            px[x + 1, y] = hexrgb(c2)
        if rng.random() < 0.35 and y + 1 < H:
            px[x, y + 1] = hexrgb(c2)
    # Title block: the project's K-UI wordmark, V3.0 and Muramasa.
    wm = Image.open(HERE / "startup-muramasa-wordmark.png").convert("RGBA")
    wm = wm.resize((int(wm.width * 0.5), int(wm.height * 0.5)), Image.LANCZOS)
    wpx = wm.load()
    for y in range(wm.height):  # threshold alpha: crisp pixel edges
        for x in range(wm.width):
            r, g, b, a = wpx[x, y]
            wpx[x, y] = (r, g, b, 255 if a > 110 else 0)
    title_x = 160
    img.paste(wm, (title_x + 10, 34), wm)
    small = ImageFont.truetype(FONT + "DejaVuSans-Bold.ttf", 11)
    serif = ImageFont.truetype(FONT + "DejaVuSerif-Bold.ttf", 17)

    def gradient_text(xy, text, font, top_c, bottom_c, stroke="#1a0206"):
        mask = Image.new("L", (W, H))
        md = ImageDraw.Draw(mask)
        md.fontmode = "1"
        md.text(xy, text, font=font, fill=255)
        box = mask.getbbox()
        outline = Image.new("L", (W, H))
        od = ImageDraw.Draw(outline)
        od.fontmode = "1"
        od.text(xy, text, font=font, fill=255, stroke_width=1, stroke_fill=255)
        op, mp = outline.load(), mask.load()
        for y in range(box[1] - 1, box[3] + 1):
            for x in range(box[0] - 1, box[2] + 1):
                if 0 <= x < W and 0 <= y < H:
                    if mp[x, y]:
                        tt = (y - box[1]) / max(1, box[3] - box[1] - 1)
                        px[x, y] = lerp(hexrgb(top_c), hexrgb(bottom_c), tt)
                    elif op[x, y]:
                        px[x, y] = hexrgb(stroke)
        return box
    gradient_text((title_x + 40, 64), "V3.0", small, "#fff2e0", "#ffb070")
    gradient_text((title_x + 6, 76), "Muramasa", serif, "#ffd27a", "#ff2a46")
    art = img.resize((W * 2, H * 2), Image.NEAREST)
    # The small credit stays at full resolution so it remains legible on a TV.
    ad = ImageDraw.Draw(art)
    credit = ImageFont.truetype(FONT + "DejaVuSans-Bold.ttf", 11)
    ad.text((2 * title_x + 70, 2 * 98 + 4), "by TacitusVox", font=credit, fill="#f6e6ee",
            stroke_width=2, stroke_fill="#1a0206")
    canvas = Image.new("RGB", (640, 480), "#090102")
    canvas.paste(art, (32, 24))
    canvas.save(out_path)
    print("wrote", out_path, "colors:", len(canvas.getcolors(1 << 16)))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: startup-muramasa.py OUTPUT.png")
    main(sys.argv[1])
