# K-UI Tyrfing boot-disc badge

The orange Tyrfing badge replaces the earlier cyan visor for K-UI 2.0 at the
maintainer's request on October 15, 2026. The new rune-etched sword consists of
original editable vector paths in `boot-disc-badge.svg`. The large K-UI wordmark
keeps the earlier badge's exact path geometry, position and size, and the
credit keeps its existing position and font. On November 17, 2026 the credit
became `github.com/TacitusVox` (previously `github.com/TPMJB`), at 16 px
instead of 17 so the longer name ends 20 px inside the border. Orange
`#ff8a22` and amber `#ffc56a` sit against the existing navy `#080f23` background.

The earlier badge came from TacitusVox's (then TPMJB's) `K-UI_DS` revision
[`2a5309298dde8fb100da1e2e4e10517695c9780f`](https://github.com/TacitusVox/K-UI_DS/tree/2a5309298dde8fb100da1e2e4e10517695c9780f).
Its origin and unchanged wordmark are documented in
[the original branding record](https://github.com/TacitusVox/K-UI_DS/blob/2a5309298dde8fb100da1e2e4e10517695c9780f/resources/branding/README.md).
The original badge remains in Git history. These project-created assets use
this repository's GPL-3.0-only terms; no DreamShell rendering code is imported.

| File | SHA-256 |
| --- | --- |
| `boot-disc-badge.svg` | `1632303224ac4bf993a25119ddc8e9e7f6b07dc0e3da902cea7e383986804015` |
| `boot-disc-badge.png` | `c4ff37f326060177233db852662b5c931be3b091f23edb6a1fbefd6bda2ce140` |
| `boot-disc-badge.mr` | `946193357fe2692412259364b9b319a7f0a33c77397f5c384ab544fd9a889de2` |
| `launcher-brand.png` | `26110492a9b4116e2a205ddd149240346a7976cb86b958aae6408e814f029301` |

The badge PNG is the actual 320×90 MR preview: a 32-entry palette, of which
the current render uses 31 colors. The two-pixel `#c0c0c0` border is the
console's transparency color. The MR is 4,810 bytes,
within the 8,192-byte reservation. Packaging passes it to mkdcdisc's `-i`
option, which places it at bootstrap offset `0x3820`. Its decoded pixels were
checked against the PNG. Packaging reads it back from the generated CDI and
rejects a missing or substituted badge before publishing the bootstrap artifact.

The same editable SVG supplies the 256×128 launcher header: the sword group
uses `translate(7 21) scale(.95)` and the original wordmark group uses
`translate(12 29) scale(.79)`, plus the amber credit. Since November 17, 2026 the
credit is the maintainer's GitHub link, `github.com/TacitusVox` (previously
`by TacitusVox`), set at x=86 with no letter spacing so it stays inside the
header. The runtime displays that header at its existing 128×64 size. The IP.BIN
author field is `TacitusVox` (previously `K-UI Team`), as the field holds at most 16
characters. App icon assets and embedded
array dimensions are unchanged.

Rebuild the preview, header and MR with Inkscape and Pillow, then regenerate
the embedded shell pixels:

```sh
python3 tools/boot_badge.py --rebuild-branding
python3 tools/generate_shell_art.py
python3 tools/generate_shell_art.py --check
python3 tools/boot_badge.py
python3 tests/test_boot_badge.py
```

The committed rendering used Inkscape 1.2.2 and Pillow 12.3.0, with the existing
DejaVu Sans Mono font. The badge is quantized to at most 32 colors with median-cut and
no dithering. The independent MR encoder uses indexed literals and runs of
2–127 pixels. Ordinary builds and badge validation consume the pinned output
without Inkscape, Pillow, image decoding or runtime allocation. If intentionally
changing the source, review both preview sizes and update the pins in
`tools/boot_badge.py`, `tools/generate_shell_art.py` and these provenance records.

Format references:
[KallistiOS makeip](https://github.com/KallistiOS/KallistiOS/tree/fcfa7d869471591ca1c777543261a7bfea7cb726/utils/makeip),
[mkdcdisc license-screen option](https://gitlab.com/simulant/mkdcdisc/-/blob/4d74e40dd2122e14389a305ed1d86dd024201389/README.md).
