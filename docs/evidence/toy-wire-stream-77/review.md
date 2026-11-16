# Test 77: the review as returned

Three independent readers were asked, each with one angle, the saved diff
of the change and the code around it. Two returned. The third, on the
tests of the new behaviour, was cut short by the session's usage limit;
what its unfinished experiments had shown is at the end.

Each reader's summary and findings are as returned, shortened nowhere.
"Done" under a finding is the author's.

## First reader: everything outside the new behaviour

Angle: everything outside the new behaviour. I reviewed the saved diff
(change.diff) on private copies of HEAD and of the working tree under
scratchpad/t77/rev-outside; nothing was run or changed in /home/user/K-UI-
NeXT.
Result for the four things that had to hold: (1) Builds without the switch are
unchanged. The auxiliary, the stream source and the entry assembly preprocess
to the same tokens as HEAD in every configuration I tried; the worker differs
only in two constant expressions that fold to the same value, and its objects
are byte-identical. (2) The Makefile does the same as before for every
variable set without TOY_WIRE_SEQ; the low residents get no wire flag. (3) The
report struct and the kept state fit, the rows agree with the test, nothing
else reads the struct. (4) The mutation lists are exactly HEAD's without
TOY_WIRE_SEQ, and every entry of Test 77's list is unique in its file and
compiled.
Seven findings, none of them in a build without the switch and none in the
behaviour of the default Test 77 module. Four are gaps in tests or checks: the
integration host test fails for Test 77 without STAY in the saved diff; the
mutation script builds its stream unit test without GAP when GAP is asked for
under SEQ; the report test's "full run" values are not consistent with each
other, and made consistent the last row no longer fits; TOY_WIRE_SEQ=1 without
TOY_WIRE_STREAM=1 is silently ignored. Three are wording: the worker's screen
title, the Test 76 decoder against the new rows, three comments.
One thing to know when reading this: the working tree moved after the diff was
saved. As of 21:56 UTC only tests/test_toy_wire_integration.c differs from the
saved diff (95 lines: guards on STAY, a new scenario sequence_lent). Every
source, header, Makefile and other test is still the saved diff's. The first
finding is about the saved diff's state of that file.

### 1. Saved diff: the integration host test fails for Test 77 built without STAY

- Severity: test gap; confidence: shown from the code
- Where: tests/test_toy_wire_integration.c:2279 in the saved diff (sequence_scattered, change.diff line 743). STAY=0 is a part-off variant, not the configuration under test. Related resident code, compiled in this configuration: src/loader/retail_async.c:1379-1400 and 1437-1443.
- Trigger: make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_STAY=0 toy-wire-integration-host-test
- Consequence: The run aborts on `c->stats.hit_requests == requests - 1u && w->other_windows == 1u && !c->stats.abandoned` (make exit 2). I reproduced it on a copy of the saved-diff state; the developer's own matrix shows the same exit 2. So a part the Makefile says can be left out has no passing host test. The mutation script never runs the unmutated tree, so with TOY_WIRE_SEQ=1 TOY_WIRE_STAY=0 it would report all 201 entries caught whatever the mutants do. The cause is the reader's rule, not only the test: every EXEC and CHECK of an ordinary read reaches the window as a waited call, because the resident drops the wait only for reads on its movie path; without STAY such a read gives its window up at its second call. The working tree's file was changed after the diff was saved: that check is now `if(!mean && KUI_TOY_WIRE_STAY)` and several sequence_* scenarios sit inside `#if KUI_TOY_WIRE_STAY`; the developer's later log shows the run passing. I did not review that later revision, beyond checking that it preprocesses to the same tokens as the saved diff's for builds without the switch (Tests 68, 75, 76).
- Suggested: Keep the STAY guards that are now in the working tree. Let the mutation script build and run the unmutated tree first and stop if that fails.
- Done: Fixed in the fixture before the commit. The scenarios of ordinary reads that have to wait for the stream are run only in builds with STAY, which is their premise, and `sequence_waited()` shows what a build without it does. The mutation script now runs the tests without a mutation first and stops if they fail.

### 2. Mutation script: with TOY_WIRE_SEQ=1 and TOY_WIRE_GAP=1 the stream unit test is built without GAP

- Severity: test gap; confidence: shown from the code
- Where: tests/test_toy_wire_stream_mutations.py:695-710 (DEFINES), used by the direct build at 750-752; the default it meets is include/kui/toy_wire_report.h:199. GAP=1 is not the configuration under test.
- Trigger: TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_GAP=1 python3 tests/test_toy_wire_stream_mutations.py
- Consequence: GAP's five mutations join the list (217 entries), but DEFINES gets -DKUI_TOY_WIRE_SEQ=1 and no -DKUI_TOY_WIRE_GAP=1, and under SEQ the header's default for GAP is 0. The mutated lines are not compiled in the script's direct `cc` build, so that stage passes for each of the five, and it tests a stream without GAP for every other stream mutant too. The integration stage is built by make, which takes TOY_WIRE_GAP=1 from the environment and passes -DKUI_TOY_WIRE_GAP=1. I ran the five in a scratch copy: all five are caught, by the integration test alone. Nothing is reported missed, but the stream unit test does not check the build it is run for. The rule is right in the other two cases: without SEQ it is HEAD's, and with SEQ and GAP not asked for both lists say GAP=0, as the Makefile does.
- Suggested: Where SEQ == "1" and GAP is on, append "-DKUI_TOY_WIRE_GAP=1" to DEFINES and "TOY_WIRE_GAP=1" to SWITCHES.
- Done: Fixed: with `TOY_WIRE_SEQ=1` and `TOY_WIRE_GAP=1` the script says GAP in its own defines and switches.

