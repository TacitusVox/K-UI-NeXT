# The stream reader beyond Toy Commander: a map

An independent reader was asked, after the Test 76 console run, to map
what in the Toy Commander stream reader is specific to that title and
what is general, how the installed readers of the other launch families
work, and what a staged plan under this repository's rules could look
like. It read code and documents and ran no build or test. What follows
is its answer as returned, unedited but for line wrapping and for paths,
which are given as the repository has them (`<scratch>` stands for the
reader's own directory, which is not kept). Verdicts are its own:
`confirmed` for what it read in code or documents, `new` for its own
proposals, `corrected` where it corrected the question put to it. The
author's use of it is `docs/fmv-stream-reader-plan.md`.

## Summary

The stream reader cannot be packaged for every game as it stands. Its card
stream, sector checks, block feed and memory layout are already general, but
three things tie it to Toy Commander: what opens and closes the window (hooks
at fixed addresses of one exact executable), the Toy-suite builds of the low
residents it runs on, and the test wiring.

**The hiccup.** It is measured, but its cause is not established.
- It sits about 39 s into the 65.6 s second movie, not near the end: 0.466 s
without a new frame after frame 1,147, then 26 frames skipped.
- All of it lies in the eighth of the twelve 5.37 s pieces (137 frames instead
of 161).
- The card's data was on time there: requests waited under 21 ms in total, and
the wire stood still for 0.08 s.
- Two readings fit every row: the player was short of processor time, or its
own buffer ran dry. The report has no row that decides between them.

**Where it could go first.** Only one launch family already has what the
reader needs: compact Background "New" on a raw SCI GDI image. It has the 284
KiB lease, the SCI interrupt route and the auxiliary. The Toy stream layout
fits inside that same lease, so it needs no more of the game's memory than the
accepted reservation. It does replace the 192 KiB block cache with a 62-sector
window and a 32 KiB ring.

**What blocks a general build.**
- Nothing in the reader detects sequential reading by itself; a
title-independent trigger has to be written.
- The player's update hook is one of the reader's pump sources. Whether the
reader keeps Test 76's result without it is unknown.
- The auxiliary's image is full (24,576 of 24,576 bytes).
- The reader works only on raw 2,352-byte Mode 1 images.

**Plan.** The points give a staged plan, each step with its baseline, its one
change, what is rebuilt and its console test, followed by the owner's
decisions. The first technical step is the title-independent trigger on the
Toy module itself, compared against Test 76.

I ran no build or test. Everything is from reading code and documents and from
decoding the four console reports with a small script:
<scratch>/agent-map/decode.py

## Points

1. (corrected, certain) The repository's HEAD is 94080af, two commits past the
61e24e9 named in the task. Both later commits add only documents and evidence,
among them the Test 76 record with its console result. No file under src,
include, tools, tests or any Makefile changed, so every line reference below
holds for both commits.

   git log --oneline -4 (94080af, 10abc4c, 61e24e9, 4be776b); git diff --stat
   61e24e9 HEAD -- src include tools tests 'Makefile*' AGENTS.md is empty;
   docs/toy-wire-stream-test-76.md and
   docs/evidence/toy-wire-stream-76/ are new.

2. (confirmed, certain) Hiccup, measured: the second movie submitted 1,942 of
its 1,968 frames and skipped 26. The longest stretch without a new frame was
363,719 ticks (0.466 s, fourteen frame periods) right after frame 1,147, and
the player's first skip was at its frame 1,162. That is about 39 s after the
movie started, a little under 60% of the way through, not near its end. All
missing frames lie in the eighth of the twelve 5.37 s pieces (37.6 to 42.9 s),
which has 137 frames where a whole piece has 161 or 162. Every other piece is
whole.

   docs/evidence/toy-wire-stream-76/console-report-66-10.txt
   rows 'I SUBMIT CADENCE MAX AT' = 796 78E 58CC7 47B, 'I FIRST LAST SKIP EXIT
   SF' = 98807 7B0 1A 7DC 48A, TF = 88A3A2A1 A2A2A089 A1A2A1D7. Row meanings
   and packing (earliest piece is the highest byte):
   src/loader/toy_suite_report_save.inc:58-67, 90-94,
   117. The same reading is recorded at
   docs/toy-wire-stream-test-76.md:479-516.

3. (confirmed, certain) Hiccup, measured for that eighth piece: requests whose
wait ended there waited less than 21 ms in total (TW 00). The wire stood still
for timer waits for 0.08 s (TP 04). The reader used 0.73 s of the 5.37 s,
13.7% of the processor (TR 23 hex). 2,400 sectors were served against an
average pace of about 2,110 (TD 96 hex). The player's own buffer at its
emptiest held about 28 KiB of its 804 KiB (TB C2 of C9). Over the whole run
the reader's safety counters stayed at zero (K MB 0, LB 0, AHEAD STOP 0, W ERR
0).

   Same report: TW = 3000000 0 1150003, TP = 20302 1020104 5195926, TR =
   18201F1F 1D1F1C23 27240C1B, TD = 7B999395 94949396 A18B2669, TB = 89C1B3AE
   A3A4C1C2 C1C21146, 'D N T SEEK CAP' = ... C9120. Row definitions:
   src/loader/toy_suite_report_save.inc:102-141.

