# Toy Commander Test 70: the auxiliary's private stack through the cache

Status: host- and linked-qualified. It has not run on a console.

## Baseline and the one change

- Baseline: Test 68, build `806015566fc7`
  (`docs/toy-wire-stream-test-68.md`), as it ran on the console: first
  movie fluid, second a slideshow.
- Test 69 (`docs/toy-wire-stream-test-69.md`) regressed on the console and
  is not built on. Its timer continuation is withdrawn: the build refuses
  `TOY_WIRE_TICK` together with this test's switch. Its report rows are
  kept, because they measured what this test acts on.
- What Test 69 measured. The reader spent 32.4 s in its visits over about
  76 s of movies, and the first movie skipped 107 frames with its data on
  time. The movies lack CPU, not data.
- Where that time goes, from the compiled code. Every visit runs on the
  auxiliary's private stack, which the gate installs at its uncached
  address. The ring reader's per-word loop, compiled for size, keeps its
  temporaries there: four writes and five reads for every four bytes, each
  a bus cycle of some 70 ns. For one block that is about 90 of the loop's
  145 microseconds. Everything else a visit runs pays the same for every
  saved register and local.
- The one behavior change: the gate uses the private stack, and reads its
  two guard words, through the cached address of the same page.
- Decision the console test answers: how much of the reader's time this
  removes (the `V` rows against Test 69's), and what that does for the two
  movies (against Test 68's).

Estimate, from instruction and access counts and not from any measurement:
the ring reader's loop falls from about 145 to about 70 microseconds a
block, a block all told from about 185 to about 105, and the reader's total
from 32 s to about 18 s. If the game's cache setting writes through (see
`P CCR` below) only the reads gain and the total is nearer 24 s.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Compiled without the switches; byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The visit gate (`src/loader/native_readahead_aux_entry.S`). The C code differs from Test 68 only by the report's counters. Image 23,744 of 24,576 bytes. |
| `native-worker` | The report's rows and title. |
| stage | None. It checks, clears and seeds the stack page at its uncached address as before. |
| Card Tools | Not rebuilt. 09c9a4b686d4 loads the module from its Test 66 path. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1`.
Without `TOY_WIRE_CACHED` the Test 68 module reproduces byte for byte, and
with `TOY_WIRE_TICK` instead the Test 69 module does.

## The change

The stack page is linked, exported, checked and seeded at its uncached P2
address (`0xac427000..0xac428000` before relocation), unchanged. On entry
the gate now reads the MMU's control register. With the MMU off it
subtracts `0xa0000000` from the three linked addresses it uses (the stack
top less 32 and the two guard words), which gives the P0 address of the
same page; with the MMU on it subtracts nothing and behaves as before. The
adjustment is kept in R12 across the dispatch, which is saved and restored
on the caller's stack with R8 to R11.

Why P0 and not P1. Both go through the operand cache. Whether a write stays
in the cache or is also written to memory at once is set by the game's
cache control register separately for the two: bit 2 for P1, bit 1 for P0.
The settings this project has seen (`0x105`, `0x909`) both leave P0 in
copy-back mode; the second leaves P1 write-through. The report now carries
the register so that this is known for Toy Commander.

Why it is safe:

- A visit owns the page for its whole length with interrupts blocked
  (`SR.BL` set by the gate), and needs nothing of it once it returns.
  Whatever the cache still holds of it afterwards is dead; if it is written
  back later it lands in the same page.
- Nothing else touches the page. The stage's writes are made once, through
  P2, after it has purged the lease's cache lines, before the first visit.
  The auxiliary's C code takes no absolute stack address, and the
  movie-player hooks that enter the auxiliary without the gate run on the
  game's stack.
- The guard words are read through the same address the stack is used at,
  so a stack that ran into them is seen through the cache too. The top
  guard's cache line is above the first push and the bottom one a page
  below it: in normal use neither is written.
- With the MMU off a P0 address is the physical address with no lookup, so
  no exception is possible under `SR.BL`. With the MMU on the gate does not
  use P0.
- If the game has the cache off, P0 is a plain memory access: Test 68's
  behaviour at Test 68's cost.

What is not known until the console run: the game's control register
values, and so whether writes stay in the cache; and the size of the gain.

## Known and not changed here

In order of what they are estimated to be worth, each its own later change:

1. The ring reader's per-word loop itself: 79 instructions for four bytes.
   Written to keep its values in registers, with the CRC16 taken two bytes
   to a step and the bit reversal done on whole words, about 46. Estimated
   30 microseconds a block.
2. The sector check after a sector whose blocks all passed their CRC16
   without a lost byte: the EDC is then a second check of the same bytes,
   about 90 microseconds a sector. The address check would stay.
3. The auxiliary's context page, which is also at its uncached address. It
   is read by the worker and checked by the stage at that address, so
   moving it touches three modules.
4. Reads of the ring through the cache, a line at a time.
5. A recorded lost position that was not a lost byte (Test 69, finding 4):
   17 restarts in that run, 104 ms.
6. The review findings listed in `docs/toy-wire-stream-test-69.md`.
7. Whether the SCI can deliver bytes most significant bit first, which
   would remove the bit reversal altogether. A hardware question that
   needs its own probe.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 70`. The rows are Test 69's, with one difference:

| Row | Meaning |
|---|---|
| `P CCR MMU BUDGET RT RMAX` | The cache control register (`0xff00001c`) and the MMU control register (`0xff000010`) as read when a window opened; visits that ran out of time with blocks left; ticks from a stream given up to its successor receiving, in all and at most |

`V SCI TMR GAME N TICKS`: the timer's visits in this build only end a wait,
as in Test 68, and are counted.

## Reading it

- Sum of the three `V` tick values against Test 69's `74CB63 + 54EADE +
  B8248C` (32.4 s): the reader's time with the stack cached. Per visit of
  each kind as well: the game's calls were 136 microseconds each.
- `P MMU` even: the cached address was used. Odd: it was not, and the run
  is Test 68 with more report rows.
- `P CCR` with bit 1 clear (last digit 1, 5, 9 or D): stack writes stay in
  the cache. Bit 1 set: they are written through and only reads gained;
  the stack would then be better at P1 if bit 2 is set.
- `I SUBMIT`, `I SKIP` against Test 68's `13F` and `671`; `B SKIP` against
  Test 68's `E`.
- `Q` is large whenever the player skips, since it then reads as fast as
  it is served. It says something only once the skipping has stopped.

## Verification

Host and linked checks only; none is a console result.

Source commit `b492f00a5478` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-70/`.

| Check | What ran | Result |
|---|---|---|
| The gate as linked | `tests/test_toy_wire_cached_stack.py` on the auxiliary's ELF: the gate's instructions in order and its literal words (`linked-gate.log`) | The MMU register is read; both guards are read through the adjusted address before anything is written and again after the dispatch; R12 is saved, carries the adjustment and is restored; the stack is the linked top less 32 less the adjustment; R4 to R7 reach the dispatcher untouched. Stack at `0x0c427fe0`, guards at `0x0c427000` and `0x0c427ff0` before relocation. The Test 68 and Test 69 builds of the same source show the old gate, and each check refuses the other kind. |
| Relocation | `sh-elf-readelf -r` on the gate object | Five relocations as before: the three stack words are still the linked symbols; the register address and the alias constant are plain words |
| Whole reader | `tests/test_toy_wire_integration.c` in all three configurations | Every request completed with the image's exact bytes. The report's visit counts agree with the model's interrupts and calls. |
| Card stream, lost-frame note, parity fill, report | `toy-wire-host-tests` in all three configurations | Pass. All 21 report rows fit at a full run's values (1,001 of 1,012 bytes). |
| What a cheaper reader does | `playing` in the integration test with the reader's cost halved and cut to a third, nothing else changed | On a quiet bus a frame's data is ready 0 of 21 times at the measured cost, 20 of 21 at half, 21 of 21 at a third, without any timer. The model's arithmetic, not a measurement of this build. |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` | 38 of 38 deliberate defects caught. The gate's assembly is outside its reach. |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; otherwise only the 12 label bytes differ |
| Reproducible | Clean worktree and working tree | Same module, SHA-256 `7802a1d7...d7dd` |
| Earlier deliveries | Unchanged recipes on this source | Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Limits. No host test runs SH-4 code or models the cache: the change itself
is checked only as linked text and by reasoning about the hardware. The
estimate of the gain rests on counted instructions and an assumed cost per
uncached access.

## Package

`KUI-Toy-Wire-Stream-Test-70-14NOV2026-0131-CST-b492f00.zip`, 1,801,717 bytes, SHA-256
`196a1e9caf2392bcc2824c083bc5c012f003694f091c7f242866c7d8014aaa97`, prepared 2026-11-14 01:31 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `b492f00a5478` | `3A86E651` | `7802a1d7...d7dd` | Changed: Test 70, replaces the Test 69 module `fb625c09dde6` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  BUILD_ID=b492f00a5478 build/toy-suite-70/retail-toy-test-70.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test toy-wire-linked-check
python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 70), CD Music and virtual VMU Off. Confirm `TEST 70` and
`BUILD b492f00a5478` in the saved `/reports/toy/66-N.txt`.
