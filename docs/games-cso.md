<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Native CSO candidate

This branch adds a separate experimental native SCI Standard reader for CSO.
Physical console acceptance is still required. Use the matching runtime and
`cso-boot.kui` from one build; a standalone resident is not a shell update.
The source CSO stays read-only and is not expanded at launch.

## Admitted profile

- CISO version 1, 24-byte header, zero reserved bytes.
- Independent raw-DEFLATE or uncompressed 2048-byte ISO sectors.
- One native Dreamcast data-session track, with valid IP/boot executable.
- SCI microSD, Standard reader, Background loading Off.
- No Windows CE, CD music or executable RAM patches on this reader.
- At most 31 physical fragments; full header/index/payload file mapping.
- Positive decoded size, exact multiple of 2048, at most 2 GiB; physical file
  length fits 32 bits. Alignment shift at most 9, encoded spans at most 4096.

`.cso` selects DEFLATE automatically; the ZSO codec selector is only for ZSO.
CISO v2, larger compressed blocks and other transports are refused explicitly.
The original uncompressed image and PC importer remain available.

The independent allocation-free decoder supports stored, fixed and dynamic
Huffman streams, including multiple DEFLATE blocks. Exact output size,
lookbehind, all input bounds and alignment padding are checked. It reads
compressed bytes in place from the physical-sector cache into a 32-bit bit
buffer, finds each code with one comparison per code length rather than a
call per bit, and maps the code to its symbol by scanning packed 4-bit code
widths eight at a time; fixed codes map arithmetically. There are no lookup
tables: a 304-byte workspace is private to the serialized CSO reader, and no
game RAM is borrowed.

Host and computer builds also keep per-length counts for 32-symbol rank
buckets (128 more bytes), so a scan skips whole buckets. The low CSO resident
has no room for them (`KUI_DEFLATE_BUCKETS=0` in `Makefile.dc`), nor for the
ZSO readers' index window and word-store copy, so it keeps one index read
per sector. Decoding dominates CSO either way: for faster loading, convert a
CSO to standard LZ4 ZSO on a computer
(`--output-format zso`; see [compressed image import](games-formats.md#compressed-image-import))
and use the [native ZSO reader](games-zso.md). Compare loading speed and
stalls against the same title's uncompressed image; no console throughput or
game-compatibility claim follows from host tests.

At the pinned KOS-patched GCC 15.2.0 toolchain, the linked resident ends at
`0x8c007708`, 248 bytes below the existing `0x8c007800` limit. Its conservative
service-stack sum is 1216 of 1232 bytes. The linked integer-instruction,
relocation, embedded-byte and stack audits pass. These are build measurements,
not console performance results.

## Isolation and evidence

The package `KUIRCB01` and prepared map `KUIRCS01` are typed separately from
LZ4 and LZO ZSO. The existing ZSO wire forms remain unchanged. `native-cso`
and `tools/check_zso_loader.py --codec cso` use the existing low code/BSS and
service-stack limits, linked-instruction checks, embedded byte checks and
source identity checks. The CSO high stage validates the map, source index and
owner IP/executable CRCs before copying trusted prepared state to its resident.

The CSO-only terminal screen reports the failed request and storage error.
It keeps function, parameter, command, LBA, count, destination and reject code;
it omits the ordinary reader's owner-stack snapshot to fit the same reservation.
Normal menu return reboots through the boot ROM immediately, without the
ordinary diagnostic return-counter pages.

Host tests compare independent zlib streams, hostile/truncated input, callback
failures, exact game-image reads, detached physical extents, launch admission
and typed packages. Real FAT32/exFAT fixtures confirm source media remain
unchanged. Hardware acceptance needs boot, continued gameplay reads and a real
load using the complete matching update, followed by comparison with the
known-working uncompressed source.

CHD is separate: [the CHD reader milestone](games-chd.md) does not enable CHD
Play or establish support for common modern v5 codecs.
