# Virtual VMU and the stream reader: how the VMU uses the card

An independent reader's answer as returned, unedited but for line wrapping and
paths (repository paths relative to its root, scratch copies as <scratch>). It
was asked one part of the question the owner put on 16 November 2026 after
Test 77's console result; the brief is quoted in
docs/fmv-stream-reader-plan.md. The sceptical check of these claims that was
to follow was cut off by the session's usage limit; what the author checked
himself is said in the plan.

## Summary

While a game runs, the virtual VMU keeps none of its 128 KiB image in memory,
only one checksum per block: the image is a file on the SD card, every block
the game reads is fetched from the card, and every finished block write goes
to the card at once, one card transaction per block. Nothing else uses the
card for it: device and media queries, the four data phases of a write and the
reset are answered from memory, there is no timer, flush, launch or exit
access by the worker, and LCD and clock are not emulated. Each transaction
runs inside the game's Maple completion with all interrupts masked, after the
worker borrows the card from the disc reader. If the game has a read
outstanding the borrowing is refused and the game is told to ask again;
otherwise the reader's open read command is ended (CMD12), the SCI port
changes hands, and the reader stays closed until the game's next read reopens
it. In the Test 77 reader that same step halts the stream and throws the
window away, and a flag that is set there and never cleared stops the player's
hook from pumping afterwards; this has a host scenario and has never run on
the console with a window open. Toy Commander's movie requests are served at
once from the window, so a VMU block read arriving during a movie would as a
rule be granted, not deferred (inferred). A movie with no VMU block reads or
saves sees no borrowing at all, only the worker's hooks on every Maple
transfer, which Test 77 already runs in their VMU-off form. How long one
borrowing lasts has never been measured, and the code bounds it in bytes
clocked, not in time. How often is the game's doing: Toy Commander and House
of the Dead 2 were each seen to read 15 to 25 blocks once when they find the
card, with no writes; whether that burst falls inside a movie is not
established. The Test 77 module itself cannot run with the VMU on:
KUI_TOY_SUITE_MODE=66 removes the VMU code, and its worker has 12.5 KiB spare
where that code takes 13.3 KiB.

## Facts

1. (read in code) Where the data lives. On the card: a plain 256-sector image
file card.vmd and a two-sector journal card.journal in the game's folder
/KUI/vmu-test/pNNNN. The worker receives only their sector extents (at most 16
for the image, 2 for the journal) and one CRC32 per block in a 1,272-byte
handoff. In RAM there is no copy of the image and no block cache: the whole
durable object is at most 4,608 bytes (handoff with 256 CRCs, extent maps, one
512-byte bounce buffer, the journal's data and marker buffers, one staged
512-byte block).

   Evidence: include/kui/vmu_durable.h:11-27,51-66;
   include/kui/vmu_store.h:50-64 ("no 128 KiB RAM mirror");
   src/core/vmu_store.c:2-3; src/core/vmu_game_card_service.c:2,16;
   src/loader/vmu_native_passive.c:246

2. (read in code) Which Maple commands reach the card. Answered from RAM, no
card: DEVINFO/ALLINFO (1, 2), RESET (3), media info (10), each of the four
BLOCK WRITE phases (12, staged only), and the read's get-last-error (13 phase
1). BLOCK READ (11) reads the card every time, one sector, checked against the
RAM CRC. The final SYNC (13 phase 4) writes that one block through at once and
acknowledges only afterwards; nothing is batched or deferred. LCD and clock
are not emulated: the unit advertises storage only, and a packet of any other
shape addressed to it is refused before the transfer and stops the game with
failure code 21.

   Evidence: src/core/vmu_emulation.c:71-91,147-159,181-270;
   include/kui/vmu_emulation.h:24-34;
   src/core/vmu_durable.c:166-175,209-218,241-252;
   src/loader/vmu_readonly_overlay.c:70-84,143-147;
   src/loader/vmu_native_passive.c:2431-2433

