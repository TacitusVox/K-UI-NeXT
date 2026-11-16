# Virtual VMU and the stream reader: the written record

An independent reader's answer as returned, unedited but for line wrapping and
paths (repository paths relative to its root, scratch copies as <scratch>). It
was asked one part of the question the owner put on 16 November 2026 after
Test 77's console result; the brief is quoted in
docs/fmv-stream-reader-plan.md. The sceptical check of these claims that was
to follow was cut off by the session's usage limit; what the author checked
himself is said in the plan.

## Summary

Read at HEAD 1e072b2: three documentation commits landed on top of 7456726
while I read (the last records the owner's direction of 16 November); no
source, test or Makefile differs from ded3083, and all line numbers below are
from 1e072b2. For the native Background family the record holds console
results only for the combined mode (Background New with the virtual VMU on),
all on earlier builds: Evolution 2, Rayman 2, Lodoss, Code Veronica, House of
the Dead 2, Time Stalkers, EGG and Sonic Adventure. The installed cohort
(f8807cb16561 / 9e3295bcfaec) is written down as host/linked-qualified, not
console-accepted, with one Evolution 2 timing capture (Background New, VMU On)
called a slow baseline. Read-ahead only on that module (VMU Off, which keeps
the whole 284 KiB lease) has one explicit console run (HOTD2 test 7c) plus the
Toy test modules; VMU only inside the Background family rests on one owner
remark about DOA2, the accepted VMU-only route being the separate Standard 72
KiB service. Toy Commander's virtual-card saves were made and reloaded after a
reboot in October, but on an exact-Toy loader on the Standard reader with
background loading and CD music off and a ten-minute bound; nothing records
Toy on the installed Background module with the VMU on, and every stream test
from 68 to 77 had it off. The recorded VMU-on trouble in this family is HOTD2:
a freeze and lag with the card on, none with it off, the cause left open
between the Maple overlay and the card's loans answered "send again" while the
reader is busy; Toy's movie stutter is recorded without any virtual VMU. The
gates a combined build meets are: a card loan is a scheduling condition and
every guest call during it is audited; a save is acknowledged only after
journal, readback and the card handed back healthy; lost-byte blocks are
quarantined; the compact release gate pins the exact geometry; return and
reset retire the reader and the worker. The Test 77 record itself says a loan
with a window open has only a host scenario, and that after one loan the
player's hook stops pumping, which its review says "matters once a loan can
come before or during a movie (the plan's step 6)". The roadmap puts the four
service modes (section 2, sequence item 4) before FMV and CD-music streaming
(section 4, item 5) and calls the stream plan not approved; the plan has VMU
beside the stream as step 6, after the reader has become the family's default.
The owner's direction of 16 November names "Universal VMU" as the first goal,
and the plan now says the reader counts for it only where it runs beside the
virtual VMU; no sentence says the owner normally plays with it on, but the
usual-options capture, the setting's default and nearly every recorded console
result have it on.

## Facts

1. (measured here) State of the tree while reading: HEAD is 1e072b2, not
7456726. Commits 12f9b03, 9c9867c and 1e072b2 (documentation only) are on top;
1e072b2 arrived mid-read and changed fmv-stream-reader-plan.md (197 to 242
lines), universal-loader-roadmap.md (303 to 324) and
toy-wire-stream-test-77.md (504 to 512). Nothing under src/, include/, tests/
or the Makefiles differs from ded3083. Working tree clean; I changed nothing.
Line numbers below are at 1e072b2.

   Evidence: git log --oneline -5 and git show --stat 1e072b2 in
   /home/user/K-UI-NeXT; git diff --stat ded3083 HEAD -- src/ include/
   Makefile* tests/ (empty); git status --short (empty)

2. (read in a document) Q1, installed cohort: no service combination has a
console acceptance on it. "The current native test source is `f8807cb16561`
... It is host/linked-qualified, not console-accepted." The one console datum
is the owner's Evolution 2 capture, "21.704 seconds before handoff on
Background New, VMU On, actual module payload CRC `9B37B199`" (module
retail-native-dual.kui), of which the ledger says "This is a slow console
baseline, not performance acceptance." The f8807cb game-time components are
byte-preserved from earlier cohorts ("Twenty-seven installed game-time
components preserve complete initialized bodies and memory geometry"), and
"These results are not new console compatibility or latency acceptance." The
roadmap names 9e3295bcfaec as the delivered candidate.

   Evidence: AGENTS.md:34-36; docs/accepted-progress.md:13-27;
   docs/evolution2-f8807cb-measurement.md:3-9;
   docs/shared-launch-coverage-restoration.md:58-66;
   docs/universal-loader-roadmap.md:3-5

3. (read in a document) Q1, both services (Background New, virtual VMU on, 192
KiB read-ahead): console results exist on earlier builds. Evolution 2 on
d0aac421584c ("SCI Background PIO pair, virtual A2"), owner: "I think we fixed
it! I noticed a single hiccup. If there's anything else it's imperceptible."
Rayman 2 and Lodoss on f158ba5e24f9 (New (X), virtual A2; then a 384 KiB lease
with a 128 KiB worker): flawless FMVs and gameplay, saving and loading work,
power cycle not described. Code Veronica on 2322fda6f92e: play, save, full
power cycle, reload and Disc 2 pass. HOTD2 test 7c: "the game finds the
virtual card in A2 and saves, and the save loads after a power cycle". Time
Stalkers image boot and improved HOTD2 on bbe142d4a278; EGG save/load on
242169041894 (power-cycle reload not reported); Sonic Adventure by owner word
of 7 Nov, no build. The table sums up: "20 games pass the Background
virtual-VMU gates, and the owner has tested them on console." The ledger calls
the "combined284-KiB/cache192-KiB installations" accepted.

   Evidence:
   docs/evidence/native-read-ahead-pio-pair-observed-2026-10-25.json:4-9;
   docs/native-reader-compatibility-test-sheet.md:10-13,25;
   docs/native-dual-reader-test.md:13-14,92-97;
   docs/evidence/native-code-veronica-console-2026-10-28.json:6-20;
   docs/native-zr-family.md:393-396;
   docs/universal-vmu-review-2026-11-01.md:167-169,275-276;
   docs/evidence/library-console-results.json:62-69;
   docs/library-compat-table.md:87-88; docs/accepted-progress.md:898-899

4. (read in a document) Q1, read-ahead only: in this family that is the
compact module with Virtual VMU Off, which keeps everything ("Off retains that
worker, full reservation and shared reader hooks; it disables virtual card
service"; "VMU Off in the compact reader-only mode retains its full lease. A
smaller prefetch-only reservation is not established."). One explicit console
run: HOTD2 test 7c, "With Virtual VMU Off on the same module, with the same
reader, prefetch and arena, the boss was passed without a freeze." The Toy
stream tests 68 to 77 are console runs in this mode on a test module. A Skies
check of "Background On / New (X) / Virtual VMU Off" was asked for; I found no
result for it.

   Evidence: docs/native-skies-background72.md:7-11,82-86;
   docs/shared-loader-audit-2026-11-06.md:49;
   docs/universal-loader-roadmap.md:177-178; docs/native-zr-family.md:403-404;
   docs/toy-wire-stream-test-77.md:190-191

5. (read in a document) Q1, VMU only and neither: VMU only inside the
Background family rests on one owner remark of 7 Nov 2026 with no build: "Dead
or Alive 2 works with virtual VMU on the Background loader without prefetch, a
combination this table does not model". The roadmap's section 3 still lists
DOA2 as "Standalone demand reader | Prove memory ownership before adding
virtual VMU or cache"; the two records disagree. The console-accepted VMU-only
route is the separate Standard 72 KiB service (Power Stone 2 on d503f1aa423d
and 2e5314bd, Silver on 3c80d14a0529, Skies on d6d517fa133a, Toy on
bf91dd1e994c); the ledger says the accepted installations "remain distinct
from VMU-only operation". Neither is the ordinary Standard reader with VMU off
or standalone New, not a mode of the compact module. Sequence item 4, separate
accounting of the four modes, is open.

   Evidence: docs/library-compat-table.md:21-23,56,97-100;
   docs/evidence/library-console-results.json:96-103;
   docs/universal-loader-roadmap.md:27,171,191;
   docs/vmu-feature-comparison.md:15-21,32-35;
   docs/native-skies-background72.md:19-21; docs/accepted-progress.md:898-901;
   docs/global-launch-policy.md:43-45

6. (read in a document) Q2, Toy Commander's virtual-VMU record: saves made and
reloaded after a full reboot on 19 and 20 October 2026, on the exact-Toy
persistent loader bf91dd1e994c installed as retail-boot.kui. Route: "SCI,
Standard reader, background loading Off, CD music/CDDA Off and RAM patches
Off". Result: "Toy Commander lists the old imported save and newly created
saves, and the new saves survive a full reboot"; a dedicated card p0005 the
next day. The loader carried "a diagnostic card window of roughly five minutes
and an overall bound of roughly ten minutes". The coverage record: "The older
Toy virtual-VMU pass is a separate configuration." The compat table's Toy row
cites only these, with an empty build field.

   Evidence: docs/vmu-p0004-imported-card-test.md:10-19,29-31,44-49;
   docs/VMU-RESET-HANDOFF.md:6-7,23-34; docs/vmu-emulation-status.md:352-364;
   docs/universal-library-coverage.md:70;
   docs/evidence/library-console-results.json:144-151

7. (not established) Q2, Toy Commander on the installed compact Background New
module with the virtual VMU on: no console result is recorded. It is listed as
a "new console test candidate" for "SCI, Background On, New, Auto encoding, CD
music Off, RAM patches Off and virtual VMU On" ("After that, try Quake III,
Smash Pack and Toy Commander using the same settings") with no outcome
written; the static gates pass (table column "virtual VMU"); it is absent from
native-console-known.json. Every stream test from 68 to 77 ran with "CD Music
and virtual VMU off", and Test 77 says its runs answer nothing "about CD music
or about the virtual VMU, both of which were off".

   Evidence: docs/vmu-native-family-expansion.md:15,24-33;
   docs/library-compat-table.md:54;
   docs/evidence/native-console-known.json:1-38;
   docs/toy-wire-stream-test-77.md:190-191,304-307,508-510; grep of VMU over
   docs/toy-*.md

8. (read in a document) Q2, recorded problems. Toy's intro movie stutter is
recorded for the 2.0.0 release, which had no VMU emulation ("VMU emulation ...
remain future work"), so it is not a VMU effect; no VMU-on against VMU-off
comparison of Toy's movies exists. An early Toy VMU observer (87140926efd1)
once "stopped during an accepted GD DMAREAD with CD music off"; later runs of
that build reached their planned stop. HOTD2 test 7c: "With the card On it
froze once during the first boss battle ... There was also some lag", and with
the card off no freeze: "The cause therefore lies in the card-On path: the
per-transfer overlay on the early driver's full submit path, or the card's SD
loans, each answered \"send again\" while the reader is busy." Later builds
improved it but it "retains annoying lag". Evolution 1 and 2: "save and load
succeed; FMV slowdown reported" on the earlier Standard-reader VMU builds,
before the read-ahead. PSO: intro video slow, though "Spawn reproduces poor
streaming without a virtual VMU". WinCE Armada's intro became a slideshow with
VMU on until its Maple hook cost was cut, but "Native games use a different
VMU implementation; this measurement says nothing about them."

   Evidence: docs/release-v2.0-notes.md:117-118,220,228;
   docs/vmu-emulation-status.md:90-99; docs/native-zr-family.md:397-407;
   docs/universal-vmu-review-2026-11-01.md:170-176;
   docs/universal-loader-roadmap.md:197;
   docs/evidence/library-console-results.json:179-194;
   docs/vmu-native-a2-background-test.md:21-22;
   docs/evidence/native-pso-spawn-console-c2e6f07-2026-10-28.json
   (interpretation/2); docs/armada-vmu-on-audio-fix-test.md:223-253,323-324

9. (read in a document) Q3, the gate on a card loan: "A virtual-card loan is a
scheduling condition, not a disc error. Audit every guest API that can arrive
during a loan, including status, initialization, callback registration and
cancellation; request submission alone is not enough." Waiting callers retry
the real operation after ownership returns; "never fabricate a successful
operation". "Keep this regression when altering shared ownership or adding
another SDK profile." It comes from VC2, where "while the virtual-card worker
borrows the SD reader, the low GD gateway returns -1 for non-request control
calls" and the game turned that into a permanent drive error. The roadmap's
section 1 repeats it: "Audit every storage-loan control operation and
completion owner." Rebuilt native routes "need their own existing linked
ownership/stack checks".

   Evidence: docs/shared-loader-vmu-regressions.md:8-25,68-72;
   docs/accepted-progress.md:269-277; docs/vc2-gd-loan-control-race.md:1-15;
   docs/universal-loader-roadmap.md:112-113

10. (read in a document) Q3, the gate on a save: matrix row "Save durability |
Persistent-session, durable-worker and loan fixtures | Journal ordering,
readback, SYNC acknowledgement and ownership return before publication". For
the native worker: "Final write ACK requires the existing journal/data
readback and successful restoration of the exact scoped SCI loan shared with
GD I/O", and "Each external SCI loan keeps a complete proof immediately before
acquisition and after restoration; failed proofs cannot publish the virtual
reply or final save ACK." Roadmap section 4: "Deferred VMU commits must retain
journal/readback and acknowledgement ordering. Do not grow a reservation from
apparent RAM headroom or silently suppress saves." A past regression of
exactly this kind: a safe SCI BUSY produced a valid Maple AGAIN reply that the
overlay refused (detail 17); READ, pending WRITE and final SYNC retries must
pass "without publishing data or acknowledging a durable commit".

   Evidence: docs/shared-loader-vmu-regressions.md:65;
   docs/native-worker-checkpoint.md:271-272,351-353;
   docs/universal-loader-roadmap.md:254-256;
   docs/vmu-native-a2-background-test.md:4-9

11. (read in a document) Q3, what the installed read-ahead is recorded to do
around a loan, the accepted behaviour a stream must match: "The optional
producer runs only from the controller heartbeat, with no active demand
request or VMU loan"; "VMU storage loans quiesce the optional producer;
checked cached disc bytes survive the loan." Its design rule from the 25
October checkpoint: require the "loan, demand, port and DMA admission guards"
and "Never start this work from a GD delivery or SCI interrupt". The accepted
Evolution 2 capture exercised no loan ("quiesces": 0, "loan_resumes": 0), and
the checkpoint before it says "No loan skips occurred". So the documents I
read hold no console measurement of a card loan arriving while read-ahead is
in flight, for the installed reader either.

   Evidence: docs/vmu-native-readahead-test.md:27-31,48-49,126-128;
   docs/checkpoint-native-read-ahead-2026-10-25.md:10-11,20,30-35;
   docs/evidence/native-read-ahead-pio-pair-observed-2026-10-25.json:34-35

12. (read in a document) Q3, quarantine and geometry gates. "CRC
reconstruction after a lost DMA byte does not establish that the rest of the
received block is correct. Quarantine such blocks before cursor/payload
delivery ... do not reintroduce reconstructed-byte acceptance in another title
or loader"; the SCI test checks "CRC failure and failed-stop quarantine". The
plan accepts this for the stream: only raw sectors, with parity or EDC as the
second check. The installed cohort gate: "All five linked component owners
must agree on 72 KiB worker / 284 KiB reservation / 192 KiB cache and the
exact virtual slot", mixed geometries "are refused by the normal release
gate", and "Both reader engines and the physical/virtual VMU toggle remain
covered by the compact family's complete audit." The stream build keeps the
284 KiB lease but has a 24 KiB auxiliary and a 180 KiB arena where production
has 12 KiB and 192 KiB, so a family build must have "the release gate told the
new geometry deliberately".

   Evidence: docs/global-launch-policy.md:13-17;
   docs/loader-integration-baseline.md:10-13;
   docs/fmv-stream-reader-plan.md:47-56,95;
   docs/shared-loader-vmu-regressions.md:84-92;
   docs/shared-loader-performance-policy.md:44-50,122-124;
   docs/evidence/toy-wire-stream-76/generalization-map.md:375-382,634-643

13. (read in a document) Q3, reset and return: "Return is part of the loader
lifecycle. Reaching E0 does not prove VMU, reader, DMA or Maple retirement."
Integrations must verify "that a pending command cannot be retired" and "Prove
both a fresh cold launch and a launch after another game returns." In Test 77
a window's stream is ended by "INIT, RESET, an abort, an error, or the card
being lent"; until then a filled window's stream holds the card inside its
read command, DMA channel 1's addresses, a timer channel and the SCI level. A
rule ending a standing stream after a time "belongs before this reader meets
another game" and is one change of its own before step 4.

   Evidence: docs/global-launch-policy.md:83-88;
   docs/loader-integration-baseline.md:35-51;
   docs/toy-wire-stream-test-77.md:319-334;
   docs/fmv-stream-reader-plan.md:131-139

14. (read in code) Q3, what the Test 77 record says of its own loan path.
Console: "A loan with a window open has not run on the console ... That path
has its host scenario (`sequence_lent()`) and no console run." Host: "The
window given up, the stream stopped, one CMD12, channel, timers and the SDK's
table as before; given back, the game reads on and has a window again". Known
limit 6: "after the card has been lent once the player's update call no longer
pumps (`quiesced` is set and never cleared). In this module the only loan is
the report's save, which ends the run." Review finding 7: "It matters once a
loan can come before or during a movie (the plan's step 6), and the new
sequence_lent scenario reads on after a loan without looking at it"; suggested
fix not taken. The source agrees: quiesce() sets C.quiesced, and the two
places where production clears it lie in code compiled only when
KUI_TOY_SUITE_ASYNC_DEMAND is 0. That macro is derived from KUI_TOY_SUITE_MODE
(Test 77 recipe: TOY_SUITE_MODE=66; production recipe
Makefile.vmu-native-background-compact sets no suite mode). The remaining
clear, on KUI_NATIVE_RA_RESUME, has no sender.

   Evidence: docs/toy-wire-stream-test-77.md:297-302,357-361,384,498-500;
   docs/evidence/toy-wire-stream-77/review.md:224-231,244;
   src/loader/native_readahead_aux.c:768-811 (clear at
   778),812-823,1627-1629,1639-1640; src/loader/sci_stream.h:37-41;
   src/loader/retail_async.c:1116;
   docs/shared-native-bootstrap-rollout.md:65-66

15. (inferred) Q3, the Test 77 module is reader-only by construction. The map:
"The movie scan before it also requires Toy Commander's IP.BIN checksum,
executable checksum and size, the asynchronous reader, and reader-only mode
(virtual VMU off)." The source has it: toy_movie_scan() returns at once unless
native_runtime_mode() is KUI_VMU_NATIVE_MODE_READER_ONLY. Test 77's window
still needs "the verified title on a source the stage found raw, as before
(`toy_poll_owner`, `suite_source`)". So a launch of this module with the
virtual VMU on would, as built, most likely give no stream window at all; I
did not trace how toy_poll_owner and suite_source are set, nor whether Card
Tools can start this module with the VMU on.

   Evidence: docs/evidence/toy-wire-stream-76/generalization-map.md:135-143;
   src/loader/toy_movie_iso_stage.inc:115-121;
   docs/toy-wire-stream-test-77.md:51-55

16. (read in a document) Q3, what a combined build must show before a console
test, and what the test must hold. Before compiling: "identify the accepted
source/configuration, the intended behavior change, affected module roles, and
the decision a new test answers"; one behavior change per commit against a
named baseline; "Keep VMU-only, prefetch-only, combined, and neither as
separate requirements. Disabling a service does not prove its RAM reservation
was released." Section 2's completion evidence: "actual route selection and
rejection behavior, all retained owners accounted for, unchanged physical VMU
behavior, and save/reload checks on the changed configurations." Host side:
the loan, durability, quarantine and geometry gates above, production
admission on the packaged module bytes, and the available original executables
for a changed family. Console side: plan step 6, "An FMV with VMU on, a save,
power off, reload" (the map: "a game-requested save"); section 3, "launch,
loading-heavy gameplay, virtual card detection, game-requested save,
power-cycle reload and appropriate exit"; section 4, "improved same-scene
playback on hardware ... save/reload, with unchanged gameplay responsiveness";
the read-ahead plan, "Check a physical A1 with virtual A2, load an existing
save, make a new save, power off fully, and reload it"; and AGENTS.md: first
and repeat launches on two or three games "with the same image format,
storage, reader and VMU options", both launch phases, cold ROOT boot, the
actual module path, build and payload CRC.

   Evidence: AGENTS.md:46-50,58-61,75-81,89-92,111-113,135;
   docs/universal-loader-roadmap.md:182-185,218-221,258-261;
   docs/fmv-stream-reader-plan.md:110-112,122;
   docs/evidence/toy-wire-stream-76/generalization-map.md:649-652;
   docs/native-read-ahead-compatibility-plan.md:64-79

17. (read in a document) Q4, the roadmap's order. Sequence of 6 November: 1
ordinary WinCE reader with the Armada VMU route kept; 2 native family gaps; 3
executable lifetime through secondary loads; 4 "Broaden virtual saving and
independent service modes. Keep separate accounting for neither service, VMU
only, New/read-ahead only, and combined operation", done when "a game save
survives power-off and reload"; 5 native CD music and native/WinCE streaming,
done when "the same FMV scene improve[s] on console, with responsive play and
durable saves"; 6 the 42-game ledger. The sections are "ordered dependencies".
Section 2 asks to "Establish actual routing and retained memory for all four
configurations", Combined being "Both services under a checked memory and
storage ownership contract", and notes that changing a reservation "requires
new linker bounds, relocation/stack checks, allocator and complete lifetime
proofs". Section 4 asks to investigate "a safely reserved sequential streaming
ring", to reuse the shared stream and verification primitives, to "Preserve
working audio playback and save durability", to measure request sizes,
sequentiality, occupancy and underruns, and to "Keep demand traffic and audio
deadlines ahead of speculative reads."

   Evidence: docs/universal-loader-roadmap.md:22-29,64,165-185,223-261;
   docs/accepted-progress.md:119-123

18. (read in a document) Q4, where VMU beside the stream stands. In the
roadmap it belongs to section 2 and sequence item 4, ahead of the streaming
work of section 4 and item 5; the stream plan "is not approved and changes
nothing in this sequence". In the plan it is step 6 of 8 with step 5 as its
baseline, after step 3 (the player's hook no longer pumps), the
standing-stream rule, step 4 (a module of its own) and step 5 (the family's
default, tested with "an FMV, a load and a save"); the family is admitted
first only "with virtual VMU and CD music off" because "The stream has to stop
for the worker's use of the card and start again". The 16 November text added
on top says the reader "counts for the first two goals only where it runs
beside the virtual VMU and, for a CD music game, beside the music", and that
VMU and CD music "were off in every run of this reader so far". The step table
itself was not reordered, and decision 2 (order) is still the owner's.

   Evidence: docs/universal-loader-roadmap.md:27-28,85-89,225-229;
   docs/fmv-stream-reader-plan.md:83-87,98,114-124,131-139,167-172,187-202

19. (inferred) Q5, whether the owner normally plays with the virtual VMU on:
no sentence says so outright. What is written points that way. The capture
taken under "Use normal raw images and usual options" identifies "Background
New, virtual VMU On". The setting's own note: "Existing installations default
to On." Nearly every recorded console result is with a virtual card ("20 games
... the owner has tested them on console"). The read-ahead was proven on
Evolution 2 with virtual A2 on. The roadmap's goal is a build that selects
"virtual VMU and optional read-ahead for each runtime", with "Physical VMUs
remain available where virtual VMU is not yet supported." And the owner on 16
November: "So the eventual goal is - Universal VMU - Universal reader ... -
Run-ahead or prefetch where possible/needed", "I want those other three goals
ironed out", and of the Evolution games, "They just were my first proving
grounds for the prefetch. They got rid of the lag. Wondering if this new
loader would be better."

   Evidence: docs/shared-launch-coverage-restoration.md:68;
   docs/evolution2-f8807cb-measurement.md:5-8; docs/vmu-toggle-test.md:5;
   docs/library-compat-table.md:87-88;
   docs/native-reader-compatibility-test-sheet.md:25;
   docs/universal-loader-roadmap.md:44-48,70-89;
   docs/fmv-stream-reader-plan.md:147-172

20. (read in a document) docs/games-background-reader.md, which the task
named, carries nothing on the virtual VMU: it is the 1.8.5 status of the
background SCI reader (A, X at 20 and Y at 25 blocks a call), and its Known
risks concern a game changing its vectors while a block is in flight, a stream
restart waiting for the card's token inside an interrupt or GD call, and the
reader's interrupt waiting while the game masks interrupts. The
universal-library-coverage record adds for Toy as a CD-music title:
"Independent virtual VMU, read-ahead and CDDA combinations still need
coverage", and the feature comparison: "New native and Armada writable tests
still gate CDDA off."

   Evidence: docs/games-background-reader.md:3-19,178-190;
   docs/universal-library-coverage.md:24; docs/vmu-feature-comparison.md:111

## Open questions

- The plan's step table still has the virtual VMU as step 6, after the reader
has replaced the installed module in step 5, while the owner's direction of 16
November puts "Universal VMU" first and the plan now says the reader counts
for that goal only beside the virtual VMU. Whether the VMU step moves forward
is not written; decision 2 (order) is still open.
- Is the Evolution 1/2 comparison against the installed prefetch to run with
the virtual VMU on? The installed prefetch was proven there with virtual A2
on, and the owner's usual-options capture is Background New with VMU On; a
stream build that only works with it off would not be compared like for like.
No document says which.
- Has Toy Commander ever run on the installed compact Background New module
with the virtual VMU on (CD music off)? It is listed as a candidate in
vmu-native-family-expansion.md with no result. Its only recorded virtual-card
saves are on the October exact-Toy Standard loader.
- No console record exists of a card loan arriving while read-ahead or a
stream is in flight: the Evolution 2 capture had zero quiesces and zero loan
resumes, and Test 77 had no window open at its only loan. HOTD2's freeze with
the card on names "the card's SD loans, each answered 'send again' while the
reader is busy" as one of two unresolved candidate causes.
- Which bytes are the installed baseline for any VMU-on comparison: AGENTS.md
names f8807cb16561 (keep unchanged while its console baseline is pending), the
roadmap and coverage record name 9e3295bcfaec. Plan decision 8 asks for the
module path, build and payload CRC to be read from the owner's card first.
- Can the Test 77 module be started with the virtual VMU on at all, and what
does it then do? The stage's movie scan returns unless the mode is reader-only
(toy_movie_iso_stage.inc:117); how toy_poll_owner and suite_source follow from
that, and what Card Tools passes, is for the code readers.
- The DOA2 record conflicts: the compat table has it working with virtual VMU
on the Background loader without prefetch (owner, 7 Nov, no build), while
roadmap section 3 lists it as standalone demand reader with memory ownership
still to prove. The table itself marks the roadmap's section 3 entries as
"claims to confirm".
- The result of the requested Skies check "Background On / New (X) / Virtual
VMU Off" (native-skies-background72.md:82-86) was not found in the documents
read.
