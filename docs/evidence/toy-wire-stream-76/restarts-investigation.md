# Streams begun again in Test 76: an investigation

An independent investigator was asked, after the Test 76 console run
and before the run of the variant without GAP, why the number of streams
begun again rose (640 ended on a byte between blocks that is neither
idle nor a token, against 460 in Test 75), and whether a part of Test 76
causes it. It read the code and the saved reports, disassembled a
development build of the auxiliary, and ran nothing on a console. What
follows is its answer as returned, unedited but for line wrapping and
for paths. Its reading of what the variant's run would mean is point 9;
that run (report 66-11: 628 such stops, 52 single losses the CRC16 could
not explain) is the outcome it reads as clearing GAP. The author's use
of it is in `docs/toy-wire-stream-test-76.md`, Console result.

## Summary

The rise is real, and I found no way for GAP or any other Test 76 part to
cause it in the code. Why the rate doubled is not established: nothing in the
Test 76 report can tell which kind of event the 640 are, because the counters
that classified them were compiled out in Tests 72 to 75.

- **H1 (GAP reads a stale byte): refuted, with one hardware assumption left
open.** Every cached read of the ring follows a drop of its line in the same
call, in the source and in the linked code I disassembled. The open point is
that GAP's invalidate and its load are adjacent instructions, which RING never
does. If the SH7091 let that load still hit the old line, GAP could add at
most about 180 stale reads a run and realistically far fewer.
- **H2 (known anomaly, more frequent because the reader answers sooner): not
determinable.** Test 76 did shorten the path to the capture. But two earlier
timing changes of that kind left the rate untouched.
- **H3 (QUIT, APART, the guard, HOT, COPY): refuted for each candidate.** None
reaches the code that frames blocks, and the counters that would show their
faults are clean.

The strongest evidence already in the report is `W UNFIX`. It doubled with LF
(22 to 48), as expected if all 641 are the old "one position too many" event
and not if the extra ones were stale gap bytes.

The delivered no-GAP module is the right single experiment. The second module
cannot separate H1 from H2. GAP showed no measurable gain, so leaving it out
costs nothing either way.

I ran nothing on a console. The disassembly is of
`build/dev76/native-ra-aux.elf`, a development build, not the packaged module.

## Points

1. (confirmed, certain) The count rose for real, beyond what fewer overruns or
their grouping explain. Tests 74 and 75 (four runs) share one rate per closely
following loss; Test 76 has double.

   Measured: streams that could not be followed per hundred overruns in a run
   were 0.16 (Test 68), 0.08 (69), 0.34 (70), 0.65 (71), 0.93 (72), 1.32 (74),
   1.14 (74 second module), 1.94 (75), 1.81 (75 second), 3.22 (76). Derived
   from W BLK FIX UNFIX HOLED NOCRC and W CUT SEG: per loss that follows
   another inside one block, 3.61, 3.16, 3.63, 3.59 for the four Test 74 and
   75 runs and 7.12 for Test 76. Per block with several losses: 6.7, 5.7, 6.7,
   6.4 against 21.2 percent. So Test 75's rise over Test 74 was more grouped
   losses at an unchanged rate, as its document inferred for LATER. Test 76
   has about as many grouped losses as Test 74 (8,324 against 7,793) and twice
   the streams lost (641 against about 306). Script:
   <scratch>/agent-lf/rates.py

2. (corrected, certain) The task text's summary of Test 71's finding 5 is the
wrong way round: the known anomaly is not a held byte stored twice.

   docs/toy-wire-stream-test-71.md finding 5: all 201
   unexpected bytes lay directly behind a block's end, only 2 of those blocks
   had any byte stored from the receiver's register, none repeated the byte
   before it (report row F END HELD TWICE: C9 2 0). Test 69 finding 4 and Test
   70 finding 4: every case followed a block handed on with lost bytes, and
   the noted bytes are that block's own last CRC byte followed by one idle
   byte and the token. So the block has one position too many, and one of its
   listed lost positions was taken by the channel and never lost. The hardware
   cause was left open. The counters behind that row are compiled only without
   KUI_TOY_WIRE_PACE (toy_wire_stream.c lines 885 to 943 and 348 to 354), so
   no report since Test 72 can say which kind its cases are.

