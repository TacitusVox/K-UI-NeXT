# Test 76: independent review of the changes

Five reviewers, one for each change (GAP and APART together), read the
source at commit `6a39343`, before the last code commit, under Test 76's
switches. Each was asked for concrete failure cases only: a sequence of
states and events and the wrong outcome it leads to. They read; they did
not build or run anything but read-only scripts, and none of this is a
console result. What follows is their answers as returned, unedited but
for line wrapping. The author's dispositions are in
`docs/toy-wire-stream-test-76.md`, Review; what they led to is commit
`7fe7e83`.

Before any of this was built, three readers checked the plan: the
analysis of Test 75's two console reports, a hand-written copy for both
copies, and the window's state in cached memory. Their answers changed
the plan (the wait at every stream's end was theirs to find; the copy out
to the game was dropped; the window's state stayed in the context) and
are summarised in the same section.

## The base: the switch, LATER off, the report (commit 1861085)

The base commit 1861085 is sound as written: I found no crash, wrong-data or
stall case on the Test 76 settings, and one report-reading hazard. This is
from reading the code and git history only; nothing was built or run. The
decisive evidence: (1) FLUID flags are appended to FLAGS, NATIVE_FLAGS,
TOY_MOVIE_TEST_FLAGS and TOY_WIRE_TICK_FLAG exactly as the STEADY flags are,
after the immediate 'private FLAGS := $(FLAGS)' freeze for the residents. The
worker compiles with NATIVE_FLAGS and the auxiliary with RA_FLAGS =
$(NATIVE_FLAGS), so writer and reader share one layout. struct toy_wire_keep
comes to 924 bytes on the target by my count (report 724) against 1024, and
the _Static_assert at src/loader/toy_wire_ahead.inc:121 enforces it. (2) Every
new game-memory read sits behind toy_chunk_ram, which forces word alignment
and 0x8c/0xac main RAM with the whole object below 0x8d000000. The function
writes only fields no visit writes. (3) seen_pause is written only inside
visits and zeroed together with n.wait_ticks. (4) Nothing can return between
the two clock reads around kui_wire_stop. (6) With LATER=0 the pump takes the
same '#elif KUI_TOY_WIRE_MEASURE' line as Test 74's delivered source
(e82b316), and STAY has no dependence on LATER.

### Finding 1: TB cannot be scaled or validated from the Test 76 report: capacity is stored but not printed, and the code comment's scale is wrong for the real player

Certainty: likely. Kind: report only. Where: src/loader/toy_suite_report_save.inc:116-119 (value written at src/loader/toy_wire_ahead.inc:756)

toy_wire_update_seen stores w->buffer_capacity, but no Test 76 row prints it
(Test 73 printed it as CAP in the IB row). The comment at the TB row says 'its
megabyte is 100', and the host fixture uses 0x100000. The only console
measurement of this buffer is report 66-7 (Test 73): CAP C9120 = 823,584
bytes, also recorded in docs/toy-wire-stream-test-73.md:218. In TB's 4 KiB
units that is 0xC9, not 0x100. Step by step: the player's buffer runs dry in a
piece; TB prints about C8 for it. A reader using the stated megabyte scale
takes that as 78% free, about 220 KiB still held, and concludes the player was
not short of data, when the buffer was in fact empty. Second consequence: if
the three-pointer chain or the sanity check ever fails on this build, TB is
all zeros, which reads as 'buffer always full'. CAP was the witness that
sampling worked, and it is no longer in the text. No data or playback effect;
the capacity must be taken from Test 73's report or printed.

### Examined and found to hold

- Makefile flag reach: TOY_WIRE_FLUID_FLAGS go to FLAGS, NATIVE_FLAGS,
TOY_MOVIE_TEST_FLAGS and TOY_WIRE_TICK_FLAG; NATIVE_FLAGS and RA_FLAGS are
recursive, so worker (vmu_native_passive.c with toy_suite_report_save.inc) and
auxiliary (native_readahead_aux.c, toy_wire_stream.c) both see FLUID. No
problem.
- Low residents: '$(RESIDENT_OBJ) $(BUILD)/resident-sci.elf: private FLAGS :=
$(FLAGS)' is expanded immediately, before -DKUI_TOY_WIRE_STREAM and everything
after it, so no FLUID flag reaches them. No problem.
- Stale objects across settings: toy-suite-config records TOY_WIRE_FLUID=1 and
LATER=, and every object depends on it; BUILD defaults to build/toy-suite-76.
No problem.
- LATER default: Makefile gives LATER=0 under FLUID and the STEADY block then
passes -DKUI_TOY_WIRE_LATER=0; an explicit LATER=1 passes =1 from the FLUID
block only; the header default (STEADY && !FLUID) agrees. No problem.
- sizeof(struct toy_wire_keep) on the target: 924 bytes by hand count (report
181 words = 724, lost[34] = 68, rest 132) against 1024 before the ring, and
the _Static_assert guards it. No problem.
- Fields removed from the report struct under FLUID (visit_max, restart_*,
wait_*, ccr/mmucr, course_stops, gd_*, update_ticks, course_lag): every writer
is already under !LEAN, !IDLE or !FLUID. No problem.
- GD_ENTER/GD_LEAVE visits under FLUID: the unchanged resident still sends
them; the auxiliary only skips toy_wire_gd_time, which wrote nothing but its
own counters. No behaviour change.
- toy_wire_update_seen faults: ctx+8 lies in the object whose +0x90, +0xac and
+0x1c0 were already read; stream, channel and avi are each checked for 20, 72
and 108 bytes, and the reads end at +8, +72 and +108. No address error or
out-of-RAM read.
- Interrupt visit in the middle of toy_wire_update_seen: it writes player[],
skip_first, seen_*, buffer_capacity and course_free; visits write none of
these (they write visit_*, course_pause, seen_pause, halt_ticks). out and skip
are locals by the time 'frame' is computed. No read-modify-write race added.
- CAPTURE_START reset against update_seen: the reset is a gated visit; it
zeroes the report and seen_pause together and leaves seen_* alone. No problem.
- Negative piece: 'if(piece<0 || !frame)return' precedes every index;
toy_wire_course clamps to 11. No problem.
- Unsigned wrap in 'free': used = filled - released wraps harmlessly and used
> capacity is rejected before capacity - used; capacity is bounded to
0x400000. No wrap reaches the report.
- Frame-only sampling bias in TB: a starved player takes no frame, so the
emptiest moment is not sampled; the next frame call samples after one refill.
Requests average about 11 sectors (22 KiB, about 6 of 201 units) in report
66-9, so the understatement is small. Not a failure.
- course_pause going backwards: n.wait_ticks is zeroed only by toy_wire_reset,
which zeroes seen_pause in the same gated visit; a wait in progress across the
reset is added whole afterwards, never subtracted. No problem.
- course_pause double or wrong-movie counting: every toy_wire_visit_time takes
the delta once and then sets seen_pause, including when piece < 0 (delta
discarded, first movie). The early return on an invalid clock leaves
seen_pause alone, so the delta carries over. Only waits ended between the last
visit before the second movie and its first visit land in piece 0. No problem.
- Timer visits: toy_wire_timer_visit returns before anything when k->on is 0,
otherwise it resumes the wait and calls toy_wire_visit_time, so the delta is
placed in that visit. No problem.
- Waits cut off by a halt: kui_wire_stop from WAIT never adds the open wait to
n.wait_ticks, so TP and WTICK miss it. This predates the commit and the halt
normally follows the wait's start within the same visit. Negligible.
- Overflow of course_pause and of TR's sum: a piece is 2^22 ticks; only the
clamped last piece can collect more, and 2^32 ticks is 91 minutes. No
overflow.
- Halt timer: no return between the two toy_wire_phase() reads; the k->on == 0
return comes before both; halt is never inside the parse, feed, service or
seek brackets while k->on is set, so P's parts stay disjoint. No problem.
- Report length: with the values of console reports 66-9 and 66-9 alternate
mapped to Test 76's rows, and TB, TP, TR at full 8-digit words and a 6-digit
HALT, the text is 987 and 990 bytes of 1012, a margin of 25 and 22. TR, the
last row, is the first to drop; a first error with detail adds about 7.
Nothing dropped at realistic values.
- Row buffer: the longest label is 32 characters and six 8-digit values make
95 bytes of row[104]. No overrun.
- TB and TP clamps: the real capacity 0xC9120 gives at most 0xC9 at shift 12,
and TP at shift 14 reaches 255 only for a whole piece of standstill. No clamp
at real values except the last piece.
- LATER=0 pump: the taken line is identical to Test 74's (e82b316
toy_wire_ahead.inc:747), INTERRUPT ticks for an interrupt visit without a
waiting request, FOREGROUND otherwise. toy_wire_later and its enum are
compiled out. Confirmed.
- STAY without LATER: toy_wire_stay uses only the clock, the token and
A.verified; the 'selected' term and the waiting pump (demand, FOREGROUND
ticks) are the same with either setting. No dependence.
- RING with interrupt visits reading again: every visit is atomic under the
gate whoever calls it, and nothing outside the gate touches the ring's cache
lines. No new exposure.

## QUIT (commit 697453d)

QUIT holds as written; I found no failure for a healthy stream. The decisive
evidence is that the state it creates (channel off, RDRF and ORER both set,
DAR1/TCR1 at an interior position) is the same state every cut with a held
byte already hands to wire_capture() and wire_handoff(), and that path has run
on the console. The one new register write, wr32(CHCR1, rd32(CHCR1) & ~1) at
src/loader/toy_wire_stream.c:594, only removes the possibility of a late DMAC
take. wire_capture() still reads DAR1/TCR1 behind a fence, and the stop path
uses the count only for the report's byte total. The single behavioural
regression is narrow: the write happens before the "ours" check, so a channel
1 that someone else reprogrammed is no longer left alone. Nothing in this game
is known to do that. The other two entries are test gaps, not defects. All of
this is from reading the code and models; I ran nothing, and the DMAC/SCI
points rest on the SH7750 manual's behaviour as I recall it, not on a console
run.

### Finding 1: QUIT clears DE on channel 1 before checking the channel is still the stream's

Certainty: possible. Kind: stall or slowdown. Where: src/loader/toy_wire_stream.c:594

