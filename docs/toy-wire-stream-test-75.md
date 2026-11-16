# Toy Commander Test 75: a clean start, and the reader out of the player's way

Status: delivered 2026-11-14 (build `bf6f1c703add`). Ran on the owner's
console on 2026-11-14, both modules (see Console result). Not accepted:
the second movie shows more frames than it skips for the first time
(1,407 of 1,968) and still stutters every few seconds, with one pause of
nearly two seconds. Kept as the baseline of Test 76
(`docs/toy-wire-stream-test-76.md`).

Test 75 is not one change either. After Test 74's console run the owner
wrote: "I think maybe 2-3 more refinements is all we need, if you can
figure out what's causing the remainder of the stuttering." It carries
three changes and a report that measures what Test 74's could only
estimate. As in Test 74 the rule of one behavior change per console test
is set aside at the owner's request and only so far: each change is a
source commit of its own under a switch of its own, any of them can be
left out of a later build, and the package carries a second module
without the one change that depends on how the processor's cache behaves
on the hardware. If the combined build regresses against Test 74, the
next step is to isolate against Test 74 with those switches, not to add
to it.

Card Tools has no Test 75 entry. The module is installed where it looks
for Test 66 and starts from "66 SCI movie playback".

## Baseline and what the last run showed

Baseline: Test 74, source `e82b31657f69`, module `9e3bb2b2...d4e4`, as
run on the owner's console (`docs/toy-wire-stream-test-74.md`, Console
result). In short:

- The second movie showed 967 of 1,968 frames (Test 73: 513), with 36
  calls of the player's that skipped and showed nothing, 0.44 s each.
- Through the first 43 s requests hardly waited (0.36 s) and the player
  still fell behind: it is short of processor time there. A call that
  shows a frame takes 46.0 ms; a frame has 33.3 ms; the first movie's
  takes 25.5 ms.
- The reader's pump time fell to 12.86 s from 15.12 s, 95.5 microseconds
  a card block, where the instruction counts had promised a quarter to a
  third less. The report could not say where the time is.
- The second movie's window was opened 45 times (Test 73: once), its
  first frame came 0.28 s later and its first skip at frame 10 instead
  of 27.

The decision this test answers: does the second movie start cleanly once
waited requests keep their window; how much of the pausing goes when the
interrupt visits stop reading and the ring is read through the cache;
and where, by the clock, the reader's time is.

## The changes

All behind `TOY_WIRE_STEADY=1` on Test 74's recipe. Each part defaults to
on with it and is left out with `TOY_WIRE_<PART>=0`.

| Commit | Part | Change |
|---|---|---|
| `19d49e0` | base | The switch, the report's layout, four timers inside the reader, the reasons streams and windows ended, the frame of the second movie's first skip. Nothing in what is read, checked or served. |
| `8a3a7e3` | `STAY` | A request the game waits for in a loop of its own keeps the window; the stream serves it. |
| `680950a` | | Two more timers: the stream's service and the sectors' checks. Nothing else. |
| `361fec6` | `LATER` | An interrupt visit with no request waiting answers the stream and leaves the reading to the game's calls. |
| `98f6a74` | `RING` | The block loop reads the ring through the cache, each line dropped from it first. |
| `bf6f1c7` | | What the review of LATER found (a wait begun by a visit that leaves is looked at once more) and STAY's quarter second counted from the last sector. See Review. |

Not changed: every card block's CRC16 is taken and checked; the sector
checks are Test 74's; the low residents are byte-identical to Test 66's
but for the build label; the stream's run and wait policy, the window's
size, the rule for the game's idle calls and the first movie's path are
Test 74's.

### STAY

For some requests the game does not go on while it waits: a loop of its
own (its caller at `0x8c09712a`) calls EXEC and CHECK until the data is
there. The resident tells such a call apart from the game's polls and
passes it on with a wait count, on which the low reader reads at once
inside the call: the only way to make progress where no interrupt may
come. `toy_movie_ahead` therefore gave the window up at the first such
call for a request it could not serve yet, and the low reader read the
request.

The stream needs no interrupt to go on. `kui_wire_service` looks at the
receiver, the channel and the wait timer's flag on every visit, so each
call of the game's loop accounts a run that ended, begins the next and
reads what has arrived. With STAY the request keeps a window that stands
at its first sector, and is served from it, usually on arrival because
the stream has read ahead.

