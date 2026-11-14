# Toy Commander Test 73: the game's idle GD calls, and the player itself in the report

Status: delivered on 2026-11-14 as build `e623c2a697d5`. Host and linked
checks only; it has not run on a console. The owner's console report is
pending.

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

Pending.

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
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | Still running when this record was committed: 48 of the 79 deliberate defects had run and 0 were missed. The final count follows in a later commit |
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