1. A run is armed, w->state == KUI_WIRE_RUN. 2. Another owner reprograms DMAC
channel 1 (SAR1 != RDR, or DAR1 outside the run) and enables it; the receiver
overruns within two bytes, but the ERI is still held off (interrupts masked,
or a foreground visit is already under way). 3. That visit calls
toy_wire_halt() (for example toy_ahead_clear, or the CLOSE/JUMP halts), which
calls kui_wire_stop() with state RUN. 4. The QUIT line writes rd32(CHCR1) & ~1
unconditionally, clearing the other owner's enable bit. 5. wire_capture() then
finds !ours and returns E_FOREIGN, and kui_wire_stop() skips the saved_dma
restore, so nothing re-enables the channel. Before QUIT, CHCR1 was written
only inside the ours branch (toy_wire_stream.c:312-319), matching the rule
stated in sci_stream.c:775 that a reprogrammed channel is left alone.
Consequence: the foreign transfer is suspended for good, on top of the
E_FOREIGN port fault that happened before too. The precondition is not known
to occur in this game: kui_wire_start() requires channel 1 idle, and no
console report has shown first_error 16. wire_arm() already rewrites the
channel unchecked after a WAIT or FULL, so this widens an existing exposure
and does not open the first one.

### Finding 2: The integration model runs each reception to its own overrun on the CHCR read, so QUIT is inert there

Certainty: certain. Kind: test gap. Where: tests/sci_stream_model.h:266

kui_sci_stream_test_read(CHCR) calls advance(), and receive() loops until ORER
in that one call. In tests/test_toy_wire_integration.c the read half of QUIT's
read-modify-write therefore clocks the run to its natural cut or end with DE
still 1; the write of DE=0 lands on a receiver that has already stopped. With
m.delay=100000 the receiver has not started, so DE=0 precedes every byte and
count == seg_len. No integration case reaches the state QUIT creates on the
console (bytes stored, DE cleared mid-run, forced overrun, interior
DAR1/TCR1), and removing the QUIT line changes nothing the integration test
can observe. Only tests/test_toy_wire_stream.c exercises that state. That
model in turn never reaches a stop in RUN with ORER already pending: line 248
always services a pending overrun before the reader loop that stops.

### Finding 3: The stream model's waited <= 2 counts SSR looks, and its CHCR is a plain store

Certainty: certain. Kind: test gap. Where: tests/test_toy_wire_stream.c:205

In this model one byte is clocked per rd8(SSR) and at no other time, and case
0xffa0001c stores the written value unchanged (line 154). Two wrong sequences
pass stream_stop()'s assertion and every other check in the file. (a) A wait
loop bounded at 2 status reads instead of 200000: the model sets ORER on the
second look, while on the console two reads take roughly 100 ns against the
1.28 us needed, so SCR=0 would land mid-byte. (b) Stopping the channel by
writing TE=1 (rd32(CHCR1) | 2): the model's dma predicate (chcr & 3) == 1 goes
false and waited <= 2 holds, while the real TE bit ignores a written 1 and the
run would be waited out as before. The model also has no transfer in flight
and no time between the read and write halves of the read-modify-write. The
shipped code has neither defect; the point is that the assertion cannot
establish the loop bound or the choice of bit.

### Examined and found to hold

- DE cleared while a byte transfer is in progress: the transfer is one atomic
unit (RDR read clears RDRF, DAR1+1, TCR1-1); wire_capture() reads DAR1 only
after fence() and re-reads TCR1/DAR1 after its own CHCR write and second
fence, so start + seg_len - count == DAR1 holds whether or not that last
transfer happened.
- ours checks for a healthy stream after QUIT: SAR1 is fixed, DAR1 stays in
[start, start+seg_len], count stays consistent; this includes a run armed in
the same visit and stopped before any byte (DAR1 == start, count == seg_len).
- TE when the count runs out between the read and the write of the
read-modify-write: the value written carries TE=0, which does not clear a flag
that was not read as 1; wire_capture() then sees TE, count 0, DAR1 ==
start+seg_len. Even if TE were lost, the stop path never calls wire_account(),
so E_END cannot arise.
- TE already set at the stop (run ended, two trailing bytes pending): clearing
DE changes nothing; ORER arrives as before.
- Pending RXI with DE=0 and SCR=0x50: SPTR=0x83 has EIO set, so RXI does not
go to the INTC; the same unserved-RDRF-with-RIE state already existed at every
run end. wire_capture() writes SCR=0 and clears RDRF/ORER, and wire_handoff()
leaves SCR=0x30, long before the visit unmasks; the resident also hooks both
the 0x4E0 and 0x500 slots.
- ERI raised by the forced overrun while SR.BL is set: the pre-QUIT stop also
always ended in ORER (it waited for it) and withdrew the request the same way.
- wire_handoff(overrun=true) after QUIT: status is RDRF|ORER, both read as 1
and then written 0, so step 3's SSR check passes; identical to a cut with a
held byte.
- Stop with ORER already set and not serviced: the loop exits at once; DE=0
only prevents a late take of the held byte, and wire_capture() accepts either
outcome.
- Stop from inside the SCI error interrupt visit: the service runs first
(re-arm to RUN, or WAIT with a timer), then the halt; the fresh run overruns
two bytes after SCR=0x50, the timer is cancelled and the slot restored before
the QUIT line, as before.
- Stop in WAIT (timer armed), FULL, FAULT or OFF: the QUIT line is inside the
state == RUN branch; those paths are byte-identical to before.
- Byte accounting (toy_wire_stream.c:600): seg_begin == run_begin on every
path, so the sum is the bytes the channel stored; the held and overrun bytes
are left out, as they were before. n.bytes feeds only report rows.
- Low reader after the stop: stop_card() sends CMD12 into a CMD18 at an
unknown byte and assumes nothing about how far the card got; cuts and segment
ends already left it at arbitrary positions, gaps included. A stop two bytes
after take-over is exercised in the integration test's m.delay sections.
- Loop bound of 200000 status reads: with DE=0 the receiver in RUN (SCR=0x50,
internal clock) must overrun within two byte times, 1.28 us. Before QUIT the
worst case was 5.24 ms against at least about 7 ms of loop even at one
peripheral clock per read. If it ever ran out, SCR=0 would land mid-byte with
no module reset and the stop would still report usable; that is unchanged and
I see no way to reach it.
- Stop while channel 2 (higher priority, burst) holds the bus: DE=0 is an
on-chip write, ORER comes within two bytes, and wire_capture()'s fence waits
for the bus exactly as it does at any cut.
- QUIT mutations: removing the DE clear trips waited <= 2; SCR=0 in its place
and dropping the wait both trip the model's assert(!receiving()) on the SCR
write; the retired old-wait mutation is covered by the third.

## HOT (commit d585ae0)

HOT holds as written; I found no concrete failure. In the linked Test 76
module (build/dev76/native-ra-aux.elf: context 0xac421040, size 0x720, state
at 0xac421700..0xac42175f) the only access to the member's 96 bytes through P2
is the memset(&C,0,0x720) in kui_native_readahead_aux_init (0x8c423d78). That
call runs once, before any P1 use, into lines the cold bind purged with OCBP
(src/loader/vmu_native_readahead_stage.inc:92-97) and that hold the image's
zeros in any case. Every other access goes through the literals 0x8c421700 (7)
and 0x8c421740 (15). A register-tracking scan of the disassembly found no P2
literal plus displacement or add that reaches 0xac421700 or above; the highest
P2 effective address it tracked is 0xac4216f4. The member starts on a line
boundary and its three lines hold nothing else: the bytes before it lie in the
line at 0x...6e0, bytes 0x748..0x75f are the context's tail padding, and .bss
starts at 0xac421760. All of this is read from source and the linked ELF;
nothing was run on a console, and I ran no test suites.

No findings.

### Examined and found to hold