The fixture `waited_for()` drives eight requests the way that loop does,
with the interrupt level held off throughout (the wire runs on, nobody
is told). Without STAY (Test 74, and Test 75 with the part left out):
eight windows opened, eight given up as waiting, the low reader read
them all. This is the pattern of Test 74's console run, which opened 45
windows for the second movie. With STAY: one window, one stream, one
CMD18, no CMD12, nothing given up.

A wait the stream does not end is not left to it. `toy_wire_stay` notes
the request's token, the window's end and the clock. Once the window has
not grown for 195,312 ticks (a quarter of a second) it is given up as
before and the low reader reads the request (`waited_in_vain()`). While
sectors keep coming the wait goes on and what has been read is not
thrown away (`waited_slowly()`); the quarter second counts from the
game's first call that finds the window grown. Without the clock that
bounds it, or for a request that is not the window's next, nothing is
kept (`waited_elsewhere()`).

What it does not change: a request's first visit (the EXEC behind its
REQUEST) opens or serves as before; a waited request that finds no
window does not open one.

### LATER

Up to Test 74 every SCI interrupt visit ran the whole pump: account the
run that ended, begin the next, then read blocks out of the ring into
the window for up to 390 ticks. On the console that was 28,610 visits of
238 microseconds, about three blocks each.

What the interrupt has to do is the first part, so that the wire goes
on. The reading is in no hurry while no request waits for it: the game's
calls come every few milliseconds (the rule of Tests 72 and 73 passes
one in four milliseconds on, and at once when the reader needs the
foreground) and read in larger pieces.

With LATER an interrupt visit with no request waiting calls
`toy_wire_later`: it serves the stream and returns without reading while

- the stream then receives or waits out a busy bus (`RUN` or `WAIT`),
- fewer than 16,384 of the ring's 32,768 bytes are unread, and
- fewer than 128 of the 256 entries of the stream's log of lost
  positions are outstanding.

Otherwise, and whenever a request waits, it reads as before, now for as
long as a call of the game's may (1,172 ticks; the 390 of an interrupt
visit without a waiting request are gone). The second and third
conditions keep the stream from standing still for a reader that is not
coming: a ring half unread leaves room for two more full runs, and the
log's mark was added when the fixture `later_standstill()` showed the
log filling on a very busy bus long before the ring.

A wait that the visit's own look at the stream has just begun is looked
at once more before the visit leaves, as the reading always did between
blocks. Without a timer that second look is what ends the wait; with
one it is where the timer's interrupt level is given back if the SDK
has taken it away. Both were found by the review (see Review) and are
the fallback paths: every console report so far shows a timer in use
(`TMR` 1) and its visits arriving.

Fixtures: `later_visits()` goes visit by visit over a bus that cuts a
run every two kilobytes (58 visits left the reading with up to 16,303
bytes unread, 27 read; a call of the game's four milliseconds on read
what they had left; 7 requests waited and no visit left meanwhile);
`later_standstill()`; `later_waits()` with and without a timer;
`later_fault()` (a stream whose channel was taken is ended by the visit
that finds it so). The older `playing()` fixture,
interrupts only at the console's pace, had no frame's data ready in Test
74 and has every frame's in Test 75, because an interrupt visit that
reads at all now reads for 1,172 ticks.

### RING

The DMA channel writes the ring behind the cache's back, so the reader
looked at it through its uncached address (P2): one bus read for every
word, 129 for a block's data. Through the cache (P1) a line of 32 bytes
comes in one burst.

`kui_wire_fresh` gives the block loop the cached address of a word run
after dropping every cache line the run touches (`OCBI`), so that each
is fetched afresh from memory when the loop reads it. All the run's
bytes have landed by then: `kui_wire_next` takes a block only when the
channel's address, read behind a bus fence, is past its last byte. Bytes
of the same line that land later belong to a later run, which drops the
line again. Nothing is ever written through the cached address, so a
drop loses nothing. Runs shorter than a line (eight words), the search
for the token, the lost-position bookkeeping and the two CRC bytes stay
at the uncached address. The held byte the reader itself stores into the
ring at a stop goes through the uncached address as before.

The CRC16 is taken over the bytes as the loop read them, as it always
was. A byte read stale would fail it and the stream would be restarted
(`S` row, `BC`), not served.

What rests on the hardware and cannot be shown here: that `OCBI` drops
the line and the next read then comes from memory. This is the SH-4's
documented behaviour and the module already uses `OCBP` and `OCBI` on
the console (the Test 70 report read `CCR` as `0x105`: the operand cache
on, P1 copy-back, the MMU off). Whether a line fetch is cheaper than
eight bus reads here is the measurement: `P RING` by `W BLK`, against
the second module's.

