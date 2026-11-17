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
| Stage 1 runs with the caches on | `ca47ca3e` | IP.BIN turns both caches off before it starts 1ST_READ.BIN; stage 1 now turns them on (write-through) so its code and CRC checks do not run uncached. |
| Each GD-ROM command bounded to 10 s | `cfc15175` | A read that never finishes is aborted and the next copy is tried, instead of hanging. |
| Builder refuses discs that verify but cannot boot or protect | `8942d7c4` | ELF inputs, unapplied sort files, adjacent copies and mkdcdisc warnings stop the build. |
| Damaged chunks recovered one copy at a time | `9431bbd6` | A damaged copy costs one failed read, not one per chunk; rebuilds no longer seek between copies for every sector. |
| Missing sectors retried; drive set up again after "not ready" | `7535903a` | A sector that fails transiently on every copy is read on a later round instead of sending the console to the BIOS menu. |
| BIOS GD functions initialised once | `5eece4b7` | Setting the drive up again repeats only what KOS's `cdrom_reinit` does. |
| Builder sees coloured mkdcdisc warnings | `7ee9ade0` | Builder only: mkdcdisc runs with `NO_COLOR`; disc images are unchanged. |

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

- **IP.BIN and a 3 KB stage 1** are all the BIOS reads (1ST_READ.BIN). They
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
  start. Each chunk is read whole and checked against the CRC-32 table that
  [tools/boot_disc.py](../tools/boot_disc.py) wrote into stage 1. Reading starts
  at the nearest copy and stays on whichever copy last delivered a good chunk,
  so a damaged copy costs one failed read and one seek.
- **Rebuild.** A chunk no copy delivers intact is rebuilt from the copies in
  turn: each supplies, in disc order, the sectors still missing. A damaged
  sector fails the drive's own EDC check and reads as an error, so a sector
  that reads is normally right. Missing sectors get up to four rounds over all
  copies (five attempts per copy). Only if the rebuilt chunk still fails its
  CRC (silent damage) does each sector take the version two copies agree on.
  The whole image is then checked again.
- **Caches.** IP.BIN's bootstrap turns both caches off (CCR=0x808) just before it
  starts 1ST_READ.BIN. Stage 1 turns them back on with the value IP.BIN itself
  writes first (0x090b: invalidated, write-through), so no line is ever dirty.
  The bootstrap is written and checked only through its uncached alias.
- **No hang on a stuck read.** Each GD-ROM command gets 10 s, timed with the
  real-time clock. On expiry stage 1 aborts it as KOS's `cdrom_abort_cmd` does;
  if the BIOS does not release it within 2 s, stage 1 resets the GD functions
  and sets the drive up again. The read counts as failed, so the next copy is
  tried.
- **Drive errors.** After "not ready" or "disc changed" the drive is set up
  again before the next read, as KOS's iso9660 driver does: CMD_INIT, the
  drive check and the sector mode, without initialising the BIOS GD functions
  again. Setup keeps asking for up to 10 s while the drive reports either.
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
and the boot badge. It refuses:

- ELF or empty inputs, and a stage 1 over its 32 KiB limit;
- any mkdcdisc warning or error (an unmatched sort-file entry leaves the copies
  side by side);
- a first copy that does not directly follow 1ST_READ.BIN;
- copies out of order, or closer than their own length plus the spacer;
- spacers under 1 MiB. The 156 MB image is almost all zero padding; zipped, it is
about 1.7 MB. The previous single-file layout remains buildable with mkdcdisc as before.

## Verification

- **Console, November 17, 2026:** the burned `bb3a7e1` redundant disc stayed on
  the SEGA screen. Stage 1 called the BIOS GD functions without KOS's G1 bus
  reactivation; `0e4ab5a5` adds it. The owner then reported of the `0e4ab5a`
  disc: "It worked that time." This build also has the owner's pixel-art splash
  (`122578e`) and the slim countdown bar (`bb3a7e1`).

- `tests/test_boot_stage1.c` (in `make test`): the real stage-1 C code against
  a three-copy disc model, with injected read errors, transient failures and
  silent damage. It counts reads and seeks between copies. It covers:
  - nearest-copy reads, fallback, and staying on the copy that works;
  - a ring scratch over one copy (one failed read in total) and damage through
    all three copies of every chunk (311 reads and 79 seeks, previously 766 and
    about 765);
  - sectors that fail two, four or five times on every copy;
  - the majority vote, including a re-read that fails everywhere;
  - unrecoverable and identically damaged sectors, and every table check.
- `tests/test_boot_disc.py` (`make test-boot-disc`, part of `make test`): tables
  built in Python drive the C loader, MIL-CD scrambling round-trips, the
  placeholder patch is exact, and every builder refusal above is exercised. When
  the pinned mkdcdisc and stage 1 are built, it also builds a real CDI and loads
  it from its own data track.
- Flycast 2.7 with its HLE BIOS (no BIOS files), using `dc-boot-shot`:
  - The redundant disc boots to the Bootloader v3 screen.
  - It also boots with one copy damaged, with damage spread across all copies,
    with three copies damaged at different sectors of the same chunk, and with
    every chunk of every copy damaged (a radial scratch).
  - With the same sector damaged in every copy, it returns to the BIOS.
  - The standard single-file disc and the GUI (booted directly) show the new
    art correctly.
- An independent review of stage 1 and the builder (five lenses: SH-4
  assembly, BIOS GD-ROM calls, caches and handoff, builder, failure modes)
  reported nine findings. All are fixed in the commits above, and two verifiers
  per finding found none still present in `5eece4b7`. One verifier drove the
  real `stage1.c` with a mock BIOS and clock in a scratch harness (not part of
  the repository):

  | Case | Result |
  | --- | --- |
  | Clean disc | boots in about 1 s |
  | Read wedges, abort works | next copy, boots in about 10.5 s |
  | Read wedges, abort fails | reset, drive set up again, boots in about 15.8 s |
  | Frozen clock | boots, no false timeout |
  | Clock crosses a 16-bit rollover | correct 10 s limit |
  | One sector wedged on every copy | BIOS menu after about 151 s |
  | Every command wedged | BIOS menu after about 78 min, instead of never |

  The same review found that the builder missed coloured mkdcdisc warnings
  (fixed in `7ee9ade0`).
- Limits:
  - A drive that never answers takes about 78 minutes to reach the BIOS menu;
    such a drive cannot boot anything and needs a power cycle. A drive that
    stays not ready (lid open) is asked again for up to 10 s before each read,
    and loading continues once it is ready.
  - If the drive-setup command itself wedges and its abort also fails, stage 1
    resets but does not repeat that setup command. It may then reach the BIOS
    menu instead of booting. This needs several faults at once, none observed on
    hardware; KOS's `cdrom_reinit` has the same exposure.
  - The emulator does not model SH-4 caches or real BIOS timing.
  - It cannot produce read errors, drive errors or a hung command, only silent
    data changes, so the retry, re-init and timeout paths are tested only in the
    host model (retries) or not at all (re-init, timeout).
  - Console behaviour, including the time stage 1 adds, is not yet tested.
  - The test ZIP also carries the single-file disc as a fallback.