4. (not-determinable, certain) Hiccup, cause: not decidable from this report.
Two readings fit every row. (1) The player was short of processor time: its
call that shows a frame takes 30.5 ms on average of a 33.2 ms frame period,
and this stretch was the worst in Tests 74 and 75 with hardly any request
waiting. (2) The player's own buffer ran dry: it is at its floor in that
piece. Against the second, the buffer is just as low in four pieces that lost
no frame. What would decide it is the time during which the player's next
chunk was missing from its buffer. The game already calls a loader hook at
that place, but its handler does nothing in this build and the auxiliary has
no room for a new row.

   Report row 'IP ON OT OC OI SN ST' = 795 2C18444 2C1F61E 286831 2 182C
   (46,236,740 ticks over 1,941 calls). Tests 74/75 TF rows in
   docs/evidence/toy-wire-stream-74/console-report-66-8.txt
   and
   docs/evidence/toy-wire-stream-75/console-report-66-9.txt
   (pieces 7 and 8: 25 and 59; 54 and 93). The repo's own conclusion:
   docs/toy-wire-stream-test-76.md:550-602. Handler is a
   no-op: src/loader/native_readahead_aux.c:1131-1137.
   Image full: docs/toy-wire-stream-test-76.md:343-350.

5. (new, likely) Hiccup, one gap in the evidence: the Test 76 report cannot
say when the second movie's window was opened the second time (PIPE I = 2),
because the row giving the reasons windows were given up was removed in Test
76. Test 75 had the same two openings per movie, with two drops for the reason
'other'. A reopen near each movie's start is therefore the likely explanation.
A window given up at the hiccup would be expected to show as a third opening,
but no row excludes it.

   src/loader/toy_suite_report_save.inc:77-85 (rows
   removed); Test 76 PIPE row = 2 2 33E0 63FA 35 FD; Test 75 PIPE = 2 2 ...
   and 'X E N I C W O' = 0 2 0 0 0 2; drop reasons at
   src/loader/toy_movie_ahead.inc:494-510.

6. (confirmed, certain) Q1, Toy-specific: the movie logic is enabled for one
exact executable only. The stage sets the owner flag only when the movie's
track is raw, the executable is 748,444 bytes long and its SHA-256 is
ee68a326...9bbd (1GUTH.BIN). The movie scan before it also requires Toy
Commander's IP.BIN checksum, executable checksum and size, the asynchronous
reader, and reader-only mode (virtual VMU off).

   src/loader/vmu_native_readahead_stage.inc:189-195;
   src/loader/toy_movie_iso_stage.inc:115-121.

7. (confirmed, certain) Q1, Toy-specific: where the movies are. The stage
looks up four fixed file names in the disc's root directory
(50HZ_BUMPER_LRTF.AVI, 60HZ_BUMPER_LRTF.AVI, 50HZ_INTRO_LRTF.AVI,
60HZ_INTRO_LRTF.AVI) and passes their sector ranges to the auxiliary. The
variant numbers 1 to 4 are those four files in that order. Movie B (the
bumper) is variants 1 and 2; movie I (the intro) is variants 3 and 4. The hook
recognises the same names inside the player's context at offset 0xbc.

   src/loader/toy_movie_iso_stage.inc:72-113;
   src/loader/vmu_native_readahead_stage.inc:183-185;
   src/loader/native_readahead_aux.c:800-810, 821-842;
   src/loader/toy_movie_exit_hook.inc:438-452.

8. (confirmed, certain) Q1, Toy-specific: five hooks patched into the game's
code at fixed addresses, each only after the original value is confirmed. They
are 0x8c06d428 (original 0x8c08d638, the player's update pointer: start,
update, exit witness, and the call that pumps the reader), 0x8c08d950
(original 0x8c08edbc, chunk lookup), 0x8c098a88 (original 0x8c097186, buffer
refill), 0x8c04c568 (original 0x8c0fc464, frame presentation) and 0x8c04c774
(original 0x8c04c238, render entry). The update hook tells its callers apart
by the return addresses 0x8c06d3f0, 0x8c06d39a and 0x8c06d3c0. It reads the
player's context at offsets 0xbc, 0x1c0, 0x244, 0xa0, 0xac and 0x90, the lag
word at 0x8c1138c4, command 0x0200 and statuses 0x07db, 0x07dc and 0xf843.

   src/loader/vmu_native_readahead_stage.inc:245-278;
   src/loader/toy_movie_exit_stage.inc:4-13;
   src/loader/toy_movie_exit_hook.inc:21-57, 423-459;
   src/loader/native_readahead_aux_entry.S:14.

9. (confirmed, certain) Q1, Toy-specific: how a movie's start and end are
detected. A movie counts as playing while the exit witness written by the
update hook says started and not ended, with start status 0x07db. The window
is given up as soon as that no longer holds. The clock value of the movie's
start, from which every per-piece row is counted, is taken at the same hook.

   src/loader/toy_movie_ahead.inc:134-139, 494-510;
   src/loader/native_readahead_aux.c:539-549;
   src/loader/toy_movie_present_trace.inc:5-50;
   src/loader/toy_wire_ahead.inc:230-237.

10. (confirmed, certain) Q1, Toy-specific: which requests open and use the
window. A request is 'selected' only if it lies wholly inside one of the four
movie files, at least 386 sectors into it, on a raw track, while that movie is
playing. It must also be a fresh request of at most 60 sectors whose cursor
matches. The window is then opened from that request to the end of the movie
file. Nothing in this path detects sequential reading by itself.

   src/loader/native_readahead_aux.c:445-463, 521-553;
   src/loader/toy_movie_ahead.inc:14-21, 158-197,
   451-486, 514-527.

11. (confirmed, certain) Q1, Toy-specific: two further scheduling inputs. The
player's update call is passed to the reader as a visit of its own when the
reader needs the foreground, or else once in 4 ms. The game's disc calls are
classified as polling or blocking by fixed return addresses inside this
executable's SDK (0x8c0bd55a, 0x8c0b1fe6, 0x8c0bd374, 0x8c0b145c, 0x8c0b3b88,
0x8c097144, 0x8c09712a).

   src/loader/native_readahead_aux.c:485-501, 1327-1349,
   1443-1459; src/loader/toy_wire_ahead.inc:667-712;
   src/loader/toy_movie_exit_hook.inc:431-433.

