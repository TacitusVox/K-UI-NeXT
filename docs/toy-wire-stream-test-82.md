# Toy Commander Test 82: the virtual VMU beside the stream

Status: built 16 November 2026 (build `af5ebae4f0a5`) and run by the
owner on the console the same day (Console result): "everything seems
fine", with no detail and no launch record. A test module, not an
accepted component, and not a replacement for the installed New
reader: it has no block prefetch, and no report pages. Delivered behind
Tests 80 and 81 (`docs/toy-wire-stream-test-81.md`), which the owner
ran before it.

The module is built from `af5ebae4f0a5`: Test 81's source and the change
(`b604dea`), the corrections its two reviews asked for (`af5ebae`), and
between them two commits of tests alone (`941f2e5`, `53d334d`) and Tests
80 and 81's documents (`f7574f8`).

It is the third of the owner's three changes of 16 November: "Universal
VMU" beside "Universal reader" and "prefetch where possible". Until now
the stream reader had run with the virtual VMU off only, and a Toy test
module stopped a launch that had it on.

## Baseline

For the reader: Test 81, source `684f485644e5`, module
`5ee3273b...2db4`. Test 82's low residents and its auxiliary are Test
81's.

For the virtual VMU: the installed New reader of the delivered
candidate, build `f8807cb16561`, module `09c1480c...481a`, with the
virtual VMU on, as the owner runs it (Evolution 2's launch record of
that build: Background New, virtual VMU On, the module at
`KUI/apps/games/retail-native-dual.kui`, payload CRC32 `9B37B199`).
Test 82's worker is that module's worker.

## Why a composition and not a switch

The Toy tests' worker shows the test's report and has no virtual VMU:
the VMU's code was taken out of it to make room for the report's pages.
With the VMU put back it is 4,832 bytes too large for its 72 KiB. The
installed worker has the virtual VMU and no report, with 384 bytes to
spare. So the virtual VMU beside the stream is the installed worker in
the Toy worker's place.

That works because the worker reads nothing of the reader's memory. It
asks the low resident for the card through one entry, and the low
resident answers for the reader and the auxiliary. The Toy tests'
worker used the same entry to write its report.

## The change

Behind `TOY_WIRE_VMU=1` on Test 81's recipe. A module is four parts;
one of them is new.

