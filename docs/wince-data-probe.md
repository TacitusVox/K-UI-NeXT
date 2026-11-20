# WinCE data probe (test build of `ce-boot.kui`, 17 Nov 2026)

**Status: host-tested, not console-tested.** Delivered as
`KUI-WinCE-Data-Session-17NOV2026-0150-CST-19ff02f.zip`: build `19ff02faaec4`,
92,456 bytes, payload CRC `E74008FC`, SHA-256 `5c1d82902426c82f...`. Each stage
report carries its module's build ID, so the data names the build that ran. This is a measurement build for one
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
  read, block write, write complete. Block reads and writes count only for the
  storage function, so a VMU screen (LCD) write is not taken for a save. The
  list is read during GD calls, so a frame counted is evidence and a frame not
  counted is not proof of absence.
- **The game's memory.** The console's 16 MiB of RAM at the end of the game, as
  non-zero 4 KiB pages. Before Windows CE starts, the stage clears the RAM the
  launcher left from the end of the loaded kernel up to its own prefix
  (0x8ce00000), and from its BSS up to its stack reservation (0x8cfe0000). In
  those ranges the dump holds only what the kernel and game wrote; the stage
  report names them, and the miner keeps other dumped pages (the stage image
  and stack area, about 330 KiB) apart. Offline tools read the kernel's
  process, module, heap and DirectSound state from it.

It answers roadmap questions; it changes no behaviour the games depend on.

## What runs where

| Part | When | What it does |
|---|---|---|
| Stage (`src/loader/wce_probe_stage.inc`) | before Windows CE starts | Finds `0:/KUI/WCEDATA/WCEDATA.BIN` by a read-only FAT32/exFAT lookup (`src/core/wce_probe_fat.c`). Claims the first EMPTY slot. Writes the stage report and the CLAIMED header. Clears stale RAM. Hands the slot's card runs to the reader through its seed word. |
| Reader (`src/loader/wce_probe_reader.inc`) | every GD call during the game | Counts calls and commands, logs CD audio commands, and reads the Maple DMA list named by MDSTAR (uncached). Reads only. |
| Reader terminal | A+B+X+Y+Start (BIOS menu command 1), or a reader fault that stops the game | Captures registers. Checks the seed (CRC, layout, partition). Writes the operand cache back, so the dump holds what the game last wrote. Stops the card stream. Writes the terminal report, the page map and every non-zero RAM page into the claimed slot, retrying a failed write once. Writes the COMPLETE header last. Restarts into K-UI. |

A reader fault records its GD function and its message (named by the
message's length, which differs for each; `tools/wce_probe_data.py` maps it
back). Unlike the installed module, the probe build does not stop on the fault
page: it saves the fault, or shows red or yellow, and restarts.

Boot cold from the K-UI CD for the session: Restart K-UI can start a saved
bank that does not use the copied files (`AGENTS.md`).

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
| Red (10 s) | The save failed part-way. The slot keeps its stage report and the pages written; the decoder flags the dump as cut off. If K-UI does not start after a red screen, switch the console off and on. |
| Yellow (10 s) | Nothing to save: no data file or free slot at launch, the launch was not probed, or the game stopped before its first disc read (a fault that early is not recorded). |

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
| +8..+15 | terminal report: reason (and, for a fault, its GD function and message), 16 registers (SR in the hook, VBR, CCR, MMUCR, PTEH, TTB, MDSTAR, ISTNRM, video mode, Holly interrupt masks), seed, counters; page map at byte 3584 |
| +16.. | the non-zero 4 KiB pages of 0x8c000000-0x8d000000, in address order |

The formats are in `include/kui/wce_probe.h`. The delivered file has 64 slots
(1.00 GiB on the card; it compresses to about 1 MB). A slot holds one launch;
when all are used the probe turns itself off (yellow at the end). A file holds
at most 255 slots (under 4 GiB).

`tools/wce_probe_data.py read WCEDATA.BIN --json out.json --dumps DIR` decodes
every used slot and rebuilds each dump as a 16 MiB image (`-partial.ram` when
the dump was cut off; a truncated upload stops at its last whole slot). A
saturated counter reads `65535+` (16-bit) or `255+` (the 8-bit Maple counters).
`tools/wce_probe_mine.py WCEDATA.BIN --out DIR` turns the file into per-game
findings, starting with the roadmap answers: launches ended by the combination,
by a reader fault or with no end record; the card slots sent a storage frame,
read from, and saved to (a block write seen), sampled during GD calls, so
absence is not proof; completion frames (command 13) are listed apart, since
CE's driver sends them after block reads as well as writes; process and module
names, read as the 8-bit strings these kernels keep; and CE's free pages when
the dump was taken. These kernels keep no low-water mark (KData+0x364 is
0), so none is reported. The miner also records whether KData+0x334 names KData,
and takes the ROMHDR from KData+0x330 when the stage report has none (CE2).

