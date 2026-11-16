# Review of Toy Test 80

An independent reader's review of commit `e58dd2e`, 16 November 2026. The
reader worked on scratch exports outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened and a note on the housekeeping of its own scratch
files left out. Nothing here ran on a console: what it ran is the host model,
a host test or a linked build. What was done with each finding is in the
test's document, under Review.

## Finding 1

No delivered test has an SCI interrupt visit meet a command while a window
stands: a mutant that stops the stream there passes all 318,027,918 checks.

- Severity as the reader gave it: minor
- Where: tests/test_toy_wire_integration.c:2590-2600 (asked_command),
  :2629-2645 (sequence_commands), :3391-3467 (titles_asked, weak asserts at
  :3444 for the stalled buses); code under test
  src/loader/toy_movie_ahead.inc:717-719

Scenario:

On the console the stream's SCI error interrupt comes every few ms while a
window fills, so in Evolution 2 an interrupt visit will regularly find GETSCD
pending, or completed and not yet acknowledged. That visit is the one the
change exists for (in Test 79 it stopped the stream at once). In the fixture
it never happens: on a quiet bus the window is full before the command is
sent, and in the other-title scenarios (no timer, m.delay 0) all progress is
made inside the game's calls. A regression that gives the window up, or only
halts the stream, when the visit is an interrupt would ship unnoticed: the
window still counts as kept, but every such interrupt costs a CMD12, a CMD18
and a programmed block, which is the cost Test 80 is meant to remove.

Evidence:

Ran, on an export of e58dd2e. (1) Trap inserted behind line 718: `if(A.variant
&& s->command && s->command!=KUI_GD_PIOREAD && s->command!=KUI_GD_DMAREAD &&
e->in_irq)__builtin_trap();` -> toy-wire-integration-host-test still PASS,
318027918 checks (the unmodified count). (2) Mutant `... &&
(toy_ahead_foreign(s) || e->in_irq);` -> PASS, same count. Both are live: my
own scenario run against the trap build dies with SIGILL (exit 132), and
against the mutant shows CMD18 124 where the real code has 86. The author's 38
listed mutations are all caught (I ran them: 38 of 38), none is of this kind.
The real code is right on this path: my scenarios (m.delay=100000, both title
configurations, random reads/commands) had about 2,700 interrupt visits meet
one of the seven commands, pending and unacknowledged, with data exact, no
CMD12, window and stream kept, KEPT exact. Files:
<scratch>/review81/t80/mut/I.log, I2.log;
<scratch>/review81/t80/exp/extra3.inc, extra4.inc (x_irq_first).

Reached on a console:

Yes, constantly in the game the test is for (Evolution 2, and any title that
sends one of the seven while a stream runs). The delivered code behaves
correctly there in the host model; what is missing is the test.

Suggested:

Add a scenario with a run in flight (m.delay large): REQUEST one of the seven,
let hardware() deliver interrupts until `eris` moves, assert stream_on(),
card.cmd12 and w->n.starts unchanged, queries==1; again between EXEC and
CHECK; and the same with STOP (stream stopped in the interrupt visit, C
counted once by the next call). Add the mutant `toy_ahead_foreign(s) ||
e->in_irq` to QUERY_MUTATIONS. In titles_asked's stalled variants set m.delay
so interrupts do the work, as sequence_lent and window_gone already do.

## Finding 2

Which commands give a window up is pinned for STOP and SEEK only: INIT,
SET_MODE, PLAY, PLAY2, PAUSE and RELEASE can be moved to the kept side with
every test passing.

- Severity as the reader gave it: minor
- Where: src/loader/toy_movie_ahead.inc:611-615 (the table);
  tests/test_toy_wire_integration.c:2642, 2654, 2661 (STOP, SEEK), :3613-3619
  (number 95); tests/test_toy_wire_stream_mutations.py:819-824

Scenario:

The commit message and the comment at toy_movie_ahead.inc:599-606 claim SEEK,
STOP, SET_MODE, INIT, PLAY/PAUSE/RELEASE and unknown numbers give the window
up as before. The fixture sends only STOP and SEEK (and pokes 95). A wrong bit
for any of the other six in `asks` (a later edit, or an enum value that
changes) is not noticed. Today such a mistake would be harmless for data,
since none of those commands touches the card or the drive position either,
but the stated rule is not what the tests hold.

Evidence:

Ran: mutant with
`|UINT32_C(1)<<KUI_GD_COMMAND_INIT|UINT32_C(1)<<KUI_RETAIL_GD_SET_MODE|UINT32_C(1)<<KUI_RETAIL_GD_PLAY|UINT32_C(1)<<KUI_RETAIL_GD_PLAY2|UINT32_C(1)<<KUI_RETAIL_GD_PAUSE|UINT32_C(1)<<KUI_RETAIL_GD_RELEASE`
added to asks[0] -> toy-wire-integration-host-test PASS with the unmodified
count 318027918, i.e. none of the six is ever sent in any scenario
(<scratch>/review81/t80/mut/A.log). The list has the two additions that are
caught (STOP, SEEK) and none of these. Also unexercised: the bound itself
(only 95 is tried; 63 and 64 never).

Reached on a console:

The commands are reachable (the service accepts all of them,
src/core/retail_gd.c:282-291); a table error would be reachable too. No defect
in the delivered table: its two words as linked are 0x600C0000 and 0x00000114,
exactly the seven.

Suggested:

One table-driven scenario over every command request() accepts that is no read
(18,19,20,21,22,23,24,27,29,30,31,33,34,36,40): window open, send it, assert
kept or gone and the named bit; plus 63 and 64 by poke. Add one mutation per
give-up command.

## Finding 3

KEPT counts a command when a window first meets it, not when the window has
stood through it.

- Severity as the reader gave it: minor
- Where: src/loader/toy_movie_ahead.inc:622-625 (count), :709 and :719 (idle
  gives the window up later), src/loader/native_readahead_aux.c:1463-1470
  (ABORT/INIT end the window uncounted),
  src/loader/toy_suite_report_save.inc:125-132 (row text),
  src/core/retail_gd.c:453-456 and :622

Scenario:

(a) A command of the seven that fails: GETTOC or GETTOC2 for area 1 on a
CD-type image is accepted and completes FAILED/UNAVAILABLE. The EXEC's visit
counts it as stood through; the next visit sees a finished command whose
status is not COMPLETED and gives the window up as I (request state). Report:
KEPT 1, I 1, C 0. (b) A pending command of the seven is aborted, or the game
calls the INIT/RESET function: the window is ended by MOVIE_COMPLETE, counted
in no column of X, and KEPT keeps its 1 (Test 79 counted C for the same
sequence). (c) A window lost to an error while the command is current is
counted the same way. So KEPT can exceed the windows' true survivals, and OW
minus the drops no longer accounts for them.

Evidence:

Ran in the fixture (my scenarios x_failed_query and x_abort_init,
<scratch>/review81/t80/exp/extra1.inc, extra2.inc): (a) after
kui_retail_gd_set_disc_type(...,0x10,...) and GETTOC2 area 1: 'after EXEC ...
KEPT 1 ... status -1', 'CHECK -> -1 err 1 4', 'after CHECK variant 0 ... drops
E/N/I/C/W/O 0/0/1/0/0/0 ... cmd12 1'. (b) ABORT of a pending GETSCD: 'variant
0 stream off ... drops 0/0/0/0/0/0 | KEPT 1'; the same sequence built without
the switch: drops 0/0/0/1/0/0.

Reached on a console:

(a) only on a CD-type image whose game asks for the high-density table while a
window stands: rare. (b) any title that aborts a query or calls INIT/RESET
with one pending: rare. In Evolution 2's expected pattern (GETSCD answered and
acknowledged) the count is exact; my random runs had KEPT equal to the
commands met with a window open in every case.