12. (corrected, certain) Q1, player measurements, with a correction to the
task's list. The reader reads the player's output and skip counters and its
frame number from the player's context. For row TB it follows three pointers
from the context to the player's AVI object and reads the buffer's capacity
and fill. In the Test 76 configuration the routine toy_chunk_buffer is
compiled out, the refill hold that reads 0x8c0b0228 returns at once without
reading it, and the chunk hook's handler does nothing. TB comes from inline
code in toy_wire_update_seen.

   src/loader/toy_wire_ahead.inc:714-783;
   src/loader/native_readahead_aux.c:855-870, 930-942,
   967, 1131-1137; switch defaults at
   Makefile.vmu-native-toy-suite:11-30.

13. (confirmed, certain) Q1, Toy-specific constants inside the otherwise
general card stream: the SDK's interrupt table base (0x8c00f400), its SCI
error slot (0x8c00f65c) and its default handler (0x8c0be408). They are used
only to find a free timer and to install and remove the timer's callback.

   src/loader/toy_wire_stream.c:48-53, 170-173, 449-463,
   559-563, 574-579.

14. (confirmed, certain) Q1, Toy-specific and easy to miss: the low residents
(the small readers kept below the game) in the test module are Test 66's
Toy-suite builds, not the production ones. The stream's glue reads and writes
fields of the low reader's state that exist only in that configuration
(toy_suite_active, token_bounded, token_budget). The shared low-reader sources
carry many Toy-suite conditionals.

   src/loader/sci_stream.h:141-153;
   src/loader/toy_wire_ahead.inc:502-504, 520-522,
   548-550; src/loader/toy_movie_ahead.inc:285-293;
   Makefile.vmu-native-toy-suite:105-113;
   docs/toy-wire-stream-test-76.md:311-316; grep counts
   of KUI_TOY_SUITE: 86 in src/loader/sci_stream.c, 32 in
   src/loader/retail_async.c.

15. (corrected, certain) Q1, test wiring, with a correction. The module is
built by Makefile.vmu-native-toy-suite with TOY_SUITE_MODE=66 and the TOY_WIRE
switches, which name it Test 76 and copy the compact Background module to
retail-toy-test-76.kui. The layout header refuses the stream outside mode 66.
The report rows and the 'TOY TEST nn' title are in the worker. The Card Tools
entry '66 SCI movie playback' is not in this branch's source: it is commit
09c9a4b on branch toy-gui-history, which is not an ancestor of HEAD. The
package ships that Card Tools build as an unchanged file and installs the
module at the Test 66 path.

   Makefile.vmu-native-toy-suite:3-62, 105-116, 286-293,
   323-328; include/kui/native_readahead_layout.h:32-46;
   src/loader/toy_suite_report_save.inc:69-143;
   src/loader/vmu_native_passive.c:1021-1045; git branch
   -a --contains 09c9a4b686d4 gives toy-gui-history, touching
   include/kui/toy_suite.h and src/core/shell.c;
   <scratch>/pkg76/stage/README.txt:137-142,
   308-311.

16. (not-determinable, likely) Q1, admission rules as coded: the movie's track
must be raw (2,352-byte sectors with a 16-byte header) and lie in the image
map, and the request's track is rechecked on every request. The rule that
tracks begin at their file's first byte is stated in the Test 76 record as
this title's admission. In the reader it matters only for the word-wise window
copy, which falls back to the ordinary copy for anything not a multiple of
four. I did not find where that rule is enforced.

   src/loader/native_readahead_aux.c:453-461;
   src/loader/toy_movie_iso_stage.inc:19-31, 124-129;
   src/loader/toy_movie_ahead.inc:52-70, 158-170;
   docs/toy-wire-stream-test-76.md:164-168;
   src/core/retail_cursor.c:14-15, 65-69.

17. (confirmed, certain) Q2, general already: the card stream. It takes
whatever the open multi-block read sends, by DMA, into a 32 KiB ring. It
records the position of every lost byte, restarts reception after each
overrun, and hands on 512-byte blocks marked clean, rebuilt or with lost
bytes. Its interface names no game. It requires the card left in an open CMD18
just behind a block's checksum and deselected, the SCI in the bus's fast
command setting, DMA channel 1 idle with the DMA controller enabled and
error-free, and all calls made with interrupts masked. It saves and restores
channel 1's registers and stops with a fault if another owner takes the
channel. The installed SCI reader uses the same channel, so this is not a new
demand on a game.

   src/loader/toy_wire_stream.h:8-27, 175-214;
   src/loader/toy_wire_stream.c:38-41, 249-264, 304-331,
   516-568, 570-623; production producer's idle test on the same channel:
   src/loader/native_readahead_aux.c:91-96, 353-356.

18. (confirmed, certain) Q2, general with one requirement per SDK family: the
stream's wait timer. At each stream start it tries TMU channel 2, then 1. It
takes a channel only if it is stopped, has no underflow interrupt enabled, has
interrupt level 0, and its slot in the SDK's interrupt table still holds the
SDK's default handler; the vector base must also be the SDK's. It then writes
its callback into that slot and sets the channel's level to 1; both are undone
at stop. With no free channel it runs without a timer, and a wait ends at the
next call that passes by. The console run used TMU1.

   src/loader/toy_wire_stream.c:175-187, 398-402,
   423-447, 449-463, 559-563, 574-579; report row 'W BYTES WTICK TMR ERR DET'
   = 44C5F16 3C7E8B 1 0 0.

