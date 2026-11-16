# Review of Toy Test 78

An independent reader's review of commit `1d0e5bab78ba`, 16 November 2026. The
reader worked in a private copy outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened. `ran`: it built and ran something that shows the
finding (always the host fixture or a host test; nothing here ran on a
console). `read`: derived from the code. What was done with each finding is in
the test's document, under Review.

## Finding 1: A large request the window has begun waits for the game's next EXEC even when the window already holds every sector it lacks; with no further EXEC it never completes on the movie path, where Test 77 completes it

- Kind: hang-or-stall; established: ran
- Where: src/loader/toy_movie_ahead.inc: toy_ahead_publish (AHEAD_SLICE) and the tail of toy_movie_ahead (nothing follows the second publication); meets src/loader/retail_async.c kui_retail_async_call (CHECK only reports on the movie path) and kui_retail_native_demand_wake (returns at kui_sci_stream_busy())

Scenario:

Host fixture, movie playing, request inside its file past sector 386
(toy_suite_active=1). The window stands full (61 sectors) at the request's
first sector and the stream is at rest (KUI_WIRE_FULL, low stream state 2).
The game requests 61 or 100 sectors and then only calls CHECK (or one EXEC,
then CHECK). The request's own visit hands over 16 sectors, the pump's drain
ends on its budget with the ring refilled so the stream is still FULL, the
second publication hands over 16 more and frees window room, and the visit
returns. No run is in flight, so no SCI interrupt comes; the resident's wake
returns early because stream state 2 reads as a reception in flight; CHECK on
this path only calls report().

Consequence:

The request stays PROCESSING for good at 32/61 (29 owed, all 29 verified in
the window) or 32/100 and 64/100. The Test 77 build completes the same
requests with no EXEC at all, because there they are the low reader's. Limits,
so that this is weighed rightly: (a) host model only, where a run lands whole
at once; on the console a stream that was re-armed keeps interrupting, and the
stall needs the stream to stay at rest after the visit; (b) Test 77 already
depends on EXEC in the same way for a request of up to 60 sectors that is not
yet in the window (40 and 60 sectors: still PROCESSING in both builds); (c)
Toy Commander's player calls EXEC continuously, and off the movie path every
EXEC and CHECK visits, so there the cost is one more call per 32 sectors. What
is new in Test 78 is the state 'sectors verified in the window, request owed,
no visit pending', and that requests over 60 sectors on the movie path join
the class.

Evidence:

