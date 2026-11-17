# K-UI V3.0 Muramasa startup artwork

The maintainer asked on November 17, 2026 for the splash of the planned 3.0
release, provisionally named "Muramasa", with TacitusVox branding. After the
first pixel-art version looked crude ("like something drawn in MS Paint"), they
asked for a more creative repaint. The boot disc now has its own
version-agnostic art; see [boot-red-README.md](boot-red-README.md).

`startup.png` is rendered by [startup-muramasa.py](startup-muramasa.py), with
the shared helpers in [kui_art.py](kui_art.py), written by an AI coding
assistant (Claude Code) at that request. The full 640×480 scene: a katana
driven into a neon grid in front of a blood moon, its blade dark polished
steel with a bright shinogi, a soft hamon and a red edge glint, a wrapped
tsuka and gold habaki; glowing fissures and a crimson aura where it entered
the ground; moonlit ridges with a torii; drifting sakura and embers; the chrome
K-UI wordmark (boot-disc badge paths, set italic), chrome `V3.0`, crimson-chrome
`Muramasa`, a 村正 seal and `by TacitusVox`. Fixed seeds make it repeatable. It
is rendered in linear light at 1280×960, tone mapped, box-downsampled and
ordered-dithered (8×8 Bayer) to exact RGB565 values, so the runtime's encoding
is lossless. Title and seal stay inside the television-safe area.

```sh
python3 resources/branding/startup-muramasa.py --fonts FONT_DIR resources/branding/startup.png
```

| Input | Pin |
| --- | --- |
| `startup-muramasa.py` | `ceccbff5a1e8b36f0b984f996e2dd6eb520f4b099359ef0e8462c4ee623cd99e` |
| `kui_art.py` | `f10adc942150373348e02903e84c87a92937dd2110ffe24d3b5c9f53dcee51be` |
| Kanit 900 Italic (`@fontsource/kanit` 5.3.0, `kanit-latin-900-italic.woff`, OFL-1.1) | `9d7c9601801de8760f1c41172ddaf6b71d622e63dadf127c05bd1ab82e832fb7` |
| Michroma 400 (`@fontsource/michroma` 5.3.0, `michroma-latin-400-normal.woff`, OFL-1.1) | `bd6218f6b39b65298b2eff876d99f15bbb0181c813f1fa9bb823bc095fc80c9c` |
| Zen Antique 400, 村 subset (`@fontsource/zen-antique` 5.3.1, `zen-antique-109-400-normal.woff`, OFL-1.1) | `34cb18e5be7efac93c225196899e4e3518b1aae0341f084f45778d9067fd6a6f` |
| Zen Antique 400, 正 subset (`zen-antique-117-400-normal.woff`) | `cbc6471c8fd043a5593c039160377a96c45a2c073615e2b02309e064d25478b4` |

The generator refuses font files with other bytes. Fonts are used only to
rasterize glyphs; no font file is distributed. The pinned output was produced
with Python 3.13, numpy 2.5.3 and Pillow 12.3.0.

- Deployment Git blob: `19c4fb03197b14eebdae77f1c217a0691ee99cab`.
- Deployment SHA-256: `5e80f38f5663f37861b5ce915cdb70e4364c8c7b79e6bd7f4f5436451a98164b`.

`tools/build_splash.py` pins those bytes, so other library or font versions
cannot change a build silently; regenerate and re-pin deliberately. The art
alone changes no startup timing, buffer, input or chime behavior. It already
reads `V3.0`; the release/version string is bumped separately.

Original project artwork and derived assets are offered under GPL-3.0-only.

## Previous: K-UI V3.0 Muramasa pixel-art startup (first version)

The maintainer asked on November 17, 2026 for the splash of the planned 3.0
release, provisionally named "Muramasa", with TacitusVox branding.
`startup.png` and `boot-red.png` are identical copies of this artwork; the
historical boot-red filename remains the bootstrap asset contract.

The scene is procedural pixel art written by an AI coding assistant (Claude
Code) at that request: a crimson synthwave horizon with a blood moon, a
snow-capped peak, a katana driven into stone, drifting petals, the project's
K-UI wordmark, `V3.0`, `Muramasa` and `by TacitusVox`.
[startup-muramasa.py](startup-muramasa.py) draws it with a fixed seed (1858) at
288×162, scales it 2× nearest-neighbour into the 576×324 window and pastes it at
(32,24) on a 640×480 RGB `#090102` canvas. The 32-pixel side and 24-pixel upper
margins match the previous artwork, and everything from y=348 down stays the
dark canvas, so the bootstrap countdown at y=360..456 needs no overlay change.
`tests/test_splash_assets.py` asserts that band.

