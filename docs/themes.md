# Themes

**Moonlight Muramasa** is the K-UI 3.0 starting theme. Its splash shows the samurai on a rooftop above a rainy village. A saved choice takes priority on later boots, including a previously saved **K-UI Original** selection. The stable folder name is `moonlit-muramasa`, so existing settings still work.

The collection also includes **Chrome Muramasa**, **Neon Eclipse**, **Indigo Waves**, **Thanksgiving Samurai**, **Zombie Turkey**, and **Gravy Racer**. The **Banner** row also offers the original wide **Midnight Forge**, **Vermilion Sun**, and **Thanksgiving Pie** artwork. You can pair a banner with any splash or palette.

## Installation and saved choices

Personal themes have their own folders under `KUI/themes/` on the card. Moonlight Muramasa uses these three files.

```text
KUI/themes/moonlit-muramasa/theme.ini
KUI/themes/moonlit-muramasa/splash.kuiimg
KUI/themes/moonlit-muramasa/header.kuiimg
```

Open **Settings → Appearance**. Choose the whole theme first, then change the splash, banner, background or colors independently. For example, choose Moonlight Muramasa under **Theme**, move to **Banner**, and select Midnight Forge.

| Action | Control |
| --- | --- |
| Highlight a setting | D-pad up or down |
| Cycle a theme or image | D-pad left or right |
| Open the list for Theme, Splash, Banner or Background | A on that row |
| Browse names and preview the highlighted choice | D-pad up or down in the list |
| Use the highlighted choice in your draft | A in the list |
| Close the list and keep your previous choice | B in the list |
| Preview the menu with the draft theme, colors, background or banner | Y in Appearance or Colors |
| Preview a splash at full size | Highlight Splash and press Y |
| Return from preview and keep editing the same draft | B |
| Save the draft from the editor or preview | START |
| Reset one color | X in Colors |
| Restore all theme colors | Select **Theme** on the **Preset** row in Colors |
| Rescan installed packs | X on the Appearance screen |

Choosing a different whole theme clears the draft's individual image and color overrides. **Restore defaults** selects Moonlight Muramasa in the draft. Press **START** to save it. This also lets you switch an upgraded installation that explicitly saved the original look.

The picker opens on the current choice. Its preview uses the splash for a whole theme and preserves the wide proportions of a banner. On the Background row, packs with backdrop artwork show the image. The bundled themes use the draft's background color there.

K-UI alternates between two CRC-checked, 256-byte preference records.

```text
KUI/apps/system/theme-a.bin
KUI/apps/system/theme-b.bin
```

The current valid slot is not opened for writing. Saving writes the other
slot, syncs/closes it, then rereads and verifies its bytes. This reduces the
chance of losing the previous selection, while retaining the normal
filesystem/card power-loss limits. Theme packs and manual-copy software
packages must never ship these preference files. At startup, a missing
theme, invalid manifest, unsupported image or failed image CRC uses the
built-in fallback. Preview and Apply instead report a damaged selection
and retain the working appearance. A theme does not contain executable
code or change game compatibility.

## Runtime pack format

The folder name is the stable theme ID: 1–31 ASCII letters, digits,
underscores or hyphens, starting with a letter or digit. `default` and
`none` are reserved selections, including other capitalizations. The runtime catalog holds up to 24 themes,
including the built-in original. Remove unused folders if the catalog is
reported as truncated.

`theme.ini` is an ASCII file no larger than 4096 bytes. Lines are at most
191 bytes. Keys and section names are case-sensitive. Blank lines and
whole-line `#`/`;` comments are permitted. Unknown or duplicate keys are
rejected so typos do not silently change the appearance. The required fields are `[theme]` and `version=1`. A dedicated banner pack adds `type=banner` and a `header` asset. Its name appears when choosing a banner.

```ini
[theme]
version=1
name=My theme
author=TacitusVox
splash=splash.kuiimg
header=header.kuiimg
background=none

[colors]
background=#080F23
panel=#101F35
selection=#20455E
edge=#395369
text=#E7F1FF
muted=#A5BBD1
accent=#6EDDED
accent2=#FF7F86
success=#91DEC0
warning=#FFD18A
selection_text=#F6FAFF
```

