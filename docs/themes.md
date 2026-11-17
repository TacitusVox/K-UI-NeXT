# Themes

K-UI themes select runtime splash artwork, a shared menu header backdrop,
an optional menu background, and named UI colors. Theme settings are
independent of game/VMU/reader settings and the boot-disc V3 artwork.

The built-in **Default** theme retains the current embedded artwork and
original palette. **Moonlit Muramasa** provides the maintainer's rainy
village/samurai splash, a dark header backdrop, and a coordinated midnight
blue, cyan and coral palette.

## Installation and saved choices

Install a prepared theme folder under `KUI/themes/` on the card. For example:

```text
KUI/themes/moonlit-muramasa/theme.ini
KUI/themes/moonlit-muramasa/splash.kuiimg
KUI/themes/moonlit-muramasa/header.kuiimg
```

Open **Settings → Appearance** to select a theme. Splash, header and
background selections can inherit the selected theme, use the built-in
default, be disabled, or come from another installed theme. Color overrides
are separate from the image choices. Preview changes before applying;
discarding the draft restores the saved appearance.

| Screen | Controls |
| --- | --- |
| Appearance | Up/down selects a row; left/right or A changes the theme or image choice. A opens Colors, previews, applies, or restores defaults. X rescans installed packs; Y previews the splash; START applies; B cancels. |
| Colors | Choose Theme, Neon, Ember or Mono preset, then a color role and its red/green/blue values. Left/right adjusts a channel by one; holding repeats. X resets that role; Y restores all theme colors; A or B returns to Appearance. |

Choosing a different whole theme clears independent image and color
overrides. **Restore defaults** changes only the draft; **Apply** saves it.

The user's choices live in alternating, CRC-checked 256-byte records:

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
including the built-in default; remove unused folders if the catalog is
reported as truncated.

`theme.ini` is an ASCII file no larger than 4096 bytes. Lines are at most
191 bytes. Keys and section names are case-sensitive. Blank lines and
whole-line `#`/`;` comments are permitted. Unknown or duplicate keys are
rejected so typos do not silently change the appearance. Version 1 requires
`[theme]` and `version=1`; omitted colors and artwork inherit the defaults.

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
| Header | 640×100 | 128,000 | Backdrop at `(0,0)`, behind the existing live header |
| Background | 640×480 | 614,400 | Menu backdrop, with normal opaque panels above it |

Headers should be dark and unobtrusive. The existing title/status occupy
the upper-left/center; music and build information occupy the right;
the project URL lies near the lower-left of the header. Do not bake those
labels into a header image or they will appear twice. The earlier 3:1
artwork banners are source concepts, not directly sized runtime headers.

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

Source PNG paths are relative to the JSON file. Image options are:

| Option | Meaning |
| --- | --- |
| `source` | Required PNG path |
| `sha256` | Optional exact source hash to pin the chosen artwork |
| `filter` | `nearest` (default) or `lanczos` |
| `fit` | `contain` (default, letterbox without distortion) or `cover` (crop to fill) |
| `matte` | `#RRGGBB` for transparency/letterboxing; defaults to the pack's background color |
| `crop` | Optional `[left, top, right, bottom]` source-pixel rectangle |
| `brightness` | Optional 0–1 multiplier, applied once during preparation |

An asset value of `"none"` disables that slot. Missing assets inherit the
default. The packer only writes the declared `.kuiimg` files and
`theme.ini`; it never creates user preferences. It publishes the manifest
after preparing all assets. Use `build ... --check` to verify existing
output without rewriting it. `verify` checks a prepared folder without
requiring Pillow.

## Performance and validation

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

Startup immediately shows the existing embedded splash and retains its
three-second deadline and chime. The storage worker reads only the selected
theme definitions and assets during the mounted settings session; it does
not scan the themes directory at boot. A prepared custom splash replaces
the embedded image if ready within the existing display window. Appearance
scans folders only when opened or explicitly refreshed.

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
