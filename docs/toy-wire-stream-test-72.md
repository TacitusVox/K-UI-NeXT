# Toy Commander Test 72: the player's update calls reach the reader only when needed

Status: ran on the console on 2026-11-14 (build `8c00ede90b9a`, owner's
report `/reports/toy/66-6.txt`). The change did what it was built to do
and the second movie hardly changed: 567 frames against 524. The cost it
was aimed at is not in the player's update calls but in the game's GD
calls. Kept as the baseline of Test 73
(`docs/toy-wire-stream-test-73.md`); not accepted as a whole. See
"Console result" below.

## Baseline and the one change

- Baseline: Test 71, build `4ee37937a022`
  (`docs/toy-wire-stream-test-71.md`), as it ran on the console: the first
  movie as from the disc; the second movie showing 524 of 1,968 frames;
  the reader's time 15.42 s, of which 9.99 s in 193,622 visits from the
  game's calls.
- What that run showed. Through the second movie's slideshow the player is
  short of CPU time and not of data, and the reader's time in the game's
  calls does not follow the data: about 0.7 s in every 5.4 s piece of the
  movie, the same where 608 sectors were served as where 2,496 were. The
  game calls the player's update some 2,500 times a second, and the hook
  (`toy_movie_exit_hook.inc`, `kui_toy_movie_ahead_update`) turned each
  call into a visit: gate, dispatcher and reader, 52 microseconds inside
  the reader alone, mostly to find nothing to do.
- The one behavior change: with `TOY_WIRE_PACE=1` the hook asks
  `toy_wire_update_due()` (`src/loader/toy_wire_ahead.inc`) first and
  visits only when it says so:
  1. a stream has to be started and the window has room;
  2. the stream stands still with its ring full (`KUI_WIRE_FULL`) and the
     window has room;
  3. the stream's interrupt is due and has not come
     (`kui_wire_overdue()`): an overrun not yet answered, a wait whose
     timer has run out, or a wait that has no timer to end it;
  4. no call of the game's has read for 3,125 ticks (4 ms), or the window
     is in error, or the clock is not the expected one.

  Without a window the call does nothing, as the visit did before.
- Not changed: the game's GD calls, the SCI error interrupt and the
  stream's timer visit as before. Every sector is verified and every
  block's CRC16 checked as before. What a visit does once inside is the
  same code.
- Decision the console test answers: how much of the reader's time the
  update calls were costing (`V GAME` against Test 71's, and the new `U`
  row), and how the second movie plays with that time given back to the
  player.

Estimate, from Test 71's numbers and not from any measurement: the game's
visits fall from 193,622 to a few tens of thousands and their time from
10 s to 2 or 3 s. The gate and dispatcher around each visit were never
inside the reader's own measurement, so what the player gets back may be
more than the report's `V` difference. Whether it is enough for the
second movie's first 54 s is not known.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The update hook; `kui_wire_overdue`; three counters. The notes on unexpected bytes are not compiled. Image 24,000 of 24,576 bytes: 576 are left. |
| `native-worker` | The report's last row and title. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1`. Without `TOY_WIRE_PACE` the Test 71
module reproduces byte for byte.

## The hook

The hook runs on the game's stack with the game's interrupt mask, outside
the gate. Of the reader's state it only reads: the window's variant and
error flag, whether the window has room, whether a stream is on and its
state, and through `kui_wire_overdue` the SCI's status register or the
timer's control register. It writes its own three counts and the clock
value of the spacing, which no interrupt visit touches. A visit that
interrupts it can make one call's answer stale: the next call, on average
400 microseconds later, reads the new state.

Why each case is there:

- Cases 1 and 2 are the work only a call of the game's can do. A stream
  is started and a full ring is emptied into the window from the
  foreground; if these waited for the 4 ms call the wire would stand
  still for up to that long each time, and the wire is what the first
  54 s of the second movie are short of.