Suggested:

Count at the end: keep the token as now, and add to `queries` in the visit
that finds that token acknowledged (command gone or a new token) with the
window still open; or take the count back where a window is given up or ended
while `s->token==k->query_token`. Otherwise call the value what it is (met
with a window open) in the row's comment and the test document.

## Finding 4

With a command that only asks pending, the report's lease is refused and the
stream now stays running: Start cannot restart the console (Test 79 had ended
the stream by then).

- Severity as the reader gave it: minor
- Where: src/loader/retail_async.c:1113-1120 (BUSY while service.pending),
  src/loader/native_readahead_aux.c:818-829 (quiesce refuses the same),
  src/loader/vmu_native_passive.c:1012-1017 (the gesture's wait), :1375-1381 and
  src/loader/native_return_hardware.c:92-95 (restart refuses while CHCR1 bit 0
  is set); behaviour changed by src/loader/toy_movie_ahead.inc:717-719

Scenario:

The gesture waits up to 60 looks for no command pending and no read active,
then asks for the card anyway. If one of the seven is pending at that moment
the resident answers BUSY, as it did before. In Test 79 the first visit that
saw the command (an interrupt, or any GD function call such as DRIVE) had
already given the window up, halted the stream and sent CMD12, so channel 1
was off and Start worked. In Test 80 the window stands and its stream runs on;
the worker never serves it again, CHCR1 keeps its enable bit after the run,
and every Start shows RESTART BUSY / TRY AGAIN OR POWER OFF. The pages
themselves are shown. This is Known limit 12 of
docs/toy-wire-stream-test-79.md widened from 'a game still reading' to 'a game
with a query pending'.

Evidence:

Ran the same scenario in both configurations (x_lease_pending_run,
<scratch>/review81/t80/exp/extra6.inc): window open, a run in flight, REQUEST
GETSCD, one DRIVE call, then kui_retail_native_async_loan_begin. Test 80:
'lease -> 1 (BUSY is 1); then: stream on, a run in flight 1, channel 1 ... ON,
cmd12 0, window open'. Test 79: 'lease -> 1; then: stream off, a run in flight
0, channel 1 ... off, cmd12 1, window gone'. With the command completed and
only unacknowledged the lease succeeds in Test 80 too (x_lease_completed:
lease 0, stream off, cmd12 1). That the worker's looks can all fall on a
pending query is inference: it needs a title that has one pending at each of
60 consecutive looks, which Evolution 2's pattern (a GETSCD beside its reads)
does not suggest.

Reached on a console:

Reachable from a Games-menu launch of a title that keeps one of the seven
pending across the gesture's second; unlikely in the two Evolution games.
Consequence is power-off instead of Start; no data or card damage (the card is
left inside CMD18, as in limit 12).

Suggested:

At least record it with limit 12 in the Test 80 document. To close it: let the
terminal report path halt the wire stream when the lease is refused
(toy_wire_halt sends no card command and may run with interrupts held), or let
quiesce() treat a pending command that is no read as no owner of the card, so
the loan's QUIESCE visit ends the window as it does when nothing is pending.

## Finding 5

VMAX's D, and D's count and sum, cover whole-path delivery visits only and
include the low reader's own blocking reads: not comparable with Test 79's
Evolution 2 report.

- Severity as the reader gave it: note
- Where: src/loader/native_readahead_aux.c:1520-1532 (quick path returns
  before the clock is read), :1671-1681 (D, deliver_longest),
  src/loader/toy_movie_ahead.inc:582-595 (toy_ahead_plain still excludes every
  command that is no read), src/loader/toy_suite_report_save.inc:174-181

Scenario:

deliver_longest is the longest DELIVER visit from a game call that took the
whole path. That time includes C.config.ops->deliver(wait) when no window
serves: the low reader waiting for up to 20 blocks, or re-reading a block from
a fresh CMD18. A D of a frame or more will most likely be that, which the
installed reader has too, and the row cannot say whether the window or the low
reader held the processor. Separately: in Test 79's Evolution 2 run nearly
every idle call took the whole path (no window stood), 32,512 visits; in Test
80 a window stands and idle calls take the quick path, which D does not count.
D's N and T will fall sharply for that reason alone. Visits while one of the
seven is current still take the whole path (toy_ahead_plain was not changed),
interrupt visits included: same result as the quick path, a little more work,
and counted in D.

Evidence:

Read. The quick path's `return 1u` at native_readahead_aux.c:1527-1529
precedes `struct ra_clock before=clock_read()` at :1571; ++w->deliver_calls is
at :1676. suite_demand ends in C.config.ops->deliver(wait) at :677 when
toy_movie_ahead returns false. Not run on a console.

Reached on a console:

Yes, in every run; it affects how the new row and the D row are read, not the
reader.

Suggested:

Keep two maxima (visit ended in the low reader's deliver / the window's own),
or say in the test document that D's maximum bounds the low reader's catch-up
as well, and that D N T must not be set against Test 79's figures for
Evolution 2. Optionally let toy_ahead_plain accept the seven commands so such
visits take the short way.

## Finding 6

Toy Commander's report has 3 bytes of room at the test's 'full run'; a real
run is wider than that sample in two rows.

- Severity as the reader gave it: note
- Where: src/loader/toy_suite_report_save.inc:28-44, :125-132, :174-181;
  tests/test_toy_wire_report.c (the sample values, 1009 of 1012 bytes)

Scenario:

Two rows were added to a text of 1,012 usable bytes. At the test's sample
values Toy Commander's 21 rows take 1,009. Rows are written whole or not at
all, so a wider run drops BP first (by design), then whatever no longer fits.
The sample is not an upper bound: Test 78's console report 66-12 is wider in
PIPE (calls 14899 against 9000) and D N T SEEK, narrower elsewhere; in Test
80's layout that run would take about 972 bytes (my estimate from the row
widths, not run through the formatter). A session several times longer loses
BP.

