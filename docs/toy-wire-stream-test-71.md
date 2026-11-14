# Toy Commander Test 71: the ring reader's block loop, a word at a time

Status: ran on the console on 2026-11-14 (build `4ee37937a022`, owner's
report `/reports/toy/66-5.txt`). A further improvement: the second movie
shows 524 frames against 436, and its last twelve seconds run at full
speed. Kept as the baseline of Test 72
(`docs/toy-wire-stream-test-72.md`); not accepted as a whole. The course
rows show what holds the second movie back. See "Console result" below.

## Baseline and the one change

- Baseline: Test 70, build `b492f00a5478`
  (`docs/toy-wire-stream-test-70.md`), as it ran on the console: the first
  movie as from the disc, the reader's time halved to 16.7 s, the second
  movie showing 436 of 1,968 frames.
- What is left of the reader's time, from the compiled code of Test 70. The
  loop in `kui_wire_next` that takes a block's bytes out of the ring runs
  79 instructions for every four bytes, nine of them stack accesses that
  are now cached but still executed, and one uncached read of the ring.
  About 65 microseconds a block, a little over half of the reader's time.
  The sector check adds about 20, and each of the 246,000 visits its fixed
  cost.
- The one behavior change: a run of whole words of the ring goes through
  `wire_words()` (`src/loader/toy_wire_stream.c`). It takes the CRC16 two
  bytes to a step, from a second table built beside the first, reverses
  each byte from the table as before, and keeps nothing in memory between
  words. As linked it is 49 instructions for four bytes, with no stack
  access. Bytes ahead of a word boundary, behind the last whole word, and
  the block's two CRC bytes take the byte steps as before.
- What comes out is unchanged: the same CRC16 remainder and the same 512
  bytes, so the same blocks are accepted, rebuilt, handed on or refused.
