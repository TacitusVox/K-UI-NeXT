# Native CD music in the code, and where it meets VMU, prefetch and the stream

An independent reader's answer as returned, unedited but for line wrapping and
paths (repository paths relative to its root, scratch copies as <scratch>). It
was asked one part of the question the owner put on 16 November 2026 after
Test 77's console result; the brief is quoted in
docs/fmv-stream-reader-plan.md. The sceptical check of these claims that was
to follow was cut off by the session's usage limit; what the author checked
himself is said in the plan.

## Summary

Today one launch loads exactly one loader file, and CD music, virtual VMU and
prefetch sit in different files, so no native game can have CD music together
with either of the other two. With CD Music On, Games forces the Standard
reader and loads a CD-music file of its own (KUI/apps/games/cdda-toy-raw.kui
for the raw Toy Commander image); that file has no virtual VMU and no
prefetch, and it reads the movies the old way, two sectors per disc call. CD
music exists for two game programs only, Toy Commander and MDK2, each admitted
by the full SHA-256 of the program and of its sound driver file plus about
twenty fixed addresses, because the adapter plays through the game's own sound
driver. The adapter runs from the game's sound-update call, reads one audio
sector per card command (75 a second, interrupts blocked for each), gives way
to the game's data reads, and keeps about 0.74 s of music buffered, so it can
wait some hundreds of milliseconds before the music gaps. Virtual VMU beside
it is not possible today: no VMU code is built into those files, no card is
prepared, the launcher refuses the launch when Virtual VMU is On and the game
already has virtual cards, and the older Toy virtual-card file needs CD music
Off and claims the same 192 KiB of game memory as the music worker. The stream
reader and the prefetch exist only in the Background family, which has no CD
music code: music commands are answered and nothing plays, as in every Test 77
run. What stands between the three: (a) no CD music adapter exists on the
Background reader, and no prefetch or VMU on the CD-music Standard files; (b)
each service takes the game's first heap block for itself, with no room or
allocator proof for another; (c) no rule or interface lets an audio reader
with a deadline share the card with a stream that stays inside one open read
and a VMU writer (the one lending interface is the Background family's and
knows one borrower), and both card paths use DMA channel 1; (d) the launcher's
one-file routing and its refusals; (e) for "universal", sound adapters for
more than two programs. None of these combinations has been built or run, and
the CD-music files now installed (source f8807cb16561) have earlier positive
music reports for their adapters but no console acceptance of their own. Read
at HEAD 1e072b2, a documentation-only commit made after 9c9867c during this
reading.

## Facts

1. (read in code) Routing in the menu: with CD Music On for a native GDI the
shell sets the reader to Standard and launches with Standard plus the CD-music
flag. Choosing another reader switches CD music off. The virtual VMU slot
choice and the card picker are skipped while CD music is On.

   Evidence: src/core/shell.c:1463-1466, 3376-3391, 1373-1374, 1295-1297, 1430

2. (read in code) Routing in the launcher: CD music is refused with any reader
but Standard and with anything but a native unscrambled GDI on SCI microSD
with high-density audio tracks. Every track is mapped, then a separate file
replaces the ordinary loader: cdda-toy-raw.kui, cdda-toy-cooked.kui or
retail-cdda-mdk2.kui (the same bytes under cdda-native-*.kui when the exact
name is absent or the disc layout differs). An unknown program stops with
"Experimental CD music: unrecognized native runtime; turn CD music Off to
launch"; nothing falls back to the ordinary loader.

   Evidence: src/apps/games_retail.c:895-898, 1079-1083, 1187-1190, 1295-1300,
   1380-1417; src/core/games_cdda_profile.h:151-176;
   include/kui/games_retail.h:45-48; docs/experimental-cdda.md lines 12-17
   (merged 2026-11-01)

