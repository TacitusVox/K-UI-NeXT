# WinCE data session 1 (19 Nov 2026): results

The owner ran the data probe ([wince-data-probe.md](wince-data-probe.md)) on
17 Windows CE games: build `88ff969d505c` from
`KUI-WinCE-Data-Session-18NOV2026-1406-CST-88ff969.zip`, CD music Off,
Virtual VMU Off, a memory card in A1 and in A2, cold boot from the K-UI CD.
The probe build reads exactly as the installed `ce-boot.kui` `7b216f2e1136`
does. The owner then ran the same spots on `7b216f2e1136` itself ("about the
same"), so the probe is not the cause of what follows.

All 17 slots hold a complete dump. This record keeps only derived values: no
game, kernel or disc bytes, and no saves. Decoded with the corrected miner
(`tools/wce_probe_mine.py` as of commit `7cf5f9c1`; the later timing
additions leave these results unchanged).

## Per game

| Game | End | Kernel | CD line | VMU family | Card slots addressed | Block write seen | Free at the end | Owner |
|---|---|---|---|---|---|---|---|---|
| Bang! Gunship Elite (US) | combination | NK-CE3-B | 1 | 1 | A1, A2 | A1 | 5404 KiB | |
| Bust-A-Move 4 (US) | combination | NK-CE3-A | 2 | 6 | A1 | - | 11004 KiB | |
| KISS Psycho Circus (US) | reader fault | NK-CE3-B | 3 | 4 | - | - | 3688 KiB | failed right after boot |
| Kitahei Gold (JPN) | combination | NK-CE2-B | 6 | 8 | A1, A2 | A1 | 4908 KiB | |
| Maximum Pool (US) | combination | NK-CE3-B | 3 | 4 | A1 | - | 12216 KiB | laggy openings and menus; saved |
| NFL QB Club 2000 (US) | combination | NK-CE2-A | 4 | 11 | A1, A2 | - | 3796 KiB | |
| Nightmare Creatures II (US) | combination | NK-CE3-B | 3 | 4 | A1, A2 | - | 5864 KiB | |
| Railroad Tycoon II (US) | combination | NK-CE3-B | 1 | 2 | A1 | - | 5760 KiB | |
| Resident Evil 2 (US) | combination | NK-CE3-A | 2 | 6 | A1, A2 | - | 8600 KiB | intro movie halts |
| Sega Rally 2 (JPN) | combination | NK-CE2-C | 5 | 12 | A1, A2 | - | 8488 KiB | |
| Sega Rally 2 (US) | combination | NK-CE2-A | 4 | 9 | A1, A2 | - | 8464 KiB | |
| Sno-Cross (US) | combination | NK-CE3-B | 1 | 2 | A2 | - | 11612 KiB | movie hangs |
| Super Producers (JPN) | combination | NK-CE2-B | 6 | 10 | A1 | - | 5860 KiB | movie hangs, slightly |
| Super Runabout (US) | combination | NK-CE3-A | 2 | 6 | A1, A2 | - | 12048 KiB | |
| Urban Chaos (US) | combination | NK-CE3-B | 1 | 3 | - | - | 3508 KiB | car lurches while the card is idle |
| Virtua Cop 2 (JPN) | reader fault | NK-CE3-A | 2 | 5 | A1, A2 | - | 3896 KiB | crashed after launch |
| Wild Metal (US) | combination | NK-CE3-A | 2 | 7 | A1, A2 | - | 3656 KiB | long load after Start (it loads) |

The Maple list is sampled only during GD calls: a slot listed is evidence, a
dash is not proof of absence. Several games need over 100 blocks, so the
owner saved only where a 40-block card allowed; Maximum Pool saved, and no
block write was sampled. Completion frames (command 13) follow reads as well
as writes on CE and are not counted as saves.

## What the data proves

**KISS and Virtua Cop 2 stop on one refusal.** Both ended on the reader fault
`GD FUNCTION UNSUPPORTED` in `DMA_TRANSFER`: the GD driver sent part of a
`DMAREAD_STREAM_EX` straight into PowerVR video memory (KISS `0x044C0360`,
`0x4800` bytes; Virtua Cop 2 `0x04575600`, `0x80000` bytes), between RAM
pieces of 32 and 2,016 bytes, and the CE reader admits only main RAM. The
other 15 games sent no such transfer in the part played. The fix is a
separate test build: [wince-vram-stream.md](wince-vram-stream.md).

**No reader fault, refused command or stuck read in the other 15.** Every
one ended on the combination with no refused request and nothing pending.
Wild Metal's end state shows no unfinished command; the owner confirms it
loads after a long wait.

**Card and processes.** Every game runs `NK.EXE`, `filesys.exe`,
`gwes.exe` and `wdevice.exe` plus its own executable; `maple.dll` comes from
ROM in all 17, `mapledev.dll` from the disc in Bust-A-Move 4, Kitahei Gold,
both Sega Rally 2 and Super Producers. DirectSound is loaded from the disc in
those five; Maximum Pool, Resident Evil 2, Super Runabout and Wild Metal had
no `dsound.dll` loaded at the end.

## What it does not answer

The movie hangs (Resident Evil 2, Sno-Cross, Super Producers), Maximum Pool's
lag and Urban Chaos's lurching. The counters cover whole sessions without
timestamps, so they cannot place a hang against a read. What they show:

- Resident Evil 2 read almost entirely in the background (49,765 card
  blocks by the reader's interrupt, 706 inside GD calls), with almost no
  read-ahead between requests (30 KB prefetched in the session).
- Sno-Cross and Super Producers moved 7.2 MB and 3.8 MB through PIO stream
  transfers that the reader fills inside the game's GD call; so did
  Bust-A-Move 4, QB Club 2000 and Kitahei Gold, with no complaint.
- From the fourth launch on, every game had SCI overruns (0.2-6% of its card
  blocks), after each of which the reader takes the next 64 blocks at the
  slower clock: 3-70% of the blocks arrived at 6.25 MHz (Maximum Pool 29%,
  Resident Evil 2 35%, Urban Chaos 31%, Super Runabout 70%). The first three
  launches had no overrun at all; nothing in the data explains why.
- The owner's description of the movies (the audio sometimes moves on,
  sometimes repeats until the reader catches up; the movie never dies; the
  same spot each time) fits data arriving late rather than a fault.

The probe's own work is estimated (not measured) at well under 1% of the CPU,
and the owner's control run on `7b216f2e1136` behaved the same.

The timing session ([wince-data-probe.md](wince-data-probe.md#timing-session-kui_wce_probe2),
`KUI_WCE_PROBE=2`) records, for the latest 190 read requests before the
combination, when each began and ended (to TMU0's tick, not CE's 25 ms
steps), the GD calls and the reader's time in them and in its interrupt
meanwhile, and the SCI overruns and slower-clock blocks meanwhile.

The dumps also show what the timing ring's area (`0x8c00e000`, 8 KiB:
IP.BIN's bootstrap 2) holds once CE runs: identical bytes in 13 of the 17
games (two variants of the first page). Both NK-CE2-B games, Kitahei Gold and
Super Producers, held the reader hook's own save frame at `0x8c00f3f0`
(return address `0x8c0120f2`, SR `0x700000f0`) from a GD call their kernel's
boot code makes on a stack at `0x8c00f3f8`, with bootstrap 2's VBR, before
the MMU is on. Sega Rally 2 (JPN), Super Producers and Virtua Cop 2 held
fuller IP.BIN code. The probe takes the ring only at a GD call made with the
MMU on and with neither VBR nor the calling stack in the area.
