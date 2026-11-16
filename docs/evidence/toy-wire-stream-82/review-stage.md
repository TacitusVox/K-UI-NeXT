# Review of Toy Test 82: the composition and the cold stage

An independent reader's review of commit `b604dea`, 16 November 2026. The
reader worked on scratch exports outside the tree and was told what the change
was meant to do, not what to conclude. This is its result as it returned it,
with scratch paths shortened and a note on the housekeeping of its own scratch
files left out. Nothing here ran on a console: what it ran is the host model,
a host test or a linked build. What was done with each finding is in the
test's document, under Review.

## Finding 1

Toy Commander with the VMU off on Test 82 is not Test 81: the installed worker
never sends the controller's wake the Toy low reader waits for.

- Severity as the reader gave it: minor
- Where: Makefile.vmu-native-toy-suite:431 (worker loses every Toy switch);
  src/loader/vmu_native_reader_capture.inc:251-285 (reader_cooperative_service:
  Toy branch 252-266 calls the low gateway with the pump cookie, installed
  branch 268-285 does not); src/loader/vmu_native_background_resident.c:162-177;
  src/loader/retail_async.c:1414-1428 (kui_retail_native_demand_wake);
  src/loader/toy_movie_iso_stage.inc:162-167 (Toy Commander stays a source with
  the VMU off); tests/test_toy_wire_integration.c:2487-2489

Scenario:

Toy Commander is launched on the Test 82 module with the virtual VMU off. The
stage makes it a source by its own files and installs its movie hooks exactly
as Test 81 does. During a movie a read that the low reader's movie path owns
(stream.toy_suite_active set) is pending and the game makes no GD call for a
while. In Test 81 the worker calls the low gateway with the pump cookie on
every proved controller completion and kui_retail_native_demand_wake() moves
the read. Test 82's worker is the installed one and has no such call, so the
read stands until the game's next GD call. The commit message presents Toy
Commander with the VMU off as unchanged ('stays a source with the virtual VMU
off only'); a Toy Commander run on Test 82 used as a comparison with Test 81
carries this difference, and Test 82 has no report to show it.

Evidence:

Ran: the literal 0x4b504d50 (KUI_RETAIL_NATIVE_PUMP_COOKIE) occurs once in
build/t81/native-worker.bin and zero times in build/t82/native-worker.bin and
build/inst/native-worker.bin, while resident-sci.bin (identical in 81 and 82)
still carries the pump entry. Preprocessed worker source, Test 81 flags
against Test 82 flags, shows the two bodies of reader_cooperative_service.
Experiment in a copy of the tree: with the one wake call at
tests/test_toy_wire_integration.c:2489 removed, the whole-reader fixture
aborts in sequence_pending(2) ('the game then asking again with no call at
all') at line 2480, assertion rounds < 20000u: in that state nothing but the
wake moves the read; the two variants with game polls finish in 70 rounds.
Read, as compiled for Test 82 (preprocessed auxiliary): toy_suite_active is
set only for a request inside one of Toy Commander's movie files, so for
another title the wake returned false in Test 81 as well and nothing differs
there.

Reached on a console:

Yes, on Test 82 with Toy Commander and the virtual VMU off only. Not reached
by another title (flag never set), nor by Toy Commander with the VMU on (no
source, no movie path). How often a real Toy Commander movie has a pending
movie-path read with no GD call is not known; its player normally polls.

Suggested:

Stage-only, in keeping with the composition: under KUI_TOY_WIRE_VMU make Toy
Commander no source with the VMU off as well (return from toy_movie_scan
before the card is asked for), so no Toy-only liveness path depends on a
worker that does not serve it. Otherwise say in the test's document that Toy
Commander on Test 82 is not Test 81's reader and must not be used as a
regression comparison.

## Finding 2

On Test 82 a reader stop is shown as the installed worker's VMU journal page,
the same page and code as a game exit; there is no report and nothing on
screen names the test.

- Severity as the reader gave it: minor
- Where: src/loader/retail_resident.c:844-860 (report_fault under
  KUI_TOY_SUITE_ASYNC_DEMAND goes to kui_vmu_observe_low_failure; the installed
  residents print their own 'GD STOP' page) and 1050-1077;
  src/loader/vmu_native_background_resident.c:200-207;
  src/loader/vmu_native_passive.c:1488-1540 and 1631-1637 (terminal sets DETAIL
  0x100 and prints the VMU page), 2303-2312 and 2328-2336 (held gesture only for
  early-server SDK families); src/loader/vmu_native_reader_capture.inc:61-70
  (auxiliary row suppressed, see the note on the 0x3000 bound)

Scenario:

First console run with real VMU traffic: something stops the low reader (image
read failed after a loan, a request rejected, an unsupported GD function). The
Toy residents have no stop page of their own: they enter the worker's
terminal, which in Tests 68 to 81 was the Toy worker's. In Test 82 it is the
installed worker's and prints 'VMU / A2', 'SCI JOURNAL', PHASE, 'DETAIL 100',
the VMU counters and 'PHOTO / POWER OFF'. A game leaving through a hooked exit
prints exactly the same page with the same DETAIL 100. The photograph cannot
tell a reader stop from an exit, shows no function, command or LBA, none of
the stream's counters (windows, stops, what was given up), and no saved report
exists. With the VMU off Test 81 and Test 82 built from one commit also show
the same launch lines and BUILD label.

Evidence:

Ran strings over the built parts: build/inst/resident-sci.bin contains 'GD
STOP' and 'RET', build/t81 and build/t82 residents contain neither;
build/t82/native-worker.bin contains 'SCI JOURNAL' and neither 'FAULT CODE AT
VALUE' nor 'REPORT SAVED TO SD' (both in Test 81's worker);
build/t82/stage.bin has no 'TOY TEST' text, only '72 KIB VMU / 284 KIB
BACKGROUND', 'SCI STREAM READ AHEAD READY', 'NO STREAM READ AHEAD / PLAIN
READER' and the BUILD label (Test 81's stage also has 'TOY TEST 81 ...').
Preprocessed residents, installed against Test 82, show report_fault's two
bodies. The rest is reading of report() and kui_vmu_native_passive_report().

