# K-UI theme assets

`moonlit-muramasa/` contains a prepared theme for the runtime theme engine.
Copy only its `theme.ini`, `splash.kuiimg` and `header.kuiimg` to
`KUI/themes/moonlit-muramasa/` on the card, or produce a clean install folder:

```sh
python3 tools/pack_theme.py build resources/themes/moonlit-muramasa/source.json build/theme-stage/KUI/themes/moonlit-muramasa
```

The built-in **Default** choice uses the current embedded splash, branding,
icons and original palette. It has no external theme folder to install and
does not replace the owner's separate boot-disc V3 artwork.

## Moonlit Muramasa provenance

The maintainer requested retro-futuristic/pixel artwork for K-UI 3.0 on
November 17, 2026, and selected the rainy village and samurai as a favorite.
`sources/moonlit-muramasa.png` is the unmodified 1448×1086 image returned by
the built-in image generation tool in that conversation. It contains the
K-UI, 3.0, MURAMASA and by TacitusVox lettering. Its SHA-256 is:

```text
d032f007afc952b337bb2dbf47fb19cbf9801832f5a8b5a1197844a249433c23
```

The PC-only `source.json` pins that input and records all deployment
conversion steps. The splash keeps the complete 4:3 composition, resized
to 640×480 using nearest-neighbor sampling and quantized to RGB565.
The header takes the rectangle `(0,570)..(1448,796)`, resizes it to 640×100,
and applies 18% brightness before RGB565 conversion. This region contains
no baked-in title or credit; the existing K-UI header continues to draw its
live status, music information, build and project URL above it. No artwork
is generated, repainted or decoded while navigating menus.

The sample deliberately uses a plain palette-colored menu background. A
theme can supply its own background through the documented pack format.

The original creative brief was a midnight-indigo pixel-art cyber-samurai
viewed from behind on a rainy rooftop, futuristic Japanese rooftops below
a pale moon, cyan lighting and small coral accents. The approved image is
preserved as a source rather than trying to reproduce its generation.
These K-UI artwork assets and conversion sources are distributed under the
project's GPL-3.0-only license, consistent with the other original project
artwork in `resources/branding/`.

## Reproducibility and checks

```sh
python3 tools/pack_theme.py build resources/themes/moonlit-muramasa/source.json resources/themes/moonlit-muramasa --check
python3 tools/pack_theme.py verify resources/themes/moonlit-muramasa
python3 tests/test_pack_theme.py
```

The tests consume the actual prepared files through the production C
manifest/header/CRC code, and reject a corrupted copy. The original
boot-disc and embedded runtime assets are not modified by theme packaging.

See [the theme format and user guide](../../docs/themes.md).
