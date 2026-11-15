# Toy Commander Test 76: what the clocks in Test 75 pointed at

Status: delivered 15 November 2026 (build `7fe7e83c2082`) and run on
the owner's console the same day: 1,942 of the second movie's 1,968
frames, the first movie whole (Console result). A test module for one
title, not an accepted component.

Test 76 is not one change. After Test 75's console run the owner wrote:
"SO CLOSE! [...] I think the next build will be the winner", and after a
restart of the session: "Continue on. Hopefully we can finish this?" It
carries six changes and three new measurements. As in Tests 74 and 75
the rule of one behavior change per console test is set aside at the
owner's request and only so far: each change is a source commit of its
own under a switch of its own, and the package carries a second module
that is the baseline with one switch changed. A part can be left out of
a later build, with one limit that the full build's size sets (Module
roles). If the combined build regresses against Test 75, the next step
is to isolate against Test 75 with those switches, not to add to it.

Card Tools has no Test 76 entry. The module is installed where it looks
for Test 66 and starts from "66 SCI movie playback".

## Baseline and what the last run showed

Baseline: Test 75, source `bf6f1c703add`, module `9ef4c29b...eb08`, as
run on the owner's console (`docs/toy-wire-stream-test-75.md`, Console
result). In short:

- The second movie showed 1,407 of 1,968 frames (Test 74: 967), with 34
  calls of the player's that showed nothing, 0.10 s each, and one gap of
  1.9 s after frame 1,441.
- RING was the gain: 33 against 49 microseconds for a block out of the
  ring. LATER moved none of the reading out of the player's frame calls
  and made the stream's losses come closer together.
- By the clock a card block cost the reader 86 microseconds in pumps: 33
  in the ring loop, 18 feeding the window, 7 in the sectors' checks, 4
  in the stream's service, 7 reopening streams, 18 in the rest. An
  independent check of that account found in the rest a wait no timer
  covered: a stream given up while it receives was stopped by waiting
  out its run.
- Two bad places that are not alike. Around 32 to 38 s the player is
  short of processor time with the data there. From 43 to 48 s requests
  waited 1.3 s, as in every run since Test 71, and the 1.9 s gap
  follows.
- By the two runs' own arithmetic the 561 frames still skipped stand for
  1.8 to 2.6 s of the reader's time in the movie's first 54 s, where the
  reader takes about 9.3 s (pumps 8.3 s, the copy out 1.0 s).

The decision this test answers: how much of the stutter goes when the
reader's work on each block is cut where the clocks said it is; whether
LATER's cost to the stream was real (the second module); and, by the new
rows, what the player's own buffer and the wire do in the two bad
places.

## The changes

All behind `TOY_WIRE_FLUID=1` on Test 75's recipe. Each part defaults to
on with it and is left out with `TOY_WIRE_<PART>=0`. `LATER` defaults to
off with it and is put back with `TOY_WIRE_LATER=1`.

| Commit | Part | Change |
|---|---|---|
| `1861085` | base | The switch; `LATER` off by default; the report's layout: the ticks spent ending streams, and by piece of the second movie's course the player's buffer and the wire's standstill. Nothing else in what is read, checked or served. |
| `697453d` | `QUIT` | A stream given up while it receives is ended at once. |
| `d585ae0` | `HOT` | The window's own state, with the cursor the low reader's feed works on, is reached through the cache. |
| `b78c4e5` | `COPY` | A block goes into the window a cache line at a time, the line claimed without being fetched. |
| `3893bef` | `GAP` | The search for a block's token reads the ring through the cache. |
| `6a39343` | `APART` | The buffer a block is taken into shares no place in the cache with the reader's stack or with its bytes in the ring. |
| `7fe7e83` | | What the review found (see Review): QUIT first asks whether the channel is still the stream's; APART's two buffers lie before and behind the tables, all clear of the stack's lines; the report prints the capacity of the player's buffer. The modules are built from this commit. |
| `4be776b`, `61e24e9` | | Tests only: what the stream's host test says about blocks handed on wrong, and how it judges a block (see Review). No source of the module. |

Not changed: every card block's CRC16 is taken and checked; the sector
checks are Test 74's; the low residents are byte-identical to Test 66's
but for the build label; the stream's run and wait policy, the window's
size, STAY, the rule for the game's idle calls, the copy out to the game
and the first movie's path are Test 75's.

### LATER left out

With `LATER` off an interrupt visit reads what the stream has brought,
for up to 390 ticks unless a request waits, exactly as in Test 74. Test
75's finding 4 has the counts. What they suggest is that a visit which
stays to read keeps the game's code that stalls the bus from running
while the freshly begun run gets under way: in Test 74 a run begun by a
visit that stayed was cut again within 128 bytes 18 times in 100, one
begun by a timer visit, which leaves at once, 52 times. That is an
inference. The second module of this package is Test 75's recipe with
`TOY_WIRE_LATER=0` and nothing else, which is the comparison.

### QUIT

`kui_wire_stop` has to leave the card on a whole byte, and the receiver
ends on one only at an overrun. For a stream that was receiving it
therefore waited, with interrupts held off, for the running run's next
overrun: a cut, or the run's end, up to one segment of 8,192 bytes away,
5.2 ms. Test 75's console run gave up 478 streams, 462 of them because
the reader could not follow.

With QUIT the channel is taken off the receiver first (the enable bit of
`CHCR1` cleared). The next byte then stays in the receiver's register,
the one after it overruns, and the receiver stops on that byte's last
bit as at any cut: two byte times. The rest of the stop is the code that
has answered every cut since Test 68: the channel's address and count
are read as its last whole transfer left them.