Reached on a console:

Yes: any title on Test 82 whenever the low reader stops or the game leaves
through a hooked exit; VMU on or off.

Suggested:

Before the console run, write down what DETAIL 100 means on this build and
have the owner record the module's payload bytes and CRC from the launch
record (311,048 bytes for my label) to identify it. If a stop must be
readable, the cheap stage-only step is a launch line that names the test; a
stop page with FN/CMD/LBA needs the residents' own page back, which would make
them no longer Test 81's bytes and is a separate decision.

## Finding 3

TOY_WIRE_VMU=1 is accepted without TOY_WIRE_STREAM=1 and silently builds a
module under another test's name.

- Severity as the reader gave it: minor
- Where: Makefile.vmu-native-toy-suite:77-90 (the error chain for switches
  used without the stream has no TOY_WIRE_VMU entry), 390-399 (its checks sit
  inside the stream block), 420-435 (the block that swaps the worker sits
  outside it)

Scenario:

make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_VMU=1 (stream
switch forgotten) builds build/toy-suite-66/retail-toy-test-66.kui: Test 66's
residents and auxiliary, the installed worker, a stage compiled with
KUI_TOY_WIRE_VMU and no report seed, under the very file name Card Tools' '66
SCI movie playback' entry loads. The same happens for any other TOY_SUITE_MODE
(the default gives retail-toy-test-1.kui). TOY_WIRE_VMU=2 without the stream
is silently treated as 0.

Evidence:

Ran make -n for these invocations in my export: exit 0, 59 commands carry
-DKUI_TOY_WIRE_VMU=1, the worker's compile line has no Toy -D at all, no
toy_return_hardware object, last step 'cp ... retail-toy-test-66.kui'. With
the stream switches the guards work: TOY_WIRE_VMU=2 stops at line 398, Test
80's switches plus TOY_WIRE_VMU=1 stop at line 395. Every other TOY_WIRE_*
switch has a '... extends the Test 68 reader: use TOY_WIRE_STREAM=1' error in
lines 77-89.

Reached on a console:

Host-only: a build mistake. It would reach a console only if such a file were
packaged.

Suggested:

Add to the chain at lines 77-90: else ifneq ($(TOY_WIRE_VMU),0) / $(error
TOY_WIRE_VMU extends the Test 68 reader: use TOY_WIRE_STREAM=1 and Test 81's
switches).

## Finding 4

No test holds the composition: a stage that still stops for the VMU, and a
worker that is no longer the installed one, pass every Test 82 target.

- Severity as the reader gave it: minor
- Where: Makefile.vmu-native-toy-suite:428-431 and the test targets 516-586;
  src/loader/vmu_native_heap_probe_stage.c:214-222;
  tests/test_toy_reader_package.c:86-121;
  tests/test_toy_wire_stream_mutations.py:916-930

