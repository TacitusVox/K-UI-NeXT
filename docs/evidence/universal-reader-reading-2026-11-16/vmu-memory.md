# Virtual VMU and the stream reader: memory

An independent reader's answer as returned, unedited but for line wrapping and
paths (repository paths relative to its root, scratch copies as <scratch>). It
was asked one part of the question the owner put on 16 November 2026 after
Test 77's console result; the brief is quoted in
docs/fmv-stream-reader-plan.md. The sceptical check of these claims that was
to follow was cut off by the session's usage limit; what the author checked
himself is said in the plan.

## Summary

Yes at the level of the lease, no at the level of today's Test 77 worker. The
lease is 0x47000 (290,816) bytes that the cold stage takes from the game's own
allocator as the last block of the game's heap, at no fixed address; it is the
same size with VMU On, with VMU Off and in Test 77. The virtual VMU lives
wholly in its first 0x12000 bytes, the worker: 49,920 bytes of code and
constants, 20,352 bytes of state and a 3,072-byte stack, with the card's
contents on the SD card and not in RAM. The stream reader lives wholly from
0x13000 up (24,576 bytes of auxiliary image, a 4,096-byte stack, a
184,320-byte arena for window and ring), and the 12,288 bytes more that its
code needed came out of the installed 196,608-byte block cache, not out of the
worker. So the two do not want the same bytes, but nothing is left over: the
installed recipe's worker with the VMU has 384 bytes free, Test 77's auxiliary
32, and the arena is used to the byte. Test 77's own worker made room for its
report and screen pages by compiling the VMU code out (KUI_TOY_SUITE_MODE=66
makes reader_only() constant true); with the VMU code put back in a scratch
copy it is 75,488 bytes against the 70,656 below its stack, 4,832 short. What
gives way is that test-only report and terminal code, 5,216 bytes over the
installed worker; the window and the ring cannot help the worker without
moving every offset above 0x12000, and the arena has no spare page (the window
sits inside the 64-sector staging area and the ring can only halve). VMU Off
releases none of this in the installed build, and even Test 77's worker still
links 13,804 bytes of VMU state. Any change of these sizes touches constants
shared by stage, worker, both low engines and auxiliary, the linker scripts,
the geometry oracle and release checks (0x12000 / 0x47000 / 0x30000 / 0xc00,
auxiliary span 0x4000) and the launcher's VMU capability record. Measured on
the installed recipe built from the sources at HEAD (9c9867c, two
record-keeping commits after the 7456726 the task names; code unchanged since
ded3083), because the delivered candidate's commit is not in this shallow
clone.

## Facts

1. (read in code) Who allocates the lease and where it lies: the cold stage,
right after the game's own heap initializer has returned, calls the game's own
allocator once for 0x47000 (290,816) bytes. The block is the tail of the
game's heap: payload = heap begin + heap bytes - 0x47000, with the allocator's
32-byte header just below it. There is no fixed address; it must be 32-byte
aligned in main RAM with physical address from 0x0c010020 to
0x0d000000-0x47000. A title that links no SDK allocator gets a reviewed fixed
arena that is the whole lease (House of the Dead 2: 0x8cdb9000).

   Evidence: src/loader/vmu_native_heap_probe_stage.c:538-585 (allocate at
   579-580; lease size at 561-563; arena case 519-535);
   src/core/vmu_native_heap.c:611-625 (payload at 621) and 567-569;
   include/kui/vmu_native_background.h:87-93; docs/native-zr-family.md:309-323

2. (read in code) (a) Installed build, VMU On with read-ahead. 0x00000-0x12000
worker span (73,728 bytes; its last 3,072 are the worker's own stack, from
0x11400). 0x12000 launch map, 1,020 bytes. 0x12400-0x12b60 the low reader's
vector/receive/state region, 0x760 = 1,888 bytes. 0x13000-0x16000 auxiliary
image, limit 0x3000 = 12,288. 0x16000-0x17000 auxiliary stack, 4,096.
0x17000-0x47000 block cache, 0x30000 = 196,608 bytes = 384 card blocks of 512.

   Evidence: include/kui/vmu_native_background.h:21-25,43-44,46-48;
   include/kui/native_readahead_layout.h:62-67,81-84;
   src/loader/vmu_native_performance.ld:8-15,46-55;
   include/kui/native_readahead_cache.h:9-19;
   tools/native_runtime_layout.py:67-73