The stream test's register model clocks a byte for every fourth look
at the receiver's status (every look, until the review asked what a wait
of two looks would show there). 1,111 streams given up while receiving
cost 2,220 bytes of wire with QUIT, at most two each; without it 1,162
cost 395,634, up to 8,193 each, which is a whole run and the two bytes
behind it. (On average 340 bytes a stop: the model's bus is busier than
the console's and its runs shorter.)

What rests on the hardware: that the channel, with its enable bit
cleared while the receiver still asks, finishes the transfer it is in
and takes no other. That is the SH-4's documented behaviour. The
report's `P HALT` by `W START` says what a stop took.

### HOT

The window's state (where it stands, how far it is verified, and the
cursor the low reader's feed advances for every block) is the last
member of the auxiliary's context, which is linked at its uncached
address. Counted in the source, a block went through it about fifty
times: the drain loop 11, the feed and its `locate` about 26, the write
callback about 9.

With HOT every use in the auxiliary's C goes through `KUI_TOY_AHEAD`,
which is the member's linked address less the distance between the
uncached and the cached view of the same memory: a constant the linker
computes, so the code is no larger (the final build is 64 bytes smaller
with HOT than without).
Nothing else knows the member: the gate, the hooks and the worker use
the context's first members only, and the low reader is handed the
cursor's address. The member is aligned to a cache line and is the
context's last three lines, so that a line written back carries no
neighbour's stale bytes. In the linked module it goes by another name
(`movie_ahead_uncached`), so that a use of the uncached address does not
compile. The context is zeroed at init through its uncached address,
into lines the cold bind has purged.

What this adds to what already rests on the cache keeping its lines: 96
bytes. The stream's state, the kept counters, the verified sectors and
the private stack have been used the same way since Tests 68 to 70.

### COPY

Every byte is copied once more after the ring loop, from the stream's
block into its sector's place in the window (`toy_ahead_write`). The C
library's loop took a load and a store by turns. The first store into
each of the destination's sixteen lines made the cache fetch the line
from memory only to have it written over; and where source and
destination share a place in the cache each turn threw the other's line
out.

`kui_wire_copy` (`src/loader/toy_wire_copy.inc`), by hand: single words
until the destination stands on a line; for each whole line its eight
words loaded first, `MOVCA.L` for the first store, which claims the line
in the cache without fetching it, and seven stores more; single words
for what is left. 20 instructions a line, 338 for a block of 512 (the
library's loop: about 640). `MOVCA.L` is used only on a line all of
which the call writes at once.

The routine stores whole words wherever it is told to, and inside a
visit a misaligned store is a reset. `toy_wire_copy` gives it only
pieces whose two places and length are multiples of four and copies any
other as before. For this title every piece qualifies: its admission
requires tracks that begin at their file's first byte.

It is used for the window only, which is the reader's own memory. The
copy out to the game is unchanged (see Considered and left out).

### GAP

Between two blocks the card sends idle bytes until the next token, and
`kui_wire_next` read each of them through the ring's uncached address.
With GAP the search reads through the cached view by RING's rule: a line
is dropped from the cache before a call first reads it. Every byte the
search reads has landed; bytes of the line that land later are read by a
later call or by the block loop, each of which drops the line again. The
single bytes around a block and its CRC, and the held byte the reader
stores, stay at the uncached address.

### APART

The console's operand cache has 512 lines of 32 bytes, and address bits
5 to 13 pick the one line an address can be in. With this reader's
arena at `0x8c428000`:

| What | Address | Lines (bits 5 to 13, as an offset) |
|---|---|---|
| Private stack | `0x0c427fe0` downward | `0x3fe0` downward |
| Stream state, lost-position log | `0x8c44b9a0` | `0x39a0` to `0x3e7c` |
| Block buffer up to Test 75 | `0x8c44bea4` | `0x3ea4` to `0x3fff`, then `0x0000` to `0x00a4` |
| Tables up to Test 75 | `0x8c44c0c0` | `0x00c0` to `0x06c0` |
| First block buffer with APART | `0x8c44c000` | `0x0000` to `0x0200` |
| Tables with APART | `0x8c44c200` | `0x0200` to `0x0800` |
| Second block buffer with APART | `0x8c44c800` | `0x0800` to `0x0a00` |
| Kept counters | `0x8c44cc00` | `0x0c00` to `0x1000` |
| Window state (HOT) | `0x8c421700` | `0x1700` |
| Ring, 32 KiB | `0x8c44d000` | all of them, twice |

(`docs/evidence/toy-wire-stream-76/cache-places.txt` has the whole list
for both tests.) So the buffer every block passes through had its first
348 bytes on the lines of the reader's own stack frames: between filling
it and emptying it, each look at a local threw a line of the block out
and brought it back. And for one block in sixteen the ring bytes it is
filled from lie on the buffer's own lines, each word read and each word
stored throwing the other's out.

With APART the buffer is no longer part of the stream's state. It is one
of two that the caller provides, one just before the tables and one just
behind them. The second begins 64 lines behind the first, further than a
block's 514 bytes in the ring reach (18 lines at most), and is used for
exactly the blocks whose bytes in the ring share a line with the first.
Which buffer holds a block changes nothing else.

The reader's stack is the 4 KiB just below its arena wherever the lease
is, so its lines are the arena's last quarter of every 16 KiB. The three
are placed from the first multiple of 16 KiB behind the stream's state
on: the lines just above the stack's. Static assertions in
`toy_wire_ahead.inc` hold that against the lease's layout and against
the size of the stream's state, so that a later change to either cannot
move them unnoticed. (The first layout, in `6a39343`, put the buffers
behind the tables and so moved the tables down by the 512 bytes the
state had lost: the two tables of the other bit order came to lie on the
stack's lines, and the single bytes around a block go through one of
them in either order. The review found it.)

What this is and is not: the addresses follow from the module's layout
and the cache's shape is the SH7091's documented one. What the sharing
cost on the console is an estimate from that, not a measurement.

### The report

`P`'s last value is the ticks spent ending streams (`HALT`). `TB` is, by
piece, the most of the player's own buffer that was free: it is read
from the game's memory as Test 73 read it, but only in a call in which
the player took a frame. `D`'s last value is that buffer's capacity as
the sampling found it (Test 73's console run: 823,584 bytes, not the
megabyte Test 67's document took it for); zero means the buffer was
never found and `TB` says nothing. `TP` is, by piece, how long the wire
stood still for the stream's timer waits: what the stream's own count
gained between two visits is placed in the piece the later visit falls
in. `TR` is the reader's pump time by piece, Test 75's `TI` and `TG`
added. Gone: the game's GD calls timed whole (Tests 74 and 75 measured
30 microseconds a call) and the reasons windows were given up (Test 75:
none for a waited request).

The phase timers of Test 75 stay: five looks at the clock a block. They
are what this run is read by.

### Considered and left out

- The hand-written copy for the copy out to the game. The independent
  check of that plan found nothing that blocks it and six things to
  handle; what decided against it is the gain. The first sector of a
  request would stay on the resident's path, the source lines have to
  be fetched either way, and the 64 lines a sector claims in the cache
  push out as many of the game's, which the game fetches back outside
  any timer of ours. It would also leave lines of the game's buffer in
  the cache, which the resident's writer never does. `P OUT` stays at
  about 1.2 s.
- Following a stream past a position that is not a lost byte (Test 71,
  finding 5) instead of beginning it again. For a block with one or two
  listed positions the CRC16 can say which listed position was never
  lost. It is a change to the block reader's most delicate part, and
  with QUIT a restart costs about half of what it did: the 2 ms of
  reopening remain. 460 streams ended that way in Test 75's run, about
  310 in Test 74's.
- Moving the reader's hot functions so that they do not share lines in
  the 8 KiB instruction cache. In this build 11 lines are claimed by
  two of the functions that run for every block. It takes the linker
  script, which every earlier test is built with.
- The budget's look at the clock in the drain loop (five register reads
  a block where one would do), and the three bus reads a block that are
  left (the low reader's operations and the arena's address).
- A larger window. The arena is fixed; the ring could give up half of
  itself for seven more sectors, a fifteenth of a second of the dense
  part.

## Estimate

Estimates, not measurements. Of the last four of this kind three were
too hopeful and one (RING) too cautious. In microseconds for a card
block, against Test 75's measured 86.3 in pumps:

| Part | Estimate | From |
|---|---|---|
| LATER off | 0 to 4 | Test 74's stream counts: about 110 fewer streams begun again, 2,000 fewer sectors filled from parity, 1,200 fewer through the EDC |
| QUIT | 4 to 7 | 478 stops, by estimate 1 to 2 ms each |
| HOT | 5 to 8 | about 46 bus cycles a block at the 0.15 microseconds RING measured for one |
| COPY | 4 to 7 | sixteen line fetches, 300 instructions, and the shared lines |
| GAP | 1 to 4 | 0.13 microseconds for each idle byte between blocks; the card's gap is not known exactly |
| APART | 2 to 6 | the cache's shape, as above |

Together 16 to 36 of 86: a fifth to two fifths of the reader's pump
time. The model of Test 75's finding 8 asks for a fifth to a quarter of
the reader's time in the dense part. So: the stutter through the first
43 seconds much reduced and possibly gone; pieces 6, 8 and 9 better
and not certainly cured.

For the place where requests wait (43 to 48 s) only two things here add
supply: LATER off (Test 75's streams received 3 to 5 percent more wire
bytes for the same data) and QUIT (a restart frees the card up to 5 ms
sooner). Nothing here makes the wire carry more than 1.56 MB/s. `TB`
will say whether the player's buffer runs empty there.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The parts, the timers and the report's counters. Image 24,576 of 24,576 bytes: it is at its limit. |
| `native-worker` | The report's rows and title. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1
TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1`. Without `TOY_WIRE_FLUID` the Test 75
module reproduces byte for byte.

Code space. The auxiliary's text may be 20,472 bytes, its image 24,576.
Built on the final source with the modules' build label
(`code-space.log`):

| Build | Text | Image |
|---|---|---|
| Test 75 | 20,144 | 24,256 |
| Test 75 without LATER (the second module) | 20,036 | 24,160 |
| Test 76 with no part | 20,240 | 24,352 |
| Test 76 | 20,444 | 24,576 |
| Test 76 without APART | 20,400 | 24,512 |
| Test 76 without QUIT | 20,388 | 24,512 |
| Test 76 without GAP | 20,388 | 24,512 |
| Test 76 without COPY | 20,332 | 24,448 |
| Test 76 without HOT | 20,508 | does not link |
| Test 76 with LATER put back | 20,552 | does not link |
| Test 76 without HOT and without GAP | 20,452 | 24,576 |
| Test 76 without HOT and without COPY | 20,400 | 24,512 |
| Test 76 with LATER put back and without COPY | 20,440 | 24,544 |

HOT saves 64 bytes of code and LATER costs 108. With the other parts in
place neither of those two switches fits by itself any more: to leave
HOT out another part has to go with it (GAP or COPY, as built), and to
put LATER back COPY has to go. APART, QUIT, GAP and COPY can each be left
out alone. The host tests run every part by itself all the same
(`matrix.log`); the limit is the linked image's. Test 76's text leaves
28 bytes before the next step of 32, so any further change to the
auxiliary needs room made first.

## Known and not changed here

1. Streams that cannot be followed (see above).
2. The rest of the auxiliary's context at its uncached address.
3. The gate and the dispatcher are entered three times for each of the
   game's EXEC and CHECK calls.
4. The review findings listed in `docs/toy-wire-stream-test-69.md`.
5. `test-sci-stream-quarantine` stops on its fingerprint of
   `src/loader/sci_stream.c` as it has since before Test 73; Tests 68 to
   76 do not touch that file or that test.
6. The auxiliary's image is at its limit (Module roles).
7. The stream hands on a wrong block in the corner it always has: behind
   a byte lost between blocks, when the CRC16 cannot refuse the reading
   (Review). The window's sector check is what refuses it, as designed.
   What is new is only that the stream's host test says so correctly.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 76`. In order:

| Row | Meaning |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | As Test 75. `PT` adds 8 `QUIT`, 10 `HOT`, 20 `COPY`, 40 `GAP`, 80 `APART` (hexadecimal): `FD` the main module; the second module is Test 75's and prints 5 |
| `AHEAD`, `I SUBMIT`, `I FIRST`, `B SUBMIT`, `IP`, `BP`, `K`, `V` | As Test 75 |
| `P RING FEED CHK OUT SVC HALT` | As Test 75, with the ticks spent ending streams in the place of the visits that left the reading |
| `D N T SEEK CAP` | As Test 75 without the game's GD calls timed whole; then the capacity of the player's buffer in bytes as `TB`'s sampling found it (Test 73: `C9120`) |
| `TF`, `TD`, `TW` | As Test 75 |
| `TB` | By piece, the most of the player's buffer that was free, in units of 4 KiB (`C9` is all of Test 73's 823,584 bytes: an empty buffer) |
| `TP` | By piece, the ticks the wire stood still for timer waits, in units of 16,384 ticks (21 ms), as `TW` |
| `W START`, `W BYTES`, `W BLK`, `S C S F LF AM BC`, `W FILL REF BAD` | As Test 75 |
| `TR` | By piece, the reader's pump ticks, interrupt visits and the game's calls together, in units of 21 ms |

