# Toy Commander Test 81: the stream's timer in every title

Status: built 16 November 2026 (build `684f485644e5`), host and linked
checks only. It has not run on a console. A test module, not an accepted
component, and not a replacement for the installed New reader: it has no
virtual VMU and no block prefetch. Delivered together with Test 80
(`docs/toy-wire-stream-test-80.md`), its baseline.

The module is built from `684f485644e5`: Test 80's source and the change
(`494a6e9`), the correction its review asked for (`9d4e338`) and the
tests Test 80's review asked for (`684f485`).

It follows from Test 79's run on the console
(`docs/toy-wire-stream-test-79.md`, Console result), and is the second
of the two things that run showed the stream to lack in another title.
The third of the owner's three changes of 16 November, the virtual VMU
beside the stream, comes behind it: Test 82
(`docs/toy-wire-stream-test-82.md`).

## Baseline

Test 80, delivered in the same package: source `e58dd2e6b641`, module
`c1b58a97...a3dc`. It has not run on a console either; its own baseline
is Test 79 as the owner ran it on 16 November 2026.

What this change is measured against is in that run's reports of both
games (`docs/evidence/toy-wire-stream-79/`), and in Test 78's report of
Toy Commander for a stream that has its timer.

For the owner's question, whether play is smoother than under the
installed prefetch, the comparison is still the installed New reader of
build `f8807cb16561` with the virtual VMU off, in the same games on the
same day.

## Why

| | Evolution, Test 79 | Evolution 2, Test 79 | Toy Commander, Test 78 |
|---|---|---|---|
| The stream's timer (`TMR`) | none | none | TMU1 |
| Runs cut by an overrun | 9,813 | 3,783 | 20,945 |
| Of those answered with a wait | 3,421 | 606 | 9,381 |
| The stream stood for them | 34.6 s | 5.9 s | 5.5 s |
| A wait, on average | 10.1 ms | 9.8 ms | 0.58 ms |

A run of the card stream that a busy bus cuts short within its first
128 bytes is not begun again at once: the game's own DMA has the bus,
and a run begun under it loses a byte and costs an interrupt each time.
The stream waits, 82 microseconds at first and twice that after each
such cut up to 1.3 ms. In Toy Commander a spare timer of the game's
ends the wait. In any other title the stream had no timer, and a wait
ended with the game's next disc call: one a frame in the two Evolution
games, which is the 10 ms. While it stands, nothing is read ahead, and
a read the game has asked for and polls once a frame is a frame later
for every wait in it. Test 79's document named this before the run (its
limit 3); the run showed its size.

The controller's heartbeat would not shorten it: it comes once a frame,
as the games' disc calls do.

## The change

Behind `TOY_WIRE_TIMER=1` on Test 80's recipe.

| | Test 80 | Test 81 |
|---|---|---|
| Whose table the stream looks for a spare timer in | Toy Commander's, by two constants: its vector base `0x8c00f400` and its SDK default callback `0x8c0be408`. In any other title: none | The title's own, by the two words the low resident holds for it: the vector base its SDK table hangs on and the callback that table's unused entries hold |
| When it takes one | The live vector base is that constant; the timer (TMU2, else TMU1) is stopped, has no underflow interrupt and no priority; its entry in the table holds that default | The same conditions, asked of the title's own table and default, at every stream's start |
| What it does with it | Publishes its callback in the entry, gives the timer priority 1, arms it for each wait, and at the stream's end stops it, takes the priority away and puts the default back. The timer's reload, count and control stay as the stream's last stop left them | Keeps the timer's reload, count and control as it finds them. Then the same, and at the stream's end all of it goes back: the three registers, the priority, the title's default |
| A title that takes the timer while a stream has it | Not provided for: the one title was a verified executable that never does | The stream writes the timer only behind a look at it: before each arming whether the entry is still its own and the timer still stopped, at a wait whether the entry is, and at its end both. A timer the title has started and an entry the title has taken are the title's from then on and are not written again. What is still the stream's goes back at that look: the entry while it holds the stream's callback, the priority while it is the stream's 1. The stream's waits then end as without a timer |
| An underflow of the timer outside a wait of the stream's | Answered as the stream's own: the timer stopped | Not of the stream's arming, so the title's: the timer runs on, the entry and the stream's priority go back, and the underflow waits for whoever the title gives the timer to |
| A stream that ends in a fault | The timer stopped | The timer goes back there and then, by the same rule |
| The title's table set up anew while the stream has the timer | The entry given up as the title's, the timer left as it stood | The title's default in the entry is no handler: the timer is stopped and goes back as found, at the stream's next look or its end |
| A title that uses both spare timers | No timer | No timer, as before: Test 79's behaviour |