- Whole-context operations through P2: only memset(&C,0,sizeof(C)) in init
(mov.w literal 0x720, the only 16-bit literal in 0x6a0..0x75f). config_valid
uses &C and sizeof(C) for arithmetic only. RESET, QUIESCE and CAPTURE_START
touch C.stats, C.diagnostics, C.cache, C.config, C.quiesced and similar, never
the member; quiesce() and toy_ahead_clear() reach it through A.
- Struct copies and stored pointers: with the wire stream AHEAD_LOW_PIPE is 0,
so there is no e->cursor=A.cursor copy. The 48-byte memcpys in dispatch are
C.resume.cursor (0xac421698) and the engine cursor. &A.cursor goes only to
ops->feed (kui_retail_cursor_feed itself, retail_async.c:921) and
kui_retail_cursor_begin, both of which work through the pointer they are given
and do not keep it. A.cursor holds manifest (the low reader's), write (code)
and context (NULL): no pointer into the member in another view.
- feed's callback: toy_ahead_write reads &A.cursor through the same P1 address
feed was given, so the two agree, as they did at P2 before.
- P2 bases near the member: 0xac4216bc (C.resume, displacement up to 12),
0xac4216d0 (byte displacements 8 and 9, C.resume.state and variant),
0xac42167c (displacement 24, C.current_event), 0xac421698 (48-byte memcpy
ending at 0xac4216c8). None reaches 0xac421700.
- Init order: init reads only C.busy and C.initialized (P2) before the memset.
toy_wire_reset(true) touches the arena's kept counters, not A. No P1 read of
the page happens between the bind's OCBP loop and init: relocation, the
exports check and the canary writes all go through P2, and
kui_native_passive_publish only does OCBP and sets ICI. A P1 line loaded after
relocation would hold zeros, the same as the memset writes. init cannot run
twice (C.initialized), and a new bind purges again.
- Stale-line risk at bind: the same OCBP loop covers the stack's guard words,
which are written through P2 at bind and read through P0 in every visit since
Test 70; a surviving stale line there would reject every visit. The member's
lines rely on nothing the console-tested builds did not already rely on. Not
console-verified for these three lines specifically.
- Other users of 0x8c4217xx: the gate reads only context+0 and +64 through P2
and the guard words at 0x0c427000 and 0x0c427ff0. The hooks use context+656
(movie_exit) through P2. The worker and reader capture read only the public
prefix. toy_present (0xac421760) and hash.c's ready (0xac421774) share the
line at 0x...760, outside the member.
- hash.c EDC table: table.1 is at 0xac421780, size 0x800, and its OCBI loop
covers 0x8c421780..0x8c421f7f (cache indices 188 and up). The member is at
0x8c421700..0x8c42175f (indices 184..186): no overlap, and OCBI acts only on a
tag hit.
- Line ownership: the context starts at 0xac421040 (a multiple of 32; the
lease base and offsets are checked 32-aligned, and relocation keeps the area
bits and adds one delta to both views). offsetof is 0x6c0, so the member is at
0x...700; its 72 bytes end at 0x748 and the context ends at 0x760. A
write-back of any of the three lines carries only member bytes and padding.
- Runtime cache control: every OCBP in the loader is harmless here
(write-back, then invalidate). The only OCBI at run time is hash.c's own
table. CCR writes while the game runs only set ICI (session invalidate,
publish); the writes with OCI are on the return-to-firmware path, after a full
write-back loop. A game-side CCR write with OCI would lose dirty lines, but
that exposure already exists for the cached arena state and stack since Test
70 and is not new with HOT.
- Callers with interrupts open (kui_toy_movie_ahead_update ->
toy_wire_update_seen, toy_wire_foreground_due, toy_wire_room): they only read
A (variant, error, cursor.done/count, verified, first) and write only the kept
counters. An interrupt visit between two reads can give an inconsistent pair,
so one wrong due/not-due answer, which the dispatch rechecks inside the gate
(KUI_TOY_AHEAD.variant and the request state). Same as before HOT; a single
view is always coherent.
- Cache mode: P1 is valid with the MMU on or off and under any CCR setting
(with OCE off or CB clear it is just uncached or write-through), so HOT needs
no MMUCR test like the stack's P0 alias does.
- Relocation: tools/vmu_native_relocations.py records every internal-address
word from its DIR32 relocation and refuses unannotated ones; the dev76 .reloc
was produced, so the 22 P1 constants are relocated. kui_vmu_native_relocate
keeps each word's area bits.
- Compiler treatment of the out-of-object address: checked in the output
rather than by argument. The accesses are plain loads and stores off the
literals 0x8c421700 and 0x8c421740; values are reloaded after the indirect
feed call; no function mixes a P2 and a P1 access to the member.
- Cache-index conflicts (speed only): the member's indices 184..186 are not
used by the P0 stack (0x0c427000..0x0c427fff is indices 384..511), the stream
state (461) or the kept counters (96). No literal pool is loaded from
0x8c425700..0x8c42575f. Window sectors and ring lines share the index once per
16 KiB, which costs an occasional write-back, not a ping-pong.
- Host tests: host builds still use movie_ahead with the same C. The only
target-only part is the address expression and the alignment, covered by the
compile-time asserts and the linked check. No path the host tests used to
cover became target-only.
- tests/test_toy_wire_linked_hot.py blind spot: it counts 4-byte literals
inside [state, end). An uncached access compiled as a neighbouring base plus
displacement would pass unseen, and existing code does address C.resume that
way (0xac4216d0+8, 0xac4216bc+12; mov.l reach from 0xac4216d0 goes to
0xac42170c), as does the context base plus a 16-bit offset. The test's
docstring states this limit. No such access exists in the current build (scan
above), and the source cannot name the member through C because it is called
movie_ahead_uncached on the target.
- Later commits (COPY, GAP, APART): none changes
native_readahead_aux_internal.h, native_readahead_aux.c or the linker script.
The added lines in toy_movie_ahead.inc and toy_wire_ahead.inc reach the member
only through A. The linked check on the HEAD build reports 22 cached constants
and none uncached (I ran only this read-only script).

## COPY (commit b78c4e5)

COPY (b78c4e5) is sound as written; I found no step-by-step failure on the
hardware, only one gap in its linked test. Decisive evidence: (a) the routine
as linked in build/dev76/native-ra-aux.elf (config: FLUID=1 with COPY, GAP,
APART) disassembles to exactly the source, with the expected encodings (tst
#31,r0 = c81f, movca.l r0,@r4 = 04c3, displacements 4..28, bt.s with add
#32,r4 in the slot, bra with add #-4,r6 in the slot, pops r10, r9, then r8 in
the rts slot). (b) MOVCA.L is executed only when out is on a 32-byte line and
at least 32 bytes of the piece remain, and the same iteration stores all eight
words with no load, stack access or branch in between; the eight loads come
first and stop exactly at the piece's end. (c) toy_ahead_write bounds every
piece to one sector's slot of the sector being assembled (bytes non-zero,
bytes <= 2352-part, lba-A.first < 62), so a claimed line never holds a
neighbour slot's bytes: the 16/16 lines between an even and an odd slot are
always finished by plain word stores. The window ends at arena+145824, a
multiple of 32, so its last line shares nothing with the stream state. (d) The
destination is the P1 view (toy_resume_data clears bit 29), copy-back under
CCR=0x105; the sources (w->block behind the tables, or the low reader's
R.area0/area1) are different memory and never the destination's line. (e)
toy_ahead_write is reached only through ops->feed (kui_retail_cursor_feed,
called synchronously) from toy_wire_drain and toy_wire_seek, both under
toy_ahead_pump, which only kui_native_readahead_aux_dispatch calls, and the
gate is the dispatcher's only caller. Even an eviction between MOVCA.L and the
eighth store would be harmless: the half-written line is written back, fetched
again by the next store and completed, and nobody reads an unverified sector's
bytes.
One check was not run: a throwaway in-memory patch of the linked routine to
demonstrate the test gap below was denied by the permission system, so that
finding rests on reading tests/sh4_mini.py and
tests/test_toy_wire_linked_copy.py.

### Finding 1: The linked test cannot see r8, r9 and r10 restored in the wrong order

Certainty: likely. Kind: test gap. Where: tests/test_toy_wire_linked_copy.py:61 (with tests/sh4_mini.py:144-154)

Machine.__init__ sets all sixteen registers to 0, and Machine.call sets only
R4-R6 and R15 before recording saved = r[8..15]. The test never gives R8-R10 a
value, so all three are 0 at every one of the 3,302 calls. A routine whose
epilogue pops into the wrong registers (for example mov.l @r15+,r8 / mov.l
@r15+,r9 / rts / mov.l @r15+,r10, which swaps the caller's R8 and R10)
restores three zeros into three zeros and passes the check 'a callee-saved
register or the stack pointer was not restored'. The data, MOVCA.L,
touched-memory and 20-instructions-a-line checks all pass too. On the console
such a routine would swap toy_ahead_write's callee-saved values (k, raw, lba
and the like) after every copy with a whole line, giving wrong data or a reset
inside the visit. The routine as committed is correct (push r8, r9, r10; pop
r10, r9, r8), so this is a gap in the test's stated claim only; distinct
values in R8-R14 before each call would close it. COPY_MUTATIONS in
tests/test_toy_wire_stream_mutations.py cover only the C guard, not the
assembly. Established by reading; the in-memory demonstration was not
permitted to run.

### Examined and found to hold

- Register use against the SH C ABI: R8-R10 pushed in that order and popped
r10, r9, r8 (r8 in the rts slot); R0-R3 and R7 are scratch; PR, GBR, R11-R14
untouched; 12 bytes of stack.
- Branches and delay slots: bt.s 3b runs add #32,r4 whether or not taken, then
falls to 5: with R4 already advanced; bra 1b runs add #-4,r6 before the tst at
1:. No branch sits in a delay slot, and all 8-bit displacements are in range
as linked.
- bytes = 0: tst r6,r6 sets T and bt 9f goes straight to the pops; nothing is
loaded or stored.
- bytes < 32 with out aligned: add #-32 goes negative, cmp/pz clears T, bf 5f
adds the 32 back and copies single words until R6 is 0; no MOVCA.L.
- out unaligned with bytes smaller than the distance to the line: single words
until R6 is 0, exit at 1:; the line is never reached.
- Signed test cmp/pz: wrong only for bytes >= 0x80000020, which would merely
fall to the word loop (still correct); toy_ahead_write and the cursor bound
bytes to 1..512.
- Encodings in the linked ELF: tst #31,r0 (c81f, needs R0, which holds a copy
of R4), movca.l r0,@r4 (04c3), mov.l Rm,@(4..28,r4) (1411..14a7): all as
intended.
- MOVCA.L on a hit: an ordinary store into the cached line.
- MOVCA.L on a miss with a dirty victim (for example a line of w->block at the
same index): the victim is written back to its own address before the entry is
claimed, as copy_page-style loops rely on.
- MOVCA.L with cache off, write-through, or a P2 destination: behaves as a
plain store. Here the destination is P1 and copy-back (CCR.CB=1); P0 is
copy-back too (WT=0).
- Stale bytes of a claimed line reaching memory or a reader: nothing but seven
stores to the same line runs between MOVCA.L and the eighth store; visits run
with SR.BL set; and an eviction there would be repaired by the remaining
stores anyway.
- toy_ahead_write outside the gate: not reachable. Its only callers are cursor
feeds in toy_wire_drain and toy_wire_seek, under toy_ahead_pump, called from
toy_movie_ahead and the dispatcher's quick path. kui_toy_movie_ahead_update
and the timer callback enter through kui_native_readahead_aux_visit; the code
that runs outside the gate (toy_wire_update_seen, toy_wire_foreground_due)
never feeds.
- Re-entry: the gate rejects a visit while the context's busy word is set, so
an SCI or timer visit cannot nest inside a copy.
- A piece beginning or ending inside a claimed line: impossible, since a line
is claimed only when out is on it and 32 or more bytes of the same call
remain.
- Adjacent slots sharing a line (2352 mod 32 = 16): an odd slot's first 16
bytes and an even slot's last 16 go by plain word stores, which keep the
neighbour's half; a piece never crosses a slot (bytes <= 2352 - part).
- Last slot against the stream state: 62*2352 = 145824 = 32*4557, so the
window's last line is wholly window. The first line at 0x8c428000 is distinct
from the stack's top line 0x..427fe0, which holds a canary.
- Window wrap and the slot written: only the sector at A.verified is written,
lba - A.first < 62, and toy_wire_room keeps a block from completing more than
one sector.
- Source overlap: w->block lies at arena+145824 or beyond (32-aligned, within
the 4704 bytes before the kept counters, under a static assert); the low
reader's areas are members of the resident's region. Neither overlaps the
window, so no source line is a destination line.
- Source and destination sharing a cache index (including a source that
straddles two lines): costs an exchange per line, never wrong data, because a
line's eight words are all loaded before its MOVCA.L.
- Loads past the piece's end: the line loop loads 32 bytes only while 32 or
more remain, and the word loop 4 while 4 or more remain; bytes is a multiple
of 4.
- The guard in toy_wire_copy(): (out|in|bytes)&3 sends every piece with an odd
place or length to memcpy. With a track at a multiple of 4 in its file all
three are multiples of 4; with file offset 2 every piece fails the guard on in
or out.
- Zero or enormous bytes reaching the routine: toy_ahead_write rejects !bytes
and bytes > 2352-part before the copy; the routine also handles 0.
- Interpreter's MOVCA.L model: poisons the other 28 bytes of the right 32-byte
line (0xEE in the backing store and in the undefined set), faults on a read of
one, and the test fails if any remain or if a byte outside the destination
changed. This is the worst case of the hardware.
- Traced.line_loads: proves eight source loads between the previous
destination store and each MOVCA.L. A load slipped in after MOVCA.L would not
be counted there, but the 20-instructions-a-line check leaves no room for one,
and it would cost time, not data.
- MOVCA.L placement check: the executed list must equal the lines wholly
inside the destination, in order and on each line's first word, so a claim on
a partly written line fails.
- Assembly and C switch agreement: entry.S includes the routine when FLUID and
COPY (default on); C calls it when STREAM and COPY. A mismatch would be an
undefined symbol at link time, not a runtime fault.
- Resident unchanged: the commit touches no file of the low reader;
retail_cursor.c, retail_async.c and retail_resident.c are not in its diff.