Names and authors are printable ASCII, at most 47 characters. An asset
value is either `none`, or a single basename of at most 63 characters
containing ASCII letters, digits, underscores, hyphens and dots, starting
with a letter or digit. Paths, drive prefixes, slashes and `..` are
rejected. Omit a key to use the built-in artwork for that slot; do not use
an empty value.

| Color key | UI role |
| --- | --- |
| `background` | Plain menu background |
| `panel` | Pane and row surfaces |
| `selection` | Highlighted row background |
| `edge` | Borders and separators |
| `text` | Primary text and titles |
| `muted` | Secondary text |
| `accent` | Primary highlights and links |
| `accent2` | Selection marks and secondary highlights |
| `success` | Successful/positive status |
| `warning` | Warning/attention status |
| `selection_text` | Highlighted selection text |

Color values use exactly `#RRGGBB`. The renderer converts the eleven
resolved roles to RGB565 at the start of a draw. Keep text and selection
text visibly distinct from their surfaces, and keep warning/success colors
recognizable on the connected television.

## Image format and layout

All images are prepared on a PC into `.kuiimg` files. The console does not
decode PNGs, resize pictures or run image effects during menu drawing.

| Slot | Pixel size | Uncompressed pixel bytes | Use |
| --- | --- | ---: | --- |
| Splash | 640×480 | 614,400 | Runtime startup artwork; separate from the CD's own artwork |
| Banner (`header`) | 640×100 | 128,000 | Artwork at the top of the menu with live shell information |
| Background | 640×480 | 614,400 | Menu backdrop, with normal opaque panels above it |

The bundled wide banners put their artwork on the left, leaving room on the right for live status. Prepare the banner on its final 640×100 canvas so text and faces keep their proportions.

Each image has this 32-byte header followed by exact row-major,
little-endian RGB565 pixels. There is no compression or transparency in
version 1. Alpha in a source PNG is flattened onto the chosen matte while
preparing the pack.

| Offset | Bytes | Meaning |
| ---: | ---: | --- |
| 0 | 8 | ASCII `KUITEX01` |
| 8 | 4 | Little-endian version, exactly 1 |
| 12 | 2 | Little-endian width |
| 14 | 2 | Little-endian height |
| 16 | 4 | Little-endian pixel payload byte count |
| 20 | 4 | Little-endian CRC32 of the pixel payload |
| 24 | 8 | Reserved, all zero |
| 32 | width × height × 2 | Little-endian RGB565 pixels |

The slot determines the exact allowed dimensions. Truncation, trailing
bytes, nonzero reserved fields and CRC mismatches are rejected before the
image is published to the renderer.

An optional picker preview sits beside each full-size image, for example
`header.kuiimg.thumb`. Splash and backdrop previews measure 320×240.
A regular header measures 320×50 in the picker. A standalone banner pack
shows its left 320×100 artwork at native size. The right half of that
menu strip is reserved for live status. The preview uses the same little-endian
RGB565 layout with the magic `KUITHM01`. Its 32-byte header stores
the preview's payload CRC at offset 20, the matching full-size payload
CRC at offset 24, and four zero bytes at offset 28.

## Prepare a new theme

The PC packer uses Python 3 and Pillow. Its source JSON is an authoring
file, not a runtime preference. A minimal example:

```json
{
  "version": 1,
  "name": "My theme",
  "author": "TacitusVox",
  "colors": {"accent": "#6EDDED", "accent2": "#FF7F86"},
  "assets": {
    "splash": {"source": "my-splash.png", "filter": "nearest"},
    "header": {"source": "my-header.png"}
  }
}
```

```sh
python3 tools/pack_theme.py build my-theme.json staging/KUI/themes/my-theme
python3 tools/pack_theme.py verify staging/KUI/themes/my-theme
```

To stage the seven factory themes and three banners for a full release, run
`python3 tools/pack_theme.py stage-all --managed staging/KUI/assets/themes`.
The SD ZIP and full KUP use the same files. The command verifies the tracked
prepared packs and writes their manifests, runtime artwork, previews and a
bounded `index.txt`. The selected software bank resolves each file. Personal
packs in `KUI/themes` take precedence when their IDs match a factory pack.
The manifest, artwork and previews all follow the same source. Saved choices
continue to use the theme IDs and the existing settings records.

Source PNG paths are relative to the JSON file. Each image supports these options.

