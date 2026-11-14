# Toy Commander Test 68: the continuous card stream under the movie window

Status: ran on the console on 2026-11-13 (build `806015566fc7`, owner's
report `/reports/toy/66-2.txt`). The stream and its verification work; the
first movie played through, the second fell into a slideshow two seconds in
and stayed one to its end. Not accepted. See "Console result" below; the
follow-up is Test 69 (`docs/toy-wire-stream-test-69.md`).

## Baseline and the one change

- Baseline: the Test 66 SD reader, source `3124522ea906`
  (`retail-toy-test-66.kui`, SHA-256 `06f34778...f8d2`), imported in
  `e1104f4` and reproduced byte for byte. Test 66 itself has no console
  result on record.
- Measurement the change rests on: Test 67 (`docs/toy-wire-probe-test-67.md`,
  console result in `docs/evidence/toy-wire-probe-67/`). Beside the original
  GD-ROM playing both movies at full speed, one continuous receive of the
  card's CMD18 stream moved 0.87 and 1.12 MB/s (0.94 and 1.23 outside the
  drive's own transfers) against 0.81 and 0.94 needed, for about 1.3 % of the
  CPU in its interrupt handler, with 6,004 and 27,783 receive overruns each
  costing exactly one byte.
- The one behavior change: the verified movie window's producer. Test 66
  fills the window through the low reader, one 513-byte reception per block.
  Test 68 fills it from one continuous background reception that is only
  interrupted by the receiver's own overruns. Everything the game sees is
  unchanged: whole requests served from sectors whose address and EDC
  passed, through the same publish path.
- Decision the console test answers: does the SD reader play both opening
  movies at full speed when fed this way, and if not, which stage holds it
  back (the report separates them, see below).

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66 (`cmp` in the build evidence); compiled without the new switch. |
| `native-ra-aux` (auxiliary) | New producer; two more lease pages of code; arena 0x2d000. |
| `native-worker` | Layout constants; the report's Test 68 rows. |
| stage | Layout constants only. |
| Card Tools | Not rebuilt. 09c9a4b686d4 loads the module from its Test 66 path. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1`: every mode
conditional stays at 66 and only code under `KUI_TOY_WIRE_STREAM` differs.
The lease stays 0x47000. Inside it the auxiliary span grows from 0x5000 to
0x7000 and the arena shrinks from 0x2f000 to 0x2d000.

## How it works

1. **Opening.** The low reader seeks as before: CMD18 at the window's next
   block and one block by programmed transfers, CRC-checked. A block it
   still holds from the previous request is taken without touching the card.
2. **Takeover** (`toy_wire_take_over`). With the card stopped behind a
   block's CRC, the stream selects it and starts DMA channel 1 into a 32 KiB
   ring (`kui_wire_start`). The resident's stream state is set to DMA: that
   is the one state in which the resident routes the SCI's error interrupt
   to the auxiliary and keeps the interrupt level.
3. **Reception** (`src/loader/toy_wire_stream.c`). Runs of up to 8 KiB.
   When the receiver overruns, the run is stopped, counted and handed off
   exactly as `sci_stream.c` and the Test 67 probe do it, the lost position
   is logged, and reception resumes at the next byte: at once after a run
   of 128 bytes or more, after a one-shot timer wait of 64 to 1024 ticks
   after a shorter one. The byte the receiver still held is stored; the one
   that overran is the lost one. The timer is TMU2 or TMU1, whichever the
   game leaves free, through the SDK's own interrupt table.
4. **Reading the ring** (`kui_wire_next`), in the foreground and in the
   interrupt visit, within a time budget. Token search between blocks; a
   block with no lost byte must pass its CRC16; one lost data byte is
   rebuilt from the CRC16; more are handed on with their positions. A byte
   lost between blocks is either an idle byte or the token: both readings
   are kept until the block's CRC decides.
5. **Sectors** (`toy_wire_ahead.inc`, `src/core/mode1_erasure.c`). Blocks go
   through the same cursor into the window's raw sectors. Lost positions
   are carried along; when a sector is complete they are filled from its
   own P and Q parity (each code word XORs to zero: no field arithmetic).
   Then the unchanged check: mode byte, address and EDC. Only then does the
   sector count as verified.
6. **A sector that fails** (more than 32 lost bytes, a pattern the parity
   cannot untangle, or a failed check) stops the stream. The low reader
   seeks again to that sector and reads all of it by programmed transfers,
   which cannot overrun; then the stream takes over again. Every such
   re-read gains a sector, however busy the bus is.
7. **Stopping** never sends a card command, so it is allowed in the
   interrupt: the SCI goes back to command framing, the stream state to
   LOST, and the low reader ends the CMD18 with CMD12 before its next read,
   in the foreground, as it always did.

What is deliberately not new: no register sequence (all are those of
`sci_stream.c` as the probe ran them 33,000 times on the console), no
resident byte, no change to which requests the window serves or how they
are published.

## What is known and what is not

- Known from the console (Test 67): the wire rate beside the decoder; that
  resuming in the interrupt keeps the card's framing intact over 33,000
  stops; the handler cost; TMU1 was free.
- Known from Tests 48 to 51: after an overrun exactly one byte is lost at
  the position this code computes, and reception continues correctly from
  the next one.
- Not known until this test: that the positions also hold for runs of
  thousands of bytes resumed in the interrupt (`UNFIX` in the report is the
  direct measure: a single lost byte whose block CRC cannot explain it);
  that the image's raw sectors carry real P/Q parity (`PARITY`); how the
  bus behaves without the GD-ROM drive's transfers; and whether Test 66's
  request path serves from the window on the console at all (Tests 62, 64
  and 65 served 0, 0 and 13 sectors from it).

Sizing from the Test 67 cut pattern (`docs/evidence/toy-wire-stream-68/`):
with 73 % of cuts following within 64 bytes of a resume, as measured, about
a third of all sectors arrive with lost bytes, one in ten of those is
settled by the block CRC alone, and about 0.1 % of sectors need the re-read
of step 6. That is a model of the measured histogram, not a measurement of
this reader.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 68`. Values are hexadecimal without leading zeros; a row that
does not fit the two report sectors is left out whole, last rows first.