- Decision the console test answers: what that takes off the reader's time
  (`V` against Test 70's), and, from the new rows, what the second movie
  does over its length.

Estimate, from instruction counts and not from any measurement: about 22
microseconds less a block, 3 s less over both movies, 16.7 s to under 14 s.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The block loop; the report's counters. Image 24,384 of 24,576 bytes: 192 are left. |
| `native-worker` | The report's rows and title. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1`. Without `TOY_WIRE_WORDS` the Test 70 module reproduces
byte for byte.

## The loop

For a pair of bytes `b0`, `b1` and `t = crc ^ (b0 | b1 << 8)`, two CRC16
steps are `two[t & 0xff] ^ one[t >> 8]`, where `one` is the existing table
and `two[i] = (one[i] >> 8) ^ one[one[i] & 0xff]`: the table is linear in
its index, so the second step's entry splits into the part the first step's
result contributes and `one[t >> 8]`. A word of the ring gives two such
pairs. The tables are read as signed 16-bit words to save an extension per
load; the bits a sign extension adds lie above bit 15 and are masked off
where an index is taken and once at the end.

The second table needs 512 bytes more beside the stream's state in the
arena, where there was room. The loop that Test 70 unrolled over a word's
four bytes is not compiled any more.

## Known and not changed here

1. Streams that cannot be followed: 137 in Test 70. See that document's
   finding 4. This build only counts the signature of one cause.
2. The sector check behind a sector whose blocks were all clean.
3. The auxiliary's context page at its uncached address, and the fixed
   cost of a visit.
4. Reads of the ring through the cache.
5. The first second of the second movie is read before the window opens;
   every run so far skips first at frame 20 to 24.
6. The review findings listed in `docs/toy-wire-stream-test-69.md`.
7. The auxiliary's code space is nearly used up: 192 bytes. A later change
   that adds code must take some out or move the split with the arena.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 71`. Rows as in Test 70 down to `Q`, then:

| Row | Meaning |
|---|---|
| `P BUDGET RT RMAX` | Visits that ran out of time with blocks left; ticks from a stream given up to its successor receiving, in all and at most |
| `TF` | The second movie's course: frames submitted in each of twelve pieces of 2^22 ticks (5.4 s) from the movie's start |
| `TD` | Sectors served from the window in each piece, in sixteens |
| `TW` | Ticks requests waited in each piece, in units of 2^14 (21 ms) |
| `TI` | The reader's ticks in SCI and timer visits in each piece, same unit |
| `TG` | The reader's ticks in the game's calls in each piece, same unit |
| `TX` | Streams given up for a restart in each piece |
| `W START` to `W STOP` | As Test 70 |
| `F END HELD TWICE` | Unexpected bytes directly behind a block's end; of those, blocks with a lost position whose preceding byte was stored from the receiver's register; of those, where that byte equals the one before it |

Each course row is three words: a byte a piece, four pieces a word, the
earliest in the top byte, a byte that reaches `FF` standing for that or
more. Words are printed without leading zeros. The thirteenth piece's
second, if the movie runs into it, is kept in the twelfth.

Gone to make room: the first two notes and the bit and kind counts of the
unexpected bytes, the window drops (`0 2 0 0 9 0` in every run), the low
reader's counts, and the control registers (`105` and `0`).

## Reading it

- Sum of the three `V` tick values against Test 70's `49A5DF + 79C5 +
  7C7623` (16.7 s).
- `TF` at `A1` is a piece in which every frame was submitted. The owner saw
  about a second and a half at the start and two seconds at the end: the
  first and last pieces should stand out, not reach `A1`.
- Pieces without frames with `TW` high and `TG` high: the player is waiting
  for data and the reader works in its calls. With `TW` low and `TI` high:
  the data is there and the reader's interrupt visits take the time. `TD`
  above `84` in a piece: the player is reading faster than the movie plays,
  skipping.
- `TX` against `TF`: whether frames stop where streams are restarted.
- `F HELD` and `F TWICE` close to `F END`: the positions that are not lost
  bytes come from a held byte stored twice, and the reader can be taught
  to see it. `F TWICE` near nothing: another cause.

## Console result

Owner's run, cold CD boot, Card Tools 09c9a4b686d4, raw Toy Commander GDI,
"66 SCI movie playback", CD Music and virtual VMU off. Saved report
`docs/evidence/toy-wire-stream-71/console-report-66-5.txt` (1,024 bytes,
SHA-256 `8e442907...bcd7`), headed `TEST 71`, `BUILD 4ee37937a022`.

Owner's observation: "Video 1 was flawless as per usual. Video 2 played for
2 seconds, stopped, played for another half second, stopped. It was a
slideshow, but each time it would show a frame before, this time it would
show 3-4 frames before stopping again."

| | First movie (B) | Second movie (I) |
|---|---|---|
| Frames submitted of the movie's | 331 of 331 | 524 of 1,968 (Test 70: 436) |
| Of those on time and in sequence | 329 | 481 |
| Frames skipped | 0 | 1,444, from frame 25 (Test 70: 1,532) |
| Longest gap between frames | 50 ms after frame 215 | 2.15 s after frame 1,021 |
| Requests that waited for the window | 1, for 21 ms | 199, 4.60 s in all, at most 48 ms |
| Player exit | `7DC` | `7DC` at frame 1,968 |

| Caller | Visits | Time | Longest | Test 70 |
|---|---|---|---|---|
| SCI interrupt | 35,800 | 5.40 s | 5.54 ms | 6.18 s |
| Timer | 7,901 | 0.03 s | 15 us | 0.04 s |
| The game's calls | 193,622 | 9.99 s | 8.41 ms | 10.44 s |
| All | 237,323 | 15.42 s | | 16.66 s |

The second movie's course, twelve pieces of 5.4 s from its start (the last
also holds the movie's final second):

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted (161 is all) | 46 | 2 | 24 | 12 | 11 | 7 | 7 | 3 | 14 | 21 | 162 | 215 |
| Sectors served | 1,712 | 2,464 | 2,352 | 2,416 | 2,352 | 2,336 | 2,368 | 2,416 | 2,448 | 2,496 | 608 | 1,680 |
| Requests waited, s | 0.15 | 0.36 | 0.06 | 0.13 | 0.06 | 0.17 | 0.34 | 0.65 | 1.59 | 1.01 | 0 | 0 |
| Reader in interrupt visits, s | 0.31 | 0.44 | 0.38 | 0.38 | 0.36 | 0.38 | 0.40 | 0.46 | 0.61 | 0.55 | 0.10 | 0.25 |
| Reader in the game's calls, s | 0.52 | 0.67 | 0.73 | 0.73 | 0.69 | 0.69 | 0.69 | 0.67 | 0.71 | 0.73 | 0.65 | 0.78 |
| Streams given up | 8 | 2 | 15 | 10 | 8 | 5 | 6 | 2 | 14 | 17 | 20 | 13 |

Stream: 219 begun, 6,214 full runs, 30,956 overruns in a run, 10,062 timer
waits (3.59 s), 1,577 stops for room, 72,184,733 wire bytes, TMU1, no
fault. Blocks 134,504: 14,630 rebuilt from CRC16, 19 single losses not
explained, 8,411 handed on with lost bytes, 72 with only a CRC byte lost.
Sectors filled from parity 3,640; 1 beyond the fill, 5 failed their check.
One clean block failed its CRC16. Streams ended: close 11, sector 6, frame
202. Restarts cost 1.19 s in all, at most 35 ms. 10,302 visits ran out of
time. No game read failed. `F END HELD TWICE`: 201, 2, 0.

What the run establishes:

1. The loop gained less than estimated: the reader's time is 15.4 s
   against 16.7 s, not under 14 s. Reading blocks is the smaller part of
   the visits; their fixed cost is the larger.
2. The second movie plays at full speed for its last twelve seconds (162
   and 215 frames in the last two pieces). That part of the movie needs a
   third of the data: 608 and 1,680 sectors a piece against about 2,400.
   Its first 54 s need 435 sectors a second, 1.02 MB/s of raw data, more
   than the movie's average.
3. Through the slideshow the player is short of CPU, not of data. In
   pieces 1 to 7 requests waited between 0.06 and 0.65 s of each 5.4 s,
   and the player took its data at 1.1 times the movie's own pace: it works
   through the movie only a little faster than it plays, so once behind its
   clock it stays behind and shows the odd frame. It catches up for three
   or four frames at a time, as the owner saw.
4. Two thirds of the reader's time is in the game's calls, and that time
   does not follow the data: about 0.7 s in every piece, 0.65 s where 608
   sectors were served and 0.73 s where 2,496 were. The game calls the
   reader 193,622 times, some 2,500 a second, of which about 2,300 serve a
   request. The others are the player's update calls, each of which the
   hook in `toy_movie_exit_hook.inc` turns into a visit through
   `kui_toy_movie_ahead_update`. At 52 microseconds each inside the reader,
   and the gate and dispatcher around it unmeasured, they take more than a
   tenth of the CPU and mostly find nothing to do. Test 72 acts on this.
5. A held byte stored twice is not the cause of the positions that are not
   lost bytes. All 201 unexpected bytes lie directly behind a block's end,
   but only two of those blocks have a byte stored from the receiver's
   register at all and none repeats the byte before it. The extra position
   is one where the channel had already taken the held byte: an overrun
   flagged with nothing lost behind it, or one byte more at the resume.
   Which is not known. Either way the reader could try the block without
   each of its lost positions before giving the stream up; the CRC16 would
   settle it for a block with one such position. A later change.
6. Restarts rose again to 208 (120 while the second movie played), 1.19 s
   and about 3 MB of wire bytes received twice. The two pieces where
   requests waited longest (8 and 9) had 14 and 17.

## Verification

Host and linked checks only; none is a console result.

Source commit `4ee37937a022` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-71/`.

| Check | What ran | Result |
|---|---|---|
| The loop computes what the byte steps compute | In every host build of `toy_wire_stream.c` with the switch, each word run is recomputed by the byte steps and compared: CRC16 and every byte | Held over 180,011 blocks and 528,436 overruns in the stream test and over every scenario of the integration test |
| The loop as linked | `tests/test_toy_wire_word_loop.py` on the auxiliary's ELF (`linked-loop.log`) | 49 instructions for four bytes; no stack access; one word from the ring, four CRC table loads, four reversed bytes stored |
| The gate as linked | `tests/test_toy_wire_cached_stack.py` | Unchanged from Test 70 |
| Whole reader | `tests/test_toy_wire_integration.c` in all four configurations | Every request completed with the image's exact bytes. The course's pieces add up to the report's totals; a piece is 2^22 ticks from the movie's start and the last keeps the rest; nothing is added once the movie is over. |
| A held byte stored twice | Stream test: one overrun at which the channel takes the held byte and the receiver still shows it | The block has one position too many, its single lost byte is not explained by the CRC16, and the reader finds the block's own last CRC byte directly behind its end: the console's pattern. Counted in all three `F` values. This shows the cause is consistent with the notes, not that it is the cause. |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All 23 rows fit at a full run's values (988 of 1,012 bytes); the 16 rows the test is read by end at byte 681 |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` | 51 of 51 deliberate defects caught, 13 of them in what only Test 71 has |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `73e8f9ed...a26f` |
| Earlier deliveries | Unchanged recipes on this source | Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Limits. The loop's result is checked on the host as compiled for the host;
on the console it is the same C source as compiled for the SH-4, read as
linked but not run. The estimate of the gain rests on counted instructions.

## Package

`KUI-Toy-Wire-Stream-Test-71-14NOV2026-0657-CST-4ee3793.zip`, 1,802,197 bytes, SHA-256
`2bfd898899dc5c515153dc24fc4d686d30c8837d8e1c8726ed55ce8fa736b2fa`, prepared 2026-11-14 06:57 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `4ee37937a022` | `939D6FDC` | `73e8f9ed...a26f` | Changed: Test 71, replaces the Test 70 module `b492f00a5478` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 BUILD_ID=4ee37937a022 build/toy-suite-71/retail-toy-test-71.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test \
  toy-wire-linked-check
python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 71), CD Music and virtual VMU Off. Confirm `TEST 71` and
`BUILD 4ee37937a022` in the saved `/reports/toy/66-N.txt`.