3. (refuted, likely) H1: GAP lets the token search read a stale byte.

   Code as written (toy_wire_stream.c 952 to 1049, 628 to 635, 739 to 744):
   'landed' is taken once per call, behind the uncached read. The search reads
   only positions below it. 'fresh' starts unset in every call, so the first
   read of every line follows a drop. A hole skips the read and leaves 'fresh'
   alone. Across the wrap the line index changes from 1023 to 0. The block
   loop drops every line of a word run again before reading and takes single
   bytes through the uncached address. wire_account's uncached store of a held
   byte happens only in kui_wire_service, never inside kui_wire_next. Visits
   run with interrupts and exceptions blocked (gate mask 0x100000f0,
   native_readahead_aux_entry.S lines 44 to 47 and 191), so nothing nests. No
   PREF exists anywhere in the reader; MOVCA.L only writes the window. Linked
   code (my disassembly of build/dev76/native-ra-aux.elf): 8c425d90 'ocbi @r2'
   with r2 = view + (at & 0x7fe0), then 8c425d92 'mov.b @(r0,r13),r11'; the
   register shared by 'fresh' and the byte is restored at 8c425dae on every
   continue path; kui_wire_fresh (8c425c34) drops all lines and returns before
   any load. Hardware rules as documented: OCBI on a line not in the cache
   does nothing and the read then misses and fills from memory. Two ring lines
   16 KiB apart share a cache place but differ in address bit 14, which is in
   the tag, so a drop of one never touches the other and its read evicts the
   other, which is never dirty. Console: with GAP the token's line is often
   fetched while the block's first bytes are still arriving, and the block
   loop's drop of that line is all that keeps such blocks from failing their
   CRC16; one clean block of 135,712 failed (S BC 1).

4. (new, possible) One hardware assumption under GAP is not covered by RING's
console record: the invalidate and the load are adjacent instructions. Even if
it failed, it could explain only part of the rise.

   Linked code: 8c425d90 ocbi, 8c425d92 load from the same line. RING never
   does this (kui_wire_fresh returns before the word loop loads). Whether the
   SH7091 applies an invalidate before the very next instruction's cache
   lookup I could not verify. The copy routine's MOVCA.L followed at once by a
   store to the same line (8c426c1a, 8c426c1c) works about 2 million times a
   run, which shows an address-array update reaches the next store, not the
   next load. Bound, by inference: Test 69's two notes show the gap is one
   idle byte and the token, so the search reads about two bytes a block. A
   cached copy older than a gap byte exists only when reception stopped with
   its next position inside the gap (26,835 run ends times 2 in 516, about 104
   a run) or a search call ended between the idle byte and the token (about
   39,000 visits in 516, about 75). The copy must also survive in a 16 KiB
   cache until the next visit. With the wire running, the block loop's copy of
   the last line is made about 20 microseconds after the block was seen
   complete and already holds both gap bytes. So at most about 180 a run and
   realistically far fewer, against an excess of about 330 over the Test 74
   level.

5. (new, likely) The report's own W UNFIX value says the extra streams lost
are the old kind of event, not stale gap bytes.

   Code: a block with exactly one listed lost byte that the CRC16 cannot
   explain is counted in 'unsolved' and handed on (toy_wire_stream.c 1060 to
   1074). A block with one position too many and a single listed loss ends
   there 255 times in 256, and the next search then meets its last CRC byte. A
   stale gap byte ends the stream with no such block. Measured: UNFIX 25 and
   14 (Test 74, two modules), 22 and 18 (Test 75), 48 (Test 76). Inference: in
   the four earlier runs UNFIX is 0.115 of (streams lost times the share of
   losses that are alone in their block), from 79 events. That predicts 42 for
   Test 76 if all 641 are the old event, 25 if only Test 75's rate per
   overrun's worth (386) are, 17 at Test 74's rate. Seeing 48 or more has
   probability 0.21, 5e-5 and 1e-9. This rules out more than about 250
   unrelated cases; it cannot rule out 100.

6. (not-determinable, likely) H2: it is the known anomaly, and its rate per
overrun rose because wire_capture runs sooner after the overrun.

   Which code decides the timing. Outside a visit: SCI error interrupt, SDK
   table, resident callback, the gate (two uncached accesses of the context's
   busy word), dispatcher, toy_ahead_plain, toy_ahead_pump, toy_wire_drain,
   toy_wire_serve, kui_wire_service, wire_stopped, wire_capture. Inside a
   visit: the poll at the top of the drain loop once a block
   (toy_wire_ahead.inc 561 to 563) and once behind it (622). In wire_capture
   SCR is cleared before the first uncached read (toy_wire_stream.c 309 to
   310), so a channel request still pending at that instant is withdrawn, not
   completed. What Test 76 changed: HOT turned the window-state reads on both
   paths into cache hits (toy_ahead_plain, the pump's opening, toy_wire_room,
   the cursor checks after feed), and feed fell from 17.9 to 8.4 microseconds.
   From the end of a block's feed to capture's first register write there is
   now no bus cycle; the gate's two and the dispatcher's uncached context
   accesses remain on the interrupt path. LATER off does not change the path
   to the first answer, and QUIT acts only in kui_wire_stop. For: the rate per
   grouped loss doubled, and it has climbed in most builds that shortened the
   reader since Test 68. Against: RING on or off (block loop 33 against 49
   microseconds) left it at 3.63 against 3.59; losses answered by interrupt
   visits (Test 75) or by in-visit polls (Test 74) gave 3.63 against 3.61; and
   in Test 71 the cases were not the held-byte kind an early capture would
   produce. Missing: the F END HELD TWICE counters, the R row (compiled only
   in Test 74), restarts by piece (not counted since Test 73), and a variant
   that moves the capture's timing without changing how the movie plays.

