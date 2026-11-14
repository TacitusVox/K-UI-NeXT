# Toy Commander Test 73: the game's idle GD calls, and the player itself in the report

Status: ran on the console on 2026-11-14 (build `e623c2a697d5`, owner's
report `/reports/toy/66-7.txt`). The change did what it was built to do
and the second movie did not change: 513 frames against 567. The player's
own figures, printed for the first time, show why Tests 70 to 73 moved so
little: the reader's time is in the reading itself and in the dispatcher
ahead of it, not in the number of visits, and the second movie's player is
out of processor time by a hair. Kept as the baseline of Test 74
(`docs/toy-wire-stream-test-74.md`); not accepted as a whole. See "Console
result" below.

## Baseline and the one change

- Baseline: Test 72, build `8c00ede90b9a`
  (`docs/toy-wire-stream-test-72.md`), as it ran on the console: the first
  movie as from the disc; the second movie showing 567 of 1,968 frames;
  48 of 96,961 update calls reaching the reader; the reader's time
  14.77 s, of which 9.67 s in 119,274 visits from the game's calls.
- What that run showed. The visits Test 72 removed were the cheap ones.
  The game's GD calls, EXEC and CHECK, come about 1,550 times a second
  while a movie plays, for some thirty requests a second. The low reader
  passes each on as a delivery visit (`deliver()` in
  `src/loader/retail_async.c`), the auxiliary's dispatcher hands it to
  `toy_movie_ahead()`, and with no request to serve that ended in the
  foreground pump every time: 81 microseconds inside the pump, the same
  0.5 to 0.7 s in every 5.4 s of the second movie whatever data was read.
- The one behavior change: with `TOY_WIRE_IDLE=1`, a delivery visit from
  one of the game's GD calls that has no request to serve or to wait for
  pumps only under Test 72's rule (`toy_wire_foreground_due()` in
  `src/loader/toy_wire_ahead.inc`, now shared by both kinds of call):
  1. a stream has to be started and the window has room;
  2. the stream stands still with its ring full and the window has room;
  3. the stream's interrupt is due and has not come;
  4. no call of the game's has read for 3,125 ticks (4 ms), or the window
     is in error, or the clock is not the expected one.
