# Toy Commander Test 78: a window serves a request of any size

Status: delivered 16 November 2026 (build `1d0e5bab78ba`) and run once
on the console by the owner the same day (Console result). A test module
for one title, not an accepted component. Delivered together with Test 79
(`docs/toy-wire-stream-test-79.md`), which is this build with one change
more.

The owner, 16 November 2026, on the three things that stand between the
stream reader and the two Evolution games
(`docs/fmv-stream-reader-plan.md`, The order from here): "Go ahead and
work on those changes." This is the first of them: one behavior change
against a named baseline, with its own console test, as `AGENTS.md` asks.

Card Tools has no Test 78 entry. The module is installed where it looks
for Test 66 and starts from "66 SCI movie playback".

## Baseline

Test 77: source `ded3083ecdf8`, module `1c1e54f3...7323`, payload CRC32
`AB8FE168`, as run on the owner's console
(`docs/toy-wire-stream-test-77.md`, Console result). In short: the second
movie at 1,959 and 1,965 of 1,968 frames, the first all 331; 27 and 34
windows opened, each given up for a read elsewhere or a command; no
error.

## Why

A window served whole requests only, of at most sixty sectors: one had to
fit beside the sector still being received. A larger read was the low
reader's, and a window that stood at its first sector was given up for it
with everything it had read ahead.

Evolution 2 reads in larger pieces. Its console capture of 25 October
(`docs/evidence/native-read-ahead-pio-pair-observed-2026-10-25.json`) has
eight requests in a row of 73 and 50 sectors by turns, each beginning
where the last ended. Under Test 77 the request of 50 sectors would open
a window, the next of 73 would give it up, and so on: every second
request read by the low reader with the game waiting, and the read-ahead
between them thrown away each time. Toy Commander itself reads a movie's
first 384 sectors in one request.

## The change

Behind `TOY_WIRE_ANY=1` on Test 77's recipe.

| | Test 77 | Test 78 |
|---|---|---|
| A request the window serves | Of at most 60 sectors | Of any size |
| How | Whole, once the window has all of it | Up to 60 sectors: whole, as before. More: a slice at a time as the stream delivers, at most 16 sectors a publication and two publications a visit |
| Where the request has got to | Nowhere or done | The request's own cursor counts the sectors it has had; the window stands at its next sector |
| A read of more than 60 sectors that begins behind the read before it | The low reader's | Opens a window like a smaller one |
| A read of more than 60 sectors at an open window's first sector | The window is given up for it | Served from the window, which stays |
| A request the window has begun and cannot finish | Cannot happen | Goes back to the low reader whole: its cursor is begun again at its first sector, and the low reader delivers every sector of it, over what the window wrote |

A request of up to sixty sectors takes the path it took in Test 77,
line for line: that is every refill of Toy Commander's movies.

Two things follow and are part of the change, behind the same switch:

- **What the game is told of a request's progress.** The resident
  reports the cursor's count to the GD service after every visit, so a
  large request's status now shows bytes transferred while it is still
  under way, as it does when the low reader serves it. A request that
  goes back to the low reader is read again from its start; what the
  game was told never goes back, because the service only moves that
  count forward.
- **What a waiting request asks of the wire.** The pump has the stream
  end a run at the byte where a waiting request's last sector is in. It
  now reckons that from what the request is still owed.

What is not changed: the card stream, the sector checks, the low
residents, when a window is opened (a read that begins where the read
before it ended, of the verified title, with the SCI route selected and
the low reader at rest), every reason a window is given up for, the
movie policy for Toy's movie requests, and the rule that a request the
game waits for keeps its window while the window grows.

### Room

The auxiliary's image had 32 of its 24,576 bytes free. This build does
not keep the second movie's course by piece: the report's rows `TF`,
`TD`, `TW`, `TB` and `TR` and the code that filled them are not
compiled. The owner has put that movie's remaining loss to rest ("I
don't know that chasing down those 9 frames is value-added"). The image
is 24,160 bytes, 416 free. The frame counts of both movies,
the player's rows and everything else of Test 77's report are kept.

### What the game's reads become

