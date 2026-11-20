# K-UI shell font sources

These are unmodified DejaVu Sans and DejaVu Sans Bold font files supplied by the
local Debian `fonts-dejavu-core` package. They are independent of DreamShell.
The font license is in `LICENSE.txt` and `../../LICENSES/DejaVu-fonts.txt`; the
latter is included automatically in both downloadable build packages.

The shell uses original rasterizations of ASCII 32–126 at 14 pixels regular
and 20 pixels bold. Four-bit coverage preserves antialiasing with a small static
atlas. Glyphs fit within individual advance cells; the renderer adds one pixel
between characters. The line boxes are 18 and 24 pixels high.

Normal builds compile the committed `src/dreamcast/shell_font_data.inc`; font
generation is not a build step. The font renderer requires no Pillow, FreeType
or filesystem access at runtime. To regenerate byte-identically, use Pillow
**12.3.0** with FreeType **2.14.3**:

```sh
python3 tools/generate_shell_font.py
python3 tools/generate_shell_font.py --check
```

The generator checks these source SHA-256 values and the rasterizer versions:

| Font | SHA-256 |
|---|---|
| `DejaVuSans.ttf` | `ae7b7855e115a5966d8b1b3f80f254ccc117ec86f9965e202ee2940453837280` |
| `DejaVuSans-Bold.ttf` | `5c1247acef7f2b8522a31742c76d6adcb5569bacc0be7ceaa4dc39dd252ce895` |

Upstream project: <https://dejavu-fonts.github.io/>.

## Japanese GUI glyphs

The GUI includes 7,331 full-width glyph cells and 63 half-width kana cells derived
from the Japanese face of Noto Sans CJK Regular 2.004. The source collection's
SHA-256 is `b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a`.
The committed bitmap is compiled only into the Japanese-enabled Dreamcast GUI.
Drawing neither reads BIOS ROM nor touches the card. Private-use characters
and characters absent from the subset use the question-mark glyph.

To regenerate with Pillow 13.0.0, FreeType 2.14.3, and fontTools, run
`python3 tools/generate_shell_japanese_font.py /path/to/NotoSansCJK-Regular.ttc`.
Add `--check` to verify the committed output. The font and derived bitmaps carry
the [SIL Open Font License](../../LICENSES/Noto-CJK.txt).
