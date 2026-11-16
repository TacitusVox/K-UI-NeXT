# Virtual VMU and the stream reader: what a launch with VMU On does

An independent reader's answer as returned, unedited but for line wrapping and
paths (repository paths relative to its root, scratch copies as <scratch>). It
was asked one part of the question the owner put on 16 November 2026 after
Test 77's console result; the brief is quoted in
docs/fmv-stream-reader-plan.md. The sceptical check of these claims that was
to follow was cut off by the session's usage limit; what the author checked
himself is said in the plan.

## Summary

If you turn the virtual VMU On and run this test build, nothing changes: you
get the same run as before. The Card Tools entry "66 SCI movie playback" does
not read your VMU setting; it sets the virtual VMU Off itself for every Toy
test launch and starts the module in its "reader only" mode, so the real VMUs
in the controller keep working, no virtual card appears, and the stream window
opens as in Test 77. The test module could not do otherwise: its VMU worker is
built without the virtual-card code and refuses to start in any other mode. A
launch that did reach it with VMU On would stop on a refusal screen (PHASE 9,
DETAIL 1) before the game runs, with no stream window and no read-ahead; that
needs the test file put in the installed reader's place, which the package
does not ask for, and I traced it in the code and the linked worker without
running it. So the stream reader has never run beside the virtual VMU. With
the installed reader (a normal Games launch on Background New), Toy Commander
loads KUI/apps/games/retail-native-dual.kui for both settings: VMU On gives a
virtual card in A2, VMU Off passes the real VMUs through. Both keep the 284
KiB reservation and the ordinary 192 KiB block read-ahead, which reads at most
two card blocks per controller frame and pauses itself while the game streams
a movie; the stream window is not in that module. No console run of Toy
Commander on the installed Background module with VMU On is recorded; the
recorded Toy virtual-VMU pass used an older, separate module. The repository
head moved from 7456726 to 12f9b03 while I read (documents and evidence only),
so the source line numbers below hold for both.

## Facts

1. (read in code) Q1. The owner's VMU setting is a saved record on the card
(two slots; On when both are missing). The Games app reads it at every native
launch and logs 'Native VMU emulation: On/Off; background reader selection is
unchanged'.

   Evidence: include/kui/vmu_emulation_settings.h:8-11;
   src/core/vmu_emulation_settings.c:57-67; src/apps/games_retail.c:929-937

2. (read in code) Q1. For a Background launch (reader New/X or Legacy/Y,
'native_dual') the Games app writes the setting into the loaded module in
memory only: it finds the one valid 32-byte KUINVM01 record and stores mode 0
(KUI_VMU_NATIVE_MODE_NORMAL) for On or 7 (KUI_VMU_NATIVE_MODE_READER_ONLY) for
Off. Off needs the module's toggle capability record; a missing or ambiguous
record refuses the launch. This is the only caller.

   Evidence: src/apps/games_retail.c:902-903 and 1060;
   src/apps/games_vmu_card.inc:123-157; include/kui/vmu_native_mode.h:7-18;
   include/kui/vmu_native_toggle.h:6-32