### 3. Report test: Test 77's "full run" values fit only because the counts that go with its window count are left small

- Severity: test gap; confidence: shown from the code
- Where: tests/test_toy_wire_report.c:339 with 252, 256, 360-361, and the assertion at 392. Compiled in this configuration (line 339 is under KUI_TOY_WIRE_SEQ).
- Trigger: The test sets other_windows = 0x2F00 but leaves windows given up for a read at 6, streams closed at 3, streams begun at 0x1F and the low reader's seek blocks at 0x2F. Each of 0x2F00 windows is at least one stream begun behind a block of the low reader, and is given up and its stream stopped.
- Consequence: At the test's values the body is 1004 of 1012 bytes with every row, 8 bytes to spare (Test 76: 1001). With those four counts set to go with 0x2F00 windows, the production formatter (run in a scratch copy) leaves out the last row, TR, the reader's time by piece of the second movie. The 21 rows the test calls essential still fit (984 bytes). So `rows == LABELS` at line 392 does not show that every row survives the run the test describes. For the console this is an estimate on assumed counts, not a measurement: Test 76's real report was 980 bytes; rewritten to Test 77's rows I get about 978 with no other window, about 990 with 0x100 and about 1000 with 0x800. TR would go only in a run with several thousand windows. TP is already gone in Test 77, so TR is the one row left for comparing the reader's time by piece with the Test 76 baseline.
- Suggested: Set drops[OTHER], stops[CLOSE], n.starts and seek_blocks to go with other_windows, and have the test say which row may be given up; or free a few bytes (a shorter label) so that TR fits at those values.
- Done: Changed. The first movie's player row is written last, so that it is the row to go for room. The test's full run has counts that go together (12,032 windows opened by ordinary reads, as many streams and stops) and checks that every row through `TR` is written there; with the movies' windows alone every row fits (999 of 1,000 bytes at full-run values; Test 76's real report was 980).

### 4. TOY_WIRE_SEQ=1 without TOY_WIRE_STREAM=1 is silently ignored

- Severity: test gap; confidence: shown from the code
- Where: Makefile.vmu-native-toy-suite:63-65 (only TICK and CACHED are refused without STREAM); SEQ's checks at 290-307 lie inside the STREAM block (109-316); the config line at 339.
- Trigger: make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_SEQ=1 (TOY_WIRE_STREAM=1 forgotten), or TOY_WIRE_SEQ=1 with any other mode.
- Consequence: No error. A dry run shows no KUI_TOY_WIRE flag on any command: apart from the config stamp it is the plain Test 66 build, written to build/toy-suite-66/retail-toy-test-66.kui, which is the name Card Tools loads. The stamp toy-suite-config records 'TOY_WIRE_SEQ=1' all the same. The host test targets without STREAM build Test 68's tests. WORDS to FLUID already behave this way; SEQ joins them. No documented command of an earlier test is affected. With STREAM=1 the switch is never ignored: it gives Test 77 or one of the errors (SEQ without FLUID, SEQ not 0 or 1, GAP without RING).
- Suggested: Extend the test at line 63 to every wire switch, SEQ included.
- Done: Fixed for this switch: `TOY_WIRE_SEQ=1` without `TOY_WIRE_STREAM=1` is refused. WORDS to FLUID behave as before.

### 5. The worker's screen pages call a Test 77 module "TOY TEST 75"

- Severity: wording; confidence: shown from the code
- Where: src/loader/vmu_native_passive.c:1025-1043, used at 1054 and 1135-1138. Not touched by the diff; compiled in this configuration (the STEADY branch at 1029-1030 is taken).
- Trigger: Any Test 76 or Test 77 build: STEADY is set and there is no FLUID or SEQ branch. The pages are shown after every run and are the only record when the save fails.
- Consequence: The saved file says TEST 77 (src/loader/toy_suite_report_save.inc:541, compiled), the screen says TOY TEST 75. A filmed screen cannot tell Test 77 from Test 76 or 75, nor from an older alternate module in the same package; the pages carry no build label. Test 76 was delivered this way.
- Suggested: Add a SEQ branch ("TOY TEST 77") ahead of the STEADY one. Adding a FLUID branch as well would change Test 76's worker bytes and with them its recorded module hash.
- Done: Fixed for Test 77: its worker's pages say TOY TEST 77. Test 76's worker is left as delivered.

### 6. Report: HIT and OUT count every window, B and I only the movies'; the Test 76 decoder divides one by the other

