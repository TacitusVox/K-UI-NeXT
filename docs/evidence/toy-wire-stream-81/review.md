# Review of Toy Test 81

An independent reader's review of commit `494a6e9`, 16 November 2026. The
reader worked on scratch exports outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened and a note on the housekeeping of its own scratch
files left out. Nothing here ran on a console: what it ran is the host model,
a host test or a linked build. What was done with each finding is in the
test's document, under Review.

## Finding 1

A title that uses the spare timer as a counter or stopwatch (no SDK-table
entry) is rewritten, armed and stopped by the stream: the guard knows a taking
only by the table entry, and by TSTR at arming alone.

- Severity as the reader gave it: major
- Where: src/loader/toy_wire_stream.c:494-510 (selection asks TSTR bit, UNIE,
  IPRA nibble, entry; not TCR's other bits, TCOR, TCNT), :631-635 (start writes
  TCR at once), :175-187 (timer_cancel / timer_arm), :391 (the only place that
  asks TSTR), :419-423 (kui_wire_timer: no check), :465-473 (wait branch: entry
  only, and no clock fallback while mode is set), :649-654 (stop: entry only;
  TCR/TCOR/TCNT never given back); src/loader/toy_wire_stream.h:224-227 ("give
  every borrowed register back")

Scenario:

A title of the family uses TMU1 or TMU2 without a handler in the SDK table (a
plain counter: no underflow interrupt). Four orders, each reproduced on the
unchanged production source in the repository's own register model. (a) The
timer is set up but stopped when a stream begins (TCR Pphi/1024, its own
TCOR/TCNT): it is taken as 'one the title has not touched'; kui_wire_start
writes TCR 0x0002 at once, the first wait writes TCOR 0xffffffff and TCNT, and
the stop puts none of the three back. (b) The title starts it while a stream
receives, and the stream is stopped before its next short cut (a command gives
the window up, a read elsewhere, a rewind, a loan): kui_wire_stop clears the
title's TSTR bit and writes TCR 0x0002. (c) The title starts and stops it
between two short cuts and means to read the count: the stream's next short
cut arms it (TCNT 0x40, TCR 0x0022, running, level 1). (d) The title restarts
it for itself under an armed wait: the stream's interrupt never comes, and a
wait with a timer has no clock fallback: it stood through 200,000 looks and
2x10^8 ticks of TCNT0, kui_wire_overdue false; it ends only when the window is
given up (toy_wire_stay's quarter second if a request waits), and that stop
then stops the title's counter.

Evidence:

Ran. Stream level: R1 (b) TSTR 05->01, TCR2 0004->0002; R3 (a) taken as mode
2, TCR2 0004->0002, TCOR2 00010000->ffffffff, TCNT2 00004321->00000040,
nothing back after stop; R7 (c); R4 (d) --
<scratch>/review81/t81/exp/stream-scenarios.log (source exp_toy_wire_stream.c,
made by make_stream_exp.py from tests/test_toy_wire_stream.c). Whole reader
(real resident, GD service, auxiliary, stream): W1 = (b) through the game's GD
STOP command, W2 = (a) -- .../exp/whole-reader-scenarios.log. The repository's
tests do not make these orders: timer_claimed(1)
(tests/test_toy_wire_stream.c:328-386) starts the title's timer only outside a
wait, and its course meets a short cut before any stop (I ran that course over
400 seeds: never a stop first, so it is not luck, the order is simply absent);
timer_titles compares TCR by its UNIE bit alone (:430, :751). The title's side
is inference: no executable is here. By reading only: if the title's counter
underflows under the stream's wait, the wait branch calls kui_wire_timer,
which stops it.

Reached on a console:

In any title where the stream gets a timer and the title uses that TMU channel
as a counter without an SDK-table handler. Conditional on the title
(inference). Not Toy Commander by its console history (TMU1 was the stream's
through Tests 67-78). Nothing in the repository says whether either Evolution
game touches TMU1/TMU2; Test 81's own run answers only for the moments a
stream begins.

Suggested:

Let the stream know whether it has the timer running (set in timer_arm,
cleared in timer_cancel) and put one check before every write to the timer
(arming, kui_wire_timer, wait branch, stop, fault): the entry is the stream's,
and the registers are as the stream left them (not armed: TSTR bit clear,
TCR/TCOR/TCNT unchanged since the stream's last write; armed: TSTR bit set,
TCR without UNF 0x0022, TCOR 0xffffffff); otherwise drop without writing. Save
TCR/TCOR/TCNT in kui_wire_start and put them back in kui_wire_stop while the
timer is still the stream's. Give a wait with a timer the clock as a backstop
(for instance twice its time by TCNT0). Add the four orders to
tests/test_toy_wire_stream.c and compare all of TCR, TCOR and TCNT there.

## Finding 2

When the guard gives the timer up it can leave the stream's own armed timer
running with its interrupt on and nobody to answer it; and the live vector
base is not asked again after the stream's start.

- Severity as the reader gave it: minor
- Where: src/loader/toy_wire_stream.c:465-468 (wait branch only sets mode 0),
  :649-654 (stop skips timer_cancel and the priority when the entry is not the
  stream's), :391 and :496 (vector base asked in timer_select only);
  src/loader/toy_wire_ahead.inc:955 (hook()'s answer not acted on while a stream
  runs)

Scenario:

(a) A wait stands armed (TSTR bit set, TCR 0x0022, level 1) and the timer's
entry stops being the stream's callback without a handler of the title's that
answers the timer: for instance the SDK's table set up anew, whose initializer
writes the default into every entry 0x040..0x800. The next look sets mode 0
('not the stream's to answer or to stop') and kui_wire_stop then does nothing
either. Left behind: the timer running, underflow interrupt on, level 1, the
SDK default as its handler. Up to Test 80 the stop cancelled the timer and
cleared its level whatever the entry held, and the wait branch answered an
underflow whatever the entry held. (b) The title moves VBR after the stream
began: the guard still reads the old table's entry (still the stream's) and
TSTR (clear) and arms; the interrupt is vectored through whatever the new base
leads to. The resident sees the lost route (sdk_sci_ready false, hook() drops
the SCI's level) but the stream is neither stopped nor told. Also: after a
drop for a timer the title started (the repository's own timer_claimed(1)) the
stream's callback and level 1 stay in the title's table and IPRA for the rest
of the session; the test asserts that as intended
(tests/test_toy_wire_stream.c:383). I could construct no harm from it, but
that channel is never the stream's again.

Evidence:

Ran: R5 and R6 in <scratch>/review81/t81/exp/stream-scenarios.log (R5 after
the look and after kui_wire_stop: TSTR 05, TCR2 0022, IPRA 0010, entry
8c1add0c). What the SDK default does with an event nobody registered is the
repository's own record, not mine:
docs/evidence/native-sdk-sci-route-2026-10-24.json:124-137 (handled events
0x300/0x340/0x380/0x3c0 only; every other returns without acknowledgement;
'enabled pending SCI can retrigger after SDK RTE ... starving mainline',
marked there as source-derived inference, not measured) and
docs/vmu-native-sdk-sci-route-test.md:17-22. That any title re-initialises the
table or moves VBR during play is inference; none is known. Same class as
Known limit 6 of docs/toy-wire-stream-test-79.md for the SCI's own interrupt.

Reached on a console:

Only in a title that rewrites the SDK table or moves VBR while a wait stands
armed (at most 1.3 ms each); if it happens, a level-1 interrupt that is never
acknowledged, i.e. a hang by the repository's own analysis. Not expected in
Toy Commander or the Evolution games; unverified.

Suggested:

On a drop, if the timer is still exactly as the stream armed it (TSTR bit, TCR
without UNF 0x0022, TCOR 0xffffffff), cancel it and clear its level before
letting go, in the wait branch and in kui_wire_stop; when the entry is back at
timer_default treat the timer as the stream's own. Keep the admitted vector
base in the stream and ask vector_base() == it in the guard at arming and at a
wait (or halt the stream when hook() leaves release.armed != 0x10).

## Finding 3

wire_fault stops the timer without the guard.

- Severity as the reader gave it: minor
- Where: src/loader/toy_wire_stream.c:189-194 (if(w->mode) timer_cancel(w))

Scenario:

The title has taken the timer wholly since the stream's last short cut (its
handler in the entry, its own period, underflow interrupt, level 5, running):
the case the guard was written for. The stream faults before its next arming
(any of E_HANDOFF, E_FOREIGN, E_END, E_STOP, E_ARM, E_HOLES): wire_fault
clears the title's TSTR bit and writes TCR 0x0002, interrupt off. The stop
that follows finds the entry the title's and leaves everything as it is (by
reading), so the title's timer stays dead.

Evidence:

Ran: R2 in <scratch>/review81/t81/exp/stream-scenarios.log (fault 16; TSTR
05->01, TCR2 0024->0002, entry still 8c123456). Rare: the console reports have
no stream fault (Toy Commander Test 78 'S ... F' 0; both Evolution runs
'stream errors' 0).

Reached on a console:

A title that takes the timer while a stream runs, and a stream fault before
the next short cut. Unlikely; conditional on the title (inference).

Suggested:

The same check as the guard before timer_cancel in wire_fault; better one
helper used by every place that writes the timer.

## Finding 4

The tests do not bind the stream to the resident's vector base, and are
indifferent to what the stream does to a title's timer in the orders above.

- Severity as the reader gave it: minor
- Where: tests/test_toy_wire_integration.c:128, :156, :165 (table fixed at
  0x8c00f600, kui_wire_test_vbr a constant), :3318-3323;
  tests/test_toy_wire_stream.c:430, :751, :328-386;
  tests/test_toy_wire_stream_mutations.py:793-844

Scenario:

(a) With toy_wire_ahead.inc:496 handing UINT32_C(0x8c00f400) to the stream
instead of r->vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX], both host targets pass,
the integration test with the same 323,813,906 checks. In the whole reader
'another title' has Toy Commander's vector base and only another default; a
structurally discovered title whose table lies elsewhere is covered at
kui_wire_start's own interface and nowhere through the glue, which is the seam
this change is about. (b) Three variants that change what the stream does to a
title's timer (the stop asks TSTR too; the fault asks the entry; a dropped
timer still as armed is cancelled) pass the stream test's ten courses
unchanged. (c) 'the title's entries, timers and priorities as they were'
compares TCR by its UNIE bit alone and never TCOR or TCNT. (d) The list's 21
entries are all caught (I ran them), but it has no entry for the new
selection's three hardware conditions or for the stop's timer_cancel; the
tests do catch those (four such mutants of mine: caught), the list just does
not say so.

Evidence:

Ran: <scratch>/review81/t81/exp/mut-vbr-integration.log and mut-vbr-host.log
(mutant applied to a scratch export of 494a6e9, then restored);
.../exp/extra-mutants.log (stream_mutants.py: 4 caught, 3 pass);
.../exp/timer-mutations.log (TOY_WIRE_ONLY=295:316: 21 of 21 caught).

Reached on a console:

host-only (test coverage)

Suggested:

Let the integration fixture's kui_wire_test_vbr() return the resident's own
hw.vbr and key the table model to
R.vector600[KUI_NATIVE_SDK_SCI_VBR_INDEX]+0x200, with one other-title scenario
at another base; add 'a constant vector base handed over' to the list; add the
orders of the first three findings to the stream test and compare TCR, TCOR
and TCNT whole; list the selection's three conditions and the stop's cancel.

## Finding 5

The report cannot tell a timer the guard dropped.

- Severity as the reader gave it: note
- Where: src/loader/toy_wire_ahead.inc:512 (timer_mode written at take-over
  only); row 'W BYTES WTICK TMR ERR DET' in src/loader/toy_suite_report_save.inc

Scenario:

In a title where the guard drops the timer now and then, every new stream
selects again when the channel looks free, so the report says TMR 1 or 2 while
many waits ended by the clock at the game's next call. The planned reading
(docs/fmv-stream-reader-plan.md:336: 'TMR 1 or 2, a wait under 1.3 ms') then
shows a timer and long waits with no row that says why. The timer's visits
against WAIT is no clean measure: Toy Commander's own Test 78 run has 0x1A6F
timer visits for 0x24A5 waits with no drop at all.

Evidence:

Read: no counter of drops exists;
docs/evidence/toy-wire-stream-78/console-report-66-12.txt rows V and W.
Inference as to any title that would do it.

Reached on a console:

Only where a title makes the guard drop; it affects the reading of the result,
not the game.

Suggested:

Count drops (by entry, by a running timer) in the report if a row has room, or
name it under Known limits of Test 81 so that 'TMR set, waits long' is read as
a possible drop.

## Examined and found sound

- The two words at every stream's start: the stage writes plan.previous and
  plan.vbr only after admission (exact owner: live VBR must be the owner's and
  both SCI entries must hold the owner's default window; structural: the live
  VBR and the initializer's literal found live in both SCI entries;
  vmu_native_sdk_sci_route_stage.inc:115-131 requires plan.vbr == live VBR), the
  auxiliary is bound only with the cookie present
  (vmu_native_readahead_stage.inc:83), and toy_wire_take_over calls hook() and
  requires armed == 0x10, which hook() sets only while sdk_sci_ready() holds at
  that instant (cookie, live VBR == the word, both SCI entries the resident's).
- Table layout: the dispatch formula VBR+0x200+((event-0x200)>>3) and the
  initializer's fill of every entry 0x040..0x800 with the default are recorded
  for the exact owners
  (docs/evidence/native-sdk-sci-route-2026-10-24.json:98-112;
  tests/test_native_sdk_sci_owner_contract.py contract_cases) and the same
  initializer fingerprint is required of every discovered family
  (native_sdk_sci_family.inc family_init), which gives +0x244 and +0x248; the
  linked auxiliary carries 0x0244/0x0248 and the right TSTR, IPRA, TCOR1/2,
  TCR1/2 addresses (literal pool of kui_wire_start).
- An entry that does not hold the default means no timer and nothing else:
  timer_select leaves mode 0 and every write to entry, TCR, TSTR and IPRA is
  behind w->mode; the waits are then Test 79's.
- Callback convention: kui_toy_wire_timer_callback is a tail jump to the visit
  gate with no stack use; the gate masks BL and IMASK 15, refuses re-entry,
  saves r8-r12 and PR on the caller's stack, runs on the private stack and
  restores SR; the auxiliary is built -m4-single
  -mfixed-range=fr0-fr15,fpul-fpul -mdiv=call-div1 and its linked image has no
  FPU, GBR or bank instruction (the apparent ones are literal-pool words), so
  either dispatch common serves it; it clobbers r0-r7, T and MACH/MACL exactly
  as the resident's own SCI callback path already does.
- Nesting: this build's GD hooks run with BL and IMASK 15
  (retail_resident.S:166-178, .Lirq_mask 0x100000f0), SDK callbacks run at IMASK
  15, the timer and the SCI are both level 1, and the gate has its own busy
  flag; a loan or quiesce halts the stream (kui_wire_stop) before the card is
  lent, so a timer visit cannot meet a GD call, an SCI visit or the worker
  inside the reader.
- The timer visit in another title assumes nothing of Toy Commander:
  toy_wire_timer_visit reads k->on, the clock, kui_wire_timer and the visit's
  counters; toy_wire_course returns -1 in this build; no toy_present, movie or
  hook state, no request state, no card command.
- The hardware sequence when nobody interferes: stop, reload, count, TCR
  0x0022, start; cancel is stop, TCR 0x0002 (which clears UNF) and a read back;
  the start publishes the entry before the level and the stop cancels before it
  clears the level and restores the entry, all inside the masked visit, so no
  underflow can reach the default on these paths.
- Toy Commander: with the entry the stream's and the timer stopped at each
  arming the register writes are Test 80's in the same order, and the two words
  are the old constants by the console's own record (TMR 1 in Tests 67-78 means
  the old code found live VBR 0x8c00f400 and 0x8c0be408 in TMU1's entry, which
  the one initializer literal also put in the SCI entries); the one new path is
  the guard's drop, which Toy Commander can reach only if TSTR's bit is found
  set at an arming, where Test 80 simply armed again.
- Without the switch nothing is compiled differently: the modules of Tests 78,
  79 and 80 built from e58dd2e and from 494a6e9 with one BUILD_ID are
  byte-identical (Test 80 SHA-256 34559527...2614), and at 494a6e9 Test 81
  against Test 80 has the same residents (15,848 and 15,120 bytes), the
  auxiliary 24,352 -> 24,448 bytes as the commit says.
- Stack: the auxiliary's own deepest path grows from 520 to 528 bytes of the 4
  KiB private stack, and the timer callback uses none of it.

## What the reader ran

- make -f Makefile.vmu-native-toy-suite <Test 81 switches>
  BUILD=build/review-t81 toy-wire-host-tests: passed (card stream, report,
  titles stage, msf, mode1 erasure)
- make -f Makefile.vmu-native-toy-suite <Test 81 switches>
  BUILD=build/review-t81 toy-wire-integration-host-test: PASS Test 81 reader
  integration, 323,813,906 checks
- The same two targets with Test 80's switches on 494a6e9
  (BUILD=build/review-t81/t80host): passed, integration 318,027,918 checks
- Target build of the auxiliary (BUILD=build/review-t81/native-ra-aux.elf) and
  disassembly: timer callback, kui_wire_timer, kui_wire_start's selection and
  literal pool read; instruction tally for FPU/GBR/MAC use
- Scratch exports of e58dd2e and 494a6e9 (git archive, under
  <scratch>/review81/t81/base and head): retail-toy-test-78/79/80.kui with
  BUILD_ID=e58dd2e00000 from both: identical; retail-toy-test-81.kui from head:
  residents identical to Test 80's, auxiliary 24,448 bytes
- toy-wire-linked-check and toy-wire-package-test on the scratch Test 81
  build: all passed, module admitted by Card Tools' checks
- TMPDIR=<scratch> TOY_WIRE_BASELINE=0 TOY_WIRE_ONLY=295:316 <Test 81 env>
  python3 tests/test_toy_wire_stream_mutations.py: 21 of 21 mutations caught
- Reviewer's stream scenarios R1-R7 on the unchanged
  src/loader/toy_wire_stream.c (exp/exp-stream review): each shows the behaviour
  named in the findings; log exp/stream-scenarios.log
- Reviewer's whole-reader scenarios W1, W2 (exp/exp-integration with
  REVIEW=1): the title's counter stopped through a GD STOP command, the title's
  prescaler rewritten; log exp/whole-reader-scenarios.log