Gone to make room: `D`'s `G N T`, `X`, and `TI` and `TG` as two rows.

## Reading it

- `I SUBMIT` against `57F` (1,407), its `MAX AT` against `16B621` after
  `5A1`, `IP`'s `SN` against `22` and `OT / ON` against 40.5 ms; `TF`
  against `796D926D 7877365D 7828A1D7`.
- `B SUBMIT 14B`: the first movie must stay as it is.
- `V`'s three tick values added, by `W BLK`'s first: Test 75 67.4 ticks,
  86.3 microseconds a block. Then `P`'s values by the same count against
  Test 75's 33.1, 25.0 (with 7.1 in `CHK`), 3.6 and `D SEEK`'s 7.1.
  `RING` should fall with GAP and APART, `FEED` less `CHK` with HOT,
  COPY and APART, the rest with QUIT and HOT.
- `P HALT` by `W START`: what ending a stream took. With QUIT some tens
  of ticks; a wait would be thousands.
- `W WAIT`, `W BLK HOLED`, `W FILL`, `W START`, `W BYTES` against Test
  74's 6,556, 4,270, 1,925, 365 and 73.3 MB and Test 75's 10,930,
  6,667, 3,918, 478 and 75.8 MB: whether the stream is Test 74's again.
  The second module's report asks the same of Test 75's reader.