- Severity: wording; confidence: shown from the code
- Where: src/loader/toy_movie_ahead.inc:416-434 and src/loader/toy_suite_report_save.inc:88-103 (both compiled in this configuration); docs/evidence/toy-wire-stream-76/decode_report.py:41-42 and 48.
- Trigger: A Test 77 report with OS above zero, read with the Test 75/76 decoder. It is the only thing in the tree besides the worker and the two tests that reads the rows.
- Consequence: The decoder takes the new layout without error: it finds rows by prefix and already treats CAP and TP as optional. It ignores OS and the X row. Two of its figures come out wrong: "sectors each" = (B+I)/HIT too small and "us a sector" = OUT/(B+I) too large, because hit_requests and publish_ticks are counted for every request served, from any window, while B and I take only sectors from windows opened inside a movie's file. The report has no count of requests served from other windows, so the two classes cannot be separated from it. Sectors follow the window, not the request: a window opened inside a movie's file counts later reads behind the file under that movie, and one opened ahead of a file counts the movie's sectors under OS. The formatter's comment says this and sequence_file pins it.
- Suggested: When Test 77's result is recorded, take B+I+OS as the sectors served and decode the X row; say in the document that HIT, EDC, STOP and OUT are totals over all windows.
- Done: Recorded in the document (The report). The decoder for Test 77's rows is written when its console report is recorded.

### 7. Three comments of the diff say a little more than the code does

- Severity: wording; confidence: shown from the code
- Where: include/kui/toy_wire_report.h:153-154; src/loader/toy_movie_ahead.inc:538-539; Makefile.vmu-native-toy-suite:297.
- Trigger: Reading the comments against the code.
- Consequence: (a) "The files and the player's word decide nothing about the window any more." With TOY_WIRE_OPEN=0, which the Makefile accepts with SEQ (its host test passes on the saved diff), src/loader/toy_movie_ahead.inc:516-521 is compiled and refuses the window to reads in the second movie's file until the player's hold is over. Those lines are not compiled in the configuration under test. In every build the files still decide, through the resident's movie path, whether a waiting request's calls reach the window as waited calls (first finding). (b) "outside the movie files the resident reports only from the game's next call." The resident's bit is set only from 386 sectors into a file (src/loader/native_readahead_aux.c:566-567, compiled), so a file's first 386 sectors are in the same case. (c) "GAP is out by default here and so is not said." The plain Test 77 command line carries -DKUI_TOY_WIRE_GAP=0, said by the FLUID block at lines 273-275; it is in the developer's build log.
- Suggested: Reword (a) to name the exceptions; add "or in a file's first 386 sectors" to (b); in (c) say that this block adds nothing for the default.
- Done: Reworded: the header's account of what the files still decide, the comment on the resident's movie path, the Makefile's note on GAP.

### Cleared