| Row | Meaning |
|---|---|
| `PIPE B I CALLS IRQ FAIL` | Window openings per movie, foreground and interrupt visits, failed game reads |
| `AHEAD B I HIT EDC STOP` | Sectors served per movie, requests served, sectors verified, windows given up on error |
| `SD BLOCKS PIO START CRC` | Low reader: DMA blocks, programmed blocks, CMD18 starts, CRC failures |
| `W START SEG CUT WAIT FULL` | Streams begun, full runs, overruns in a run, overruns answered with a wait, stops for room |
| `W BYTES WTICK TMR ERR DET` | Wire bytes, ticks in timer waits, timer channel, first fault and detail |
| `W BLK FIX UNFIX HOLED NOCRC SEEK` | Blocks; rebuilt from CRC16; single loss the CRC16 could not explain; handed on with losses; only a CRC byte lost; blocks read by the low reader |
| `W GAPI GAPT BADCRC FRAME AMBIG` | Lost gap bytes taken as idle or as token; streams ended by a clean block failing CRC, a bad byte between blocks, or too many losses |
| `W FILL BYTES MOST REFUSE BAD` | Sectors filled from parity, bytes filled, most losses in one sector, sectors not fillable, sectors failing address or EDC |
| `W STOP CLOSE JUMP SECT FRAME FAULT` | Stream ends by reason |
| `W DROP ERR END IDLE CMD WAIT OTHER` | Windows given up by reason |
| `W IRQ TICKS MAX PARITY DECL` | Interrupt visits while streaming, their ticks in total and at most, parity consistent on a whole sector (1) or not (2), takeovers declined |
| `SUBMIT...`, `FIRST...`, `ENTER...` per movie | Test 63/66 frame counters |
| `WAIT...`, `READ...`, `I LAG LIMIT` | Test 66 first-wait rows, if room is left |

Reading it:

- Movies smooth, `SKIP` 0, exits `7DC`: the transport is the fix.
- `UNFIX` near the number of single losses, `BAD` high: lost-byte positions
  are not what Tests 48 to 51 established for long runs. The transport
  needs a different resume rule; the rest stands.