3. (read in code) Q1. The stage carries that record with mode 0 as shipped,
decodes it in native_runtime_mode() (an invalid record stops with 'NATIVE VMU
RUNTIME MODE REFUSED'), and hands the worker a 'diagnostics' value: mode 7
becomes KUI_VMU_NATIVE_DIAGNOSTICS_READER_ONLY (5), mode 0 becomes NORMAL (0).

   Evidence: src/loader/vmu_native_heap_probe_stage.c:57-64 and 211-231;
   src/core/vmu_native_mode.c:7-17;
   src/loader/vmu_native_passive_stage.inc:309-317 and 408;
   include/kui/vmu_native_passive.h:28-35

4. (measured here) Q2. Nothing in this tree's GUI source launches the test
module. include/kui/toy_suite.h and src/apps/toy_suite_report_prepare.inc do
not exist here and src/apps/games_retail.c has no Toy test path; the tree only
builds the module and checks Card Tools' admission with copies of its
predicates. The Card Tools source is commit 09c9a4b686d4, the tip of the local
branch toy-gui-history, which is not an ancestor of HEAD.

   Evidence: ls and grep in /home/user/K-UI-NeXT (no include/kui/toy_suite.h,
   no src/apps/toy_suite*); git merge-base --is-ancestor 09c9a4b686d4 HEAD
   exits 1; Makefile.vmu-native-toy-suite:317;
   tests/test_toy_reader_package.c:1-12

5. (read in code) Q2. The entry is Card Tools, a game's Options, Advanced,
Tests, '66 SCI movie playback'. It builds the launch request as reader New
plus the test number (KUI_RETAIL_READER_ASYNC | 66<<16) and nothing else, and
the module path is 0:/KUI/apps/games/retail-toy-test-66.kui. Card Tools has no
Test 77 entry; the Test 77 file is installed at that path.

   Evidence: git show 09c9a4b686d4:src/core/shell.c:3604-3607; git show
   09c9a4b686d4:include/kui/toy_suite.h:18-19, 43, 164-168 (repository
   /home/user/K-UI-NeXT); docs/toy-wire-stream-test-77.md:15-16;
   docs/evidence/toy-wire-stream-77/package-README.txt:36-39 and 71-75

6. (read in code) Q2. That entry forces the mode and does not pass the setting
on. For a Toy test the code sets emulation_enabled=false in place of loading
the saved setting, drops RAM patches, card selection and session, writes mode
7 into the module, and prepares no card. It logs 'Toy movie test %u: %s,
%u-byte image, CD music and virtual VMU Off'; the Tests page prints 'Toy tests
use New (X), CD music and virtual VMU Off.' The launch does not rewrite the
saved setting. The forced-Off line is in every recorded Card Tools snapshot
back to Test 33.

   Evidence: git show 09c9a4b686d4:src/apps/games_retail.c:865-877, 947-955,
   1015-1019, 1089, 1582-1600; git show
   09c9a4b686d4:src/dreamcast/shell_draw.c:3019-3021; git show
   09c9a4b686d4:src/dreamcast/main.c:914-915 and 4118-4140; git log
   -S'if(toy_test)emulation_enabled=false;' toy-gui-history reaches e3db7c7
   (repository /home/user/K-UI-NeXT)

7. (measured here) Q2. The Card Tools binary shipped in the Test 77 package
agrees with that source: SHA-256 04b8657a...c800 as the Test 77 record's
Package table has it, build label 09c9a4b686d4, and it contains both strings
quoted above and the path retail-toy-test-66.kui. That it was compiled from
exactly that commit is inferred from the label (the GUI's build label is the
12-digit commit id) and the strings.

   Evidence: sha256sum and strings on <scratch>/pkg77/stage/KUI/tools.kui; git
   show 09c9a4b686d4:Makefile.dc:148-160; docs/toy-wire-stream-test-77.md
   section Package

8. (measured here) Q2. The Test 77 module as packaged (SHA-256
1c1e54f3...7323) has one valid mode record at payload offset 0x20260 holding 0
(NORMAL) on disk, a toggle capability at 0x34300, and a card profile slot at
0x202c0 that still holds the build template ('KUI-P2-TEMPLATE!'). The recorded
admission says 'emulation Off: ok, mode slot at 0x20260 (GUI writes 7 at
+16)'.

   Evidence: byte scan of
   docs/evidence/toy-wire-stream-77/retail-toy-test-77.kui;
   docs/evidence/toy-wire-stream-77/package-admission.log:7-8;
   tools/vmu_persist_profile.py:268-293

9. (read in code) Flags that differ. The Test 77 recipe is the production
compact Background recipe plus -DKUI_NATIVE_TOY_MOVIE_CAPTURE=1,
-DKUI_NATIVE_TOY_PLAYBACK_CAPTURE=1, -DKUI_TOY_SUITE_MODE=66 and
-DKUI_TOY_WIRE_STREAM=1 (with the TOY_WIRE switches). The installed
retail-native-dual.kui is built by Makefile.vmu-native-background-compact,
where none of the four is defined. Both share KUI_NATIVE_BACKGROUND_COMPACT=1,
KUI_NATIVE_READAHEAD=1 and KUI_NATIVE_SPLIT_CONTROLLER_STREAM_GUARD=1.

   Evidence: Makefile.vmu-native-toy-suite:69-72 and 111-122;
   Makefile.vmu-native-toy-playback:3-5; Makefile.vmu-native-toy-movie:4-6;
   Makefile.vmu-native-background-compact:1-13;
   tools/update_build_modules.py:39-43;
   <scratch>/clean77/build/toy-suite-77/toy-suite-config

10. (measured here) Q3. With KUI_TOY_SUITE_MODE set, the worker's
reader_only() returns true always and its arm function returns -2 for any
configuration that is not READER_ONLY. Without the flag (production)
reader_only() follows the launch mode and NORMAL is accepted. The linked Test
77 worker shows the same: the arm function compares the diagnostics word with
5 and otherwise clears its configuration and returns -2; the two card handlers
are reduced to 'return 0'; no kui_vmu_durable_* function is linked.

   Evidence: src/loader/vmu_native_passive.c:317-345 and 1655-1663;
   sh-elf-objdump and sh-elf-nm on
   <scratch>/clean77/build/toy-suite-77/native-worker.elf
   (_kui_vmu_native_passive_arm 0x8c102734: cmp/eq #5 at 0x8c102938, return -2
   at 0x8c102946; _persistent_handler 0x8c100114; _persistent_admit
   0x8c100118); listing kept at <scratch>/vmu/part1/arm77.dis

