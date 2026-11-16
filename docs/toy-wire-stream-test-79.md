# Toy Commander Test 79: the window for every title of the family

Status: built 16 November 2026 (build `9f183a7f72f5`), host and linked checks only.
It has not run on a console. A test module, not an accepted component,
and not a replacement for the installed New reader: it has no virtual
VMU and no block prefetch. Delivered together with Test 78
(`docs/toy-wire-stream-test-78.md`), its baseline.

The module is built from `9f183a7f72f5`: the change as first committed
(`fed458d6ef97`) with three corrections its review asked for
(`ab5b4fe`, `e4844b3`, `8e13e95`; Review, below), and the commits of
tests between and behind them (`23a23f5`, `63abdc5`, `9f183a7`), which
change no module. No module of the first commit was delivered.

The owner, 16 November 2026: "Evolution/Evolution 2 had no CD music. They
just were my first proving grounds for the prefetch. They got rid of the
lag. Wondering if this new loader would be better." And on the three
things that stood between the stream reader and those two games: "Go
ahead and work on those changes." This is the second of them. The
third, the virtual VMU on beside the stream, is not in this build.

## Baseline

Test 78: source `1d0e5bab78ba`, module `dbff8925...92de`. It is delivered
in the same package and has not run on a console either. Its own
baseline is Test 77 as the owner ran it
(`docs/toy-wire-stream-test-77.md`, Console result).

For the question the owner asked, the comparison is another one: the
installed New reader, `KUI/apps/games/retail-native-dual.kui` of build
`f8807cb16561`, with the virtual VMU off, on the same two games.

That file is the delivered native candidate's, whose own console
baseline is still pending (`docs/accepted-progress.md`, Current console
checkpoint). This test does not change the candidate or its archive: the
test module takes that one file's place on the card for the runs, and
the owner's own file goes back afterwards.

## Why

Since Test 77 the window is opened by what the game reads, not by what
the game is: a read that begins where the read before it ended. But two
places still asked for Toy Commander before any of that was reached. The
cold stage looked for that game's executable and its four movie files
before it called a disc a source for the window. And the auxiliary asked
for the verified title with every request. In any other game the reader
was the low reader alone, with no read-ahead of any kind, since this
test module has never had the installed reader's block prefetch.

## The change

Behind `TOY_WIRE_TITLES=1` on Test 78's recipe.

| | Test 78 | Test 79 |
|---|---|---|
| A disc the stage calls a source for the window | Toy Commander's, by its executable and its movie files | That, as before. And any other title's, when the launch has the New reader, the virtual VMU off and the SCI card, and the image has a raw 2,352-byte Mode 1 data track. Nothing of the disc is read for it |
| A request the auxiliary offers to the window | A read of 2,048-byte sectors on a raw track, of the verified title | A read of 2,048-byte sectors on a raw track |
| Which of those a window serves | Test 78's rule | The same rule, unchanged |
| A stream that has no timer, cut short by a busy bus | Its wait was ended by whatever looked at the stream next: the same visit, microseconds later. (No console run was ever in this state: Toy Commander has its timer) | The wait lasts its time by the game's own clock, 82 microseconds doubling to 1.3 ms as with a timer, and the first call that passes by after it ends it |

What stays Toy Commander's alone, by the same verification as before
(its executable's SHA-256 and size, its boot sector, and its four movie
files found): the five hooks in its movie player, its movie files'
ranges, the low reader's policy for requests inside them, and the
classification of its disc calls. Another title gets none of them.
Another title is another executable: Toy Commander's executable on any
other boot sector is no source at all, as before Test 79. Nothing is
written into another game's code or data beyond what the installed
module of this family writes there.

The last row is not the window's but the stream's, and is here because
this change is what lets a title without the timer in. The stream takes
a spare timer of the game's only where the SDK's table holds Toy
Commander's own default callback. Without one, a run cut short was begun
again at once, by the next look of the same interrupt visit. On a bus a
game's own DMA holds for milliseconds in every frame that is an
interrupt and a lost byte each time until the block cannot be mended
and the stream is stopped. The review's reader showed it in the host
model: 3,573 of 3,719 waits ended within the visit that began them.