7. (refuted, likely) H3: QUIT's leftover state, APART's buffers or tables, the
stricter 'ours' guard, HOT or COPY cause the extra lost streams.

   The count is raised only in kui_wire_next's search, which uses ring bytes,
   the lost-position log and 'landed'. QUIT: the stopped stream is discarded.
   Behind it come wire_capture, a module reset whose register values are
   checked, restored channel registers, a whole CRC-checked block read by the
   low reader (W SEEK 711 for 658 starts), and kui_wire_start's own checks of
   SCR, SMR, BRR, SSR, CS, CHCR1 and DMAOR; a failure of any would show in W
   ERR DET, which read 0 0. P HALT gives 11 microseconds a stop. The card is
   left at an arbitrary byte with or without QUIT. Guard: it only decides
   whether the enable bit is cleared; capture's own guard is unchanged and a
   foreign channel would be a fault. APART: the block loop writes at most 512
   bytes into either buffer; overwritten tables would fail clean blocks' CRC16
   in bulk (BC 1), break the single-byte rebuilds (14,821 succeeded, 11,630
   confirmed by the P code word, none refused), and the layout is held by
   static assertions (toy_wire_ahead.inc 123 to 148). HOT and COPY touch the
   window only; wrong bytes there would show as K LB or W BAD (0 and 12, as
   before).

8. (new, possible) Test 76's level already existed in Tests 71 and 72 wherever
the movie played unstarved, in builds with no cached ring reads at all.

   Measured from the TX and TD rows and each document's restart count. Test
   71: 208 restarts, 120 during the second movie, so 88 in the first movie's
   3,403 sectors: 26 per thousand sectors, 8.0 a second. Test 72: 106 in
   3,398: 31 per thousand. Piece 10 of the second movie (608 sectors, every
   frame shown): 20 and 19, 33 and 31 per thousand. The starved dense pieces 1
   to 9: 79 in 21,648 and 103 in 21,664, 3.6 and 4.8 per thousand. Test 76
   overall: 654 in 29,245, 22 per thousand, 8.5 a second. Inference: within
   one build the rate varies sevenfold with what the game is doing, so the
   reader's code alone does not set it; Test 76 is the first run in which the
   whole second movie plays like the first. Caveats: the Test 75 pair showed
   the same rate at 874 and 1,407 frames; piece 11 (also unstarved) had only 8
   per thousand; Test 76 has no count by piece or by movie.

9. (confirmed, likely) The author's first proposal is right: Test 76 without
GAP is the cheapest run that separates H1 from H2.

   It is one switch, already built and delivered
   (docs/evidence/toy-wire-stream-76/variant-without-gap.log, ZIP of
   2026-11-15 09:45 CST). GAP reads only about two bytes a block and the ring
   loop did not move with it (33.7 against 33.1 microseconds a block), so
   playback should stay as smooth and the comparison is clean. Reading: with I
   SUBMIT near 796 hex, S LF between about 560 and 720 with W UNFIX between
   about 35 and 60 means GAP is not the cause. LF at or below about 450
   implicates GAP; since the code has no stale path, the adjacent invalidate
   and load would be the suspect. If LF falls while UNFIX stays near 48, the
   removed cases were not the old event, which is what stale gap bytes would
   look like. Anything between calls for a repeat of both modules. Run-to-run
   spread is known only from two same-day pairs (306 and 276; 462 and 441).

