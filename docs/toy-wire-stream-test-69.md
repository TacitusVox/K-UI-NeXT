# Toy Commander Test 69: reading the card stream between the game's calls

Status: ran on the console on 2026-11-14 (build `fb625c09dde6`, owner's
report `/reports/toy/66-3.txt`). A regression against Test 68: the first
movie skipped frames where it had been fluid, and the second fell into the
slideshow sooner. Not accepted; the timer continuation is withdrawn. The
run's measurements are what Test 70 rests on
(`docs/toy-wire-stream-test-70.md`). See "Console result" below.

## Baseline and the one change

- Baseline: Test 68, build `806015566fc7`
  (`docs/toy-wire-stream-test-68.md`), as it ran on the console on
  2026-11-13. Its stream and checks worked, the first movie played through,
  and the second fell into a slideshow two seconds in. The saved report is
  `docs/evidence/toy-wire-stream-68/console-report-66-2.txt`.
- What that report showed. The second movie's data was read mostly inside
  the game's own read calls. An SCI interrupt visit reads for 390 ticks,
  about six blocks at the reader's measured cost; a full run brings
  sixteen. 5,595 full runs carried two thirds of the bytes, the ring
  filled, and the stream stood still for room 1,845 times, each time until
  the game's next call. While a movie plays in step that call comes once a
  frame, so each frame's sectors were read with the player waiting and the
  wire idle in between.
- The one behavior change: when the stream is read. A visit that runs out
  of its time with blocks left has the stream's spare timer call once more,
  586 ticks later. The timer's visit, which in Test 68 only ended a wait,
  now also reads for the 390 ticks an SCI interrupt visit reads. Reading
  goes on in the same short pieces, with the game running between them,
  whether or not the game calls.
- Measurement only, in the same report: where the reader's time goes, how
  long requests wait, what a restarted stream costs, and what lies around a
  byte between blocks that is neither idle nor the token.
- Decision the console test answers: does the second movie play in step
  once its data is read in the background? If not, the report says which
  of the remaining causes it is (see "Reading it").

Not changed: every register sequence, the block and sector checks, the
window and what it serves, how requests are published, the visits' time
limits, the low residents, Card Tools.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Compiled without either switch; byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The timer call and the timer visit's read; counters. Image 24,032 of 24,576 bytes (Test 68: 22,816). |
| `native-worker` | The report's Test 69 rows and title. |
| stage | None. |
| Card Tools | Not rebuilt. 09c9a4b686d4 loads the module from its Test 66 path. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_TICK=1`.
Only code under `KUI_TOY_WIRE_TICK` differs from Test 68; without the switch
the Test 68 module reproduces byte for byte. Lease, spans and arena are
Test 68's.

## How it works

1. `toy_wire_drain` reports whether it stopped for time rather than for
   lack of data or room.
2. If it did, `toy_wire_read` counts that and calls `kui_wire_tick`
   (`src/loader/toy_wire_stream.c`). While the stream receives or holds for
   room, and a timer is free, this arms the stream's one-shot timer with
   the same sequence a wait uses. During a wait nothing is armed: the wait
   owns the timer and its end brings the same visit.
3. The timer's callback enters the auxiliary through the visit gate as in
   Test 68. `toy_wire_tick_visit` ends a wait if one is running, then, if
   the window is live and the region is the reader's to use, reads for 390
   ticks and arranges the next call if that ran out too.
4. The gap of 586 ticks leaves the game three fifths of the time while the
   reader is behind. At the measured cost that still reads faster than the
   wire delivers, so the reader catches up and the calls stop.

What the timer visit may do is what an SCI interrupt visit already does:
service the stream, feed blocks into the window, and stop the stream
without a card command when it cannot be followed. No request state is read
and no card command is sent from it. The resident's GD entry runs with
interrupts blocked from its entry (`retail_resident.S`,
`.Lgd_common`), so the timer, like the SCI interrupt, only ever interrupts
the game's own code, never a visit or the resident.

If no timer is free (`TMR` 0) nothing is armed and the reader behaves as
Test 68.

## Known and not changed here

- The reader's cost. About 67 microseconds a block in the ring reader, and
  roughly a fifth of the CPU over both movies. Test 69 moves that work out
  of the game's read calls; it does not reduce it. If the second movie
  still falls behind with its data ready on time, this is next: a
  register-only, word-wise block path, and the auxiliary's stack out of
  uncached memory.
- 74 streams in Test 68 ended at an unexpected byte between blocks. Not
  explained. The `F` rows are there to explain it.
- A restarted stream needs the foreground for its CMD12, CMD18 and first
  block. `P RT RMAX` measures what that costs.
- Findings of the independent review of Test 68 on paths the console run
  did not reach (`DECL` 0, `FAULT` 0, no window lost to an error). Each is
  its own later change:
  1. A low-reader reception left in flight when a window opens is never
     retired; the seek then waits on it for good.
  2. A timer wait has no second exit if its interrupt is lost; only the
     next visit from elsewhere ends it.
  3. A full lost-position log while the next block is incomplete leaves
     stream and reader waiting on each other.
  4. Requests of 61 to 64 sectors are not served from the 62-sector window
     and close it, counted under `O`.
  5. Stopping a running stream waits for its next overrun with interrupts
     masked, up to one run's time (the 5.29 ms longest visit in Test 68).
  6. The kept counters are first written through the cached view.
  7. A CMD12 sent into the middle of a block may have its answer misread
     once.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 69`. Values are hexadecimal without leading zeros. The rows are
