# K-UI V3.0 Muramasa startup artwork

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