| Option | Meaning |
| --- | --- |
| `source` | Required PNG path |
| `sha256` | Optional exact source hash to pin the chosen artwork |
| `filter` | `nearest` (default) or `lanczos` |
| `fit` | `contain` (default, letterbox without distortion), `cover` (crop to fill), or `header-left` (place a full theme’s header artwork to the left of the menu text and fade into the matte) |
| `matte` | `#RRGGBB` for transparency/letterboxing; defaults to the pack's background color |
| `crop` | Optional `[left, top, right, bottom]` source-pixel rectangle |
| `brightness` | Optional 0–1 multiplier, applied once during preparation |
| `dither` | Use `true` to smooth gradients when encoding RGB565. The bundled headers use this setting. Full-size assets default to `false`, and previews default to `true`. |
| `dither` | Spread RGB565 rounding error over neighboring artwork pixels to smooth gradients. Defaults to on for picker previews and off for full-size assets. |
| `credit_at` | For a standalone banner, draw “by” and the theme’s `author` at `[left, top]` in the native 320×100 artwork area. Position it over the original byline to replace small lettering. |
| `logo_detail` | For a standalone banner, fit a `crop` from the original PNG into a `box` in the 320×100 artwork area. Both rectangles use `[left, top, right, bottom]`. |

An asset value of `"none"` disables that slot. Missing assets inherit the
default. The packer writes the declared `.kuiimg` files, their `.thumb`
previews and `theme.ini`. It publishes the manifest
after preparing all assets. Use `build ... --check` to verify existing
output without rewriting it. `verify` checks a prepared folder without
requiring Pillow.

## Performance and validation

In the picker, leaving a choice highlighted for 110 ms requests its
preview. For a bundled splash this reads 153,664 file bytes, including
both headers. A regular header takes 32,064 bytes and a standalone
banner takes 64,064. The picker checks the
full-size header and the preview's CRC. Full preview and Apply still
verify the entire selected artwork. For a missing, damaged or mismatched
preview, the picker reads the installed image. It copies artwork for
standalone banners at native size and averages each 2×2 block into one
pixel for other artwork. The picker holds at most two visited thumbnails,
including an image being loaded, and frees them when you close the list.

The theme engine uses the shell's existing RGB565 software framebuffer;
it does not add a new graphics backend. The selected images are read and
validated outside drawing. Subsequent frames use cached pixels and the
resolved palette. Unselected theme images are not loaded into memory.
The header costs 125 KiB of pixel RAM; each full-screen picture costs
600 KiB. Normal menu image buffers therefore total at most 725 KiB.
The splash buffer is freed when leaving startup or applying an appearance.
All theme image buffers are released before a memory test or executable/game
handoff, and menu images reload on return. Previewing retains a working
appearance while preparing the candidate; temporary RAM use can exceed the
normal menu total.

Cold startup resolves the saved splash before drawing the first SD-runtime artwork. The runtime reads only the selected settings and splash for this frame. The rest of the menu artwork loads separately. With no valid saved choice, startup uses Moonlight Muramasa. A saved **None** selection uses the background color. The existing V3 boot disc shows its own countdown before the SD runtime takes control.

Image loading and theme changes still have a one-time storage cost.
An optional full-screen menu background also adds a framebuffer copy
relative to a plain fill; it is not an unconditional promise of zero CPU
cost. Static artwork, prepared dimensions and bounded allocations are the
first-version scope. Animated backgrounds, scripts and arbitrary layouts
are not part of this format.

Host checks exercise the manifest and saved-record parser, image bounds
and corruption cases, real prepared packs through production C parsing,
controller state transitions, and rendered output. Those checks cannot
establish Dreamcast input latency or SD load timing. Hardware acceptance
still requires comparing default versus custom appearance, opening and
closing Appearance, changing colors/assets, reboot persistence, and game
launch/return while holding the reader and VMU configuration constant.

The host preview tool uses the production renderer and validates prepared
assets with the production manifest/header/CRC code:

```sh
make build/render-theme
build/render-theme appearance resources/themes/moonlit-muramasa build/theme-appearance.ppm
build/render-theme colors resources/themes/moonlit-muramasa build/theme-colors.ppm
build/render-theme home default build/theme-default.ppm
```

The other modes include `splash` for the temporary splash preview screen and `startup` for the first frame. Output is a 640×480 PPM image. The host tool supplies
clearly labeled sample build/music state; it performs no card writes and
does not measure hardware timing.