in order of importance and a row that does not fit is left out whole.
Sample: `docs/evidence/toy-wire-stream-69/report-sample.txt`.

| Row | Meaning |
|---|---|
| `PIPE B I CALLS IRQ` | Window openings per movie; reader visits from the game and from the SCI interrupt |
| `AHEAD B I HIT EDC STOP` | Sectors served per movie, requests served, sectors verified, windows given up on error |
| `I SUBMIT CADENCE MAX AT` (and `B`) | Frames submitted, of those on time and in sequence, longest gap in ticks, the frame before it |
| `I FIRST LAST SKIP EXIT WF SF` (and `B`) | Ticks to the first frame, last frame, frames skipped, exit code, first frame with a read still pending, first frame skipped |
| `V SCI TMR GAME N TICKS` | Reader visits from the SCI interrupt, the timer and the game; then the ticks spent in each |
| `V MAX SCI TMR GAME` | Longest single visit of each kind |
| `Q B N T MAX I N T MAX` | Per movie: requests the window could not serve when first asked, their ticks until served, the longest |
| `P TICK BUDGET RT RMAX` | Timer calls arranged; visits that ran out of time with blocks left; ticks from a stream given up to its successor receiving, in all and at most |
| `W START SEG CUT WAIT FULL` | As Test 68 |
| `W BYTES WTICK TMR ERR DET` | As Test 68 |
| `W BLK FIX UNFIX HOLED NOCRC SEEK` | As Test 68 |
| `W GAPI GAPT BADCRC FRAME AMBIG` | As Test 68 |
| `W FILL BYTES MOST REF BAD DECL` | Test 68's `W FILL` row with takeovers declined appended |
| `W STOP C J S F X` | Stream ends: close, jump, sector, frame, fault |
| `W DROP E N I C W O` | Windows given up: error, movie over, idle, command, waited for with interrupts off, other |
| `F BIT03 BIT47 KIND` | Unexpected bytes between blocks by their lowest zero bit (a count in a byte each, bit 0 lowest) and by the kind of the block before (none, clean, rebuilt, with lost bytes) |
| `F1`, `F2` | The first two: four bytes before; the byte and three after; distance from the last block's end, lost positions among the eight and how many of the last four had arrived; distance back to the nearest lost byte; blocks delivered by that stream with the last one's kind |
| `SD BLK PIO START CRC` | Low reader: DMA blocks, programmed blocks, CMD18 starts, CRC failures |

Bytes in `F1` and `F2` are as the receiver delivers them, bit-reversed:
idle is `FF` and the token `7F`.

## Reading it

- `I SKIP` small, `I SUBMIT` near `7B0`, both exits `7DC`: reading in the
  background was what the second movie lacked.