Inputs and the tools that produced the pinned file:

| Input | SHA-256 |
| --- | --- |
| `startup-muramasa.py` | `bd653d813aa89a734bf3a2b6cb4d2042db61b7cbbafaec7cba2307822febac2b` |
| `startup-muramasa-wordmark.svg` (K-UI wordmark paths from the boot-disc badge, chrome gradient) | `377578e4802d26255662105af925b588900aa12157cf446d3a3ed9f48f86fa01` |
| `startup-muramasa-wordmark.png` (`inkscape startup-muramasa-wordmark.svg --export-type=png -w 190`, Inkscape 1.2.2) | `350718501eacdc869c2f5ea57cf891b653a4ea50a13a1a543f37ec53083e36c3` |
| DejaVu Sans Bold 2.37 (`fonts-dejavu-core` 2.37-8), rendered into pixels only | `5c1247acef7f2b8522a31742c76d6adcb5569bacc0be7ceaa4dc39dd252ce895` |
| DejaVu Serif Bold 2.37 (`fonts-dejavu-core` 2.37-8), rendered into pixels only | `847b33e13925f19ff87e4d934d6b3cf7cac35ce16424f6f670e40c2f377cf2df` |

The generator ran with Pillow 12.3.0:

```sh
python3 resources/branding/startup-muramasa.py resources/branding/startup.png
cp resources/branding/startup.png resources/branding/boot-red.png
```

- Deployment Git blob: `2119ff70b524754a3c851b70f955752e6a090b9d`.
- Deployment SHA-256: `f65a983667f7b6b7b29419e40f3cfc36145ec23368a7b5344d1cff781ff2f5cc`.

`tools/build_splash.py` pins those deployment bytes before encoding the same
640×480 RGB565 arrays, so other Pillow, font or Inkscape versions cannot change
a build silently; regenerate and re-pin deliberately. Runtime buffers, timing,
the startup chime, input and loaders are unchanged. The bootstrap package
ships the generator and wordmark next to the historical Tyrfing records. The
artwork already reads `V3.0`; the release/version string is bumped separately.

Original project artwork and derived assets are offered under GPL-3.0-only. No
font files are distributed; the glyphs exist only as rendered pixels.

## Previous: K-UI V2.0 Tyrfing pixel-art startup — RC14

The maintainer selected variation 2, the character-free pixel-art Tyrfing
scene, on October 15, 2026. `startup-tyrfing-pixel-master.png` is the unmodified
1672×941 RGB image returned by the built-in image generation tool. Its exact
production prompt and reference are recorded in
[startup-tyrfing-pixel-prompt.txt](startup-tyrfing-pixel-prompt.txt).
The previous portrait master and prompt remain as historical sources.

Deployment conversion only: Pillow nearest-neighbor resize of the complete
master to 576×324, pasted at (32,24) on a 640×480 RGB `#090102` canvas. This
preserves all lettering, 32-pixel side margins and a clear lower region for
the existing bootstrap countdown at y=360..456. No cropping, repainted pixels
or lettering edits are applied. `startup.png` and `boot-red.png` are identical.
The historical boot-red filename remains the bootstrap asset contract.

- Deployment Git blob: `a851c7eee7f32cc8340f72de93e9c0030fce1b97`.
- Deployment SHA-256: `45f70493ee914e05f65cd804f17e3f8db7cc7ad927e2b003ca64001f9a9b4d09`.
- Master SHA-256: `9dd274432cf82cb35087823c2819ed9d8932c6c940a4ce0443e81cbef1d6c74b`.

`tools/build_splash.py` pins those deployment bytes before encoding the same
640×480 RGB565 arrays. Runtime buffers, timing, sound, input and loaders do not
change. The orange vector header and disc sword badge remain from RC11.

`release-v2.0-banner.jpg` uses the complete selected master resized with
nearest-neighbor to 1280×720, JPEG quality 95 without chroma subsampling.
The runtime update supplies the new splash; the optional matching CDI supplies
the boot-disc background. The existing compatible boot CD remains usable.
To preview the bootstrap from that CD, use its Card tools entry with the
matching `KUI/tools.kui` from this candidate's optional bootstrap package.

The existing three-note 2.65-second mono startup chime is unchanged. Original
project artwork and derived assets are offered under GPL-3.0-only.