## Timing session (`KUI_WCE_PROBE=2`)

The first session's counters cover whole launches, so they cannot place a
movie freeze or a stutter against a read. The timing build is the same probe
(same stage, claim, slot format and end-of-game dump) with these differences:

- **A request ring.** For the latest 190 read requests (`PIOREAD`,
  `DMAREAD` and both stream commands): when the REQUEST was accepted and when
  the read was seen to end, each as CE's millisecond count plus TMU0's count
  with its underflow flag; whether it ended, failed (its status was not
  COMPLETED: an error, an abort or a reset) or was replaced by the next request before
  its end was seen; the GD calls made while it was open (65535 at most), the
  TMU0 ticks inside them and the longest; the TMU0 ticks in the reader's
  interrupt meanwhile; and the SCI overruns and slower-clock blocks meanwhile.
  The header holds the requests begun, whether the newest is still open,
  TMU0's prescaler and reload, CE's count when the ring began and at the end,
  and the TMU0 ticks in every GD call and in the interrupt since the ring
  began (carried past 32 bits). The layout is `struct kui_wce_probe_timing`
  in `include/kui/wce_probe.h`.
- **Counters from the ring's start.** The reader's counters (the same
  structure as the data session's) live in the ring area, so GD calls made
  before the ring is taken (bootstrap 2's, CE's first `INIT`) are not in them.
- **No Maple sampling, AICA clock or CD audio log.** The first session
  answered the port questions. The counters' Maple and clock fields stay
  zero; CD audio commands are counted but not logged.
- **One write outside the reader:** taking the ring writes 8 KiB at
  `0x8c00e000` (below). The data build writes nothing there.

**Time sources**, both read only: CE's millisecond count (the word after
KData's reschedule flag, `kui_retail_ce_kernel[2] + 4`, as the Armada timing
builds use it), which steps once per TMU0 period (25 ms in the first
session), and TMU0 (`TCNT0`, `TCOR0`, `TCR0` with its UNF flag), which
drives CE's tick. The decoder places each begin and end at CE's count plus
TMU0's ticks since the tick CE last counted (a whole period more while an
underflow was pending), so a read's time is known to TMU0's tick (80 ns at
Pphi/4), not to 25 ms. GD calls run with interrupts masked (IMASK 15) and
the reader's interrupt with SR.BL set, so neither splits the other's update
and a pending underflow stays visible: a call or an interrupt is timed
correctly while at most one TMU0 underflow falls in it (so up to two periods
when none was pending at its start).

**The ring's area.** The ring is 8 KiB at `0x8c00e000`: IP.BIN's bootstrap
2, its boot VBR (`0x8c00f400`) and, on the NK-CE2-B kernels, the stack of a
GD call made before the MMU is on (`0x8c00f3f8`). The reader takes the area,
with one 8 KiB write, at the first GD call of a session the stage prepared (a
data file with a free slot) that CE makes with its MMU on (MMUCR.AT), its
own VBR and a stack outside the area; never from its interrupt, and never in
a launch with no slot. In the first session's dumps 13 of the 17 games held
identical bytes there (two variants of the first page); Sega Rally 2 (JPN),
Super Producers and Virtua Cop 2 held fuller IP.BIN code. That suggests
nothing writes the area once CE runs, which is an inference: a dump shows the
final bytes only. Both NK-CE2-B games, Kitahei Gold and Super Producers, also
held the reader hook's own save frame at `0x8c00f3f0` from a GD call on that
boot stack (SR `0x700000f0`: BL set, before the MMU is on). MMUCR.AT alone
does not exclude that call: the probe restarts the console with AT still set,
and whether the boot ROM clears it is not established; hence the VBR and
stack checks.

The end-of-game dump saves the ring with the rest of RAM (an all-zero ring
page is skipped like any other).
`tools/wce_probe_data.py` decodes it as each session's `timing`, but only
when its pages were written whole: the dump writes pages in ascending order,
so a complete dump or a later page read back present proves it; a dump cut
off inside the ring gives an error instead of reads that are not there.
`tools/wce_probe_mine.py` reports the window covered, read rates and
latencies, the reader's time in GD calls and in its interrupt (while a kept
read was open, and since the ring began), the slowest reads, the longest gaps
between reads, and up to the last 30 whole seconds the kept reads cover,
second by second (the second holding the oldest kept read is partial once the
ring has dropped older ones, so it is left out). The end itself has only
CE's count, so the gap from the last read to the end is given to CE's 25 ms
tick.