11. (read in code) Q3, READER_ONLY (what Card Tools sets) in the test build.
The stage skips all virtual-card preparation. toy_movie_scan runs for the
exact Toy identity on reader New and marks a raw movie track as source 1,
which the auxiliary needs before any window opens. The worker leaves the
controller's replies as the real devices sent them, so physical VMUs work and
no virtual A2 is announced. The screen says 'NEW READER / VIRTUAL A2 OFF' and
'SCI MOVIE READ AHEAD READY'.

   Evidence: src/loader/vmu_native_persist_stage.inc:165-170;
   src/loader/toy_movie_iso_stage.inc:115-129;
   src/loader/vmu_native_readahead_stage.inc:186-195 and 304-305;
   src/loader/vmu_native_passive.c:2271-2275 and 2318-2322;
   src/loader/vmu_native_passive_stage.inc:666-671

12. (inferred) Q3, NORMAL (VMU On) in the test build: the launch stops; it
does not run with physical VMUs, with a window or with read-ahead. Storage
start demands a card profile inside the module that fits the card, else
'NATIVE VMU PREPARED PROFILE / GAME MAP REFUSED'; with one it runs 'NATIVE
VMU: LIVE OWNERSHIP / RECOVERY - KEEP POWER ON'. toy_movie_scan returns at
once (source stays 0). After the game's allocator has given the 284 KiB lease,
the worker's arm returns -2 and the stage shows 'PERSISTENT VMU PREPARATION
STOPPED', PHASE 9, DETAIL 1, 'PROBE REFUSED - PHOTO THEN POWER OFF'. The
read-ahead auxiliary is bound only after a successful arm. Each step was read
at the lines given; the start itself was not run on host or console.

   Evidence: src/loader/vmu_native_heap_probe_stage.c:69-73, 345-366, 594-595;
   src/loader/vmu_native_persist_stage.inc:171-175 and 192-205;
   src/loader/retail_stage.c:1456 and 1565-1568;
   src/loader/toy_movie_iso_stage.inc:115-121;
   src/loader/vmu_native_passive_stage.inc:394-400, 408, 530-532, 601-602

13. (inferred) Q3. NORMAL cannot be reached from the Card Tools entry, which
always writes mode 7. It would take a GUI that passes the setting on, that is
the ordinary Games app with the test file at
KUI/apps/games/retail-native-dual.kui. Its module checks (dual descriptor,
lease 0x47000, toggle, mode record, card profile slot) are the ones the Test
77 file passes for Card Tools, and with VMU On that GUI also fills the card
profile, so the stage's ownership and recovery step would run on the real
virtual card before the stop. The package installs the file only at the Test
66 path.

   Evidence: src/apps/games_retail.c:1053-1060 and 1553-1568;
   docs/evidence/toy-wire-stream-77/package-admission.log:1-12;
   docs/evidence/toy-wire-stream-77/package-README.txt:71-75

14. (read in code) Q3. The ordinary block read-ahead is not active in the Test
77 module in any mode. Mode 66 makes KUI_TOY_SUITE_ASYNC_DEMAND true: the
auxiliary's heartbeat event does nothing, deliveries go to the Toy path, and
the worker's controller-frame tick is replaced. The arena is 0x2d000 bytes for
the window and the ring where production has the 0x30000 cache; the linked
auxiliary has the cache's init function only.

   Evidence: src/loader/sci_stream.h:5-41;
   src/loader/native_readahead_aux.c:1596-1601 and 1615-1620;
   src/loader/vmu_native_reader_capture.inc:251-267;
   include/kui/native_readahead_layout.h:32-46 and 62-84; sh-elf-nm on
   <scratch>/clean77/build/toy-suite-77/native-ra-aux.elf

15. (read in code) Q4, file. A normal Background launch loads
0:/KUI/apps/games/retail-native-dual.kui. The A1 variant
retail-native-dual-a1.kui is taken only when the launch asks for the card in
A1 and VMU is On; 'Off always takes the A2 module'.

   Evidence: include/kui/native_background_route.h:8-15;
   include/kui/games_retail.h:10-16; src/apps/games_retail.c:988-992