- `TB` in pieces 8 and 9, against `D`'s `CAP` in the same units (`C9`
  for Test 73's capacity): a buffer that runs empty there is the player
  living from the wire; one that stays full while frames are lost is
  the player short of time. `TP` in the same pieces against `TW`.
- `K`'s `MB` and `LB` and `AHEAD`'s `STOP` must stay 0. A copy that put
  wrong bytes in the window would show as `LB` and `W BAD`.

## Console result

Owner's run of the main module, report received 2026-11-15. The owner
reported no deviation from the package's procedure (cold CD boot, Card
Tools 09c9a4b686d4, raw Toy Commander GDI, "66 SCI movie playback", CD
Music and virtual VMU off). Saved report, 1,024 bytes, headed `TEST 76`,
`BUILD 7fe7e83c2082`, `PT` `FD`:
`docs/evidence/toy-wire-stream-76/console-report-66-10.txt`, SHA-256
`dd602090...8b41`. The second module was not run.

Owner's observation: "Playback was about 99.9% smooth with test 76. There
was a SMALL hiccup of maybe 20 frames toward the end that didn't work."

| | Test 76 | Test 75 | Test 74 |
|---|---|---|---|
| First movie: frames submitted / on time | 331 / 330 | 331 / 330 | 331 / 330 |
| Second movie: frames submitted of 1,968 | 1,942 | 1,407 | 967 |
| Of those on time and in sequence | 1,934 | 1,283 | 856 |
| Frames skipped, first at the player's frame | 26, 1,162 | 561, 37 | 1,001, 10 |
| First frame after | 0.800 s | 0.802 s | 1.105 s |
| Longest gap between frames | 0.47 s after 1,147 | 1.91 s after 1,441 | 1.95 s after 1,441 |
| Windows opened, first / second movie | 2 / 2 | 2 / 2 | 10 / 45 |
| Waited requests that kept their window | 53 | 52 | |
| Sectors served from the window | 3,421 / 25,824 | 3,423 / 25,827 | 3,403 / 25,770 |

The player's own update calls:

| | Test 76 | Test 75 |
|---|---|---|
| Second movie: calls that put a frame out | 1,941 | 1,395 |
| Time inside them, each | 59.18 s, 30.5 ms | 56.49 s, 40.5 ms |
| Reader's interrupt pumps in that span, each | 1.75 ms | 2.67 ms |
| Calls that skipped and put nothing out | 2, 0.008 s | 34, 3.55 s |
| First movie: calls that put a frame out, each | 330, 22.7 ms | 330, 25.8 ms |

The reader:

| | Test 76 | Test 75 |
|---|---|---|
| Visits from the SCI interrupt | 25,594, 4.11 s | 29,617, 4.52 s |
| Pumps from the game's calls | 13,280, 4.74 s | 14,147, 7.13 s |
| Timer visits | 5,861, 0.03 s | 8,834, 0.04 s |
| All pumps | 8.88 s | 11.69 s |
| Card blocks, pump time for each | 135,712, 65.5 us | 135,381, 86.3 us |
| Sectors accepted on CRC16 and address / through the EDC | 23,912 / 5,389 | 22,657 / 6,643 |
| Rebuilt bytes their P code word confirmed / did not | 11,630 / 0 | 8,895 / 0 |
| Sectors that passed the lighter check and failed the EDC watch | 0 | 0 |
| Delivery visits on the whole path | 6,586, 9.44 s | 8,024, 10.62 s |
| Capacity of the player's buffer as sampled | 823,584 bytes | |

The reader's pump time by what it was doing, microseconds for each card
block:

| | Test 76 | Test 75 | Estimate for the change |
|---|---|---|---|
| Taking the block out of the ring: token, CRC16, bytes | 33.7 | 33.1 | GAP 1 to 4, APART 2 to 6 less |
| Feeding it to the window, without the sectors' checks | 8.4 | 17.9 | HOT 5 to 8, COPY 4 to 7 less |
| The sectors' checks | 6.1 | 7.1 | |
| The stream's service | 3.4 | 3.6 | |
| The low reader bringing the card to a stream's first block | 9.6 | 7.1 | |
| The rest of the pumps | 4.3 | 17.6 | QUIT 4 to 7 less |
| All | 65.5 | 86.3 | 16 to 36 less |

Ending a stream took 5,648 ticks in all for 658 streams: 11 microseconds
each (Test 75 waited out the run, up to 5 ms). Outside the pumps, copying
requests out to the game took 1.21 s as before, 41 microseconds for each
2,048-byte sector. Reopening took 1.30 s, 2.0 ms for each stream begun.