10. (corrected, likely) The second module (Test 75 with LATER off) cannot
separate H1 from H2, contrary to the proposal.

   It lacks QUIT, HOT, COPY, GAP and APART together, and its reader is Test
   75's (86 microseconds a block), so the second movie will be starved again.
   Under every hypothesis it should show fewer streams lost than Test 76:
   about 300 if the Test 74 and 75 rate of 3.6 per hundred grouped losses
   holds with Test 74-like grouping, somewhat more if the count follows frames
   shown. A lower count there says nothing about GAP. Its value is the check
   of Test 76 finding 5 (LATER's cost) and one more point for the rate per
   grouped loss.

11. (new, likely) GAP has no measured benefit, because the card's gap is two
bytes, not dozens.

   Measured: taking a block out of the ring cost 33.7 microseconds in Test 76
   against 33.1 in Test 75 (Test 76 document, finding 3).
   docs/toy-wire-stream-test-69.md finding 5: one idle
   byte before the token in both noted cases. The comment at toy_wire_stream.c
   line 959 ('a few dozen of them to a block') and the estimate of 1 to 4
   microseconds assumed a longer gap. With GAP one line fetch replaces two bus
   reads. W BYTES by W BLK is 531 a block, 15 above the 516 a two-byte gap
   gives; I attribute that to bytes received again after restarts, which I
   could not separate.

12. (not-determinable, possible) Whether the restarts contributed to the
owner's hiccup in piece 7 cannot be read from this report.

   Measured: each stream begun costs the low reader 2.0 ms (D SEEK F74FF for
   658 starts); 654 restarts in 76.6 s is 8.5 a second. Piece 7's reader time
   is 0.73 s against 0.59 to 0.65 s in pieces 1 to 6 for the same sectors, its
   wire standstill 0.08 s against 0.02 to 0.06, and the player's buffer fell
   to 28 KiB there. Inference: reader time minus blocks times the average 55.9
   microseconds leaves 0.12 to 0.18 s in pieces 7 to 9 and 0.05 s or less in
   pieces 0 to 6, which would fit some 60 to 90 restarts a piece there, or
   equally more parity and EDC work. Missing: restarts by piece.

## Recommendations

- Have the owner run the already delivered Test 76 no-GAP module once, same
procedure, and read I SUBMIT, S LF, W UNFIX, W CUT, SEG, FIX, HOLED and NOCRC
together. LF near 640 with UNFIX near 48 clears GAP; LF at or below about 450
implicates it; in between, repeat both modules.
- Do not use the second module (Test 75 with LATER off) to judge GAP. If it is
run, compare its streams lost per grouped loss with 3.6 per hundred and read
it together with frames shown.
- GAP can be left out of the next build at no cost whatever the no-GAP run
shows: it gained nothing measurable. If it is kept, drop the search's line
before the loop, as kui_wire_fresh does, so that no load directly follows the
invalidate.
- Make the next console build measure before it changes behaviour: restore the
counters that classify these cases (F END HELD TWICE, the R row of runs cut
again within 8, 32 and 128 bytes, restarts by piece) and add a split of
restarts by movie. The first three exist in the source behind the switches of
Tests 71, 72 and 74. The no-GAP auxiliary has 64 bytes free, so something has
to give way.
- If those counters show the cases directly behind a block's end again, the
remedy is to follow the stream past the position that was never lost, as Tests
71, 75 and 76 already list. About 593 of the 641 lie in blocks with several
listed positions (four on average), so the CRC16 alone cannot say which one to
remove; the sector's parity and EDC would have to.
- Only if the counters point at the capture's timing (held or twice cases),
try one change by itself: an uncached read before SCR is cleared in
wire_capture, so that a channel transfer still pending completes instead of
being withdrawn.

## Not verified

- Whether the SH7091 applies an OCBI before the cache lookup of the load in
the very next instruction. I know of no rule either way and did not look one
up.
- That the delivered module's code equals what I disassembled.
build/dev76/native-ra-aux.elf is a development build; I did not compare it
with the packaged retail-toy-test-76.kui.
- The hardware cause of the anomaly itself (an overrun flagged with nothing
lost, or one byte more at the resume). The project's documents leave it open
and I could not derive it.
- What stalls the bus, DMAOR's channel priority on this title, and whether an
uncached read always lets channel 1's pending transfer land first while the
game's own DMA runs. kui_wire_start checks only DMAOR's low three bits.
- How Test 76's restarts divide between the two movies and among the pieces of
the second. The estimate from reader time by piece is rough and confounded
with parity and EDC work.
- That the gap is two bytes for every block. It rests on Test 69's two noted
cases and on GAP showing no gain.
- The figure of about 310 for Test 74 and its split into sector and frame
stops; that report has no S row.
- Nothing was run on a console and no host or linked test was executed by me.
Numbers are from the saved reports and my arithmetic on them.
- The upper bound of about 180 stale reads for the adjacency case is my own
estimate from run ends, visits and a two-byte gap, not a measurement.
