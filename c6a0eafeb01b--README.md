# K-UI 3.0 artwork

The maintainer’s November 17, 2026 artwork set contains seven 1448 × 1086 splash images and three 2172 × 724 banners. The full-size originals live in `sources/`. Each theme or banner directory contains a PC-side `source.json` that pins the original PNG by SHA-256 and specifies the conversion to Dreamcast RGB565.

## Full themes

| Settings name | Stable folder ID | Original composition |
| --- | --- | --- |
| Moonlight Muramasa | `moonlit-muramasa` | A cyber-samurai overlooks a rainy village beneath a blue moon |
| Chrome Muramasa | `chrome-muramasa` | Chrome K-UI lettering and an orbital glow above a silver katana |
| Neon Eclipse | `neon-eclipse` | A glowing katana crosses an orange eclipse over a magenta grid |
| Indigo Waves | `indigo-waves` | Indigo woodblock waves, Mount Fuji and a vermilion sun |
| Thanksgiving Samurai | `thanksgiving-samurai` | An undead samurai raises his sword over a Thanksgiving feast |
| Zombie Turkey | `zombie-turkey` | A turkey escapes a refrigerator with a steaming cup |
| Gravy Racer | `gravy-racer` | A turkey races a flying gravy boat through a neon city |

Moonlight Muramasa is the 3.0 starting theme. It retains the original `moonlit-muramasa` folder ID so existing selections resolve. Its original PNG has SHA-256 `d032f007afc952b337bb2dbf47fb19cbf9801832f5a8b5a1197844a249433c23`. The pack keeps the full rainy-rooftop composition for the splash and uses a darkened scene crop behind the standard menu heading.

The alternate full packs contain their own 640 × 480 splash, 640 × 100 menu artwork, and palette. Each splash preserves the full 4:3 composition, lettering and author credit. Each menu uses a separate crop and brightness from the corresponding source. Colors, crop coordinates and resampling settings are recorded in the pack definition.

## Independent banners

| Settings name | Stable folder ID | Original composition |
| --- | --- | --- |
| Midnight Forge | `midnight-forge` | A glowing blade in a dark swordsmith’s workshop |
| Vermilion Sun | `vermilion-sun` | A katana with a digital cyan trail crosses a red sun |
| Thanksgiving Pie | `thanksgiving-pie` | A turkey guards a pie from approaching zombies |

These packs use `"type": "banner"`. Each pack crops its original artwork to fill the left 320 × 100 pixels while retaining the lettering and author credit. The picker shows those pixels at native size. The menu draws the current status in the right half of the 640 × 100 strip. Users can pair any of these banners with any full theme.

## Build and install

From the repository root, run the following command to stage the prepared collection for an SD card.

```sh
python3 tools/pack_theme.py stage-all build/theme-stage/KUI/themes
```

To reproduce a single pack and compare it with the checked-in runtime files, use its stable folder ID.

```sh
python3 tools/pack_theme.py build resources/themes/midnight-forge/source.json resources/themes/midnight-forge --check
python3 tools/pack_theme.py verify resources/themes/midnight-forge
```

Copy the staged `theme.ini`, `.kuiimg` artwork and `.kuiimg.thumb` picker previews to the card. The packer filters the original PNGs for the previews and copies the finished artwork pixels for the three standalone banners. K-UI loads the highlighted preview on demand. Older packs can supply just the manifest and artwork. The original PNGs and conversion definitions stay on the build computer. These K-UI artwork sources and their prepared assets use the project’s GPL-3.0-only license. The separate minimalist V3 boot-disc artwork belongs to the boot-disc project.

See the [theme installation and controls guide](../../docs/themes.md).