| Part | Test 81 | Test 82 |
|---|---|---|
| Low residents (the reader in the game's low memory) | The Toy tests' since Test 66 | The same bytes |
| Auxiliary (the stream, the window) | Test 81's | The same bytes |
| Worker (the controller port's side) | The Toy worker: the report, no virtual VMU | The installed worker: the virtual VMU, no report. No Toy switch reaches its compilation. Built with the installed module's label it is that module's worker byte for byte |
| Cold stage (runs once at launch, puts the parts in place) | Stops a launch that has the virtual VMU on. Prepares the report's place on the card | Its own. It hands the worker the installed configuration, prepares no report, does not stop for the virtual VMU, makes another title a source for the window with the virtual VMU on as with it off, makes Toy Commander none, and names the test on the launch screen |

Compared as each recipe compiles it, the stage differs from Test 81's
in those things and in nothing else. From the installed stage it
differs by the Toy tests' sizes and addresses (the larger auxiliary and
low residents, the stream's memory where the block cache was) and by
the Toy tests' additions, and by nothing on the virtual card's own
path: its files, its configuration and its entries into the low
resident are set up by the installed stage's lines.

Toy Commander is no source for the window in this module, with the
virtual VMU on or off, and runs on the plain reader with no window and
no prefetch. Its movies are fed from hooks in its player, and that
path leans on the Toy tests' worker in two places the installed worker
does not serve (Known limit 3). The first commit of the change kept
Toy Commander a source with the virtual VMU off; its review showed
that was not Test 81's reader.

## What the virtual VMU asks of the reader

Nothing of the virtual card is held in memory but a checksum a block.
For every block the game reads from it and every block it writes, the
worker borrows the SD card from the reader: one loan, one card
transaction, inside the game's controller-port completion with
interrupts held off.

| When the loan is asked for | What happens |
|---|---|
| A command of the game's is pending or being executed, or a read is in flight | Refused. The game is told to ask again, as with the installed reader |
| A window is open, its stream receiving, waiting or standing | The stream is stopped at once and its timer and table entry given back; the card's open read is ended; the window is given up, and counted in no column: it was no reason of the game's. The card changes hands |
| Nothing of the reader's is open | The card changes hands |
| The loan ends | The reader stays closed until the game's next read. A read that follows on from the last one opens a window again and is served from it, with a new card command |

So a game that reads its card while it reads its disc pays a stopped
stream and a new card command for every block of the card. Toy
Commander was seen to read 15 to 25 blocks once, when it finds the
card, and to write nothing outside a save. What the two Evolution
games read during play is not recorded.

## Started from the Games menu

As Test 79 (`docs/toy-wire-stream-test-79.md`, Started from the Games
menu): cold boot from the CD, Games, the game, New (X). The Games app
loads the file at `KUI/apps/games/retail-native-dual.kui` and asks of
it what it asks of the installed one. With the virtual VMU on it also
asks for the game's card profile slot, which this module has where the
installed one has it.

| Setting | What runs |
|---|---|
| Virtual VMU on, the virtual card in slot A2 | This module, the virtual VMU beside the stream |
| Virtual VMU on, the virtual card in slot A1 | `retail-native-dual-a1.kui`, the installed module, with no message. This test has no A1 module |
| Virtual VMU off | This module with the physical cards passed through: for another title Test 81's reader, without Test 81's report |
| Toy Commander, either setting | This module's plain reader: no window, nothing read ahead for its movies |
| Legacy (Y), CD Music on, Restart K-UI with a saved bank | Something else, as in Test 79's table |

There are no report pages. What says which module ran:

- The stage's lines while the game loads: `TEST 82 / STREAM READER /
  INSTALLED VMU`, and `SCI STREAM READ AHEAD READY` where the disc is a
  source for the window. The installed module prints `BOUNDED READ
  AHEAD READY / 192 KIB CACHE`.
- `KUI/apps/games/launch-timing.txt` after the launch:
  `source_module_build: af5ebae4f0a5`, `metric.module_crc32: 0x4962DB2D`,
  `virtual_vmu: On`.

## Console test

Evolution 2, then Evolution, each with the virtual VMU on, New (X),
cold boot from the CD, this module in the installed module's place.
Run Tests 80 and 81 first: this build has their two changes in it, and
no report of its own to tell them apart.

In each game: start it and watch for the memory card being found; load
a save that was made under the installed reader; play where the lag
was; watch a movie; save; power off; boot again and load that save.

Asked of the owner, for each game: did it start; was the card found
and the old save loaded; play and movies as under Test 81, or how they
differed; did the save work, and was it there after power off; and
anything that looked or sounded wrong, with the point at which it
happened. And `launch-timing.txt` from the card after one launch.

| Outcome | What it says |
|---|---|
| Starts, card found, old save loads, play as Test 81, saves and reloads | The virtual VMU and the stream live side by side in these two games. It does not show that a window was open when a loan came: nothing counts that on a screen. Next: a way to read the reader's counts without the Toy worker, Toy Commander's movies with the VMU on, then the reader as the family's default |
| A stop screen at launch | The stage's VMU path in this composition. The screen's text and number say where |
| A page headed `VMU / A2` with `SCI JOURNAL`, `DETAIL 100` and `PHOTO / POWER OFF` during play | The installed worker's last page. On this module it is shown for a stop of the reader as well as for the game leaving through one of its exits, and it does not say which: no function, command or sector is on it. With what the game was doing when it came |
| The game does not find the card, or hangs where it looks for it | The worker's loans against this reader, at the game's first card traffic. The reader had its card lent under a host model only |
| The card is found, the old save does not load or is damaged | Stop there. Put the installed module back and say so: nothing else should be saved with this build |
| Play stutters more than under Test 81 | The game reads its card during play, and every block costs the window. How often is what a counter would say; this build has no page to show one |
| A save hangs or the game freezes during it | Dozens of loans one behind the other with the reader's stream between them. With the point at which it happened |

Before the test, a copy of the card's `KUI/vmu-test` and `KUI/vmu`
folders is worth the minute it takes: this is the first build that
reads and writes the game cards' files through this reader.

## Console result

The owner, 16 November 2026, with the photographs of Tests 80 and 81:
"As for test 82, everything seems fine." And of the stutter seen under
Test 79: "I don't know if that stutter is real or imagined. Not worth
hunting down at this very moment."

It was a short run: the message came within a quarter of an hour of
the archive. Nothing else was reported: not which of the two games
were run, not whether an old save loaded or a new one was there after
power off, and no `launch-timing.txt`.

By the outcomes above that is the first one as far as the owner's word
reaches: the module starts and plays with the virtual VMU on, with
nothing seen to be wrong. It is the first console run of the installed
worker's loans against the stream reader. What it does not establish:

- that a save made under this module survives power off and loads
  again, in either game (asked of the owner again);
- that a window was open when a loan came, or how often loans came:
  this module has no page to show it (Known limit 1);
- the identity of what ran, beyond the owner's having installed the
  archive: no launch record was sent.

## Known limits, not changed here

Tests 80 and 81's limits stand for the reader. Added by this change:

1. No report. The auxiliary counts as in Test 81, and nothing shows the
   counts: the installed worker has no page for them and no room for
   one, and it cannot read the stream's auxiliary at all (limit 6). A
   run of this build is read by what the owner sees and by the launch
   record. A run in which everything works does not show whether
   windows were open when the loans came, or how often a loan was
   refused.
2. A stop of the reader has no page of its own. The Toy tests' low
   residents hand a stop to the worker, and this worker shows its own
   last page for it: `VMU / A2`, `SCI JOURNAL`, a phase, `DETAIL 100`,
   the virtual card's counts and `PHOTO / POWER OFF`. The same page
   with the same detail is shown when a game leaves through one of its
   exits. The installed module's own stop page, with the function, the
   command and the sector, is in its low residents, which this module
   does not have.
3. Toy Commander has no window and no prefetch in this module, with
   the virtual VMU on or off: its movies will not play as they do under
   Test 81. Two things stand between it and the installed worker. That
   worker does not wake a movie read at the controller's completions,
   as the Toy tests' worker does, and a read the game only asks the
   status of would never be moved. And a loan sets a flag in the
   auxiliary that nothing in this build clears, which the path from
   Toy Commander's hooks asks. No other title has such hooks, and no
   other path asks either. Both need a pump in the low residents or
   the auxiliary that does not depend on the worker; the auxiliary has
   32 bytes left.
4. A game that reads or writes its card often during play loses its
   window as often. What was read ahead is thrown away each time, and
   the next read begins a new stream.
5. The card's loan with a window open has run in the host model only,
   and the low resident's gateway, which takes the card's port for the
   worker around its transaction, is not host code and is in no host
   test. The Toy tests' report went through that gateway on the
   console, at a run's end with the reader at rest. How long a loan
   holds interrupts off with a stream to stop first has never been
   measured on a console: the stream's stop, the end of the card's
   read and the card's own busy time come before the worker's
   transaction.
6. The installed worker was compiled for the installed auxiliary. It
   asks an auxiliary's size before it calls one and refuses an image
   larger than 0x3000 bytes; the stream's is 0x6000. That refusal alone
   keeps the worker's ticks at the controller's completions, and the
   reset its capture gesture would send, away from the stream's
   auxiliary. The parts check holds the size; an auxiliary that passed
   the worker's check would need those two events refused on its own
   side.
7. Where the stream's stop fails for good with a window open, every
   later loan is refused, the virtual card answers "again" without
   end, and the fault shows only when the next disc read fails. The
   installed reader reports it at the second loan. Fatal either way.
8. One slot. The module is the A2 one. A game set to the virtual card
   in A1 runs the installed A1 module.
9. With the virtual VMU off this build is, for another title, Test 81's
   reader with nothing to read it by. Test 81 is the build for that.

## Verification

The module is built from commit `af5ebae4f0a5` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree.
Logs are in `docs/evidence/toy-wire-stream-82/`. None of this is a
console result.

| Check | What ran | Result |
|---|---|---|
| The parts' bytes | `toy-wire-vmu-parts-check` and the same comparison in the final run: this recipe, the same without the switch, and the installed recipe, each built with this module's label | The worker (49,920 bytes) is the installed recipe's; the low residents (15,848 and 15,120) and the auxiliary (24,576) are Test 81's recipe's. The auxiliary is the delivered Test 81 module's, which does not carry the label |
| The worker against the delivered candidate | This recipe built with the candidate's label `f8807cb16561`; the candidate's module rebuilt at its own source (public commit `e79da8ce`, the tree its record names) | The candidate's module is reproduced (`09c1480c...481a`), and this recipe's worker is that module's worker, byte for byte (`22799207...a693`) |
| The installed recipe on this source | The installed recipe with the candidate's label at this source and at four commits behind it, and the sources of its low residents and auxiliary as compiled, compared from commit to commit (`installed-recipe-on-this-source.log`) | Its worker is the candidate's at every commit. Its low residents and its block-cache auxiliary are not: the same sizes, 390, 6 and 3,074 bytes differ, since the two imports of the Toy tests' sources that came before Test 68 (`f216fac`, `e1104f4`). No line that recipe compiles for them has changed in Tests 68 to 82. Test 82 takes the worker alone; the rest is a finding for the plan's owed list, not for this module |
| The stage as compiled | The stage's main source preprocessed as each recipe compiles it, and compared (`stage-as-compiled.log`) | From Test 81's: 44 lines of Test 81's are gone and 4 are new, in 8 places: the two rules of the scan, the report's wire with its seed and its preparation, the stop for the virtual VMU, the report's place in the worker's configuration and that configuration's size, the call that prepares the report, and the line that names the test. From the installed stage: 34 lines of the installed stage's are replaced and 342 are new, in 37 places, all of them the Toy tests' sizes, addresses and additions; nothing on the virtual card's path |
| The stage's choice of a source | `tests/test_toy_titles_stage.c` on the stage's own text, with the switch | Another title on the New reader is a source by its raw Mode 1 track on the SCI card with the virtual VMU off or on, and by nothing else; Toy Commander never, and its files are not looked for |
| The card lent with a window open | `titles_lent()` in the whole reader's test: another title reading as Evolution 2 does, the card asked for behind every read, on a quiet bus and two that cut runs short | On each bus six loans granted and one refused, the one asked for while the subcode's command was pending. Every granted loan found the stream stopped, the card's command ended and the title's table and timers its own, and counted nothing as given up; inside each the borrower read a block of the card through the card's own port, exact; four requests of five were then served from four windows, a new one behind each loan. Stalls 1500/20: 46 streams begun. Stalls 150/80: 348 streams begun. Data exact |
| The same on other buses and discs | `sequence_lent()`, with the stream receiving and standing | Passed, as in Test 81 |
| Whole reader | `tests/test_toy_wire_integration.c` in this configuration | Passed: 359,191,981 checks; every request completed with the image's exact bytes. It is the same test as in Test 81's configuration on this source: the preprocessed test and production sources are the same but for the one define this switch adds (16,173 preprocessed lines in each) |
| Test 81's scenarios | The lists of Tests 80 and 81's documents, in this build | Passed. Toy Commander's are the reader's of Test 81; this module's stage does not make Toy Commander a source, and none of them is reached by it |
| The module for the Games menu | `tests/test_toy_reader_package.c` with `no-report` on the packaged bytes | The envelope, the layout, the dual descriptor, the toggle and the mode record as for every module; no report seed; the mapping's seed slot; one card profile slot; no stop for the virtual VMU |
| The Games app's byte checks | The scratch program of Test 79's verification on this module's bytes (`games-admission.log`; the program is not in the tree) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, no session descriptor, the New and the Legacy match, the toggle, a blank cache seed slot, the game card's profile slot with its template (asked for with the virtual VMU on), the mode record for off (0 to 7, one byte changed) and for on (left 0, nothing changed), the handoff limit: all as the Games app asks |
| As linked | The linked checks of Test 77 on the module's auxiliary | Passed: it is Test 81's auxiliary |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with Test 81's switches and `TOY_WIRE_VMU=1`: the entries of the stage's scan and of this change (`mutation-check.log`) | 14 of 14 caught, with the tests without a mutation passing. The list's other entries are Test 81's, on the low residents, the auxiliary and the tests that are the same compiled code in both configurations; they were run on Test 81's commit and are not run again here |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 351 of 351 for Test 82 and 349 of 349 for Test 81 change the code that is compiled |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `9f8faeac...3a42` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66 to 79 with their second modules and variants, and Tests 80 and 81: all 20 as recorded |

Not checked by any test here, because only a console has it: the
installed worker's loans during play against this reader, through the
low resident's gateway; the stage's virtual card path in this
composition on real hardware; what the games read and write on their
cards while they play.

## Review

Two readers beside the author, each without the other's conclusions or
the author's, on 16 November 2026, on the change as first committed
(`b604dea`). One read the composition and the cold stage, the other the
run time: the installed worker against the Toy tests' low residents
and the stream's auxiliary. Both worked on scratch exports outside the
tree, built the three recipes with one label and compared their parts,
compiled probes of the shared structures under each part's own flags,
read the linked parts, and ran the host targets. The second also ran
scenarios of its own in a copy of the whole reader's fixture. Their
results are in `docs/evidence/toy-wire-stream-82/` as they returned
them (`review-stage.md`, `review-runtime.md`).

Neither found a fault on the path the test is for, another title with
the virtual VMU on. What they found, and what was done with each
(`af5ebae` is the commit of the corrections):

| # | Finding | How established | What was done |
|---|---|---|---|
| 1 | Both: Toy Commander with the virtual VMU off was not Test 81 on this module, though the change called it unchanged. Its movies' path leans on the Toy tests' worker, which wakes a movie read at every controller completion; the installed worker has no such call. A read the game only asks the status of would never be moved | Run: the worker's wake is in Test 81's worker and in neither Test 82's nor the installed one; with the fixture's own wake call taken out, a pending movie read is still pending after 19,990 rounds | Corrected: Toy Commander is no source in this module with the virtual VMU on or off, and its files are not looked for. It runs on the plain reader. Known limit 3 |
| 2 | Both: a run leaves no record of what the reader and the loans did, and a reader stop is shown on the installed worker's page (`VMU / A2`, `SCI JOURNAL`, `DETAIL 100`), which a game's exit through a hooked exit also shows. Nothing on a screen named the test | Read; the parts' texts compared | The launch now names the test. The rest is not changed: Known limits 1 and 2, and the Console test says what each outcome can and cannot show |
| 3 | `TOY_WIRE_VMU=1` without the stream's switch built a module under another test's name | Run (`make -n`) | Corrected: refused, as every other switch of the stream is |
| 4 | No test held the composition. Three planted defects passed every target but one: the stage keeping its stop for the virtual VMU; a Toy flag let through to the worker; the auxiliary's flags lost, which builds the installed block cache into the module | Run, each | Corrected: `toy-wire-vmu-parts-check` compares the parts' bytes with the two recipes they come from, and the package test refuses a module with the stop in it |
| 5 | The loan scenario was the loan's begin and end alone: the borrower's own card traffic, and the low resident's gateway that takes the port for the worker, were in no Toy host test | Read | The borrower now reads a block of the card through the card's own port inside every loan. The gateway is not host code: Known limit 5. The Toy tests' report went through it on the console, with the reader at rest |
| 6 | Both: the installed worker was compiled for the installed auxiliary, an image of 0x3000 bytes at most. It refuses the stream's larger image, and that refusal alone keeps its controller ticks and its capture gesture away from the stream's auxiliary, which would take the second for a reset | Compiled probes; the linked worker read | No change to the parts. The parts check asks that the image stays larger than the bound, and the dependency is named here and in Known limit 6 |
| 7 | The package test did not ask what a launch with the virtual VMU on needs | Run with a program of its own: the module has all of it | Corrected: the seed slot, one card profile slot by the Games app's own search, the mode record, no stop |
| 8 | Where the stream's stop fails for good with a window open, every later loan is answered "again" and the fault is not reported until the next disc read fails; the installed reader reports it at the second loan | Run in the fixture | Not changed: it is fatal either way and only the symptom differs. Known limit 7. It needs the low resident or the auxiliary changed, which this test leaves as Test 81's |
| 9 | A refused loan is not always without effect: refused by the stream's own stop failing once, the stream is already halted, and the next ask is granted | Run in the fixture: recovers by itself | The document says "refused and changes nothing" of a pending command only |
| 10 | Comments in the recipe: a document not yet in the tree, a wrong statement about the residents' flags; the plan's row said this step clears the auxiliary's flag, which it does not | Read | Corrected, the plan with this document |

What the readers examined and found sound, in short. The worker's 32
compile and link commands are the installed recipe's word for word,
and with the candidate's label `f8807cb16561` the worker is the
delivered candidate's, by the hash in that candidate's own record. The
low residents and the auxiliary are Test 81's with one label. The
stage differs from Test 81's in the five things the change named as
first committed (the corrections added Toy Commander's rule and the
line that names the test) and from the installed stage in the Toy
tests' geometry and additions, and in nothing on the virtual card's
path. The worker's configuration has
the same offsets and size under the stage's flags and the worker's (27
probes), and the worker refuses any other size. The structures the
worker shares with the reader have the same offsets as compiled for
each part (72 and more probes). The low entries handed to the worker
lie inside the range the worker accepts. The loan's code is the same
source in both variants of the low resident. Nothing in the loan's
path waits for an interrupt or a timer, and nothing of the reader runs
inside a loan. In 800 randomised runs of the fixture, 330,127 loans
were granted with the stream running, waiting with and without its
timer, and standing full: each found no window, the stream off, the
card out of its read, the channel off, the timers stopped and as the
title left them and the title's table kept; a card transaction of the
borrower's then succeeded, later reads were exact, and windows opened
again. The auxiliary's flag is read by Toy Commander's hook path
alone. Without the switch every earlier module builds to the same
bytes.

What they could not settle, all of it for the console or for code that
is not host code: the low resident's gateway with real card traffic
during play; how long a loan holds interrupts off with a stream to
stop first; what the two Evolution games read and write on their cards
during play; whether Toy Commander ever waits on a movie read without
polling.

The readers' scratch copies and scenario files are not in the tree.
What was taken from them is in the tests and the recipe, under the
commit named above.

## Package

`KUI-Stream-Reader-Test-82-16NOV2026-1542-CST-af5ebae.zip`, 184,793
bytes, SHA-256
`38295229b277e8a0f848ba66c81cccaecff5bfdf279f40934494c3eb85872fd2`,
prepared 2026-11-16 15:42 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | `af5ebae4f0a5` | `4962DB2D` | `9f8faeac...3a42` | Changed for the test only: Test 82 in the place of the installed New reader module (build f8807cb16561), which goes back from the owner's 06 NOV package afterwards |

Module SHA-256 in full:
`9f8faeaca27521708130a1dae8fe3de7df67e465d2f2124a08f3396c23083a42`.

Retained on the card: everything else. The installed New reader module
(build `f8807cb16561`, 290,104 bytes, SHA-256 `09c1480c...481a` in the
candidate's record) goes back from the candidate's own archive when the
test is done; it is not in this one. No KUP, bank or user data is
included.

The worker's text is 49,892 bytes; with its data, its zeroed memory and
its stack it fills its 73,728 but for 384 bytes, as in the installed
module. The auxiliary's text is 20,544 and its image 24,576 of 24,576,
as Test 81's. The stage's text is 301,660.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_VMU=1 BUILD_ID=af5ebae4f0a5 \
  build/toy-suite-82/retail-toy-test-82.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_VMU=1 BUILD_ID=af5ebae4f0a5 toy-wire-host-tests \
  toy-wire-integration-host-test toy-wire-package-test toy-wire-vmu-parts-check \
  toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
```

Run on the console: the steps of Console test, with the archive's
README. Cold CD boot, not Restart K-UI. Confirm the stage's lines `TEST
82 / STREAM READER / INSTALLED VMU` and `SCI STREAM READ AHEAD READY`,
and `KUI/apps/games/launch-timing.txt` after the launch:
`source_module_build: af5ebae4f0a5`, `metric.module_crc32:
0x4962DB2D`.