## GAP and APART (commits 3893bef and 6a39343)

GAP and APART are sound as written for data: I found no sequence in which a
reader acts on a byte other than what memory holds, and no block that lands in
a buffer sharing a cache place with its ring bytes. The decisive evidence is
(a) every cached read of the ring is preceded, in the same kui_wire_next call,
by an OCBI of that line, and kui_wire_next's call tree contains no writer of
the ring; the linked SEEK loop in build/dev76/native-ra-aux.elf
(0x8c425d7e-0x8c425d94) does OCBI before the load and reloads `fresh` from
index>>5 on every path; (b) a brute-force run of wire_apart with the real
addresses (ring view 0x8c44d000, buffers 0x8c44c4c0 and 0x8c44c8e0) over all
32768 start indices, ring wrap included, gave 0 shared places. The one real
defect is a side effect of APART's layout. Shrinking the struct to 1292 bytes
moved the table block 512 bytes down (TOY_WIRE_TABLE_OFFSET 0x240c0 ->
0x23ec0, confirmed in the linked r75 and dev76 modules). The bit-reversal
table now lies on the cache lines of the private stack's top 288 bytes,
exactly where the old block buffer's first 348 bytes lay. So "clear of the
stack, of the tables in use" holds for the buffers and the two CRC tables but
not for the reverse and inverse tables. This costs time only, and with the
console's MSB-first order it is still a net gain over Test 75.

### Finding 1: APART moves the reverse/inverse tables onto the private stack's cache lines

Certainty: certain. Kind: stall or slowdown. Where: src/loader/toy_wire_ahead.inc:112 (TOY_WIRE_TABLE_OFFSET), src/loader/toy_wire_stream.c:466-468 and 750