19. (confirmed, certain) Q2, general with a requirement: time is read from TMU
channel 0, which the reader never writes. A reading counts as valid only with
the expected clock setting: FRQCR low bits 0x0e0a, TMU0 running from a reload
of 0xffffffff at one sixty-fourth of the peripheral clock, without interrupt.
Without a valid clock the reader still works, but an interrupt visit reads at
most four blocks, a waited request does not keep its window, and the low
reader's seek loop stops after one step.

   include/kui/toy_pilot_clock.h:8-11;
   src/loader/native_readahead_aux.c:78-87, 469-472;
   src/loader/toy_wire_ahead.inc:514, 569-573, 843-846.

20. (confirmed, certain) Q2, general with a requirement: the SCI's
receive-error interrupt must reach the reader. It does so through the low
resident's callback in the SDK's own interrupt table at SCI level 1, the
lowest. The stream is started only when that route is armed at exactly that
level; otherwise the old block-by-block reader stays in charge. The route
needs the game's vector base and its two SCI slots to be the admitted ones.
Because level 1 is the lowest, every stretch in which the game holds
interrupts off or runs other handlers costs wire time. The design accepts that
and counts it (19,913 overruns in a run and 8,368 timer waits in the Test 76
run).

   src/loader/toy_wire_ahead.inc:454-478, 507-513;
   src/loader/retail_async.c:95-99, 278-335;
   include/kui/native_sci_priority.h:17-19;
   src/loader/toy_wire_stream.h:11-18; report row 'W
   START SEG CUT WAIT FULL' = 292 1B0A 4DC9 20B0 5B9.

21. (confirmed, certain) Q2, general: each visit of the auxiliary runs with
interrupts masked on its own 4 KiB stack. Its reading budget is about 1.5 ms
from one of the game's calls and 0.5 ms from an interrupt when no request
waits; the budget is checked between blocks. With the MMU off the stack is
used through the cache; with the MMU on it is used uncached as before.

   src/loader/native_readahead_aux_entry.S:36-75;
   src/loader/toy_wire_ahead.inc:20-21, 553-624, 915-926;
   include/kui/native_readahead_layout.h:40-41.

22. (confirmed, certain) Q2, general for raw Mode 1 images only: the verified
window and its sector checks. The window holds 62 raw sectors. A sector made
only of blocks whose CRC16 passed is accepted when its address follows the
previous one. A byte rebuilt from a block's CRC16 must be confirmed by the
sector's P parity. Everything else, plus one accepted sector in sixteen as a
watch, goes through the sector's EDC. Up to 32 lost bytes of a sector are
filled from its parity. A sector that fails is read again and nothing of it is
served. None of this exists for 2,048-byte (cooked) or compressed images, and
the project's policy forbids accepting a block rebuilt from its CRC16 without
such a second check.

   src/loader/toy_movie_ahead.inc:14-23, 72-132;
   src/loader/toy_wire_ahead.inc:22-48, 339-447;
   include/kui/mode1_erasure.h:8;
   docs/global-launch-policy.md:13-17.

23. (confirmed, certain) Q2, general: the low resident's cursor feed. The
auxiliary hands each 512-byte card block to kui_retail_cursor_feed, which uses
the image map to cut it into sector pieces and calls the cursor's write
routine. The window is a private cursor in raw mode whose write routine stores
into the window. The table of low operations it uses (deliver, hook, unhook,
fetch, poll, take, stop, busy, feed) is the production one.

   src/loader/retail_async.c:908-922;
   include/kui/retail_cursor.h:36-42;
   include/kui/native_readahead.h:60-78;
   src/loader/toy_movie_ahead.inc:158-170;
   src/core/retail_cursor.c:14-15, 65-69.

24. (confirmed, certain) Q2, memory taken from the game: the lease is 0x47000
bytes (284 KiB), the same size as the accepted compact Background reservation.
In the Toy stream layout its first 0x13000 bytes (76 KiB) are the VMU worker's
span and the reader's region. Then the auxiliary takes 0x7000: an image of at
most 0x6000 (24 KiB) and a private stack of 0x1000 (4 KiB). The remaining
0x2d000 bytes (180 KiB) are the arena. The production layout differs only
inside the lease: an auxiliary of 0x4000 (12 KiB image, 4 KiB stack) and an
arena of 0x30000 (192 KiB).

   include/kui/native_readahead_layout.h:32-46, 62-67,
   81-99; Makefile.vmu-native-background-compact:1-13;
   include/kui/vmu_native_background.h:23;
   Makefile.vmu-native-toy-suite:286-287.

25. (confirmed, certain) Q2, how the arena is used: the window takes 62 x
2,352 = 145,824 bytes. Behind it the stream's state, two 512-byte block
buffers and 1,536 bytes of tables end below 150,528, where the kept counters
start. The ring is the last 32,768 bytes. The arena is full: the record notes
the ring would have to give up half of itself for seven more sectors. The
auxiliary's image is full as well, 24,576 of 24,576 bytes, with 28 bytes of
code space left.

   src/loader/toy_wire_ahead.inc:109-148;
   src/loader/toy_wire_stream.h:29, 59-71;
   src/loader/toy_movie_ahead.inc:12-23;
   docs/toy-wire-stream-test-76.md:278-280, 314, 323-350.