- **A movie's start.** The file's first two sectors are the low
  reader's. The 384 sectors behind them begin where that read ended:
  they now open the window and come through it, and the refills behind
  them are served from the same window. Test 77 had the low reader read
  the 384 and opened the window with the first refill. The first frame's
  time (`I FIRST`) says what that costs or gains. (Those 384 sectors lie
  in the file's first 386, which is not the low reader's movie path:
  Known limit 4 is not about them.)
- **A movie's refills.** As Test 77.
- **Ordinary loading.** A large read behind another read has a window
  where it had none. Whether that is quicker or slower than the low
  reader for one large read is not known: the wire is the same, the
  window checks every sector and copies it once more. The owner's
  observation of loading times is part of the result.
- **Refills in large pieces.** The Evolution 2 pattern, in the host
  model: the first request the low reader's, the second opens a window,
  and every request after it is served from that window with no card
  command.

## Module roles

| Part | Changed | How it is checked |
|---|---|---|
| Auxiliary (`native-ra-aux`) | Yes: the window's service and what the pump asks of the wire; the course by piece left out. Text 20,028 of 20,472 bytes (baseline 20,424), image 24,160 of 24,576 (24,544) | Host scenarios, mutations, linked checks |
| Worker (`native-worker`) | Yes: the report's rows and the screen title | `tests/test_toy_wire_report.c` |
| Low residents | No | Byte-identical with Test 66's build label to Test 66's |
| Stage, entry | No source change. The stage carries the worker and the auxiliary | Its own objects compared with the baseline's |
| Installed readers and GUI | Not touched | The module is the test module at the Test 66 path |

## The report

Test 77's rows, with these differences:

| Row | Change |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | `PT` is `3BD`: Test 77 (`1BD`) and ANY (`200`) |
| `BIG N S BACK` | New, behind `X`. Requests of more than sixty sectors a window served to their end; their sectors; requests a window had begun to serve that went back to the low reader |
| `TF`, `TD`, `TW`, `TB`, `TR` | Left out, with the code that kept them |
| `BP ON OT OC OI SN ST` | Still written last; with the five rows gone it always fits |

`AHEAD`'s `B`, `I` and `OS` count sectors as they are handed over, so a
large request that went back to the low reader, or that the game
aborted, has left its slices there: with `BACK` not zero they overstate
what the windows spared the low reader. `HIT` and `BIG` count requests
served to their end.

## Console test

One run of Toy Commander, as Test 77's second run: both movies without
skipping, then through the menus into the first level, a minute of play,
then the report.

| Outcome | What it says |
|---|---|
| Frames and skips as Test 77; `BIG` `N` two or more, `BACK` zero | The movies' large opening reads came through the window and the refills were not disturbed |
| The second movie's first frame (`I FIRST`) later than 0.81 s by more than a few hundredths, or `PIPE` `B` and `I` fewer than three | The movie's start has changed, as expected: later says the window is slower than the low reader for the opening read, and by how much |
| `BACK` not zero, `X` `E` or `W` not zero | Requests went back to the low reader: an error, or a wait that outlasted the stream |
| The level loads and plays normally; `BIG` counts grow there | Large ordinary loads went through windows |
| Loading visibly slower than before | The window costs a large load more than the low reader does. Next: large reads open no window by themselves, and only an open one serves them |
| An error screen, a hang, wrong picture or sound | The change does not hold. The report's rows say where |

## Console result

The owner's run of 16 November 2026 through Card Tools, by the package's
README, with CD Music and virtual VMU off. One saved report of 1,024
bytes, `/reports/toy/66-12.txt`, headed `TEST 78`, `BUILD 1d0e5bab78ba`,
`PT` `3BD`:

| File | SHA-256 |
|---|---|
| `docs/evidence/toy-wire-stream-78/console-report-66-12.txt` | `da85eb66...6b49` |

Decoded by `docs/evidence/toy-wire-stream-79/decode_report.py`
(`console-report-decoded.txt` here), which reads this test's rows and
Test 79's; Test 77's two runs are as its own decoder has them.

Owner's observation: "66-12 is build 78, where no issues were noted. I
did not even notice any frame skips so maybe we silently fixed those
last 9 frames along the way."

