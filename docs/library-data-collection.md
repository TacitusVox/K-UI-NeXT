# Library data collection for universal compatibility

The owner has every US, PAL and JPN Dreamcast release. This plan says which
data to collect from those discs with the startup tool, `tools/kui-library-startup-bundle.py`
(v2.2), and what each item is for. Copied files stay private in the session
that analyzes them. The repository records only metadata: titles, product
codes, hashes, addresses, sizes and verdicts.

## What one run collects per disc

| Item | Used for |
| --- | --- |
| `IP.BIN` (disc header) | Identity: title, product, version, region, disc number. Its peripheral flags mark Windows CE games. |
| Boot executable named by the header (`1ST_READ.BIN` or `0WINCEOS.BIN`) | Native: SDK family; heap initializer; Maple (controller/VMU) driver family and layout; SDK SCI family for the background reader; fixed-RAM games that need RAM maps; loader-only boot files. Windows CE: the kernel and its `maple.dll`/`mapledev.dll` families. |
| Windows CE programs (`.EXE`) | The CE CD-music binder, which finds each game's DirectSound owner. |
| Small runtime and sound-driver files (for example `AUDIO64.DRV`) | Native CD music adapters, which are built around the game's own sound driver. |
| File listing (names and sizes, no contents) | Movie formats (Sofdec `.SFD`, `.ADX` audio and other streams), streaming-heavy games, programs a loader starts (`requests.json`). |
| Track layout | Real CD audio tracks, as opposed to short layout tracks. |

No audio, movies, textures or other game data are copied.

## Runs, in order of value per upload

1. **PAL and JPN Windows CE games (small).** This completes the
   [controller-driver survey](wince-vmu-driver-families.md). A PAL or JPN
   kernel whose driver matches a known family needs no new hook work.

   ```sh
   python3 kui-library-startup-bundle.py "/path/to/PAL" --output-dir ~/kui-pal-ce --windows-ce --name PAL-wince
   python3 kui-library-startup-bundle.py "/path/to/JPN" --output-dir ~/kui-jpn-ce --windows-ce --name JPN-wince
   ```

2. **US native games.** This extends the [library compatibility table](library-compat-table.md)
   from the owner's roughly 70 games to the whole US library. It shows which
   families block virtual VMU and the background reader, and how many games
   each fix would unlock. `already-sent.json` skips startups sent before.

   ```sh
   python3 kui-library-startup-bundle.py "/path/to/US" --output-dir ~/kui-us --name US-startup
   ```

3. **PAL native games, then JPN native games.** Run each the same way, with
   `--name PAL-startup` or `--name JPN-startup`. Regional builds often differ
   from the US executable. Identical startups (a second disc, or a 2048-byte
   copy of a raw GDI) are listed as duplicates and not copied again.

Each part is at most 25 MiB, and part 01 always lists every game. Earlier runs
averaged about 1.3 MB per native startup, so a 200-game run is about 11
parts. Only GDI and CUE/BIN images are read; convert CDI, CHD and other
formats first if they matter.

## Collected so far

- US Windows CE: all 36 games. See [driver families](wince-vmu-driver-families.md).
- US native: the whole US library, in 8 parts. See [US compatibility](library-compat-us.md),
  where Maple layout, Maple driver and SDK heap families are the first blocker for 91 games.
- Sports Jam, Street Fighter III: 3rd Strike and Toy Story 2 (boot programs of
  13.5 to 14.3 MB) came from a v2.3 rerun, after the per-file limit was raised
  to the main-RAM bound.
- US CD audio: 99 games with real audio tracks; native ones ship a few standard sound
  drivers (`MANATEE.DRV`, `AUDIO64.DRV`, `MANATEE2.DRV`). See [US CD audio](library-cd-audio-us.md).
- JPN: all 615 discs (run `eee9cdc7`). See [JPN compatibility](library-compat-jpn.md);
  the Maple driver family is the first blocker for 157 games. Its 45 Windows CE games
  are in the [driver families](wince-vmu-driver-families.md), and its CD audio games in
  [CD audio](library-cd-audio-us.md).
- Still to run: PAL Windows CE and PAL native startups.

## What the discs cannot tell us

- Console behavior: launches, FMV smoothness, saves and power-cycle reloads
  need console tests.
- Save compatibility details beyond what the executable shows.
- Whether a game streams CD audio during play. Audio tracks alone do not prove
  that.

## Analysis on receipt

- Windows CE: `docs/wince-vmu-driver-families.md` (method and families) and
  its survey evidence record.
- Native: `tools/library_compat_table.py` and `tools/native_library_sweep.py`
  over the received parts. Regenerate `docs/library-compat-table.md` with a
  region column.