The stage report's flags carry bit 1 for a timing build. The ring keeps the
end of each launch: the owner presses the combination soon after the moment
to measure. At the first session's average read rates (reads over CE's
uptime) 190 reads cover about 14 s of Railroad Tycoon II, the busiest
reader, 46 to 121 s of the games on the timing list, and up to 5 minutes of
the lightest; bursts such as a movie's shorten that.

The timing build's background resident ends at `0x8c00dd60` (the probe
limit is `0x8c00ddc0`); its call-graph stack worst case is 392 of 528 bytes.
The end-to-end host test runs it with the data build: the ring's take
conditions; an underflow during, and pending at the end of, a call; failed,
aborted and reset-ended reads; a ring that wraps and is decoded by
`tools/wce_probe_data.py` with its saturated sectors, calls and latency,
replaced reads and both carried totals; and a dump cut inside the ring.
`tests/test_wce_probe_mine.py` checks the miner's gap to the end, its whole
seconds and its per-second sums. Each of these checks fails when the code it
covers is removed or altered.

Delivered for the second data session: `5c1cdd89387b`
(`KUI-WinCE-Timing-Session-20NOV2026-0158-CST-5c1cdd8.zip`, see
[accepted progress](accepted-progress.md)).

## Build and gates

```
make -f Makefile.dc build/retail-ce/entry.elf BUILD_ID=<12 hex> \
    KUI_RETAIL_CE_ASYNC_SLOTS=96 KUI_RETAIL_CE_MAP_RECORD=1 KUI_WCE_PROBE=1
```

then `envelope(flatten_elf(entry.elf), build_id)` from `tools/runtime_package.py`,
as for the installed module. The built-in gates run: relaxed calls, retail
instruction audit, `check_retail_loader_layout.py --ce` (the probe resident ends
by 0x8c00ddc0 with a 576-byte guarded stack, its own linker script
`retail_resident_ce_async_probe.ld`; `check_retail_stack.py` counts the calls
into IPA-split clones for the probe: worst case 392 of 528 bytes), and the
build record.

- `KUI_WCE_PROBE=0` (the default) builds the generic `ce-boot.kui`; the
  probe adds nothing to any other module. `KUI_WCE_PROBE=1` is the data
  session's build and `2` the timing session's; both keep the reader the
  first session measured (`KUI_CE_VRAM_DMA=0`, [wince-vram-stream.md](wince-vram-stream.md)).
- The probe's resident adds a counters block, the seed and the terminal writer.
  It uses the same SCI bus and card descriptor as the reader. The bus's
  byte-work clock (`loan_ticks`) is linked so the SD write path has its timeouts.
  The reader's own stream never calls it.

## Host tests

| Test | What it proves |
|---|---|
| `tests/test_wce_probe_fat.py` | FAT32 and exFAT lookup, fragmented files, cluster limits, and volumes it must not trust (inactive FAT, short ValidDataLength, looping chains, a wrapped FAT size) |
| `tests/test_wce_probe_end_to_end.py` | the stage claim, reader sessions and end-of-game dumps on a FatFs card image (FAT32/exFAT, contiguous and fragmented): green with a retried write, red, yellow, damaged seeds, an interrupted claim, a reader fault, a dump cut off part-way, a VMU-screen write that is not a save, the claim's refusals (full, bad file header, damaged slot header, non-blank slot), the decoder's rebuild of every page and the miner |
| `tests/test_wce_probe_mine.py` | the miner on an authored RAM image: KData, CE2/CE3 process arrays, the module list (ROM and disc DLLs, page walk), users, memory counters, refusals, the library join and the report |
| `tests/test_wce_probe_admission.py MODULE` | the GUI's production admission (`build/games-retail`) of the actual module bytes: 96-slot record, background and Standard readers, refusals |

## Not answered by this build

- Any behaviour with CD music On, the virtual VMU, or Armada's dedicated modules.
- Absence of a Maple frame: frames are sampled during GD calls only.
- Games that never reach the BIOS menu call (they still record a stage report).
- Timing: the probe does not measure launch phases.
- Anything that needs a changed GUI: launch timing for CD-music-Off launches, CHD images. (The GUI's `last-launch-a/b.txt` names the module path, build and CRC of the last two launches.)