- Builds without the switch, token streams. Preprocessed at HEAD and in the change with the console flags of the developer's build log: src/loader/native_readahead_aux.c, src/loader/toy_wire_stream.c and native_readahead_aux_entry.S are identical in 30 wire configurations (Tests 68 to 76, each part of 74, 75 and 76 left out, LATER=1, SEQ=0 said). The auxiliary and the worker are identical in all 44 suite modes without the stream, with TOY_FMV_COMPARE, and in four profiles without any suite mode. The auxiliary as the host tests compile it is identical in 25 wire configurations and 38 suite modes.
- Builds without the switch, the worker. src/loader/vmu_native_passive.c differs in wire builds in two constant expressions only: the test number chain gains `0?77u:`, and from Test 76 on the PT value gains `| 0 << 8` and `&& !0`. Compiled without -g at HEAD and in the change for Tests 68, 75, 76 and 76 without GAP, the objects are byte-identical.
- Changed defaults. For SEQ undefined or 0, KUI_TOY_WIRE_GAP and KUI_TOY_WIRE_PARTS have their old values. GAP is used outside #if only inside PARTS, and PARTS only in the worker's PT value.
- Functions. suite_movie is still defined for every mode that calls it (native_readahead_aux.c:554, 675, 687, 718 and toy_suite_burst.inc:441). suite_raw and suite_file exist only under STREAM && SEQ, and include/kui/native_readahead_layout.h:35 makes STREAM mean mode 66, where suite_demand uses both. toy_ahead_live is defined exactly where it is used. 43 syntax-only compiles with -Wall -Wextra -Werror pass for Test 77 with each part left out (console and host auxiliary, the integration test, worker, stream, report and stream tests).
- Residents. retail_async.c, sci_stream.c, retail_gd.c and retail_cursor.c contain no KUI_TOY_WIRE name. `private FLAGS := $(FLAGS)` at Makefile.vmu-native-toy-suite:117 is taken before any wire flag is added and LEGACY_FLAGS is fixed at line 71. In a dry run of Test 77 and in the developer's build log the resident and legacy objects carry no wire flag.
- Makefile without SEQ. 51 dry runs (Tests 68 to 76 with all four host targets, parts left out, GAP=1, SEQ=0 said, 13 other modes, FMV compare, the liveness, movie and witness host tests) are the same as HEAD's apart from one extra blank in the config recipe, which writes the same file. Five old error cases and GAP=1 with RING=0 give the same messages; only the Makefile's line numbers differ.
- Makefile with SEQ. The test number is 77 (the SEQ line stands before 76's), the directory build/toy-suite-77, the module retail-toy-test-77.kui. The config line shows GAP's value and 'TOY_WIRE_SEQ=1'. -DKUI_TOY_WIRE_GAP=0 is passed by default and -DKUI_TOY_WIRE_GAP=1 from the SEQ block when asked. The auxiliary, stream, worker and all three host tests receive the flags. Errors come for SEQ without FLUID, with TICK, for SEQ=2 or empty, for GAP=2 and for GAP=1 with RING=0.
- Report layout. The struct grows by two words at its end: 720 to 728 bytes (measured with a size probe compiled from the preprocessed auxiliary). The kept state grows from 920 to 928 of the 1024 bytes before the ring; the static assert at src/loader/toy_wire_ahead.inc:145 guards it. `on` lies directly behind the report (the size is a multiple of four), which is what stream_on() reads. toy_wire_reset zeroes the new words with the rest. All of this is compiled in the configuration under test.
- Report rows. No row has more than six values and no label reaches 40 characters. The order is the test's label order. The header says TEST 77. PT is 1BD by default and gains 40 with GAP. The X row skips the unused "movie ended" count and maps E, I, C, W, O as the test's 1 3 4 35 6 shows. Nothing under tools/ or tests/ reads the struct or the rows with a fixed layout; the one text decoder is the Test 76 evidence script (sixth finding).
- Mutation lists without SEQ. In 3,360 environments (the documented commands, every switch at 0 and 1, every pair of parts, on eleven chains, and TOY_WIRE_SEQ=0 said) MUTATIONS, SWITCHES and DEFINES are equal at HEAD and in the change. For eleven earlier configurations (30 to 190 entries) every entry's text still occurs once in the changed sources and still reaches the compiler.
- Mutation list with SEQ. 212 entries, each text once in its file and each compiled in both builds the script makes. With QUICK, STAY, both, OPEN, CLEAN, MSB, QUIT, COPY, APART or HOT left out, or LATER on, no entry is on uncompiled lines; the counts 202, 201 and 191 are what the filters should give. SEQ_GONE's four names are exactly the entries on the #else lines at src/loader/toy_movie_ahead.inc:476 and 511. The one remark, five APART entries not compiled with RING=0, is the same at HEAD without SEQ.
- The window's variant 5 in unchanged code. Every `(A.variant-1u)/2u` is either behind the AHEAD_OTHER test (toy_movie_ahead.inc:432-433) or not compiled in this configuration (toy_movie_ahead.inc:148 without SEQ only, 284 without the stream only, toy_wire_ahead.inc:284 without LEAN only). All other uses test it for zero.
- The value handed on to unchanged code. `variant` as suite_demand now forms it (suite_raw, then suite_file) is the same conjunction as suite_movie's for modes from 26 on, so toy_suite_active, in_play and toy_resume_deliver get what they got.
- Comments checked and found right: the resident reports from the interrupt only under its movie bit (retail_async.c:1678); position_lba is the last sector delivered and SEEK moves it (retail_gd.c:571, 616, 699 to 922); the low reader "begins nothing but reads" (reads() at retail_async.c:987-998 is PIOREAD or DMAREAD in this build); the track's end_lba is exclusive; the "movie ended" reason is never counted under SEQ; the mutation script's three "no mutation" notes.
- Other records of the changed files. docs/evidence/updater-preserved-main-source.json and tools/package_vmu_native_dual_delivery.py:308 pin an older native_readahead_aux.c and were already out of step with HEAD. No CI workflow builds the toy suite. No tool or test lists test numbers or module names.

### Not verified

- Nothing here is a console result. No module was built and no module hash of Tests 68 to 76 was compared; the object comparison covers the worker in four configurations, and the other sources are the same token for token.
- I did not run Test 77's whole mutation list. I ran only GAP's five with GAP asked for. The developer's logs show entries 181 to 211 caught. For the 181 inherited entries I checked that they are compiled under SEQ, not that a test still fails on them.
- Host tests I ran myself, on a copy of the saved-diff state: the integration test for Test 77 without STAY (fails) and without OPEN (passes), the stream unit test with the script's SEQ defines (passes), and the report test for Test 77 (passes). The other pass results I cite are from the developer's matrix log.
- The later revision of tests/test_toy_wire_integration.c in the working tree (the STAY guards, sequence_lent) is not part of the diff I reviewed. I checked only that it is unchanged for builds without the switch in three configurations.
- The sizes I give for a real Test 77 report are Test 76's console values rewritten to the new rows plus assumed counts for ordinary loading.
- toy-movie-host-tests receives the SEQ flags through TOY_MOVIE_TEST_FLAGS, as it already receives FLUID's. I did not examine that target with wire flags.
- Whether a mutant is still caught in part-off variants of Test 77 (for example the reason counts without STAY) was not checked.

## Second reader: the window's logic with the switch on

Read-only review of the window's logic with KUI_TOY_WIRE_SEQ on (mode 66 with
the Test 77 switches). Nothing under /home/user/K-UI-NeXT was changed. I
walked toy_movie_ahead for every visit source and tried the suspect paths on
the host fixture in scratch copies (my scenarios:
<scratch>/t77/rl/rl_scenarios.inc; a copy with two trial changes:
<scratch>/t77/rlfix).
The rules the switch adds hold. I found no path of theirs on which a request
waits for ever or is given other sectors' bytes, no index out of bounds, no
false return with a window open, and no wrap in the give-up test. Random walks
of game behaviour over the fixture (4,000 seeds) produced no wrong byte and no
failed read.
Three things are worth deciding before the console run:
1. A hang that is older than this change (it reproduces without the switch): a
token search the low reader left pending when the game aborted a movie-path
read is carried on by the next window, and the low reader's reception that
follows is never ended. The switch changes who can be the second read (any
consecutive read), not the mechanism. A one-line guard in the pump removed it
in my copy.
2. A single failed CMD12 when a window is given up for a read fails that read
with no retry, and off the movie path that stops the console. The branch is
old, but it used to run about twice a console run and is now how most windows
end.
3. A filled window now leaves its stream standing until the game reads
elsewhere, with DMA channel 1's registers and a timer channel still the
stream's; restarting or ending it writes both without looking. It only matters
if the game uses them in play, which I do not know.
Costs to expect in the X row: a command that is not a read, or a read of more
than 60 sectors, between consecutive reads costs a CMD12 and a new CMD18 each
time.
The tree moved while I read. The reader's code is still what the saved diff
has (one comment in toy_movie_ahead.inc was reworded, which moves its later
lines down by one; I cite the tree as of 22:21 UTC).
tests/test_toy_wire_integration.c, the mutation script, the Makefile,
toy_suite_report_save.inc and vmu_native_passive.c have changed since the diff
was saved; I used the fixture as of 21:35, which has sequence_lent.