### The timers

`toy_wire_phase()` reads TMU0 where the reader did not read it before:
five times a block (the stream's service, a block out of the ring, a
block fed), twice a sector checked, twice a request copied out, twice a
reopening. `P RING FEED CHK OUT SVC` and `D`'s `SEEK` are the sums.
`CHK` is inside `FEED`. What is left of `V`'s pump time after `RING`,
`FEED` and the part of `SVC` spent in pumps is the pump's own: entry,
the request state, the bookkeeping around each block.

### Considered and left out

- A hand-written copy for the window and for the copy out to the game
  (`MOVCA.L` for whole destination lines). By estimate under one percent
  of the processor, the copy out would have to go around the resident's
  own writer, and it would have left no room in the auxiliary. `P FEED`
  and `P OUT` say what the copies cost.
- Following a stream past the position that is not a lost byte (Test 71,
  finding 5) instead of restarting it: Test 74's run began about 310
  streams beyond its windows' own. The `S` row now counts them by kind;
  the retry it would take is a change to the block reader of its own.
- A shorter gate for the resident's two trace visits around each GD
  call. Test 74 measured such a call whole at 29 microseconds including
  the pumps some of them run; the gate's share is small.
- The CRC16 four bytes to a step, and a longer spacing of the game's
  calls that read.

## Estimate

Estimates, not measurements, and the last three of this kind were too
hopeful.

- STAY: the second movie's windows from 45 to a few, its first frame
  about where Test 73 had it, and its start with the player's buffer
  full, which Test 73's hold did not give it.
- LATER: most of the 28,610 interrupt visits answer the stream and
  leave. Their reading moves to the game's calls, in fewer and larger
  pieces. One to two and a half percent of the processor, mostly from
  the caches.
- RING: 129 bus reads against 17 line fetches a block. If a bus read
  costs 80 to 170 ns and a line fetch 130 to 250 ns, 6 to 13 of a
  block's 95 microseconds: one to two percent of the processor. If a bus
  read is cheaper than that, nothing.

Together two to five percent. The player needs about six (2 of a
frame's 33 ms). So: fewer and shorter pauses, probably not none.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The three parts, the timers and the report's counters. Image 24,256 of 24,576 bytes: 320 are left. Without `RING` 24,160. |
| `native-worker` | The report's rows and title. The two modules of the package differ here in one constant, the `PT` value. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1
TOY_WIRE_STEADY=1`. Without `TOY_WIRE_STEADY` the Test 74 module
reproduces byte for byte.

Code space, bytes of the auxiliary's text: base -588 (the chunk observer
no longer does anything; the first four timers), STAY +132, the two
timers +68, LATER +96, RING +104, the review's changes +32.

## Known and not changed here

1. Streams that cannot be followed (see above).
2. The auxiliary's context page at its uncached address.
3. The gate and the dispatcher are entered three times for each of the
   game's EXEC and CHECK calls.
4. The review findings listed in `docs/toy-wire-stream-test-69.md`.
5. `test-sci-stream-quarantine` stops on its fingerprint of
   `src/loader/sci_stream.c` as it has since before Test 73; Tests 68 to
   75 do not touch that file or that test.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 75`. In order:

| Row | Meaning |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | As Test 74, then the requests the game waited for that kept their window, and the parts the module has: 1 `STAY`, 2 `LATER`, 4 `RING`, added (7 the main module, 3 the second) |
| `AHEAD`, `I SUBMIT`, `B SUBMIT`, `IP`, `BP`, `K`, `V` | As Test 74. `V`'s interrupt count has every interrupt visit, also those that left the reading |
| `I FIRST LAST SKIP EXIT SF` | As Test 74 without `WF`. `SF` is the second movie's frame at the player's first skip, by the player's own frame count plus one, taken in the update hook |
| `P RING FEED CHK OUT SVC LATER` | Ticks taking blocks out of the ring; feeding them to the window; in the sectors' checks (inside `FEED`); copying requests out; in the stream's service; then the interrupt visits that answered the stream and left the reading |
| `D N T G N T SEEK` | As Test 74, then the ticks of the low reader bringing the card to a stream's first block |
| `TF`, `TD`, `TW`, `W START`, `W BYTES`, `W BLK` | As Test 74 |
| `S C S F LF AM BC` | Streams ended because the window closed, a sector failed its check, the reader could not follow; then the blocks' own counts of the last kind: a byte between blocks neither idle nor token, a token lost among lost bytes, a block without lost bytes failing its CRC16 |
| `X E N I C W O` | Windows given up, by the first reason that held: error, movie over, request state no longer a finished read, a command that is not a read, a waited request (with `STAY` one waited for in vain, or without a clock), other |
| `W FILL REF BAD` | Sectors completed from parity, sectors the parity could not complete, sectors that failed their check |
| `TI`, `TG` | As Test 74 |

Gone to make room: `IC`, `U`, `R`, `WF`, and `W FILL`'s byte counts. The
chunk observer behind `IC` and `WF` does nothing in this build.

## Reading it

- `PIPE`: Test 74 opened `A` and `2D` windows. `STAY` is how many waited
  requests kept theirs; `X`'s `W` how many still cost one.
- `I FIRST` against `D2CBD` (1.105 s), `SF` against `A`, `I SUBMIT`
  against `3C7`, `IP`'s `SN` against `24` and `OT / ON` against 46.0 ms.
- `B SUBMIT 14B`: the first movie must stay as it is.
- `V`'s three tick values added, by `W BLK`'s first: Test 74 74.6 ticks,
  95.5 microseconds a block. `V`'s first tick value and `IP`'s `OI`
  should fall and `V`'s third rise: the reading has moved.
- `P LATER` by `V`'s first count: the share of interrupt visits that
  left the reading.
- `P RING` by `W BLK`'s first: the block loop's ticks a block. The same
  from the second module's run is the loop at the uncached address.
- `P FEED` less `CHK`, by blocks: the copy to the window and its
  bookkeeping. `CHK` by `K`'s `LIGHT` plus `EDC`: a sector's check.
  `OUT` by `AHEAD`'s `HIT`: a request copied out.
- `P SVC` by `W START SEG CUT` added: what answering a run costs.
- `S`: `F` against `W START` less `PIPE`'s windows is the share of
  streams begun again because one could not be followed; `LF`, `AM`,
  `BC` say what was met.

## Console result

Owner's runs of both modules, reports received 2026-11-14. The owner
reported no deviation from the package's procedure (cold CD boot, Card
Tools 09c9a4b686d4, raw Toy Commander GDI, "66 SCI movie playback", CD
Music and virtual VMU off). Saved reports, each 1,024 bytes and headed
`TEST 75`, `BUILD bf6f1c703add`:

- main module: `docs/evidence/toy-wire-stream-75/console-report-66-9.txt`,
  SHA-256 `410e5168...a764`, `PT` 7;
- second module (no RING):
  `docs/evidence/toy-wire-stream-75/console-report-66-9-alternate.txt`,
  SHA-256 `e7f67d65...6e8d`, `PT` 3.

Owner's observation: "SO CLOSE! There are more fluid frames than there are
missed frames. [...] The alternate wasn't as good, of course, but still
better than last run. The first seconds of the second movie started
stuttering a couple seconds in, was fluid, then every few seconds it
would stutter again. It's honestly really good compared to where we
started."

| | Main | Second (no RING) | Test 74 |
|---|---|---|---|
| First movie: frames submitted / on time | 331 / 330 | 329 / 326 | 331 / 330 |
| Second movie: frames submitted of 1,968 | 1,407 | 874 | 967 |
| Of those on time and in sequence | 1,283 | 764 | 856 |
| Frames skipped, first at frame | 561, 37 | 1,094, 43 | 1,001, 10 |
| First frame after | 0.802 s | 0.808 s | 1.105 s |
| Longest gap between frames | 1.91 s after 1,441 | 1.98 s after 1,441 | 1.95 s after 1,441 |
| Windows opened, first / second movie | 2 / 2 | 2 / 2 | 10 / 45 |
| Waited requests that kept their window | 52 | 53 | |
| Sectors served from the window | 3,423 / 25,827 | 3,421 / 25,827 | 3,403 / 25,770 |

The player's own update calls:

| | Main | Second | Test 74 |
|---|---|---|---|
| Second movie: calls that put a frame out | 1,395 | 863 | 956 |
| Time inside them, each | 56.49 s, 40.5 ms | 44.32 s, 51.4 ms | 43.99 s, 46.0 ms |
| Reader's interrupt pumps in that span, each | 2.67 ms | 4.47 ms | 4.42 ms |
| Calls that skipped and put nothing out | 34, 3.55 s, 0.104 s each | 39, 15.82 s, 0.406 s each | 36, 16.01 s, 0.445 s each |
| First movie: calls that put a frame out, each | 330, 25.8 ms | 328, 26.8 ms | 330, 25.5 ms |

