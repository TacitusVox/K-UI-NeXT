# WinCE adaptive slow window (generic `ce-boot.kui`, 21 Nov 2026)

**Status: console-tested 21 Nov 2026 (seven launches, results below).** One behaviour change
to the generic Windows CE module's background reader, measured on the timing
probe so its rings compare with the two earlier sessions
([timing session 1](wince-timing-session-1.md) at 64 receptions,
[slow-window test](wince-slow-window-test.md) at 16).

Test build: `871c4c7784b4` (`KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2`),
93,096 bytes, payload CRC `C3140C14`, SHA-256 `b5932b2a83bd00472345349d162d1e6fa3866126250d49a5ec9130b29463548e`.

## Why

The 16-reception window fixed Super Producers' movie. On the everyday
candidate `41e3450bc94d` Armada's intro echo seemed to come back (CD music
Off); that launch in fact ran Armada's legacy `ce-probe.kui`, not the
candidate (correction below), so the 16-reception window has no Armada result.
The case that remains: Super Producers' echo on the candidate and Worms'
slight audio lag, and the rings showing overruns clustering. Just after one,
the bus is often still held; a short window can end inside such a hold, the
fast clock then overruns again, and each overrun costs a fresh read of its
block.

## The change

`KUI_SCI_STREAM_SLOW_ADAPT` (`src/loader/sci_stream.h`, Makefile.dc
`KUI_CE_SLOW_ADAPT`, 0 by default):

- An overrun at 12.5 MHz starts a window of `16 << level` receptions at
  6.25 MHz (16, 32 or 64; `KUI_CE_SLOW_ADAPT=2`).
- If fewer than 8 DMA receptions at 12.5 MHz (`KUI_SCI_STREAM_SLOW_QUICK`)
  completed since the last window ended, the bus was still held: the level
  rises by one, up to 64 receptions. Otherwise it falls by one, down to 16.
- An overrun at 6.25 MHz still ends the window at once and the reader's
  programmed refetch follows; the count of fast receptions then runs from
  that point, so an overrun soon after it also raises the level.
- Programmed receptions cannot overrun and are not counted. Opening the
  stream starts again at 16.
- One more byte of stream state (the struct grows by a word in this build;
  only the same module reads it).

Unchanged: commands and token searches at 12.5 MHz, every block's CRC check,
refusal of a block that lost a byte, the retry and programmed fallbacks.

## Build and gates

```
make -f Makefile.dc build/retail-ce/entry.elf BUILD_ID=<12 hex> \
    KUI_RETAIL_CE_ASYNC_SLOTS=96 KUI_RETAIL_CE_MAP_RECORD=1 \
    KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2
```

