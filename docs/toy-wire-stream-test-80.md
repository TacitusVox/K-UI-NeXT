# Toy Commander Test 80: a window stands through a command that only asks

Status: built 16 November 2026 (build `e58dd2e6b641`), host and linked
checks only. It has not run on a console. A test module, not an accepted
component, and not a replacement for the installed New reader: it has no
virtual VMU and no block prefetch. Delivered together with Test 81
(`docs/toy-wire-stream-test-81.md`), which is this build with one change
more.

The module is built from `e58dd2e6b641`: the change (`00d13e1`), its
report (`e58dd2e`).

It follows from Test 79's run on the console
(`docs/toy-wire-stream-test-79.md`, Console result), and is the first
of the two things that run showed the window to lack in another title.
The third of the owner's three changes of 16 November, the virtual VMU
beside the stream, comes behind them.

## Baseline

Test 79 as the owner ran it on 16 November 2026: source `9f183a7f72f5`,
module `557822ec...47be`, in Evolution and in Evolution 2 from the Games
menu, with the virtual VMU off. Its reports as read from the owner's
photographs are `docs/evidence/toy-wire-stream-79/console-report-
evolution.txt` and `console-report-evolution-2.txt`.

For the owner's question, whether play is smoother than under the
installed prefetch, the comparison is still the installed New reader,
`KUI/apps/games/retail-native-dual.kui` of build `f8807cb16561`, with
the virtual VMU off, in the same games on the same day. No such run has
been reported yet. That file is the delivered native candidate's
(`docs/accepted-progress.md`, Current console checkpoint): this test
does not change the candidate or its archive.

## Why

| Evolution 2 under Test 79 | |
|---|---|
| Windows opened | 972 |
| Given up for a command that is not a read | 969 |
| Requests served to a window | 1.28 |
| Sectors served to a window | 12.6 (Evolution: 445) |
| Sectors read ahead and never served | 3,104 of 15,342, a fifth |
| Streams begun | 1,066 |
| Stops of the stream because its window was full | 1 (Evolution: 2,225) |

A window was given up for every command of the game's that is not a
read: its stream stopped, the card's command ended, what it held thrown
away. Evolution 2 sends such a command between its reads. The one it
is known to send is the request for the subcode, GETSCD
(`docs/evolution2-getscd-test.md`), which the service answers from what
it holds. So the game's reads were each served from a window opened
anew for the request, at the card's pace, and nothing was ever ready
ahead of one. Test 79's document named this before the run (its limit
5); the run showed it in full.

## The change

Behind `TOY_WIRE_QUERY=1` on Test 79's recipe.

| The game's command, with a window open | Test 79 | Test 80 |
|---|---|---|
| One that only asks the drive something, or does nothing: GETTOC (18), GETTOC2 (19), NOP (29), REQ_MODE (30), GETSCD (34), REQ_STAT (36), GET_VERS (40) | The window is given up | The window stands, and its stream runs on |
| One that moves or stops the drive, sets what a sector is, or is the audio's: SEEK (27), STOP (33), SET_MODE (31), INIT (24), PLAY, PLAY2, PAUSE, RELEASE (20 to 23) | The window is given up | The same |
| Any other number | The service refuses it at the request. It never becomes the service's command, and no window sees it | The same. (Should one ever stand in the command's place, the window is given up for it) |
| The functions INIT and RESET, and ABORT of a waiting read | End the window with the request state | The same |

Why those seven. Each is answered by the service's own `execute()`
(`src/core/retail_gd.c`) from what the service holds: the table of
contents from the track map, the subcode and the status from the
drive's position, the mode's words, the version's text. None of them
reads the card, and none moves the drive's position, which is what says
whether the next read follows on from the last. A window through one of
them is in the state it was in before it.

Why not the others. SEEK moves the position and STOP stops the drive:
a game that sends either has said where it reads next, or that it does
not. SET_MODE and INIT set what a sector is. The audio's commands do
nothing in this module, which plays no CD music; a module that does
will need the card for them, and they are kept out of the rule now
rather than taken back then.

What a window does while such a command is the service's current one:
nothing for the command. No request is the window's then (the low
reader begins nothing but reads), so nothing is handed over. The visit
pumps the stream under the rule for the game's idle calls, as it does
with no command at all. The read that follows finds the window where it
stood.

What is not changed: which read opens a window, which requests a window
serves and how, the sector checks, the card stream (which still has no
timer in another title: Test 81), the low residents, the Games app, and
of the worker everything but its report.