- Case 3 covers an interrupt that cannot arrive: the game holds interrupts
  off, or the SDK has taken the SCI's level. Test 71 answered such an
  overrun within one update call, 400 microseconds; without this case it
  could wait up to 4 ms. With the interrupt arriving normally the flag is
  answered before the hook can see it, and the case costs one register
  read a call.
- Case 4 keeps what the interrupt visits leave unread moving and notices a
  window that is over. An interrupt visit has 390 ticks; a call of the
  game's has 1,172. The game's GD calls reset the spacing too: any
  foreground visit that has just read counts.

Everything else is left to the interrupt visits, which run whenever a card
run ends.

## Known and not changed here

1. Streams that cannot be followed: 208 restarts in Test 71. The position
   that is not a lost byte is one where the channel had already taken the
   held byte (Test 71, finding 5). Trying the block without each of its
   lost positions, settled by the CRC16, is a later change.
2. The sector check behind a sector whose blocks were all clean.
3. The auxiliary's context page at its uncached address. The hook now
   reads about ten words of it on every call instead of entering the gate.
4. Reads of the ring through the cache.
5. The first second of the second movie is read before the window opens;
   every run so far skips first at frame 20 to 25.
6. The review findings listed in `docs/toy-wire-stream-test-69.md`.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 72`. Rows as in Test 71, except the last:

| Row | Meaning |
|---|---|
| `U SEEN WORK DUE` | The player's update calls while a player was running; of those, calls passed on for cases 1 to 3; calls passed on for case 4 |

`F END HELD TWICE` is gone: Test 71 answered it (201, 2, 0). The counts
behind it and the note on each unexpected byte are not compiled in this
build.

`PIPE CALLS` and `V GAME N` count visits, so from this test on they no
longer include every update call. `U SEEN` is the number of calls.

## Reading it

- `V GAME` visits and ticks against Test 71's `2F456` and `7710F9`
  (193,622 and 9.99 s). `V SCI` against `8BD8` and `406C1E` (35,800 and
  5.40 s): the interrupt visits may take a little more, since less is read
  in between.
- `U WORK + U DUE` against `U SEEN`: the part of the calls still passed
  on. `U DUE` cannot exceed 250 a second. `U WORK` holds every call made
  while a stream was being started (Test 71 spent 1.19 s in restarts, some
  3,000 calls) and one for each stop for room (`W FULL`, 1,577 in Test
  71). Much more than those together would mean case 3 is firing:
  interrupts are being held off.
- `I SUBMIT` and `I SKIP` against `20C` and `5A4`; the course rows against
  Test 71's (`TF 2E02180C B070703 E15A2D7`).
- `TW` against Test 71's `7110306 308101F 4C300000`: if requests now wait
  longer for data, the 4 ms spacing is leaving the window short and the
  wire, not the CPU, is the limit again.
- `B SUBMIT 14B` and `B SKIP 0`: the first movie must stay as it is.

## Console result

Owner's run, cold CD boot, Card Tools 09c9a4b686d4, raw Toy Commander GDI,
"66 SCI movie playback", CD Music and virtual VMU off. Saved report
`docs/evidence/toy-wire-stream-72/console-report-66-6.txt` (1,024 bytes,
SHA-256 `ade69a23...cb58`), headed `TEST 72`, `BUILD 8c00ede90b9a`.

Owner's observation: "Video 1 is unaffected. If that recent build had any
effect on video 2 playback it's been marginal. Maybe a few more frames here
and there or a shift in where the fluid frames show up."

| | First movie (B) | Second movie (I) |
|---|---|---|
| Frames submitted of the movie's | 331 of 331 | 567 of 1,968 (Test 71: 524) |
| Of those on time and in sequence | 327 | 520 |
| Frames skipped | 0 | 1,401, from frame 26 (Test 71: 1,444) |
| Longest gap between frames | 54 ms after frame 197 | 2.05 s after frame 1,022 |
| Requests that waited for the window | 1, for 22 ms | 158, 3.43 s in all, at most 44 ms |
| Player exit | `7DC` | `7DC` at frame 1,968 |

| Caller | Visits | Time | Each | Longest | Test 71 |
|---|---|---|---|---|---|
| SCI interrupt | 31,335 | 5.07 s | 162 us | 5.46 ms | 35,800, 5.40 s |
| Timer | 7,167 | 0.03 s | 4 us | 15 us | 7,901, 0.03 s |
| The game's calls | 119,274 | 9.67 s | 81 us | 8.56 ms | 193,622, 9.99 s |
| All | 157,776 | 14.77 s | | | 237,323, 15.42 s |

Update calls: 96,961 seen, 9 passed on for work, 39 as due.

The second movie's course, twelve pieces of 5.4 s from its start:

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted (161 is all) | 45 | 2 | 35 | 14 | 24 | 8 | 9 | 7 | 19 | 27 | 162 | 215 |
| Sectors served | 1,712 | 2,480 | 2,352 | 2,416 | 2,368 | 2,336 | 2,416 | 2,384 | 2,496 | 2,416 | 608 | 1,680 |
| Requests waited, s | 0.15 | 0.31 | 0.04 | 0.08 | 0.08 | 0.10 | 0.23 | 0.31 | 1.38 | 0.59 | 0 | 0 |
| Reader in interrupt visits, s | 0.29 | 0.40 | 0.34 | 0.40 | 0.38 | 0.36 | 0.40 | 0.40 | 0.59 | 0.50 | 0.06 | 0.23 |
| Reader in the game's calls, s | 0.52 | 0.69 | 0.73 | 0.69 | 0.65 | 0.67 | 0.67 | 0.67 | 0.71 | 0.73 | 0.50 | 0.69 |
| Streams given up | 11 | 2 | 15 | 14 | 12 | 7 | 8 | 6 | 17 | 22 | 19 | 15 |

Stream: 265 begun, 6,277 full runs, 26,358 overruns in a run, 8,410 timer
waits (3.54 s), 1,594 stops for room, 72,672,179 wire bytes, TMU1, no
fault. Blocks 134,592: 14,771 rebuilt from CRC16, 26 single losses not
explained, 6,616 handed on with lost bytes, 73 with only a CRC byte lost.
Sectors filled from parity 3,056; none beyond the fill, 9 failed their
check. Two clean blocks failed their CRC16. Streams ended: close 11, sector
9, frame 245. Restarts cost 1.46 s in all, at most 32 ms. 10,156 visits ran
out of time. No game read failed.

What the run establishes:

1. The change works as built. Of 96,961 update calls 48 reached the
   reader. The first movie is unchanged.
2. The cost was not where Test 71's finding 4 put it. That finding took
   every visit from the game that did not serve a request for an update
   call. This run separates them: with the update calls gone there are
   still 119,274 visits from the game, taking 9.67 s against 9.99 s. They
   are the game's GD calls, EXEC and CHECK, each of which the low reader
   passes on as a delivery visit that ends in the reader's foreground
   pump: about 1,550 a second, 81 microseconds each inside the pump, for
   some 2,300 requests. The update calls of Test 71 cost about a fifth of
   that each.
3. The four-millisecond call was hardly ever due (39 times): the GD calls
   read from the foreground far more often than that. The spacing has not
   yet been exercised as the only source of foreground visits.
4. The reader's time in the game's calls still does not follow the data:
   0.50 to 0.73 s in every piece, 0.50 s where 608 sectors were served.
5. The game makes more of these calls when each costs it less. Test 71's
   193,622 visits were its GD calls and its update calls together; this
   run has 119,226 GD calls and 96,961 update calls, 216,187 in all, a
   ninth more. The game repeats them in a loop while it waits, and the
   loop turned more often. Time taken from a loop that is waiting is not
   time taken from the decoder. This puts in doubt that the reader's total
   time is what the second movie lacks, which Tests 70 to 72 assumed. In
   the first 54 s it shows 190 of about 1,610 frames with requests waiting
   3.3 s in all; neither waiting for the window nor the reader's measured
   share of the CPU (a fifth) accounts for seven frames in eight.
6. Not established: what the player is waiting for when it skips. The
   existing hooks on the player (its chunk acquisition, its buffer's fill,
   its skip flag at frame 26 in every run, its lag at exit) are recorded
   but have not been in the report since Test 67. Test 73 prints them and
   times the player's own calls.

## Verification

Host and linked checks only; none is a console result.

Source commit `8c00ede90b9a` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-72/`.

