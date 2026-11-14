# Toy Commander Test 74: the reader's own work cut down

Status: delivered 2026-11-14 (build `e82b31657f69`), host and linked
checks only. It has not run on a console. Not accepted.

Test 74 is not one change. The owner asked, after Test 73, for one build
that carries everything likely to help: "Put a lot of thought into this
next build. See what you can create that will fix all my problems with
FMVs." The standing rule of one behavior change per console test is set
aside for this build at that request, and only so far: each change is a
source commit of its own under a switch of its own, any of them can be
left out of a later build, and the package carries a second module
without the one change whose effect on the hardware is not known. If the
combined build regresses against Test 73, the next step is to isolate
against Test 73 with those switches, not to add to it.

## Baseline and what the last run showed

- Baseline: Test 73, build `e623c2a697d5`
  (`docs/toy-wire-stream-test-73.md`), as it ran on the console (report
  66-7): the first movie as from the disc; the second movie showing 513
  of 1,968 frames.
- The reader's time is the reading itself. 15.12 s in pumps for 134,563
  card blocks, 112 microseconds a block, in every test since 71, whoever
  visits. Tests 72 and 73 removed visits; the visits that were left did
  the same reading.
- The second movie's player has no idle time. It works through the
  dense first 54 s at 31 to 33 ms a frame, shown or not. By the estimate
  in the Test 73 document it needs 19 to 20 ms of that for itself and
  the reader takes the rest. The movie sits on the edge.
- As linked in the Test 73 module, a byte of a block costs about 22
  instructions: 49 for four bytes in the block loop, 6 for four in the
  copy to the window, 16 for two in the sector's EDC over 2,064 of each
  sector's 2,352 bytes, and the bookkeeping around them.
- A correction to that document. Its "61 microseconds a delivery visit
  outside the pump" is the dispatcher's whole time in those visits less
  the pump, and that includes the low reader's own reads at the start of
  each movie, every copy of a request's sectors out to the game, and
  whatever else the full path did. It is not the cost of a visit that
  finds nothing to do, which is a few hundred instructions. The gain
  from shortening that path (QUICK below) is therefore small, and this
  build measures it instead of estimating it: the game's GD calls are
  timed whole.

## The changes

All behind `TOY_WIRE_LEAN=1` on Test 73's recipe. Each part defaults to
on with it and is left out with `TOY_WIRE_<PART>=0`.

| Commit | Part | Change |
|---|---|---|
| `d450fb1` | base | The switch, the report's layout, the game's GD calls timed whole, runs cut again soon after they began counted. Nothing in what is read, checked or served. |
| `6caf9cd` | `OPEN` | No hold on the second movie's refills at its start (Test 57's), and the window serves that movie from its first request. |
| `b87e14a` | `CLEAN` | A sector whose blocks were all checked by their CRC16 is not also passed through its EDC. |
| `a9d7d27` | `MSB` | The SCI is asked for bytes most significant bit first; the first token received says whether it did. |
| `63a2084` | `QUICK` | A delivery visit with no request and nothing to decide goes straight to the pump's own rule. |
| `9abfa56` | `SWIFT` | The block loop every received byte goes through, written by hand for the SH-4. |
| `7ca6a5f` | | The table builder's statements kept in Test 73's order, so that the earlier recipes reproduce (see Verification). No build computes anything else. |
| `e82b316` | | The linked loop check for a build without `MSB`. Tests only. |

Not changed: every card block's CRC16 is taken and checked; the low
residents are byte-identical to Test 66's but for the build label; the
stream's run and wait policy, the window's size and the request path are
Test 73's; a request that waits pumps every time; the first movie's
route is the second's.

### OPEN

Test 57 held the player's refill reads back at the second movie's start,
for up to 30 frames while its buffer stood above 384 KiB, so that the
low reader's blocking reads would not disturb the first frames; the
window did not serve that movie until the hold was over. Test 73's
report shows what that costs with the card stream: at the first skip
(frame 27) the buffer held 362,241 bytes and the next chunk was
complete, so the skip was not for want of data; the reader was starting,
reading ahead and keeping pace at once beside a buffer run half empty.
Every run since Test 68 skipped first between frames 20 and 27.

`kui_toy_movie_refill_defer` now answers no, `toy_movie_ahead`'s choice
of the window no longer waits for the hold to end, and the hold's code
and its reads of the player's buffer objects are not compiled (804 bytes
of the auxiliary image).

### CLEAN