3. (read in code) Admitted programs: two. Toy Commander (748,444 bytes, CRC32
cdc493b3, SHA-256 ee68a326...9bbd) and MDK2 (750,804 bytes, CRC32 682d90e3,
SHA-256 ac5826c9...bb48). The menu picks a candidate by size and CRC (the
exact route also by track geometry, IP and GDI CRC) and checks the loader file
by SHA-256. The stage hashes the whole loaded program and checks the original
word at every patched address (20 in Toy, 17 in MDK2) before any hook. The
worker hashes the sound driver file as the game loads it (Audio64 1.80d.2,
20,740 bytes, 477ede37...04e5 for Toy; 1.90b.1, 20,824 bytes, af8f9044...a413
for MDK2). Quake III is recorded as not admitted. The owner's "remember this
is a CDDA game so I need that to be universal as well" (16 November) asks for
adapters that do not exist beyond these two.

   Evidence: include/kui/native_cdda_compat.h:37-57;
   src/core/games_cdda_profile.h:36-62, 87-144, 195-260;
   src/loader/toy_pilot_stage.c:47-49, 94-115, 364-379;
   src/loader/mdk2_cdda_stage.c:45-47, 72-90, 343-351;
   src/loader/toy_pilot_worker.c:135-138, 422-456;
   include/kui/cdda_audio64_profile.h:69-102; docs/experimental-cdda.md lines
   60-78; docs/fmv-stream-reader-plan.md decision 3 (2026-11-16)

4. (read in a document) What is installed: the menu is bound to three CD-music
builds of source f8807cb16561 (toy-raw 85,312 bytes CRC 0f2775eb; toy-cooked
85,344 bytes CRC 4a77a2c6; mdk2 67,332 bytes CRC af320cfd), each delivered
under two names. Their manifest says console_tested false and claims no new
console acceptance. The coverage record of 2026-11-06 (on 9e3295bcfaec) says
the adapters "have earlier positive music reports. Toy's FMV stutter remains.
MDK2 had an occasional audio cut" and that "Independent virtual VMU,
read-ahead and CDDA combinations still need coverage".

   Evidence: resources/updater-bindings/native-cdda-manifest.json;
   resources/updater-bindings/native-cdda-manifest.h:4-8;
   docs/universal-library-coverage.md lines 24 and 70; AGENTS.md
   ("host/linked-qualified, not console-accepted")

5. (read in code) Where the audio comes from: the card, one raw 2,352-byte
sector per read (the count must be 1), through the Standard low reader: take
the SCI, one CMD18 for the card blocks under that sector, CMD12, release. Each
read is a complete card command made with SR.BL and interrupt mask 15 set. Up
to 4 sectors per worker visit (16 ms admission), two visits per sound update.
PLAY must name one whole audio track with repeat 0 or 15; PLAY by position
(command 21) is refused.

   Evidence: src/loader/toy_pilot_worker.c:1290-1297, 1362, 1477-1488,
   1647-1653, 565-597; include/kui/toy_pilot.h:33-34, 43-46;
   include/kui/toy_loader_ad_followup.h (refill 4 quanta, 12,500 ticks);
   src/loader/toy_pilot_resident.c:63-86;
   src/loader/retail_resident.c:493-535; src/loader/retail_sd.c:294-308;
   src/loader/sd_reader.c:409, 460

6. (read in code) Where the audio goes: split into left and right planes and
written by short programmed writes (at most 256 bytes a call, refused while
any G2 DMA channel is active) into a 131,072-byte stereo ring (32,768 frames,
about 0.74 s, eight blocks of 4,096 frames) in the sound chip's memory,
allocated from the game's own sound-memory manager. The game's own installed
sound driver plays it as a loop on two ports kept from the game (62 and 63 in
Toy), commanded by packets on the driver's queue; position is read from the
driver's cursor words.

   Evidence: src/loader/toy_pilot_worker.c:33-37, 754-809, 1237-1260,
   1398-1449; src/loader/toy_pilot_lease.c:70-118;
   src/loader/toy_pilot_bus.c:58-72, 152-217;
   include/kui/cdda_retail_binding.h:19-35; include/kui/toy_pilot_ring.h:7-10;
   src/loader/toy_pilot_stage.c:101-103, 331-333

7. (read in code) What drives it: no interrupt and no timer of its own. The
stage replaces the game's pointer to its sound-update routine (0x8c04a270 in
Toy) with a hook that visits the worker once before and once after the
original call, on a private 8 KiB stack. Timer channel 0 is only read as a
clock and must be in the retail setting, else the adapter faults. While paused
or stopped it makes no card reads.

   Evidence: src/loader/toy_pilot_worker.S:49-113, 342;
   src/loader/toy_pilot_stage.c:104, 334;
   src/loader/toy_pilot_worker.c:695-716, 1569-1574, 1595-1604, 1633-1634