| | Test 77, run 1 | Test 77, run 2 | Test 78 |
|---|---|---|---|
| First movie: frames submitted / on time | 331 / 330 | 331 / 330 | 331 / 330 |
| Second movie: frames submitted of 1,968 | 1,959 | 1,965 | 1,968 |
| Of those on time and in sequence | 1,953 | 1,960 | 1,966 |
| Frames skipped | 9 | 3 | 0 |
| Longest gap between frames | 0.100 s after 1,133 | 0.065 s after 1,177 | 0.060 s after 1,796 |
| Second movie's first frame | 0.806 s | 0.809 s | 0.803 s |
| Calls that put a frame out, each | 1,958, 30.2 ms | 1,964, 30.2 ms | 1,967, 30.0 ms |
| Windows opened, first movie / second | 3 / 3 | 3 / 3 | 3 / 3 |
| Sectors served from them | 3,421 / 25,824 | 3,421 / 25,824 | 3,421 / 25,828 |
| Windows opened by other reads, sectors served from them | 21, 106 | 28, 133 | 21, 2,241 |
| Requests of more than sixty sectors served through a window, their sectors | none can be | none can be | 6, 2,135 |
| Of those, back to the low reader | | | 0 |
| Requests served from a window | 2,450 | 2,457 | 2,476 |
| Windows given up: for a read elsewhere / for a command | 23 / 4 | 29 / 5 | 23 / 4 |
| Given up on an error / on the request's state / on a wait | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| Sectors verified into windows / served | 29,914 / 29,351 | 30,149 / 29,378 | 32,047 / 31,490 |
| Delivery visits from the game's disc calls | 10,772 | 46,531 | 79,274 |
| All pumps | 9.17 s | 9.20 s | 10.06 s |
| Card blocks, pump time for each | 138,695, 66.1 us | 139,684, 65.9 us | 148,685, 67.6 us |
| Streams begun | 700 | 685 | 794 |
| Ended on a byte between blocks neither idle nor token | 655 | 627 | 744 |
| The same for every hundred overruns in a run | 3.22 | 3.10 | 3.55 |
| Blocks handed on with lost bytes / sectors completed from parity | 2,786 / 1,775 | 2,704 / 1,716 | 3,086 / 2,001 |
| Sectors with more lost bytes than the parity fills (read again) | 0 | 0 | 1 |
| Rebuilt bytes not confirmed / sectors failing the EDC watch | 0 / 0 | 0 / 0 | 0 / 0 |
| Stream errors | 0 | 0 | 0 |

What the run says, by the outcomes above:

1. **Large requests come through the window.** Six requests of more
   than sixty sectors, 2,135 sectors between them, were served from
   windows to their end, and none went back to the low reader. The
   sectors served from windows that are no movie's are 2,241 against
   106 and 133. No window was given up on an error, on a wait or on the
   request's state. That is the first and the fourth row of the
   outcomes.