- `Q I` still large: requests still waited. `P BUDGET` near `P TICK` and
  `W FULL` small say the reader did keep reading; then look at `P RT`
  against `W STOP F`: restarted streams are the wait. `P TICK` 0 with `TMR`
  0 says no timer was free.
- `Q I` small and `I SKIP` still large: the data was there in time and the
  player still fell behind. The sum of `V` ticks over the movies' length is
  the reader's share of the CPU; the reader is then too expensive and the
  block path is the next change.
- `F BIT03 BIT47` concentrated in one bit, with `F1`/`F2` showing idle bytes
  and then a token shifted by a bit: the receiver lost bit alignment at a
  resume. Spread like random data, close behind a lost position: the
  framing slipped by a byte.
- `V MAX` much above 390 for `SCI` or `TMR`: a visit held interrupts off
  that long, as Test 68's 5.29 ms did when a stream was stopped.

## Console result

Owner's run, cold CD boot, Card Tools 09c9a4b686d4, raw Toy Commander GDI,
"66 SCI movie playback", CD Music and virtual VMU off. Saved report
`docs/evidence/toy-wire-stream-69/console-report-66-3.txt` (1,024 bytes,
SHA-256 `77544e94...7a3a`), headed `TEST 69`, `BUILD fb625c09dde6`.

Owner's observation: "First video had some frame skips where it was
completely fluid before. Second video turned into a slideshow much sooner,
but still managed to make it till the end. This is a regression."

| | First movie (B) | Second movie (I) |
|---|---|---|
| Frames submitted of the movie's | 224 of 331 (Test 68: 317) | 277 of 1,968 (Test 68: 319) |
| Of those on time and in sequence | 205 | 226 |
| Frames skipped | 107, from frame 3 (Test 68: 14) | 1,691, from frame 20 (Test 68: 1,649) |
| Longest gap between frames | 0.53 s after frame 226 | 2.39 s after frame 61 |
| Requests that waited for the window | 1, for 21 ms | 390, 7.31 s in all, at most 37 ms |
| Player exit | `7DC` | `7DC` at frame 1,968 |

The reader's own time, measured for the first time, over both movies
(about 76 s):

| Caller | Visits | Time | Longest |
|---|---|---|---|
| SCI interrupt | 24,538 | 9.80 s | 5.60 ms |
| Timer | 15,144 | 7.12 s | 1.63 ms |
| The game's calls | 113,302 | 15.45 s | 8.66 ms |
| All | 152,984 | 32.37 s | |

Stream: 29 begun, 5,751 full runs, 22,311 overruns in a run, 5,027 timer
waits (0.96 s), 2,742 stops for room, 69,842,495 wire bytes, TMU1, no fault.
Blocks 134,152: 17,953 rebuilt from CRC16, 3 single losses not explained,
3,498 handed on with lost bytes, 99 with only a CRC byte lost. Sectors
filled from parity 2,755; none beyond the fill; one failed its check. 29,960
timer calls arranged by 29,984 visits that ran out of time. Streams ended:
close 11, sector 1, frame 17. Restarted streams cost 104 ms in all, at most
15.8 ms. No clean block failed its CRC16 and no game read failed.

What the run establishes:

1. The change did what it was built to do and that did not help. The timer
   was called 15,144 times, and the first movie's requests no longer waited
   for data (one, for 21 ms). It skipped 107 frames all the same. A movie
   whose data is on time and which still skips is short of CPU, not of data.
2. The reader takes about 42 % of the CPU. 32.4 s in its visits over 76 s
   of movies, before interrupt entry and exit and before the copies to the
   game. Test 68 had the same cost and did not measure it. Moving the work
   between callers cannot help while it is this large, and the timer's
   extra visits and the measurement itself added to it: hence the
   regression.