### 1. A token search the low reader left pending is carried on by the window's first reads, and the block reception that follows is never ended (older than this change)

- Severity: wrong data or hang; confidence: shown from the code
- Where: src/loader/toy_wire_ahead.inc:501 and 518-525 (toy_wire_seek), 891-957 (toy_ahead_pump: nothing polls the low reader while the stream is off); src/loader/sci_stream.c:739-741 and 765; src/loader/toy_movie_ahead.inc:590 (the opening test asks ops->busy() only); src/loader/retail_async.c:1654-1688. All compiled in this configuration; the resident lines are Test 66's and unchanged.
- Trigger: The resident's stream stands in a pending token search (state 5) for card block b, token_polled 0, with no request. A read on the low reader's movie path (toy_suite_active set, so it looks for the token 64 bytes a visit) leaves that state when the game ends it with ABORT, INIT or RESET before the token has come; reads off that path search without the limit and leave nothing. Then a read opens a window whose seek asks the low reader for block b: with the switch, any read of at most 60 sectors at position_lba+1. Host reproduction: a movie file over the track; read 45020 x12; next token late; request 45032 x61 (too large for a window); ABORT, CHECK; request 45032 x13.
- Consequence: kui_sci_stream_fetch(b, ..., polled=true) continues the pending search with the earlier caller's polled=false and starts the low reader's own DMA reception (stream state 2). toy_wire_seek returns at once while the low reader is busy, interrupt visits never seek, and the stream build's pump never calls ops->poll, so that reception is never ended. Each SCI interrupt is returned as handled with the receiver's overrun still set, the window verifies nothing, and the request stays pending. On the model it was still pending after 20,000 interrupt visits and 4,000 recognised EXEC polls (wait 0, the way a read in a movie file is polled). Polled by EXEC/CHECK the resident does not recognise (wait nonzero), STAY gives the window up after its quarter second, kui_sci_stream_stop ends the reception and the low reader completes the read, provided those calls run at all; on the console I expect the unanswered interrupt to keep them from running (not verified). Random walks with aborts over a track that is all movie file ended this way in 39 of 1,200 seeds and in no other way; walks without aborts, or with ordinary reads only (aborts included), in none of 2,800. Not introduced here: the same sequence hangs a build without the switch while the movie plays. The switch changes which second read qualifies: any consecutive one, movie playing or not.
- Suggested: In toy_ahead_pump, when the stream is off and ops->busy(), end the low reader's reception with ops->poll(r->engine.in_irq!=0) before the seek. The block then stands ready for the seek's take, and an interrupt visit removes its own cause. Tried in a scratch copy together with the change of the next finding: the reproduction completes in 6 rounds on one CMD18, all 1,200 abort seeds end well, and the whole fixture still passes. Alternatively refuse a window at toy_movie_ahead.inc:590 while r->shared.stream.state is beyond RA_RESUME.
- Done: Acted on, for builds with the switch: no window is opened while the low reader stands in a wait of its own (`toy_movie_ahead`, the opening test). The reader's own reproduction is the scenario `sequence_pending()`, with three ways of polling; a mutation that removes the test is caught. Builds without the switch are not changed and keep the fault (Known limits, 6). The reader's other remedy, ending the low reader's reception in the pump, was not taken: it changes a path every window runs through.

### 2. One failed CMD12 when a window is given up for a read ends that read with an I/O error, which off the movie path stops the console; that is now how most windows end