Evidence:

Ran build/host80/test-toy-wire-report with an output file and compared row
widths with docs/evidence/toy-wire-stream-78/console-report-66-12.txt: 'PIPE
... real 21 sample 20 REAL WIDER', 'D N T SEEK real 19 sample 18 REAL WIDER';
sample padding before END REPORT is 3 spaces. For another title: 16 rows, 36
lines, three pages, and no value row can pass 50 places at plausible values
(six values would need to average more than 7.5 hex digits).

Reached on a console:

Only in a long Toy Commander run; the row lost first is the one the code names
for it.

Suggested:

None needed for Test 80. Any further row for Toy Commander needs room made
first; say so beside the test's 1,009.

## Finding 7

Small inaccuracies in the change's own text.

- Severity as the reader gave it: note
- Where: src/loader/toy_movie_ahead.inc:604-606, :37-42 and :860-862

Scenario:

The comment says 'SET_MODE and INIT set what a sector is'. SET_MODE stores the
four mode words and nothing else (src/core/retail_gd.c:612-615); what a sector
is is set by the INIT command and by the DATATYPE function, which is no
command and never gave a window up by itself (the next read's own test does:
2,352-byte sectors at a kept window's first sector give it up as O and the low
reader reads them exact, which I ran). AHEAD_QUERY is defined beside AHEAD_ANY
and AHEAD_SEQ but not undefined with them at the file's end.

Evidence:

Read; DATATYPE with a window standing through a query run as x_datatype
(refused while the query is pending, accepted once completed, raw read exact,
window reopened by the next consecutive 2,048-byte read).

Reached on a console:

No effect on behaviour.

Suggested:

Reword the comment (SET_MODE is given up for out of caution); #undef
AHEAD_QUERY with the others.

## Examined and found sound

