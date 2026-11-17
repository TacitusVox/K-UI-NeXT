# WinCE data probe (test build of `ce-boot.kui`, 17 Nov 2026)

**Status: host-tested, not console-tested.** Delivered as
`KUI-WinCE-Data-Session-17NOV2026-0021-CST-a257f13.zip`: build `a257f13ec7d1`,
92,336 bytes, payload CRC `6714546A`, SHA-256 `36a0be57bfdf28a9...`. This is a measurement build for one
owner data session, not a candidate for acceptance. The owner swaps it in for
the installed `KUI/apps/games/ce-boot.kui` (`7b216f2e1136`, payload CRC
`04D83892`, delivered 9 Nov, console report pending) and restores that file
afterwards. No GUI change.

**Purpose.** Collect, in one bulk session, the per-game facts that the WinCE
roadmap ([scope](wince-universal-scope.md) section 4, [roadmap](wince-roadmap.md))
needs before any feature build:

- **Boot evidence.** Which CD-driver lines and kernels get from the K-UI stage
  into a running game on the generic reader. Each launch records its stage
  report; a game that reaches the end-of-game combination also records its
  counters.
- **Disc use.** GD calls by function and request command, read volume and
  largest read, refused commands (for example 26, open tray), drive-status
  polling, and every CD audio command (PLAY, PLAY2, PAUSE, RELEASE, STOP) with
  its track range and repeat count. With CD music Off these are acknowledged
  silently; the log shows what the game asked for.
- **Controller ports.** Which port and slot each Maple frame addresses (A main,
  A1, A2, ... D2), by command kind: device info, condition, media info, block
  read, block write, write complete. Block writes to a storage unit show that the
  game saves and to which slot.
- **The game's memory.** The console's 16 MiB of RAM at the end of the game, as
  non-zero 4 KiB pages. The stage clears the RAM the launcher left behind, so the
  dump holds only what the kernel and game wrote. Offline tools read the kernel's
  process, module, heap and DirectSound state from it.

It answers roadmap questions; it changes no behaviour the games depend on.

## What runs where

| Part | When | What it does |
|---|---|---|
| Stage (`src/loader/wce_probe_stage.inc`) | before Windows CE starts | Finds `0:/KUI/WCEDATA/WCEDATA.BIN` by a read-only FAT32/exFAT lookup (`src/core/wce_probe_fat.c`). Claims the first EMPTY slot. Writes the stage report and the CLAIMED header. Clears stale RAM. Hands the slot's card runs to the reader through its seed word. |
| Reader (`src/loader/wce_probe_reader.inc`) | every GD call during the game | Counts calls and commands, logs CD audio commands, and every fourth call reads the Maple DMA list named by MDSTAR. Reads only. |
| Reader terminal | A+B+X+Y+Start (BIOS menu command 1), or a reader fault that stops the game | Captures registers. Stops the card stream. Writes the terminal report, the page map and every non-zero RAM page into the claimed slot. Writes the COMPLETE header last. Restarts into K-UI. |

The probe arms only on the background reader. The GUI routes every WinCE launch
with CD music Off to `ce-boot.kui` with that reader (`src/core/shell.c`
`games_launch_choice`). With CD music On the GUI uses a `ce-cdda-*` module, and
Armada's own disc may use `ce-probe.kui` or `ce-armada-all-on.kui`; those
launches are not probed.

## Screen colours at the end of a game

| Colour | Meaning |
|---|---|
| Blue | Saving. Do not power off. A full 16 MiB dump takes up to about two minutes. |
| Green (3 s) | Saved. The console restarts into K-UI. |
| Red (10 s) | The save failed part-way. The slot keeps its stage report; the decoder labels it. |
| Yellow (10 s) | Nothing to save: no data file or free slot at launch, the launch was not probed, or the game never read the disc. |

If A+B+X+Y+Start does nothing within about 10 seconds, the game does not reach
the BIOS menu call. Power off. The slot keeps its stage report and is labelled
"claimed, no end-of-game record".

## Card safety

Every write goes into the one slot the stage claimed for this launch:

- **File.** The lookup follows the FAT32 or exFAT directory and cluster chain of
  `KUI/WCEDATA/WCEDATA.BIN` with bounds checks on every cluster and sector. A
  file in more than 128 runs is refused.
- **Slot.** The file header must carry the magic, version, layout, CRC and a
  size that covers every slot. A slot is claimed only if its header reads back
  as this file's exact EMPTY header for that slot, and the first sector of each
  of its runs reads back blank. Used slots must carry this file's ID and a valid
  CRC; anything else stops the probe for the launch. A slot split into more than
  8 runs is skipped.
- **Writes.** Stage writes are bounded to the partition and go only to the
  claimed slot's stage report and header. The reader receives only the slot's
  runs. It writes the terminal report and pages through those runs and then the
  slot header the stage prepared.
- **Failure.** Any failure before the claim leaves the launch exactly as the
  installed module would run it, with nothing armed.

## Data file

`tools/wce_probe_data.py blank OUT --slots N` makes an empty file. Sector 0 holds
the file header. Slots start at sector 2048, each 32,784 sectors:

| Slot sector | Content |
|---|---|
| +0 | slot header: EMPTY, CLAIMED or COMPLETE, sequence, AICA clock |
| +1..+7 | stage report: module build, IP.BIN, kernel bytes/CRC/prefix CRC, ROMHDR address and bytes, CE RAM start/free/end, card and partition, map slots, cleared range |
| +8..+15 | terminal report: reason, registers (SR, VBR, CCR, MMU, Maple, ASIC, video, timers), seed, counters; page map at byte 3584 |
| +16.. | the non-zero 4 KiB pages of 0x8c000000-0x8d000000, in address order |

The formats are in `include/kui/wce_probe.h`. The delivered file has 64 slots
(1.00 GiB on the card; it compresses to about 1 MB). A slot holds one launch;
when all are used the probe turns itself off (yellow at the end).

`tools/wce_probe_data.py read WCEDATA.BIN --json out.json --dumps DIR` decodes
every used slot and rebuilds each dump as a 16 MiB image. `tools/wce_probe_mine.py`
(see below) turns those into per-game findings.

## Build and gates

```
make -f Makefile.dc build/retail-ce/entry.elf BUILD_ID=<12 hex> \
    KUI_RETAIL_CE_ASYNC_SLOTS=96 KUI_RETAIL_CE_MAP_RECORD=1 KUI_WCE_PROBE=1
```

then `envelope(flatten_elf(entry.elf), build_id)` from `tools/runtime_package.py`,
as for the installed module. The built-in gates run: relaxed calls, retail
instruction audit, `check_retail_loader_layout.py --ce` (the probe resident ends
by 0x8c00dd80, its own linker script `retail_resident_ce_async_probe.ld`, stack
from `check_retail_stack.py`), and the build record.

- `KUI_WCE_PROBE=0` (the default) rebuilds the installed `ce-boot.kui`
  `7b216f2e1136` byte for byte. The probe adds nothing to any other module.
- The probe's resident adds a counters block, the seed and the terminal writer.
  It uses the same SCI bus and card descriptor as the reader. The bus's
  byte-work clock (`loan_ticks`) is linked so the SD write path has its timeouts.
  The reader's own stream never calls it.

## Host tests

| Test | What it proves |
|---|---|
| `tests/test_wce_probe_fat.py` | FAT32 and exFAT lookup, fragmented files, cluster limits, the slot claim and its refusals |
| `tests/test_wce_probe_end_to_end.py` | the stage claim, a reader session and the end-of-game dump on a FatFs card image (FAT32/exFAT, contiguous and fragmented), red, yellow and full-file paths, and the decoder's rebuild of every page |
| `tests/test_wce_probe_admission.py MODULE` | the GUI's production admission (`build/games-retail`) of the actual module bytes: 96-slot record, background and Standard readers, refusals |

## Not answered by this build

- Any behaviour with CD music On, the virtual VMU, or Armada's dedicated modules.
- Games that never reach the BIOS menu call (they still record a stage report).
- Timing: the probe does not measure launch phases.
- Anything that needs a changed GUI: launch timing for CD-music-Off launches, CHD images. (The GUI's `last-launch-a/b.txt` names the module path, build and CRC of the last two launches.)