26. (confirmed, certain) Q2, fixed addresses: nothing of the lease is at a
fixed address. The auxiliary is linked at 0x8c421000 and relocated to wherever
the stage's lease lies, which must be 32-byte aligned main memory between
0x0c010020 and the top of RAM. Only the low residents sit at fixed addresses,
below the game (0x8c004000 to 0x8c007d00). What a game must provide is a 284
KiB lease from its own allocator that stays untouched. That is the existing
Background admission, not something the stream adds.

   Makefile.vmu-native-readahead:27-28;
   src/loader/vmu_native_readahead_stage.inc:65-70,
   85-104; src/loader/native_readahead_aux.c:15-18,
   107-126; docs/games-launch-map.md:13-15;
   docs/native-read-ahead-compatibility-plan.md:47-57.

27. (confirmed, certain) Q2, cache assumptions: the Test 75 and 76 speed-ups
assume the processor's operand cache is on in its ordinary mode. The ring and
the bytes between blocks are read through the cached address after each line
is dropped, the window's state is reached through the cached address, and the
window copy claims lines with MOVCA.L. The layout assertions about cache
places concern speed only. I did not examine what happens in a game that runs
with the cache off or in another mode.

   src/loader/toy_wire_stream.c:157-166, 721-745,
   958-993;
   src/loader/native_readahead_aux_internal.h:116-156;
   src/loader/toy_movie_ahead.inc:52-70;
   src/loader/toy_wire_ahead.inc:112-132.

28. (confirmed, certain) Q3, production native read-ahead (compact Background
'New', retail-native-dual and its A1 variant), how it is called and triggered.
The low resident's deliver() calls the auxiliary on every delivery, from the
game's disc calls and from the SCI interrupt. The auxiliary serves cache hits
of CRC-checked 512-byte card blocks into the game's cursor; on a miss it calls
the ordinary demand path. After every completed request it re-bases the cache
at the next contiguous card block, bounded by the physical extent in the image
map. There is no file knowledge and no explicit test for sequential reading; a
request elsewhere simply misses and stops the speculation.

   src/loader/retail_async.c:877-907;
   src/loader/native_readahead_aux.c:408-425, 731-772;
   include/kui/native_readahead_cache.h:21-50.

29. (confirmed, likely) Q3, production read-ahead, how it produces. It works
only on the controller heartbeat, never in an interrupt or at a disc call. It
needs the open card stream paused exactly at the next block and DMA channel 1
idle. It reads by programmed transfer, at most two blocks a heartbeat, or one
for the early SDK family. Inference: at one heartbeat a frame that is about 60
KiB a second, more than ten times below what a movie needs (Toy's second movie
averages about 0.8 MB a second of data).

   src/loader/vmu_native_reader_capture.inc:268-280;
   src/loader/native_readahead_aux.c:317-407;
   include/kui/native_readahead.h:53-58, 140-144;
   docs/checkpoint-native-read-ahead-2026-10-25.md:30-38.
   The rate is my arithmetic (2 x 512 bytes x about 60 a second); the
   heartbeat rate itself was not measured.

30. (confirmed, certain) Q3, production read-ahead, what it has and lacks. It
has the same 284 KiB lease with a 192 KiB cache of 384 card blocks, a 12 KiB
image and a 4 KiB stack. Against the Toy reader it lacks continuous DMA
reception, service from the SCI interrupt and a timer, the raw-sector window
with parity repair, and the Toy-suite fields of the low reader the stream's
glue uses. Neither design leaves room for the other in the arena.

   include/kui/native_readahead_layout.h:62-67, 81-84;
   include/kui/native_readahead_cache.h:9-19;
   docs/shared-loader-performance-policy.md:44-50;
   src/loader/sci_stream.h:141-153;
   src/loader/toy_wire_ahead.inc:137-148.

31. (confirmed, certain) Q3, Standard and standalone readers. The default
Standard SCI reader has a 72 KiB VMU service and a paced receive and no
read-ahead lease; the read-ahead refuses to compile without the Background
lease and the SDK's SCI route. Standalone New is a demand-only reader. In its
foreground variant the SCI interrupt route reports not ready, and it holds no
lease. The roadmap lists DOA2, Crazy Taxi and Zombie Revenge as needing memory
proofs before any cache. Both families therefore miss the lease, and
standalone also misses the interrupt.

   docs/shared-loader-performance-policy.md:59-62;
   src/loader/retail_async.c:33-35, 282-285;
   docs/universal-library-coverage.md:22-23;
   docs/universal-loader-roadmap.md:166-177.

32. (confirmed, certain) Q3, CD music and WinCE. Native CD music (Toy raw and
cooked, MDK2) uses separate adapters, and the coverage record says
combinations of virtual VMU, read-ahead and CD music still need coverage.
WinCE has a different ABI and memory ownership, runs with the MMU on, and uses
its kernel's interrupt table. The auxiliary refuses to compile for it, and the
roadmap says CE needs its own scheduling and completion adapter.

   docs/universal-library-coverage.md:24-25;
   src/loader/native_readahead_aux.c:20-22;
   src/loader/retail_async.c:121-129;
   docs/shared-loader-performance-policy.md:64-66;
   docs/universal-loader-roadmap.md:202-207.

33. (confirmed, certain) Q3, remaining routes. Physical disc play reads the
real drive, so there is no card stream to feed. Compressed images (ZSO, LZO)
and cooked 2,048-byte images have no raw sectors on the card, so the window's
repair does not apply. Smash Pack runs with Background and prefetch off and
its accepted bytes are to be preserved. The stream is tied to the SCI port and
DMA channel 1; SCIF and IDE are untouched by it. The build plan lists 13 cold
reader roles.

   docs/universal-library-coverage.md:22-27;
   docs/global-launch-policy.md:56-63;
   src/loader/toy_wire_stream.c:20-41; output of python3
   tools/build_universal_loader.py plan (roles ce, ce_armada, ce_bam4,
   ce_worms, ce_wwp, disc_physical, lzo, native_mdk2, native_toy_cooked,
   native_toy_raw, physical, standalone, zso).