The reader:

| | Main | Second | Test 74 |
|---|---|---|---|
| Visits from the SCI interrupt | 29,617, 4.52 s | 29,928, 6.02 s | 28,610, 6.80 s |
| Of those, left the reading | 15,135 | 14,176 | |
| Pumps from the game's calls | 14,147, 7.13 s | 14,116, 7.57 s | 12,854, 6.03 s |
| Timer visits | 8,834, 0.04 s | 8,916, 0.04 s | 5,600, 0.03 s |
| All pumps | 11.69 s | 13.63 s | 12.86 s |
| Card blocks, pump time for each | 135,381, 86.3 us | 135,388, 100.7 us | 134,740, 95.5 us |
| Sectors accepted on CRC16 and address / through the EDC | 22,657 / 6,643 | 22,703 / 6,594 | 23,788 / 5,428 |
| Rebuilt bytes their P code word confirmed / did not | 8,895 / 0 | 9,447 / 0 | 13,051 / 0 |
| Sectors that passed the lighter check and failed the EDC watch | 0 | 0 | 0 |
| Delivery visits on the whole path | 8,024, 10.62 s | 7,991, 9.88 s | 9,197, 10.15 s |
| The game's EXEC and CHECK calls, entry to exit | 139,815, 4.19 s, 30.0 us each | 130,828, 4.57 s, 34.9 us each | 144,733, 4.26 s, 29.4 us each |

The reader's pump time by what it was doing, microseconds for each card
block (the timers this test added):

| | Main | Second |
|---|---|---|
| Taking the block out of the ring: token, CRC16, bytes | 33.1 | 49.4 |
| Feeding it to the window, without the sectors' checks | 17.9 | 18.5 |
| The sectors' checks | 7.1 | 7.0 |
| The stream's service | 3.6 | 3.6 |
| The low reader bringing the card to a stream's first block | 7.1 | 6.6 |
| The rest of the pumps | 17.6 | 15.5 |
| All | 86.3 | 100.7 |

Outside the pumps: copying requests out to the game took 1.23 s (1.25 s),
42 microseconds for each 2,048-byte sector. A sector's check took 33
microseconds on average over all sectors. Reopening took 0.96 s (0.89 s),
2.0 ms for each stream begun.

The second movie's course in the main module's run, twelve pieces of
5.4 s from its start:

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted (161 is all) | 121 | 109 | 146 | 109 | 120 | 119 | 54 | 93 | 120 | 40 | 161 | 215 |
| Second module | 68 | 84 | 86 | 46 | 50 | 29 | 14 | 61 | 42 | 34 | 161 | 199 |
| Test 74 | 49 | 93 | 91 | 47 | 54 | 46 | 25 | 59 | 92 | 26 | 161 | 224 |
| Sectors served | 1,936 | 2,400 | 2,304 | 2,416 | 2,400 | 2,320 | 2,448 | 2,384 | 2,464 | 2,400 | 608 | 1,680 |
| Requests waited, s | 0.08 | 0.04 | 0.00 | 0.08 | 0.04 | 0.02 | 0.04 | 0.00 | 1.26 | 0.52 | 0 | 0 |
| Second module | 0.06 | 0.10 | 0.02 | 0.10 | 0.06 | 0.06 | 0.15 | 0.08 | 1.43 | 0.61 | 0 | 0 |
| Reader in interrupt visits, s | 0.27 | 0.34 | 0.34 | 0.36 | 0.34 | 0.34 | 0.31 | 0.36 | 0.50 | 0.38 | 0.08 | 0.29 |
| Reader in the game's calls, s | 0.34 | 0.48 | 0.42 | 0.52 | 0.46 | 0.48 | 0.48 | 0.50 | 0.50 | 0.55 | 0.29 | 0.48 |