3. (read in code) (b) Installed build with VMU Off (reader-only): the same
module file, the same 0x47000 lease and the same division as (a). The launcher
writes mode 7 into the module; the stage turns it into the worker's
READER_ONLY setting and only leaves out the card hand-over; reader_only() is a
run-time test in this recipe, so the whole VMU code and its state are still
copied into the lease. The stage's own comment: the selector 'never changes
reservation'.

   Evidence: src/apps/games_vmu_card.inc:154;
   src/loader/vmu_native_passive_stage.inc:306-312,395-400,666-671;
   src/loader/vmu_native_passive.c:322-323;
   docs/universal-loader-roadmap.md:177-180

4. (read in code) (c) Test 77 module: the same 0x47000; worker span, map and
region exactly where (a) has them. From 0x13000: auxiliary image limit 0x6000
= 24,576; auxiliary stack 0x19000-0x1a000 = 4,096; arena 0x1a000-0x47000 =
0x2d000 = 184,320. Inside the arena: 62 window slots of 2,352 = 145,824 bytes
from offset 0; the stream's state (1,292 bytes) at 145,824; two 512-byte block
buffers and 1,536 bytes of tables at 147,456-150,016; struct toy_wire_keep
(928 bytes) at 150,528; the 32,768-byte ring at 151,552, ending at the lease's
end.

   Evidence: include/kui/native_readahead_layout.h:32-46;
   Makefile.vmu-native-toy-suite:311-312;
   src/loader/toy_wire_ahead.inc:109-111,123-125,141-148;
   src/loader/toy_movie_ahead.inc:12-14;
   src/loader/toy_wire_stream.h:29,59,68; struct sizes from the debug
   information of <scratch>/clean77/build/toy-suite-77/native-ra-aux.elf

5. (read in code) The stream took its room from the block cache, not from the
worker. Against (a), Test 77 gives 0x3000 = 12,288 bytes more to auxiliary
code (span 0x4000 to 0x7000) and the arena shrinks by the same (0x30000 to
0x2d000). KUI_VMU_NATIVE_BACKGROUND_WORKER_BYTES 0x12000, the map at 0x12000,
the region at 0x12400 and KUI_NATIVE_RA_AUX_OFFSET 0x13000 are the same in
both layouts.

   Evidence: include/kui/native_readahead_layout.h:38-46 against 62-67,81-84;
   include/kui/vmu_native_background.h:21-25

6. (read in code) The installed recipe is
Makefile.vmu-native-background-compact (target all; installed as
apps/games/retail-native-dual.kui) and its -a1 variant (adds
-DKUI_VMU_VIRTUAL_A1=1; retail-native-dual-a1.kui). Both recipes share the
worker's layout flags: KUI_NATIVE_BACKGROUND_COMPACT=1,
KUI_NATIVE_GENERAL_LEAN_LAYOUT=1, KUI_NATIVE_SPLIT_CONTROLLER_GENERAL=1,
KUI_NATIVE_READAHEAD=1, KUI_VMU_NATIVE_PERSISTENT_PROBE=1, the script
src/loader/vmu_native_background_compact.ld and NATIVE_SPAN 0x12000. Test 77
adds to them -DKUI_NATIVE_TOY_MOVIE_CAPTURE=1
-DKUI_NATIVE_TOY_PLAYBACK_CAPTURE=1 -DKUI_TOY_SUITE_MODE=66
-DKUI_TOY_WIRE_STREAM=1 and the TOY_WIRE switches (CACHED WORDS PACE IDLE LEAN
STEADY FLUID SEQ =1, LATER=0, GAP=0), plus native_return_hardware.c and
toy_return_hardware.o.

   Evidence: tools/update_build_modules.py:32-43;
   Makefile.vmu-native-background-compact:7-23;
   Makefile.vmu-native-background-compact-a1:7-8;
   Makefile.vmu-native-toy-suite:70-71,120-121,331-332;
   docs/shared-loader-performance-policy.md:43-49; compile lines in
   <scratch>/vmu/memory/toy-e1-build.log and
   <scratch>/vmu/memory/prod-build.log