8. (read in code) Game data while music plays: audio gives way. No audio read
starts while the low reader is active or a disc read (command 16 or 17) is
pending. The game's data reads are served inside its disc calls by the same
Standard low reader at a fixed 2 sectors per call, each its own CMD18 to CMD12
under the same lock, so one card command at a time and never two open. In the
Toy files only, data before the first PLAY goes through an interrupt-driven
receiver (own vector table, SCI level in IPRB, DMA channel 1) that is closed
for good at the first accepted PLAY; MDK2 has none.

   Evidence: src/loader/toy_pilot_worker.c:1262-1273, 1302-1309;
   src/loader/toy_pilot_resident.c:75-83;
   src/loader/retail_resident.c:1786-1800; src/core/retail_gd.c:548-573;
   Makefile.toy_pilot:10, 61; Makefile.mdk2_cdda:18-23;
   src/loader/toy_pilot_gd.c:72-87, 139-166; src/loader/toy_pilot_sci.c:12-14,
   131-151; src/loader/toy_pilot_sci.h:97-108;
   tools/build_native_cdda_compat.py:16-18

9. (read in code) Memory it reserves: (a) the low reader at 0x8c004000 to
0x8c007800 with its stack to 0x8c007d00; (b) 192 KiB at 0x8cfd0000 to
0x8d000000, taken by calling the game's own allocator once from a hook on the
game's heap start; the stage stops unless the block lands exactly there with
the exact heap bookkeeping. The worker (code, data, 8 KiB stack) must end
below 0x8cfe0000, 64 KiB, because the code treats the top 128 KiB as the game
SDK's fixed scratch; MDK2 keeps its 256-slot map at 0x8cfd8000. (c) 128 KiB of
sound memory and two driver ports.

   Evidence: include/kui/native_cdda_compat.h:33-36;
   include/kui/toy_pilot_boot.h:36-42; src/loader/toy_pilot_stage.c:256-289;
   src/loader/mdk2_cdda_stage.c:227-256, 272-290;
   src/loader/toy_pilot_worker.ld:13, 52-55;
   src/loader/toy_pilot_resident.c:50-52; include/kui/mdk2_cdda_layout.h:7-14;
   include/kui/toy_pilot.h:22-32

10. (read in code) Virtual VMU with CD music today: no. The Standard
virtual-VMU file (retail-boot.kui) is chosen only when CD music is Off; for a
CD music launch the launcher's virtual-card flag is false and no card is
prepared. The CD-music build recipes list no VMU source, and three build
guards stop VMU code from compiling into them.

   Evidence: src/apps/games_retail.c:904, 994, 1046, 1553-1569;
   include/kui/native_standard_route.h:17; Makefile.toy_pilot:241-252;
   Makefile.mdk2_cdda:32-44; src/loader/sci_sd_bus.c:10-12;
   include/kui/vmu_backing.h:6-8; src/loader/vmu_maple_observe_stage.c:4