The second movie's course, twelve pieces of 5.4 s from its start:

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted (161 is all) | 136 | 163 | 162 | 161 | 162 | 162 | 160 | 137 | 161 | 162 | 161 | 215 |
| Test 75 | 121 | 109 | 146 | 109 | 120 | 119 | 54 | 93 | 120 | 40 | 161 | 215 |
| Sectors served | 1,968 | 2,448 | 2,352 | 2,384 | 2,368 | 2,368 | 2,352 | 2,400 | 2,576 | 2,224 | 608 | 1,680 |
| Requests waited, s | 0.06 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.02 | 0.44 | 0 | 0.06 |
| Test 75 | 0.08 | 0.04 | 0.00 | 0.08 | 0.04 | 0.02 | 0.04 | 0.00 | 1.26 | 0.52 | 0 | 0 |
| Player's buffer at its emptiest, KiB of 804 | 256 | 32 | 88 | 108 | 152 | 148 | 32 | 28 | 32 | 28 | 736 | 524 |
| Wire standing still for timer waits, s | 0.00 | 0.04 | 0.06 | 0.04 | 0.02 | 0.04 | 0.02 | 0.08 | 0.10 | 0.52 | 1.87 | 0.80 |
| Reader, interrupt visits and the game's calls, s | 0.50 | 0.67 | 0.65 | 0.65 | 0.61 | 0.65 | 0.59 | 0.73 | 0.82 | 0.75 | 0.25 | 0.57 |
| Test 75 | 0.61 | 0.82 | 0.76 | 0.88 | 0.80 | 0.82 | 0.79 | 0.86 | 1.00 | 0.93 | 0.37 | 0.77 |

(The first piece holds the 0.8 s before the first frame, and the last
collects the movie's tail.)

Stream (Test 75, Test 74): 658 begun (478, 365), 6,922 full runs (7,069,
6,569), 19,913 overruns in a run (23,825, 23,226), 8,368 timer waits of
5.07 s (10,930 of 4.27 s, 6,556 of 3.36 s), 1,465 stops for room (1,515,
1,631), 72,113,942 wire bytes (75,825,683, 73,303,443), TMU1, no fault.
Blocks 135,712: 14,821 rebuilt from CRC16 (11,194, 16,838), 48 single
losses not explained (22, 25), 2,885 handed on with lost bytes (6,667,
4,270), 46 with only a CRC byte lost (52, 66); 711 read by the low reader
to open or reopen (527, 384). Streams ended because the window closed 4,
a sector failed its check 13, the stream could not be followed 641: a
byte between blocks neither idle nor token 640, a block without lost
bytes failing its CRC16 1 (Test 75: 4, 12, 462: 460 and 2). Sectors
completed from their parity 1,887 (3,918, 1,925); 1 refused, 12 failed
their check and were read again. No window was given up on an error.

What the run establishes:

1. The second movie shows 1,942 of its 1,968 frames, against 1,407. The
   first shows all of its own, as before. All 26 skipped frames lie in
   one piece of the course, piece 7 (37.6 to 42.9 s), which has 137
   frames where a whole piece has 161 or 162. Every other piece is
   whole.
2. The pause near the 48th second is gone. Pieces 8 and 9 are whole;
   requests waited 0.02 s and 0.44 s there, against 1.26 s and 0.52 s,
   and the longest gap of the run is 0.47 s where it was 1.91 s.
3. A card block costs the reader 65.5 microseconds in pumps, against
   86.3: 20.8 less, inside the estimate of 16 to 36. Where it went is
   not where every part was estimated:
   - the rest of the pumps fell from 17.6 to 4.3. Ending a stream now
     takes 11 microseconds. QUIT was estimated at 4 to 7 for a block;
     the fall is 13, of which HOT's share cannot be told apart;
   - feeding the window fell from 17.9 to 8.4 (HOT and COPY, estimated 9
     to 15 together, with APART's share of the block buffer);
   - the ring loop did not move: 33.7 against 33.1. GAP and APART were
     estimated at 3 to 10 there and show nothing;
   - the low reader's reopening rose from 7.1 to 9.6, with the number of
     streams begun (finding 6).
4. The player's call that puts a frame out takes 30.5 ms on average,
   against 40.5, where a frame has 33.2. In the first movie 22.7 against
   25.8. The reader's interrupt pumps take 1.75 ms of each frame's span,
   against 2.67.
5. LATER's cost to the stream was real, as far as one run against
   another can say: with it off, blocks handed on with lost bytes are
   2,885 against 6,667 (Test 74: 4,270) and sectors completed from
   parity 1,887 against 3,918 (1,925), on 19,913 overruns in a run
   against 23,825. The second module, which differs from Test 75 in that
   switch alone, was not run.
6. One number is worse. 658 streams were begun, against 478 and 365,
   and 640 of them because the reader met a byte between blocks that is
   neither idle nor a token: 3.2 for every hundred overruns in a run,
   against 1.9 in Test 75 and about 1.3 in Test 74. Each costs the low
   reader 2.0 ms to bring the card back, 1.30 s in all, and the wire's
   time besides. The cause is not established. GAP is the only part of
   this test that changes how the bytes between blocks are read; the
   stream is begun again either way and no wrong byte follows from it.
7. The hiccup, as far as the report measures it. Behind the 1,147th
   frame no frame came for 0.466 s. That is fourteen of the movie's
   frame periods to within a hundredth (the period measures 25,962
   ticks by this clock, not the nominal 26,042, so a whole piece is
   161.5 frames): the frame that ended the gap came on the player's own
   grid. The gap accounts for 13 of the frames piece 7 lacks. The piece
   lacks about 24.5, and the player counted 26 skipped in all, the first
   at its frame 1,162. So about eleven more frame times passed without a
   new frame in the 3.5 s behind the gap, in six further breaks at most:
   1,934 of the 1,941 frames that followed another came on time and in
   sequence. The report keeps only the longest break.

   The card's data was on time there. Requests whose wait ended in
   piece 7 waited under 21 ms together, the wire stood still for 0.08 s,
   the reader's pumps took 0.73 s of the piece (piece 8: 0.82 s, and
   whole), reopening took 2.0 ms a stream as in Test 75, and no window
   was given up.
8. Why the player dropped frames there is not established. Two readings
   fit every row of the report:
   - The player was short of processor time. Its call that puts a frame
     out takes 30.5 ms of a frame time of 33.2 ms, on average over the
     whole movie with its light last twelve seconds. Pieces 6 and 7 were
     among the worst of the course in Test 75 (54 and 93 frames) with
     hardly a request waiting (0.04 s and none), which that test read as
     processor shortage. And frames shown have followed the reader's
     pump time from test to test: 12.9 s gave 967, 11.7 s gave 1,407,
     8.9 s gives 1,942.
   - The player ran out of data in its own buffer. The buffer is at its
     floor in that piece: 28 KiB of 804 at its emptiest.

   Against deciding for the second: the buffer is as empty in pieces 1,
   6, 8 and 9, which are whole, and piece 9 had requests waiting 0.44 s
   and the wire standing still 0.52 s besides and lost nothing. `TB` is
   the emptiest level seen in a piece at calls that took a frame; it
   cannot be placed at the gap, and a wait for data is never sampled.

   The author first told the owner that the player takes in at most one
   read of 16 sectors a frame and that the dense stretch uses about
   that much. That rested on piece 8 alone (2,576 sectors for 161
   frames). The run's requests are 12.1 sectors each at about one a
   frame, pieces 1 to 7 took 14.6 to 15.2 sectors a frame time, and an
   independent check of the reading did not bear it out. It is
   withdrawn, with the guess that the 0.47 s was a refill to a
   threshold: no row shows one.

   What would decide it is whether the player's next chunk was missing
   from its buffer during the gap. The game already calls a hook of
   ours where it looks for that chunk (`kui_toy_movie_chunk_observe`),
   and a course row of the ticks in which the chunk was not there would
   say. It needs a row's room in the report (this run left 33 bytes
   free, a run at full width leaves 11) and room in the auxiliary's
   image, which is at its limit. It is a measurement and no change of
   behaviour. It is the next step and is not built.