16. (read in code) Q4, VMU On against Off in the installed module. On: mode 0,
the GUI puts the game's own card into the module, the stage prepares it and
the 72 KiB worker serves a virtual card in A2. Off: mode 7, no card is looked
up, and the GUI logs 'Virtual A2 Off: physical VMUs pass through; the full
reader cache remains reserved'. Both keep the whole 284 KiB reservation.

   Evidence: src/apps/games_retail.c:1553-1571;
   src/apps/games_vmu_card.inc:80-86;
   docs/shared-loader-performance-policy.md:44-50;
   docs/universal-library-coverage.md:23

17. (read in code) Q4, read-ahead in the installed module, the same for both
settings: a 192 KiB cache of 384 card blocks, filled from the controller-frame
tick with at most two blocks a visit (one for the early SDK family);
'Reader-only mode intentionally uses the same family policy'. The stream guard
skips the tick while the game streams (more than 96 blocks in the last 32
controller frames, 'an FMV') and, with VMU On, while a virtual-card transfer
is in flight. There is no stream window: KUI_TOY_WIRE_STREAM is not defined in
that recipe.

   Evidence: include/kui/native_readahead_layout.h:62-67 and 81-84;
   include/kui/native_readahead.h:53-58 and 140;
   src/loader/vmu_native_reader_capture.inc:198-223 and 268-280;
   Makefile.vmu-native-split-controller-stream-guard:5-7;
   src/loader/vmu_native_readahead_stage.inc:357

18. (read in code) Q4, the title's constants. KUI_VMU_LAUNCH_TOY_* (GDI CRC
e25531d1, IP CRC 38ba2868, executable CRC cdc493b3, 748,444 bytes) gate the
optional exact-title module vmu-toy.kui, the test module's movie scan and Card
Tools' identity check. vmu-toy.kui is looked for only when VMU is On and the
launch is not Background (optional_virtual = emulation_enabled &&
!native_dual), and not once the Standard virtual-A2 module was recognised, so
never on a Background New launch.

   Evidence: include/kui/vmu_launch_package.h:18-24;
   src/core/vmu_launch_package.c:13-15 and 41-55;
   src/apps/games_retail.c:1159-1181 and 1437-1456;
   src/loader/toy_movie_iso_stage.inc:115-121

19. (read in a document) Q4, the documents. Toy Commander is listed as
admitted by the compact Background module with virtual VMU; that is the
launcher's static checks run on a host, not a console run. Its recorded
console virtual-VMU result ('sees imported saves, creates new saves and loads
them after reboot; CD music works; intro movies still stall') has no build
recorded and comes from the dedicated Toy module of October, 'a separate
configuration'. accepted-progress.md has no Toy Commander entry for this
family; games-background-reader.md describes the X/Y readers of release 1.8.5
and says nothing of VMU or Toy.

   Evidence: docs/library-compat-table.md:9-12 and 54;
   docs/evidence/library-console-results.json (games entry 17);
   docs/VMU-RESET-HANDOFF.md:6-7 and 23-28;
   docs/universal-library-coverage.md:70; docs/accepted-progress.md:13-27;
   docs/games-background-reader.md:1-40

20. (not established) The stream reader has not run beside the virtual VMU.
Both Test 77 console runs were 'with CD Music and virtual VMU off'; the plan
lists 'Virtual VMU on beside the stream' as step 6 and names what is missing:
'The stream has to stop for the worker's use of the card and start again'. How
Toy Commander behaves on the installed Background module with VMU On (movies,
saves) is in no record I found.

   Evidence: docs/toy-wire-stream-test-77.md:190-191;
   docs/fmv-stream-reader-plan.md:98 and 122

## Open questions

- Is the Card Tools on the owner's card build 09c9a4b686d4, and was that
binary compiled from commit 09c9a4b686d4? The README requires that build and
the packaged binary's label and strings agree with the commit, but
tests/test_toy_reader_package.c:8-9 says the source that built it is in the
handoff bundle, not in this tree.
- The VMU On start of the test module was traced in the source and the linked
worker, not run. Whether the build-template card profile is refused at storage
start on the owner's card depends on the card's geometry (the template
describes a partition from sector 2048 to 1048576), which is not in the
repository.
- What the stage's ownership and recovery step writes to the virtual card's
files before the worker's refusal, in that same hypothetical start, was not
traced.
- Which build of retail-native-dual.kui is on the owner's card: AGENTS.md
names f8807cb16561, the coverage record 9e3295bcfaec (plan decision 8). Path,
build and payload CRC have to be read from the card.
- No console record exists of Toy Commander on the installed compact
Background module with VMU On. Whether its movies and saves work there today
is unknown.