The stream classifies every block it hands on
(`enum kui_wire_kind`, `src/loader/toy_wire_stream.h`): clean (no byte
lost, CRC16 remainder zero), rebuilt (one lost data byte solved from the
CRC16), or neither. The window (`toy_wire_sector` in
`src/loader/toy_wire_ahead.inc`) then takes a sector one of two ways.

The lighter way, for a sector made of clean blocks alone that follows
the sector accepted last: its mode byte is 1 and its three address bytes
are the binary coded decimal successor of that sector's
(`src/loader/toy_wire_msf.h`). A rebuilt byte keeps its sector on this
way if the sector confirms it: the P code word through the byte, 26
bytes 86 apart from byte 12, sums to zero
(`toy_wire_mend_holds`). A wrong block passes its CRC16 once in 65,536
times; a block rebuilt wrong has to leave a remainder a single byte can
leave (once in 256) and then fit its code word (once in 256).

The full way, parity fill and EDC as in Test 73, for everything else: a
sector with a block that was neither clean nor rebuilt; a rebuilt byte
in the Q parity (from byte 2,248), in the other sector its block
touches, or one its code word does not confirm; more than six rebuilt
bytes in a sector; every block the low reader brings, the first sector
of every stream among them; a sector whose predecessor is not the one
accepted last.

One in sixteen of the sectors taken the lighter way goes through the EDC
all the same. A single one that fails is refused and read again,
switches the lighter way off for the session, and is counted (`LB`).

What is given up, deliberately. A block stored wrong on the card with a
CRC16 that fits what is stored, no byte lost on the wire, passes every
check of the stream. Test 73's EDC refused such a sector; Test 74 serves
it as stored unless the sample meets it. The transport is checked as
before; the card's stored data only by sample. This is the check the
other launch families have: the sector EDC is taken only by the Toy
movie window and its predecessors (`toy_movie_ahead.inc`,
`toy_resume_verify.inc`, the Test 1 to 66 stages), not by the demand
readers. The standing requirement, every SD block's CRC16, is met.

Whether an image carries real parity is no longer asked once: a rebuilt
byte its code word does not confirm goes to the EDC each time.

### MSB

The SCI delivers every byte bit-reversed and the reader turns each one
round by table: four loads and four stores in every turn of the block
loop. `SCSCMR1.SDIR` (0xffe00018 bit 3) selects conversion most
significant bit first. The SH7091's manual describes the bit for the
smart card interface. Whether the SCI honours it in clocked synchronous
mode is not known to me; KallistiOS's SCI SPI driver reverses in
software and never writes the register. No host test can say.

So the stream asks, and the first token of a stream answers:

- `SDIR` is written at each run's start with the SCI stopped
  (`wire_start_dma`), and cleared at the top of every handoff
  (`wire_handoff`), before the module reset, before a command byte, and
  before the low reader has the SCI again. The low reader's own entry
  (`kui_sci_sd_acquire`) and its module reset write zero there as they
  always have.
- 0xfe as sent: the receiver did it (`ORD` 2). Until a block with no
  lost byte has passed its CRC16 in that order, nothing is rebuilt from
  a CRC16 taken in that order; such a block goes on with its lost byte
  listed, to its sector's parity and EDC. The first such block that
  passes confirms the order for the session (`ORD` 3). One that fails
  ends the asking (`ORD` 4) and the next stream reads as Test 73 does.
- 0x7f: the bit does nothing here (`ORD` 1). The same stream goes on as
  Test 73 does and the bit is not written again.
- Anything else, or a byte lost ahead of the first token: that stream is
  given up undecided and the next asks again; after four such streams
  the asking ends (`ORD` 1).

Read as sent, a block's CRC16 is the standard CRC16-CCITT. Its table is
stored byte-swapped, which makes the step the very one the reflected
table takes; every loop, the two-byte step and the rebuild of a lost
byte run unchanged on either table.

Risk. If the SCI did something else with the bit than these three
answers, the stream's checks stand as before (token, CRC16, sector
check), the window is given up after repeated failures and the low
reader plays the movie as Test 66 does. That is the expected worst case,
not a tested one. The package's second module is this build with
`TOY_WIRE_MSB=0`: no code in it sets the bit; it writes zero to the
register at each handoff, as every test since 68 has.

### QUICK