9. Checks: no rebuilt byte went unconfirmed (`MB` 0), no sector passed
   the lighter check and failed the EDC watch (`LB` 0), the receiver's
   bit order was confirmed (`ORD` 3), and the sampling found the
   player's buffer at Test 73's capacity (`CAP` `C9120`).

## Verification

Host and linked checks only; none is a console result.

The two modules are built from commit `7fe7e83c2082`, the last commit
that changes a source of the module, on a clean worktree
(`source_dirty: false`). The checks below ran on a clean worktree of
commit `61e24e9db5ad`, which differs from it in three files under
`tests/` and nothing else (see Review for why). Built again from nothing
on the first of the two tests-only commits with the same build label,
both modules have the same bytes. Logs are in
`docs/evidence/toy-wire-stream-76/`.

| Check | What ran | Result |
|---|---|---|
| Whole reader, each part | `tests/test_toy_wire_integration.c`: the real low reader, auxiliary and stream on the card model, the whole scenario list twice (a receiver that ignores the bit order request and one that honours it) | Passed in Test 76 (243,068,473 checks) and with each part left out in turn; every request completed with the image's exact bytes |
| LATER left out | `playing()`, `steady()` in Tests 74, 75, 75 without LATER and 76 (`model-part-by-part.log`) | Test 75 without LATER, the second module's recipe, gives Test 74's figures line for line; Test 76 gives the same where the model's bus does not stall and nearly the same where it does (QUIT changes the course). The model still favours LATER in one case, as it did before Test 75 ran: at 70 ticks a block and no stalls it has data ready for 21 of 21 frames with LATER and for none without. The console's run decided against LATER, and the second module puts that question to the console once more |
| QUIT | `stream_stop()` on every stream the stream test gives up; `quit_foreign()` | 1,111 streams given up while they received: the card clocked on for 2 bytes at most, 2,220 in all (Test 75 and Test 76 without QUIT: 1,162 streams, 8,193 at most, 395,634 in all). A channel taken by another source, or pointed outside the run, is left as it was and the stop reports it |
| HOT as linked | `tests/test_toy_wire_linked_hot.py` | The window's state, the context's last three cache lines, is used at its cached address `0x8c421700` only: 22 constants in the linked code, none at the uncached address |
| COPY as linked | `tests/test_toy_wire_linked_copy.py` with the interpreter of `tests/sh4_mini.py`, which leaves the rest of a line undefined at `MOVCA.L` | 3,302 copies exact, destinations and sources at every word of a line; 18,797 lines claimed, each wholly inside its destination and wholly stored, its eight words loaded first; 20 instructions a line. Eight defects built into the routine on purpose are each caught |
| COPY in the reader | `misaligned()` and the stand-in in every scenario | A track that begins two bytes into its block is served with none of its pieces given to the word copy; 383,772 pieces, 162,169,184 bytes copied by the stand-in elsewhere |
| GAP | Both host tests against a view of the ring that keeps what memory held at each line's last drop | 3,349,007 lines dropped and fetched afresh for 180,000 blocks; nothing read stale |
| APART | The stream test on every block; compile-time assertions on the layout; `cache-places.txt` | 166,590 blocks taken into the first buffer and 13,764 into the second; none shared a place in the cache with its bytes in the ring. Buffers and tables lie on none of the stack's lines |
| The report | `buffer_seen()`, `pause_placed()`; `tests/test_toy_wire_report.c` on the production formatter | The buffer's emptiest in each piece is taken when the player took a frame; the wire's standstill is placed in the course's pieces to the tick. All rows fit at a full run's values (1,001 of 1,012 bytes); the rows the test is read by end at byte 971 |
| The hand-written loops, the cache view and the gate as linked | `tests/test_toy_wire_linked_loops.py`, `tests/test_toy_wire_linked_ring.py`, `tests/test_toy_wire_cached_stack.py` | As Test 75 |
| Card stream | `tests/test_toy_wire_stream.c`, each block judged by where the stream took it from | 180,000 blocks checked against the card over 526,445 overruns: from its own place every byte not listed as lost is the card's. 348 handed on from beside their place, each behind a byte lost between blocks and none after a whole CRC16; in these courses all with lost bytes listed |
| The stream's corner by construction | The same test: a block whose data begins like two idle bytes and a token, its own token lost, read three bytes late; a block of FF read three bytes early; the bit order question | With a byte of the late reading's CRC16 lost the block is handed on unchecked where the byte behind looks like a gap, and refused for the right reading where it does not. With one data byte lost it is handed on rebuilt and wrong for 3 of the 492 places tried (35, 353, 418; the same on a second block) and rebuilt right for the others. The block of FF is handed on with 24 lost bytes listed and every other byte the card's, and the stream then stops beside the blocks. Four streams that lose a byte ahead of their first token end the asking; an honoured order is given up when a whole reading from beside its place fails its CRC16 before a block has settled it; the next stream reads the old way and right. In each of 986 constructed streams the bytes lost are exactly the two listed |
| The same over more courses | The stream test over 96 courses in place of 12, in every configuration, built with the sanitizers and without (`long-courses.log`) | All 29 configurations pass in both builds, with the same output but for the count of blocks in APART's two buffers, which goes by the host's addresses: 1.44 million blocks in each. All three kinds of wrong block come up there by themselves, for example in Test 75's configuration 2,956 with lost bytes listed and 2 with a byte of the CRC16 lost, in Test 76's 2,911 listed and 1 rebuilt wrong |
| All configurations | Both host targets in Tests 68 to 75 with their variants, Test 76, Test 76 with each part left out, with LATER put back, without the bit order request, without RING, without STAY and with no part (`matrix.log`) | 58 of 58 passed |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | 190 of 190 deliberate defects caught, 35 of them in what only Test 76 has. The lists of Tests 73 (79), 74 (125) and 75 (167) still apply to this source |
| Other suites on the shared wire model | As for Test 75 (`other-suites.log`) | 17 of 18 passed. `test-sci-stream-quarantine` stops on its fingerprint of `src/loader/sci_stream.c` as at the commits of Tests 73 to 75; not caused by this change and not repaired here |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes of both modules | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; the same in both modules |
| Reproducible | Clean worktrees of both commits | Module SHA-256 `6540c38d...a881`, second module `a742b4a7...ee51` |
| Earlier deliveries | Unchanged recipes on this source | Test 75 `9ef4c29b...eb08` and its second module `52423cf2...da1f`, Test 74 `9e3bb2b2...d4e4` and its second module `582e2d7a...d86c`, Test 73 `7f28c744...7124`, Test 72 `04c6b439...fe9b`, Test 71 `73e8f9ed...a26f`, Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