Scenario:

Three defects planted one at a time in a copy of the tree, each built as Test
82 and run through toy-wire-host-tests, toy-wire-package-test (no-report) and
toy-wire-linked-check. D1: the stage keeps the 'THIS TEST BUILD NEEDS VIRTUAL
VMU OFF' stop under the switch (the change's point (c) undone): everything
passes, the stop's text is in stage.bin. D2: the filter lets
-DKUI_NATIVE_TOY_MOVIE_CAPTURE=1 through to the worker: the module builds with
a 50,304-byte worker that is not the installed 49,920-byte one, everything
passes. D3: 'RA_FLAGS := $(RA_FLAGS)' removed: the auxiliary silently becomes
the installed block cache (10,400 bytes), the module still builds, host and
package tests pass, and only toy-wire-linked-check fails ('the gate does not
read the MMU's control register'). The mutation list's three new entries cover
the scan's rule (d) only.

Evidence:

Ran all three (logs under <scratch>/review82/stage/mk/ : d1-build.log,
d1-tests.log, d2-build.log, d2-tests.log, d3-build.log, d3-tests.log,
d3-linked.log). The committed build itself is right on all three points (see
checked_sound); this is about what would notice a later slip, for example a
new switch whose flag does not match -DKUI_TOY_% or -DKUI_NATIVE_TOY_%.

Reached on a console:

Host-only today. D1 on a console would stop every VMU-on launch with a plain
message; D2 would put an unreviewed worker beside the virtual card.

Suggested:

A check target for TOY_WIRE_VMU=1 that builds the installed recipe and Test 81
with the same BUILD_ID and compares native-worker.bin and .reloc with the
first and resident-sci.bin, resident-legacy.bin, native-ra-aux.bin and .reloc
with the second (cmp), run with the package test; and have the no-report
package test refuse a module that contains the stop's text, or add a host test
of native_runtime_mode() as the titles test has for the scan.

## Finding 5

titles_lent's loan is loan_begin and loan_end alone: the gateway the worker
really calls, its port acquire and release, and any card traffic during the
loan are in no Toy host test.

- Severity as the reader gave it: minor
- Where: tests/test_toy_wire_integration.c:3628-3660 (lend_card) and
  3661-3720; src/loader/vmu_native_background_resident.c:154-199
  (kui_vmu_observe_sd_lease); Makefile.vmu-native-toy-suite:561-576 (the
  fixture's sources: retail_async.c, sci_stream.c, the auxiliary and the stream;
  no resident, no sci_sd_bus.c)

Scenario:

What the installed worker does for each VMU block is: low gateway (guard on
the callback's address), loan_begin, kui_retail_storage_acquire on the SCI
port, the worker's own card commands for one block through the lent bus,
release, loan_end. The new scenario does begin, its checks, end(true). So the
host tests say nothing of: the gateway's branch that closes the loan and
answers BUSY when the port cannot be taken; the SCI registers as the real
release leaves them before the next window opens; the card after commands that
are not the stream's. The commit message's 'as the virtual VMU's worker asks
for it' is wider than what is run.

Evidence:

Read the scenario and the gateway; grep over tests/: kui_vmu_observe_sd_lease
and vmu_native_background_resident.c appear only in checks of other recipes,
and kui_sci_sd_acquire in the fixture is a model
(tests/test_toy_sdk_liveness.c:103, tests/test_retail_async.c:58). I did not
extend the fixture. One fact that lowers the risk, by reading: the Toy
worker's report save at the end of Tests 68 to 81 goes through this same
gateway with card traffic (vmu_native_passive.c:817-820,
toy_suite_report_save.inc:966), so it has run on the console after stream use,
with the reader at rest.

Reached on a console:

Yes: every block the game reads from or writes to the virtual card on Test 82.

Suggested:

Let lend_card take the port as the gateway does and read or write one block on
the card model between begin and end (compiling the real gateway is not
practical on the host), or state this limit beside the scenario and in the
test's document.

## Finding 6

The worker's compiled view of the auxiliary is the installed geometry; in this
composition every visit it would make to the auxiliary is refused by its own
bound.

- Severity as the reader gave it: note
- Where: src/loader/vmu_native_reader_capture.inc:49-77, 268-281, 374-377;
  include/kui/native_readahead_layout.h:32-83

Scenario:

The worker, compiled without Toy flags, has the auxiliary's image limit as
0x3000 bytes, its stack at 0x16000, its data at 0x17000 for 0x30000 bytes and
384 blocks; the stage and auxiliary have 0x6000, 0x19000, 0x1a000 for 0x2d000
and 360. reader_readahead_exports() therefore returns NULL (image_end = load +
0x6000 > load + 0x3000), so the TICK the installed worker sends on controller
completions, CAPTURE_START and the read-ahead rows of its pages never reach
the stream's auxiliary. Nothing on the virtual card's path uses them and the
Toy auxiliary's TICK case only counts, so I found no harm; but it is a shared
constant that does differ, the outcome rests on the image being larger than
0x3000 bytes, and the commit message does not mention it.

Evidence:

Compiled probes (wrapper translation units built with each part's exact flags,
values read from the assembly): worker 0x3000/0x16000/0x17000/0x30000, stage
and auxiliary 0x6000/0x19000/0x1a000/0x2d000; build/t82/native-ra-aux.bin is
24,576 bytes. The handler's behaviour is from the preprocessed auxiliary.

Reached on a console:

Yes, on every proved controller completion: a read-only check that returns
NULL.

Suggested:

Say it in the test's document. No code change needed for this test.

## Finding 7

The Test 82 package test does not ask the module for what a VMU-on launch
needs.

- Severity as the reader gave it: note
- Where: tests/test_toy_reader_package.c:60-74 (the copied mode check is for
  enabled=false only), 109-116 (cache seed printed as optional; no card profile
  slot check); src/apps/games_retail.c:1556-1568;
  src/apps/games_vmu_card.inc:126-165

Scenario:

For a VMU-on launch the Games app needs the mode record writable to NORMAL, a
ready native mapping (the blank cache seed slot) and exactly one card profile
slot. In no-report mode the test still treats the seed as optional and never
looks for the profile slot, so a module missing either would pass and fail
only at launch.

Evidence:

Ran my own host program on the built bytes with the tree's functions
(kui_vmu_native_toggle_find, the mode loop of games_card_emulation_mode for On
and Off, kui_vmu_native_cache_seed_find, kui_vmu_game_card_profile_slot): Test
82 has one toggle capability, one mode slot (default mode 0), one blank seed
and one template profile slot, and the same raw magic counts as the installed
recipe's module. So the committed module is fine; the test does not hold it.

Reached on a console:

Not a defect of the delivered module; a gap in its test.

Suggested:

In no-report mode require the seed and the profile slot and run the mode write
for enabled=true.

## Finding 8

Makefile comments: a document that is not in the tree, and a wrong statement
about the residents' flags.

- Severity as the reader gave it: note
- Where: Makefile.vmu-native-toy-suite:391-393
  (docs/toy-wire-stream-test-82.md), 421-423

Scenario:

The comment names docs/toy-wire-stream-test-82.md, which does not exist at
this commit (the last such file is toy-wire-stream-test-79.md). The comment
'The low residents are compiled with the same flags and do not ask' is not
what happens: their FLAGS were fixed at line 142 and -DKUI_TOY_WIRE_VMU=1
never reaches them, which is why they stay Test 81's bytes.

Evidence:

ls docs; make -n: 0 of 24 resident and legacy compile commands carry the flag.

Reached on a console:

No.

Suggested:

Correct the comment; add the document with the evidence commit.

## Examined and found sound

- Worker identity: with one BUILD_ID the worker's 32 compile and link commands
  are word-for-word the installed recipe's (make -n compared per target), its
  preprocessed sources are identical, and native-worker.bin and .reloc are
  byte-identical to the installed recipe's (sha256 ccea1bd4...); the Toy tower
  adds nothing else to its sources, objects, layout
  (vmu_native_background_compact.ld), span (0x12000) or link.
- Stronger than claimed: built with the label f8807cb16561 the worker is
  sha256 2279920728e94c48... and its .reloc aa7a1dda..., exactly the delivered
  installed cohort's values in
  docs/evidence/native-shared-f8807cb/snapshot-linked.json (role background_a2);
  two labels change 12 bytes of the worker.
- Residents and auxiliary: resident-sci.bin, resident-legacy.bin,
  native-ra-aux.bin and .reloc are byte-identical between Test 81 and Test 82
  under one label; all 9 auxiliary objects keep every Toy switch, hash.o keeps
  -DKUI_CD_EDC_CACHED_PAIR=1, and -DKUI_TOY_WIRE_VMU reaches none of the 24
  resident/legacy objects nor the auxiliary.
- Config stamp and switching: toy-suite-config gains TOY_WIRE_VMU=1; switching
  0 to 1 and back in one BUILD directory recompiles everything (119 and 121
  compiles) and gives the same modules as clean builds; the module does not
  depend on the BUILD path.
- Nothing changes without the switch: Tests 79, 80, 81 and the installed
  recipe build to identical modules at b604dea and at its parent under one
  label, and Test 79 with its delivered label 9f183a7f72f5 reproduces the
  recorded 557822ecfcd9...47be.
- Stage, Test 81 against Test 82, as preprocessed: exactly five differences
  (toy_other_mode() is 1; no report struct, seed or prepare; no VMU-off stop;
  config 2,304 bytes; no report call). The other 40 stage translation units are
  identical.
- Stage, installed against Test 82, as preprocessed: only the Toy geometry
  (auxiliary span, stack, data), the low limit and hook stack, the linked low
  addresses, toy_movie_scan and the Toy read-ahead binding differ; nothing on
  the virtual card's path (card admission at storage init, durable handoff,
  exits, diagnostics, heap and terminal hooks, mode, seed replay, toggle
  capability) differs or changes order. For another title the scan touches no
  card; for Toy Commander with the VMU on it returns before the card is asked
  for.