34. (new, likely) Q3, where a stream-fed window would plug in: the auxiliary's
handling of the delivery event, in place of today's block cache. It would sit
behind the same visit gate, in the same lease, with the same table of low
operations. The Toy module is already built that way: it is the compact
Background build with switches added.

   src/loader/native_readahead_aux.c:1558-1576 (dispatch
   of DELIVER), 731-772 (today's cache path), 521-556 (the Toy path at the
   same place); include chain
   Makefile.vmu-native-toy-suite:63 ->
   Makefile.vmu-native-toy-playback:3 -> Makefile.vmu-native-toy-movie:4 ->
   Makefile.vmu-native-background-compact.

35. (confirmed, certain) Q4, rules that bound any plan. AGENTS.md forbids a
title-specific performance opt-in and wants mechanisms applied to every
applicable launch family by default. It wants one behaviour change per source
commit, with its console comparison before the next. It wants the delivered
native candidate left unchanged while its console baseline is pending. It
wants manual-copy ZIPs with an exact changed and retained inventory, and both
GUI aliases if the GUI changes. The compact Background geometry is a release
gate: stage, worker, low engines and auxiliary must carry the same geometry,
and a test rejects larger or mixed ones. The roadmap puts FMV streaming fifth
of six items and asks that request sizes, sequentiality and buffer levels be
measured.

   AGENTS.md sections 'K-UI work and release
   requirements', 'Console measurement checkpoint' (line 69) and 'Required
   delivery path';
   docs/shared-loader-performance-policy.md:9-12, 44-50,
   122-124; docs/universal-loader-roadmap.md:22-29,
   202-234.

36. (new, likely) Q4, step 0, no build: fix the baseline and the scope.
Baseline: the Test 76 module, build 7fe7e83c2082, payload CRC32 8D0D12DA,
report 66-10, installed only at the Card Tools test path. It replaces no
installed reader, which is why this work has not collided with the measurement
checkpoint so far. Behaviour change: none. Rebuilt: nothing. Console test:
none. Outcome: the owner's answers to the decisions listed below.

   <scratch>/pkg76/stage/README.txt:302-306;
   docs/toy-wire-stream-test-76.md:411-419;
   AGENTS.md:69.

37. (new, likely) Q4, step 1, optional, only if the owner wants the hiccup
explained first: a measurement-only build of the Toy module. Baseline: Test
76. One change, no behaviour: a per-piece row of the time in which the
player's next chunk was missing, filled from the hook the game already calls.
Room has to be made first because the image is full. Rebuilt: auxiliary and
worker only; low residents, stage and Card Tools stay byte-identical, as in
every test since 68. Console test: the same Toy run with the same settings.
Accepted when playback matches Test 76 within run-to-run spread and the new
row says whether the chunk was missing in the eighth piece.

   docs/toy-wire-stream-test-76.md:595-602, 311-316,
   343-350; src/loader/native_readahead_aux.c:1131-1162.

38. (new, likely) Q4, step 2: the title-independent trigger, still on Toy and
still in the test slot. Baseline: Test 76, or step 1's build. One change: the
window opens when the reader itself sees consecutive requests on a raw data
track, each beginning where the last ended. It runs to the end of that track's
extent instead of the movie file's end. It closes on a request elsewhere, on
any other disc command, or after a set idle time. The movie ranges and the
playing-movie witness no longer decide anything. The player's update hook
keeps pumping in this step so that only one thing changes, and the hooks stay
for the report. Rebuilt: the auxiliary, and the worker if rows change. Console
test: the same Toy run. Accepted when both movies match Test 76 (first movie
14B frames, second about 1,940, K MB and LB 0, AHEAD STOP 0) and play into the
first level works, because ordinary sequential loads now open windows too.

   What it replaces:
   src/loader/native_readahead_aux.c:445-463, 521-553 and
   src/loader/toy_movie_ahead.inc:134-139, 158-197. An
   extent bound already exists:
   include/kui/native_readahead_cache.h:46-50. Drop
   conditions already there:
   src/loader/toy_movie_ahead.inc:494-510.

39. (new, likely) Q4, step 3: the same reader without the player's hook as a
pump. Baseline: step 2. One change: the update hook no longer visits the
reader and only measures. Pumps then come from SCI interrupts, the stream's
timer and the game's own disc calls. Rebuilt: auxiliary. Console test: the
same Toy run. This step says whether a title-independent reader can keep Test
76's result. If the second movie gets worse, the next single change is another
general foreground source: the extra timer call that Test 69 had and later
tests withdrew, or the controller heartbeat the production read-ahead already
receives.

   src/loader/native_readahead_aux.c:485-501, 1443-1459;
   src/loader/toy_wire_ahead.inc:49-58, 626-654, 667-712;
   src/loader/toy_wire_stream.c:404-413;
   Makefile.vmu-native-toy-suite:117-128;
   src/loader/vmu_native_reader_capture.inc:268-280. Test
   76's report does not separate hook pumps from disc-call pumps (row V GAME
   33E0).

40. (new, possible) Q4, step 4: out of the Toy module. A separately named
reader module for the compact Background family, built without the Toy
executable admission and hooks. It carries general counters instead of the Toy
report, and takes the three SDK table values from the family's admitted
interrupt route instead of constants. It has to be decided here whether it is
built on Test 66's low residents with their Toy parts inert, or on the
production residents extended; the second rebuilds residents and so touches
every game on that reader. Baseline: the installed compact Background New
module (path, build and payload CRC read from the card) and step 3's Toy
result. One change for each game: the stream-fed window in place of today's
two-blocks-a-heartbeat cache. Rebuilt: the new module only, installed modules
byte-identical; a GUI only if the installed one cannot route to the new file.
Console test: Toy first, which must equal step 3, then two or three other raw
GDI games of the owner's choice. For each: the same FMV scene before and after
with VMU and CD music off, first and repeat launch, both launch phases timed,
and a loading-heavy area.

   src/loader/toy_wire_stream.c:48-53; structural route
   values already in the resident:
   src/loader/retail_async.c:286-296 and
   src/core/native_sdk_sci_route.c:94-141;
   src/loader/sci_stream.h:141-153;
   AGENTS.md sections 'Console measurement checkpoint'
   and 'Required delivery path'.