`toy_ahead_plain` (`src/loader/toy_movie_ahead.inc`) names the state in
which `toy_movie_ahead` does nothing but apply Test 73's rule for idle
calls: a window, no request under way, the region not lent out, the low
reader not mid-resume, no error, the movie still playing, the last
request completed, its command a read or none, the reader open, the bus
acquired and not faulted. For a delivery visit in that state the
dispatcher applies the rule itself and returns. An interrupt visit
pumps; one of the game's calls pumps when the rule says so. A pump that
ends in error gives the window up as the whole path does, and what the
fallback leaves to the low reader is handed to the whole path in the
same visit.

Not on the short way: the delivery visits' own clock reads and the
dispatcher's count of them. The pump times itself as before. The
report's `D` row therefore counts the visits that took the whole path,
and `G` every GD call whole.

### SWIFT

`src/loader/toy_wire_words.inc`: `kui_wire_words_lsb` and
`kui_wire_words_true`, the two-byte CRC16 steps and the four byte stores
of `wire_words`, 41 and 34 instructions for four bytes against the
compiler's 49 and 45. The remainder is kept cut to 16 bits, which lets
one shift bring the word's upper half into place; the reverse table is
indexed straight from the register the stores go through.

### Considered and left out

- Reversing a word's bits by masks and shifts instead of by table: as
  linked no shorter than the table on the SH-4, whose logical and shift
  instructions do not pair.
- Reading the ring through the cache: half a millisecond a frame at
  best, for a new ordering hazard between the DMA and the cache.
- Writing blocks straight into the window's slot, a shorter gate for the
  visits that only count, and another policy for restarting a run after
  an overrun. Each is a change of its own; the report's `R` row is there
  for the last.

## Estimate

From instruction counts and the model, not from any measurement.

- Work on a byte in the pump, per four bytes: Test 73 about 89 (loop 49,
  copy 6, EDC 28, bookkeeping 6). With 60 to 65% of sectors taken the
  lighter way, as the model gives with stalls at the console's rate and
  as the console's own counts of holed blocks allow: EDC 12, loop 41 or
  34. About 65 with bytes LSB first, 58 with bytes as sent: 27% and 35%
  less.
- Pumps: 15.1 s to about 11.4 s, or 10.3 s with `ORD` 3. The reader in
  all, with the dispatcher, the trace visits and the gates put at 4 s as
  in the Test 73 document: about 19 s to 14 or 15 s.
- In the second movie's dense part that is 2 to 3 ms of each frame for a
  player that today needs 31 to 33 ms for a frame of 33.3.

What that does to the picture is not known. It should show from the
first seconds (OPEN) and through the dense part; whether it is enough
for every frame is the console's answer.

## Module roles

| Role | Change |
|---|---|
| `resident-sci`, `resident-legacy` (low residents) | None. Byte-identical to Test 66's with its build label. |
| `native-ra-aux` (auxiliary) | The five parts and the report's counters. Image 24,416 of 24,576 bytes: 160 are left. Without `MSB` 23,904. |
| `native-worker` | The report's rows and title. Identical in the two modules of the package. |
| stage, Card Tools | None. |

The build switch is `TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1
TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1`. Without
`TOY_WIRE_LEAN` the Test 73 module reproduces byte for byte.

Code space by part, bytes of the auxiliary's text: base -540 (the
measurements no longer taken), OPEN -804, CLEAN +504, MSB +560, QUICK
+252, SWIFT -76.

## Known and not changed here

