# Internet box art

**Games → START → Scan from internet** matches a disc's IP.BIN product number and region to a front cover. The catalogue includes 1,104 release-specific Libretro boxes. For 329 of those releases K-UI can get a smaller JPEG directly from Internet Archive. For example, the US Crazy Taxi JPEG is 192 × 190 and 23,900 bytes. Its Libretro PNG is 512 × 510 and 490,502 bytes. The corresponding US Skies of Arcadia files are 9,798 and 421,855 bytes.

K-UI requests the smaller version first when present. If that file is missing, oversized, invalid, or no longer matches a pinned item thumbnail, it can fetch the regional Libretro PNG. Both sources use HTTPS. These indexes ship with K-UI, so the Dreamcast only requests covers for selected games. User-assigned covers keep priority.

K-UI draws at 640 × 480. The Games views use 160-, 104- and 56-pixel square cover tiles. K-UI fits each cover to all three sizes on the Dreamcast and preserves its aspect ratio. The compact RGB565 file occupies 79,120 bytes and its cache record takes another 512 bytes before filesystem allocation. A scan can also convert a previously downloaded PNG already on the card.

The scanner identifies the source by its header. It accepts baseline JPEG and non-interlaced PNG up to 4096 × 4096 pixels and 32 MiB per file. Large progressive JPEGs and interlaced PNGs are unsupported. Full-size PNGs are reduced a few rows at a time. Large baseline JPEGs use decoder scaling before the final resize. A 3840 × 2160 transparent PNG and a 2160 × 3840 JPEG used 229,328 and 484,304 bytes of extra scan heap respectively on the test host. These figures exclude the running GUI, network and TLS. The original download is removed after the compact cover is installed.

## Matching

A single disc product and compatible region take precedence. Japanese limited editions and demos can select distinct art. An exact normalized retail title and compatible region can also identify a cover. Multiple equally valid editions or regions result in a missing match. The complete catalogue and the smaller JPEG index share a fingerprint of their sorted release names, checked when building the console code.

The jewel-case catalogue contributes 307 smaller covers. A source must publish a JPEG thumbnail linked to its named original PNG. The generator retains region and edition, and only combines candidate files across disc, revision or language suffixes when their thumbnail hashes agree. A further 22 front scans are matched to individual archive items. Their manifest records the exact file, size, SHA-1 and CRC32. Fourteen of those items expose their verified front as the archive item tile. K-UI checks the downloaded tile against its known size and CRC before installing it. [Jewel-case collection](https://archive.org/details/advanceddaisenryakusturmubereuropajapan) · [US Crazy Taxi front scan](https://archive.org/details/CrazyTaxi-CompleteScans) · [Individual release manifest](../resources/game-media/cover-scans.tsv)

## Catalogue maintenance

The generated release index is adapted from the Redump Dreamcast DAT distributed by Libretro and the filenames in the Libretro Dreamcast thumbnails project. It joins disc product numbers and regions to existing image filenames, consolidates discs sharing a retail box and omits disc hashes. The derived catalogue uses the upstream database's Creative Commons Attribution-ShareAlike 4.0 International license. The indexes contain metadata. Artwork is retrieved from the providers. [Libretro Dreamcast DAT](https://github.com/libretro/libretro-database/blob/969e31a324119a17add184e8b716315a74adb51b/metadat/redump/Sega%20-%20Dreamcast.dat) · [Libretro Dreamcast covers](https://github.com/libretro-thumbnails/Sega_-_Dreamcast/tree/28b140d16f188301ad697e5935c6dbb412a4ceda/Named_Boxarts) · [Database license](https://github.com/libretro/libretro-database/blob/969e31a324119a17add184e8b716315a74adb51b/LICENSE)

To reproduce both indexes, use the linked DAT and a Git checkout containing the linked thumbnails revision. The first generator checks the DAT blob and artwork tree. The second checks the jewel-case thumbnail inventory against its pinned digest. With `--verify-items` it also compares the individual front scan manifest to the live source metadata. Ordinary builds use the checked-in indexes.

```sh
python3 tools/build_internet_cover_catalog.py "/path/to/Sega - Dreamcast.dat" /path/to/Sega_-_Dreamcast
python3 tools/index_small_cover_sources.py --verify-items
```

The opt-in live test retrieves the real US Crazy Taxi and Skies of Arcadia sources and verifies their hashes. Its card mode exercises the smaller response and the full PNG fallback in the production Games worker on FAT32 and exFAT image files.

```sh
make build/bba-media-integration build/games-covers-image
python3 tests/test_internet_cover_live.py --card
```
