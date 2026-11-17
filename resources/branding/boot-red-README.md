# Boot disc art (boot-red.png)

**Current art (November 17, 2026): supplied by the owner.** Pixel-art "K-UI V3 /
by TacitusVox / github.com/TacitusVox/K-UI-NeXT" over a night sky with hills
and a setting sun. The owner's 1448x1086 image (4:3) was reduced to 640x480 by
area averaging (Pillow BOX), with no other change. The countdown screen shows it full-screen
under the "Starting K-UI" panel; the menu pages use a dimmed copy.
`tools/build_splash.py` pins this file by its git blob hash.

The earlier generated v3 art (resources/branding/boot-v3.py) is no longer used
for this file. Its notes follow for history.

# K-UI Bootloader v3 boot artwork

The maintainer asked on November 17, 2026 for a boot-disc splash that is
version-agnostic: no release artwork or name (Muramasa, Dáinsleif, Tyrfing),
just a version number in a simple, minimal, retro-futuristic look, reading
"v3" because this is the third K-UI bootloader disc.

`boot-red.png` (the historical bootstrap asset name) is rendered by
[boot-v3.py](boot-v3.py) with the shared helpers in [kui_art.py](kui_art.py):
a black sky with faint stars, a neon perspective grid fading into the horizon
haze, the chrome K-UI wordmark (the boot-disc badge paths, set italic),
BOOTLOADER and a neon `v3`. The scene is rendered in linear light at
1280×960, tone mapped, box-downsampled and ordered-dithered (8×8 Bayer) to
exact RGB565 values, so the bootstrap's encoding is lossless.

```sh
python3 resources/branding/boot-v3.py --fonts FONT_DIR resources/branding/boot-red.png
```

| Input | Pin |
| --- | --- |
| `boot-v3.py` | `31542626281777f3ad8b31853770fda098aeb0ffe864fcd03a075626288cc028` |
| `kui_art.py` | `f10adc942150373348e02903e84c87a92937dd2110ffe24d3b5c9f53dcee51be` |
| Michroma 400 (`@fontsource/michroma` 5.3.0, `michroma-latin-400-normal.woff`, OFL-1.1) | `bd6218f6b39b65298b2eff876d99f15bbb0181c813f1fa9bb823bc095fc80c9c` |
| Kanit 900 Italic (`@fontsource/kanit` 5.3.0, `kanit-latin-900-italic.woff`, OFL-1.1) | `9d7c9601801de8760f1c41172ddaf6b71d622e63dadf127c05bd1ab82e832fb7` |

The generator refuses font files with other bytes. Fonts are used only to
rasterize glyphs; no font file is distributed. The pinned output was produced
with Python 3.13, numpy 2.5.3 and Pillow 12.3.0.

- Deployment Git blob: `d150653181034f95cc416062f120beedffb51015`.
- Deployment SHA-256: `71c5161e52b8f9930e835d32d8432ed29df6c8d1e4bf4838bb1af36860f38c2d`.

`tools/build_splash.py` pins those bytes, so other library or font versions
cannot change a build silently; regenerate and re-pin deliberately.

The countdown panel at y=360..456 sits over the grid. Menu, recovery,
diagnostics and help keep their dimmed backdrop and opaque controls. The art
alone changes no boot delay, buffer, input or loader behavior.

## Previous: K-UI V3.0 Muramasa pixel-art boot artwork

`boot-red.png` now contains the 3.0 Muramasa artwork, identical to
`startup.png`. The historical filename is retained to keep the existing
bootstrap asset contract. See [startup-README.md](startup-README.md) for the
generator, inputs, tool versions, Git/SHA-256 pins and shared startup sound.

The countdown controls at y=360..456 sit over the dark lower canvas. Menu,
recovery, diagnostics and help keep their existing dimmed background and
opaque controls. No boot delay, buffer size, runtime decoder or input behavior
changes accompany the artwork.

## Previous: K-UI V2.0 orange Tyrfing boot artwork

`boot-red.png` now contains the orange pixel-art Tyrfing artwork, identical to
`startup.png`. The historical filename is retained to keep the existing
bootstrap asset contract. See [startup-README.md](startup-README.md) for the
master image, exact conversion, Git/SHA-256 pins and shared startup sound.

The built-in image generation tool created the master for TPMJB's October 15,
2026 request: orange retro futuristic artwork with the cursed blade Tyrfing.
The exact production prompt is [startup-tyrfing-pixel-prompt.txt](startup-tyrfing-pixel-prompt.txt).
`boot-red-prompt.txt` and `startup-dainsleif-prompt.txt` document the previous
Dáinsleif release and are historical records.

The 32-pixel side and 24-pixel upper margins retain all generated lettering.
The countdown controls at y=360..456 sit over the dark lower reserved canvas.
Menu, recovery, diagnostics and help retain their existing dimmed background
and opaque controls. No boot delay, buffer size, runtime decoder or input
behavior changes accompany the artwork.

The source artwork and generated project assets are provided under the
repository's GPL-3.0-only terms. Historical release banners are retained.