- Not changed: a GD call that serves a request pumps behind it, a GD call
  that waits for a request pumps every time, the SCI error interrupt and
  the stream's timer visit as before, an update call the hook has passed
  on pumps. Every check `toy_movie_ahead()` makes ahead of the pump (the
  movie still playing, the window still the request's, no error) runs on
  every call as before. Every sector is verified and every block's CRC16
  checked. The low residents are not touched: the gate and the dispatcher
  are still entered for each GD call.
- Decision the console test answers: what the idle GD calls were costing
  (`V GAME`, `U`, `D`), and, from the player's own counters, whether the
  second movie's player is short of processor time, of data, or of
  neither.

Estimate, from Test 72's numbers and not from any measurement: the pumps
from the game's calls fall from 119,274 to between 20,000 and 30,000 and
their time from 9.7 s to about 3 s.

What is not expected with any confidence is the effect on the picture.
Test 72's run showed the game making more of these calls when each cost
it less: it repeats them while it waits. If the reader's time in them
comes out of time the game spends waiting, removing it changes little, as
removing the update visits did. That is why this build measures the
player.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The gate before the pump; the shared rule; the hook stub's clock value; the player's measurements; the dispatcher's count of delivery visits. Restart timing is not compiled. Image 24,512 of 24,576 bytes: 64 are left. |
| `native-worker` | The report's rows and title. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1`. Without
`TOY_WIRE_IDLE` the Test 72 module reproduces byte for byte.

## Telling the calls apart

At the pump, an idle GD call and an update call the hook has passed on
look alike: no request, no movie named by a request. The update call has
already been through the rule in the hook, and holding it to the rule a
second time would stop it from ever reading (the hook has just taken the
clock value the rule compares with). The dispatcher therefore records the
event of an update visit in `C.current_event`, as it does for every other
event, and the gate applies the rule only to a visit whose event is a
delivery, outside an interrupt, with no request waiting. The first version
of the gate tested for a movie named by the request instead; the reader
integration test showed it never applying, since `suite_movie()` names a
movie only while a request is active.

## The player's measurements

The hook stub (`src/loader/toy_movie_exit_hook.inc`) wraps the game's own
movie update at `0x8c08d638`. It now reads the clock before it calls the
original, keeps the value in the stack word that was spare, and hands it
to `kui_toy_movie_ahead_update()` afterwards. On the game's stack, outside
the gate, `toy_wire_update_seen()` then reads, and only reads, from the
game's memory:

- the player's output counter (`ctx+0xac`) and skip counter (`ctx+0x90`),
  the two the exit witness has recorded since Test 49;
- the player's lag counter at `0x8c1138c4`, which the hook already
  records at the player's lag exit, with twice the frame rate as its
  limit;
- the player's buffer, through the three pointers `toy_chunk_buffer()`
  follows, each checked to lie in the game's RAM: capacity at `avi+0x14`,
  released and filled at `avi+0x64` and `avi+0x68`.

A call in which the output counter moved is counted as a frame put out,
with the ticks inside the update, the ticks since the update before
returned, and the reader's SCI and timer ticks in that span. A call in
which only the skip counter moved is counted as a skip with its ticks. A
movie's first call only takes the counters as they stand. For the second
movie each piece of the course keeps the most of the buffer that was free
and the highest lag.

Limits of these numbers. The update's own time includes whatever the
player does inside it, waiting included. The interrupt ticks are those of
the reader's pump, not of the gate, the SDK's dispatcher or the cache
misses a visit leaves behind. The lag counter's unit is the player's own;
the limit recorded with it is twice the frame rate, so it is taken to be
frames.
That reading is not established: it rests on what the earlier hook
recorded as the limit.

## Known and not changed here

1. Streams that cannot be followed: 265 in Test 72. The retry without
   each lost position, settled by the CRC16, is a later change.
2. The sector check behind a sector whose blocks were all clean.
3. The auxiliary's context page at its uncached address.
4. Reads of the ring through the cache.
5. The first second of the second movie is read before the window opens;
   every run so far skips first at frame 20 to 26.
6. The gate and the dispatcher are still entered three times for each of
   the game's EXEC and CHECK calls (entry trace, delivery, exit trace).
   `D` measures the delivery visit's share of that.
7. The auxiliary's code space: 64 bytes. The next change that adds code
   has to take some out or move the split with the arena.
8. The review findings listed in `docs/toy-wire-stream-test-69.md`.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 73`. In order:

| Row | Meaning |
|---|---|
| `PIPE`, `AHEAD`, `I SUBMIT`, `I FIRST`, `B SUBMIT` | As Test 72 |
| `IP ON OT OC OI SN ST` | Second movie's update calls: frames put out; ticks inside those calls; ticks from the update before to their end; the reader's interrupt ticks in that span; frames skipped; ticks inside those calls |
| `BP ON OT OC OI SN ST` | The same for the first movie |
| `IB SKU SKN WTU WTN CAP ALL` | The player's buffer: bytes in it and bytes of the next video chunk not yet in it, at the first skip and at the first wait for a read; its capacity; ticks inside all the second movie's update calls |
| `IC RDY WGD NOGD OTH` | The second movie's chunk acquisitions: ready; not there with a GD request pending; not there with none pending; other |
| `V SCI TMR GAME N TICKS` | As Test 72: pumps and their ticks by caller |
| `U S W D G S W D` | Update calls seen, passed on for work, passed on as due; idle GD calls seen, pumped for work, pumped as due |
| `D N TICKS` | Delivery visits from the game's GD calls, and the dispatcher's ticks in them |
| `TF`, `TD`, `TW` | As Test 72 |
| `TB` | Most of the player's buffer that was free in each piece, in units of 8 KiB |
| `TL` | Highest value of the player's lag counter in each piece |
| `W START`, `W BYTES`, `W BLK` | As Test 72 |
| `TI`, `TG` | As Test 72 |

Gone to make room: `B FIRST`, `V MAX`, `Q`, `P`, `TX`, `W GAPI`, `W FILL`,
`W STOP`. `IB`'s first four values and `IC` have been recorded since Test
66 and were last printed by it.

## Reading it

- `V GAME` against Test 72's `1D1EA` and `734262` (119,274 and 9.67 s);
  `G` seen against pumped; `D` ticks against `V GAME` ticks: the part of a
  delivery visit that is not the pump.
- `BP`: the first movie plays every frame, so `OT / ON` there is what a
  frame's update costs when the player keeps up, and `OC / ON` should be
  near a frame's 26,042 ticks.
- `IP` against it. `OT / ON` well above the first movie's, or above
  26,042: the second movie's frames take longer to produce than they
  last, and `OI / ON` is the reader's interrupt share of that. `OT / ON`
  no higher than the first movie's while most frames are skipped: the
  player is not short of processor time in its update.
- `SN` against `I SKIP`, and `ST / SN`: what a skipped frame costs.
- `TB` near the capacity (`80` for a megabyte) in the pieces that skip,
  or `IC WGD` and `NOGD` large against `RDY`: the player is running out of
  data in its own buffer although the window served its requests.
- `TL` rising through the pieces that skip and falling in the last two.
- `IB ALL` against the movie's 51 million ticks: how much of the movie
  the player spends inside its update at all.
- `TW` against Test 72's `70F0204 4050B0F 421C0000`: if requests now wait
  longer, the spacing is leaving the window short.
- `B SUBMIT 14B`: the first movie must stay as it is.

## Console result

Owner's run, cold CD boot, Card Tools 09c9a4b686d4, raw Toy Commander GDI,
"66 SCI movie playback", CD Music and virtual VMU off. Saved report
`docs/evidence/toy-wire-stream-73/console-report-66-7.txt` (1,024 bytes,
SHA-256 `fcdd2c57...e4ff`), headed `TEST 73`, `BUILD e623c2a697d5`.

Owner's observation: "Test 73 wasn't substantially different than test 72.
Maybe more of the intro was fluid before it started skipping around, but
only like a half second more."

| | First movie (B) | Second movie (I) |
|---|---|---|
| Frames submitted of the movie's | 331 of 331 | 513 of 1,968 (Test 72: 567) |
| Of those on time and in sequence | 330 | 469 |
| Frames skipped | not printed | 1,455, from frame 27 |
| Longest gap between frames | 47 ms after frame 315 | 2.10 s after frame 1,021 |
| Player exit | not printed | `7DC` at frame 1,968 |

The player's own update calls (`IP`, `BP`, `IB`, `IC`):

| | First movie | Second movie |
|---|---|---|
| Calls that put a frame out | 330 | 488 |
| Time inside them | 8.79 s, 26.6 ms each | 41.75 s, 85.6 ms each |
| From the call before to their end | 26.7 ms each | 85.6 ms each |
| Reader's interrupt pumps in that span | 0.98 s, 2.97 ms each, 11.1% | 4.69 s, 9.62 ms each, 11.2% |
| Calls that skipped and put nothing out | 0 | 19, 18.32 s, 0.96 s each |
| Time inside all update calls | not printed | 65.67 s |
| Chunk acquisitions: ready / not there with a request pending / with none / other | not printed | 1,968 / 7,275 / 410 / 1 |
| Buffer capacity | | 823,584 bytes (402 sectors) |
| Buffer at the first skip (frame 27) | | 362,241 bytes in it, the next chunk all there |
| Buffer at the first wait (frame 121) | | 34,741 bytes in it, 128,967 of the next chunk missing |

| Caller | Pumps | Time | Each | Test 72 |
|---|---|---|---|---|
| SCI interrupt | 35,892 | 7.94 s | 221 us | 31,335, 5.07 s |
| Timer | 7,425 | 0.03 s | 4 us | 7,167, 0.03 s |
| The game's calls | 17,109 | 7.15 s | 418 us | 119,274, 9.67 s |
| All | 60,426 | 15.12 s | | 157,776, 14.77 s |

Update calls: 91,373 seen, 10 passed on for work, 908 as due. Idle GD
calls: 105,841 seen, 638 pumped for work, 5,331 as due. Delivery visits
from the game's GD calls: 120,642, 14.50 s in the dispatcher.

The second movie's course, twelve pieces of 5.4 s from its start:

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted (161 is all) | 44 | 2 | 14 | 14 | 14 | 6 | 6 | 3 | 14 | 19 | 162 | 215 |
| Sectors served | 1,712 | 2,448 | 2,384 | 2,416 | 2,352 | 2,320 | 2,400 | 2,384 | 2,432 | 2,496 | 640 | 1,680 |
| Requests waited, s | 0.19 | 0.48 | 0.10 | 0.15 | 0.06 | 0.19 | 0.34 | 0.67 | 1.61 | 1.13 | 0 | 0 |
| Most of the player's buffer free, KiB | 768 | 0 | 768 | 768 | 768 | 768 | 768 | 752 | 0 | 432 | 96 | 440 |
| Highest lag counter | 2 | 0 | 2 | 2 | 2 | 2 | 2 | 2 | 0 | 2 | 0 | 1 |
| Reader in interrupt visits, s | 0.46 | 0.63 | 0.59 | 0.59 | 0.59 | 0.59 | 0.63 | 0.63 | 0.73 | 0.71 | 0.15 | 0.40 |
| Reader in the game's calls, s | 0.40 | 0.50 | 0.52 | 0.55 | 0.52 | 0.48 | 0.50 | 0.50 | 0.61 | 0.61 | 0.36 | 0.36 |

Stream: 260 begun, 6,454 full runs, 30,974 overruns in a run, 8,562 timer
waits (3.41 s), 1,634 stops for room, 72,834,759 wire bytes, TMU1, no
fault. Blocks 134,563: 17,109 rebuilt from CRC16, 10 single losses not
explained, 7,457 handed on with lost bytes, 54 with only a CRC byte lost;
299 read by the low reader to open or reopen. No game read failed.

What the run establishes:

1. The change works as built. Of 105,841 idle GD calls 5,969 pumped, and
   the pumps from the game's calls fell from 119,274 to 17,109. The first
   movie is unchanged.
2. The reader's time did not fall with them: 15.12 s in pumps against
   14.77 s. What the idle pumps no longer read, the interrupt visits read
   (7.94 s against 5.07 s) and the remaining pumps from the game (418
   microseconds each). The pump's time is the reading: 112 microseconds
   for each of the 134,563 card blocks, in every test since the word loop
   of Test 71. Tests 72 and 73 took its "81 microseconds a visit" for a
   fixed cost of visiting. That was wrong: it was the reading, done by
   whoever visited.
3. As linked, the reading is about 22 instructions for each byte of a
   block: 49 for four bytes in the block loop (CRC16 and bit reversal),
   6 for four in the copy to the window, and 16 for two in the sector's
   EDC, which runs over every sector. Read from the Test 73 module, not
   measured part by part.
4. A delivery visit costs as much again outside the pump. The dispatcher
   took 14.50 s for the game's 120,642 delivery visits, 7.15 s of that in
   the pump: 61 microseconds a visit for the dispatcher's own path and for
   copying 29,159 sectors out to the game. An interrupt's visit takes the
   same path (43,317 of them); its share is not measured.
5. The player is inside its update for 65.67 s of a movie of 65.4 s: in
   the second movie it has no idle time. Every frame's chunk is acquired
   (1,968 ready), shown or not. The calls that put a frame out take
   85.6 ms on average against 26.6 ms in the first movie, and 19 calls
   take about a second each and put nothing out. The last two pieces show
   377 frames in 10.7 s, so those calls take at most 28.5 ms each there;
   that leaves 49 to 53 s of such calls for the 1,590 or so frames of the
   first 54 s: 31 to 33 ms a frame, shown or not. The player works
   through that part at real time or just under it. It never gets ahead,
   so it shows a frame or a few and is late again.
6. Measured, the first movie's frame takes 26.6 ms of which 2.97 ms are
   the reader's interrupt pumps, and leaves 6.7 ms of its 33.3 ms unused.
   The rest of the reader's share inside those calls (the dispatcher's
   path for each interrupt, and whatever GD calls the player makes there)
   is not measured. Taking it at 4 to 5 ms, by the per-visit costs in 4,
   the player's own work on a displayed frame comes to 19 to 20 ms. The
   same reckoning for the first 54 s of the second movie (32 ms a frame,
   the reader's share put at 12 to 13 ms as in 7) gives the same 19 to
   20 ms. An estimate that agrees with itself across the two movies; not
   a measurement of the decoder.
7. On that estimate the player can spare about 13 ms of each frame for
   whatever reads its data, and in the first 54 s of the second movie the
   reader takes about that. Per frame there, from the report: 6.8 ms in
   pumps and 3.2 ms in delivery visits outside the pump; and by estimate
   0.9 ms in the same path for interrupt visits, 0.8 ms in the two trace
   visits around every GD call, and the gate around all of them. The
   second movie sits on the edge. That fits Tests 70 to 73 moving between
   436 and 567 frames for reader times between 14.8 and 16.7 s, and the
   first movie skipping only while the reader took 32 s (Tests 68 and
   69).
8. The first skip is not for want of data. At frame 27 the player's
   buffer held 362,241 bytes and the next chunk was complete. Test 57's
   opening hold on the game's refills is still in this build
   (`kui_toy_movie_refill_defer`): it lets the buffer run from full down
   to 384 KiB before the window may serve, so the reader has to start,
   read ahead and keep pace all at once with half the buffer gone. From
   frame 121 on the buffer is nearly empty at some point in most pieces.
9. The lag counter at `0x8c1138c4` stands between 0 and 2. It is not a
   count of frames behind.
10. Pieces 1 and 8 show no free buffer and no lag. If no update call
    returned in those 5.4 s there was nothing to sample; the 19 long
    calls would have to include two of more than five seconds. Not
    established.

What follows for the next change: the reader has to cost half of what it
does. Fewer visits do not get there; cheaper reading and a shorter way to
it do. Test 74 is built on that.

## Verification

Host and linked checks only; none is a console result.

Source commit `e623c2a697d5` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-73/`.

| Check | What ran | Result |
|---|---|---|
| Idle GD calls pump only when needed | `polling()` in `tests/test_toy_wire_integration.c`: the real low reader, auxiliary and stream on the card model, two EXEC calls with nothing pending to every 1.3 ms of a played movie, the reader at the console's measured pace | 1,050 idle calls: 127 pumps on a quiet bus, 136 with stalls, 150 under heavy stalls. All foreground pumps 148 against 1,092 in the Test 72 configuration. Every request completed with the image's exact bytes; the window was never given up |
| The game is served as before | The same scenario in the Test 72 configuration (`idle-model-comparison.log`) | Data ready for 21 of 21 frames when asked in both, on a quiet bus and with stalls; 0 of 21 in both under heavy stalls with 45 restarted streams |
| A waiting request is not an idle call | `waiting()`: a request far ahead of the window, runs held in flight | Every call that waits for it pumps and none is counted idle; the request completes with exact bytes |
| An update call passed on reads | `updating()`: each call the hook passes on is followed by exactly one pump | Held for all three bus conditions |
| The player's measurements | `player_measured()`: a model of the player's counters, lag and buffer behind the hook's reads | Frames put out and skipped are told apart by the counters; a call that did both is a frame; a movie's first call counts nothing; the ticks inside the update, since the update before and of the reader's interrupts in between are exact; each piece keeps the emptiest buffer and the highest lag; a buffer with no capacity, with more in it than it holds, or behind a pointer outside the game's RAM is left out; the first movie is counted apart |
| The code as linked | `linked-idle-gate.log`: the hook stub, `kui_toy_movie_ahead_update`, `toy_wire_foreground_due` and the gate in `toy_movie_ahead`, disassembled from the auxiliary's ELF and read against the source | The clock is read before the original and handed over in the call's delay slot; the hook pushes 28 bytes, reads the game's memory and writes only the reader's kept block; the gate pumps for a waiting request, an interrupt's visit and any event but a delivery |
| The gate and the word loop as linked | `tests/test_toy_wire_cached_stack.py`, `tests/test_toy_wire_word_loop.py` | Unchanged from Tests 70 and 71 |
| Whole reader | `tests/test_toy_wire_integration.c` in all six configurations | Passed; 28,400,636 checks in this one |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All 22 rows fit at a full run's values (976 of 1,012 bytes); the 17 rows the test is read by end at byte 758 |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | 79 of 79 deliberate defects caught, 30 of them in what only Test 73 has |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `7f28c744...7124` |
| Earlier deliveries | Unchanged recipes on this source | Test 72 `04c6b439...fe9b`, Test 71 `73e8f9ed...a26f`, Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Limits. The model's GD calls come at an even pace; the console's come in
bursts between frames. The player's memory in the model is nine words at
the addresses the hook reads; that the game's player keeps those values
there rests on the earlier tests' use of the same offsets (exit witness,
chunk observer, lag exit), not on a run of the game's executable, which
was not available to these checks. The hook and the gate are the same C
and assembly as built for the SH-4, read as linked but not run.

## Package

`KUI-Toy-Wire-Stream-Test-73-14NOV2026-0818-CST-e623c2a.zip`, 1,802,623 bytes, SHA-256
`b180eec43a4e8dcdb771a1c2e87378228e0c22e01af506d6430acae8c22f68b8`, prepared 2026-11-14 08:18 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `e623c2a697d5` | `0F34C364` | `7f28c744...7124` | Changed: Test 73, replaces the Test 72 module `8c00ede90b9a` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 BUILD_ID=e623c2a697d5 \
  build/toy-suite-73/retail-toy-test-73.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 toy-wire-host-tests \
  toy-wire-integration-host-test toy-wire-package-test toy-wire-linked-check
python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 73), CD Music and virtual VMU Off. Confirm `TEST 73` and
`BUILD e623c2a697d5` in the saved `/reports/toy/66-N.txt`.