## The report

Test 79's rows, with these differences:

| Row | Change |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | `PT` is `FBD`: Test 79 (`7BD`) and QUERY (`800`) |
| `XC KEPT LO HI` | New, behind `SD`. `KEPT`: the commands that are no reads which a window met and was not given up for, each counted once, when it is first met. A window can still go while such a command is current, for another reason: the command fails, is aborted, or INIT or RESET ends everything. `KEPT` has counted it all the same. `LO` and `HI`: every command that is no read which a window met, kept or given up for, a bit to a command number: bit n of `LO` for command n up to 31, bit n-32 of `HI` from 32 on |
| `VMAX SCI TMR GAME D` | New, behind `V`. The longest single pump of the reader from the SCI's interrupt, from the stream's timer and from one of the game's calls; and the longest whole delivery visit from one of the game's disc calls, which has the low reader's work in it too. In ticks of 1.28 microseconds: a frame of the display is `32DD` |
| `I SUBMIT ...`, `I FIRST ...`, `B SUBMIT ...`, `IP ...`, `BP ...` | Left out when `TOY` is 0. They are Toy Commander's movie player and were zeros in another game. The report of another game is sixteen rows: three pages on the screen, where Test 79's was four |

The commands by their bits:

| Command | Row value | | Command | Row value |
|---|---|---|---|---|
| GETTOC (18) | `LO` 40000 | | GETSCD (34) | `HI` 4 |
| GETTOC2 (19) | `LO` 80000 | | REQ_STAT (36) | `HI` 10 |
| NOP (29) | `LO` 20000000 | | GET_VERS (40) | `HI` 100 |
| REQ_MODE (30) | `LO` 40000000 | | STOP (33) | `HI` 2 |
| SEEK (27) | `LO` 8000000 | | SET_MODE (31) | `LO` 80000000 |
| INIT (24) | `LO` 1000000 | | PLAY to RELEASE (20 to 23) | `LO` 100000 to 800000 |

A game that asks for the subcode and nothing else shows `LO` 0, `HI` 4.
Values add: `HI` 14 is GETSCD and REQ_STAT.

The report is asked for and shown as in Test 79: both triggers and
A+B+X+Y held for about three seconds with the controller in port A;
`REPORT NOT SAVED / FILM EVERY PAGE`; a page for about five seconds,
then the next, round and round; Start reboots to the K-UI CD.

## Console test

One game: Evolution 2, where the change does its work. With this module
in the installed module's place, the New reader (X), the virtual VMU
off, cold boot from the CD: the opening movie, then to where the lag
was before the prefetch, a few minutes of play there, as in Test 79's
run. Then the gesture, the three pages photographed, and Start.

Evolution needs no run of this build. No command met a window there in
Test 79's run (`C` 0), so this change has nothing to do in it. It is
run with Test 81.

Asked of the owner: whether the stutter seen under Test 79 while the
card was read is still there, less, or gone; whether anything else
changed; the three photographs.

| Outcome | What it says |
|---|---|
| `C` zero or small; `KEPT` large; `HI` 4; windows opened (`OW`) far fewer than Test 79's 972 for like play; requests to a window (`HIT` over `OW`) far above 1.3; `W` `FULL` no longer 1 | The window stands through the game's command and reads ahead in Evolution 2 as it does in Evolution. Next: Test 81's timer, then the virtual VMU |
| `C` still large, and `LO` or `HI` has a bit of the second table's kind | The game sends another kind of command between its reads. The row names it. Whether a window may stand through that one is the next thing to read |
| `C` small but `O` large | The game reads in two places by turns: one window is not enough for it |
| `C` small, windows live, and the stutter as before | The stutter is not the windows given up. `VMAX` says whether one visit of the reader holds the processor for a frame; `WAIT` and `WTICK` are Test 81's |
| `VMAX` `D` or `GAME` near or above `32DD` | A single call of the game's was held for about a frame by the reader: a large request handed over in one visit, or the low reader's work in the call. That is a stutter's size, and the next thing to bound |
| A stop screen, a hang, wrong picture or sound | The change does not hold for this game. The screen's lines, or the report's rows, say where |

## Known limits, not changed here

Test 79's limits stand. Added by this change, or made to matter by it:

1. A window that stands through the game's commands stands longer. In a
   game that asks the drive something every frame and has stopped
   reading, the stream fills its window and then stands with the card
   inside its command until a read elsewhere, a command of the other
   kind, the functions INIT and RESET or the report's gesture ends it.
   That is Test 79's limit 6, reached more often. The rule that ends a
   standing stream after a time is still owed before the reader is
   anyone's default.
2. The stream has no timer in another title in this build. Its waits
   last until the game's next disc call, as in Test 79. Test 81.
3. The seven commands are those the service of this module answers
   without the card. A module that plays CD music from the card has the
   same seven; a service that came to read the card for one of them
   would have to take it out of the rule.
4. `VMAX`'s first three values are the reader's own pumps. Its fourth
   is a whole delivery visit, which for a read no window serves has the
   low reader's blocks in it: a long one is as likely the low reader's
   catching up as the window's. None of them is the time the game
   waited for a request. `D`'s count, sum and longest are of the visits
   that take the whole path. A call with a window open and nothing to
   do takes the short one and is in none of them. In Test 79's run of
   Evolution 2 no window stood, and every idle call took the whole
   path: its `D` figures are not to be set against this build's.
5. `KEPT` counts a command when a window first meets it. One of the
   seven that fails (a table of contents asked for an area the disc
   has not), or is aborted, or is ended by INIT or RESET, is counted,
   and the window then goes as `I` or in no column. In the pattern the
   test is for, a subcode asked and acknowledged, the count is exact.
6. The report asked for while one of the seven is pending: the card's
   loan is refused, as it was, but the stream now runs on, and Start
   cannot restart the console (`RESTART BUSY`). Test 79 had ended the
   stream by then. It needs a game that has such a command pending at
   each of the sixty looks the gesture takes; the pages are shown all
   the same, and the console is then switched off. Test 79's limit 12,
   wider.
