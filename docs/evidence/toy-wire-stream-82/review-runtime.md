# Review of Toy Test 82: run time, the installed worker against the reader

An independent reader's review of commit `b604dea`, 16 November 2026. The
reader worked on scratch exports outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened and a note on the housekeeping of its own scratch
files left out. Nothing here ran on a console: what it ran is the host model,
a host test or a linked build. What was done with each finding is in the
test's document, under Review.

## Finding 1

Toy Commander with the VMU off is not Test 81 on this module: the low
residents' controller wake has no caller, and the fixture's scenario for it
passes only because the fixture makes the call itself.

- Severity as the reader gave it: minor
- Where: src/loader/vmu_native_reader_capture.inc:251-286 (worker: Toy form
  calls config.sd_lease(NULL, PUMP_COOKIE); installed form sends TICK instead);
  src/loader/vmu_native_passive.c:2530-2541 (called at every controller
  completion); src/loader/vmu_native_background_resident.c:162-170;
  src/loader/retail_async.c:1375-1384 (status-only CHECK on the movie policy),
  1413-1429 (kui_retail_native_demand_wake);
  src/loader/native_readahead_aux.c:549-567 (toy_suite_active);
  src/loader/toy_movie_iso_stage.inc:130-166 (Toy Commander stays a source with
  the VMU off); tests/test_toy_wire_integration.c:2440-2489 (sequence_pending,
  wake at 2489)

Scenario:

Toy Commander, virtual VMU off, Test 82 module. The stage still gives it movie
ranges and hooks as in Test 81, so requests 386 sectors or more into a movie
file run on the Toy residents' movie policy: sliced token search, and a CHECK
from a caller with interrupts open returns the status without doing work. In
Tests 32-81 the Toy worker called the low wake at every controller completion
(deliver(0) when the low stream is not busy, a re-arm of the SCI interrupt
level when it is). The installed worker is compiled without Toy switches and
never makes that call, and nothing else in the module does. If a token search
is left standing (low stream state 5) and the game then only asks for status,
the read never completes. The commit message and the stage comment present Toy
Commander with the VMU off as unchanged ('as before'); neither mentions the
wake.

Evidence:

Read: the two forms of reader_cooperative_service(). Ran: preprocessed both
workers and diffed them (scratch pp/worker-81-82.diff); disassembled
_reader_cooperative_service in build/t82/native-worker.elf: no call through
config.sd_lease. Ran in a copy of the fixture (scratch fix/review_fixture.c,
RV_WAKE=1): sequence_pending's third game as committed completes in 63 rounds;
with its single independent_demand_wake() call removed (RV_NOWAKE=1) it is
still pending after 19,990 rounds (engine.active 1, low state 5, done 0 of 13,
toy_suite_active 1); the same hang when the game calls CHECK every fifth
round, in both call forms (gd() and sdk_poll from the movie poll site). The
committed fixture's other two games (EXEC polls) complete in 70 rounds without
the wake. Inference, not run on anything real: Toy Commander's player polls
EXEC every frame, so on a console I expect fewer reader visits per frame on
the low reader's movie path (and no per-frame re-arm of IPRB while the stream
holds the port) rather than a hang; a phase of the game that waits with CHECK
only would hang.

Reached on a console:

Toy Commander with the virtual VMU off on the Test 82 module only. Not reached
by other titles (their movie ranges are empty, suite_file() returns 0,
toy_suite_active stays 0) and not by Toy Commander with the VMU on (no ranges,
no hooks). Whether the game ever waits on a movie-file read without calling
EXEC is not established.

Suggested:

Either say in the test's document and instructions that Toy Commander with the
VMU off on this module is not a Test 81 control and must not be compared with
it, or have the stage keep Toy Commander on the plain reader in this module
with the VMU off too. Before Toy Commander is a source in any module with the
installed worker, give the low residents a pump that does not depend on the
worker. Make sequence_pending's third game stop calling the wake when
KUI_TOY_WIRE_VMU is set, so the fixture models what the module does.

## Finding 2

A console run of this module leaves no record of what the reader and the loans
did, and a reader stop appears on the VMU worker's terminal page.

- Severity as the reader gave it: minor
- Where: include/kui/vmu_native_passive.h:8-14,91-93 (no report in the
  configuration); src/loader/vmu_native_reader_capture.inc:59-66,73-77
  (auxiliary exports rejected: image_end bound is load+0x3000, the Toy
  auxiliary's image is 0x6000); src/loader/vmu_native_passive.c:1512-1532 (the
  page's auxiliary line needs those exports), 2303-2333 (held-controller page
  only for early-server driver families); src/loader/retail_resident.c:844-863
  (Toy variant: report_fault goes to kui_vmu_observe_low_failure, no 'GD STOP'
  page); src/loader/vmu_native_background_resident.c:200-207