So that the module can be started from the normal Games menu, where the
installed module's file is, it now meets two things Card Tools kept
from it. Both are part of this change and behind the same switch.

- **The owner's VMU setting.** Card Tools forces it off. This worker has
  no virtual VMU: with the setting on it refused, but by the launch
  path's trace only after the stage had run the virtual card's recovery
  on the SD card and taken memory from the game, and with a message
  that named neither. Now the
  stage stops at its first look at the setting, with `THIS TEST BUILD
  NEEDS VIRTUAL VMU OFF`, before it opens the card and before the game
  is loaded.
- **No report file.** Card Tools prepares the file the report is saved
  in. Without it the worker showed two fixed pages built for Toy
  Commander's movies. Now it shows the report's own text, twelve lines
  to a page, a page at a time for as long as the console is on. The
  text has a row more: the low reader's own totals, which only the fixed
  pages showed. The gesture waits for the game's disc command to finish,
  as it does before a save, and the report asks the resident for the
  card as a save does. That is what ends a card stream still running;
  left running, Start could not restart the console from the pages. The
  write itself refuses for want of a file before it touches the card.

What is not changed: the window's rules, the sector checks, the low
residents, the Games app; and of the card stream everything but the
wait named above.

## What another game gets, and what it does not

This matters for reading the result. The test module differs from the
installed New reader in more than the thing under test.