Private copy <scratch>/t79/review/revA, scenario tests/reva_nopoll.inc, built
with <scratch>/t79/review/revA-build.sh (the Makefile's own compile line) and
revA-build77.sh (the same without -DKUI_TOY_WIRE_ANY=1). 'REVN=1
build/rev/revn78' prints: '(window 61 sectors, pace 0)  no-EXEC, 61 sectors on
the movie path (build ANY=1): STILL PROCESSING after 200000 rounds, 0 wake
visits; cursor 32/61, told 65536 bytes, window 45046..45088 variant 4, stream
on, low state 2' and '... no-EXEC after the first, 100 sectors ... STILL
PROCESSING ... cursor 64/100'. 'REVN=1 build/rev/revn77' prints: 'no-EXEC, 61
sectors on the movie path (build ANY=0): COMPLETED after 280 rounds' and '...
100 sectors ... COMPLETED after 459 rounds'. Both builds print '(window 2
sectors, pace 70)  no-EXEC, 40 sectors on the movie path ...: STILL PROCESSING
... cursor 0/40'. Off the movie path the same shape shows as a wait, not a
hang: reva_silent prints 'its own visit 32, interrupts alone to 32 in 0
moments'.

Suggested:

In a visit of the game's own, hand over all the window holds of the request
(the 2 x 16 bound is needed for interrupt visits only; a foreground visit
already copied up to 60 sectors in Test 77). After the last publication serve
the stream once more, so that a stream standing still for room is re-armed and
its run's end brings the next visit. Add a no-EXEC scenario to the fixture so
the remaining dependence is visible.

## Finding 2: The pump's new line (what a request is still owed) is pinned by no test: reverting it to the whole request passes the whole-reader fixture

- Kind: test-gap; established: ran
- Where: src/loader/toy_wire_ahead.inc toy_ahead_pump ('const uint32_t owed=demand?s->count-r->engine.cursor.done:0u;'); tests/test_toy_wire_stream_mutations.py ANY_MUTATIONS

Scenario:

Change the line to 'const uint32_t owed=demand?s->count:0u;' (a large request
then always counts as waiting, whatever it has had), build the fixture with
the Test 78 flags and run all of it.

Consequence:

The fixture passes. The line decides w->want (where the stream cuts a run to
tell the reader) and whether an interrupt visit drains for 390 or 1172 ticks,
so a wrong value is a timing and interrupt-latency defect that nothing here
would show. The commit message names this change ('The pump reckons what a
waiting request still lacks from what it is owed') and says '34 mutations of
the new lines'; none of the 34 is on this line. The smaller host tests do not
compile the pump, so this fixture is its only host test.

Evidence:

Mutant tree <scratch>/t79/review/revA/m1 (line 925 changed), built with
revA-build-root.sh, run as build/rev/m1; log revA-m1.log ends 'PASS Test 78
reader integration: 282768579 checks' / 'exit=0'. Unmutated: 'PASS Test 78
reader integration: 282768539 checks' (revA-base78.log).

Suggested:

Add the mutation, and a scenario that tells the two apart: at reader_pace(70),
a large request whose remaining sectors are all in the window must leave
wire_state()->want at 0 and have an interrupt visit read at most the interrupt
budget's blocks; one that lacks n sectors must have want == tail + n*2499.

## Finding 3: The fixture does not run several of the ways a begun large request ends or is served; none of the any_* scenarios runs at the console's pace

- Kind: test-gap; established: ran
- Where: tests/test_toy_wire_integration.c: any_evolution, any_long, any_returned, any_waited, sequence_track_end, sequence_pending

Scenario:

Not in the fixture: (1) a begun request given up because STAY ran out, the
second cause the header names ('a wait that outlasts the stream'):
waited_in_vain and waited_slowly use 13 and 26 sectors, which are never begun;
(2) a large request over the track's end that has already been handed slices
(it opens its own window, the channel is taken so the low reader fills the
window in the opening visit): sequence_track_end uses 22, 36 and 47 sectors;
(3) the game aborting a partly served request, then asking for the same
sectors or for the rest; (4) a large request on the low reader's movie path,
including one of 61 to 64 sectors that goes back and is then staged by
toy_resume_deliver in the arena the window used; (5) a window that fails by
the reader's own pump (any_returned sets movie_ahead.error from outside); (6)
any large request at reader_pace(70); sequence_pending keeps its old subject
by setting failed_token by hand.

Consequence:

All of these pass in my private scenarios, so this is coverage and not a
defect: a later change to toy_ahead_fallback's callers, to STAY or to the
staging path could break them unseen. The most important one to add is (1)
together with (2): they are the two organic routes into toy_ahead_restart with
sectors already delivered.

Evidence:

<scratch>/t79/review/revA/tests/reva_scenarios.inc, run as 'REVA=n
build/rev/reva78': 'R7 a wait in vain after 74 of 150: status 2 in 18 rounds,
back 1' (drops 0/0/0/0/1/0); 'R2.2/0 117 sectors over the track's end: status
2 in 403 rounds, back 1' (13 sectors handed in the opening visit); 'R1.0 abort
after 40/54 sectors, then the same again: status 2'; 'R10 style 3 pace 0:
sector 40 of 64 unverifiable on the movie path: status 2 in 485 rounds; back 1
... suite 0/0/0/1'; 'R4 style 0 pace 70: sector 100 of 150 unverifiable:
status 2 in 48 rounds; back 1'; 'R8 style 1 pace 70 stalls 1500/20: 200
sectors: status 2 in 1902 rounds; big 1 back 0'. Every one with 0 failed
checks.

Suggested:

Add scenarios for (1) to (4) and run any_long once at reader_pace(70).

## Finding 4: Two lines the fixture prints are untrue in the Test 78 build

- Kind: misleading-report-or-message; established: ran
- Where: tests/test_toy_wire_integration.c: the printf at the end of sequence_plain and of sequence_movie

Scenario:

Run the fixture with TOY_WIRE_ANY=1 (the command in the task). sequence_plain
itself asserts w->other_windows == 2u and counts two windows given up (gone =
2), and the 61-sector read is served from the window; sequence_movie asserts
w->large_requests == 1u and hit_requests == 1u after the first 61-sector read.

Consequence:

The printed text has the count and the reasons of Test 77 hard-coded: it says
three windows and a window given up for the read of 61 sectors, where there
were two windows and that read was served; and it says the 61-sector read was
the low reader's and counts 13 refills, where the first one was served by the
window and 12 refills were served. These lines are what gets kept as the run's
record.

Evidence:

revA-base78.log, from 'make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66
... TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 BUILD=build/rev
toy-wire-integration-host-test': '  ordinary reads one behind the other: the
second opens a window with no card command; 7 requests of 9 served from 3
windows, 167 sectors; given up for a read elsewhere, for one of 61 sectors and
at the track's end; CMD18 3, CMD12 2' and '  a movie's start as the game reads
it: a read of 61 sectors by the low reader, the first refill a window with no
card command; played again after its end, a second window; 13 refills served'.

Suggested:

Print w->other_windows and the reasons from the counters; word the movie line
by KUI_TOY_WIRE_ANY (first play: served through the window; 12 refills and one
large read).

## Finding 5: AHEAD's sector counts (B, I, OS) now include slices of requests that went back or were aborted, which the low reader then read again

- Kind: minor; established: ran
- Where: src/loader/toy_movie_ahead.inc toy_ahead_publish ('w->other_sectors+=give' and the stage_result line, added per slice); comment in src/loader/toy_suite_report_save.inc ('AHEAD's sectors and HIT include them')

Scenario:

A request of 150 sectors is served 100 sectors by the window, the window
fails, the request goes back and the low reader delivers all 150. Or the game
aborts after 54 sectors.

Consequence:

OS is 100 (or 54) for a request that counts neither in HIT nor in BIG, and
whose sectors the low reader read in full. Up to Test 77 sectors were added
only with a request served to its end, so B + I + OS was exactly what the
windows spared the low reader; now it overstates that whenever BACK is not
zero, and the report gives BACK as a number of requests, not of sectors. The
other counters hold: large_requests, large_sectors and hit_requests are added
only at a request's end, and in 32,467 random requests none was both a hit and
a return.

Evidence:

'REVA=4 build/rev/reva78': '[R4 end] ... done 150/150 pending 0 status 2 ...
hit 0 big 0/0 back 1 os 100 ow 1 ... cmd18 10 cmd12 9'. 'REVA=1': '[R1 after
abort+check] ... done 54/150 ... hit 0 big 0/0 back 0 os 54'. The fixture
asserts the same on purpose: any_returned has CHECK(w->other_sectors == had
...).

Suggested:

Either say so in the report's comment and add the returned sectors as a fourth
BIG value, or take a returned request's slices back out in toy_ahead_restart.

## Finding 6: A request that goes back is written a second time below what CHECK has already reported as transferred, also from the low reader's own interrupts

- Kind: minor; established: read
- Where: src/loader/toy_movie_ahead.inc toy_ahead_restart; src/core/retail_gd.c check() (put32(out + 8, total)) and kui_retail_gd_progress

Scenario:

The window hands over had sectors; a CHECK tells the game completed_bytes =
had*2048; the window fails or the wait runs out; toy_ahead_restart begins the
cursor at sector 0 and the low reader delivers sectors 0..had-1 again, in the
game's calls and in its SCI interrupts between them.

Consequence:

This is the one place where bytes the game has been told are final are written
again (item 3 of the brief). They are the same bytes, so a game that only
reads them sees nothing; in 3,224 returned requests of the random runs every
sector told was the image's at the moment it was told and at the end. A game
that had changed the reported part in place before the request completed would
have its change undone. No such title is known to me; it is a property the low
reader alone never had, since it writes each byte once.

Evidence:

toy_ahead_restart:
'if(kui_retail_cursor_begin(&e->cursor,e->manifest,s->lba,s->count,
KUI_GAME_SECTOR_MODE1,write,context)!=KUI_GAME_OK)'; its comment: 'deliver
every sector from the first, over what the window wrote, which is the same
bytes'. retail_gd.c check(): 'uint32_t total=s->completed_bytes; ... put32(out
+ 8, total);'.

Suggested:

None needed unless a title is found that consumes in place. If one is, the low
reader could be given a cursor for the rest only, with the auxiliary writing
e->cursor.done back to the request's count before the resident reports.

## Examined and found sound

- Low reader on a sliced cursor (read, and every run): each call of ops->deliver
  lies behind toy_movie_ahead returning false, which implies A.variant==0; a
  window ends under a live request only in toy_ahead_fallback, whose two
  clearing exits both restart a begun request; MOVIE_COMPLETE and quiesce clear
  a window only when the request is cancelled, reset or absent.
- toy_ahead_fallback, window cleared at once: restart, low reader delivers all,
  data exact (R4, R7, R2.2, R10).
- toy_ahead_fallback in an interrupt visit that cannot end the card's command:
  stream halted, window and sliced cursor left standing, nothing published while
  A.error is set, the game's next call clears and restarts (R9; any_returned in
  the fixture).