Scenario:

Evolution-class title, virtual VMU on, movie playing, saves made. What the
owner sees is the same whether windows opened and were stopped and reopened
around each loan, no window ever opened, or loans were refused for long
stretches (the VMU answering 'again'): the stage prepares no report, the
auxiliary's stream counters (starts, drops, restarts) are never shown or
saved, and the worker cannot read the auxiliary because its header check fails
on the image size. If the low reader stops (refused call, failed read), the
Toy residents enter the worker's terminal, so the screen is the VMU worker's
page without function, command or sector, where the installed residents would
show their own reader page.

Evidence:

Read the listed code. Ran: disassembly of the t82 worker shows the bound
lease+0x16000 against the Toy auxiliary's image end at lease+0x19000 (nm on
build/t82/native-ra-aux.elf), so reader_readahead_exports() returns NULL. The
terminal page content (state.error 0x100,
src/loader/vmu_native_passive.c:1631-1637) is from reading, not seen on a
screen.

Reached on a console:

Every console run of the Test 82 module, any title, VMU on or off.

Suggested:

State in the test's instructions that the module gives pass/fail observations
only, what each visible outcome can and cannot show (in particular that a
clean run does not show a window was open when a loan came), and that a VMU
worker terminal page on this module can be a reader stop. If numbers are
wanted for the loan path, a later step needs a report route that does not go
through the Toy worker.

## Finding 3

When the stream's stop fails for good, every later loan is answered 'busy' and
the fault is never reported by the loan.

- Severity as the reader gave it: note
- Where: src/loader/toy_wire_ahead.inc:208-235 (toy_wire_halt sets port.fault
  and A.error); src/loader/toy_movie_ahead.inc:172-187 (toy_ahead_clear returns
  before clearing A.receiving); src/loader/native_readahead_aux.c:818-843
  (quiesce); src/loader/retail_async.c:1101-1147 (loan_begin returns BUSY on a
  refused QUIESCE before its own port.fault check)

Scenario:

A window is receiving, the worker asks for the card, and kui_wire_stop()
cannot hand the port back (in my scenario: DMA channel 1 taken by another
source during a run, with the bus's latched fault). The first loan is refused;
every later QUIESCE retries the low stop on a faulted port and is refused
again, so the VMU answers 'again' indefinitely and no fault page appears until
the next disc read fails.

Evidence:

Ran scenario R4 in my copy of the fixture (RV_DET=1): eight loan attempts all
BUSY, port.fault 1, window still marked (variant 5, receiving 1), stream off,
low state 3; the next read ends FAILED with error 1. By reading only (not
run): in the installed auxiliary the second attempt reaches loan_begin's port
fault and returns FAULT, which the worker shows as fail(24).

Reached on a console:

Needs a real, lasting stop failure with a window open and the VMU on (another
title). Fatal either way; only the symptom differs.

Suggested:

In loan_begin, look at port.fault after a refused QUIESCE and return the
fault, or have the Toy branch of quiesce() report a failed halt distinctly
from 'busy'.

## Finding 4

A refused loan is not always without effect: the stream can already be
stopped.

- Severity as the reader gave it: note
- Where: src/loader/native_readahead_aux.c:818-843;
  src/loader/toy_movie_ahead.inc:172-187; tests/test_toy_wire_integration.c
  (titles_lent)

Scenario:

The commit message says a loan asked for while a command is pending 'is
refused and changes nothing'. That holds for a refusal before QUIESCE. When
the refusal comes from QUIESCE itself (the card's stop command fails once),
the stream has been halted and the window is still marked as receiving; the
next ask is granted.

Evidence:

Ran scenario R7 in my copy of the fixture: first ask BUSY with stream off,
variant 5, receiving 1, low state 3, hooked 1; granted one ask later; reads
behind it exact and a window open again (windows 2).

Reached on a console:

Another title, VMU on, window open, a CMD12 that fails once. Recovers by
itself in the model.

Suggested:

None needed for behaviour. Narrow the sentence in the test's document to the
case tested, or add this case to titles_lent().

## Finding 5

The image-size check in the worker is the only thing keeping the installed
worker's visits away from the Toy auxiliary.

- Severity as the reader gave it: note
- Where: src/loader/vmu_native_reader_capture.inc:59-66,73-77,276-279,374-377;
  src/loader/native_readahead_aux.c:1550-1557 (CAPTURE_START resets statistics
  and calls toy_wire_reset(false)), 1621-1626 (TICK counts only);
  include/kui/native_readahead_layout.h:44,82

Scenario:

The installed worker sends TICK at controller completions and CAPTURE_START on
its profile gesture. Neither reaches the Toy auxiliary, because the worker was
compiled for a 0x3000-byte auxiliary image and rejects the 0x6000-byte one.
Nobody states this as a requirement; a later auxiliary that passes the check
would receive both, and CAPTURE_START would reset the stream's state from the
worker.

Evidence:

Read the code; disassembly of the t82 worker for the bound and for
reader_cooperative_service (tail call to the auxiliary's visit only when
exports are non-NULL). I did not examine whether toy_wire_reset(false) is safe
with a stream live, since it is not reached.

Reached on a console:

Not reached in this module.

Suggested:

Record the dependency where the composition is described, or make the
auxiliary refuse these two events from the worker explicitly in the Toy build.

## Finding 6

The recipe cites a document that is not in the commit, and the plan's row for
this step still lists work the commit does not do.

- Severity as the reader gave it: note
- Where: Makefile.vmu-native-toy-suite:390-393
  (docs/toy-wire-stream-test-82.md); docs/fmv-stream-reader-plan.md:337
  ('quiesced cleared')

Scenario:

A reader following the Makefile comment finds no document. The plan says this
step clears 'quiesced'; the commit instead keeps Toy Commander off the window
with the VMU on (stated in the commit message) and leaves the flag as it was.

Evidence:

git ls-tree of b604dea has no such file; git show --stat lists no file under
docs; read the plan row.

Reached on a console:

Not a run-time matter.

Suggested:

Add the document in the documentation commit and correct the plan row there.

## Examined and found sound

- Worker identity: build/t82 native-worker.bin and .reloc equal the installed
  recipe's built with the same label (49,920 bytes, sha256 ccea1bd4b36901db...,
  reloc 519073f1114df353...), and with label f8807cb16561 the worker equals the
  hash recorded in docs/evidence/native-shared-f8807cb/snapshot-linked.json
  (22799207...a693).
- Residents and auxiliary identity: resident-sci.bin (15,848 bytes,
  96d6357784d56fc1...), resident-legacy.bin (9ed0684db15ac385...) and
  native-ra-aux.bin/.reloc (24,576 bytes, 45dfb95f72c93b1e... /
  161db911bdebcd23...) are equal between the Test 81 and Test 82 builds; only
  stage.bin differs (302,856 against 291,840 bytes).
- Shared structures: a probe compiled with each part's real flags gives the
  same offsets for the retained region (engine 0x240, shared 0x320, loan state
  0x2f4, service.pending 0x370, stream 0x464, port 0x4dc), the low control block
  (0x38 bytes) and the card device for the Toy residents, the worker's view, the
  auxiliary and the installed residents.
- Lease geometry: both layouts total 0x47000 and both preprocessed workers
  check lease_bytes against 0x47000; the worker addresses nothing in the
  auxiliary's image, stack or arena by a fixed offset (grep), only the region at
  +0x12400 and the header at +0x13000.
- Low entries: the heap hook, return hook and lease entry bound into the Test
  82 stage (0x8c0073e8, 0x8c0074f8, 0x8c007514) lie inside the worker's accepted
  range [0x8c004000, 0x8c007800).
- Lease and loan code: kui_vmu_observe_sd_lease and loan_begin/loan_end are
  the same source in both resident variants apart from the NULL-callback wake
  branch, and the installed worker never passes a NULL callback (disassembly).
- Stack on the loan path: per-function stack use of the low lease path differs
  by at most a few bytes between variants (lease 72 against 68, stop 16 against
  12), and the QUIESCE work runs on the auxiliary's private stack.
- Masking and timing of loan_begin: the stop path only polls hardware flags in
  bounded loops (overrun wait up to 200,000 polls, transmit end up to 10,000,
  the stop command's busy wait up to 200,001 bytes); nothing in it waits for an
  interrupt or a timer, and worker hooks, the auxiliary gate and the SDK SCI
  entry all run masked, so nothing of the reader runs inside a loan.
- Loan against the stream, host model: in 800 randomised runs of my copy of
  the fixture, 330,127 granted loans each found no window, the stream off, the
  card out of its read command, chip select high, DMA channel 1 off, no overrun
  or receive flag, timers stopped and restored, IPRA/IPRB as the title left them
  and the title's table kept; a worker-like card transaction then succeeded,
  later reads were exact and windows opened again.
- States at a grant: loans were granted with the wire stream running, waiting
  (timer armed and timerless) and full, with an overrun pending (30 times) and a
  timer underflow pending (16 times), and with low stream states 0 to 3; none
  was refused while the reader was idle and none returned a fault.
- The quiesced flag: it is set in quiesce()
  (src/loader/native_readahead_aux.c:821), read only at the MOVIE_AHEAD gate
  (1488-1503) and cleared only by RESUME (1645-1646), which nothing sends;
  MOVIE_AHEAD is sent only by Toy Commander's hook, so for other titles the flag
  gates nothing.
- Toy Commander with the VMU on: the stage returns before any scan, so there
  are no ranges, no source, no hooks and toy_suite_active is never set; the
  title runs on the plain Toy-variant low reader, nothing in residents or
  auxiliary depends on reader-only mode, and 138,456 loans against that reader
  in the model caused no failure.
- Other titles and the movie policy: their movie ranges are empty, so
  status-only CHECK, the sliced token search and the wake never apply to them
  with the VMU on or off; for them the reader with the VMU off is Test 81's.
- Worker visits to the auxiliary: the only two (TICK and CAPTURE_START) go
  through reader_readahead_visit and are dropped because the exports are
  rejected; the Toy auxiliary's TICK is a counter only.
- Exit and fault entries: the low menu return ends at the reset vector in both
  variants, and a low fault enters the worker's terminal through the same entry
  the return hook uses.

## What the reader ran

- Built in an export of b604dea under the scratch directory: Test 82
  (TOY_WIRE_VMU=1, BUILD_ID=aaaaaaaaaa82, build/t82), Test 81 (same label,
  build/t81), the installed recipe with the same label (build/inst) and with
  label f8807cb16561 (build/instf): all exit 0.
- sha256 over the parts of the four builds: worker equal between t82 and
  installed; residents and auxiliary equal between t81 and t82; stage differs;
  worker with label f8807cb16561 equals the recorded accepted hash.
- make toy-wire-integration-host-test with TOY_WIRE_VMU=1 (build/h82): PASS,
  359,154,001 checks, 53.6 s; titles_lent reports 6 loans granted and 1 refused
  for each of three bus settings.
- make toy-wire-host-tests with TOY_WIRE_VMU=1: all PASS, 17.6 s.
- Layout probe (<scratch>/review82/runtime/probe/probe.c) compiled with four
  flag sets: offsets equal as listed.
- Preprocessed diffs: worker Test 81 against Test 82 (664 differing lines, 41
  hunks, read); residents Toy against installed (retail_async 167 lines,
  sci_stream 220, sci_sd_bus 11, background resident 59, retail_gd 0).
- objdump and nm on the t81/t82 worker and auxiliary ELFs; stack-usage (.su)
  comparison of the low loan path.
- Reviewer fuzz v1 (random reads, commands, idle, a loan attempt between any
  two events; 160 runs): 72,025 granted, 1,029,979 refused, all invariants held,
  data exact.
- Reviewer fuzz v2 (adds interrupts held off, aborted requests, late tokens,
  raw reads; 320 runs): 119,646 granted, 2,434,392 refused, first_error 0;
  control without loans shows the same kinds of drops.
- Reviewer fuzz v2 with no window source (320 runs): 138,456 granted, 12,612
  refused, no failure.
- Deterministic scenarios R4 (stop fails for good), R6 (loan ended unhealthy:
  quarantined, later read stays in progress) and R7 (stop command fails once):
  results as given in the findings.
- sequence_pending's third game in my copy of the fixture: with the wake 63
  rounds; without it still pending after 19,990 rounds; same hang with
  CHECK-only polling in two call forms; with the wake both CHECK-only forms
  complete in 63 rounds.

## Not examined

- Anything on a console or emulator: the card's behaviour when its read
  command is ended mid-block for a loan, the SCI pin read-back in
  kui_sci_sd_release, real loan duration with interrupts masked, and how long a
  VMU access is held off while a movie's requests are outstanding.
- The lease entry itself in the reader fixture: the fixture and my scenarios
  call loan_begin/loan_end directly; kui_vmu_observe_sd_lease, storage
  acquire/release and the worker's durable transaction are not linked there, and
  my card transaction is a CMD18 plus CMD12 stand-in.
- The stage's VMU preparation, launch screens and package routing (outside my
  lens), the mutation list and the package test (not run).
- The worker's VMU protocol and Maple logic (unchanged installed code) and the
  legacy resident beyond its hash.
- Toy Commander's real polling pattern: whether it ever waits on a movie-file
  read without EXEC is not known to me, so the console effect of the missing
  wake is inference.
- Partial arrival of a DMA run: the model lands runs whole.
- Low stream states 4 to 7 standing at a loan: by reading they need the Toy
  movie policy, which has no loans in this module; I did not run a loan against
  them.
- Why HEAD's installed recipe with label f8807cb16561 reproduces the recorded
  worker but not the recorded resident-sci.bin and native-ra-aux.bin hashes in
  the f8807cb snapshot (outside this commit; I may have compared against another
  family's record).
- Whether toy_wire_reset(false) would be safe with a stream live (unreached).