- Worker configuration: compiled under the stage's flags and under the
  worker's it has the same 14 field offsets, the same nested sizes and 2,304
  bytes (27 probes equal); the worker's arm refuses any other size
  (vmu_native_passive.c:1644), so a mismatch would fail closed.
- Structures shared across the flag sets: the reader region, engine, shared
  service, stream and port offsets, the loan states and result codes, the lease
  result codes, the SDK cookie index and the map and region offsets are equal as
  compiled for the worker, the residents and the auxiliary (72 probes); struct
  kui_sci_stream_state has different members under Toy flags but the same size
  and the same offsets for what the worker reads.
- Low entries handed to the worker: heap hook 0x8c0073e8, return hook
  0x8c0074f8, lease 0x8c007514 match the linked Toy residents and pass the
  installed worker's hard-coded range check (below 0x8c007800,
  vmu_native_passive.c:1713-1718).
- Worker stack on the lease path: from the residents' LTO callgraphs the low
  share of the storage path is 132 bytes with the Toy residents against 128 with
  the installed ones, the Toy visit gate parks 24 bytes against 20, and the lent
  bus callbacks are 8/24/0 in both; the worker's stack is 3 KiB. This is my
  arithmetic, not the tree's stack proof.
- The loan functions (loan_begin, loan_end, native_loan_blocked, fault, vector
  check, unhook) are textually identical in the installed and Toy residents as
  preprocessed, and sci_sd_bus.c has no mode-66 difference.
- Module as the Games app sees it: envelope and CRC good, one dual descriptor
  (lease 0x47000), no session descriptor, one toggle capability, the mode record
  writable for On and Off, one blank cache seed, one template card profile slot,
  payload 311,048 bytes under the 0x112000 handoff limit; stage binary ends at
  0x8ce49f08 and bss at 0x8cec3b48, inside the linker's limits; bottom-heap
  placement is compiled, so the stage's 21,088 bytes of growth over the
  installed stage matter only to a game whose allocator sentinel or table lies
  in that span (inference).
- toy_other_mode(): another title on a raw SCI image is a source with the VMU
  on or off; cooked, audio-only, Mode 2, other adapters and the Legacy reader
  are not; Toy Commander is one with the VMU off only and its executable on
  another boot sector never; tests/test_toy_titles_stage.c passes, and all 14
  scan entries of the mutation list are caught for Test 82 (12 of 12 for Test
  81). With no source the auxiliary opens no window (suite_raw needs
  suite_source 1).
- The auxiliary's quiesced flag stays set after the first loan (nothing sends
  RESUME), but as compiled for Test 82 it is read only by the MOVIE_AHEAD event,
  which only Toy Commander's hook sends; other titles are unaffected, which
  matches the commit's restriction on Toy Commander.
- The marker symbols of stage, worker, residents and auxiliary are the same
  sets as in Test 81 and the installed build respectively; the lease split adds
  up (worker 0x12000, map and region page, auxiliary 0x7000, data 0x2d000 =
  0x47000).
- The Test 82 targets pass as committed: toy-wire-host-tests, the whole-reader
  integration test (359,154,001 checks, titles_lent on three buses),
  toy-wire-package-test with no-report, toy-wire-linked-check.