| | Installed New reader (`f8807cb16561`), as this tree's source of that family has it | This module |
|---|---|---|
| Read-ahead | Block prefetch: on the controller's heartbeat the reader fetches the blocks behind the last read into a cache of 384 blocks | The window: once a read begins where the last ended, a card stream fills up to 62 sectors ahead, by DMA. No block prefetch. A game whose reads never follow one another gets no read-ahead |
| A request of the game served from what was read ahead | At most 20 blocks a call of the game (the Evolution 2 capture: 73 sectors in 17 calls, 83 ms) | Up to 60 sectors in one call; larger requests 32 sectors a call |
| Image | Raw or 2,048-byte | The window needs a raw 2,352-byte track. On any other the low reader reads alone |
| What the read-ahead checks | Each block's CRC16 | Each block's CRC16, and each sector's header and EDC (one sector in sixteen while blocks arrive clean, every sector otherwise) |
| The stream's own timer | Not applicable | Taken only where the SDK's timer slot holds Toy Commander's own default callback: not in another title. There a stream cut short within its first 128 bytes waits its time and then for the game's next disc call: it stands still in between |
| The low reader after a receive overrun | Repairs the block from its CRC | Reads the block again (Test 66's low reader, unchanged since) |
| Memory below the game | To `0x8c007d00`, as this tree builds that family | To `0x8c007fe0` (Test 66's residents) |
| Virtual VMU | On or off | Off only |

The stream's timer and the block prefetch are the two that could decide
a result by themselves. Both are named in the outcomes below.

## Started from the Games menu

The installed main GUI is build `f8807cb16561`: the owner's own launch
record of Evolution 2 says so
(`docs/evidence/evolution2-f8807cb/launch-timing.txt`, Background New,
module path `0:/KUI/apps/games/retail-native-dual.kui`). That source is
not in this repository. Its Games app is: the SHA-256 the candidate's
records give for `src/apps/games_retail.c` (`649970e1...0721`,
`docs/evidence/native-shared-f8807cb/`) is this tree's file's, and the
files it includes have not changed here since the commit that brought
it. The binary itself has not been examined.

What that Games app does with a module at the installed path, with the
virtual VMU off (read from the source; the byte checks run on this
module's bytes, `games-admission.log`):

- It accepts the file. Nothing on this route is keyed on a build id, a
  hash or a manifest: the checks are the envelope and its CRC32, the
  layout header, exactly one dual-reader descriptor with the 0x47000
  lease, the VMU toggle and its one mode record, and the cache seed
  slot. This module has all of them where the installed one has.
- It writes three things into its loaded copy: the mode (7, reader
  only), the game's cache seed, and the track map. The file on the card
  is not written.
- It records the module it loaded in
  `KUI/apps/games/launch-timing.txt`: the GUI's build, the module's
  build, its payload bytes and CRC32, the reader and the VMU setting.

Things that send a launch somewhere else, so that this module is not
the one that runs:

| | Where it goes |
|---|---|
| Restart K-UI from the menu, with a saved bank active | A bank's pinned reader can replace the copied file. Cold boot from the CD reads the card's own file |
| Virtual VMU on and the card in slot A1 for that game | `retail-native-dual-a1.kui`, the installed module, with no message |
| Legacy (Y) | Accepted, and runs this build's legacy resident with no stream. Use New (X) |
| CD Music on (Toy Commander) | The Standard reader's CD-music module |
| A game whose executable the New reader cannot resolve | `retail-native-standalone.kui`, if installed. Not expected for the two Evolution games, which run on the installed New reader |

The stage's own lines say which module is running: `SCI STREAM READ
AHEAD READY` is printed by this build alone. Where the disc is no
source it prints `NO STREAM READ AHEAD / PLAIN READER`, and gives no
reason: the image, the card adapter, the reader, or Toy Commander's own
files not found.

## Module roles

| Part | Changed | How it is checked |
|---|---|---|
| Auxiliary (`native-ra-aux`) | Yes: one condition in the request's path, and the stream's wait without a timer. Text 20,060 of 20,472 bytes (baseline 20,028), image 24,192 of 24,576 (24,160) | Host scenarios, the stream's own test, mutations, linked checks |
| Stage | Yes: which disc is a source; the hooks' condition; the stop for the VMU setting; one message line | `tests/test_toy_titles_stage.c`, built from the stage's own text, and twelve mutations of it. The hooks' condition, the stop and the message run on the console only |
| Worker (`native-worker`) | Yes: the report's rows, the pages, the gesture's wait, the screen title. Text 39,540 bytes (baseline 39,124) | The text, its lines for the screen, the page loop and the asking for the card without a file: `tests/test_toy_wire_report.c` on the production source, and fourteen mutations of it. The gesture's wait and the pause between pages run on the console only |
| Low residents | No | Byte-identical with Test 66's build label to Test 66's |
| Installed readers and GUI | Not touched | The Games app's byte checks run on the module |

## The report

By the gesture: both triggers and A+B+X+Y held for about three seconds,
with the controller in port A. From a Games launch nothing is saved. The
screen shows `REPORT NOT SAVED / FILM EVERY PAGE`, twelve lines of the
text and `START / REBOOT TO K-UI CD`, each page for about five seconds,
then the next, round and round. Four pages hold a usual report. The
first lines are `K-UI TOY COMMANDER REPORT`, `TEST 79` and `BUILD
9f183a7f72f5`.

Test 78's rows, with these differences:

| Row | Change |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | `PT` is `7BD`: Test 78 (`3BD`) and TITLES (`400`) |
| `BIG N S BACK TOY` | `TOY` added: 1 the verified Toy Commander, 0 any other title |
| `SD BLK PIO OVR CRC DECL` | New, behind `BIG`. The low reader's own totals: the blocks it read, those of them by programmed transfers, its receive overruns, its CRC16 failures. `DECL`: the times the stream was not let take the card over |

In another title the rows that say something:

| Row | Reads as |
|---|---|
| `X OW ...`, `AHEAD ... HIT ... OS` | `OW` windows opened; `OS` sectors served from them; `HIT` requests served from them to their end |
| `X ... E I C W O` | Why windows were given up: an error, the request state, a command that is no read, a read the game waited for too long, a read elsewhere |
| `BIG N S BACK` | Requests of more than sixty sectors served through a window, their sectors, and those that went back to the low reader |
| `SD BLK PIO OVR CRC DECL` | What the low reader read: the game's requests it served, and the blocks it brought for a window that was opening (`W BLK`'s last value, `SEEK`, counts those). Less those, and against `OS` (a sector is 4.6 blocks), it says how much of the game's reading the windows had |
| `W START SEG CUT WAIT FULL`, `W BYTES WTICK TMR ERR DET` | The stream: begun, runs, runs cut, waits, window full; bytes, ticks spent waiting, the timer (0: none), the first refusal to start and its register values |
| `K LIGHT EDC ...`, `W FILL REF BAD`, `S C S F LF AM BC` | The sector checks and why streams were stopped |
| `V SCI TMR GAME N TICKS` | The reader's visits by caller, and their time |

The movie rows (`I ...`, `B ...`, `IP`, `BP`) are Toy Commander's
player and stay at zero in another game.

## Console test

Two games, each twice: first with the installed module, then with this
one in its place, both with the virtual VMU off, the New reader and the
same image. The virtual VMU has to be off for this build, so saves on a
virtual card are not there: a new game, or a save on a real VMU.

1. With the installed module: Evolution 2, then Evolution 1: the
   opening movie, then to where the lag was before the prefetch, and a
   few minutes of play there. This is the reference for the same day,
   the same setting.
2. Keep a copy of the card's `KUI/apps/games/retail-native-dual.kui`,
   put this module in its place, cold boot from the CD.
3. The same two games the same way. The stage must say `SCI STREAM READ
   AHEAD READY`. At the end of each, the gesture, the pages filmed, and
   Start.
4. Put the kept file back.

Asked of the owner: for each game whether play was as smooth as with
the installed module, smoother or less so, and where; the opening
movies likewise; loading times as felt, and the time from choosing the
game to its first picture if one thing is timed; the film of the pages;
`KUI/apps/games/launch-timing.txt` as it stands after the last launch
with the test module (each launch writes it anew: it names the GUI's
build and the module's build, bytes and CRC32); anything on the screen
that stopped a launch.

| Outcome | What it says |
|---|---|
| As smooth as the installed module or smoother; `OW` small, `OS` large against `SD BLK`, `BACK` and `E` zero | The window does the prefetch's work in these games, with requests served in a few calls. Next: the virtual VMU beside it |
| Lag back; the stage said `NO STREAM READ AHEAD / PLAIN READER` | The image has no raw data track, or the reader was not New. Nothing was tested: the reader was the low reader alone. A raw image, or the cooked-image step of the plan |
| Lag back; `OW` zero or few, `OS` small, `SD BLK` large | The game's reads do not follow one another often enough to open windows. The trigger has to learn this game's pattern, or the block prefetch has to stay for it |
| Lag back; `OW` large with `O` near it | Windows opened and given up for reads elsewhere: the game reads in two places by turns. One window is not enough |
| Lag back; `OW` large with `C` near it | The game sends commands that are not reads while it plays, and each gives the window up. Which commands, and whether a window may stand through them, is the next thing to read |
| Lag back; windows served the reads (`OS` large), `WAIT` and `WTICK` large, `TMR` 0 | The stream stood still between the game's calls for want of a timer (`WTICK` over `WAIT` is how long, in ticks of 1.28 microseconds). Next: a timer by the title's own admitted SDK values, or the controller's heartbeat, for every title |
| `START` zero or `DECL` large, `ERR` and `DET` set | The stream could not take the card over: `DET` holds the registers that refused |
| `E` not zero, `W FILL` `BAD` or `K` `LB` large | Sectors failed their checks. With `SD` `CRC` at zero that points at the image's raw sectors (EDC), not at the card |
| A stop screen, a hang, wrong picture or sound | The change does not hold for this game. The screen's lines, or the report's rows, say where |

Toy Commander needs no run of this build: Test 78's run through Card
Tools covers what the two builds share. Started from the Games menu with
New (X), CD Music off and the virtual VMU off, this build runs it with
its hooks as before, and shows the report on the screen.

## Known limits, not changed here

Test 78's limits stand. Added by this change, or made to matter by it:

1. No virtual VMU. With the setting on the launch stops; nothing else is
   offered. The Games app has by then done its own steps for a launch
   with the setting on (it may create the game's virtual card and a
   cache file): those are the installed GUI's, as for any such launch.
2. No block prefetch. A game the window does not serve reads slower
   here than under the installed module.
3. No timer for the stream in another title. Its waits last their time,
   but only a call of the game's ends one: with no disc call for a
   while, a stream that was cut short stands still for that while. The
   controller's heartbeat does not move it in this build. A title's own
   timer, by the SDK values the reader already admits for that title,
   is the next step if the console shows the standstill to matter.
4. A raw image whose sectors do not carry a valid EDC. The window
   checks it and the installed reader does not. Every read behind
   another then opens a window that fails nine rounds before the low
   reader may read, and nothing remembers it: in the review's host run
   twelve consecutive reads took 100 CMD18 and 112 calls of the game
   where the plain reader takes one CMD18 and 24 calls. Data stays
   exact. A patched file's stale sector costs one such episode each time
   a window reaches it. No such image is known to be in use; a rule
   that stops opening windows after a few in a row have failed is owed
   before the reader is anyone's default.
5. A title that sends GD commands other than reads while it plays gives
   an open window up with each (`X`'s `C`), and the next read behind
   another opens a new one.
6. A stream runs on behind the game's last read until its window is
   full, and then stands with the card inside its command (Test 77's
   limit). In another title that is now reachable too: a game that
   rewrites its interrupt table or vector base meanwhile would leave the
   SCI's error unanswered. The installed reader has that exposure only
   while a request is active.
7. While a large request is owed more than the window holds, every
   interrupt visit reads for up to 1,172 ticks (1.5 ms) with interrupts
   held, for as long as the request lasts. Toy Commander's player waits
   for its reads; a game that keeps running through a long read has
   those 1.5 ms taken from it each time.
8. The stage's `SCI STREAM READ AHEAD READY` speaks of the disc: any raw
   data track. A window is per track. A disc with a raw track 1 and a
   2,048-byte track 3 would say READY and never open a window (`OW` 0).
9. The lines the stage shares with the installed module still say
   `PIO CACHE` and `PREFETCH AUTO`. Neither is in this build.
10. The low residents use memory to `0x8c007fe0`, 736 bytes more than
    the installed module's. Only Toy Commander has run with that
    placement on a console.
11. The report's first line still names Toy Commander.
12. The gesture ends the run. With no report file it now waits up to a
    second for the game's disc command to finish. A game still reading
    after that second keeps the card: the pages are shown, the stream is
    not ended, and Start cannot restart the console
    (`RESTART BUSY / TRY AGAIN OR POWER OFF`).

## Verification

The module is built from commit `9f183a7f72f5` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree.
Logs are in `docs/evidence/toy-wire-stream-79/`. None of this is a
console result.

| Check | What ran | Result |
|---|---|---|
| Whole reader | `tests/test_toy_wire_integration.c`: the real resident, low reader, GD service, auxiliary and stream on the card model, the scenario list twice | Passed: 307,407,911 checks; every request completed with the image's exact bytes |
| Another title on a raw disc | `titles_other()`: no verified title, no movie file, no hook, and another title's word in the SDK's table where the stream looks for its timer; Evolution 2's refills of 73 and 50 sectors, five requests | No timer taken or run, the title's table untouched. The first request the low reader's; one window, four requests served from it; nothing counted as a movie's; the low reader's movie policy never on; one CMD18, no CMD12 |
| The same on a bus that cuts runs short | `titles_other()` with stalls 1500/20 and 150/80, the stream's clock and the reader's being one clock as on the console | No timer. Stalls 1500/20: 2,223 waits, none shorter than its time, 4 of five requests from 1 window, 39 streams begun. Stalls 150/80: 10,793 waits, none shorter than its time, 2 of five requests from 3 windows, 264 streams begun. Data exact; the title's table untouched |
| Another title on a disc the stage did not call a source | `titles_other(false)` | No window; the low reader read all five requests in one card command |
| A wait without a timer | `tests/test_toy_wire_stream.c`, the courses with no timer free, the game's clock running | A look before a wait's time left it standing 234,299 times; the first look after it ended it 16,619 times |
| The same in the whole reader | `degraded()`: the scenario of old for no free timer | A wait is not due before its time and a look leaves it standing; after it the next call of the game's ends it |
| Which disc the stage calls a source | `tests/test_toy_titles_stage.c`, built from the stage's own text with its globals supplied | A raw Mode 1 data track on the SCI card, with the New reader and the virtual VMU off, and nothing else: not a 2,048-byte image, not audio tracks alone, not Mode 2, not 2,448- or 2,336-byte sectors, not the other card adapters, not the other reader, not with the virtual VMU on. Nothing of the disc is read for it. Toy Commander takes the path it took. Its executable on another boot sector is no source at all; another executable is another title, whatever of Toy Commander's its boot sector, size or checksum has |
| Test 78's scenarios | The list of `docs/toy-wire-stream-test-78.md`, Verification, in this build | Passed, with Toy Commander as the verified title as before |
| The report | `tests/test_toy_wire_report.c` on the production formatter | Passed. The rows `BIG N S BACK TOY` and `SD BLK PIO OVR CRC DECL` with their values, `TOY` 0 for a title that is not the verified one. 699 body bytes with nothing counted, 941 at a full run's values; every row fits |
| The report as lines for the screen | The same test, on the function the pages use | Every character of the text once and in order; no line over 50 places, none empty, none beginning or ending with a space; a line that is cut is cut at a space; lines of 50 places, of 51 without a space and of two words of 30 come out as they must |
| The page loop | The same test, with the worker's screen and its pause between pages supplied | A page is the heading, at most twelve lines of the text and the line for Start; the pages together are every line of the text in order, round after round; the screen is put back behind every page; from the page on which Start was refused the last line says so |
| Asking for the card without a report file | The same test, with the resident's lease supplied | The armed worker asks once, with the report's own transaction, and nothing counts as saved; unarmed, or with a controller exchange pending, it asks for nothing |
| All configurations | Both host targets in Tests 68 to 77 with their variants and in Tests 78 and 79, each with GAP, with LATER, without STAY, QUICK, MSB, RING, CLEAN, and with no part of Test 76 (`matrix.log`) | 109 of 112 jobs passed on `9f183a7`: both targets in 56 configurations. The three that failed are variants that are not delivered, and failed in the fixture, not in the reader; with the fixture corrected they pass (`d0c5bf1`, tests only; below) |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with `TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1` (`mutation-check.log`) | 258 of 258 caught |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 258 of 258 for Test 79, 226 of 226 for Test 78 and 218 of 218 for Test 77 change the code that is compiled |
| As linked | The five linked checks of Test 77 on the module's auxiliary | Passed. The window's state is used at its cached address only: 22 constants in the linked code |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | A scratch program of the launch path's reader on the packaged bytes: the production headers and sources, with three static functions of the Games app copied (`games-admission.log`; the program is not in the tree) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, no session descriptor, the New and the Legacy match, the toggle, the mode record (0 to 7 for off, one byte changed), a blank cache seed slot, the handoff limit: all as the Games app asks |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Against the baseline | Test 78 built with this module's label | Residents the same bytes. The worker, the auxiliary and the stage differ |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `557822ec...47be` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66 to 77 with their second modules and variants: all as recorded. Test 78's recipe gives the module built at Test 78's own commit, byte for byte |
| Each commit by itself | The first two corrections' commits and the tests' commit behind them, each exported alone (the third correction's is the source this module is built from): both host targets for Tests 78 and 79, and the lists' anchors (`commits-one-by-one.log`) | All passed; no entry of a list stale at any of them |
| Other suites | The Test 66 family's own fixture in every suite mode, and the suites that share the wire model (`other-suites.log`) | The Test 66 family's fixture passes in every suite mode. 17 of the 18 other suites pass; `test-sci-stream-quarantine` stops on its fingerprint of a historical source as before (`docs/toy-wire-stream-test-77.md`, Known limits 7) |

Not checked by any test here, because it runs on the console only: the
stage's stop for the VMU setting and its message line; the bind's
condition for the hooks; the gesture's wait; the pause between pages.
They were read against the code they call (below, and Review).

What was read rather than run, and by whom:

- The launch path from the Games menu to the game, for this module at
  the installed path, with the virtual VMU off and on: a reader's trace
  of the source (`launch-path-trace.md`). Checked again by the author
  in these points: the Games app's source is the installed GUI's by the
  hash the candidate's records give; the stop's place before any card
  step; the stream's timer and what ends a wait without one; the
  resident's loan and the refusal of the write without a file; that
  Start's restart refuses while DMA channel 1 is on, which is why the
  loan is asked for.
- The Evolution 2 capture of 25 October for what this build may expect
  there: the game's clock is the one the reader's timing rules need
  (`clock_bad` 0), and its disc calls are never further apart than
  16.5 ms, which is the longest a stream without a timer stands still
  behind a wait in that game.

What the verification itself found and changed, before the first
commit:

- The stage's test caught an audio track being taken for a raw data
  track: audio sectors are 2,352 bytes too. The stage now asks for a
  data track.
- The reader of the launch path found that a report shown on the screen
  alone left out the low reader's totals, which are most of what
  another game's run has to say where no window serves it. The row was
  added.
- Reading Start's restart showed that a card stream still running at
  the gesture would leave `RESTART BUSY` on the pages for good. The
  report without a file now asks for the same loan a save does.

And after it:

- The list's first whole run left ten entries uncaught, all of the
  course by piece that Test 78 stopped keeping. They are marked as gone
  for Test 78 (`23a23f5`, tests only).
- The matrix's first run failed in two variants that are not delivered,
  without STAY and without QUICK, where the fixture asserted what only
  the delivered configuration does. The fixture now says what holds
  there (`23a23f5`).
- The review's three corrections (Review, below).
- The list's first whole run on the corrected source stopped at its own
  first step: the stream test failed without any mutation. The list
  runs that test over eight courses, and the first course with no
  timer free and a bus that cuts runs is the tenth, so the new check
  for waits without a timer met none. The list runs ten courses with
  this test's switch since (`9f183a7`). Two older entries, of the line
  that says when a wait is due for a call, were made again on the line
  this test compiles.
- The matrix on this module's commit failed in three variants that are
  not delivered. Tests 78 and 79 with no part of Test 76 did not
  compile the review's scenario for what a request is owed. Test 79
  with LATER had a scenario that expected a wait without a timer to be
  ended by the visit that began it. Both are the fixture's; it is
  corrected (`d0c5bf1`, tests only), and the three jobs pass there.

## Review

Two readers beside the author, each without the other's conclusions, on
16 November 2026. The first traced the launch from the Games menu to the
game for this module at the installed path, before the change was
committed (`docs/evidence/toy-wire-stream-79/launch-path-trace.md`; what
came of it is under Verification). The second reviewed the committed
change (`fed458d6ef97`) and tried to break it in a private copy, with
the fixture and the stage's text on the host. Its six findings, and what
was done with each:

| # | Finding | How established | What was done |
|---|---|---|---|
| 1 | Every title but Toy Commander runs the stream without a timer, and a run cut short was begun again by the next look of the same visit, or left until the game's next disc call | Run in the host model with another title's table word: 3,573 of 3,719 waits ended within the visit that began them; a held bus gave 191 interrupt visits one behind the other | Corrected (`8e13e95`): a wait without a timer lasts its time. That the stream stands still between the game's calls is Known limit 3 |
| 2 | A raw image whose sectors carry no valid EDC: every read behind another opens a window that fails nine rounds before the low reader may read | Run: 100 CMD18 and 112 calls for twelve reads that the plain reader serves with one CMD18 and 24 calls; data exact | Not changed. Known limit 4, with the rule that is owed |
| 3 | Toy Commander's executable on another boot sector was called another title by the stage and then given Toy Commander's hooks by the bind, with no movie file known | The stage's text run on the host; the bind read | Corrected (`ab5b4fe`): another title is another executable, and the hooks ask for the movies' places |
| 4 | A report asked for while a request stays pending leaves the stream running, and Start can then never restart the console, though the page said to try again | Read | The page's line corrected (`e4844b3`). The hole is older than this test and stays: Known limit 12 |
| 5 | `NO RAW SCI IMAGE` was shown for Toy Commander on a raw image whenever its own file scan failed | Read | The line no longer gives a reason (`ab5b4fe`) |
| 6 | The tests left out what the change adds outside the auxiliary's one line: the fixture's other title had Toy Commander's timer; the rules of the lines for the screen were not pinned (a row of 51 places, and a row cut in the middle of a value, both passed); the worker's and the stage's changes were under no test and no mutation | Run: planted defects that passed | Corrected: the other title has another title's table word, on a quiet bus and two that cut runs short (`8e13e95`); the page loop, the lines for the screen and the asking for the card without a file are run by the report's test (`e4844b3`); 23 mutations more are listed |

What that reader examined and found sound, in short: with no verified
title, no movie file and no hook, every index by movie lies behind its
test and nothing that is Toy Commander's is entered; no fixed address of
the game is written for another title, and the only ones read are the
two table words the timer's choice looks at; an SCI error is always
answered or the stream halted; a request the game polls cannot wait for
ever without a timer; the data path is Test 78's; the report without a
file writes nothing to the card, and Start restarts the console behind
the loan; the pages cannot read past the text, show a blank page or
overrun a row; the stop for the VMU setting stands before the card is
opened; Toy Commander's own path is the one it was; and without the
switch the preprocessed sources of every part are Test 78's.

What it could not settle, all of it for the console: how the stream
without a timer fares on a real bus; whether another title's clock is
the one the reader's timing rules expect (Evolution 2's is, by its
capture of 25 October); which titles send commands other than reads
while they play; a title that rewrites its interrupt table while a
stream runs behind its last read; how common raw images without a valid
EDC are.

The reviewers' private copies and scenario files are not in the tree.
What was taken from them is in the fixture and the tests, under the
commits named above.

## Package

`KUI-Stream-Reader-Test-79-16NOV2026-1004-CST-9f183a7.zip`, 176,512 bytes, SHA-256
`786447b57259d38461ab6ad9467265b656e70f5b6bc2dbcfd9e7073f6f27bfcb`, prepared 2026-11-16 10:04 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | `9f183a7f72f5` | `44C11C5B` | `557822ec...47be` | Changed for the test only: Test 79 in the place of the installed New reader module (build f8807cb16561), which the owner keeps and puts back |

Module SHA-256 in full: `557822ecfcd9761f2617d146e69b0a5918ae745cdb16247a607466242f4147be`.

Retained on the card: everything else. The installed New reader module
(build `f8807cb16561`, 290,104 bytes, SHA-256 `09c1480c...481a` in the
candidate's record) is the owner's to keep and to put back; it is not
in the archive. No KUP, bank or user data is included.

The report names the module by its title (`TEST 79`), its build
(`9f183a7f72f5`) and `PIPE`'s last value (`7BD`).

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 BUILD_ID=9f183a7f72f5 \
  build/toy-suite-79/retail-toy-test-79.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 toy-wire-host-tests toy-wire-integration-host-test \
  toy-wire-package-test toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 \
  python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: the steps of Console test, with the archive's
README. Cold CD boot, not Restart K-UI. Confirm the stage's line `SCI
STREAM READ AHEAD READY`, `TEST 79` and `BUILD 9f183a7f72f5` on the report's
first page, and `KUI/apps/games/launch-timing.txt` after the launch:
`source_module_build: 9f183a7f72f5`, `metric.module_crc32:
0x44C11C5B`.