41. (new, possible) Q4, step 5: default for the family. The stream window
becomes the compact Background New reader's behaviour for every admitted raw
SCI GDI title, with no per-title switch. Baseline: step 4's module and
results. One change: the installed module is replaced. Rebuilt:
retail-native-dual and its A1 variant as one cohort, with the release gate
told the new geometry deliberately. Standard, standalone, WinCE, CD music,
Smash and disc files stay as exact prior bytes. Delivery: a manual-copy ZIP
with a changed and retained inventory. Console test: cold ROOT boot, module
path, build and payload CRC confirmed, the same games as step 4 plus Toy, an
FMV, a load and a save on a physical card.

   docs/shared-loader-performance-policy.md:44-50,
   122-124; docs/global-launch-policy.md:33-41;
   AGENTS.md section 'Required delivery path'.

42. (new, possible) Q4, steps 6 to 8, each its own change with its own console
test. Step 6, virtual VMU On: the stream must stop for the worker's use of the
card and start again; a quiesce path exists. Test: an FMV with VMU On, a
game-requested save, power off, reload. Step 7, CD music: one open card stream
cannot serve audio sectors and a movie window at once. First rule: the stream
stays off while CD music plays; then each adapter separately. Step 8, other
families under their own contracts: standalone New (no lease, no interrupt in
its foreground variant), Standard (no lease), cooked and compressed images (no
sector parity), WinCE (own adapter).

   src/loader/native_readahead_aux.c:774-799, 1502-1505;
   src/loader/retail_async.c:1116;
   src/loader/toy_movie_iso_stage.inc:117;
   docs/universal-library-coverage.md:23-25;
   docs/universal-loader-roadmap.md:144-164, 202-234.

43. (new, likely) Q4, the two trigger options and what each needs. A
title-independent trigger needs seven things: a contiguity test on requests in
the auxiliary; an end bound from the image map; a foreground pump that is not
the player's hook; a rule for ordinary loads, which will open windows too; the
timer's SDK table values per family; a decision on requests longer than 60
sectors, which today bypass the window; and general counters in place of the
Toy report. Per-title hooks need, for every title, the exact executable, hook
addresses and context offsets found by analysis, and a stage patch. They give
an exact start and end, a pump of about 2,500 calls a second from the player,
and player measurements. As a delivery mechanism they are a title-specific
opt-in, which AGENTS.md forbids. They fit only as measurement in test modules.

   src/loader/toy_movie_ahead.inc:18-21, 471;
   src/loader/toy_wire_ahead.inc:667-683;
   src/loader/vmu_native_readahead_stage.inc:189-195,
   245-278; AGENTS.md section 'K-UI work and release
   requirements';
   docs/universal-loader-roadmap.md:179-182.

44. (new, certain) Q5, owner's decision, order and scope. FMV streaming is
item 5 of 6 in the roadmap's current sequence, and the measurement checkpoint
says not to stack a performance change on the delivered native candidate while
its console baseline is pending. The owner decides whether FMV work moves
ahead of items 1 to 4, and whether it stays in test modules until that
checkpoint is closed.

   docs/universal-loader-roadmap.md:22-29, 38-42;
   AGENTS.md:69;
   docs/accepted-progress.md:13-27.

45. (new, likely) Q5, owner's decision, which family first and what 'every
game' means. Compact Background New on a raw SCI GDI is the only family that
already has the lease, the interrupt route and the auxiliary. The reader as
built can only serve raw 2,352-byte images on the SCI card reader. Cooked,
compressed, WinCE, standalone and Standard titles would get no stream until
their own steps, and cooked or compressed images possibly never without a
different repair method.

   src/loader/retail_async.c:33-35;
   src/loader/native_readahead_aux.c:20-27, 460-461;
   docs/global-launch-policy.md:13-17;
   docs/universal-library-coverage.md:22-27.

46. (new, possible) Q5, owner's decision, which two or three games to qualify
on. The coverage record names candidates with FMV observations: Evolution 2
(the current Background New timing checkpoint), Phantasy Star Online (poor
intro FMV), Rayman 2 and Record of Lodoss War (earlier smooth FMV), Sonic
Adventure. Spawn, Crazy Taxi 2 and Jet Grind Radio have FMV reports but on the
standalone reader. Which of these the owner has as raw GDI, and whether each
is admitted to Background New today, I did not establish.

   docs/universal-library-coverage.md:46-58, 70;
   docs/accepted-progress.md:13-27.

47. (new, certain) Q5, owner's decision, the Toy module and its hiccup. Either
accept Test 76 as the reference with the hiccup's cause left open, or first
run the measurement-only build (step 1) that would say whether the player ran
out of data or out of processor time. In both cases the module stays a test
module; it is not an installed component.

   docs/toy-wire-stream-test-76.md:567-602.