7. (read in code) The flag that removes the VMU code is KUI_TOY_SUITE_MODE: 66
in the Test 77 recipe, undefined in the installed recipe. With it
reader_only() returns constant true ('Keep the terminal services in this
worker by removing unreachable VMU code'), and the worker refuses to arm in
any launch mode but reader-only, so a Test 77 module cannot run with VMU On.
What the linker then drops: vmu_durable.o 3,724, vmu_emulation.o 1,748,
vmu_store.o 1,918, vmu_volatile_chain.o 2,794, 2,950 of
vmu_readonly_overlay.o's 3,084 and a 96-byte libgcc copy routine, 13,230
bytes, plus the VMU paths inside vmu_native_passive.o.

   Evidence: src/loader/vmu_native_passive.c:317-327,1656-1659; per-object
   comparison of
   <scratch>/vmu/memory/prod/build/native-background-compact/native-worker.map
   with <scratch>/clean77/build/toy-suite-77/native-worker.map (script
   <scratch>/vmu/memory/tools/mapobj.py)

8. (measured here) Test 77 worker as built: native-worker.bin 39,392 bytes
(text 36,188, constants 3,060), state (.bss) 18,784, together 58,176 of the
70,656 bytes below its stack; 12,480 free; stack 3,072 at span offset 0x11400.
A rebuild in a scratch copy gave a byte-identical native-worker.bin.

   Evidence: sh-elf-size -A and sh-elf-nm on
   <scratch>/clean77/build/toy-suite-77/native-worker.elf: bss
   0xac1099e0-0xac10e340, stack bottom 0xac111400; cmp with
   <scratch>/vmu/memory/e1-worker.bin

9. (measured here) Installed recipe's worker WITH the virtual VMU, built here
from HEAD sources (A2 and A1 the same sizes): native-worker.bin 49,920 bytes
(text 48,320, constants 1,456), state 20,352, together 70,272 of 70,656; 384
bytes free below the 3,072-byte stack. No built production worker existed
under build or the scratchpad before this. The delivered candidate itself
(source f8807cb16561) could not be rebuilt: that commit is not in this shallow
clone; its evidence records worker 73,728, worker stack 3,072, lease 290,816,
cache 196,608 and no fill figure.

   Evidence: sh-elf-size -A and sh-elf-nm on
   <scratch>/vmu/memory/prod/build/native-background-compact/native-worker.elf
   and
   <scratch>/vmu/memory/prod/build/native-background-compact-a1/native-worker.elf:
   bss 0xac10c300-0xac111280, stack bottom 0xac111400;
   docs/evidence/native-shared-f8807cb/five-vmu-layout.json (profiles
   background_a2 and background_a1, source_binding)

10. (read in a document) This worker has been full before: 'with the exit pair
the compact worker filled its 72 KiB exactly, so its owned stack went from 4
KiB to 3 KiB'; the audit then put the deepest call path at 1,912 bytes, 1,160
spare, and '1 KiB is now free for code and state'. Of that 1,024 bytes, 384
are left at HEAD (the fact above).

   Evidence: docs/native-zr-family.md:301-307;
   include/kui/vmu_native_passive.h:49-56

11. (read in code) Where the virtual VMU keeps its state: all of it is static
data of the worker, inside the worker span at uncached addresses. durable_card
4,160 bytes (the card hand-over record of 1,272 with 256 block checksums; the
store's state of 1,472 with two 512-byte sector buffers; the protocol's state
of 540 with one staged 512-byte block; a 512-byte bounce buffer). captured
4,096 (the game's Maple request list). issued_chain 4,096 (the list sent to
the hardware). overlay 1,452 with one staged 524-byte reply, and
overlay_frames 896. chain_frames 640. config 2,304, which holds a second copy
of the 1,272-byte hand-over. All state together 20,352 bytes.

   Evidence: src/loader/vmu_native_passive.c:230-281,353-354;
   include/kui/vmu_durable.h:45-66; include/kui/vmu_store.h:53-64;
   include/kui/vmu_emulation.h:33-49;
   include/kui/vmu_readonly_overlay.h:79-93;
   src/loader/vmu_native_performance.ld:2-4; sizes from
   <scratch>/vmu/memory/prod/build/native-background-compact/native-worker.map

12. (read in code) What is not in RAM: the card's 128 KiB of contents. They
are the base image and a journal on the SD card, read and committed one
512-byte block per storage transaction ('No entire card ... is emulated'). A
reply is staged in the worker and then written into the game's own Maple
receive buffer. Nothing of the VMU lies in the game's heap outside the lease,
in the auxiliary or in the arena (the last part by absence of any reference in
the VMU sources).

   Evidence: include/kui/vmu_emulation.h:21-35;
   include/kui/vmu_durable.h:19-30,84-91; include/kui/vmu_store.h:65-70;
   include/kui/vmu_readonly_overlay.h:23-34,79-82

13. (measured here) 'Disabling a service does not prove its RAM reservation
was released', seen in Test 77's own map: with the VMU code compiled out the
worker still links durable_card 4,160, captured 4,096, the 4,096-byte
issued-chain buffer (its second half reused for the 1,056-byte controller
workspace of the terminal report) and overlay 1,452, 13,804 bytes. Only
overlay_frames 896, chain_frames 640, issued_plan 44 and reader_stream 44
left. The 12,480 free bytes stay inside the fixed 0x12000 span, which the
stage zeroes whole.

   Evidence: section comparison of
   <scratch>/clean77/build/toy-suite-77/native-worker.map against the
   installed-recipe map (script <scratch>/vmu/memory/tools/mapsec.py);
   src/loader/vmu_native_passive.c:247-257,1358;
   src/loader/vmu_native_passive_stage.inc:481

14. (measured here) Experiment: the Test 77 worker with the VMU code reachable
again (in a scratch copy, reader_only() given the installed recipe's run-time
test; linked with a span enlarged only so that the linker would report a
size). Result: image 55,136 + state 20,352 = 75,488 bytes against 70,656:
4,832 short, and 1,760 more than the whole 73,728-byte span even with no
stack. What Test 77 adds to the installed worker is 5,216 bytes: the report
and its rows (+2,488 code net in vmu_native_passive.o, +1,734 of row labels
and an empty-report constant), native_return_hardware.o 704,
toy_return_hardware.o 228, 56 in the display code. Without them the worker is
the installed one, 384 bytes free. This is a size measurement, not a design.

   Evidence: <scratch>/vmu/memory/toy2/build/toy-suite-77/native-worker.elf:
   bss 0xac10d760-0xac1126e0; the two edits are in
   <scratch>/vmu/memory/toy2/src/loader/vmu_native_passive.c line 318 and
   <scratch>/vmu/memory/toy2/src/loader/vmu_native_performance.ld line 9

15. (measured here) A second size for comparison: the existing recipe
Makefile.vmu-native-toy-cache, which is the installed worker with the VMU plus
one small read-ahead capture page (KUI_NATIVE_TOY_CACHE_CAPTURE=1), links at
70,592 bytes: 64 free. So a worker that keeps the VMU has room for roughly 384
bytes of anything new.

   Evidence: Makefile.vmu-native-toy-cache:1-5; sh-elf-nm on
   <scratch>/vmu/memory/prod/build/native-toy-cache/native-worker.elf: bss end
   0xac1113c0

16. (measured here) The auxiliary is as full as the worker. Test 77: image
24,544 of 24,576 bytes (32 free; code 20,424); its context page holds 3,968 of
4,096, a 2,049-byte checksum table among them. The installed read-ahead
auxiliary: image 10,400 of 12,288 (code 6,296, context 904).

   Evidence: sh-elf-size -A and sh-elf-nm on
   <scratch>/clean77/build/toy-suite-77/native-ra-aux.elf (image end
   0x8c426fe0) and
   <scratch>/vmu/memory/prod/build/native-background-compact/native-ra-aux.elf
   (image end 0x8c4238a0); src/loader/native_readahead_aux.ld:26,32;
   docs/toy-wire-stream-test-77.md:129

17. (inferred) The arena has no spare page, and the window is not what pins
it. 150,528 bytes (staging for a request of 64 raw sectors, the low reader's
largest; the 62-slot window lies inside it) + 1,024 (slot of the kept
counters, which the worker reads at offset 150,528) + 32,768 (ring) = 184,320
exactly. The ring must be a power of two, so it can only halve to 16,384.
Shrinking the window alone frees no lease bytes. Two small reserves elsewhere:
1,184 bytes between the region's end (0x12b60) and the auxiliary (0x13000)
that no constant assigns, and the worker stack's 1,160 audited spare bytes.

   Evidence: src/loader/toy_wire_ahead.inc:109-111,137-148;
   include/kui/toy_wire_report.h:11-13; include/kui/retail_image.h:53;
   src/loader/toy_wire_stream.c:55,84-85;
   include/kui/vmu_native_background.h:55-62; docs/native-zr-family.md:303-305

18. (read in code) The VMU's use of the card costs the worker no new memory
for a stream: it already goes through the low reader, whose loan sends the
auxiliary a QUIESCE before it lends the card to the worker. Code to stop and
restart a card stream for a save would therefore belong to the auxiliary and
the low resident, where Test 77 has 32 bytes free.

   Evidence: src/loader/retail_async.c:1101-1117;
   src/loader/native_readahead_aux.c:1633-1640;
   src/loader/vmu_native_reader_capture.inc:268-279

19. (read in code) What a change of these sizes touches. Constants that stage,
worker, both low engines and auxiliary must share:
include/kui/vmu_native_background.h:21-25,
include/kui/vmu_native_passive.h:38-56, include/kui/native_readahead_layout.h
(its stream branch is an #error outside KUI_TOY_SUITE_MODE 66, lines 35-37),
the linker script src/loader/vmu_native_performance.ld:8-15 and NATIVE_SPAN /
RA_AUX_SPAN in the Makefiles. The independent checks, which pin worker
0x12000, lease 0x47000, cache 0x30000, worker stack 0xc00 and auxiliary span
0x4000. The launcher's VMU On/Off capability record, whose cache size is a
literal 0x30000 in the stage and is required by the launcher's finder. The
release gate's wording '72 KiB worker / 284 KiB reservation / 192 KiB cache'.
A larger lease also needs allocator and lifetime proofs, and two titles
already do not fit 284 KiB.

   Evidence: tools/native_runtime_layout.py:67-73;
   tools/check_shared_loader_performance.py:104;
   tools/check_native_compact_delivery.py:229;
   tests/test_native_background_compact_config.py:41-43,56-57;
   tools/update_build_modules.py:169-171;
   src/loader/vmu_native_heap_probe_stage.c:96-99;
   include/kui/vmu_native_toggle.h:28-30;
   docs/shared-loader-vmu-regressions.md:84-86;
   docs/universal-loader-roadmap.md:177-180; docs/library-compat-table.md:110

## Open questions

- How full the worker is in the module actually on the owner's card: not
established. The candidate's commit (f8807cb16561, or 9e3295bcfaec as the
roadmap names it) is not in this shallow clone, and its evidence file gives
the span and stack but no fill. The 384 bytes are the installed recipe built
from HEAD sources.
- Whether a general stream module needs anything new in the worker at all (a
counters page in place of the Toy report, a handshake for the VMU's card
loan). None is built. The room is 384 bytes in the worker and 32 in Test 77's
auxiliary; how much of that auxiliary is Toy-only hooks and traces that a
general module would shed was not measured here.
- Whether the auxiliary's QUIESCE, which the low reader already sends before
the VMU uses the card, ends and restarts the card stream correctly in the Test
77 code. That is behaviour, not memory, and was not examined in this part.
- Stack depth with VMU On beside the stream: the 1,912-byte figure is the
document's audit of the installed worker's 3,072-byte stack; no call-graph
audit was run on a combined build, nor on the auxiliary's 4,096-byte stack.
- For a VMU-on test inside the Toy test module: the terminal report's
controller workspace (1,056 bytes) overlays the second half of the 4,096-byte
issued Maple chain buffer, which the VMU uses while the game runs. Whether the
two can share it at report time is a design question for that test build and
was not settled here.
- Toy Commander's actual lease address is not in the sources; it follows from
that game's heap at run time.