3. (read in code) No other occasions. At arm the worker only copies the
handoff ("No I/O"). It has no timer, idle flush or exit checkpoint. The
session's first transaction also reads the journal marker sector, which must
be all zero. In the installed recipe a VMU block read or commit is the only
user of the card loan: the two other callers are compiled only with
KUI_NATIVE_SESSION_CONTROLS (default 0, absent from the recipe) or
KUI_TOY_SUITE_MODE (the test's report save).

   Evidence: include/kui/vmu_durable.h:67;
   src/core/vmu_durable.c:87-102,151-165,191,219-233;
   src/core/vmu_store.c:90-97; src/loader/native_session_worker.inc:127,253;
   src/loader/toy_suite_report_save.inc:849; include/kui/native_session.h:8

4. (read in code) Card work in one transaction. Every one first reads the
card's CID (CMD10). A read is 1 sector (2 on the session's first). A commit is
4 single-sector writes (journal marker cleared, journal payload, journal
marker, image sector), 8 status commands and 1 read-back: at most 6 sector
transfers. All of it is byte-by-byte programmed transfer, because the bus lent
in this family has no block-transfer callback.

   Evidence: src/core/vmu_durable.c:105-109,145-154;
   src/core/vmu_store.c:85-89,134-166; src/loader/vmu_backing.c:44-72;
   src/dreamcast/sci_sd_storage.c:58-117;
   src/loader/sd_reader.c:108-125,297-325; src/loader/sci_sd_bus.c:669-690;
   counts confirmed in docs/vmu-native-runtime-cost.md lines 60-62

5. (read in code) Path and interrupt mask. The transaction runs inside the
Maple completion hook: kui_vmu_native_passive_complete, overlay complete,
kui_vmu_durable_handle, durable_card_execute, persistent_lease,
config.sd_lease = kui_vmu_observe_sd_lease,
kui_retail_native_async_loan_begin, card acquire, callback, release, loan_end.
Interrupt mask 15 is required at each entry (FAULT or CONTEXT otherwise) and
kept throughout, on the worker's private stack, with the GD entry lock held
from begin to end. A full ownership proof (heap walk, hooks, layout,
command-chain compare) runs before and after every lease call, refused ones
included.

   Evidence: src/loader/vmu_native_passive.c:837-857,1963-1974,2634-2643;
   src/loader/vmu_readonly_overlay.c:231-252; src/core/vmu_durable.c:183-208;
   src/loader/vmu_native_background_resident.c:152-199;
   src/loader/retail_async.c:1101-1109,1148-1150;
   include/kui/vmu_native_async_loan.h:37-43;
   src/loader/vmu_native_passive_hooks.S:197-205