- `PARITY` 2: the image has no real P/Q bytes. Sectors with more than one
  loss per block then all take the re-read path; the follow-up is a second
  CRC16 solve per block instead of parity.
- `W DROP WAIT` or `OTHER` large and `AHEAD` small: Test 66's request path
  is turning requests away from the window, independent of the transport.
- `W FULL` large with `W IRQ MAX` large: the ring reader, not the wire, is
  the limit.
- `TMR` 0: no free timer; waits ended only at the next visit.

## Console result

Owner's run, cold CD boot, Card Tools 09c9a4b686d4, raw Toy Commander GDI,
"66 SCI movie playback", CD Music and virtual VMU off. Saved report
`docs/evidence/toy-wire-stream-68/console-report-66-2.txt` (1,024 bytes,
SHA-256 `983da9af...e92a`), headed `TEST 68`, `BUILD 806015566fc7`. The GUI
and module path are those of the package; the report does not carry the
module's payload CRC.

Owner's observation: "the first video played all the way through without
issue. Second intro video turned into a slideshow two seconds in, but it did
actually go until the end...just as a slideshow."

| | First movie (B) | Second movie (I) |
|---|---|---|
| Windows opened / sectors served from them | 10 / 3,400 | 1 / 25,756 |
| Frames submitted of the movie's | 317 of 331 | 319 of 1,968 |
| Of those on time and in sequence | 307 | 272 |
| Frames skipped | 14, from frame 11 | 1,649, from frame 23 |
| Longest gap between frames | 98 ms at frame 204 | 2.15 s at frame 121 |
| First read still pending with frames out | none | frame 116, 15,445 bytes short |
| Player exit | `7DC` | `7DC` at frame 1,968 |

Both movies together:

| What | Value |
|---|---|
| Requests served from the window | 2,279; no failed game read |
| Streams begun | 86 |
| Wire bytes | 70,692,502 |
| Full 8 KiB runs / overruns inside a run | 5,595 / 46,915 |
| Overruns answered with a timer wait | 16,454, 3.35 s in all, on TMU1 |
| Stops for room in the ring | 1,845 |
| Blocks handed on | 134,272 |
| One lost byte rebuilt from CRC16 / not explained by it | 17,140 / 11 |
| Handed on with lost bytes / only a CRC byte lost | 15,993 / 82 |
| Lost gap bytes read as idle / as token | 136 / 65 |
| Clean block failing CRC16 | 0 |
| Sectors filled from parity | 6,391 (34,526 bytes, at most 26 in one) |
| Sectors beyond the fill / failing address or EDC | 0 / 1 |
| Whole-sector parity consistent | yes |
| Streams ended: close / jump / sector / frame / fault | 11 / 0 / 1 / 74 / 0 |
| Windows given up: movie over / read waited for with interrupts off | 2 / 9 (all in the first movie) |
| Reader visits from the game / from the SCI interrupt | 156,261 / 50,812 |
| Time in the interrupt visits | 9.05 s, at most 5.29 ms in one |
| Low reader: blocks by DMA / programmed / CMD18 starts / CRC failures | 14,718 / 81 / 425 / 0 |

What the run establishes:

1. The transport and its verification hold on the console. Lost positions
   are exact for long runs resumed in the interrupt: 17,140 single losses
   rebuilt against 11 unexplained. No clean block failed its CRC16, no sector
   was beyond its parity, one failed its check and was read again, the image
   carries real P and Q bytes, and no game read failed. Test 66's request
   path serves from the window: all 25,756 sectors of the second movie.
2. The second movie starved inside the reader, not on the wire. An interrupt
   visit reads for 390 ticks, about six blocks at the cost measured here,
   and a full run brings sixteen. 5,595 full runs carried 45.8 of the 70.7
   MB; the ring filled and the stream stood still for room 1,845 times, one
   stop to three full runs, until the game's next call. While a movie plays
   in step the game calls once a frame, so the data for each frame was read
   inside that call, with the decoder waiting, and the stream idle in
   between. Once behind, the player skipped ahead in bursts: 272 of its 319
   frames came in sequence and on time, between gaps of up to 2.15 s. The
   host model reproduces this (`playing` in
   `tests/test_toy_wire_integration.c`): on a quiet bus no frame finds its
   data ready.
