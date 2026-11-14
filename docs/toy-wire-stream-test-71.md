# Toy Commander Test 71: the ring reader's block loop, a word at a time

Status: host- and linked-qualified. It has not run on a console.

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