6. (read in code) The bound on a transaction is in bytes, not time. The loan's
clock is the count of bytes clocked (8 nominal ticks a byte, "not a wall-clock
claim"). Each card wait (ready before a command, data token, write busy) ends
after 6,250,000 ticks = 781,250 byte exchanges or 1,000,000 tries; each SCI
flag wait after 10,000 polls; the reader's CMD12 busy wait after 200,000
bytes.

   Evidence: src/loader/sci_sd_bus.c:37,234-258,270-283,676-690;
   src/dreamcast/sci_sd_storage.c:4-16;
   src/loader/sd_reader.c:25-28,81-89,129-138;
   src/loader/vmu_backing.c:8,30-38; src/loader/sci_stream.c:417-418,456-461

7. (not established) The length of a loan on the console is not known. The one
timed capture of the worker (SA2, 22 Oct 2026) had zero storage calls, and the
shared-loader audit lists the loan's timing as needing hardware evidence.

   Evidence: docs/evidence/native-sa2-latency-2026-10-22.json
   (metrics.storage.calls 0); docs/shared-loader-audit-2026-11-06.md,
   "Coverage and unresolved items", row "Native and CE storage loans";
   docs/vmu-native-latency-test.md lines 49-53

8. (read in code) When the reader is busy. loan_begin returns BUSY, with no
stop and no acquisition, while the game is inside a GD call, a read request is
pending or executing, the reader is active or in its interrupt, the auxiliary
refuses to quiesce, or a DMA reception runs. The durable core then answers the
game AGAIN (for a READ, and for the final SYNC with its staged block kept).
The retry is the game's own SDK's; the worker has no queue or timer. A second
BUSY comes after the reader was already stopped and closed, if the SCI port is
then refused.

   Evidence: src/loader/retail_async.c:1107-1120;
   src/core/vmu_durable.c:193-196;
   src/core/vmu_emulation.c:193-194,208-213,261-262;
   src/loader/vmu_readonly_overlay.c:198-205;
   src/loader/vmu_native_background_resident.c:183-189;
   docs/vmu-native-a2-background-test.md lines 4-9

9. (read in code) On a fault. Any failure after acquisition quarantines the
reader for the session (loan state QUARANTINED, card marked not ready, reads
fail, no reinitialisation) and latches the virtual card's fault. The
completion hook then stops the game with the worker's failure page, code 24.

   Evidence: src/loader/retail_async.c:1088-1093,1144-1146,1152-1166;
   src/core/vmu_durable.c:66-69,197-200; src/loader/vmu_native_passive.c:2643;
   include/kui/vmu_sd_lease.h:36-38

10. (read in code) What a granted loan does to the installed reader. The
QUIESCE visit sets quiesced and stops a speculative block in reception. Then
the SCI level goes back to the game (unhook), kui_sci_stream_stop ends the
open read command with CMD12 (the reader normally keeps one open between
requests), blocks received but not delivered are discarded, the SCI port is
released and the reader is closed (opened=0). loan_end only sets the state to
IDLE and restarts nothing. The 192 KiB block cache is kept.

   Evidence: src/loader/retail_async.c:396-397,1116,1125-1143,1167-1173;
   src/loader/native_readahead_aux.c:812-837;
   src/loader/sci_stream.c:331-336,436-465,1139-1175;
   include/kui/vmu_native_async_loan.h:41-42;
   docs/vmu-native-readahead-test.md lines 47-48

11. (read in code) How the installed reader comes back. Only the game's next
read reopens it: start() calls open_bus() (SCI acquire and stream open) and
sends CMD18 for that request. The read-ahead producer cannot reopen; it skips
while the bus is closed or the stream is not paused at its next block.
quiesced is cleared by the next delivery of an active request (aux line 778)
or by a controller TICK once the bus is open again (1627).
KUI_NATIVE_RA_RESUME has a handler (1639-1640) but nothing sends it; its only
senders are in a host test.

   Evidence: src/loader/retail_async.c:398-413,1052-1063;
   src/loader/native_readahead_aux.c:335-337,351-355,778,1627-1629,1639-1640;
   git grep KUI_NATIVE_RA_RESUME: include/kui/native_readahead.h:27,
   tests/test_native_readahead_aux.c:872,942,949

12. (read in code) In the Test 77 build the QUIESCE visit takes another
branch. toy_ahead_clear halts the continuous stream at once and gives the
window up (variant=0, so the verified sectors ahead of the game are dropped).
One CMD12 follows, from that visit if a reception was under way, otherwise
from loan_begin. A running stream does not make the loan BUSY, because the
visit stops it before the busy test. Neither place that clears quiesced is
compiled in this build, so after one loan the player's update call no longer
pumps the reader.

   Evidence:
   src/loader/native_readahead_aux.c:768-769,812-822,1485-1488,1619-1620;
   src/loader/toy_movie_ahead.inc:154-169;
   src/loader/toy_wire_ahead.inc:201-228,477;
   src/loader/retail_async.c:1116-1118,1127; src/loader/sci_stream.c:1174;
   docs/evidence/toy-wire-stream-77/review.md finding 7

13. (read in a document) Status of that path in Test 77. The host scenario
sequence_lent() covers it: window given up, stream stopped, one CMD12, and
after the card is given back the game reads on and has a window again. It has
not run on the console with a window open: in both console runs every window
had been given up before the report's save lent the card. Known limit 6
records the quiesced flag as "set and never cleared".

   Evidence: docs/toy-wire-stream-test-77.md at HEAD 12f9b03: Verification row
   "The card lent with a window open" (line 374); Console result point 7
   (lines 296-301); Known limits 6 (lines 347-351)

14. (measured here) The recipes. Test 77's flags are the production compact
Background flags plus KUI_TOY_SUITE_MODE=66, KUI_NATIVE_TOY_MOVIE_CAPTURE=1,
KUI_NATIVE_TOY_PLAYBACK_CAPTURE=1 and the KUI_TOY_WIRE_* set; production has
none of these and nothing Test 77 lacks. KUI_TOY_SUITE_MODE makes
reader_only() the constant true: arm refuses any launch that is not
reader-only, the VMU handlers return 0 and persistent_lease returns FAULT. In
production reader_only() is the launch's VMU On/Off value
(config.diagnostics).

   Evidence: make -pn dumps of Makefile.vmu-native-background-compact and
   Makefile.vmu-native-toy-suite (Test 77 switches) on copies:
   <scratch>/vmu/mk/prod.db and t77.db;
   src/loader/vmu_native_passive.c:317-327,337-345,838,1656-1659

15. (measured here) As linked. The Test 77 worker contains no
kui_vmu_durable_*, kui_vmu_store_* or kui_vmu_emulation_* code; its map lists
those sections as discarded. A worker built here from the production recipe on
the same source links 13,268 bytes of them (vmu_durable.o 3,724,
vmu_readonly_overlay.o 3,084, vmu_volatile_chain.o 2,794, vmu_store.o 1,918,
vmu_emulation.o 1,748) and 1,559 bytes more BSS. Free room below the stack in
the 72 KiB worker span: 384 bytes in the production worker, 12,480 bytes in
Test 77's.

   Evidence: sh-elf-nm, sh-elf-size and link maps of
   <scratch>/clean77/build/toy-suite-77/native-worker.elf and
   <scratch>/vmu/prod/build/native-background-compact/native-worker.elf (built
   from git archive of HEAD; src unchanged since ded3083)

16. (read in code) Every frame with the VMU on and no card access. Each Maple
transfer passes the worker's submit and completion hooks at interrupt mask 15
on its private stack. A controller-only batch takes the short path: submit
checks about a dozen registers and copies at most 32 command words; completion
checks stack guard, installed hooks and 8 heap identity words, compares the
command words again, validates each reply, re-checks SDK state, and sets the
parent's attachment bit, the one store that makes the card present. Then it
gives the read-ahead its TICK: up to 2 card blocks by programmed transfer (1
for early drivers), none while more than 96 blocks reached the game in the
last 32 frames. A batch with any packet for the virtual unit takes the full
path. Test 77 already runs the same hooks in reader-only form, without the
attachment store; its controller wake of the demand reader requires
reader-only mode.

   Evidence: src/loader/vmu_native_passive_hooks.S:19-26,197-205;
   src/loader/vmu_native_split_controller.inc:53-128,145-192;
   src/loader/vmu_native_passive.c:2151-2153,2203-2341
   (2318-2322),2420-2460,2477-2553;
   src/loader/vmu_native_reader_capture.inc:204-223,251-281;
   include/kui/native_readahead.h:140

17. (read in a document) Measured hook time exists only for an older Standard
worker (build 1e5b595f9618, Sonic Adventure 2, 22 Oct 2026): 1,200 poll pairs
in 19.97 s; submit mean 559 microseconds (max 600), completion mean 500 (max
540), heap-free guard 452 microseconds at 13.9 calls a pair, all masked,
44.08% of wall time together. Later workers have host work counts only: 336
guest reads for a steady two-controller pair, 342 with a virtual INFO, 758
with a virtual block READ. No console timing of the compact split-controller
worker's hooks was found.

   Evidence: docs/vmu-performance-design-review-2026-10-22.md, "What the
   console established"; docs/evidence/native-sa2-latency-2026-10-22.json;
   docs/vmu-native-runtime-cost.md lines 10-19

18. (read in a document) Console counters on this family with the VMU on
(House of the Dead 2, Background New, virtual A2): 23 block reads, 24 sector
reads, no write phases or commits, no card faults; BUSY deferrals 181 in one
run (build dc9fc5e49302) and 5 in another (242169041894). One read-ahead block
by programmed transfer took at most 1,122 to 1,153 ticks (1.44 to 1.48 ms);
the largest reader visit 6,541 to 6,624 ticks (8.4 to 8.5 ms). The owner
reported extra lag with VMU On against VMU Off.

   Evidence: docs/universal-vmu-review-2026-11-01.md: table row "House of the
   Dead 2" (line 16); "Follow-up after console build dc9fc5e49302" (line 101);
   "Owner follow-up after 242169041894" (lines 168-173)

19. (read in a document) Toy Commander's own card traffic, seen in the earlier
Standard-route proofs, in 1,200 Maple schedules after its first card request
(schedule 28): to a physical card 28 of 4,940 packets (1 DEVINFO, 1 LCD
update, 1 media info, 25 block reads, no write or sync); to the synthetic
blank card 17 replies (1 DEVINFO, 1 media, 15 block reads, no commit).

   Evidence: docs/vmu-emulation-status.md lines 155-166 and 184-190

20. (inferred) For a movie on the stream reader. Test 77's Run 1 had requests
waiting for at most 0.48 s of a 5.4 s piece and none in most pieces, and BUSY
needs a pending request, so a block read or commit arriving in a movie would
as a rule be granted. Each grant ends the stream and drops the window (62
sectors at most), costs one CMD12 and a CMD18 at the next read, and holds
every interrupt off for its length. By the 1.45 ms measured for one programmed
read-ahead block, a read loan is a few milliseconds; a commit adds four card
programming waits, which depend on the card.

   Evidence: docs/toy-wire-stream-test-77.md Console result table, rows
   "Requests waited, s" (lines 241-242) and Known limits 2;
   src/loader/retail_async.c:1113-1120; the HOTD2 figure in
   docs/universal-vmu-review-2026-11-01.md line 101

## Open questions

- How long one card loan lasts on the console with interrupts masked, for a
block read and for a commit: no capture has ever timed one. The worker's
existing STORE row (VMU LATENCY page, profile mode) would give it in the
installed build.
- When a game's block reads fall relative to its movies. Toy Commander's 15 to
25 discovery reads begin at its 28th Maple schedule; whether that lies inside
the first opening movie is not in the documents. Whether any admitted game
reads card blocks while a movie plays is not known either.
- How quickly each SDK asks again after AGAIN: only counts exist (HOTD2: 181
and 5 deferrals for 23 reads). Every retry costs a full-path frame with two
ownership proofs, masked.
- Whether the window should survive a loan, as the installed 192 KiB block
cache does. Today toy_ahead_clear drops it; keeping the verified sectors and
only stopping the stream would be a design change for the plan's step 6.
- The quiesced flag that is never cleared in the Toy build (Known limits 6)
has to be fixed before a loan can come during play; the review suggests
clearing it when a window is next opened or when the loan ends.
- The hook time per frame of the compact split-controller worker has no
console measurement; the only figures are from the older Standard worker and
host work counts.
- Step 6 cannot be a switch on the Test 77 module: reader_only() is fixed by
KUI_TOY_SUITE_MODE and the worker lacks about 2 KiB for the VMU code beside
the Toy report code. Whether it is built on a general module (step 4) or by
trimming the Toy worker is the owner's choice.
- Which SCI state a game leaves between a loan's end and its next read, and
whether a game's own DMA channel 1 or timer use collides with the stream's
restart after a loan, were not examined here.