3. The reader's own cost is high: 9.05 s in interrupt visits for an average
   of 178 microseconds each, about 67 microseconds a block. The disassembly
   of `kui_wire_next` shows why: a table step and a branch for every byte,
   compiled for size, with loop counters kept on the auxiliary's stack,
   which is uncached memory. For both movies that is roughly 15 s of the
   CPU. Not changed by Test 69.
4. 74 of the 86 streams ended at a byte between blocks that was neither
   idle nor the token. The host model never produces this and the report
   does not say what the byte was. Each cost a CMD12, a CMD18 and a block
   by programmed transfers, from the foreground only. Unexplained; Test 69
   records the bytes around the first two and a histogram.
5. The first movie lost nine windows to reads the game waited for with
   interrupts held off, which the low reader then served. It played
   through regardless.

## Verification

Host and linked checks only; none is a console result.

Source commit `806015566fc7` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-68/`.

| Check | What ran | Result |
|---|---|---|
| Card stream and ring reader | `tests/test_toy_wire_stream.c`: the production `toy_wire_stream.c` on a register model of receiver, channel 1, spare timers and a card in an open CMD18; the wire advances in pieces; stalls from none to far denser than Test 67 measured; gaps of 0 to 37 bytes; data full of idle- and token-looking bytes | 180,011 blocks checked against the card over 528,404 overruns. No block handed on as clean or rebuilt was wrong. 353 uncheckable blocks were handed on wrong, each with lost bytes behind a lost gap byte: the corner left to the sector check. |
| Whole reader | `tests/test_toy_wire_integration.c`: the real `retail_async.c`, `sci_stream.c`, GD service and auxiliary with the stream, on the shared card model, driven by GD requests, recognised EXEC polls, SCI error interrupts through the resident's dispatch and the timer through the SDK table | Every request completed with the image's exact bytes: cold open and 21 of 22 requests served on arrival with one CMD18 and no CMD12; stalls up to continuous busy phases; scattered extents; a block damaged on the wire; an image without parity; another read mid-movie; the movie ending under a waiting request (stream stopped in the interrupt visit, CMD12 later from the foreground); no free timer; DMA channel 1 taken. The model asserts that no programmed bus byte is clocked while the stream owns the SCI and that the resident's stream state is DMA exactly then. |
| Parity fill | `tests/test_mode1_erasure.c` against `src/core/recovery_sector.c` | 27,329 patterns filled exactly, 11 refused |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All 22 rows with nothing counted; at a full run's largest plausible values the 17 essential rows end at byte 852 of 1012; at full width rows are left out whole, never cut |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` | 30 of 30 deliberate defects caught |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes, with the predicates Card Tools 09c9a4b686d4 was built from | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; with this build's label the 12 label bytes differ and nothing else |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `ae72c817...c17a` |
| Earlier deliveries | Unchanged recipes on this source | Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce; Test 66's own liveness test passes for mode 66 |
| Hardware sequences | Text comparison of `toy_wire_stream.c` with `toy_wire_probe.c` | `timer_arm`, `timer_cancel`, deselect, handoff with module reset identical; `start_dma` and capture differ only in addressing the ring |

Limits. The models deliver what they are told the hardware does; they
cannot show bus timing, interrupt latency or the game's real request
pattern. Test 66's request path, which decides whether the game is served
from the window, has no console result.

## Package

`KUI-Toy-Wire-Stream-Test-68-13NOV2026-2031-CST-8060155.zip`, 1,799,676 bytes, SHA-256
`493ece78d6e5c2fe07a01d698fb00a49cce799ccecabe4e17555b3e2f3b16d52`, prepared 2026-11-13 20:31 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `806015566fc7` | `23676B7E` | `ae72c817...c17a` | Changed: Test 68, replaces the Test 66 module `3124522ea906` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 \
  BUILD_ID=806015566fc7 build/toy-suite-68/retail-toy-test-68.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 \
  toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test
python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 68), CD Music and virtual VMU Off. Confirm `TEST 68` and
`BUILD 806015566fc7` in the saved `/reports/toy/66-N.txt`.