1. Streams that cannot be followed (260 begun in Test 73's run).
2. The auxiliary's context page at its uncached address.
3. Reads of the ring through the uncached alias.
4. The gate and the dispatcher are entered three times for each of the
   game's EXEC and CHECK calls. `G` now measures what such a call costs
   whole.
5. The review findings listed in `docs/toy-wire-stream-test-69.md`.
6. A multi-agent review of this design was started and ran out of its
   usage allowance with one of six reviewers finished (the short way's
   error path, fixed as described under QUICK). The rest of the design
   was reviewed by its author only.

## The report

Card Tools names the file after Test 66 (`/reports/toy/66-N.txt`); the text
says `TEST 74`. In order:

| Row | Meaning |
|---|---|
| `PIPE`, `AHEAD`, `I SUBMIT`, `I FIRST`, `B SUBMIT`, `IP`, `BP` | As Test 73. `AHEAD`'s fourth value is every sector taken into the window, by either check |
| `IC RDY WGD NOGD` | As Test 73 without its last value |
| `K LIGHT EDC MEND MB LB ORD` | Sectors taken the lighter way; sectors through the EDC; rebuilt bytes their P code word confirmed; rebuilt bytes it did not (the EDC decided); sectors the lighter check passed and the EDC sample refused; the bit order (`enum kui_wire_order`): 0 not asked or no stream, 1 the bit is ignored or the asking was given up, 2 token as sent and no block yet, 3 as sent and confirmed, 4 token as sent and the first block failed |
| `V SCI TMR GAME N TICKS`, `U S W D G S W D` | As Test 73 |
| `D N T G N T` | Delivery visits that took the whole path and their ticks; the game's GD calls while a movie's request is the last one, timed from the resident's entry visit to its exit visit, and their ticks |
| `TF`, `TD`, `TW` | As Test 73 |
| `W START`, `W BYTES`, `W BLK` | As Test 73 |
| `R C8 32 128 W8 32 128` | Runs an overrun cut again within 8, 32 and 128 bytes of their start: those begun at once behind a cut, then those begun after a timer wait |
| `W FILL BYTES MOST REF BAD` | Sectors completed from parity, bytes filled, most lost bytes in one sector, sectors the parity could not complete, sectors that failed their check |
| `TI`, `TG` | As Test 73 |

Gone to make room: `IB`, `TB`, `TL`, `IC`'s `OTH`, and `D`'s old form.

## Reading it

- `I SUBMIT` against Test 73's `201`, `I SKIP` against `5AF`, `SF`
  against `1B`: the first skipped frame is OPEN's question.
- `B SUBMIT 14B`: the first movie must stay as it is.
- `V`'s three tick values added, by `W BLK`'s first: the pump time a
  block. Test 73: 0x5EAB12 + 0x57F4 + 0x553586 ticks for 0x20DA3 blocks,
  88 ticks, 112 microseconds.
- `K`: `LIGHT` against `EDC` is the share the lighter check takes with
  the console's stalls; `MB` near zero on an image with real parity;
  `LB` must be 0; `ORD`.
- With `ORD` 3, the same run with the second module gives what the bit
  order alone is worth.
- `G`'s ticks by its calls: a GD call of the game's, whole. `D` against
  Test 73's `1D742` visits: how many still take the whole path.
- `R`: if runs begun at once behind a cut are cut again within a few
  bytes far more often than runs begun after a wait, receiving on at
  once is not worth what it costs.
- `IP` against `BP` as in Test 73: `OT / ON` in the second movie against
  a frame's 26,042 ticks.

## Verification

Host and linked checks only; none is a console result.

Source commit `e82b31657f69` (clean worktree, `source_dirty: false`).
Logs are in `docs/evidence/toy-wire-stream-74/`.

| Check | What ran | Result |
|---|---|---|
| Whole reader, each part | `tests/test_toy_wire_integration.c`: the real low reader, auxiliary and stream on the card model; under `MSB` the whole scenario list twice, with a receiver that ignores the bit and one that honours it | Passed in Test 74 (219,240,759 checks) and with each part left out in turn; every request completed with the image's exact bytes |
| The lighter check | `steady()`, `wrong_blocks()`, `stored_wrong()`, `no_parity()` | Quiet contiguous run: 274 sectors the lighter way, 16 through the EDC, 81 rebuilt bytes confirmed. Five constructed wrong blocks (rebuilt wrong from a CRC16 that fits other data; wrong byte in the sector before or behind the lost byte; CRC16 lost; another address than its place) each refused by the rule built for it, the sector read again, data exact. A block stored wrong with a fitting CRC16, in 40 places: served as stored 34 times, met by the sample 3 times, which then switched the lighter check off. An image without parity plays, every rebuilt byte passed by the EDC |
| The address successor | `tests/test_toy_wire_msf.c` against `src/core/hash.c` | Every sector from 150 to 719,998: 9,597 second carries, 159 minute carries, one past 99 minutes |
| The bit order | `tests/test_toy_wire_stream.c`, three answers of the wire model (ignored, honoured, token only); `order_given_up()`, `order_unconfirmed()` | Each run read right; the register asserted zero whenever the low reader has the bus, at every module reset and command framing, and written only with the SCI stopped; four undecided streams end the asking; nothing rebuilt before a block has confirmed the order |
| The short way | `window_gone()`: eleven ways a window is given up, by a call and by an interrupt (`window-outcomes.log`) | The same outcome line in the Test 73 configuration and in Test 74 |
| The hand-written loops as linked | `tests/test_toy_wire_linked_loops.py` with the interpreter of `tests/sh4_mini.py` (`linked-loops.log`) | Each routine equal to the byte steps on 400 random runs, a block's CRC16 taken to zero and a damaged block's not, nothing written outside the destination, registers and stack restored; 41 and 34 instructions for four bytes. The interpreter agrees with the compiler's loop of the Test 73 module on the same data (49) |
| The gate as linked | `tests/test_toy_wire_cached_stack.py` | Unchanged from Test 70 |
| Card stream | `tests/test_toy_wire_stream.c` | 179,992 blocks checked against the card over 524,878 overruns; none handed on as clean or rebuilt had a wrong or missing byte |
| Report | `tests/test_toy_wire_report.c` on the production formatter | All rows fit at a full run's values (1,005 of 1,012 bytes); the rows the test is read by end at byte 856 |
| All configurations | Both host targets in Tests 68 to 73 and in Test 74 with each part left out (`matrix.log`) | 24 of 24 passed |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` (`mutation-check.log`) | 125 of 125 deliberate defects caught, 55 of them in what only Test 74 has |
| Other suites on the shared wire model | The SCI stream, retail async, read-ahead auxiliary, Toy movie, CE completion, quarantine and Smash raw suites that include `tests/sci_stream_model.h` (`other-suites.log`) | 17 of 18 passed. `test-sci-stream-quarantine` stops before it reaches the model, on its fingerprint of `src/loader/sci_stream.c` ("Historical complete SCI source differs"). It stops the same way at Test 73's commit; Tests 68 to 74 do not touch that file or that test. Not caused by this change and not repaired here |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes of both modules | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label; the same in both modules |
| Reproducible | Clean worktree of the commit | Module SHA-256 `9e3bb2b2...d4e4`, second module `582e2d7a...d86c` |
| Earlier deliveries | Unchanged recipes on this source | Test 73 `7f28c744...7124`, Test 72 `04c6b439...fe9b`, Test 71 `73e8f9ed...a26f`, Test 70 `7802a1d7...d7dd`, Test 69 `c6baf010...8beb`, Test 68 `ae72c817...c17a`, Test 66 `06f34778...f8d2` and Test 67 `7533fb8b...f4e5` reproduce |

Two things this verification found and changed. The first rebuild of the
earlier recipes did not reproduce Tests 68 to 73: the MSB commit had
moved one store in the table builder, the same computation in another
order, and the compiler laid `kui_wire_start` out differently (648 bytes
of the auxiliary). Commit `7ca6a5f` puts the store back. And the linked
loop check failed on the second module, which has no loop for bytes as
sent; commit `e82b316` teaches the check. Both were found before
anything was delivered.

Limits. No host test can say what the SCI does with `SDIR`; the model's
three answers are assumptions. The hand-written loops were run by an
interpreter written for this work, held against the compiler's code but
not against a console; instruction counts are not times. The model's
stalls are drawn at rates like the console's, not recorded from it, so
the share of sectors taken the lighter way is an estimate. The player's
memory in the model is the few words the hook reads. The game's
executable was not available to these checks. In the model the clock
advances with every read of it, so counts of calls "as due" differ
between builds that read the clock a different number of times; that is
the model and not the reader.

## Package

`KUI-Toy-Wire-Stream-Test-74-14NOV2026-1444-CST-e82b316.zip`, 1,977,812 bytes, SHA-256
`05f815a7f6d48477cfbc947314a15906ccdfeabe67d5384021ceb396f9b932b6`, prepared 2026-11-14 14:44 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `e82b31657f69` | `BC747EF0` | `9e3bb2b2...d4e4` | Changed: Test 74, replaces the Test 73 module `e623c2a697d5` at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |
| `ALTERNATE/KUI/apps/games/retail-toy-test-66.kui` | `e82b31657f69` | `3085DC70` | `582e2d7a...d86c` | Outside the `KUI/` tree: Test 74 built with `TOY_WIRE_MSB=0`, to be copied only if the main module misbehaves |

Retained on the card: everything else, including the Test 67 trace module,
Test 57, reports, saves and settings. No KUP, bank or user data is included.

The two modules carry the same build label. The report tells them apart
by `ORD`, which is 0 only where the bit order is never asked.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 BUILD_ID=e82b31657f69 \
  build/toy-suite-74/retail-toy-test-74.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 toy-wire-host-tests \
  toy-wire-integration-host-test toy-wire-package-test toy-wire-linked-check \
  TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 python3 tests/test_toy_wire_stream_mutations.py
```

The second module: the first command with `TOY_WIRE_MSB=0` and another
`BUILD` directory.

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 74), CD Music and virtual VMU Off. Confirm `TEST 74` and
`BUILD e82b31657f69` in the saved `/reports/toy/66-N.txt`.