## What the reader ran

- make -n for the three recipes with BUILD_ID=aaaaaaaaaa82, compared per
  target with a script: worker commands identical installed/82; resident, legacy
  and auxiliary commands identical 81/82
- make ... TOY_WIRE_VMU=1 build/t82/retail-toy-test-82.kui; Test 81 into
  build/t81; Makefile.vmu-native-background-compact into build/inst: all built;
  parts compared by sha256 as reported
- make -f Makefile.vmu-native-background-compact BUILD_ID=f8807cb16561
  BUILD=build/instf: worker and .reloc equal the recorded delivered hashes
- Tests 79 and 80 (and 81, installed) built at b604dea and at b604dea~1 with
  one label: identical; Test 79 with BUILD_ID=9f183a7f72f5: sha256
  557822ec...47be as recorded
- Preprocessed (-E -P) every worker, stage, resident and auxiliary source
  under each recipe's exact flags and diffed them
- Layout probes: wrapper translation units compiled with the exact flags of
  the worker, resident, stage and auxiliary objects, sizeof/offsetof read from
  the assembly: results as reported
- make ... TOY_WIRE_VMU=1 toy-wire-host-tests: pass;
  toy-wire-integration-host-test: PASS; toy-wire-package-test and
  toy-wire-linked-check on build/t82: pass
- Own host program (admit.c, with src/core/runtime_image.c, native_session.c,
  vmu_native_cache_seed.c, vmu_game_card.c, vmu_persist_profile.c) on the three
  modules: On and Off admission ok for all
- tests/test_toy_wire_stream_mutations.py restricted to the scan's entries
  (TMPDIR inside my directory): 14 of 14 caught with TOY_WIRE_VMU=1, 12 of 12
  without
- Planted D1 (stage keeps the VMU-off stop), D2 (a Toy flag reaches the
  worker), D3 (auxiliary flags not fixed) in a copy: D1 and D2 pass every Test
  82 target, D3 fails only toy-wire-linked-check
- Experiment E1 in the copy: the wake call at
  tests/test_toy_wire_integration.c:2489 removed: the integration test aborts at
  line 2480 in sequence_pending(2)
- Switching TOY_WIRE_VMU 0 to 1 and back in build/sw: both results identical
  to clean builds
- make -n with TOY_SUITE_MODE=66 TOY_WIRE_VMU=1 and no stream: accepted,
  builds retail-toy-test-66.kui; TOY_WIRE_VMU=2 and Test 80's switches with
  TOY_WIRE_VMU=1: refused as intended
- python3 tools/check_native_background_compact.py build/inst --allow-dirty:
  refused ('controller witness instructions/targets changed:
  _reader_readahead_exports'), so the tree's linked audits could not be used on
  either build
- Deepest static stack paths from resident-sci.elf.ltrans0.ltrans.ci of
  build/inst and build/t82 for the lease path
- git status in /home/user/K-UI-NeXT at the end: clean, HEAD b604dea;
  everything I wrote is under <scratch>/review82/stage/

## Not examined

- The loan at run time inside the reader and the stream (quiesce,
  toy_ahead_clear, toy_wire_halt, CMD12 and the card's busy time, the SCI
  registers given back at register level, timing with interrupts masked):
  outside my lens; I only compared the loan functions' text with the installed
  residents' and read the fixture's scenario.
- The other 337 entries of the mutation list for Test 82: I ran only the 14
  for the stage's scan.
- The installed GUI's own admission code: it is not in this tree; I used this
  tree's scanning functions on the module bytes.
- The tree's linked audits (worker stack proof, gateway and witness checks)
  for this composition: the audit refuses even the installed recipe's build at
  this commit, so the stack figure above is my own reading of the callgraph for
  the low share only, and I did not walk the worker's own frames.
- Console or emulator behaviour, and anything needing a game executable or
  disc image: none available.
- The Legacy (eager) reader launched through this module with the VMU on: by
  reading it takes the installed path with HEAD's legacy resident, which is
  byte-identical to the installed recipe's at this commit; not run.
- The installed recipe at this commit no longer reproduces the delivered
  f8807cb low residents and auxiliary (same sizes, different hashes; only the
  worker matches). That predates this commit and I did not look into it.
- Whether Tests 80 and 81 have console results: the tree records none (no
  document or evidence directory for them). Test 82 contains both changes and
  has no report, so a symptom on its first run could not be attributed to the
  VMU composition alone.