TOY_WIRE_TABLE_OFFSET = STATE + round32(sizeof(struct kui_wire_stream)).
Without APART sizeof is 1796, so the tables began at 0x8c44c0c0 (cache place
0x00c0). With APART sizeof is 1292, so they begin at 0x8c44bec0: reverse[256]
at places 0x3ec0-0x3fbf, inverse[256] at 0x3fc0-0x00bf, crc at 0x00c0, crc2 at
0x02c0. The linked literal changed from 0x240c0 (build/r75) to 0x23ec0
(build/dev76, toy_ahead_pump+0x578).
The stack runs from 0x0c427fe0 downward (place 0x3fe0). In dev76 the dispatch
frame is 0x0c427f2c-0x0c427fdf and toy_ahead_pump's is 0x0c427ea8-0x0c427f2b.
All eight lines of the reverse table therefore alias live, dirty stack lines
of those two frames. These are the same lines the old block buffer's first 348
bytes occupied, which is the conflict APART set out to remove.
Step by step in MSB order (the console's, ORD 3): a block whose data does not
start on a word boundary (3 in 4) takes its 1-3 head and 1-3 tail data bytes
through wire_run's STEP, which reads reverse[byte] whenever `out` is set,
regardless of msb (linked: 0x8c4260ae-0x8c4260b0 and 0x8c4260f2-0x8c4260f4).
That is up to 4 lookups per block, each on line 0x3ec0+32*(byte>>5). Each
first touch writes back a dirty stack line and fills the table line; the next
use of that pump/dispatch local fills the stack line again. Before APART the
reverse table at 0x00c0 hit no stack line.
In LSB order (order given up after 4 undecided streams, ORDER_BACK, or an
MSB=0 build) kui_wire_words_lsb reads reverse[] for all 512 bytes. All 8 lines
are swapped against stack lines every block, against 11 lines for the old
buffer, so APART's stack gain is mostly not obtained there.
Consequence: time only, no wrong data. In MSB order it is still a net gain
over Test 75 (at most 4 line swaps instead of 11 or more), but part of the
stated gain is given back. The message's "clear of the stack, of the tables in
use" is true only if reverse/inverse count as not in use.

### Finding 2: Nothing checks APART's placement in the linked module

Certainty: certain. Kind: test gap. Where: tests/test_toy_wire_stream.c:330-347; no APART case in tests/test_toy_wire_linked_*.py

APART's only effect is where things lie in the target's cache. The host test
checks the chosen buffer against the fixture's `seen` array and host `tables`
addresses, and skips every block that runs over the ring's end (`if(from +
514u <= KUI_WIRE_RING_BYTES)`). No linked check derives the real addresses
(state 0x8c44b9a0, sizeof 1292, tables 0x8c44bec0, buffers 0x8c44c4c0 and
0x8c44c8e0) and holds them against the stack page, the tables and the kept
area.
The table move in the first finding passed every check for that reason. Any
later change to struct kui_wire_stream's size (a switch that adds or removes a
member) silently shifts tables and both buffers by a multiple of 32 with all
tests still green. For example, 320 bytes less would put the first buffer's
lines on the stack's places again.
I checked the wrap case and the current layout by hand (brute force over all
32768 start indices with the real addresses: 0 shared places); the repository
does not.

### Examined and found to hold

- GAP, bytes landing after a line was fetched: SEEK only reads positions below
`landed`, taken once before any drop and behind wire_landed's fence; later
bytes of the line are read by a later call (fresh=NONE, drops again) or by
kui_wire_fresh (drops every line of the run).
- GAP, held byte stored uncached by wire_account: kui_wire_next's call tree
(wire_landed, wire_hole, wire_block, wire_run, kui_wire_fresh, wire_words*,
wire_solve, wire_gap_like, wire_tables) never reaches wire_account or
kui_wire_service, and visits run with interrupts masked, so the store always
falls between calls; every cached reader drops the line first in its own call.
- GAP, mixed views: wire_gap_like, wire_run's head/tail/CRC bytes and runs
under 8 words read the uncached address, which is memory itself; a cached line
left by SEEK is dropped by kui_wire_fresh before the word run reads it. No
path reads cached without a drop.
- GAP, ring wrap and `fresh`: index>>5 goes 1023 -> 0, which differs, so the
line is dropped; index>>5 is at most 1023 and never equals NONE; skipping a
hole does not change `fresh`; a full lap back to the same line passes other
lines first.
- GAP, NEED then called again: tail=at=landed; the next call starts with
fresh=NONE and drops the line before its first read. With nothing new landed
the loop body does not run and nothing is dropped or read.
- GAP, DATA phase after SEEK in the same call: the block loop needs landed >=
end (end+1 with an alternative) from the same `landed`; all block bytes had
landed before the drops.
- GAP, a line fill while the DMA writes that line: the fill is one burst bus
cycle and each DMA byte its own cycle; bytes below `landed` were in memory
before the fence returned, and bytes at or above it are never used by this
call.
- GAP, dirty ring lines: nothing writes through the P1 view of the ring,
staging stops at +150528 below the ring at +151552, and kui_wire_start purges
it with OCBP; OCBI therefore loses nothing.
- GAP as linked (build/dev76): OCBI address = view + (at & 0x7fe0), executed
before `mov.b @(r0,r13)`; the compiler reuses r11 for the byte but restores
fresh = index>>5 (`mov r7,r11`) on every continue path; the hole path leaves
r11 alone.
- APART arithmetic: run-own+17 mod 512 < 33 is exactly d in [-17,15]; the
block's bytes span at most lines run..run+17 and the buffer own..own+15.
Unsigned wrap is harmless (2^32 is a multiple of 512). The second buffer at
+33 conflicts only for d in [16,48], disjoint from [-17,15] on the 512 circle.
Linked code matches (logical shifts, +17, &0x1ff, >32, +0x420).
- APART over the ring's end: the ring is 1024 lines, so index 32767 -> 0 is
contiguous mod 512; brute force with real addresses gave 0 conflicts, 2112 of
32768 starts taking the second buffer (2050 strictly needed).
- APART alignment and size: STATE 145824 and round32(1292)=1312 give tables at
+147136 (0x8c44bec0); +1536 = 0x8c44c4c0 is a multiple of 32; the room ends at
+150240, 288 bytes below the kept counters at +150528; the static assert
covers it.
- APART buffers against other places: 0x04c0-0x06bf and 0x08e0-0x0adf share no
line with the stack page, the stream state (0x39a0-0x3eab), the tables
(0x3ec0-0x04bf) or the kept area (0x0c00 on).
- Block pointer lifetime: out->data = w->block is set after the final
wire_block of the call; toy_wire_drain stores k->block = block.data and feeds
in the same iteration; kui_retail_cursor_feed keeps no pointer;
toy_wire_note's data - k->block is always within the buffer that holds this
block; the low reader's blocks in toy_wire_seek set block_lost_count=0 and
kind LOOSE, so a stale k->block is never used.
- Alternative retry: `continue` calls wire_block again, which re-picks
w->block for the new `first` before any write; the rebuilt byte is written to
the same w->block.
- kui_wire_start: the memset to offsetof(hole) zeroes blocks/block, both set
after wire_tables; the early E_BUS/E_CHANNEL returns leave them NULL with
state OFF, where kui_wire_next returns NEED and k->on stays 0.
- Staging over the working state: toy_ahead_clear calls toy_wire_halt before
any staging, even when it then returns false; every start re-zeroes the state,
refills the tables and resets both pointers; each handed block has all 512
bytes written (wire_run, or zero for lost bytes); the newly used range
149184-150240 was already inside the 64-sector staging reach.
- COPY with the new buffers: both are 32-aligned, so the multiple-of-four test
in toy_wire_copy passes as with the old _Alignas(4) buffer; kui_wire_copy
loads a line's 8 words before MOVCA.L, so a destination line at the source's
place only writes the dirty source line back.
- Host fixtures: tests/test_toy_wire_stream.c's tables array is _Alignas(32)
and sized TABLE_BYTES + BLOCK_ROOM; the integration test hands over the whole
arena under the same static assert; the reference-module struct offsets in
tests/test_toy_wire_linked_loops.py belong to the Test 73 module and are not
affected.
- Working tree: it differs from HEAD only in kui_wire_stop's QUIT part and in
tests; the GAP and APART code I read is HEAD's.

## After the review: what the last full run of the checks found (commit 4be776b)

This part is the author's account, not a reviewer's answer.

Commit `7fe7e83` answered QUIT's third finding by making the stream
model's clock a byte for every fourth look at the receiver's status
instead of every look. The checks named in that commit passed: the
Test 76 configuration's host tests, the linked checks, the changed
lists of mutations. The last full run of both host targets in all 29
configurations, on a clean worktree of that commit, then failed in 16 of
its 58 jobs: `toy-wire-host-tests` in every configuration that paces its
visits (Test 72 on) and does not end a stream at once (Tests 72 to 75 and
their variants, and Test 76 without QUIT and with no part). All 16
stopped at one assertion of `tests/test_toy_wire_stream.c`: a block
handed on wrong has lost bytes listed.

The stream was right and the assertion was not. The new clock had changed
the model's random course, and the course met a block of the card whose
data begins FF FF FE with its own token lost (course 9, block 5813). The
reader takes the FE for the token and reads the block three bytes late.
With nothing else lost that reading fails its CRC16 and the other reading
is taken. Here the late reading's last CRC16 byte was lost as well, so
nothing could refuse it: the block went on wrong, with no lost byte
listed, counted as unchecked. That is the corner `kui_wire_next` has left
to the caller's sector check since Test 68 ("the block's CRC, or failing
that its sector's EDC, refuses the reading"), and the window does check
it: a block that is not clean puts its sector through the EDC
(`toy_wire_note`; `wrong_blocks()` in the integration test hands the
window a damaged block whose CRC16 was lost and sees its sector refused
and read again).

The same model compiled without the sanitizers took another course,
because the stall draw took two random numbers in one expression and the
compiler chose their order. On that course Test 76's own configuration
met a block rebuilt wrong (course 6, block 12484): the same late reading,
one of its data bytes lost, and the remainder of the wrong bytes one that
a single byte at that place can leave. `toy_wire_stream.h` has said since
Test 74 that this happens once in 256 such readings and that a rebuilt
block is as good as checked only once the rebuilt byte is confirmed from
elsewhere; the window confirms it from the sector's parity or else by the
EDC, and `wrong_blocks()` hands it a byte rebuilt wrong from a CRC16 that
fits other data as well. The stream's own test had asserted that a block
called rebuilt is never wrong.

So the test had made two statements about every course that were true
only of the courses it had run. Commit `4be776b` (tests only) replaces
them with what follows from the code:

- a wrong block is handed on only behind a byte lost between blocks
  (one of the two counters of gap readings has moved since the last good
  block), and never after a whole CRC16;
- a block called clean is never wrong; one called rebuilt has every
  byte;
- the kinds are exactly what the counters say (rebuilt; loose with lost
  bytes listed or with a CRC16 that lost a byte).

It counts the three kinds of wrong block and makes the two rare ones by
construction, so that they are shown whatever the course: the block
above with the late reading's CRC16 byte lost, and with a data byte lost
(27 readings refused and the block rebuilt right, the 28th fits and is
handed on wrong). The stall draw's two numbers are drawn one after the
other; the test's output is now the same with and without the
sanitizers but for the count of blocks in each of APART's two buffers,
which goes by the host's addresses.

Eight times the usual number of courses (96 instead of 12) then brought
up two statements about the bit-order question of the same sort, in the
configurations that ask it. Before the order is settled, a block may be
handed on with only a byte of its CRC16 lost (the test had said: with
lost bytes listed). And under the model's token-only answer such a block
has every byte the other way round only if it was read from the right
place (the test had said it is wrong all through, which a block of FF is
not, and had then gone on from a block read from the wrong place). Both
are stated exactly now; a block from the wrong place is rewound like any
other wrong block.

What this does not change: no source of the module. The two modules of
the package are the ones built from `7fe7e83`.

What it leaves: the test's run-end statements about the bit order (the
asking is never given up, the order is read right in each of the twelve
courses) and its counts (blocks in a course, share of unsolved bytes) are
still statements about the courses it runs. They hold over all 96
courses tried in every configuration, and they are not claimed beyond
that.

## The corrected stream test, read by two more (commit 4be776b)

Two readers were given the corrected test at commit `4be776b`, the stream's
source, and the account above, and asked to find statements that are not
true on every course, expectations that hold by luck, checks that cannot
fail, and comments that claim more than the code shows. They were allowed
to build and run the single host test in a directory of their own, and
did. What follows is their answers as returned, unedited but for line
wrapping and for paths, which are given as the repository has them
(`<scratch>` stands for a reader's own directory, which is not kept).
Line numbers are those of the test at `4be776b`. The author's
dispositions are in `docs/toy-wire-stream-test-76.md`, Review; what they
led to is commit `61e24e9`.


### The corrected stream test: the model and the constructed corner


#### Finding 1: The gap-loss assertion compares against a stale baseline after a restart, so it cannot fail for the first wrong block of such a stream

Certainty: certain. Kind: vacuous. Where: tests/test_toy_wire_stream.c:424 (assertion); restarts at 339-345 and 428-432 do not refresh last_gaps; only 410 and 434 do

last_gaps is set only when a block is handed on right. Both restart paths
leave it as it was, while the stream just given up has usually moved
n.gap_idle + n.gap_token (the wrong block itself always has, by line 424). The
counters only grow, so for the next block handed on, `n.gap_idle + n.gap_token
!= last_gaps` is already true before the new stream has read a byte. Measured
with an instrumented scratch copy (no change to the course): 16 of 348 wrong
blocks in t76, 22 of 359 in t75 and 23 of 353 in t72 were checked against such
a stale baseline. With the baseline taken again at each start, none of those
wrong blocks failed, in t72, t75, t76 and in t75 at 1, 2, 8 and 16 looks per
byte; so the PASS line's 'each behind a byte lost between blocks' is true of
these courses, but the assertion does not establish it for those blocks. The
weakness predates 4be776b, but that commit removed `b.lost_count &&` from the
same assertion, so the gap counters are now the only condition on a wrong
LOOSE or REBUILT block.

Suggested: Set `last_gaps = n.gap_idle + n.gap_token;` after each kui_wire_start in the
main loop (both restart paths). The stricter form held in every course I ran
(0 failures in 8 sets of 12 courses).


#### Finding 2: The look behind the block (wire_gap_like) can be removed without any failure; the fixture builds only its accepting side

Certainty: certain. Kind: other. Where: src/loader/toy_wire_stream.c:1075 and 879-883; tests/test_toy_wire_stream.c:583 and 592-594

Mutant in my scratch directory: line 1075 changed to `accept =
wire_gap_like(w, end) || true`. Built for t76 against the unmodified test:
exit 0, all 12 courses and the constructed corner pass (340 wrong blocks, all
'with lost bytes listed'). The main loop allows any LOOSE wrong block behind a
gap loss, and the fixture's case (0) only makes the reading whose following
byte is idle (gap_of(block + 1) >= 5), so nothing in this test requires the
stream to refuse a first reading whose following byte is data.
tests/test_toy_wire_stream_mutations.py has no entry naming wire_gap_like
(grep). Not verified: whether tests/test_toy_wire_integration.c would catch
this mutant (not run). The fixture comment's condition for case (0), 'one of
the two bytes it takes for its CRC16 is lost as well', also leaves out this
second condition.

Suggested: Add the refusing side as a third constructed case: the same lost token and
lost CRC16 byte of the late reading, with block + 1 chosen so that the byte
behind the late reading is a data byte that is neither FF nor FE (for example
gap_of(block + 1) == 2); expect the block handed on right and CLEAN with
gap_token == 1. I did not run that construction.


#### Finding 3: Header history: 'the other two kinds came up' is not borne out for the rebuilt kind in any course I ran, and for neither kind in t76

Certainty: likely. Kind: wording. Where: tests/test_toy_wire_stream.c:30-36 (header), PASS line at 682-687

Current test, default 12 courses: t72 prints 352 with lost bytes listed / 1
with a CRC16 byte lost / 0 rebuilt; t75 358/1/0; t76 (the default build)
348/0/0. On t75 with the clock set to 1, 2, 8 and 16 looks per byte (scratch
copy): 357/0/0, 342/1/0, 364/0/0, 354/0/0. The review commit's own test (git
show 7fe7e83:tests/..., new clock, old statements) built for t75 fails in its
tenth course at its line 380, `assert(b.lost_count && ...)`, with the CLEAN
assertion before it silent: by that, a LOOSE block without listed bytes, i.e.
the CRC16-lost kind (which conjunct failed is my inference). So verified: the
CRC16-lost kind came up in the default course of t72 and t75. Not verified
anywhere: a rebuilt-wrong block in a random course; in every run I made it
exists only by construction, and in t76 so does the CRC16-lost kind. The
commit message's eightfold courses, the other configurations and the sanitizer
builds were not run.

Suggested: Say which kind came up in which course (CRC16 byte lost: default course of the
Test 72 to 75 builds), and where the rebuilt-wrong kind was seen, or that it
is met by construction only. The PASS line already prints the zeros honestly.


#### Finding 4: The search for the rebuilt-wrong reading does not sample a 1-in-256 chance: which places fit is fixed by the construction and is the same for every block

Certainty: certain. Kind: wording. Where: tests/test_toy_wire_stream.c:574-577 (comment), 582-584 (search), 650-654 (printed line and `right >= 8u`)

Sweep in a scratch copy (t76) over every qualifying block below 700 and every
place 8..499: 73 blocks, 35,916 readings, 219 rebuilt wrong, exactly 3 per
block; listed for blocks 10, 17, 24, 31, 52, 59: always k = 35, 353, 418.
Reason: a whole block with its CRC16 is a code word, so the reading three
bytes late has a remainder that depends only on the three bytes dropped (FF FF
FE) and the three taken in (FF FF FF of the next gap), not on the data; with
one byte lost it fits exactly when that constant is a single-byte remainder at
the lost place. Consequences: '1 of 28 readings' is the position of the first
fitting place from 8 on (k = 35 in t72, t75 and t76 alike), the rate over the
places tried is 3 in 492; the outer block loop for the second case is a dead
fallback (a block can never fit where block 10 did not); `right >= 8u` is a
fixed fact (27), not a statistical margin. 'The design reckons with one such
reading in 256' holds as an average over arbitrary wrong data, but the printed
figure is not a sample of it.

Suggested: State in the comment that the fitting places depend only on the place of the
lost byte (35, 353, 418 for this shape); drop the block loop for the second
case or assert the place found.


#### Finding 5: The lose list is exact only if the byte before the listed one is itself received; nothing in the model says when it was not

Certainty: certain. Kind: other. Where: tests/test_toy_wire_stream.c:113-117 and 124 (shift_one)

The stall is hung on the byte before the listed one. If that byte is itself
the one that overruns (directly behind another listed byte, or as the second
byte behind a run that reached its count), the stall has no effect and the
listed byte opens the next run and is stored. Verified in a scratch copy: with
the lost data byte's place set to 0 (directly behind the lost token) one
position is logged (1074), one overrun, block handed on clean; with place 1
(the byte before it is a run's first byte) exactly the listed byte is logged
(1074, 1076). Never two bytes for one entry. The fixture cannot reach the bad
state (places from 8, the other entry in the next block's gap, every position
below 1,700 of runs 8,192 long) and its counters would fail if it did, but the
fixture never compares the list with the stream's log.

Suggested: Add to the fixture, before stream_stop: `assert(w.hole_put == 2u && w.hole[0]
== T && w.hole[1] == (which ? T + 1u + k : T + 517u))` with T =
gap_of(block-2) + 515 + gap_of(block-1) + 515 + gap_of(block). It held for all
29 fixture tries (t72, t75, t76) and 35,989 sweep tries (t76), together with
exactly two overruns per try.


#### Finding 6: The printed stop statistics now include the fixture's 29 constructed stops

Certainty: certain. Kind: wording. Where: tests/test_toy_wire_stream.c:644 (fixture's stream_stop), 672-673 (printed line), 681

stream_stop() counts into stops_receiving, stop_bytes and stop_most, and the
constructed corner calls it once per try. Measured before and after the
fixture: t75 1,162 stops and 395,634 bytes from the courses, printed as 1,191
and 585,679 (190,045 bytes, a third of the figure, are the fixture's: each try
stops in a fresh run of 8,192); t72 1,135 / 344,388 printed as 1,164 /
534,433; t76 1,111 / 2,220 printed as 1,140 / 2,278. The line reads as a
statistic of the random course ('streams given up while they received'), and
for the builds without QUIT it is the number that shows the wait QUIT removes.
The assertions themselves are not weakened in what they say about the stream.
Before 4be776b the builds from Test 72 on had no fixture stop in these
numbers.

Suggested: Print and assert the three counters as they stand before the fixtures, or
count the fixtures' stops apart.


#### Finding 7: The hold-for-room state is never entered in the courses, so the FULL alternatives in the test's assertions are untested

Certainty: certain. Kind: vacuous. Where: tests/test_toy_wire_stream.c:307 (`|| w.state == KUI_WIRE_FULL`), 447 (`+ n.fulls`), 476 (fulls printed)

n.fulls is 0 in every course of t72, t75 and t76 at the shipped clock and of
t75 at 1, 2, 8, 16 and 32 looks per byte (summed in the instrumented copy; the
per-course lines print 'fulls 0'). The reader in the model never falls 24 KiB
behind, so wire_arm's hold, kui_wire_service's FULL case and a `want` taken up
from FULL are not run by this test, and the two assertion terms above cannot
be told from their absence. Not part of 4be776b; noted because it is the same
sort of statement the commit corrected (one the course never puts to the
test).

Suggested: A short constructed case with the reader kept away for four runs, or a course
whose reader stays away long enough.


#### Finding 8: Header: 'the caller confirms every rebuilt byte from its sector's parity' says more than the window does

Certainty: certain. Kind: wording. Where: tests/test_toy_wire_stream.c:26-27; src/loader/toy_wire_ahead.inc:35-38, 343-356, 400-401

By toy_wire_ahead.inc a rebuilt byte is confirmed by its P code word only
where one covers it; a rebuilt byte in the Q parity or past TOY_WIRE_P_END,
one lying in the other sector the block touches, or a seventh in one sector
sets k->loose, and a code word that does not hold counts mend_bad: in each
case the sector goes through its EDC. Builds without CLEAN send every sector
through the EDC. The confirmation is one byte of check (XOR of the 26 bytes,
toy_wire_mend_holds).

Suggested: '...confirms a rebuilt byte from its sector's P parity, or else passes the
sector through its EDC.'


#### Finding 9: The window cases show one wrong byte refused, not a misplaced block; case (a)'s check does not require the block to have been unchecked

Certainty: certain. Kind: other. Where: tests/test_toy_wire_integration.c:1863-1869 and 1893 (WRONG_UNCHECKED), 1836-1843 (WRONG_REBUILD)

By reading only (integration test not run). (a) is shown for a block with one
data byte changed on the wire (byte 77, bit 0x10,
tests/sci_stream_model.h:89-90) whose first CRC16 byte is lost; line 1893
checks `w->n.holed == 1u && w->n.unchecked <= 1u`, which also passes with
unchecked 0 (the comment allows a run's end in the same block), so no
placement is required to be the pure CRC16-lost case. (b) is shown for a block
wrong in the rebuilt byte alone (CRC16 sent changed by crc_of_change(200,
0x5a), byte 200 lost), where a single wrong byte can never satisfy its P code
word (mend_bad == 1, line 1895). The shape the stream test now constructs, a
block wrong all through its own sector, goes the same ways (LOOSE to the EDC,
toy_wire_ahead.inc:356; REBUILT to the code word and then the EDC), but for
the rebuilt one the code word then holds several wrong bytes and can hold by
chance once in 256, after which the sector is taken without its EDC unless it
is the sample: the design's stated residue (toy_wire_ahead.inc:31-35), not
something a test shows refused. 'All left to the sector check' in the PASS
line means, for that kind, left to one byte of P parity first.

Suggested: Make line 1893 require `w->n.unchecked == 1u` in at least one counted
placement; optionally a window case fed a REBUILT block that is wrong in bytes
other than the rebuilt one.


#### Finding 10: Not reached

Certainty: certain. Kind: other. Where: scope of this review

Run: t72, t75 and t76 of the 29 configurations (current test), plus the
pre-clock test on t68, t72, t75 and the review commit's test on t75. Not run:
the other 26 configurations, the eightfold courses named in the commit
message, sanitizer builds, tests/test_toy_wire_integration.c (read only),
tests/test_toy_wire_stream_mutations.py (read its list only). The sweep of the
constructed corner covered blocks below 700 in t76 only; blocks 700 to 2000 by
reading. The suggested third constructed case was not built. Nothing about
console timing was examined: the clock of a byte per four looks is the
model's, and the only limit I measured is the model's own (kui_wire_stop's
200,000 looks cover a whole run up to 24 looks per byte). Scratch material:
<scratch>/agent-model/
(patch_test.py, mutate.py, x_test.c, y_test.c, z_test.c and outputs). The
repository was not modified (git status clean).


#### Examined and found to hold

- Q1, in the fixture: the listed card byte is exactly the position the stream
  logs. Added assertion in a scratch copy (w.hole_put == 2, hole[0] == token
  position, hole[1] == T+1+k or T+517, two overruns before the stop) held for
  all 29 fixture tries in t72, t75, t76 and all 35,989 sweep tries in t76,
  with the late take drawn at random in each try.
- Q1, late take either way: wire_account
  (src/loader/toy_wire_stream.c:345-347) computes lost = at + held; a held
  byte still in the register (held 1) and one the channel took late (held 0,
  address one further) give the same lost position. By reading, and by the
  sweep above.
- Q1/Q2, no run's end or ring wrap in the scenario: the token lies at most at
  position 1,141 and the decision needs position T+518, at most 1,660; the
  first run and each run after a cut are 8,192 long (printed: seg_len 8192,
  cuts 2, runs ended 0; a third log entry would have failed the added
  assertion). want is 0 and FULL is not reached.
- Q2 case (0): the counters follow from the code. block+1 at 2 is the late
  reading's second CRC16 byte (first+513); need = end+1 (line 1053) cannot be
  met before the run behind that lost byte has started, so the entry is always
  logged before the decision; ring[end] is idle because gap_of(block+1) >= 5
  (3 would do); line 1075 accepts, 1101 counts gap_idle, 1109-1112 count holed
  and unchecked, kind LOOSE. All 73 qualifying blocks below 700 gave exactly
  this.
- Q2 case (1): both branches follow from the code. Late reading: one lost data
  byte at k-3, rebuilt if wire_solve fits (rebuilt 1, gap_idle 1, rebuilt_at
  k-3); otherwise line 1073 refuses, 1079-1082 take the other reading
  (gap_token 1), whose single lost byte always solves and solves right
  (rebuilt_at k). 35,916 readings in the sweep met the fixture's own
  assertions including rebuilt_at (t76, CLEAN on).
- Q2 data: only the block's first three bytes (FF FF FE by data_byte's first
  rule; every block of the search is 3 mod 7 since it starts at 10 and steps
  7) and the first gap bytes of block+1 are interpreted; bytes inside a block
  are never looked at for FF or FE. The exclusions of block % 11 == 5 and % 13
  == 6 are not needed (the first rule wins for i < 3; an all-FF tail changes
  no step) but harmless; that part by reading.
- Q2 'the two blocks before it are whole and right': line 615 with n.blocks ==
  3 and the totals of n.rebuilt and n.unchecked pins both as clean; nothing is
  lost before the token.
- Q3 'the alternative reading never tried' (if(0 && w->alternative) at line
  1077): caught by the main loop at test line 473 in the first course without
  stalls, and by the fixture at line 620.
- Q3 'a block with a lost CRC16 byte called clean' (kind and the unchecked
  count both dropped): the 12 courses of t76 pass; only the fixture fails,
  line 624. With the count kept and only the kind changed, line 388 would fail
  (by reading).
- Q3 'the solved byte stored at the wrong index' (lost[0] ^ 1): caught by the
  main loop at line 424 on a rebuilt block with no gap loss.
- Q3 further mutants killed by the fixture alone in t76 (12 courses pass):
  rebuilt_at reported one off within range (line 640); need = end, the
  decision not held for the byte behind (line 624); a first reading whose lost
  byte the CRC16 cannot explain handed on instead of refused (line 620); the
  other reading tried first (line 624: the fixture pins the present order of
  the two readings).
- The fixture does not depend on the course before it: it passes with 0
  courses, with 12, and on t75 at 1, 2, 8 and 16 looks per byte; block 10
  (gaps 21, 16, 7, 25; token at 1074) and k = 35 in t72, t75 and t76.
- Q4 bounded waits: wire_capture's 512 looks and kui_wire_stop's 200,000 looks
  do not end early at a byte per four looks. Longest run of consecutive status
  looks while receiving: 8 in t76, 32,772 in t72 and t75. The model does catch
  an early end (assert(!receiving()) at test line 168): shown at 32 looks per
  byte on t75, where a stop in a fresh run needs more than 200,000 looks.
  wire_deselect's loop runs with the receiver off.
- Q4 assertions and the clock: the current test passes on t75 at 1, 2, 4, 8
  and 16 looks per byte. stream_stop's `waited <= 2u` counts bytes, not looks;
  the PACE assertion at lines 281-284 takes `was` before the call and the flag
  after it, so a look that itself brings the overrun is allowed. Looks clock
  0.05 percent of the bytes in t76 (48,523 of 97.8 million) and 0.45 percent
  in t75.
- Q5 (a) confirmed by reading: wrong_block(WRONG_UNCHECKED),
  tests/test_toy_wire_integration.c:1863-1869 ('A block damaged on the wire
  whose first CRC byte is lost: its CRC16 cannot be taken, and nothing but the
  EDC can refuse it'), refused at line 1887 `CHECK(w->check_bad == 1u &&
  w->stops[KUI_TOY_WIRE_STOP_SECTOR] == 1u && !w->refused && !w->n.bad_crc);`,
  data compared by read_request (line 456), at least 3 placements (line 1910).
- Q5 (b) confirmed by reading: wrong_block(WRONG_REBUILD), lines 1836-1843
  ('sent with the CRC16 of a block that differs in the one byte then lost: the
  stream rebuilds that byte wrong, in good faith. Its P code word must not
  confirm it'), same check at 1887, `w->n.rebuilt >= 1u` at 1893, `w->mend_bad
  == (kind == WRONG_REBUILD ? 1u : 0u)` at 1895; WRONG_AHEAD (1844-1853) and
  WRONG_BEHIND (1854-1862) cover a rebuilt block wrong in the neighbouring
  sector.
- Q6 confirmed: the test of 6a39343 (a byte with every look), unmodified,
  built against the current stream source for t75 passes: '331 uncheckable
  blocks handed on wrong, each with lost bytes behind a lost gap byte', and
  that version asserts b.lost_count for each. Same for t68 (363) and t72
  (341). It needs -DKUI_WIRE_BLOCK_ROOM=0u because the review commit renamed
  that constant; these three builds have APART off, where it is zero. The
  current test with the clock set back to one look per byte prints 357/0/0 on
  t75.
- 'The stream is as it was': 4be776b changes tests/test_toy_wire_stream.c only
  (git show --stat). The review commit 7fe7e83 did change the stream source,
  but only kui_wire_start's block buffer, wire_apart and the ownership test in
  kui_wire_stop; kui_wire_next, wire_block and wire_solve are untouched, and
  the pre-clock test passing against the current source shows the new kinds
  come from the course.
- 'A block handed on with no lost byte after a clean CRC16 is never wrong':
  checked on every block, line 384 in builds with CLEAN, and through line 426
  (a wrong block that is neither listed nor unchecked must be rebuilt) in
  builds without.
- The stall draw at line 125 is now two sequenced draws; I found no other
  expression in the test that draws twice without a sequence point (by
  reading). Under this compiler the sequenced order is not the order the old
  expression took (old test 331 wrong blocks on t75, current test at the old
  clock 357), so the plain build's course changed with 4be776b as well; no
  statement depends on it.

### The corrected stream test: whether its statements follow from the stream's code


#### Finding 1: A block read from the wrong place can equal the card in every byte not listed as lost; the test calls it right, the stream then loses the frame, and the run's 'no lost frame' assertions fail

Certainty: certain. Kind: course-dependent. Where: tests/test_toy_wire_stream.c:347-352 and 434 (a block is right if its unlisted bytes match), 338 with 456, 471 and 479 (no lost frame; 465 and 488 in the working tree as changed at 11:13 UTC); stream: src/loader/toy_wire_stream.c:861 and 1076-1083

The test assumes that after a block whose unlisted bytes equal the card's, the
stream stands at that block's true end. That is not so on every course.

Mechanism: when the reading behind the token has more than 24 lost data bytes
(wire_block returns NONE) and a gap byte was lost before the token,
kui_wire_next takes the other reading, which begins before the token. With two
or more lost bytes and no reading left open it is handed on with no check at
all. On the default twelve courses this is how 142 of the 348 wrong blocks
arise with the Test 76 flags, and 147 of 359 with the Test 74 flags (measured
in a scratch copy).

For two kinds of card block that early reading can equal the block itself:
- Data FF FF FE then FF (b%7==3 and b%13==6; 165 blocks below 15000), read
three bytes early: two gap bytes and the token stand for FF FF FE. Only the
block's own data byte 2 has to be among the lost.
- Data FE then FF (b%11==5 and b%13==6, not b%7==3; 90 blocks), read one byte
early: only data byte 0 has to be lost.

Constructed course, run with the stalls given instead of drawn: block 45, with
these bytes lost: the gap byte three before its token; data bytes 2, 10, 20,
..., 230 (24 bytes); data byte 510. The block is handed on LOOSE with 24 lost
bytes listed, read at stream position 1115 where its data begins at 1118, and
the test calls it right. The next call returns KUI_WIRE_LOST_FRAME, because
the stream meets data byte 511 and the CRC byte 0x24 where it expects the gap.
rewinds[1] and n.lost_frame become 1, so line 456 (builds with MSB), 471 and
479 fail. Block 71 with its last gap byte and data bytes 0, 10, ..., 230 and
511 lost does the same. No assertion in the loop body fires; the gap assertion
passes because gap_token moved.

The stall generator can draw this: one burst of 23 dense stalls with its
opening and closing stall, then one stall 280 bytes on.

Both constructions also fail against the working-tree version of the test
(re-run at 11:18 UTC).

Frequency, estimated and not measured: over the two default courses, 289 wrong
blocks were read early, 13 of them three bytes early and 2 one byte early; 3
were on blocks of the two kinds; one early reading had its lost byte where a
block of the kind would have matched. With 255 such blocks in 15000, that
gives roughly one failing execution in a few hundred per configuration, so
with 58 configurations perhaps one chance in five to ten whenever the courses
are redrawn.

Suggested: Judge a block by its place as well as its bytes. The test already reads w.tail
- 514 for APART; compare it with the card position of the wanted block (stream
positions begin at 0 with each kui_wire_start, at card_at 0) and treat a block
read from another place as wrong, whatever its bytes. In the scratch copy this
check flagged every such block, and no block the test called wrong was read
from its own place (707 wrong blocks on two default courses, 60000 constructed
scenarios). Add the two constructions beside the corner.


#### Finding 2: Under the 'honoured' answer the order can end as BACK: one misplaced whole reading while the order is SEEN breaks the assertion tying n.order to the model's answer

Certainty: certain. Kind: course-dependent. Where: tests/test_toy_wire_stream.c:413-414 and 455, and the summary text at 662-664 ('each read right'); stream: src/loader/toy_wire_stream.c:1084-1091

The stream sets KUI_WIRE_ORDER_BACK whenever a reading with no lost byte fails
its CRC16 with no other reading open while the order is SEEN. It cannot tell a
wrong bit order from a wrong place. The test asserts that under model answer 1
the order is MSB whenever it is not SEEN.

Under answer 1 the order is still SEEN after block 1 on every stalled run,
because the first stall always loses stream byte 101, inside block 1. If each
further block has a lost byte, and then a block that begins with FF loses its
token and one byte of its leading FF run, both readings of that block are
wrong and whole. The stream refuses with BAD_CRC and sets BACK. The test
restarts, the new stream reads LSB first, and every later block is right with
n.order BACK. Line 413-414 fails at the next block; line 455 would fail at the
end of the run.

Constructed course, answer 1: lost stream byte 101 (block 1, data byte 89);
block 2 data bytes 411, 441 and 491; block 3's token and data byte 1 (block 3
begins FF FF FE); nothing in the next 517 bytes. In the scratch copy the
test's own check at 413-414 fails, against both the committed and the
working-tree version. The generator can draw it: one burst of three dense
stalls ending in a spacing of two. A search over 60000 constructed scenarios
met this failure 29 times, and no other check of the loop body failed.

On drawn courses it is very rare, since it needs the exact placement within
the first blocks of a run; my rough estimate is of the order of 1e-5 per run,
not measured.

Stream side, for the record only: once BACK, kui_wire_start (line 530) never
asks again while the caller keeps the counters, so an honoured SDIR is given
up for the session at the cost of speed, not correctness. I propose no stream
change.

Suggested: Keep n.order from before the call next to unchecked_before. Under answer 1
allow BACK only after a CRC16 refusal that came while the order was SEEN,
count those, and assert MSB otherwise at 413-414 and 455. Add the four-byte
construction so the case is met on purpose.


#### Finding 3: In commit 4be776b the gap assertion compares against a count taken before a restart, so it says nothing for the first block of a restarted stream

Certainty: certain. Kind: vacuous. Where: tests/test_toy_wire_stream.c:424 against the restarts at 339-345 and 428-432

last_gaps is set only when a block is right or placed. After a wrong block
(whose counters moved, by this same assertion) or a refusal that moved a
counter, the stream is restarted and last_gaps is left alone. If the first
block of the new stream is wrong, the assertion is already satisfied before
that stream has received a byte.

Measured on the default courses: 19 of 348 wrong blocks (Test 76 flags) and 29
of 359 (Test 74 flags) were the first block after a restart.

The stronger statement does hold. A new stream begins at card_at 0 and its
first byte cannot be lost, so its first block is wrong only behind a byte lost
in its own gap. In all 48 of those blocks, and in 60000 constructed scenarios
with stalls placed after the restart, a counter had moved since the restart.

The working tree as changed at 11:13 UTC (not by me) already sets last_gaps at
both restarts and says 'since the block before it, or since this stream
began'. That closes this.

Suggested: Keep the working-tree change: last_gaps = n.gap_idle + n.gap_token after both
kui_wire_start calls in the loop.


#### Finding 4: 'Only a block its CRC16 could not refuse' is not exact: a wrong block with one lost byte listed is one its CRC16 did show wrong

Certainty: certain. Kind: wording. Where: tests/test_toy_wire_stream.c:20-27 (header), 417-423 (comment at the assertion) and the PASS text at 682-685; stream: src/loader/toy_wire_stream.c:1069-1074

When exactly one data byte is lost and wire_solve finds no value for it, the
remainder proves that the other bytes are not a block. The stream refuses that
reading only while another is open. Otherwise it counts `unsolved` and hands
the block on with its lost byte listed.

On the default courses 4 of the 348 wrong blocks (Test 76 flags) and 4 of 359
(Test 74 flags) were of this sort; they are exactly the n.unsolved counts of
the runs. The test counts them under 'lost bytes listed', which the header and
the comment describe as blocks the CRC16 could not refuse.

The early readings in the first finding are a second sort the wording does not
fit: they are handed on because the reading before them had too many lost
bytes, not because a CRC16 was consulted.

The working-tree version keeps this sentence (line 23).

Suggested: Say what the code does: a wrong block is handed on only when it was not
checked whole by its CRC16. That is lost bytes listed (a single one when the
CRC16 could not place it and no other reading was open), a CRC16 that lost a
byte, or a single lost byte the CRC16 seemed to explain.


#### Finding 5: The header's account of wrong blocks leaves out the blocks handed on bit-reversed under the token-only answer

Certainty: certain. Kind: wording. Where: tests/test_toy_wire_stream.c:16-28 against 390-412 and 662-664

The header says every byte not listed as lost equals the card's, and that a
wrong block is handed on only behind a byte lost between blocks. Under model
answer 2, each block handed on before a whole block exposes the answer has
every byte the other way round. It is neither equal to the card's nor behind a
lost gap byte.

The code treats these correctly: it checks them against rev8 of the card,
counts them as `unsettled` (6 on the default Test 76 course) and keeps them
out of the wrong count. The comment at 390-398 describes them. Only the header
and the PASS line's 'each behind a byte lost between blocks' do not mention
them.

In every stalled answer-2 run, block 1 is such a block, because the first
stall always loses stream byte 101.

Suggested: Add one sentence to the header: under the token-only answer, blocks with lost
bytes go on bit-reversed until a whole block exposes it; they are counted
apart.


#### Finding 6: The 'undecided' path is never taken in this test: the stream that asks always sees its first token before the first stall

Certainty: certain. Kind: vacuous. Where: tests/test_toy_wire_stream.c:259, 333-336, 456 and 465-469

until_stall starts at 100, so the first stall holds byte 100 and loses byte
101. The first token is at byte gap_of(1) = 11, and no gap exceeds 37. Nothing
else can lose a byte earlier: the first run is a full segment, and `want` acts
only on later runs. So the order leaves TRY at the first token of the first
stream and never returns to it.

Consequences: `++undecided` never runs. `n.lost_frame == undecided &&
n.undecided == undecided && undecided < 4u` reduces to two zero checks.
`n.starts -= undecided` does nothing. The fallback after four undecided
streams (order forced to LSB, which under answers 1 and 2 would itself break
lines 413-414 and 455) is not exercised here.

Measured: zero undecided streams in every run of both default courses.

tests/test_toy_wire_integration.c mentions the path (lines 661 and 2064); I
did not examine it.

Suggested: Either say in the comment that this test never loses a byte ahead of the first
token and that the case is covered elsewhere, or construct it with the lose[]
list.


#### Finding 7: The gap assertion and the clean assertion hold, but they also rest on two properties of this card, not on the CRC16 alone

Certainty: likely. Kind: other. Where: tests/test_toy_wire_stream.c:384, 424 and 426; card: gap_of() and data_byte() at 67-76

Taking only 'a wrong block does not pass a whole CRC16' as given is not quite
enough. Two facts about the card are also needed. I checked both by
enumeration; the failures they prevent are reasoned, not constructed, because
the card does not offer them.

(a) No all-FF block has a CRC16 byte equal to FF. The three kinds have 7F A1,
F5 01 and 24 E0, and two neighbouring bytes are never both lost. Otherwise a
frame whose token is lost could be passed over whole as idle and the next
block handed on clean while the test still wants the passed-over one. Lines
384 and 426 would fail.

(b) None of the 90 FE-first all-FF blocks below 15000 is followed by a zero
gap. Such a block, read one byte late with its token, data byte 1 and first
CRC byte lost, equals the card in every unlisted byte. The search produced
this 9 times; the test calls it right and nothing fails, because the stream
finds the next token anyway. With a zero gap behind it, the next block would
be read from inside its data with neither gap counter moved, and line 424
would fail.

Suggested: Assert the two card properties once at the start, so that a change to
data_byte() or gap_of() reports itself instead of changing what the loop's
assertions rest on. Judging blocks by place, as suggested in the first
finding, removes the dependence in (b).


#### Finding 8: Not reached / not verified

Certainty: certain. Kind: other. Where: scratch only: <scratch>/agent-invariants/ (patch2.py, harness3.c, kt76, kt74, w76, scen.txt, card.c)

All five questions were reached. What I did not verify:

- The frequencies in the first two findings are estimates from measured counts
and the generator's spacing rule. I did not run enough courses to measure
them.
- Only two configurations were run in the scratch copy (the t76 and t74 lines
of configs-all.txt). The others are covered by reasoning only.
- I did not examine the window (toy_wire_ahead.inc), the mutation script or
the integration test.
- I cannot check the header's history paragraph (what earlier courses met).
- The working tree changed during the review (three test files, 11:12-11:13
UTC, not by me). Line numbers and measurements are for commit 4be776b. I did
not review the reworked corner construction in the working tree; I only re-ran
the two constructed courses against that version, and both still fail.

The scratch copy is the committed test with the stalls given as a list, a
position tracker and counters added; the stream source is unchanged. Files
dated 07:3x in that directory are not from this review.

To reproduce:
- SCEN="1,1,6;1:90,2:412,2:442,2:492,3:0,3:2" kt76
- SCEN="$(sed -n 1p scen.txt | cut -d' ' -f2)" kt76

The format is: answer, first block, stop before block; then block:place pairs,
with place counted from the block's token (0 the token, 1+k data byte k, 513
and 514 the CRC16, negative for gap bytes).


#### Examined and found to hold

- Line 352, li == b.lost_count: wire_block lists lost data places in ascending
  order, all below 512, and a rebuilt block reports none.
- Lines 362-371, APART: a block's 514 bytes touch at most 18 cache lines;
  wire_apart takes the first buffer only when the block's first line is 16 to
  494 places past the buffer's, and the second buffer lies 64 lines on (48
  without WORDS), out of reach. Held on both default courses and in 60000
  constructed scenarios.
- Lines 382-386, kind is one of three, a block not LOOSE has no lost byte
  listed, rebuilt_at < 512: follow directly from toy_wire_stream.c:1098-1108.
- Lines 387-388, kind against counters: exact. rebuilt, holed and unchecked
  move only on the path that returns a block; a rebuilt block has lost_count 0
  and leaves unchecked alone; unchecked moves exactly when no data byte but a
  CRC16 byte was lost.
- Line 384 (clean is never wrong) and line 426 (wrong with nothing listed and
  the CRC16 whole means rebuilt): the stream never reads backwards, so a whole
  frame with zero remainder that is not the wanted block would have to be a
  later block behind a frame passed over as idle, which no frame of this card
  allows (see the card-properties finding). No failure in 707 wrong blocks or
  60000 scenarios.
- Line 424, gap assertion: data_at is set in three places
  (toy_wire_stream.c:1027, 1030, 1079); only 1027 with no gap hole open moves
  no counter, and any lost byte met by the search leads to a count on every
  path to a block (978, 999, 1032, 1081, 1101). A wrong block with no count
  therefore needs the previous block to have ended off its true end, which on
  this card never leads to a block being handed on.
- Line 425, how: an exact partition, since lost_count > 0, unchecked moved,
  and (clean or rebuilt) are the only outcomes of a returned block.
- Lines 400-402, SEEN means the answer is not 0 and the block has a listed
  lost byte or a lost CRC16 byte: exact. SEEN arises only from a first token
  that came as FE with no lost byte ahead; under answer 0 that token arrives
  as 7F. A block returned with the order still SEEN was neither whole (that
  sets MSB) nor rebuilt (toy_wire_stream.c:1066).
- Lines 404-411, 'placed' under answer 2 and the fall-through: exact on this
  card. From its own place every byte arrives reversed and the reverse table
  is the identity; I found no block that matches rev8 of the card from another
  place (checked the three all-FF kinds by hand; none in about 20000 answer-2
  scenarios).
- Lines 413-414 for answers 0 and 2: hold. LSB is final under 0. Under 2 the
  order is SEEN or BACK; MSB would need a whole block to pass with every byte
  reversed, and the all-FF blocks' CRC16 bytes do not read the same backwards.
- Bookkeeping of expect, card_block and card_at: right in every path. expect
  advances only on a right or placed block; both restarts put the card at
  card_at 0 of expect; stream positions restart at 0 and position 0 cannot be
  lost. The placed path skips idle = 0 and ++total, which is harmless.
- stream_stop inside the loop: with QUIT the channel is disabled, the next
  byte is held and the one after overruns, so at most 2 bytes are clocked;
  none when the stream was not receiving. Held 2561 times on the default
  course.
- The model never loses two neighbouring bytes: a byte is lost only while the
  receiver is full, so the byte before it was held, and the byte after a lost
  one opens a new run. I relied on this throughout.
- 'Its beginning was read wrong' (comment at 417-419): every block the test
  called wrong was read from another place than its own, in all 707 wrong
  blocks of two default courses and in 60000 scenarios.
- Line 125, stall draw: the two random numbers are now drawn in sequence in
  both branches (a declaration, and a conditional with a comma).
- Corner by construction, committed version: the second lost byte (block+1,
  place 2) is the late reading's second CRC16 byte, and gap_of(block+1) >= 5
  makes the byte behind it gap-like. It runs and reports 1 of 28.