- Severity: failed request or lost function; confidence: shown from the code
- Where: src/loader/toy_movie_ahead.inc:160-163 (toy_ahead_clear), 445-457 (toy_ahead_fallback, e->failed at 453), reached from 542-554; src/loader/retail_resident.c:1719-1724. Compare src/loader/retail_async.c:870-874 with failure() at 444-457, where the low reader tries a failed stop again, up to eight times. All compiled in this configuration.
- Trigger: A window is open. In one of the game's calls a read arrives that the window does not serve (elsewhere, more than 60 sectors, over the track's end, whole 2,352-byte sectors), or a waited read's STAY runs out; kui_sci_stream_stop then returns anything but OK once. On the host: a window open, read 45200 x3, the first CMD12 answered with an error bit.
- Consequence: toy_ahead_clear returns false, toy_ahead_fallback sets KUI_GD_ERROR_IO and the read ends KUI_GD_FAILED with no second try (host: status -1, error 1; the same injection with no window: the low reader retries and the read completes). Off the low reader's movie path toy_suite_active is 0, so the resident's dispatch then enters its failure report, which does not return. The branch is old. Up to Test 76 it ran for a read elsewhere while a movie played: two windows in a console run by docs/toy-wire-stream-test-75.md:410-412. With the switch it runs each time ordinary loading leaves a consecutive stretch. How often a CMD12 fails on the console's card I cannot say.
- Suggested: When the stop fails with a request active, close the window all the same and return false. The stream is already halted and the resident's stream state is LOST, so the low reader's own fetch ends the card's command before it reads, with its retries. Tried in the scratch copy: the injected failure is absorbed (2 CMD12, read exact) and the whole fixture still passes.
- Done: Acted on as suggested, for builds with the switch: when the stop fails with a request active and no reception under way, the window is closed and the request left to the low reader (`toy_ahead_fallback`). Scenario `sequence_stop_failed()`; two mutations, both caught.

### 3. A stream standing still under a filled window keeps DMA channel 1 and a timer channel until the game reads elsewhere, and writes both without looking when it goes on or ends