- Service side of the seven commands (src/core/retail_gd.c:245-450, 524-626):
  none of GETTOC, GETTOC2, NOP, REQ_MODE, GETSCD, REQ_STAT, GET_VERS changes
  position_lba, drive_status, the sector mode or executing beyond its own call;
  they write only the game's buffers; a query cannot be accepted while any
  command is owned (line 252), so never while a read is pending or
  unacknowledged.
- s->lba and s->count as the query leaves them: every use in toy_movie_ahead,
  toy_ahead_publish, toy_ahead_fallback/restart, suite_raw, suite_file,
  toy_ahead_pump and toy_wire_stay is behind demand (e->active && s->pending) or
  selected, and e->active is always 0 while a query is current
  (retail_async.c:1610-1611 clears it in the query's own REQUEST); the first
  read behind it overwrites both and is matched by A.first, not by the drive
  position.
- A kept window cannot publish for a request that is no read or serve from a
  wrong sector: confirmed by run, 20+ random games (reads of 1 to 150 sectors
  consecutive and elsewhere, the seven commands with random
  EXEC/CHECK/DRIVE/interrupt interleavings, STOP, SEEK; quiet and four cut
  buses; Toy Commander's disc and another title without a timer; both bit
  orders): every read exact, no window lost during a query, no drop counted for
  one, position unchanged, port never faulted.
- Low resident across a query (retail_async.c:1353-1412, 1545-1638,
  1639-1688): nothing ends or touches a stream the auxiliary owns; unhook() runs
  only when stream.state is not DMA, which toy_wire_take_over sets and
  toy_wire_halt clears; the fixture's invariant (stream on implies state 2,
  hooked, SCI level 1) held after every step of my scenarios, with the stream
  running, waiting without a timer, and standing full.
- The low reader never runs over a live stream during a query: every path on
  which toy_movie_ahead returns false has halted the stream first; the quick
  path and the loan path return 1.
- Interrupt visits meeting a query (pending and unacknowledged), about 2,700
  of them in my runs with m.delay=100000: window and stream kept, no CMD12, KEPT
  exact; STOP met first by an interrupt visit: stream stopped there, CMD12 and C
  once at the next call.