2. **The movies are as they were.** The same sectors from the movies'
   windows (four more in the second movie's), the second movie's first
   frame at 0.80 s, a frame's call 30.0 ms against 30.2, a card block
   67.6 microseconds against 66.
3. **No frame was lost, and that is not claimed as this change's.** The
   second movie put out all 1,968 frames, 1,966 of them on time, where
   Test 77's two runs lost 9 and 3. Nothing in this change is on a
   movie's path: its requests are of sixty sectors at most, and a
   window served those whole before. Five runs of what is one reader in
   that scene have now lost 26, 15, 9, 3 and 0 frames. The spread is
   the scene's, as Test 77's result said; this run is its good end.
4. **One sector had more lost bytes than its parity fills.** It was
   read again (`W FILL` `REF` 1, the first in a console run). Nothing
   that failed a check was served.

The module stays a test module. Its change is carried by Test 79 and
what follows it.

## Known limits, not changed here

Test 77's limits stand (`docs/toy-wire-stream-test-77.md`), the standing
stream first among them. Added by this change:

1. A large request is served at the stream's pace from the moment the
   window is empty: about two sectors a millisecond less the wire's
   pauses. Nothing says yet whether the low reader is quicker for a
   single large read.
2. A request that goes back to the low reader after the window has
   served part of it is read twice in that part. It needs an error, a
   wait of a quarter second without a sector, or a failed stop command;
   none has been seen on the console.
3. The rows by piece are gone from this build's report. A later test
   that needs them needs room first.
4. On the low reader's movie path, where the game's CHECK does not
   visit the reader, a large request that has had two slices waits for
   the game's next EXEC, even with its remaining sectors in the window
   and the stream at rest. With no EXEC it would wait for good; Test 77
   completes such a request, which is the low reader's there. Toy
   Commander's player calls EXEC all the time, and its refills on that
   path are small. Off that path every call of the game's visits, and
   the cost is a call more for every 32 sectors (Review, finding 1).
5. A request that goes back is delivered again from its first sector,
   also below what CHECK has reported as transferred. They are the same
   bytes; a game that had changed them in place before the request
   completed would have its change undone. None is known.
6. While a large request is owed more than the window holds, every
   interrupt visit reads for up to 1,172 ticks (1.5 ms) with interrupts
   held, as it did for a waiting request of up to sixty sectors, but now
   for as long as the large request lasts.

## Verification

The module is built from commit `1d0e5bab78ba` on a clean worktree
(`source_dirty: false`). The checks ran on that worktree, and those of
the source as it stands with Test 79 on a clean worktree of Test 79's
commit, which has this one as its parent. Logs are in
`docs/evidence/toy-wire-stream-78/`; the matrix and the other suites,
run once for both tests, in `docs/evidence/toy-wire-stream-79/`. None
of this is a console result.

| Check | What ran | Result |
|---|---|---|
| Whole reader | `tests/test_toy_wire_integration.c`: the real resident, low reader, GD service, auxiliary and stream on the card model, the scenario list twice (a receiver that ignores the bit order request and one that honours it) | Passed: 290,814,099 checks on the source as it stands with Test 79, which has the review's scenarios; 282,768,539 at the module's own commit. Every request completed with the image's exact bytes |
| Evolution 2's refills | `any_evolution()`: 73 and 50 sectors by turns | The first the low reader's, the second opens a window, four requests served from it, the one of 73 sectors in slices and done two events after its request; one CMD18, no CMD12. One call of the game hands over two slices, 32 sectors |
| A request of 200 sectors behind a small one | `any_long()`, three bus conditions, and two of them at the console's pace of about six blocks a visit | Opens a window and comes through it: in 15 events on a quiet bus with one stream; in 81 with 30 streams; in 200 with 327 streams; at the console's pace in 66 and in 107. The read behind it is served from the same window each time |
| A request that goes back | `any_returned()`: 150 sectors, the window failing after 40 of them; and the same with the card's stop command failing once | Back to the low reader whole, read again from its first sector, data exact; counted in `BACK`; the read behind it has a window |
| A large request the game waits for | `any_waited()`: 61 sectors, the game's own loop, interrupts held off | One window, kept while it grew; 20 rounds of the game's loop |
| A large request waited for in vain | `any_in_vain()`: 150 sectors at a full window, then no run lands | The window hands over the 74 sectors it has. A quarter second with no sector more: given up as one the game waited for, back to the low reader whole, what the game was told never going back; data exact |
| A read over the track's end that had sectors of its window | `any_track_end()`: 117 sectors, the channel the game's, so the low reader fills the window in the visit that opens it | Thirteen sectors handed over; the window given up at the request's next visit; back to the low reader whole; data exact |
| What a waiting request asks of the wire | `any_owed()`: 61 sectors at a window that holds 61 | Two slices by its own visit; nothing asked of the wire; the rest by one CHECK call, with no EXEC behind the request's own and no interrupt |
| A movie's start, and its second playing | `sequence_movie()` | The read of 61 sectors opens the window and comes through it, the refills from the same window; played again after its end, that read gives the window up and the first refill opens a second; 12 refills and the one large read served |
| Ordinary reads one behind the other | `sequence_plain()` | Seven of nine requests from two windows, 167 sectors (Test 77: six from three, 106): the read of 61 sectors a window was given up for is served from it in slices |
| The track's end | `sequence_track_end()`, the game's EXEC first and its CHECK first | A read that reaches into the next track is never the window's, at an open window's first sector or with a window opened for itself |
| A token search left standing | `sequence_pending()`, three ways of polling | No window on top of it; the low reader completes the read (63 to 70 rounds); the read behind it has its window |
| Everything else of Test 77's list | The scenarios of `docs/toy-wire-stream-test-77.md`, Verification | Passed. The checks of the course by piece are not made in this build, which does not keep it |
| The report | `tests/test_toy_wire_report.c` on the production formatter | Passed. 659 body bytes with nothing counted, 898 at a full run's values; every row fits, the first movie's player too |
| All configurations | Both host targets in Tests 68 to 77 with their variants and in Tests 78 and 79, each with GAP, with LATER, without STAY, QUICK, MSB, RING, CLEAN, and with no part of Test 76 (`toy-wire-stream-79/matrix.log`) | 109 of 112 jobs passed on `9f183a7`: both targets in 56 configurations. The three that failed are variants that are not delivered, and failed in the fixture, not in the reader; with the fixture corrected they pass (`d0c5bf1`, tests only; below) |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with `TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1` (`mutation-check.log`) | 226 of 226 caught, on the source as it stands with Test 79: the list of the commit less the ten entries of the course by piece (below), with one more for the pump. At the module's own commit the 34 entries of the change's own lines were run: all caught |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 226 of 226 for Test 78, 258 of 258 for Test 79 and 218 of 218 for Test 77 change the code that is compiled |
| As linked | `tests/test_toy_wire_cached_stack.py`, `test_toy_wire_linked_loops.py`, `test_toy_wire_linked_ring.py`, `test_toy_wire_linked_hot.py`, `test_toy_wire_linked_copy.py` on the module's auxiliary | Passed. The window's state is used at its cached address only: 22 constants in the linked code |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Against the baseline | Test 77 built with this module's label | Residents the same bytes. The worker and the auxiliary differ. The stage's own objects are the same; it differs where it carries those two images |
| Reproducible | The same recipe built twice from nothing, and once more from Test 79's source | Module SHA-256 `dbff8925...92de` each time |
| Earlier deliveries | Unchanged recipes on this source and on Test 79's, each against its recorded hash | Tests 66, 67, 68, 69, 70, 71, 72, 73, 74 and its second module, 75 and its second module, 76, its second module and its variant without GAP, and 77: all as recorded |
| Other suites | The Test 66 family's own fixture in every suite mode, and the suites that share the wire model (`toy-wire-stream-79/other-suites.log`) | The Test 66 family's fixture passes in every suite mode. 17 of the 18 other suites pass; `test-sci-stream-quarantine` stops on its fingerprint of a historical source as before (`docs/toy-wire-stream-test-77.md`, Known limits 7) |

Not run for this test: the stream test over 96 courses (the card
stream's source is unchanged since Test 76).

The last three scenarios, the runs at the console's pace and the two
lines corrected in the fixture's output came of the review (`63abdc5`,
tests only); they ran on the source as it stands with Test 79.

What the verification itself found and changed, before the commit:

- A window in error was dropped by the visit at the game's call entry
  and a new one opened in its place before the scenario for a pending
  token search could see the failed request: the scenario now marks the
  request's token as failed, which is what the reader does.
- One listed defect, a read over the track's end counted as one the
  game waited for, was not caught once the reason was reckoned from what
  a request is owed. The scenario gained a read over the track's end
  that opens a window for itself.

And after it:

- The list's first whole run, under Test 79, left ten entries uncaught.
  All ten are of the course by piece and the player's buffer behind it:
  lines this build still compiles but never reaches, because the course
  names no piece any more. They are marked as gone for Test 78 in the
  list (`23a23f5`, tests only). At this module's own commit the
  list still has them.
- The matrix failed in variants of this test that are not delivered,
  twice, and each time in the fixture: without STAY and without QUICK
  it asserted what only the delivered configuration does (`23a23f5`),
  and with no part of Test 76 it did not compile the review's scenario
  for what a request is owed (`d0c5bf1`). Corrected, those jobs pass.

## Review

One reader beside the author, on 16 November 2026, reviewed the
committed change (`1d0e5bab78ba`) and tried to break it in a private
copy with the fixture. No source was changed for it: the module is the
one of that commit. Its six findings, and what was done with each:

| # | Finding | How established | What was done |
|---|---|---|---|
| 1 | A large request the window has begun waits for the game's next EXEC even when the window holds every sector it lacks. On the low reader's movie path, where the game's CHECK does not visit the reader, it then never completes without one; Test 77 completes the same request, which is the low reader's there | Run in the host model: a request of 61 sectors inside a playing movie's file, CHECK only, stood at 32 of 61 for good | Not changed. Known limit 4. Toy Commander's player calls EXEC all the time, and no other title is on that path. Off it every call visits: one CHECK brings the rest (`any_owed()`) |
| 2 | The pump's new line, what a waiting request is still owed, was pinned by no test: reckoned by the whole request it passed the whole fixture | Run | A scenario and its mutation added (`63abdc5`) |
| 3 | The fixture did not run several ways a begun large request ends or is served, and none of its scenarios ran at the console's pace | Run privately; all passed | Adopted (`63abdc5`): a request waited for in vain after part of it; a read over the track's end that had been handed sectors; the request of 200 sectors at the console's pace. Not adopted: an abort of a partly served request, large requests on the movie path, a window that fails by its own pump, and the reviewer's random sequences (below) |
| 4 | Two lines the fixture printed said what Test 77 did | Run | Corrected (`63abdc5`): both print what was counted |
| 5 | `AHEAD`'s sector counts include the slices of requests that went back or were aborted, which the low reader then read again | Run | Not changed. Said under The report: with `BACK` not zero the sectors overstate what the windows spared the low reader |
| 6 | A request that goes back is written a second time below what CHECK has reported as transferred, also from the low reader's own interrupts | Read | Not changed. Known limit 5: the same bytes, so only a game that changed them in place before the request completed could tell. None is known |

What that reader examined and found sound, in short: the low reader
never runs on a cursor the window has advanced, because every way a
window ends under a live request begins the request again; every exit
of the fallback, with the card busy, in an interrupt visit and with the
stop command failing; an error the reader marks itself in the middle of
a request; the track's end inside a request, by three ways of polling;
an abort of a partly served request, and the same sectors or the rest
asked for next; the resident's side, where the count told to the game
never goes back and the request completes only when all of it is
delivered; that an interrupt visit publishes two slices at most; and
that a request of up to sixty sectors takes Test 77's path. Its random
sequences, 230 seeds on five set-ups: 32,467 requests completed with
exact data, 2,033 aborted, none failed, none hung; 1,393 large requests
served in slices, 3,224 returned.

What it could not settle, for the console: how often a large request's
own visit finds the stream at rest and leaves it so (finding 1 shows
then as one call more); what the interrupt visits' longer reading costs
a game that keeps running through a long read (Known limit 6); whether
any title changes in place what CHECK has reported.

The reviewer's private copy and its scenario files are not in the
tree. What was taken from them is in the fixture, under the commit
named above.

## Package

`KUI-Toy-Wire-Stream-Test-78-16NOV2026-1004-CST-1d0e5ba.zip`, 1,799,612 bytes, SHA-256
`a6fedccad6b0497c6e316cc055c41ebab039db44855359fdaf7a5c5ac077d445`, prepared 2026-11-16 10:04 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `1d0e5bab78ba` | `CAC19221` | `dbff8925...92de` | Changed: Test 78, replaces the Test 77 module at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged, identical to the Test 66 package |

Module SHA-256 in full: `dbff892578510f742e0b8b99c74fb456a2870ab6bcacccd0db1b91a8863092de`.

Retained on the card: everything else, including the Test 67 trace
module, Test 57, reports, saves and settings. No KUP, bank or user data
is included.

The report names the module by its title (`TEST 78`), its build
(`1d0e5bab78ba`) and `PIPE`'s last value (`3BD`).

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 BUILD_ID=1d0e5bab78ba build/toy-suite-78/retail-toy-test-78.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test \
  toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 \
  python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 78), CD Music and virtual VMU Off; the run of Console test. Confirm
`TEST 78` and `BUILD 1d0e5bab78ba` in the saved `/reports/toy/66-N.txt`, and
`PIPE`'s last value.