- Severity: failed request or lost function; confidence: guess
- Where: src/loader/toy_wire_stream.c:268-299 with 250-264 (wire_arm programs channel 1 with no look at it; only kui_wire_start at 539-542 looks), 276-279 (state FULL), 574-579 (timer, IPRA) and 618-620 (saved registers written back) in kui_wire_stop. Reached for longer through src/loader/toy_movie_ahead.inc:541-555 and 467-480, where the window no longer ends with the movie. All compiled in this configuration.
- Trigger: An ordinary sequential load opens a window and stops reading. The window fills (61 sectors), the ring fills, and the stream goes to KUI_WIRE_FULL: CHCR1 is 0 with the stream's SAR1/DAR1/TCR1 left in place, its timer channel is still at level 1 in IPRA with the SDK slot on the reader's callback, and the resident's stream state is still DMA, so the SCI level stays the reader's. Nothing ends this but the game's next read elsewhere, a command that is not a read, INIT, RESET, an abort or a loan of the card. In between, the game programs channel 1 or starts that timer for itself.
- Consequence: The next read at the window's first sector makes room and wire_arm programs channel 1 over the game's values. The next read elsewhere ends the stream: kui_wire_stop stops the timer (TSTR bit, TCR, IPRA field) and writes SAR1 to CHCR1 back to what they were when the stream began. On the model both happen as read: the game's SAR 0c500000, DAR 0c600000, TCR 1234, CHCR 5440 were replaced, TSTR went from 04 to 00 and IPRA from 0040 to 0000. The low reader gives the channel back after every block (sci_stream.c:805-806) and asks channel_idle() before each (sci_stream.c:617); up to Test 76 a stream lived only while a movie played. The register behaviour is shown. Whether Toy Commander uses channel 1 or the chosen timer during play is a guess: I only know that TMU2 was not free on the console and the stream took TMU1 (docs/toy-wire-probe-test-67.md:169).
- Suggested: Do not leave a stream standing without bound. When the window has stood filled with no request for some time (the idle rule's clock is there), end the stream through toy_wire_halt and keep the verified sectors; the pump reopens it by a seek once the window has room. Also let wire_arm and the restore in kui_wire_stop check the channel as kui_wire_start and wire_capture do.
- Done: Not changed. It is Known limits 1, with the reader's account of the registers, and its remedy is named there as owed before this reader meets another game. Whether Toy Commander takes channel 1 or TMU1 in play is not known; Run 2 of the console test is where it would show.

### 4. A command that is not a read, or a read of more than 60 sectors, between consecutive reads costs a CMD12 and a new CMD18 each time, where the low reader alone keeps the card's command open

- Severity: cost only; confidence: shown from the code
- Where: src/loader/toy_movie_ahead.inc:542-545 (given up for a command, and for a request not selected), 503 (60 sectors), 588-593 (the next consecutive read opens again); src/loader/toy_wire_ahead.inc:845-846 (STAY refuses without the expected clock) with src/loader/retail_async.c:1396-1405 (every EXEC and CHECK of a read off the movie path carries a nonzero wait, toy_suite_poll being set only under toy_suite_active). All compiled in this configuration.
- Trigger: Host fixture, 17 consecutive reads of 13 sectors polled by EXEC and CHECK. Plain: 1 CMD18 and no CMD12 with or without windows, 16 reads served from one window. With a NOP command after each read: 16 CMD18 and 16 CMD12 with windows, 1 and 0 without. 13 and 61 sectors in turn (6 reads): 3 and 2 against 1 and 0. With the clock not the expected one: 17 and 16 and nothing served from a window, against 1 and 0.
- Consequence: Each such read pays the CMD12 (its busy wait is unbounded and runs with interrupts masked inside the game's call), a CMD18 with its token wait and a block by programmed transfer, and the stream's read-ahead is thrown away. The data was exact in every case. The X row's C, O and W will show on the console whether Toy Commander's loading does any of this; I do not know that it does.
- Suggested: None of the commands that are not reads uses the card in this build (src/core/retail_gd.c:582-621), so the window need not be given up for them, or only for SEEK. A read that reaches over the track's end could be refused a window at toy_movie_ahead.inc:588 instead of being given one that its next call gives up.
- Done: Not changed. Known limits 2 has the reader's counts. The report's `X` row will show whether the game's loading does this.

### 5. The X row: W and STAY now take in ordinary polled reads, a read over the track's end is counted as waited or as other by call order, and windows ended by INIT, RESET, abort or a loan are in no column

- Severity: wording; confidence: shown from the code
- Where: src/loader/toy_movie_ahead.inc:550-553; src/loader/toy_suite_report_save.inc:88-103; src/loader/native_readahead_aux.c:1459-1463 and 812-823. All compiled in this configuration.
- Trigger: Read 45260 x5, then 45265 x37 on a track that ends at 45300, with CHECK as the request's second call: drops[WAITING] 1, drops[OTHER] 0. sequence_track_end expects OTHER only because plain_read calls EXEC first, which still has the kick.
- Consequence: W means 'given up at a later call of a request that carried a wait', and off the movie path every EXEC and CHECK carries one, whatever the cause was. The STAY count rises with nearly every ordinary read that had to wait for the stream. Reading W as 'waited in vain' or STAY as 'the game's own waiting loop' would mislead when the console report is read, and the reasons do not add up to the windows opened. The header comment was reworded while I read and now mentions the wait; the row's comment still calls W a waited request.
- Suggested: Name the cause by what failed (place and extent before the wait), or say in the row's comment what W and STAY mean from Test 77 on.
- Done: Acted on in part. `W` is counted only where the window had the sectors to give, so a read over the track's end is `O` whichever call finds it (`sequence_track_end()` with the game's CHECK first). The document says what `STAY` counts from Test 77 on and that a window ended by INIT, RESET, an abort or a loan is in no column.

### 6. The fixture reaches none of: a pending token search under a window, a failed CMD12 at a give-up, the controller's wake, a foreign use of channel 1 or the timer while the stream stands still

- Severity: test gap; confidence: shown from the code
- Where: tests/test_toy_wire_integration.c:331 (every scenario opens the card with its first token after one byte, so the low reader's token, stop and start waits never stay pending); 1299 (quit_foreign covers a running run only); the file never calls kui_retail_native_demand_wake.
- Trigger: The whole fixture gave the same 268,581,007 checks unmodified, with the pump guard of the first finding, and with both trial changes: it runs through neither branch.
- Consequence: Nothing holds the paths of the first three findings, and mutations there would survive. For what it is worth, the sequence scenarios also pass in my copy on a slower card (first token after 385 and 900 bytes, gaps of 90, busy 50), and random walks that include the controller's wake found nothing beyond the first finding.
- Suggested: rl_livelock, rl_cmd12 and rl_parked in <scratch>/t77/rl/rl_scenarios.inc show one way to set each state up on the existing model.
- Done: Two of the four added: the token search left standing and the failing stop command. The controller's wake is used by `sequence_pending()`'s third game and has no scenario of its own: off the movie path it refuses, and on it nothing is changed. A foreign use of channel 1 or the timer under a standing stream has no scenario: Known limits 1.

### 7. After the card has been lent once, the player's update call no longer pumps: C.quiesced is set and never cleared (older than this change, no effect in this module)

- Severity: cost only; confidence: shown from the code
- Where: src/loader/native_readahead_aux.c:815 (set, also when quiesce() then refuses), 1488 (KUI_NATIVE_RA_MOVIE_AHEAD refused), 1640 (cleared only by KUI_NATIVE_RA_RESUME, which nothing sends). The other two resets, at 778 and 1627, are not compiled with KUI_TOY_SUITE_ASYNC_DEMAND.
- Trigger: Any QUIESCE, that is kui_retail_native_async_loan_begin at src/loader/retail_async.c:1116.
- Consequence: Every later update-hook visit returns without reaching toy_movie_ahead. In this module the only loan is the report's save at the end of a run, so nothing is lost today. It matters once a loan can come before or during a movie (the plan's step 6), and the new sequence_lent scenario reads on after a loan without looking at it.
- Suggested: Clear it when a window is next opened (toy_ahead_begin) or when the loan ends.
- Done: Not changed, older than this change: Known limits 6.

### Cleared

- Give-up test (toy_movie_ahead.inc:544-545, compiled): A.cursor.lba+A.cursor.count stays the track's end through toy_ahead_begin, toy_wire_rewind (also when its kui_retail_cursor_begin fails, which writes lba and count first) and kui_retail_cursor_feed (changes neither). A.first <= A.verified <= that end: toy_ahead_write advances A.verified only for a sector below cursor.lba+cursor.count, and toy_ahead_publish advances A.first only over verified sectors. So the subtraction cannot wrap, A.first can reach the end but not pass it, and at the end every request is given up for.
- Opening test (toy_movie_ahead.inc:588, compiled): position_lba is written in this build only at retail_gd.c:165/174/183 (start-up), 616 (SEEK) and 922 (kui_retail_gd_progress, partial progress included). Line 571 lies behind the KUI_RETAIL_GD_ASYNC return, 699/785/876/897 belong to CE or stream-command builds, and toy_pilot_gd.c is not in this build. After a complete read by the low reader, the staging path or the window it is the read's last sector; INIT and RESET leave it; after an aborted or failed read it is the last sector reported, or unchanged if none was.
- release.armed is written by hook() (retail_async.c:313-327) immediately before a request's first visit (retail_async.c:1588-1589). In practice a window opens only at that first visit: later visits of the same request find the low reader busy (which marks the token failed), its cursor advanced, or the same position test failing.
- Variant indices, compiled code only: suite_file and suite_demand index movie_ranges only under variant != 0; movie_exit, C.playback, C.wait and wait_token take the request's variant 1..4; the stage_result cells are [0..1] and [2..3], and the window's variant 5 is kept out by the AHEAD_OTHER tests at toy_movie_ahead.inc:432-433 and 595-596; C.resume.variant is set only with in_play, so 1..4. The places that would index with 5 (toy_ahead_live, the LOW_PIPE slice, toy_wire_wait_end's per-movie counters) are not compiled, and the #error chain SEQ -> FLUID -> STEADY -> LEAN keeps it so.
- Arena: toy_movie_ahead returns false only inside if(!A.variant) or through toy_ahead_fallback, which returns false only after toy_ahead_clear has halted the stream and zeroed A.variant. A window is not opened while staging of that token is under way (the C.resume test at 579-581), and opening one invalidates C.resume. toy_resume_deliver is called only after that false return.
- The dispatcher's short way: with toy_ahead_live gone from both sides, a visit that passes toy_ahead_plain does in toy_movie_ahead exactly what the short way does (no give-up term can hold, selected is false without an active request, the pump runs under the same idle rule). A request served in an interrupt visit and not yet reported fails the status test and takes the whole path, which keeps the window.
- The movie path is as in Test 76: suite_raw and suite_file together are suite_movie's tests term by term, so variant, toy_suite_active, in_play and toy_resume_deliver's selected get the values they got before.
- Served data: toy_ahead_publish is reached only with an active, selected request whose first sector is A.first and which fits the window (or a window just opened at it). It copies verified slots only, all or nothing, and A.head and A.first move together.
- Visits: REQUEST, EXEC and CHECK with wait zero and nonzero, the kick, SCI interrupt visits, the controller's wake, the update hook, MOVIE_COMPLETE, QUIESCE and RESET, each against a window open or not and a request at its first sector or elsewhere. Outside the first finding I found no combination in which a request is left to a window that nothing pumps, and none in which one is given up without the low reader taking it at the next foreground visit.
- Lending the card: quiesce() halts the stream (timer, IPRA, SDK slot and channel 1 given back), ends the card's command and unhooks; the developer's sequence_lent passes in my copy for a stream standing still and for one receiving.
- Random walks over the fixture in a scratch copy (consecutive and other reads of 1 to 64 sectors, whole-sector reads, NOP/SEEK/GETTOC2, INIT/RESET, aborts, idle time, four ways of polling plus the controller's wake, slow cards, scattered extents, stalls, with and without parity): 4,000 seeds, every completed read exact, no failed read, and no hang other than the 39 of the first finding.

### Not verified

- Nothing was run on a console. In particular the first finding's interrupt behaviour there (whether the unanswered SCI interrupt keeps the game's calls from running) is my expectation, not an observation.
- Whether Toy Commander aborts movie reads, issues commands that are not reads between consecutive reads, reads more than 60 sectors in its loads, or uses DMA channel 1 or TMU1 during play. How often a CMD12 fails on the console's card.
- Real cost. The host model has no bus timing; the CMD18 and CMD12 counts are the only measure I give.
- The host builds use the fixture's flags, which compile the resident sources with the fixture's defines, not the console build's. I did not build the console module.
- That builds without the switch compile as before, the Makefile's selection, the report's row layout and the mutation list: read in the diff, not reviewed.
- The two trial changes were run against the fixture and the random walks only, both applied together for the walks and the second whole-fixture run. They are not reviewed for the console build or its size.
- The developer's edits after 22:21 UTC, if any.

## Third reader: the tests of the new behaviour (not completed)

The reader did not return. Its work directory held the logs of six
experiments on a copy of the tree, which the author read:

- The idle test without its new term, in all visits: the fixture fails
  (the mutation list has it).
- The same defect in interrupt visits only: the fixture passed. An
  interrupt visit then stops the stream and leaves the window to the
  next call, which no assertion saw. `sequence_interrupt()` now checks
  that no stream is stopped and no CMD12 sent across those visits, and
  the list has the mutation; it is caught.
- Without the drive position placed in `open_movie()`: the fixture
  passes with the same number of checks. In this fixture the service
  starts with its position on the sector before the first request. The
  comment says so now.
- With the track's two format tests taken out, ten consecutive reads on
  a track of 2,048-byte sectors: nine windows opened, 81 sectors failed
  their check, nine windows abandoned, the reads exact by the low
  reader. The scenario is in the fixture now
  (`sequence_nowhere(3)`), with a mutation that takes both tests out.

Its other questions (whether every printed claim follows from an
assertion; which real callers `plain_read()` does not model; cases with
no scenario at all) were not answered by a reader. What stands in for
them in part: each of the 37 mutations of the new lines is caught by a
test.