- A query asked while a read of 150 sectors is served in slices: refused six
  times of six, read complete and exact, no fault path taken (service.command is
  set, so the resident's REQUEST REJECTED screen is not reached).
- INIT function and ABORT with a query pending and a window open: window
  ended, stream stopped, CMD12, next consecutive reads get a window again;
  DATATYPE refused while pending.
- Verified Toy Commander with its movie playing and queries between refills
  (player's polls, an unknown caller, left unacknowledged over update calls):
  window variant 4 kept, six refills served, no drop.
- toy_ahead_foreign as linked: bound cmp/hi #63, bit by and #31, word by shift
  -5; table words 0x600C0000 and 0x00000114; its literal (file offset 0x2630 ->
  0x8c426f0c) is in the relocation table; .rodata is inside the image and the
  tool refuses unannotated addresses.
- Counting by token: each query with a window open is counted once whichever
  visit sees it first; none without a window (titles_asked's 4 of 5 and my
  runs); a token cannot recur short of 2^31 requests; the report reset keeps
  query_token (at most one command uncounted).
- visit_longest and deliver_longest are maxima of the same quantities their
  sums add (toy_wire_ahead.inc:261-269, native_readahead_aux.c:1674-1681), on a
  clock accepted only when free-running, cleared with the report, no overflow in
  a visit.
- Report: another title has 16 rows, 36 lines, three pages; rows left out only
  by owner_verified, which is set once at init from toy_poll_owner and never
  cleared, the same value TOY prints; END REPORT is written at its fixed place
  whatever was left out.
- Byte identity without the switch: every source change is inside #if
  KUI_TOY_WIRE_QUERY/AHEAD_QUERY or folds to the same constant; built Test 79's
  parts from b673e26 and from e58dd2e with the same label: auxiliary, native
  worker, both residents, stage, worker identical. Test 80's residents are
  identical to Test 79's.
- Sizes claimed: auxiliary 24,352 bytes (Test 79: 24,192), 224 bytes of code
  room left; linked checks that need no reference pass on the Test 80 auxiliary
  (cached stack, ring, hot state 22 constants, copy).
- The author's mutation list for this change: 38 of 38 caught when run on
  e58dd2e; Makefile selects Test 80 before 79 and refuses QUERY without TITLES.
- HEAD with TOY_WIRE_TIMER unset builds the same Test 80 parts as e58dd2e (all
  six part binaries identical), so the host tests run at HEAD speak for the
  subject; the module is admitted by the package test.

## What the reader ran

- HEAD (494a6e9), Test 80 switches, BUILD=build/review-t80:
  toy-wire-host-tests PASS; toy-wire-integration-host-test PASS (318027918
  checks)
- Export of e58dd2e, Test 80 switches: both targets PASS (318027918 checks);
  Test 79 switches: both PASS (307407911, the recorded figure); Test 78
  switches: both PASS (290814099)
- Export of 00d13e1 alone, Test 80 switches: both targets PASS (318027694
  checks)
- Variants of Test 80 on e58dd2e (LATER=1, STAY=0, QUICK=0, GAP=1): both
  targets PASS in each
- tests/test_toy_wire_stream_mutations.py on e58dd2e, TOY_WIRE_ONLY=257:295
  (the QUERY entries), TMPDIR in scratch: baseline passes, 38 of 38 caught
- My mutants on copies of e58dd2e, integration target: six give-up commands
  added to asks -> PASS (survives); `toy_ahead_foreign(s) || e->in_irq` -> PASS
  (survives); trap on an interrupt visit meeting a non-read command with a
  window open -> PASS (never reached)
- My scenarios built from the fixture with the Makefile's flags (scratch
  exp/): failed GETTOC, long-pending query, track-end window, lease with a query
  pending (Test 80 and Test 79 builds) and unacknowledged, Toy Commander movie
  with queries, query during a sliced read, ABORT/INIT with a query pending,
  DATATYPE, interrupt-first GETSCD and STOP, random games in 30 configurations:
  all assertions hold; results quoted in the findings
- sh-elf builds from the e58dd2e and b673e26 exports: Test 79 parts compared
  (identical), Test 80 auxiliary, worker, residents, stage built; objdump/nm of
  the auxiliary; relocation table checked for the table's literal
- HEAD: make ... BUILD=build/review-t80/module toy-wire-package-test with
  BUILD_ID=9f183a7f72f5 -> module built, 'admitted by Card Tools' module checks,
  report seed bound'; parts identical to the e58dd2e export's
- Linked checks on the Test 80 auxiliary: test_toy_wire_cached_stack.py,
  test_toy_wire_linked_ring.py, test_toy_wire_linked_hot.py,
  test_toy_wire_linked_copy.py PASS

## Not examined

- Nothing ran on a console or emulator: how the stream fares on a real bus
  while it now runs through Evolution 2's commands, which commands that game
  really sends, and whether windows then stand, are for the console run.
- Where the worker's looks fall relative to a game's REQUEST and EXEC (the
  likelihood of the lease finding) could not be determined from the source.
- tests/test_toy_wire_linked_loops.py (needs a Test 73 reference ELF) and the
  check that every mutation entry changes compiled code were not run; of the
  295-entry list only the 38 entries of this change were run.
- 00d13e1's claimed auxiliary size (24,320) and Test 78's module identity were
  not rebuilt; Test 78's host tests pass and Test 79's parts are identical.
- The Toy Commander report at real Test 80 values was estimated from row
  widths, not produced by the formatter.
- Test 81 (494a6e9) is not reviewed here beyond confirming that without its
  switch it builds Test 80's parts.