- Reviewer's mutant 'Toy Commander's vector base constant handed to the
  stream' on the scratch export: both host targets pass (integration 323,813,906
  checks); logs exp/mut-vbr-*.log
- Reviewer's seven extra stream mutants (exp/stream_mutants.py): four defects
  not on the list are caught, three behaviour changes pass; log
  exp/extra-mutants.log
- The repository's own course 'the title starts the timer' over 400 seeds
  (exp/exp-seed seeds): no seed lets a stop come before the guard, so the course
  does not exercise that order
- git status in /home/user/K-UI-NeXT after all of it: clean, HEAD 494a6e9

## Not examined

- What the SDK's two dispatch commons save (r0-r7, MACH/MACL, the FPU frame):
  the repository holds fingerprints, not the code. The timer callback rests on
  the same assumption as the resident's SCI callback, whose linked audit
  (tools/check_vmu_native_sdk_sci_route.py) also leaves MACH/MACL unasserted.
  Which common Toy Commander selects against the Evolution games could not be
  read either; the callback is integer-only in both.
- Whether either Evolution game, or any other title, uses TMU1/TMU2, rewrites
  TSTR or IPRA during play, sets the SDK table up again or moves VBR: no
  executable is here, so the title's side of every finding is inference.
- Which channel will be free in another title: Toy Commander's runs took TMU1
  because TMU2 was not free, and the repository does not say why (my guess,
  unverified: the launcher's KallistiOS leaves TCR2 with its interrupt bit set).
- Lost updates when the game's own read-modify-write of TSTR or IPRA is split
  by a reader visit (the stream's bit or level put back as it was before the
  visit): reasoned only; it would cost the stream its timer or hold one wait,
  and needs a title that writes those registers during play.
- A game's exit to the BIOS or a soft reset with a stream open (the SCI and
  now a TMU source enabled at that moment): not traced.
- tests/test_native_sdk_sci_owner_contract.py and
  test_native_sdk_sci_helper_contract.py need the private executables and were
  not run; the other 295 entries of the mutation list were not run; the Test 81
  delivery documents do not exist yet and were not reviewed.
- Nothing ran on a console or an emulator: every result above is a host model
  or a linked-code reading.
