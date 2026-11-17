# K-UI Bootloader v3 boot disc

The owner asked on November 17, 2026 for every pending boot-disc change before
burning a fresh disc, for a version-agnostic look ("K-UI Bootloader v3", no
release name or artwork), and for a faster start of K-UI after the countdown
or **A**. This disc is the third K-UI bootloader. Nothing on it is tied to a K-UI
release: it loads whatever compatible `KUI/boot.kui` or `KUI/runtime.kui` the card holds.

## What changed against the previous disc

| Change | Commit | Effect |
| --- | --- | --- |
| Minimal retro-futuristic art (`boot-red.png`, [boot-v3.py](../resources/branding/boot-v3.py)) | `3c80b58e` | Chrome K-UI wordmark, BOOTLOADER, neon `v3`, perspective grid; no release name. |
| Art stored compressed and inflated once | `99e2bb7a` | 1ST_READ.BIN drops by 527,104 bytes (about half); the BIOS has half as much to read before the boot screen. |
| Header "K-UI Bootloader v3" with the build only | `2b6c0faa` | `KUI_BOOTLOADER_VERSION` is independent of K-UI releases. |
| Cheaper loading repaints | `887e5147` | The dimmed backdrop is built once, panels are filled two pixels per store, and busy repaints no longer idle for vertical blank. Simulated cost per repaint fell from 6.70M to 3.18M cycles. |
| No fixed pause before handing over | `c0e2a67f` | The validated image starts at once instead of after 250 ms. |
| Redundant layout (stage 1 + three checked copies) | `0da95cfc` | See below. |

The GUI itself loads faster from this disc's point of view because runtime.kui is
smaller: the compressed splash (−472,172 bytes) and launcher artwork (−326,524
bytes). The owner's earlier console measurement of this loader was 2,771 ms for a
1,546,484-byte runtime over SCIF (544.8 KiB/s including checks and drawing), so
the 798,696 bytes saved are worth roughly 1.2–1.4 s per launch at that rate. This
is an estimate from that measurement, not a new console timing; Diagnostics →
"Measure load time" reports the real figure.

## Redundant layout

A scratch or smudge over the 490 KB 1ST_READ.BIN used to make the disc
unbootable. Now:

- **IP.BIN and a 2.5 KB stage 1** are all the BIOS reads (1ST_READ.BIN). They
  remain single copies, like the volume descriptor and root directory the BIOS
  also needs, so the area that must be readable shrinks from about 245 sectors to
  two.
- **The CD bootstrap is stored three times** (`KUIBOOT1.BIN`–`KUIBOOT3.BIN`), with
  zero-filled spacers (`KUIPAD1.BIN`, `KUIPAD2.BIN`, 64 MiB each) between them. The
  first copy follows stage 1, so a normal boot does not seek. The copies sit
  about 4 mm apart radially on a CD-R, so a circular scratch or ring smudge rarely
  covers more than one.
- **Stage 1** ([src/bootstage](../src/bootstage)) relocates itself to 0x8cf80000 and
  reads the bootstrap 32 KiB at a time through the BIOS GD-ROM functions. It
  calls them the same way, and in the same order, as KOS's cdrom driver does at every program
  start. Each chunk comes from the nearest copy whose CRC-32 matches the table
  that [tools/boot_disc.py](../tools/boot_disc.py) wrote into stage 1. A chunk no
  copy delivers intact is rebuilt sector by sector, using the version two copies
  agree on, else the one copy that reads. The whole image is then checked again.
- **Handoff** follows KOS's own `arch_exec` contract. Operand-cache lines are
  written back and invalidated, caches are invalidated and disabled (KOS
  enables them again at startup), the BIOS's stack pointer and return address
  are restored, and stage 1 jumps to 0x8c010000. The bootstrap therefore starts exactly as
  if the BIOS had loaded it.
- **If no copy can supply a chunk**, stage 1 returns to the Dreamcast BIOS
  menu instead of starting anything unverified.

`tools/boot_disc.py` builds the disc in two passes with mkdcdisc (sort-file
placement). It reads the copies' positions from the real CDI, writes the table
and builds again with an identical layout. It then reads the final CDI back:
copy positions and bytes, the descrambled 1ST_READ.BIN against the patched stage 1,
and the boot badge. The 156 MB image is almost all zero padding; zipped, it is
about 1.7 MB. The previous single-file layout remains buildable with mkdcdisc as before.

## Verification

- `tests/test_boot_stage1.c` (in `make test`): the real stage-1 C code against
  a three-copy disc model, with injected read errors and silent damage. It covers
  nearest-copy chunk reads, fallback, sector-by-sector rebuild with majority vote,
  unrecoverable and identically damaged sectors, and every table check.
- `tests/test_boot_disc.py`: tables built in Python drive the C loader, MIL-CD
  scrambling round-trips, and the placeholder patch is exact. It also builds a real
  CDI with the pinned mkdcdisc and loads it from its own data track.
- Flycast 2.7 with its HLE BIOS (no BIOS files), using `dc-boot-shot`:
  - The redundant disc boots to the Bootloader v3 screen.
  - It also boots with one copy damaged, with damage spread across all copies,
    and with three copies damaged at different sectors of the same chunk.
  - With the same sector damaged in every copy, it returns to the BIOS.
  - The standard single-file disc and the GUI (booted directly) show the new
    art correctly.
- Limits:
  - The emulator does not model SH-4 caches or real BIOS timing.
  - It cannot produce read errors, only silent data changes.
  - Console behaviour, including the time stage 1 adds, is not yet tested.
  - The test ZIP also carries the single-file disc as a fallback.