3. Where the time goes, from the compiled code of this build
   (`kui_wire_next`, the per-word loop at `0x8c425fe6`): 79 instructions
   and nine accesses to the auxiliary's private stack for every four bytes,
   four writes and five reads of spilled temporaries, besides the one read
   of the ring. That stack is at its uncached address, so each of those is
   a bus cycle of some 70 ns: about 145 microseconds a block for this loop
   alone, 90 of them stack traffic. The sector check (`kui_cd_edc`, eight
   instructions a byte, no stack traffic) adds about 20 microseconds a
   block. At about 185 microseconds a block, 134,152 blocks are 25 s; the
   rest of the 32 s is the fixed cost of 153,000 visits.
4. The unexpected bytes between blocks are explained as far as the note
   reaches. All 17 followed a block handed on with lost bytes. In both
   noted cases the byte lies directly behind the block's end, with one idle
   byte and the token after it (`4E FF 7F`, `11 FF 7F`): it is that block's
   own last CRC byte. The block as framed is one position short, so one of
   its recorded lost positions was not a lost byte. That fits an overrun
   flagged in the instant the channel took the held byte, with the next
   byte delivered after all. 17 in 28,062 run ends here, 74 in 52,510 in
   Test 68; half a percent of the blocks handed on with lost bytes in both.
   Each costs one restart, 104 ms over the whole run. Not addressed.
5. The card's gap is one idle byte before the token in both notes, not two.

## Verification

Host and linked checks only; none is a console result.

Source commit `fb625c09dde6` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-69/`.

| Check | What ran | Result |
|---|---|---|
| The change, end to end | `playing` in `tests/test_toy_wire_integration.c`: the real low reader, SDK SCI routing, auxiliary and stream on the shared card model; the reader at the console's pace (six blocks to 390 ticks), the wire at its own (2,048 bytes a moment), one game call a frame and interrupts in between | With the switch, each of 21 frames' data is ready when the game asks, on a quiet bus and with a stall every 6,000 bytes. Without it (the Test 68 recipe, `playing-model-without-switch.log`): 0 of 21 on a quiet bus, 18 of 21 with stalls, as the console showed. Data exact in all. |
| Whole reader | The same test's Test 68 scenarios, also repeated at the console's pace | Every request completed with the image's exact bytes. The report's visit counts agree with the model's count of interrupts and calls. |
| Card stream and ring reader | `tests/test_toy_wire_stream.c` with timer calls asked at random | 179,994 blocks checked against the card over 528,338 overruns. A call is arranged exactly while the stream receives or holds for room and a timer is free; a running wait keeps its own timing; both spare timers are as found after every stop. |
| Lost-frame note | Same test: a stray byte put between two blocks, three values | Stream stopped; bytes before and after, distances, block count and both histograms as placed. |
| Parity fill | `tests/test_mode1_erasure.c` | 27,329 patterns filled exactly, 11 refused |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All 21 rows fit at a full run's values (997 of 1,012 bytes); the 18 rows the test is read by end at byte 876; at full width rows are left out whole, never cut |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` | 40 of 40 deliberate defects caught, 10 of them in what only Test 69 has |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; otherwise only label bytes differ |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `c6baf010...8beb` |
| Earlier deliveries | Unchanged recipes on this source | Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Limits. The models deliver what they are told the hardware does. The
playing scenario's pace is set from Test 68's console numbers; it shows the
mechanism, not the console's timing, the decoder's own need for the CPU, or
what the timer's interrupts cost the game. With stalls far denser than any
console run the model is paced by the wire and by sectors read again, with
or without the switch.

## Package

`KUI-Toy-Wire-Stream-Test-69-13NOV2026-2158-CST-fb625c0.zip`, 1,801,651 bytes, SHA-256
`52d2b814e320794d652efca519e3b79622fe5999b084c15297471d2618764386`, prepared 2026-11-13 21:58 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `fb625c09dde6` | `ADB4AA80` | `c6baf010...8beb` | Changed: Test 69, replaces the Test 68 module `806015566fc7` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_TICK=1 \
  BUILD_ID=fb625c09dde6 build/toy-suite-69/retail-toy-test-69.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_TICK=1 \
  toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test
python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 69), CD Music and virtual VMU Off. Confirm `TEST 69` and
`BUILD fb625c09dde6` in the saved `/reports/toy/66-N.txt`.