- Linked: the background resident ends at `0x8c00ddc0`, the probe layout's
  limit (the guarded stack's bottom; `0x8c00dd60` with the fixed window), so
  the probe has no room left; call-graph stack worst case 392 of 528 bytes,
  unchanged. In the linked `slow_start` the quick test is bit 3 of the count,
  the doubling stops at level 2 and the window stored is `16 << level`.
- `tests/test_wce_probe_admission.py` admits the exact module bytes in the
  GUI's seven production cases on FAT32 and exFAT.
- From the same commit with the switch at 0, the generic module's six
  delivered configurations rebuild byte for byte (installed `7b216f2e1136`,
  data probe `88ff969d505c`, PVR pieces `a660354d804c`, timing probe
  `5c1cdd89387b`, slow-window test `9313dad06594`, candidate `41e3450bc94d`),
  and the four CD-music modules and `ce-armada-all-on.kui` their installed
  `7b216f2e1136` bytes.
- The same policy without the probe and with the PVR pieces (a later
  everyday build, not delivered): the background resident ends at
  `0x8c00d740` (limit `0x8c00d800`), stack worst case 348 of 2,000 bytes.

## Host tests

`build/test-sci-stream-ce-adapt` (16, two doublings, quick 8),
`build/test-sci-stream-ce-adapt-shape` (12, three doublings, quick 3) and
`build/test-retail-async-ce-adapt` (the CE background reader with the
adaptive window), in `make test-ce-slow-after-overrun`:

- the first overrun after the stream opens starts the shortest window, however
  soon it comes;
- quick − 1 fast receptions after a window: the next doubles, up to the top,
  which holds; quick of them: it halves, down to the shortest, which holds;
- programmed receptions do not count; an overrun at 6.25 MHz cuts the window,
  the programmed refetch follows, and an overrun right after it doubles the
  next window; reopening the stream starts at the shortest window;
- the existing slow-window test and the CE reader's suite pass unchanged.

Nine mutations of the policy (no initial count, the boundary counted as
quick, no top, a reset instead of halving, no restart of the count, slow or
programmed receptions counted, fast ones not counted, a fixed window) each
fail the test in both shapes.

## Console test

`KUI-WinCE-Adaptive-Window-Test-20NOV2026-2111-CST-871c4c7.zip` (SHA-256
`bb31ca45ae44966a9c99c521233cfdfd2809590fca89d3add08816f7af12b62a`): the
module as `KUI/apps/games/ce-boot.kui`, a blank 64-slot
`KUI/WCEDATA/WCEDATA.BIN` (file id `1333bb630d269579d578c20d04aa86f3`), the
installed `7b216f2e1136` in `IF-NEEDED/`, and the PC tools. It has no PVR
pieces, so KISS and Virtua Cop 2 are not on its list.

Run list (CD music Off, Virtual VMU Off, no VMU, cold boot), each ending with
A+B+X+Y+Start: Armada US intro movie (the echo) and a minute of its gameplay
(the lag), Super Producers' movie to its end, Worms Armageddon's intro movies,
Super Runabout (the heaviest overruns seen), and if there is time Maximum
Pool and RE2.

The decision: whether one generic window policy keeps the 16-reception gains
(Super Producers, Armada's gameplay) and the 64-reception cure (Armada's intro
without echo). The owner's report decides; the rings show which windows ran.
With no counter of the level, the slow blocks per counted overrun read it:
about 16 at the shortest window, up to 64 at the longest (less where an
overrun at 6.25 MHz cuts one short).

## Scope

- The CD-music and Armada VMU modules keep their fixed 64-reception window.

## Result (21 Nov 2026)

Seven complete slots, every stage report naming `871c4c7784b4`, CD music Off,
Virtual VMU Off, no VMU. Armada's first launch ended on the old text screen,
not the probe's colours: with CD music Off the GUI sends Armada to its legacy
module `ce-probe.kui` whenever that is installed ([routes](wince-gui-routing.md));
nothing was saved. With `ce-probe.kui` moved off the card, Armada ran this
module (slots 0 and 1). The owner:

| Game | Result |
|---|---|
| Armada (US) intro | "may have had some incredibly small echo with the intro video but it was largely unnoticeable" |
| Armada (US) gameplay | "Gameplay worked great!" |
| Super Producers (JPN) | "audio was better, still heavily lagged at the end" |
| Worms Armageddon (US) | fine |
| Super Runabout (US) | fine |
| Maximum Pool (US) | "trash" |
| Resident Evil 2 (US) | "stuttered during the first video" |

The streamed 64 KiB reads (`DMAREAD` and `DMAREAD_STREAM_EX` of 32 sectors;
clean, such a read takes 52.3 ms), with the two earlier sessions. "Flat out"
counts the next sequential request coming one frame after a read ends (the
game polls on the 16.64 ms frame grid), against later:

| Game, window | Reads | Median ms | 90% ms | Counted overruns per 1,000 blocks | Blocks at 6.25 MHz | Flat out / later |
|---|---|---|---|---|---|---|
| Armada intro, adaptive | 66 | 89.7 | 107.1 | 50.2 | 60% | 37 / 0 |
| Armada gameplay, adaptive | 44 | 86.9 | 105.0 | 51.2 | 56% | 21 / 0 |
| Super Producers, 64 | 189 | 74.1 | 82.1 | 13.0 | 41% | 185 / 2 |
| Super Producers, 16 | 189 | 64.8 | 76.1 | 22.3 | 21% | 129 / 58 |
| Super Producers, adaptive | 189 | 64.8 | 78.5 | 19.7 | 25% | 140 / 47 |
| Super Runabout, 16 | 75 | 72.4 | 85.0 | 44.4 | 24% | 8 / 66 |
| Super Runabout, adaptive | 75 | 97.2 | 111.7 | 55.3 | 66% | 14 / 60 |
| Maximum Pool, 64 | 163 | 91.3 | 101.8 | 32.5 | 65% | 157 / 3 |
| Maximum Pool, 16 | 142 | 76.8 | 87.1 | 41.2 | 38% | 139 / 0 |
| Maximum Pool, adaptive | 163 | 81.7 | 97.0 | 37.6 | 49% | 159 / 1 |
| RE2, 64 (run 1) | 185 | 89.6 | 104.4 | 30.8 | 61% | 60 / 119 |
| RE2, 16 (run 1) | 180 | 74.4 | 81.3 | 36.0 | 29% | 73 / 96 |
| RE2, adaptive | 186 | 72.9 | 86.3 | 31.0 | 30% | 87 / 95 |

(Worms' ring holds four such reads; its intro reads 8 and 16 sectors at a time,
508 KiB/s over 11.2 s, 140 of 141 sequential requests flat out.)

1. **The window follows the contention.** Super Producers and RE2 ran near the
   shortest window, as at a fixed 16; Super Runabout, with the most counted
   overruns of any game (73 per 1,000 blocks over its whole ring), near the
   longest, so its reads were a third slower than at 16, with the game's
   slack to spare (60 of 74 requests later than a frame); Maximum Pool between,
   its reads 6% slower than at 16 and 11% faster than at 64. No read failed.
2. **Armada on the generic reader works.** Its intro is the heaviest stream on
   this reader: 190 reads in 16.1 s, 521 KiB/s, the reader open 72% of the
   time, 7.4 counted overruns per 64 KiB read, every sequential request flat
   out; clean reads would take 60% of the time. The owner heard at most a
   trace of echo, against the bad echo of `ce-probe.kui`. Its gameplay is light
   (169 KiB/s, the reader open 17% of the time). There is no ring of Armada at
   a fixed 16 or 64 on this reader, so this does not show that the adaptive
   window is what it needs.
3. **Read-limited streams.** Armada's intro, Maximum Pool's animations, Worms'
   intro and, partly, Super Producers' movie ask for the next sectors one
   frame after each read ends: 151 of 151, 162 of 163, 140 of 141 and 139 of
   185 sequential requests. Each flat-out read of about 67 to 82 ms is followed
   by a gap of about 24 ms in which the card waits. Reading on into that gap
   would add up to 29% (Maximum Pool) to 34% (Super Producers); the longest
   such gap needs 24 to 31 KiB of buffer.
4. **Super Producers' end.** In the last 16 seconds before the combination its
   reads ran as in the rest of the movie: 544 to 736 KiB/s per two seconds,
   about seven in ten sequential requests flat out, as in the 16-reception
   session. The lag the owner saw at the end is not a read slowdown in the
   ring; the movie is read-limited much of the time throughout.
5. **Maximum Pool** is read-limited with every window (flat out 157 of 160,
   139 of 139, 159 of 160). RE2's stutters, seen on 20 and 21 Nov, remain; its
   reads match the 16-reception session.

## Not answered

- Whether a fixed 16-reception window would also serve Armada on this
  reader; only an Armada launch on such a build (with `ce-probe.kui` aside)
  answers it.
- Where a read-ahead buffer can live: the top of RAM above CE3's map
  (`0x8cfd0000`) is in use in every launch from `0x8cfe0000` up, and in Super
  Producers (a CE2 kernel) from `0x8cfd0000`; the CD-music Armada module's
  stage reserves RAM outside CE's page allocator, the route a buffer would
  take.
- Armada with CD music Off keeps `ce-probe.kui` when installed. None of the
  deliveries since the slow refetch (8 Nov) carried it, so the installed one
  has no slow window; it gives Armada its virtual VMU with CD music Off.