11. (read in code) With the Virtual VMU setting On, a CD music launch is
refused when the game already has virtual cards ("this game has virtual VMU
choices; use a matching virtual-A2 module" or "this game has an imported
virtual A2 card; launch it with the combined New reader"), and for the raw Toy
image when an old vmu-toy.kui is on the card ("Virtual card requires original
GDI, SCI/Standard, Auto encoding, CD music and RAM patches Off"). Otherwise it
starts with physical VMUs only. vmu-toy.kui is in neither of the last two
delivery inventories.

   Evidence: src/apps/games_vmu_card.inc:76-119;
   src/apps/games_retail.c:1159-1181, 1557-1558;
   src/core/vmu_launch_package.c:13-15, 41-46;
   docs/evidence/reader-corrections/delivery.json;
   docs/evidence/native-shared-f8807cb/delivery.json

12. (read in code) "The older Toy virtual-VMU pass" is vmu-toy.kui: an
exact-Toy virtual card in A1 on the Standard reader, admitted only with every
launch option off (so CD music Off). Its worker claims the same block as the
CD music worker: start 0x8cfd0000, 0x30000 bytes, worker end 0x8cfe0000. The
two are alternatives by construction.

   Evidence: include/kui/games_retail.h:21-24;
   include/kui/vmu_toy_observe_admission.h:15-17;
   include/kui/toy_pilot_boot.h:40-42; src/core/vmu_launch_package.c:46;
   docs/universal-library-coverage.md line 70 (2026-11-06)

13. (read in code) Prefetch in the CD-music files: none. Neither recipe has a
read-ahead source or switch; both stages are a compile error with the
background reader; the file refuses a non-Standard reader at launch ("TOY
PILOT NEEDS STANDARD READER", "USE A; X/Y BACKGROUND IS UNSUPPORTED"); Toy's
startup receiver only delivers the request that is pending. The plan lists
"Standard SCI reader | No read-ahead lease".

   Evidence: Makefile.toy_pilot:241-271; Makefile.mdk2_cdda:32-44;
   src/loader/toy_pilot_stage.c:6-8, 71-74; src/loader/mdk2_cdda_stage.c:6-8;
   src/loader/toy_pilot_admission.h:18-27; src/loader/toy_pilot_sci.h:82-94;
   docs/fmv-stream-reader-plan.md table under "Where it can go first"
   (2026-11-16)

14. (inferred) How often it needs the card: 75 complete card commands a second
while a track plays, and about 56 to fill the ring before a track starts, all
interrupts blocked during each. The time of one is counted by the adapter
(raw_read_ticks) but no recorded value was found; a source comment puts a card
block at about 1 ms, which would make about 5 ms a sector and roughly a third
of each second.

   Evidence: src/loader/toy_pilot_worker.c:1290-1297, 1353-1376, 1489-1496,
   1636-1639; include/kui/toy_pilot.h:178-184;
   src/loader/retail_resident.c:471 (comment);
   src/loader/toy_pilot_resident.c:70-73

15. (read in code) Can it wait: within its ring. Playing starts only with all
eight blocks filled; after that a block must be refilled before the play
position comes within 5 ms of it, and the position must be seen at least every
half ring (372 ms). If not, the adapter stops the voice and fills again: a gap
in the music, counted as a recovery. A refused card take or a failed read is
different: the worker disables itself and the low reader shows its report
pages instead of going back to the game. The low SCI code refuses the take
when the SCI's receive or transmit enables are set ("belong to another user").

   Evidence: src/loader/toy_pilot_worker.c:1164-1209, 1223-1225, 1011-1027,
   1377, 249-261; include/kui/toy_pilot_ring.h:7-14;
   src/loader/toy_pilot_resident.c:165-267; src/loader/sci_sd_bus.c:142-147

16. (read in code) Lending interface: none in the CD music code as delivered.
Its only arbitration is the Standard low reader's entry lock word and the
pending-command test. kui_retail_native_async_loan_begin/end is compiled only
into the Background family, has one borrower state (WRITER), answers BUSY
while a read is pending, the receiver is active or the read-ahead cannot be
stilled, and stops that family's stream before lending. The source also holds
an undelivered Toy build switch (SHARED_SCI, with ASYNC_CDDA) in which the
high worker owns the SCI and audio claims it between data blocks; no console
result of it was found.

   Evidence: include/kui/toy_pilot.h:43-50;
   src/loader/toy_pilot_resident.c:36-37, 75-83;
   src/loader/retail_async.c:1076, 1101-1174;
   src/loader/toy_pilot_sci.h:111-136; Makefile.toy_pilot:11-12, 61, 183-197;
   tools/build_native_cdda_compat.py:16-18

17. (read in code) Shared hardware. The stream takes the SCI with receive and
its error interrupt on, DMA channel 1 for a whole run (registers saved at
start, put back at stop), a spare timer channel (2, else 1) with its level in
IPRA and a slot in the game SDK's interrupt table, and needs that table at
Toy's address 0x8c00f400. The CD music card path also uses DMA channel 1:
borrowed for one 512-byte block at a time with interrupts masked, registers
saved and put back, with a programmed fallback when the channel is busy. While
music plays it uses no SCI interrupt, no timer channel 1 or 2, and only reads
channel 0.

   Evidence: src/loader/toy_wire_stream.c:29-41, 449-463, 533-568, 570-623;
   docs/toy-wire-stream-test-77.md "Known limits" 1 (2026-11-16);
   src/loader/sci_sd_bus.c:38-45, 460-498, 617-642;
   src/loader/toy_pilot_worker.c:39-44, 695-701

18. (read in code) The two families. Native CD music exists only on the
Standard reader: shell, launcher, both profile tests and the manifest test
require it; the CD-music low reader is a wrapper compiled around the
synchronous retail_resident.c. The Background family has no CD music code: its
disc service takes PLAY, PAUSE and STOP and nothing plays (launcher log:
"commands are acknowledged silently, so music using them is absent"), and
audio tracks may be listed without their files. That is the state of every
Test 77 run ("CD Music and virtual VMU off").

   Evidence: src/apps/games_retail.c:895-898, 1191-1193, 335-337, 1361-1363;
   src/core/games_cdda_profile.h:45, 70, 122;
   include/kui/native_cdda_compat.h:70; src/loader/toy_pilot_resident.c:6-12;
   src/loader/retail_resident.c:109-112; src/loader/retail_async.c:471-473
   (the only CDDA mention, Armada CE); docs/toy-wire-stream-test-77.md line
   191

19. (inferred) No room as built. The Test 77 file's 284 KiB block is fully
given out: 72 KiB worker, map and state up to 0x13000, 28 KiB auxiliary (24
KiB image, which the plan calls full), 4 KiB stack, 180 KiB data. The CD music
worker (38,198 bytes in the RC14 audit of 2026-10-15, plus its 8 KiB stack) is
linked at the fixed address 0x8cfd0000 and has no place in that block; the 64
KiB open to the CD music worker has no place for a 72 KiB VMU worker or a
62-sector window (145,824 bytes).

   Evidence: include/kui/native_readahead_layout.h:33-45;
   include/kui/vmu_native_background.h:21-23, 45-48;
   docs/fmv-stream-reader-plan.md step 1 row;
   docs/release-v2.0-cdda-acceptance-audit.md table row 2;
   src/loader/toy_pilot_worker.ld:13, 52-53

20. (inferred) The window against music. The Test 77 window serves only
2,048-byte reads on raw data tracks and is given up on any command that is not
a read; each give-up costs a CMD12 and the next read a CMD18. An audio reader
fetching 75 sectors a second elsewhere on the card, or a game that sends music
commands or drive-status questions between reads, is such another user. The
plan's own row: "CD music | One open card stream cannot serve audio sectors
and a movie window at once".

   Evidence: src/loader/native_readahead_aux.c:446-461;
   docs/toy-wire-stream-test-77.md line 148 and "Known limits" 1 and 2;
   docs/fmv-stream-reader-plan.md line 97 (2026-11-16)

21. (read in a document) Movies under CD music. With CD Music On, Toy
Commander's movies are read by the synchronous two-sector path of fact 8, not
by the Test 77 reader. The recorded limit: "Toy Commander's intro FMV stutter
remains unresolved, even where CD music plays" (release notes, merged
2026-11-01) and "Toy's FMV stutter remains" (2026-11-06). Source comments say
the game pauses disc music around movies, a state in which the adapter makes
no card reads.

   Evidence: src/loader/retail_resident.c:1786-1800;
   docs/release-v2.0-notes.md lines 116-118;
   docs/universal-library-coverage.md line 70;
   src/loader/toy_pilot_worker.c:606-608, 705-708, 1569-1574

22. (measured here) Originals: neither game program nor either sound driver
file was found on this machine. The uploads folder holds the saved Test
reports and one handoff archive whose listed members are sources, evidence and
build archives; tests/fixtures holds no game file; a search by the four exact
sizes found nothing. Nested archives were not opened.

   Evidence: /root/.claude/uploads/ee210574-157a-5108-ac4d-f27767e180ce
   (listing and archive member names only); tests/fixtures (listing); size
   search for 748,444, 750,804, 20,740 and 20,824 bytes under /home/user and
   the uploads folder

## Open questions

- How long one audio sector read takes on the console, and so what share of
each second the card is taken and interrupts are blocked while music plays.
The adapter counts it (raw_read_ticks) but I found no recorded value; the
one-third figure rests on a source comment.
- Whether Toy Commander plays disc music while it loads levels, which is where
an audio reader and a stream window would meet, and whether it pauses music
for every movie. Only source comments speak to this; no console record was
read.
- Whether the owner's card still holds an old KUI/apps/games/vmu-toy.kui, and
whether Toy Commander has virtual cards registered. Either makes the launcher
refuse CD Music On while Virtual VMU is On; the card's file list would settle
it.
- The console standing of the f8807cb16561 CD-music files themselves. Their
manifest says not console tested; the positive music reports in the 2026-11-06
coverage record are called "earlier" and name no build.
- Where the Background family's 284 KiB block lands in Toy Commander's memory,
and whether that agrees with the CD music code's statement that 0x8cfe0000 to
0x8d000000 is the game SDK's own scratch. Not checked; it belongs to the
readers covering memory in that family.
- Whether the undelivered SHARED_SCI and ASYNC_CDDA build of the Toy adapter
(audio claiming the SCI between data blocks) was ever run on the console. No
record found under those names.
- Which other native games in the owner's library use disc music and which
sound drivers they carry. The coverage record names Record of Lodoss War,
Grandia II and Quake III as gaps; adapters exist for two programs only, and no
analysis of the newly supplied driver files was read here.
