# TJpgDec

The three source files come from ChaN's TJpgDec R0.03 source release. The SHA-256 of the upstream ZIP is `052fe3efbc9a8be29f31597ad009c5b51a4f6905878eb28569e0ab3d46d0c013`. [Upstream release](https://elm-chan.org/fsw/tjpgd/arc/tjpgd3.zip)

K-UI reads JPEGs through FatFs with the portable decoder, 1,024-byte input blocks and RGB output. Local edits validate table and frame segment lengths, coefficient bounds and table selectors, widen dequantization products, clamp DC samples, and skip unused AC dequantization at 1/8 scale. The build uses the baseline entropy decoder and arithmetic clipping. The upstream notice is included in `LICENSES/TJpgDec.txt`.