| Check | What ran | Result |
|---|---|---|
| Update calls passed on only when needed | `updating()` in `tests/test_toy_wire_integration.c`: the real low reader, auxiliary and stream on the card model, three update calls to every 1.3 ms of a played movie, the reader at the console's measured pace | 1,625 calls: 82 visits on a quiet bus (8 for work, 74 as due), 92 with stalls. Before the window opens no call is passed on. Every request completed with the image's exact bytes |
| The game is served as before | The same scenario in the Test 71 configuration (`update-model-comparison.log`) | Data ready for 21 of 21 frames when asked in both, on a quiet bus and with stalls; 0 of 21 in both under heavy stalls with 45 restarted streams |
| An interrupt that does not come | `overdue()` in the same test: a run ends in an overrun and no interrupt is delivered; and, from `056814484021`, a stream without a timer in a wait | The next update call is passed on as work and answers it; a call just before, with the run still arriving, is not passed on |
| `kui_wire_overdue` | Stream test: throughout its runs (179,998 blocks, 522,230 overruns) the answer is compared with the model's flags | Overdue exactly when an overrun is unanswered, a wait's timer has run out, or a wait has no timer |
| The hook as linked | `linked-update-hook.log`: disassembly of `kui_toy_movie_ahead_update` and `kui_wire_overdue` from the auxiliary's ELF, read against the source | 20 bytes of the caller's stack, calls to four leaf functions, one or two register reads, the gate entered by a tail jump only when a call is passed on |
| The gate and the word loop as linked | `tests/test_toy_wire_cached_stack.py`, `tests/test_toy_wire_word_loop.py` | Unchanged from Tests 70 and 71 |
| Whole reader | `tests/test_toy_wire_integration.c` in all five configurations | Passed; 24,660,721 checks in this one |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All rows fit at a full run's values (994 of 1,012 bytes); the 16 rows the test is read by end at byte 681 |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | 56 of 57 deliberate defects caught at the source commit. The one missed, a wait without a timer not counted as overdue, is caught by the full stream test but not by the check's shortened run; a test added in `056814484021` (tests only) catches it |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `04c6b439...fe9b` |
| Earlier deliveries | Unchanged recipes on this source | Test 71 `73e8f9ed...a26f`, Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Limits. The model's update calls come at an even pace and its interrupts
are delivered at once; the console's player bunches its calls and the game
masks interrupts at times the model does not know. The hook is the same C
source as compiled for the SH-4, read as linked but not run. No original
game executable took part in these checks: the player's context value and
the call pattern come from the earlier console traces.

## Package

`KUI-Toy-Wire-Stream-Test-72-14NOV2026-0731-CST-8c00ede.zip`, 1,802,084 bytes, SHA-256
`d9c828c9b8e1b44374b952c6c4305b6af02c635636dc8b265efb0d741954935a`, prepared 2026-11-14 07:31 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `8c00ede90b9a` | `FC7BBF9F` | `04c6b439...fe9b` | Changed: Test 72, replaces the Test 71 module `4ee37937a022` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 BUILD_ID=8c00ede90b9a build/toy-suite-72/retail-toy-test-72.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 toy-wire-host-tests toy-wire-integration-host-test \
  toy-wire-package-test toy-wire-linked-check
TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 72), CD Music and virtual VMU Off. Confirm `TEST 72` and
`BUILD 8c00ede90b9a` in the saved `/reports/toy/66-N.txt`.