Where the two words come from. This family's low resident reaches the
SCI's interrupt through the title's own SDK interrupt table. The cold
stage admits a title's table by the title's code, either an executable
it knows by its hash or one whose SDK it recognises by its structure
(`src/core/native_sdk_sci_route.c`), and refuses the launch otherwise.
It then keeps, in the resident's region, the vector base and the
callback the SCI's entries held before it bound its own
(`src/loader/vmu_native_sdk_sci_route_stage.inc`). The resident's
callback is armed only while its cookie is there and the live vector
base is that word, and the stream is begun only with it armed. The
stream takes the words from there.

For Toy Commander they are the constants: nothing changes for it.

Where the timer's entry lies. As Toy Commander's does from its vector
base: four bytes for every `0x20` of the event code, the SCI's error at
`+0x25c`, so TMU1's underflow at `+0x244` and TMU2's at `+0x248`. The
stage admits no title whose SCI entries lie elsewhere. That the timers'
entries of another title's table lie there too, and hold the same
default while unused, is what the SDK's one table for all events
implies and what only a console run shows: where an entry holds
anything else the stream has no timer, which is Test 80's behaviour.

What is not changed: the waits' lengths, when a run is begun again at
once, everything of the window, the sector checks, the low residents,
the worker but for its title.

## What a timer costs a title

The timer's interrupt is a visit of the reader, as the SCI's is: it
ends the wait and begins the next run, and may read what the ring
holds. It runs at the lowest priority, 1, like the SCI's. Toy
Commander's run of Test 78 had 6,767 of them, 5 microseconds each on
average (34 ms in all).

A title's unused timer is taken while a stream runs and given back at
its end, at every stream. Nothing of it is kept between streams: its
entry, its priority and its three registers are then as the title left
them.

## The report

Test 80's rows. `PT` is `1FBD`: Test 80 (`FBD`) and TIMER (`1000`). The
rows that say what the change did:

| Row | Reads as |
|---|---|
| `W BYTES WTICK TMR ERR DET` | `TMR` 2: TMU2 taken; 1: TMU1; 0: none (both in use by the title, or their entries not the title's default). `WTICK` over `W` `WAIT`: a wait's length in ticks of 1.28 microseconds |
| `V SCI TMR GAME N TICKS` | The second count and the second time are the timer's visits |
| `VMAX SCI TMR GAME D` | The second value is the longest of them |
| `W START SEG CUT WAIT FULL` | Runs, cuts and waits as before; with a timer, more runs in the same time |

## Console test

Two games, each once, with this module in the installed module's place,
the New reader (X), the virtual VMU off, cold boot from the CD: first
Evolution 2, then Evolution, each as in Test 79's run: the opening
movie, to where the lag was before the prefetch, a few minutes of play
there. Then the gesture, the three pages photographed, and Start. Run
Test 80 in Evolution 2 first, so that this build's result there has its
baseline from the same day.

Asked of the owner: whether the stutter seen under Test 79 while the
card was read is still there, less, or gone, in each game; the opening
movies as before or not; anything else that changed; the photographs.
And, when the installed module is back in its place, the same spots
under it once: whether the stutter is there too. That one look is what
tells this reader's stutter from the game's.

| Outcome | What it says |
|---|---|
| `TMR` 1 or 2 in both games; `WTICK` over `WAIT` under about `400` (1.3 ms) where Test 79 had about `1E00` (10 ms); the timer's visits in `V` | The stream has its timer in both games and its waits are as short as in Toy Commander. Next: the virtual VMU beside it |
| `TMR` 0 in a game | That title uses both spare timers, or their entries in its table do not hold the default the SCI's did. The stream is as in Test 80 there. Which of the two is the next thing to find out, by a look at the entries the report can be given |
| `TMR` 1 or 2, and waits long all the same | The title takes the timer for itself during play, and the streams give it up (Known limit 8); or the timer does not run out there as it does in Toy Commander, in which case `V` has next to no visits of the timer's |
| The stutter gone or less with the timer | The standstill was a part of it |
| The stutter as before, `TMR` taken, waits short, windows kept (Test 80) | It is neither the windows given up nor the waits. `VMAX` says whether one visit holds a frame; the low reader's overruns (`SD` `OVR`) and the run with the installed module say the rest |
| A hang, a crash, sound or picture wrong, in a game that ran under Test 80 | The timer is not free in that title after all, or its interrupt is not answered as the SCI's is. The change does not hold there. With `TMR` on the report's page, or the point at which it happened |

Toy Commander needs no run of this build for the change: its two words
are the constants it had. A run through the Games menu or Card Tools
would show `TMR` 1 as before.

## Known limits, not changed here

Test 80's limits stand. Added by this change, or made to matter by it:

1. A title that uses both spare timers has no timer for the stream, and
   waits as in Test 79. Nothing else of the title's is taken in their
   place.
2. The stream takes a timer for free by the SDK's table and the timer's
   own registers at a stream's start, and gives it back as found at its
   end. While a stream has the timer its registers are the stream's.
   What it cannot see is a title that drives the same timer by its
   registers alone, with no handler in the table, and leaves it stopped
   between uses. One set up before a stream and started during it
   starts with the stream's reload and clock in it. One set up during a
   stream and left stopped is written over at the stream's next wait or
   at its end. One started and stopped between two of the stream's
   waits is armed by the next. A title that only reads a timer it never
   started reads the stream's counts while a stream runs. No title is
   known to do any of this; the review's first finding has the orders.
   The check that would see them compares the registers with what the
   stream last left in them. It does not fit this build's auxiliary and
   is owed before the reader is anyone's default.
3. Under a wait that stands armed, 1.3 ms at most, the timer is the
   stream's by the stream's own book. A title that takes it then by a
   handler in the entry finds it running as the stream armed it, and
   one underflow of it reaches the title's handler. A title that takes
   it then by its registers alone leaves the wait without an end until
   the window is given up (a quarter of a second where a request
   waits), and the stop that follows stops the title's timer. A wait
   with a timer has no clock behind it; owed with 2.
4. A title that sets its SDK table up anew, or moves its vector base,
   while a wait stands armed: the timer's underflow then reaches the
   SDK's default callback, which answers no event it does not know, as
   the SCI's own interrupt does in the same case (Test 79's limit 6).
   Where a look of the reader's comes first, a table set up anew costs
   nothing: the timer is stopped and goes back. The vector base is not
   asked again after a stream's start.
5. An underflow flag the title left standing in a stopped timer is gone
   after a stream: the flag can be cleared by a program, not set.
6. The timer's entry is looked for where Toy Commander's lies from the
   vector base. A title of this family with the timers' entries
   elsewhere has no timer; none is known.
7. A look and the write behind it are one visit of the reader's, with
   interrupts held, so the title cannot come between them. The title's
   own steps are not held together: a title interrupted between two
   steps of taking a timer for itself, with the timer still looking
   free, can find the stream's arming in it once more, as in 3.
8. The report says which timer the last stream took at its start
   (`TMR`), not whether a stream gave it up afterwards. A title that
   takes the timer from a stream now and then would show `TMR` 1 or 2
   beside long waits.
9. The auxiliary has 32 bytes of its 24,576 left with this change.

## Verification

The module is built from commit `684f485644e5` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree.
Logs are in `docs/evidence/toy-wire-stream-81/`. None of this is a
console result.