Stream, main module (second module, Test 74's main module): 478 begun
(454, 365), 7,069 full runs (7,028, 6,569), 23,825 overruns in a run
(24,394, 23,226), 10,930 timer waits of 4.27 s (10,265 of 4.01 s, 6,556
of 3.36 s), 1,515 stops for room (1,481, 1,631), 75,825,683 wire bytes
(76,277,502, 73,303,443), TMU1, no fault. Blocks 135,381: 11,194 rebuilt
from CRC16 (12,000, 16,838), 22 single losses not explained (18, 25),
6,667 handed on with lost bytes (6,687, 4,270), 52 with only a CRC byte
lost (50, 66); 527 read by the low reader to open or reopen (490, 384).
Streams ended because the window closed 4, a sector failed its check 12,
the stream could not be followed 462: a byte between blocks neither idle
nor token 460, a block without lost bytes failing its CRC16 2 (second
module 4, 9, 441: 438 and 3). Windows given up: two because the movie was
over, two as not serving the request (one at each movie's start), none
for a waited request. Sectors completed from their parity 3,918 (3,784,
1,925); 4 refused (0, 1), 8 failed their check and were read again (9,
4). No game read failed.

What the runs establish:

1. The second movie shows more frames than it skips: 1,407 of 1,968,
   against 967. The pauses are a quarter as long: 34 calls of the
   player's that show nothing, 0.10 s each, against 36 of 0.45 s. What
   remains is the stutter the owner describes, a pause of 1.9 s near the
   49th second, and two pieces of the course that are far worse than the
   rest (6 and 9, with 54 and 40 frames of 161).
2. STAY did what it was for. Two windows for each movie where Test 74
   opened 10 and 45, no window given up for a waited request, the second
   movie's first frame after 0.80 s again and its first skip at frame 37
   instead of 10.
3. RING is where the gain is. The two modules differ in nothing else:
   33.1 against 49.4 microseconds for a block out of the ring, 86.3
   against 100.7 for a block in all, 1,407 frames against 874. Nothing
   suggests a stale read through the cache: two blocks in 135,000 failed
   their CRC16 whole and eight sectors their check, against three and
   nine in the module that reads the ring as before. The estimate was 6
   to 13 microseconds a block; it is 16.
4. LATER did not do what it was for, and it cost the stream. Half the
   interrupt visits left the reading (15,135 of 29,617), yet the
   reader's interrupt time for each frame call is where Test 74 had it
   in the module without RING (4.47 ms against 4.42): the visits that
   read now read for longer. That module's pumps took 13.63 s against
   Test 74's 12.86 s and it showed fewer frames (874 against 967). The
   stream lost no more bytes than before (23,825 and 24,394 overruns
   against 23,226 and 24,253), but the losses came closer together:
   overruns behind a run of under 128 bytes rose from 28 and 30 percent
   to 46 and 42, blocks handed on with lost bytes by a half, sectors
   completed from parity twofold, sectors through the EDC by a fifth,
   streams begun again beyond the windows' own from 310 and 282 to 474
   and 450, wire bytes by 3 to 5 percent. Test 74's own report says why
   that follows from a visit that leaves: a run begun by a visit that
   stayed to read was cut again within 128 bytes 18 times in 100 (2,980
   of 16,670), one begun by a timer visit, which leaves at once, 52
   times (3,378 of 6,556). While the reader holds the processor the
   game's code that stalls the bus does not run. This is an inference:
   no run has LATER off with STAY on, and Test 75's report no longer has
   that row. Test 76 leaves LATER out, and its second module is this
   test's with nothing else changed, which is that comparison.
5. Where a block's 86 microseconds are, by the clock: 33 in the ring
   loop, 18 feeding the window, 7 in the sectors' checks, 4 in the
   stream's service, 7 reopening streams, 18 in the rest. The rest is
   not bookkeeping alone. It holds what each of 52,598 visits costs
   before and after its blocks, and a wait no timer covers: a stream
   that is given up while it receives is stopped by waiting, with
   interrupts held off, for its run's next overrun, which is up to one
   segment away (8,192 bytes, 5.2 ms). The run gave up 478 streams. By
   estimate from the counts, not measured: half a second to a second.
   With the 0.96 s of reopening, giving streams up and beginning them
   again costs 1.5 to 2 s of the reader's 11.7 s.
6. Copying requests out to the game is 1.23 s, 42 microseconds a sector,
   one and a half percent of the processor. Test 75's document put the
   two copies together at under one percent and left a faster copy out
   for that reason. That estimate was wrong.
7. The two bad places are not alike. In piece 8 (43 to 48 s) requests
   waited 1.26 s and 1.43 s, and 1.2 to 1.6 s in every run since Test 71
   whatever the reader's speed and however many frames that piece
   showed (120 here, 42 in the second module, 14 in Test 73). The
   longest gap follows it directly: frame 1,441 is due 48.8 s into the
   movie, in piece 9. There the reader does not bring the data fast
   enough, and a faster reader has not changed that in five tests. Test
   67 found that the movie's densest second needs more than the wire
   carries (1.57 MB/s of raw image against 1.56); where in the movie
   that second lies was not recorded. In piece 6 (32 to 38 s,
   around frame 1,021, where Tests 70 to 73 had their longest gap) 54
   frames of 161 were shown with 0.04 s of waiting: there the player is
   short of processor time, not of data. What the player's own buffer
   holds at either place this report does not say; Test 76's does.
8. How much is missing. The time inside the player's update calls is
   the same in every run (60.0 s of the movie's 65.6 s; Test 73 60.1 s):
   the player takes all there is and processes every frame, shown or
   not. Between the two modules 1.69 to 1.94 s less reader time went
   with 533 fewer skipped frames, 3.2 to 3.6 ms a frame; counting the
   0.58 s less that requests waited, up to 4.7 ms. At that rate the 561
   frames still skipped stand for 1.8 to 2.6 s of the reader's time in
   the movie's first 54 s, where the pumps take 8.3 s and the copy out
   about 1.0 s. Fitted on these two runs the same line misses Tests 73
   and 74 by up to 120 frames. And about 280 of the 561 are in pieces 6,
   8 and 9, where a frame may not come back at that price.
9. Beyond RING the two modules' stream counts are alike. The second
   module's first movie lost frames (329 submitted, 326 on time, one
   call that skipped), the first time since Test 70. The owner found
   that module better than Test 74's run; by frames shown it is not (874
   against 967), by the length of its pauses about the same.

## Verification

Host and linked checks only; none is a console result.

Source commit `bf6f1c703add` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-75/`.

| Check | What ran | Result |
|---|---|---|
| Whole reader, each part | `tests/test_toy_wire_integration.c`: the real low reader, auxiliary and stream on the card model, the whole scenario list twice (a receiver that ignores the bit order request and one that honours it) | Passed in Test 75 (241,063,419 checks) and with each part left out in turn; every request completed with the image's exact bytes |
| STAY | `waited_for()`, `waited_in_vain()`, `waited_slowly()`, `waited_elsewhere()`: the game's own waiting loop, interrupt level held off throughout (`model-part-by-part.log`) | Eight waited requests: without STAY eight windows opened and given up and the low reader read them all; with it one window, one stream, one CMD18, no CMD12. A wait in vain given up a quarter second after the last sector, to the tick; one without a clock, or not at the window's first sector, at once |
| LATER | `later_visits()`, `later_standstill()`, `later_waits()`, `later_fault()`, `playing()`, `steady()` | Visit by visit: 58 left the reading and read nothing, with the stream receiving and under both marks; 27 read; a call of the game's read what they had left; no visit left while a request waited. A wait begun by a leaving visit is ended by it without a timer, and the timer's interrupt level given back with one. Requests served on arrival unchanged against Test 74 (21 of 22 on a quiet bus) |
| RING | Both host tests against a view of the ring that keeps what memory held at each line's last drop | 3,042,510 lines dropped and fetched afresh for 179,992 blocks in the stream test, 5,056,066 in the reader test; nothing read stale |
| RING as linked | `tests/test_toy_wire_linked_ring.py` with the interpreter of `tests/sh4_mini.py` (`linked-checks.log`) | 6,096 runs at four ring addresses, every alignment and length, the ring's two ends: each line of a run dropped once by `OCBI` and nothing else; runs under eight words left at the uncached address; no memory but the stack touched |
| The timers | `visits_accounted()` on every scenario: each phase span against the fixture's own look count | Exact for ring, feed, check, copy-out and service; all inside the visits |
| The hand-written loops and the gate as linked | `tests/test_toy_wire_linked_loops.py`, `tests/test_toy_wire_cached_stack.py` | As Test 74: 41 and 34 instructions for four bytes, the interpreter agreeing with the compiler's loop of the Test 73 module |
| Card stream | `tests/test_toy_wire_stream.c` | 179,992 blocks checked against the card over 524,878 overruns; none handed on as clean or rebuilt had a wrong or missing byte |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All rows fit at a full run's values (1,008 of 1,012 bytes); the rows the test is read by end at byte 925 |
| All configurations | Both host targets in Tests 68 to 74, Test 74 with each part left out, Test 75, Test 75 with each part left out, without the bit order request and with all three left out (`matrix.log`) | 36 of 36 passed |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | 167 of 167 deliberate defects caught, 45 of them in what only Test 75 has. The lists of Tests 73 (79) and 74 (125) still apply to this source |
| Other suites on the shared wire model | As for Test 74 (`other-suites.log`) | 17 of 18 passed. `test-sci-stream-quarantine` stops on its fingerprint of `src/loader/sci_stream.c` as at Test 73's and Test 74's commits; not caused by this change and not repaired here |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes of both modules | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; the same in both modules |
| Reproducible | Clean worktree of the commit | Module SHA-256 `9ef4c29b...eb08`, second module `52423cf2...da1f` |
| Earlier deliveries | Unchanged recipes on this source | Test 74 `9e3bb2b2...d4e4` and its second module `582e2d7a...d86c`, Test 73 `7f28c744...7124`, Test 72 `04c6b439...fe9b`, Test 71 `73e8f9ed...a26f`, Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

What the verification itself found and changed, before anything was
delivered:

- The first timers made the fixture's reader four times slower: its
  clock moves with every look. The timers' looks now go through a hook
  of their own in host builds, so the reader's pace in the model is Test
  74's to the tick.
- `later_standstill()` showed the stream's log of lost positions filling
  long before the ring's mark on a very busy bus, with the stream then
  standing still for a reader that was not coming. LATER got its second
  mark from that.
- Three of STAY's first nine mutations and one of LATER's eleven were
  not caught; the fixtures were tightened until they were (a window
  standing elsewhere turned out to have no reachable mutation and is
  recorded as such).
- The mutation of the pump's own count of visits out of time was being
  made on a line LATER does not compile; it is made on LATER's line now.

Limits. No host test can say what the SH-4's cache does: the view of the
ring in the fixtures is a model of the documented behaviour, stricter
than the hardware in that it never lets go of a line. The linked routine
was run by an interpreter written for this work, not on a console. The
model's stalls are drawn at rates like the console's, not recorded from
it. The game's waiting loop is modelled by its caller's address and the
calls it makes; the game's executable was not available to these checks.
Phase times in the model are looks at a clock, not times.

## Review

Three independent reviewers, one for each part, read the source at
`98f6a74` under Test 75's switches for concrete failure cases. They did
not build or run anything. Their answers are in
`docs/evidence/toy-wire-stream-75/review.md`.

| Part | Found | Disposition |
|---|---|---|
| RING | Nothing. 25 hazards examined, among them the pieces split at lost positions, the data and CRC split, the ring's wrap and second lap, the held byte, the assembly loops' reads, the linked routine's arithmetic, and index conflicts in the 16 KiB cache (speed only) | No change |
| STAY | Nothing. Remarked that the quarter second ran from a wait's first call, so that a request the stream was slow to finish would be given up with what had been read thrown away, once for each request | Changed in `bf6f1c7`: the quarter second counts from the last sector |
| LATER | Two, both for a wait that begins in a visit which then leaves: with no free timer nothing ended it until the game's next call (certain); with a timer, the look that gives the timer's interrupt level back when the SDK has taken it was no longer made (possible) | Fixed in `bf6f1c7`: such a visit looks at the wait once more, as the reading did. Fixture `later_waits()` for both; the mutation that takes the look away is caught |

Neither LATER case has been seen on the console: every report so far
shows a timer in use and its visits arriving. They were real in the
code all the same.

This review was small on purpose (three readers). Earlier, larger
multi-agent reviews in this work ran out of their usage allowance before
finishing.

## Package

`KUI-Toy-Wire-Stream-Test-75-14NOV2026-1944-CST-bf6f1c7.zip`, 1,978,231 bytes, SHA-256
`b96b728ac59771a2ecd5565e9b71a09288d8aa04554829fc151037b0841907cf`, prepared 2026-11-14 19:44 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `bf6f1c703add` | `59E5F4C4` | `9ef4c29b...eb08` | Changed: Test 75, replaces the Test 74 module `e82b31657f69` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |
| `ALTERNATE/KUI/apps/games/retail-toy-test-66.kui` | `bf6f1c703add` | `3C95285E` | `52423cf2...da1f` | Outside the `KUI/` tree: Test 75 built with `TOY_WIRE_RING=0`, as a fallback and as the comparison for RING |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

The two modules carry the same build label. The report tells them apart
by `PIPE`'s last value: 7 the main module, 3 the second.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 \
  BUILD_ID=bf6f1c703add build/toy-suite-75/retail-toy-test-75.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 \
  toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test toy-wire-linked-check \
  TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
python3 tests/test_toy_wire_stream_mutations.py
```

The second module: the first command with `TOY_WIRE_RING=0` and another
`BUILD` directory.

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 75), CD Music and virtual VMU Off. Confirm `TEST 75` and
`BUILD bf6f1c703add` in the saved `/reports/toy/66-N.txt`, and `PIPE`'s
last value.
