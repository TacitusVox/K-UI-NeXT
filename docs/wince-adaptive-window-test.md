# WinCE adaptive slow window (generic `ce-boot.kui`, 21 Nov 2026)

**Status: host-tested and linked, not console-tested.** One behaviour change
to the generic Windows CE module's background reader, measured on the timing
probe so its rings compare with the two earlier sessions
([timing session 1](wince-timing-session-1.md) at 64 receptions,
[slow-window test](wince-slow-window-test.md) at 16).

Test build: `871c4c7784b4` (`KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2`),
93,096 bytes, payload CRC `C3140C14`, SHA-256 `b5932b2a83bd00472345349d162d1e6fa3866126250d49a5ec9130b29463548e`.

## Why

The 16-reception window fixed Super Producers' movie and, on the everyday
candidate `41e3450bc94d`, removed Armada's gameplay lag, but brought back
Armada's intro echo (CD music Off). The rings show overruns clustering: just
after one, the bus is often still held. A short window can end inside such a
hold; the fast clock then overruns again, and each overrun costs a fresh read
of its block.

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

## Not answered

- The CD-music and Armada VMU modules keep their fixed 64-reception window.
- How the 8-reception threshold suits each game: the rings will show where
  the level settles, not why.