What the verification itself found and changed:

- Before the package was delivered: the last full run of all
  configurations failed in 16 of 58 jobs on the commit the modules are
  built from. The fault was in the stream test's statements, not in the
  stream; commit `4be776b` corrects the test. Review has the account,
  `matrix-before-the-test-was-corrected.log` the failing jobs.
- After it: two readers of the corrected test found it still short of
  what it said; commit `61e24e9` is what was done (Review has their
  fourteen findings). The stream was not changed by either commit, and
  the delivered modules are the ones built before both.
- After it: a build of the variant without GAP (`variant-without-gap.log`),
  begun while the copy routine's deliberate defects were being built in
  the same worktree, took one of them in. Its linked check failed on
  it. The variant was built again on the restored source and has the
  bytes of the build made before. It has not been delivered.
- The auxiliary's image is at its limit, and two single switches no
  longer link: see Module roles.

## Review

Independent readers were asked three times, each time for concrete
failure cases and not for agreement. Their answers are in
`docs/evidence/toy-wire-stream-76/review.md` as returned. None of it is a
console result.

**Before anything was built: three readers on the analysis and the plan.**
They corrected the reading of Test 75's reports in four places: the waits
in piece 8 come before the gap behind frame 1,441, not after it; the
pump's own time includes the wait at each stream's end; that LATER cost
the stream is an inference from two runs that differ in more than LATER
(which is why the second module differs from Test 75 in that switch
alone); and the copy out to the game is 1.5% of the processor, not under
1%. The wait at each stream's end, which QUIT removes, was theirs to
find. A hand-written copy for the copy out to the game was dropped on
their account, and the window's state stayed in the context, reached
through the cache, instead of moving.

**After the six parts were built: five readers, one for each change (GAP
and APART together), on commit `6a39343`.** None found a way for wrong
data to reach the game. What they found, and what commit `7fe7e83` did:

| Part | Finding | Disposition |
|---|---|---|
| Base | The report could not be scaled: `TB` is in 4 KiB units of a capacity that was stored but not printed, and the comment and the fixture took the player's buffer for a megabyte. The console measured 823,584 bytes in Test 73 (report only) | `D`'s row prints `CAP`. Comment, README and this document say 804 KiB and `C9` |
| QUIT | The channel was taken off the receiver before the check that it is still the stream's (possible; nothing on this console is known to take channel 1) | The check comes first. A channel that is not the stream's is left as it is, the run is waited out as before Test 76, and the stop reports whose it is. `quit_foreign()` shows both ways a channel can be foreign |
| QUIT | The integration model runs a reception to its overrun in one step, so QUIT changes nothing there (test gap) | Not changed. The state QUIT creates is exercised by the stream model alone, and that model was made stricter (next row) |
| QUIT | The stream model clocked a byte with every look at the receiver's status and stored whatever was written to the channel's control register: a wait of two looks, or a write of the end flag in place of the enable bit, passed (test gap) | A byte for every fourth look; the end flag is cleared by writing it and never set. Four mutations added, each caught |
| HOT | None | |
| COPY | Nothing wrong in the routine. The linked test ran it with the callee-saved registers all zero, so two of them restored in each other's place passed (test gap) | Each register is given a value of its own before every call. That defect is one of eight built into the routine on purpose; all eight are caught |
| GAP | None | |
| APART | The first layout put the block buffers behind the tables, which moved the tables for the other bit order onto the stack's cache lines: time only, no wrong data (certain) | Laid out again: a buffer, the tables, the other buffer, from the first multiple of 16 KiB behind the stream's state, the lines just above the stack's and none of them |
| APART | Nothing held the layout against the stack's place in the linked module; a later change of the state's size could shift it unseen (test gap) | The layout no longer follows the state's size but for the round-up, and four compile-time assertions tie it to the stack's page, the tables and the kept counters. `cache-places.txt` in the evidence is the result |

One remark was left as it is: the stream model seldom stops a stream
whose receiver has already overrun, because the test's loop answers an
overrun before the reader runs. QUIT does nothing new in that state (the
wait ends at once and the capture is the one every cut takes).

**After that: the last full run of the checks.** Both host targets in all
29 configurations, on a clean worktree of `7fe7e83`, failed in 16 of 58
jobs. The cause was the test of the card stream, not the stream, and it
came from the stricter clock of the row above. The stream test had said
that a block handed on wrong always has lost bytes listed and that a
block called rebuilt is never wrong. Those were true of the courses the
old clock produced and are not true of the design, which leaves two
rarer kinds of wrong block to the sector's check: one whose CRC16 itself
lost a byte, and one whose single lost byte the CRC16 seemed to explain
(once in 256 such readings; `toy_wire_stream.h` has said so since Test
74). Both arise only behind a byte lost between blocks, where a block's
beginning can be read wrong, and the window refuses both (a block that
is not clean sends its sector through the EDC; a rebuilt byte is
confirmed from the sector's parity or else by the EDC; `wrong_blocks()`
in the integration test hands the window one of each).