7. Toy Commander's report has three bytes of room at the test's sample
   of a full run. A real run is wider in some rows and narrower in
   others (Test 78's would take about 972 of the 1,012). A much longer
   session loses the row the code names for it first, `BP`. A further
   row for Toy Commander needs room made first.

## Verification

The module is built from commit `e58dd2e6b641` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree or,
where it says so, on the source of Test 81, which is this source with
one switch more. Logs are in `docs/evidence/toy-wire-stream-80/`. None
of this is a console result.

| Check | What ran | Result |
|---|---|---|
| Whole reader | `tests/test_toy_wire_integration.c`: the real resident, low reader, GD service, auxiliary and stream on the card model, the scenario list twice | Passed: 318,027,918 checks; every request completed with the image's exact bytes. With the two scenarios its review asked for, on the source of Test 81 with this test's switches: 346,526,504 |
| A command met by interrupt visits | `sequence_interrupted()`, added after the review (`684f485`) and run on the source of Test 81 with this test's switches, where the module builds to this module's bytes: a run in flight, the subcode or the drive's status asked for, three interrupt visits while it is pending and three more while it is completed and not acknowledged; and the same with the command that stops the drive | Window and stream stand, no card command is sent, the command is counted once, and the two reads behind it are the window's from the same stream. The stop ends the stream in the interrupt visit that meets it; the window goes with the game's next call, counted once |
| Every command that is no read | `sequence_every_command()`, likewise: each of the fifteen the service accepts, with a window open, and the numbers 63 and 64 in a command's place | Seven leave the window and are counted, eight give it up, each named by its bit; 63 and 64 give it up |
| Another title that asks behind every read | `titles_asked(true)`: Evolution 2's refills of 73 and 50 sectors, five requests, the subcode asked for behind each through the service's own REQUEST, EXEC and CHECK | On a quiet bus: 4 commands a window stood through, none given up for one, 4 requests of five from 1 window, CMD18 1, CMD12 0. Stalls 1500/20: 4 stood through, none given up for one, 4 of five from 1 window. Stalls 150/80: 2 stood through, none given up for one, 2 of five from 3 windows. Data exact |
| The same title telling the drive to stop behind every read | `titles_asked(false)` | 4 windows, each given up for the command: a window to a request, as Test 79 had for either. CMD18 4, CMD12 4 |
| Each of the seven commands | `sequence_commands()`: NOP, GETSCD, REQ_STAT, REQ_MODE, GET_VERS, GETTOC and GETTOC2 one after another at an open window, each with its own parameters | The window stands through all seven, has moved no sector, the drive's position is as it was; seven counted, each named by its bit; then STOP gives the window up and is named too |
| The others | The same scenario, on from there | SEEK gives the window up and a read across it opens none; a seek to the sector before a read makes that read consecutive; the functions INIT and RESET and an abort end the window, as in Test 79 |
| A number no command has | `window_gone()`: 95 in the command's place | The window is given up; nothing is counted or named, and nothing is read or written beyond the two words of bits |
| Reads by programmed transfer | `sequence_programmed()`, which the fixture did not have: its reads were by DMA | A window opened by one and three served from it; no command counted |
| The longest visits | `titles_asked()`, after every read and command | Each maximum never grows shorter, is no more than its kind's sum and no less than its mean; none for the timer, which this title's stream does not have |
| Test 79's scenarios | The list of `docs/toy-wire-stream-test-79.md`, Verification, in this build | Passed |
| The report | `tests/test_toy_wire_report.c` on the production formatter | Passed. The rows `XC KEPT LO HI` and `VMAX SCI TMR GAME D` with their values. Another title: sixteen rows, none of the movie player's, 581 body bytes with nothing counted, three pages of twelve lines. Toy Commander: every row, 1,009 of 1,012 bytes at a full run's values |
| All configurations | Both host targets in Tests 68 to 79 with their variants and in Tests 80 and 81, each with GAP, with LATER, without STAY, QUICK, MSB, RING, CLEAN, and with no part of Test 76 (`matrix.log`) | 147 of 148 jobs passed on `684f485`: both targets in 74 configurations. The one that did not is Test 81 without STAY, a variant that is not delivered: the whole reader's fixture did not compile there (a variable of Test 81's asked only by scenarios that a build without STAY does not have), and nothing of the reader was run. With the fixture corrected it passes both targets (`53d334d`, tests only) |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with Test 79's switches and `TOY_WIRE_QUERY=1` (`mutation-check.log`) | 303 of 303 caught, on the source as it stands with Test 81 |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 350 of 350 for Test 81, 303 of 303 for Test 80, 258 of 258 for Test 79, 226 of 226 for Test 78 and 218 of 218 for Test 77 change the code that is compiled |
| As linked | The five linked checks of Test 77 on the module's auxiliary | Passed |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The scratch program of Test 79's verification on this module's bytes (`games-admission.log`; the program is not in the tree) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, no session descriptor, the New and the Legacy match, the toggle, the mode record (0 to 7 for off, one byte changed), a blank cache seed slot, the handoff limit: all as the Games app asks |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Against the baseline | Test 79 built with this module's label | Residents and stage the same bytes. The worker and the auxiliary differ |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `c1b58a97...a3dc` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66 to 79 with their second modules and variants: all as recorded |
| Other suites | The Test 66 family's own fixture in every suite mode, and the suites that share the wire model (`other-suites.log`) | The Test 66 family's fixture passes in every suite mode. 17 of the 18 other suites pass; `test-sci-stream-quarantine` stops on its fingerprint of a historical source as before (`docs/toy-wire-stream-test-77.md`, Known limits 7) |

Not checked by any test here, because it runs on the console only: what
Evolution 2 sends between its reads. The service's request for the
subcode is what the tests send, by the game's stop screen of 24
September; the report's row names what the game does send.

## Review

One reader beside the author, without the author's conclusions, on
16 November 2026, on the change as committed (`e58dd2e`). It worked on
scratch exports outside the tree: built the module and its parts, ran
both host targets in this configuration, Test 79's and Test 78's and in
four variants, the change's own entries of the mutation list, planted
defects of its own, and some thirty scenarios of its own in the whole
reader's fixture, among them random games of reads and commands on a
quiet bus and four that cut runs short. Its result is in
`docs/evidence/toy-wire-stream-80/review.md` as it returned it. Its
seven findings, none of them a defect in what the change does, and what
was done with each:

| # | Finding | How established | What was done |
|---|---|---|---|
| 1 | No test had an SCI interrupt visit meet a command while a window stands, which on the console is the common case: the stream's interrupt comes every few milliseconds while a window fills. A build that ended the stream in such a visit passed all 318,027,918 checks | Run: the planted defect passed. In its own scenarios about 2,700 interrupt visits met one of the seven, pending and unacknowledged, with window and stream kept, no CMD12 and the data exact | Added (`684f485`): `sequence_interrupted()`, for a command that asks and for one that stops the drive, and the defect on the mutation list |
| 2 | Which commands give a window up was held for STOP and SEEK only. INIT, SET_MODE, PLAY, PLAY2, PAUSE and RELEASE could be moved to the kept side with every test passing | Run: planted, passed. The table as linked is right: its two words are `0x600C0000` and `0x00000114`, the seven | Added (`684f485`): `sequence_every_command()` over the fifteen commands the service accepts that are no reads, with the numbers 63 and 64, and seven entries on the list |
| 3 | `KEPT` counts a command when a window first meets it, not when the window has stood through it. One of the seven that fails is counted and the window then given up as `I`; one that is aborted, or ended by INIT or RESET, is counted and the window ended in no column | Run | Not changed: a count taken at the command's end costs code, and the auxiliary of the build behind this one has 32 bytes left. The report's section and Known limit 5 say what the value is |
| 4 | With one of the seven pending when the report is asked for, the card's loan is refused as before, but the stream now runs on and Start cannot restart the console. Test 79 had ended the stream by then | Run in both builds | Not changed: Known limit 6. The page says what to do, and the pages are shown |
| 5 | `VMAX`'s `D`, and `D`'s count and sum, cover only the visits that take the whole path and include the low reader's own blocking reads. Against Test 79's run of Evolution 2 they fall for that reason alone | Read | Known limit 4 says so |
| 6 | Toy Commander's report has three bytes of room at the test's sample of a full run; a real run is wider in two rows | Run against the console's report of Test 78 | Nothing to change for this test. Known limit 7 |
| 7 | Two slips in the change's own text: a comment that had SET_MODE set what a sector is, and a macro left defined at the file's end | Read | Corrected (`684f485`), with no compiled change |

What that reader examined and found sound, in short: none of the seven
commands changes the drive's position, its status or the sector mode,
and none can be accepted while a read is pending or unacknowledged;
the read address and count such a command leaves behind are used
nowhere but behind a pending read, which overwrites them; a kept
window cannot publish for a request that is no read or serve from a
wrong sector, in more than twenty random games on five buses, two
discs and both bit orders, every read exact and the card's port never
in fault; the low resident touches no stream the auxiliary owns across
such a command, and the low reader never runs over a live stream;
interrupt visits that meet one keep the window and the stream; one
asked for while a read of 150 sectors is served in slices is refused
and the read is whole; INIT, RESET and an abort with one pending end
the window cleanly; Toy Commander's movie with such commands between
its refills keeps its window; the table as linked, its bound and its
relocation; each command counted once whichever visit sees it first;
the longest visits are of the quantities their sums add; another
title's report is sixteen rows on three pages and no row can pass the
screen's width; without the switch every part is Test 79's bytes, and
Test 80's low residents are Test 79's.

What it could not settle, all of it for the console: how the stream
fares on a real bus now that it runs through Evolution 2's commands;
which commands that game sends; whether its windows then stand; and
where the report's looks fall beside a game's commands.

The reader's scratch copies and scenario files are not in the tree.
What was taken from them is in the tests, under the commit named
above.

## Package

`KUI-Stream-Reader-Test-80-16NOV2026-1509-CST-e58dd2e.zip`, 176,049
bytes, SHA-256
`76b69df9cb4ae6925bbbe3a534cae96323ec0df7141c28359456241652fdb142`,
prepared 2026-11-16 15:09 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | `e58dd2e6b641` | `F8F514E4` | `c1b58a97...a3dc` | Changed for the test only: Test 80 in the place of the installed New reader module (build f8807cb16561), which goes back from the owner's 06 NOV package afterwards |

Module SHA-256 in full:
`c1b58a9761b29299d542727a66a420ee185d3e30ef30629256f4e715c5f0a3dc`.

Retained on the card: everything else. The installed New reader module
(build `f8807cb16561`, 290,104 bytes, SHA-256 `09c1480c...481a` in the
candidate's record) goes back from the candidate's own archive when the
tests are done; it is not in this one. No KUP, bank or user data is
included.

The auxiliary's text is 20,236 bytes and its image 24,352 of 24,576;
the worker's text 39,700.

The report names the module by its title (`TEST 80`), its build
(`e58dd2e6b641`) and `PIPE`'s last value (`FBD`).

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 BUILD_ID=e58dd2e6b641 \
  build/toy-suite-80/retail-toy-test-80.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 toy-wire-host-tests toy-wire-integration-host-test \
  toy-wire-package-test toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 \
  python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: the steps of Console test, with the archive's
README. Cold CD boot, not Restart K-UI. Confirm `TEST 80` and `BUILD
E58DD2E6B641` on the report's first page, and `FBD` as the last value
of its `PIPE` row.