48. (new, certain) Q5, owner's decision, memory budget. Three choices. (1)
Keep the 284 KiB lease and give the 192 KiB block cache over to window and
ring, which changes ordinary loads for every game on that reader. (2) Enlarge
the lease to keep both, which needs new linker bounds, relocation and stack
checks, and allocator and lifetime proofs per family. (3) Shrink one of the
two. Note that Background with VMU Off still reserves the whole 284 KiB, the
72 KiB worker span included.

   include/kui/native_readahead_layout.h:38-46, 62-67;
   docs/universal-loader-roadmap.md:155-159;
   docs/universal-library-coverage.md:23;
   docs/toy-wire-stream-test-76.md:278-280.

49. (new, certain) Q5, owner's decision, virtual VMU and CD music, both of
which the Toy test requires Off. Until each combination has its own step,
either the stream is simply not used when one of them is On (those launches
behave as today), or the feature is held back until the combinations are
qualified. The project already requires VMU-only, read-ahead-only, combined
and neither to be treated as four separate requirements.

   <scratch>/pkg76/stage/README.txt:146-148;
   src/loader/toy_movie_iso_stage.inc:117;
   docs/universal-loader-roadmap.md:144-164;
   docs/global-launch-policy.md:49-54.

50. (new, likely) Q5, owner's decision, how other games reach a test build.
The Card Tools entry starts the Toy test only, and its source is on another
branch. A second game needs either a new module file that a changed GUI routes
to (then both GUI aliases, with admission run on the packaged bytes), or a ZIP
that replaces the installed Background module for the test. The second touches
the installed reader and so conflicts with the measurement checkpoint unless
the owner asks for it explicitly.

   git branch -a --contains 09c9a4b686d4 (toy-gui-history only);
   AGENTS.md sections 'Console measurement checkpoint'
   and 'Required delivery path';
   docs/shared-loader-performance-policy.md:44-50.

51. (confirmed, certain) Q5, to settle before step 4: which bytes are the
installed baseline. AGENTS.md names f8807cb16561 as the current native test
source and the candidate to keep unchanged. The roadmap and the coverage
record say the delivered candidate is 9e3295bcfaec, which includes f8807cb's
twelve families plus corrections to five virtual-VMU readers, compact
Background among them. The actual module path, build and payload CRC on the
owner's card must be identified before any comparison against it.

   AGENTS.md:34, 69;
   docs/universal-loader-roadmap.md:3-9;
   docs/universal-library-coverage.md:22-23.

## Recommendations

- Tell the owner plainly that 'package for every game' today can only mean one
family first: compact Background New on raw SCI GDI images with virtual VMU
and CD music off. Only that family already has the lease, the interrupt route
and the auxiliary the reader needs.
- Make the next build the title-independent trigger on the Toy module itself
(plan step 2). Keep the hooks for measurement only and compare against Test 76
under the same conditions. Then remove the player's update hook as a pump in a
separate build (step 3). That build answers the one open technical question
that decides everything after it.
- Before any second game, decide the low-resident question: Test 66's
residents with their Toy parts inert, or production residents extended. Also
decide how a second game reaches a test module. Both decide what is rebuilt
and what the console test must cover.
- Keep per-title hooks out of anything installed. They fit AGENTS.md only as
measurement inside test modules.
- Before comparing against 'the installed reader', read the actual Background
module's path, build and payload CRC from the owner's card and reconcile
AGENTS.md (f8807cb16561) with the roadmap (9e3295bcfaec).
- Plan for room in the auxiliary before adding anything. The image is full,
and the Toy measurements are the natural thing to drop once a general build no
longer needs them. A general build also needs its own small set of counters,
because the Toy report depends on the player hooks.
- Ask the owner to accept or defer the hiccup explicitly. If it is to be
explained, run the measurement-only step first; otherwise record it as
accepted with its cause open.
- Carry the roadmap's own request into step 4: record request sizes,
contiguity and gaps for the candidate games. Requests over 60 sectors bypass
the window today, and other games' players are unknown.

## Not verified

- No build, make target or test was run. All findings come from reading code
and documents and from decoding the four console reports with a small script.
- The cause of the hiccup. The report measures where it is and that the card's
data was on time there, not why the player dropped frames.
- That the low resident forwards the SCI interrupt to the auxiliary while the
stream holds the DMA state. This rests on the comment at
src/loader/toy_wire_ahead.inc:475-477; I did not trace it
in the resident's interrupt code.
- How the 284 KiB lease is obtained from each game's allocator and which games
are admitted to compact Background New today. I did not read the stage's
allocator admission or the GUI's admission manifests.
- Whether the installed GUI would admit a rebuilt or renamed Background
module, that is, whether its admission pins exact bytes for that module.
- The Card Tools test entry's code. It is on branch toy-gui-history (commit
09c9a4b); I saw only which files that commit touched.
- Where the rule 'tracks begin at their file's first byte' is enforced.
- The internals of several sources I did not read: toy_resume_verify.inc,
toy_wire_copy.inc, toy_wire_msf.h, mode1_erasure.c, native_sdk_sci_family.inc,
most of sci_stream.c and retail_async.c, and the standalone and WinCE readers.
The statements about those families come from the documents and from
compile-time guards.
- Anything about other games' movie players: their request sizes, how often
they make disc calls while a movie plays, their data rates, and whether TMU0,
a spare TMU channel, DMA channel 1 and the operand cache are in the state the
reader needs.
- The heartbeat rate behind the production read-ahead's producer. The figure
of about 60 KiB a second is my arithmetic from two blocks a heartbeat at an
assumed one heartbeat a frame.
- Whether Test 66's low residents behave like the production residents for
ordinary loads on games other than Toy Commander.
- Which software is actually on the owner's card (9e3295bcfaec or f8807cb16561
modules), and whether the second, ALTERNATE module of the Test 76 package was
ever run. The record says it was not.
- The exact size of the stream's state structure. I checked the arena layout
against the source's static assertions, not against a compiled size.