- toy_ahead_fallback with the stop command failing: cleared all the same and
  restarted (fixture any_returned(true)); with a reception still under way the
  request fails with an I/O error as before Test 78 (read; I found no way to
  reach it with a begun request, the window's own card work being programmed
  transfers).
- Error marked by the reader itself mid-request (a sector whose address is wrong
  on the card, so the window never verifies it): eight rewinds, A.error,
  restart, data exact; on the movie path a request of up to 64 sectors is then
  staged and published by toy_resume_deliver, exact (R4, R10).
- Track's end inside the request: 'beyond' is the same value at every visit of a
  request (count+lba-end), so a window standing at the first sector gives up
  before any slice; a window the request opened for itself gives up at the next
  visit and restarts if slices were handed (R2, three ways, two polling styles).
- A request ending exactly at the track's end with the stream closed: served to
  its end from what the window holds (R5, 190 sectors).
- Abort of a partly served request: nothing is written to its buffer afterwards
  (2,033 aborts in the random runs, snapshot compared), start() wipes the sliced
  cursor, the same sectors asked again and the rest asked next are both exact
  (R1, R9, R14).
- A second request at the same LBA, and a request the low reader has partly
  delivered: selected needs A.variant && at==A.first when had>0, toy_ahead_begin
  refuses a cursor with done or offset set, and the staging guard (C.resume)
  stops a window opening over a staged read (read).