Commit `4be776b` changes the test and nothing else. It states what
follows from the code (a wrong block only behind a byte lost between
blocks and never after a whole CRC16; a block called clean never wrong;
the kinds exactly what the counters say), counts the three kinds, and
makes the two rare ones by construction. Run over eight times the usual
number of courses it then showed two statements of the same sort about
the bit-order question, which are stated exactly as well. The stall
draw's two random numbers are drawn in a fixed order, so the course no
longer depends on the compiler. The full account is at the end of
`review.md`.

The earlier documents' sentence that no block handed on as clean or
rebuilt had a wrong byte (Tests 74 and 75) was a true observation of
those runs. It is not an invariant, and this document does not repeat it.

**After that: two more readers on the corrected test (commit `4be776b`).**
One read the model and the constructed corner, the other asked of every
statement in the test's loop whether it follows from the stream's code.
Both built and ran the test in directories of their own. Neither found a
fault in the stream. Both found the test still short of what it said;
commit `61e24e9` (tests only) is what was done about it:

| Finding | What was done |
|---|---|
| A block read from beside its place can equal the card in every byte not listed as lost: the card has blocks of FF behind FF FF FE. The test called it right; the stream, three bytes out, then stops at a byte that is neither idle nor a token, which the test said never happens. By the reader's estimate about one run in a few hundred | A block is judged by where the stream took it from. From its own place its bytes are asserted; from beside it, it is wrong whatever its bytes. The case is made by construction |
| Under the model's "honoured" answer the stream can give the bit order up: before a whole block has settled it, a whole reading from beside its place fails its CRC16, and the stream cannot tell a wrong order from a wrong place. The test asserted the order was always read right | The one way it can happen is asserted exactly and counted. It is made by construction, with the next stream reading the old way and right |
| The gap counters' baseline was not taken again when a stream is begun anew: for the first block of such a stream "behind a byte lost between blocks" could not fail (about twenty of a course's 350 wrong blocks) | Taken again at every start |
| The look behind a reading that cannot be checked could be removed without any test failing: the constructed case made only its accepting side | The refusing side is made too. Five mutations of this corner are added, each caught |
| The search for a block rebuilt wrong did not sample a chance of one in 256: for that shape the places where a lost byte fits are fixed by the three bytes the late reading drops and the three it takes in, whatever the block | Every place from 8 to 499 is tried on two blocks: 35, 353 and 418 fit on both, no other |
| Nothing held the bytes listed to be lost against the stream's own log | Held in each constructed stream |
| The fixture's own stops were counted into the printed figures of streams given up: a third of the bytes in builds without QUIT | Kept apart, with its overruns and cache drops. The figures in this document are the courses' |
| "A block its CRC16 could not refuse" is not exact. A single lost byte the CRC16 cannot place shows the rest wrong, and the block goes on with the byte listed when no other reading is open (4 of the 348 in Test 76's courses) | Said as it is: a wrong block is one whose CRC16 was not taken whole |
| The header left out the blocks that go on with every byte the other way round under the token-only answer; said the window confirms every rebuilt byte from parity; and named kinds as having come up without saying where | Said: counted apart; P parity where it covers the byte and otherwise the EDC; which kind came up in which build |
| No course loses a byte ahead of a stream's first token, so the "undecided" path of the bit order question never ran in this test | Made by construction: four such streams end the asking, and the next reads the old way |
| The loop's statements rest on the card as well as on the CRC16: no block of FF has a CRC16 byte of FF, and no FE-first block of FF is followed by a zero gap | The first is asserted of the card at the start. Judging by place removes the need for the second |
| The window's own case for a lost CRC16 byte did not require that case, with nothing else of the block lost, in any placement | Required in three at least. It is so in 11 of the 12 |
| The window's cases show a wrong byte refused, not a block read from beside its place. For such a block rebuilt wrong, the parity check of the rebuilt byte holds by chance once in 256, and the sector is then taken without its EDC unless it is the one in sixteen sampled | Not a test's to show refused: it is the residue the design states (`toy_wire_ahead.inc`), one chance in 65,536 of such a reading by its reckoning. For the shape constructed here the first of the two factors is 3 places in 512, not one in 256. The README says it in plain words |
| The stream test's courses never fill the ring, so the hold for room is not exercised by this test | Left. The integration test exercises it (`steady()`: 84 holds and more) |

One remark about the stream itself, recorded and not changed: an honoured
bit order given up in that way stays given up for as long as the caller
keeps the counters. That costs the shorter of the two block loops, not
correctness, and it can happen only before a session's first whole block.
The console's runs that asked ended with the order confirmed (`ORD` 3).

## Package

`KUI-Toy-Wire-Stream-Test-76-15NOV2026-0522-CST-7fe7e83.zip`, 1,979,821 bytes, SHA-256
`3f47f70e3724b5a885da2167d87a81c5adab63c91ed5b0a829617e0c35c2a27c`, prepared 2026-11-15 05:22 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `7fe7e83c2082` | `8D0D12DA` | `6540c38d...a881` | Changed: Test 76, replaces the Test 75 module `bf6f1c703add` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |
| `ALTERNATE/KUI/apps/games/retail-toy-test-66.kui` | `7fe7e83c2082` | `C3176B73` | `a742b4a7...ee51` | Outside the `KUI/` tree: Test 75 built with `TOY_WIRE_LATER=0`, as a fallback and as the comparison for LATER |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

The two modules carry the same build label, `7fe7e83c2082`: the commit of
the last change to a source of the module. The report tells them apart
by its title (`TEST 76`, and `TEST 75` for the second) and by `PIPE`'s
last value: `FD` the main module, `5` the second.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  BUILD_ID=7fe7e83c2082 build/toy-suite-76/retail-toy-test-76.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test toy-wire-linked-check \
  TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 python3 tests/test_toy_wire_stream_mutations.py
```

The second module: the first command without `TOY_WIRE_FLUID=1`, with
`TOY_WIRE_LATER=0`, the target `retail-toy-test-75.kui` and another
`BUILD` directory.

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 76), CD Music and virtual VMU Off. Confirm `TEST 76` and
`BUILD 7fe7e83c2082` in the saved `/reports/toy/66-N.txt`, and `PIPE`'s
last value.
