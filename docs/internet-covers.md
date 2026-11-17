# Internet box art

Games uses the Dreamcast disc's IP.BIN product number and market flags to look up a regional front cover. K-UI includes the index, so choosing **Scan from internet** only transfers the PNGs that are needed. The PNG comes directly from Libretro's Dreamcast thumbnails on GitHub. There is no account setup. A game that already has an owner-assigned or downloaded cover keeps it.

The index contains 1,104 distinct covers and 1,579 product-to-cover associations. Multiple discs can share a retail box. Japanese limited editions and demos can select distinct artwork when their product numbers identify them. When a product number is missing from the index, an exact normalized retail title and compatible region can identify a cover. Case, punctuation and the position of a leading article are ignored. Multiple equally valid editions or regions produce a missing result. Source coverage varies by release.

The SH-4 resolver object takes about 77 KiB for its code and read-only index, with no mutable static data or heap allocation. Only the selected file's URL is needed. K-UI uses its existing PNG decoder and stores 160, 104 and 56 pixel RGB565 thumbnails on the card. For scale, the indexed US Crazy Taxi cover is 490,502 bytes and decodes to 512 × 510 RGBA, about 1 MiB of pixel data. The encoded file and decoder work buffers add to peak memory. The scanner keeps downloads sequential.

## Index sources and maintenance

The generated index in `src/apps/internet_cover_catalog.inc` is adapted from the Redump Dreamcast DAT distributed by Libretro and the filenames in the Libretro Dreamcast thumbnails project. K-UI joins disc product numbers and regions to existing image filenames, consolidates covers shared by multiple discs and strips hashes and unrelated metadata from the source DAT. The index uses the upstream database's Creative Commons Attribution-ShareAlike 4.0 International license, reproduced in `LICENSES/CC-BY-SA-4.0.txt`. The public artwork stays on its origin. The lookup code and generator use GPL-3.0-only.

- [Libretro Dreamcast DAT at the indexed revision](https://github.com/libretro/libretro-database/blob/969e31a324119a17add184e8b716315a74adb51b/metadat/redump/Sega%20-%20Dreamcast.dat)
- [Libretro database license](https://github.com/libretro/libretro-database/blob/969e31a324119a17add184e8b716315a74adb51b/LICENSE)
- [Libretro Dreamcast Named_Boxarts at the indexed revision](https://github.com/libretro-thumbnails/Sega_-_Dreamcast/tree/28b140d16f188301ad697e5935c6dbb412a4ceda/Named_Boxarts)

To reproduce the index on a development machine, use the linked DAT and a Git checkout containing the linked thumbnails revision. The generator checks the DAT blob and the artwork tree before emitting C. It only needs the artwork filenames, so a filtered checkout is sufficient.

```sh
python3 tools/build_internet_cover_catalog.py "/path/to/Sega - Dreamcast.dat" /path/to/Sega_-_Dreamcast
```

The opt-in live check resolves real disc identifiers with the production C code, retrieves the two pinned PNGs over HTTPS on the host, verifies their original Git blob IDs and decodes them with K-UI's image decoder. With `--card`, it passes the public Crazy Taxi response bytes through the production Games media worker on disposable FAT32 and exFAT image files. The check reads back all three generated thumbnail sizes and compares their combined pixel digest with an independently calculated reference. Its fixture isolates the card/storage check from the console socket implementation.

```sh
make build/bba-media-integration build/games-covers-image
python3 tests/test_internet_cover_live.py --card
```