- Resident, src/loader/retail_async.c: report() only ever passes e.cursor.done
  to kui_retail_gd_progress, which never goes back and completes only at
  done==count, so slices and the restart to 0 are both safe; completed_bytes and
  position_lba keep their highest value through a restart; e.since stays below
  e.target through slices, which only makes wait>0 and so the STAY path;
  e.active stays 1 until the last slice; the token is untouched (read; the
  random runs check after every CHECK that what was told never goes back and
  that every sector told is the image's at that moment).
- Interrupt visits: at most two publications of at most 16 sectors and one drain
  bounded by 1172 ticks (4 blocks without a valid clock); nothing is written
  below what CHECK has reported except by the restart (finding 6); no card
  command from an interrupt visit.
- Requests of up to 60 sectors: with had==0 and give==count the new publish and
  the new conditions reduce to Test 77's (read; the fixture's older scenarios
  pass).
- Random sequences in the fixture (tests/reva_scenarios.inc, reva_fuzz): 230
  seeds on five set-ups (plain card, movie path with the player's polls,
  scattered 28-block image, waited loops with invalid clock and taken timers,
  damaged card blocks with late tokens): 32,467 requests completed with exact
  data, 2,033 aborted, 0 failed, 0 hung; 1,393 large requests served in slices
  (133,553 sectors), 3,224 returned.
- The unmodified fixture at HEAD with the Test 78 switches: 'PASS Test 78 reader
  integration: 282768539 checks'.
- Build plumbing (read): the residents' FLAGS are captured before any wire
  switch, and none of the files the change touches is included by a resident
  source, so the low reader's object cannot differ; code without the switch is
  kept under #else unchanged.

## Not settled

- Console: after the own visit of a large request that finds the stream fully at
  rest, the stream is re-armed only if that visit parsed about 16 blocks within
  its 1172 ticks (the ring must fall below 24,574 bytes). The measured pace
  (about six blocks in 390 ticks) gives 18, a thin margin. This decides how
  often finding 1 shows as a wait for the next EXEC on the console; I cannot
  measure it here.
- Console: while a large request is owed more than the window holds, every
  interrupt visit drains with the foreground budget (1172 ticks, about 1.5 ms
  with interrupts held) for the whole length of the request. The pump's comment
  rests on 'while the game waits for this request nothing else of it runs',
  which is true of the movie player's wait but not of a game that keeps running
  during a long read. Bounded per visit; its effect over a read of seconds is
  unmeasured.
- Whether any title of the family changes in place, before the request
  completes, sectors CHECK has reported as transferred (finding 6).
- I did not build for SH-4: the image size in the commit message (24,160 of
  24,576 bytes) and the resident's byte identity are not checked by me, only
  that the resident's sources and flags are untouched.
- I did not rerun the author's 34 mutations; I ran one of my own (finding 2).
- STAY needs the clock the pilot profile expects (clock_read's valid). Where it
  is not valid, a waited large request not wholly in the window is handed its
  first slices and then returned at its first waited CHECK, read twice. Toy
  Commander has the clock; this matters for Test 79's other titles and I did not
  pursue it.