| Check | What ran | Result |
|---|---|---|
| The stream by itself | `tests/test_toy_wire_stream.c` on the wire model, twelve courses, every third in another title: its table at another vector base, another default in its entries | Passed. 7 courses had the timer in Toy Commander and 3 in the other title. The timer that title leaves free is taken there as in Toy Commander, its entry holds the stream's callback while the stream runs, and every entry is the title's own again after it |
| What keeps the stream from a timer | `timer_titles()`: another title with both timers free, and each of these by itself: no table admitted; the live vector base not the admitted one; TMU2's entry the title's own handler; both entries; the entries holding Toy Commander's default; TMU2 running; its underflow interrupt on; a priority; both timers running; no table and a vector base of zero; no callback | TMU2 with both free, TMU1 where TMU2 is the title's, none otherwise. After each the title's entries and priorities are as they were, and both timers' reload, count and control equal to the bit what the title had in them |
| What the title does to a timer the stream has, seen at the stream's end | `timer_titles()`: the title puts a handler into the entry; gives the timer a priority of its own; its SDK clears the priorities; the title starts the timer under the stream's entry. And the stream stopped under a wait of its own that stands armed | A handler and a priority of the title's stay. A timer the title started runs on with the title's registers, and the entry and the stream's priority go back. A timer armed for a wait of the stream's is stopped and goes back as found |
| A title that takes the timer while the stream runs | `timer_claimed()`, on a bus that cuts runs short: by a handler in the timer's entry; by starting the timer; by a handler under a wait that stands armed | From then on the stream never writes the timer's count or control, never starts or stops it, leaves an entry the title took to the title, gives back one it still held from its next wait on with its priority, and has no timer for the streams it begins after; forty waits and two hundred blocks more come all the same |
| An underflow that is not of the stream's arming | `timer_underflow()`: the title starts the timer with its interrupt on under the stream's entry, and the underflow reaches the stream outside a wait | The timer runs on with the title's registers and its flag standing; the entry and the stream's priority go back; the stream reads on without a timer |
| A stream that ends in a fault | `timer_fault()`: the channel taken from the stream, with the timer still the stream's, taken by the title with a handler and running, or started under the stream's entry | The timer goes back at the fault by the same rule: whole, or not written at all, or its entry and the stream's priority only. The stop that follows changes nothing |
| Whole reader | `tests/test_toy_wire_integration.c` with the title's table as the low resident holds it | Passed: 352,312,520 checks; every request completed with the image's exact bytes |
| Another title with the timer | `titles_other()`: Evolution 2's refills, the title's default in its table and in the resident's word | TMU2 taken by the title's own table. On a quiet bus four requests of five from one window, as without it. Stalls 1500/20: 2,391 waits and 2,367 answers of the timer, 4 requests of five from 1 window. Stalls 150/80: 14,557 waits and 14,229 answers of the timer, 4 requests of five from 1 window. Data exact After each run the title's table and both timers' registers are as the title had them, but for the timer of a stream still running |
| The same title using both spare timers | `titles_other()` with handlers of the title's in both entries | No timer taken or run, the entries untouched. Stalls 1500/20: 2,223 waits, none shorter than its time, 4 requests of five from 1 window. Stalls 150/80: 10,793 waits, none shorter than its time, 2 requests of five from 3 windows. As Test 80 |
| Toy Commander | Every scenario of Tests 68 to 80 in this build, the resident's word being Toy Commander's default | Passed, the timer taken as before |
| Test 80's scenarios | The list of `docs/toy-wire-stream-test-80.md`, Verification, in this build | Passed |
| The report | `tests/test_toy_wire_report.c` | Passed: `TEST 81`, `PT` with `1000` |
| All configurations | Both host targets in Tests 68 to 79 with their variants and in Tests 80 and 81, each with GAP, with LATER, without STAY, QUICK, MSB, RING, CLEAN, and with no part of Test 76 (`matrix.log`) | 147 of 148 jobs passed on `684f485`: both targets in 74 configurations. The one that did not is Test 81 without STAY, a variant that is not delivered: the whole reader's fixture did not compile there (a variable of Test 81's asked only by scenarios that a build without STAY does not have), and nothing of the reader was run. With the fixture corrected it passes both targets (`53d334d`, tests only) |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with Test 80's switches and `TOY_WIRE_TIMER=1` (`mutation-check.log`) | 349 of 350 caught. The one not caught, `the report names Test 79`, is Test 80's entry for the arm of the report's test number that lies below the timer's. With the timer's switch on that arm decides nothing: the report's test compiles to the same assembly with and without the mutation (in Test 80's configuration it does not, and the entry is caught there). No test could tell the two apart, and the list leaves the entry out from `941f2e5` on |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 350 of 350 for Test 81, 303 of 303 for Test 80, 258 of 258 for Test 79, 226 of 226 for Test 78 and 218 of 218 for Test 77 change the code that is compiled |
| As linked | The five linked checks of Test 77 on the module's auxiliary | Passed |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The scratch program of Test 79's verification on this module's bytes (`games-admission.log`) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, no session descriptor, the New and the Legacy match, the toggle, the mode record (0 to 7 for off, one byte changed), a blank cache seed slot, the handoff limit: all as the Games app asks |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Against the baseline | Test 80 built with this module's label | Residents and stage the same bytes. The worker (its title and test number) and the auxiliary differ |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `5ee3273b...2db4` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66 to 79 with their second modules and variants: all as recorded. Test 80's recipe gives the module built at Test 80's own commit, byte for byte |
| Other suites | The Test 66 family's own fixture in every suite mode, and the suites that share the wire model (`other-suites.log`) | The Test 66 family's fixture passes in every suite mode. 17 of the 18 other suites pass; `test-sci-stream-quarantine` stops on its fingerprint of a historical source as before (`docs/toy-wire-stream-test-77.md`, Known limits 7) |

Not checked by any test here, because only a console has it: where the
timers' entries of another title's SDK table lie and what they hold
while unused; that the title's dispatcher answers the timer's event as
it answers the SCI's. The models put both where Toy Commander has them.
The report's `TMR` says what the console found.

## Review

One reader beside the author, without the author's conclusions, on
16 November 2026, on the change as first committed (`494a6e9`). It
worked on scratch exports outside the tree: built the module, ran both
host targets and the change's own entries of the mutation list, read
the linked auxiliary, and ran scenarios of its own on the unchanged
stream source in the repository's register model and through the whole
reader. Its result is in `docs/evidence/toy-wire-stream-81/review.md`
as it returned it.

While it worked, the author's own reading of the hand-back met the
first of its points from the other side: a timer the title starts
under the stream's entry was given up with the stream's callback left
in the title's table. The correction (`9d4e338`) was begun before the
reader's result and finished with it. Its five findings, and what was
done with each:

| # | Finding | How established | What was done |
|---|---|---|---|
| 1 | A title that uses a spare timer as a plain counter, with no handler in the SDK's table, has it rewritten, armed and stopped by the stream. In four orders: (a) set up but stopped when a stream begins, it is taken and its control, reload and count are written and never put back; (b) started while a stream receives, it is stopped by the stream's end if that comes before the next wait; (c) started and stopped between two waits, it is armed by the next; (d) started anew under a wait that stands armed, the wait has no end by the clock, and the stop that ends it stops the title's timer | Run, each order, on the stream by itself; (a) and (b) through the whole reader as well, (b) by the game's own stop command | (a) and (b) corrected. The registers are kept at the stream's start and put back, and a timer that runs outside a wait of the stream's is not written: not at the stream's end, at its next wait, at a fault, or at an underflow of it. (c) and (d) are not changed: Known limits 2 and 3. What would see them, the registers compared with what the stream last left in them and a clock behind an armed wait, does not fit the auxiliary's remaining 32 bytes. Owed before the reader is anyone's default |
| 2 | Giving the timer up could leave the stream's own armed timer running, its interrupt on and nothing to answer it, where the entry had gone back to the SDK's default (a table set up anew). The live vector base is not asked again after the stream's start. And after a timer the title started was given up, the stream's callback and priority stayed in the title's table for the session | Run; what the SDK's default does with an event it does not know is the repository's own record of 24 October | The title's default in the entry is no handler: the timer is stopped and goes back as found, at the next look or the stream's end. The callback and the priority no longer stay behind. The vector base, and an underflow that comes before any look, are not changed: Known limit 4, which the SCI's own interrupt shares |
| 3 | A fault stopped the timer without a look at whose it was | Run | Corrected: a fault gives the timer back by the one rule |
| 4 | The tests did not hold the stream to the vector base the low resident hands it: with Toy Commander's constant handed over instead, both host targets passed, because the whole reader's other title lay at Toy Commander's base. The stream's test compared the timer's control by one bit and never its reload or count. The list had no entry for the three conditions of the choice or for the stop | Run: planted defects that passed | Corrected. The whole reader's other title lies at a vector base of its own, which the fixture's processor and the resident's word hold; the constant handed over is on the list and is caught. Both spare timers' three registers are set before every case and course and compared whole after it. The list has 47 entries for the change |
| 5 | The report cannot tell a timer a stream gave up: `TMR` is written at a stream's start | Read | Not changed; no row has room for a count. Known limit 8, and the Console test's table says how it would read |

What that reader examined and found sound, in short: the two words are
written by the stage only behind its admission of the title's table
and are asked for only while the resident's callback is armed; the
timers' entries lie at `+0x244` and `+0x248` by the dispatch formula
and the initializer the stage requires of every title it admits, and
the linked auxiliary carries those and the right register addresses;
an entry that does not hold the default means no timer and no write;
the callback is integer code that uses none of the caller's stack, and
the visit behind it holds every interrupt, refuses re-entry and runs
on the reader's own stack, of which the deepest path used 528 bytes
of 4,096 in the build it read; a timer visit cannot meet a disc call,
an SCI visit or
the worker inside the reader; the visit in another title reads nothing
of Toy Commander's; with nobody interfering the register writes are
Test 80's in Test 80's order, and for Toy Commander the two words are
the old constants by the console's own record; without the switch the
modules of Tests 78, 79 and 80 are the same bytes from both commits.

What it could not settle, all of it for the console or for an
executable that is not here: what the SDK's dispatch saves around a
callback beyond what the SCI's callback already relies on; whether
either Evolution game, or any title, touches TMU1 or TMU2, rewrites
the timers' start or priority registers during play, sets its table up
anew or moves its vector base; which timer will be free in another
title (Toy Commander's runs took TMU1; why TMU2 was not free there is
not recorded); a title's own write to those registers split by a visit
of the reader's; a game's exit or reset with a stream open.

The reader's scratch copies and scenario files are not in the tree.
What was taken from them is in the tests, under the commit named
above.

## Package

`KUI-Stream-Reader-Test-81-16NOV2026-1509-CST-684f485.zip`, 176,496
bytes, SHA-256
`fc99e6ec3b61a611bf7b48dde790a71e9b7dff6253386f9713b9b61b6a4f98d9`,
prepared 2026-11-16 15:09 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | `684f485644e5` | `B40B9423` | `5ee3273b...2db4` | Changed for the test only: Test 81 in the place of the installed New reader module (build f8807cb16561), which goes back from the owner's 06 NOV package afterwards |

Module SHA-256 in full:
`5ee3273b75dceb2a02ac73cbe5ccdcf972ded0164f15a822a192949048072db4`.

Retained on the card: everything else. The installed New reader module
(build `f8807cb16561`, 290,104 bytes, SHA-256 `09c1480c...481a` in the
candidate's record) goes back from the candidate's own archive when the
tests are done; it is not in this one. No KUP, bank or user data is
included.

The auxiliary's text is 20,436 bytes and its image 24,576 of 24,576;
the worker's text 39,700.

The report names the module by its title (`TEST 81`), its build
(`684f485644e5`) and `PIPE`'s last value (`1FBD`).

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 BUILD_ID=684f485644e5 \
  build/toy-suite-81/retail-toy-test-81.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 toy-wire-host-tests toy-wire-integration-host-test \
  toy-wire-package-test toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 \
  python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: the steps of Console test, with the archive's
README, after Test 80's run. Cold CD boot, not Restart K-UI. Confirm
`TEST 81` and `BUILD 684F485644E5` on the report's first page, and
`1FBD` as the last value of its `PIPE` row.
