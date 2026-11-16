# Test 76 console report: an independent check of the reading

An independent analyst was given the author's reading of the Test 76
console report (report 66-10) as it had been told to the owner, and
asked to recompute everything from the report and from the code that
writes it and to say where the reading is wrong or goes beyond the data.
What follows is its answer as returned, unedited but for line wrapping
and for paths. The author's corrected reading is in
`docs/toy-wire-stream-test-76.md`, Console result, findings 7 and 8.

## Summary

The reading's numbers are right. Two of its conclusions go beyond what the
report shows. I recomputed every figure from 66-10 and the three earlier
reports with a script, and checked each per-piece row against the code that
writes it. Points 1, 3, 6 and 7 hold as stated.

Three corrections:

1. **The loss is more than the one gap.** The 0.47 s gap is 14.0 frame periods
and covers 13 missing frames. Piece 7 lacks about 24.5 and 26 were skipped. So
about 11 more frame-times passed without a new frame in the 3.5 s after it, in
at most six further breaks. The report keeps only the longest break, not the
frame that ended it.
2. **TB does not single out piece 7.** Piece 9 has the same C2, and pieces 1,
6 and 8 have C1, all without loss. TB is the emptiest level seen in a piece at
calls that took a frame, so it cannot be placed at the gap.
3. **"At most one read of 16 sectors per frame" rests on one equality** (TD
piece 8 = TF piece 8 = A1). The run's totals are 12.1 sectors a request at
about one request a frame.

On the cause:

- **"Not late data from the card" is supported.** Requests whose wait ended in
piece 7 waited under 21 ms in all, and the reader's time and its seeks there
are ordinary.
- **"An underrun of the player's own buffer" is not determinable from this
report.** A player short of processor time fits the same rows at least as
well. Its frame call takes 30.5 ms of a 33.2 ms frame-time on average, and the
same piece lost 68 frames in Test 75 with no request waiting.
- **Piece 9 argues against the proposed trigger.** It had an equally empty
buffer and 0.44 s of waiting requests, and lost no frame.

What would separate the two readings is whether the player's next chunk was
missing during the gap. This build's report does not record that.

My script is at
<scratch>/agent-verify/recompute.py.
Nothing in the repository was touched.

## Points

1. (confirmed, certain) 1. Second movie showed 0x796 = 1,942 of 1,968 frames
(Test 75: 1,407); first movie all 331; 26 skipped.

   I SUBMIT 796 = 1,942; LAST 7B0 = 1,968; SKIP 1A = 26; EXIT 7DC. The TF
   pieces add up to 1,942. B SUBMIT 14B = 331 with 330 on time, and BP's SN is
   0. Test 75's I SUBMIT is 57F = 1,407.

2. (confirmed, certain) 2a. All the loss is in piece 7 (0x89 = 137 frames,
37.6 to 42.9 s); every other piece is complete. Longest gap 0x58CC7 ticks =
0.47 s after frame 1,147; first skip at frame 1,162.

   TF is 136, 163, 162, 161, 162, 162, 160, 137, 161, 162, 161, 215. Frames
   submitted plus the 26 skipped reach 1,592 by the end of piece 9, the same
   position Test 75 reaches there (1,031 + 561), and both tails are 161 + 215.
   The count at the end of piece 7 is 26 short of schedule, so all 26 skips
   fall in piece 7 to within one frame. The gap is 363,719 ticks = 0.4656 s.
   AT is the last frame before the gap (toy_movie_present_trace.inc:84-87).
   Frame 1,147 is the 41st of piece 7, about 1.3 s into it, so the gap lies
   wholly inside the piece. SF is the player's frame count plus one at the
   return of the first update call whose skip counter moved
   (toy_wire_ahead.inc:738). One refinement: a full piece is 161.5 frames by
   this clock, not 161 (see the new point on the frame period), so piece 7
   lacks 24.5.

3. (corrected, likely) 2b. Reading: one stall of 0.47 s in which the player
neither showed nor skipped, then 26 frames skipped over the next seconds to
catch up.

   The gap covers 13 missing frames. Piece 7 lacks about 24.5. After the gap
   96 frames came in about 106.5 frame-times, so about 11 more are missing in
   the 3.5 s that followed. CADENCE is 78E = 1,934 of 1,941 follow-on frames,
   so at most seven breaks of over 50 ms or out of sequence in the whole
   movie; a run from the disc has one (Test 67: 1,966 of 1,967). The report
   keeps only the longest break. Whether the others are further stalls or the
   time the catch-up cost is not recorded. Whether the player skipped during
   the 0.47 s is not recorded either: neither the frame that ended the gap nor
   the skip counter across it is printed. SF = AT + 15 with a gap of 14.0
   frame periods fits the gap being the first run of skipped frames at least
   as well as a wait. One thing is measured: IP's two skip-only calls took 7.9
   ms together, so the 0.47 s was not such a call. It was spent in calls that
   took no frame, or inside a call that also put a frame out.

4. (confirmed, certain) 3. At the stall no read request was kept waiting (TW 0
for pieces 4 to 7); the wire stood still 84 ms in piece 7 (TP 4); reader's
time 0x23 steps = 0.73 s in piece 7 against 0.82 s in piece 8.

   TW is 3, 0, 0, 0, 0, 0, 0, 0, 1, 21, 0, 3, so pieces 1 to 7 are all zero.
   Zero means under 16,384 ticks (21 ms) in total, not none. A wait is timed
   from the first visit at which the window could not publish a pending
   request (toy_movie_ahead.inc:546) to its publication (:530, :563), and
   added whole to the piece in which it ends (toy_wire_ahead.inc:275-292). The
   row works in this run: piece 9 shows 21 steps. TP piece 7 is 4 steps, 84 to
   105 ms. TR is 35 steps (0.734 to 0.755 s) in piece 7 and 39 (0.818 to 0.839
   s) in piece 8. TR is pump time only (toy_suite_report_save.inc:139-141),
   without the copy out or the dispatcher's path. One addition: the reader's
   time per sector served in piece 7 is about 14 percent above pieces 1 to 6
   (35/150 against a mean of 0.204), as in pieces 8 and 9.

5. (corrected, certain) 4. The player's buffer was at its emptiest there (TB
piece 7 = C2 of C9, about 28 KiB of 804 KiB left), nearly as empty in pieces
1, 6, 8 and 9.

   TB is 137, 193, 179, 174, 163, 164, 193, 194, 193, 194, 17, 70 and CAP is
   C9120 = 823,584 bytes = 201 steps. C2 means 24.3 to 28.3 KiB left. Piece 9
   has the same C2 as piece 7, so piece 7 is not the emptiest alone. Pieces 1,
   6 and 8 have C1 (28.3 to 32.3 KiB). Four of those five pieces lost no
   frame. TB is the largest free space seen in the piece, sampled only at the
   return of an update call in which the output or skip counter moved
   (toy_wire_ahead.inc:753-781). It cannot be placed at frame 1,147, and a
   wait for data is never sampled. The floor does mean empty: Test 73 sampled
   every call and never saw less than 28 to 36 KiB, and at its first wait the
   buffer held 34,741 bytes with the next chunk incomplete.

6. (not-determinable, likely) 5a. Inference: the hiccup is an underrun of the
player's own buffer, not late data from the card.

   The second half is supported: TW piece 7 under 21 ms, PIPE I 2 and AHEAD
   STOP 0 as in Test 75, TP small, TR ordinary. The first half is not shown.
   TB is equally empty in four pieces that played completely. In Test 73,
   which the document read as a player with no idle time, TB showed the buffer
   empty in eight of ten dense pieces, so an empty buffer accompanies
   processor shortage too. The rows that would decide are absent: chunk waits
   (IC and WF, last printed in Test 74), the frame call's time by piece, and
   requests by piece.

7. (not-determinable, likely) 5b. The player takes in at most one read of 16
sectors per frame (TD piece 8 = A1 = one per frame; TD nearly the same in
Tests 75 and 76), and the dense stretch uses about that much.

   About one request a frame is supported: AHEAD HIT 96F = 2,415 requests for
   2,299 frames of both movies, and Tests 68 and 73 say the same. A 16-sector
   size is not: 29,245 sectors in 2,415 requests is 12.1 each. Per frame-time
   the dense pieces took 14.6 to 15.2 sectors (pieces 1 to 7), 16.0 (piece 8)
   and 13.8 (piece 9). So only piece 8 is at 16, and pieces 1 to 7 stayed
   below it although TB shows the buffer far from full at some point in each.
   No piece in any report exceeds 161, so a ceiling is not contradicted
   either. TD being alike in Tests 75 and 76 shows only that the player reads
   every frame's data on its clock, shown or not (Test 73 finding 5, Test 75
   finding 8). The repository's model of the game is 13 sectors once a frame
   (tests/test_toy_wire_integration.c:25, 689-725). The window serves requests
   up to 60 sectors (toy_movie_ahead.inc:20). From the disc the movie shows
   all 1,968 frames (Tests 63 and 67), so a ceiling of the player's own cannot
   be the whole cause.

8. (not-determinable, likely) 5c. One late pass or one large frame empties the
buffer; the 0.47 s could be the player refilling to a threshold before it
resumes.

   No row shows a threshold, and I found none in the code or documents. TD
   piece 7 is 150 units against 147 to 149 for the same piece in Tests 73 to
   75, so no extra intake of refill size is visible. Piece 9 of this run had
   the named trigger without a hiccup: TB C2, requests waited 0.44 to 0.46 s,
   the wire stood still 0.52 s, and all 162 frames were shown.

9. (confirmed, certain) 6. Reader's cost per card block fell from 86.3 to 65.5
microseconds; RING 33.7 (33.1), FEED 14.5 (25.0), CHK 6.1 (7.1), SVC 3.4
(3.6), OUT 1.21 s, HALT 0x1610 ticks for 658 streams (11 microseconds each);
frame call OT/ON 30.5 ms (40.5 ms).

   V ticks add to 6,940,117 over 135,712 blocks: 51.14 ticks = 65.46 us. Test
   75: 9,129,818 over 135,381 = 86.32 us. RING 33.68 against 33.06, FEED 14.53
   against 25.00, CHK 6.12 against 7.08, SVC 3.39 against 3.60. OUT is 946,832
   ticks = 1.212 s. HALT is 5,648 ticks = 10.99 us a stream. OT/ON is 23,821
   ticks = 30.49 ms; Test 75 gives 40.49 ms. Two additions: the rest of the
   pumps fell from 17.6 to 4.3 us a block, and SEEK rose from 7.1 to 9.6 us
   because 658 streams were begun. "No gain from GAP and APART" is the
   arithmetic of RING per block. Per wire byte RING rose 7 percent (72.1
   against 75.8 million bytes). Which part did what needs the one-switch run.

10. (confirmed, certain) 7. Streams begun 658 (Test 75: 478, Test 74: 365);
640 ended on a byte between blocks that is neither idle nor a token; per
overrun 3.2% against 1.9% (Test 75) and about 1.3% (Test 74).

   W START 292 = 658. The S row is 4, 13 and 641, which add to 658; LF is 280
   = 640, AM 0, BC 1. 640 of 19,913 overruns is 3.21 percent; Test 75's 460 of
   23,825 is 1.93 percent. Test 74 printed no LF. Its 1.3 percent is the 310
   streams begun beyond its 55 windows over 23,226 overruns, an upper bound
   from another count. Timer waits fell from 10,930 to 8,368 but their time
   rose from 4.27 to 5.07 s. By TP, 3.2 of the second movie's 3.6 s fall in
   pieces 9 to 11, where the movie is light.

11. (confirmed, likely) The per-piece rows mean what the reading takes them to
mean.

   Read in the code. A piece is (start tick - now) >> 22, the last collects
   the tail, and it is none unless the second movie's player is the owner
   (toy_wire_ahead.inc:233-237). TF counts at each scene submission. TD adds a
   request's sector count when the window publishes it, printed in sixteens.
   TW is as under point 3. TB is as under point 4. TP is the stream's wait
   ticks gained since the previous pump visit, placed in that visit's piece
   (toy_wire_ahead.inc:258-263); the count itself grows when a wait ends
   (toy_wire_stream.c:391). TR is interrupt plus game pump ticks, placed in
   the piece where the visit ends. Every value is shifted, cut to whole steps
   and capped at FF (toy_suite_report_save.inc:58-67). One detail not in the
   README: requests in a movie file's first 386 sectors do not go through the
   window (native_readahead_aux.c:526-535), so TD and AHEAD leave the opening
   fill out.

12. (new, likely) The movie's frame period by this clock is about 25,962
ticks, not 0x65BA = 26,042.

   Pieces 1 to 6 and 8 to 10 hold 1,454 frames in nine pieces: 161.56 a piece,
   30.09 frames a second by the nominal tick. Test 67's disc run agrees:
   51,096,650 ticks for 1,968 frames. So a full piece is 161.5 frames, and the
   longest gap is 14.01 frame periods. The gap being a whole number of periods
   means the frame that ended it came on the player's regular frame grid.

13. (new, possible) A player short of processor time around 39 to 43 s fits
the rows at least as well as a buffer underrun.

   IP gives 30.49 ms inside each call that put a frame out, against a
   frame-time of 33.2 ms, averaged over the whole movie with its light last 12
   s. About 6.2 s of the 65.4 s between first and last frame lie outside calls
   that took a frame: 3.2 ms a frame-time, where the first movie has 10.6 ms.
   Test 75 lost 68 frames in the same piece 7 (93 of 161) and 107 in piece 6,
   with 0 and 2 steps of waiting, and its document read that as processor
   shortage. Frames shown have followed the reader's pump time through four
   tests: 15.1 s gave 513, 12.9 s gave 967, 11.7 s gave 1,407, 8.9 s gives
   1,942. This is inference: the report has no player time by piece.

14. (new, likely) Other explanations: late data, a long call in the reader, a
stream restart or sector re-read at that moment, a long call in the game's own
disc path.

   Late data, a restart or a re-read that cost supply: excluded by TW piece 7
   under 21 ms. A single long reader visit: excluded by TR. 0.47 s would be 22
   of piece 7's 35 steps, leaving 13 where the neighbours need 28 to 39. SEEK
   is 1.97 ms a stream begun against 2.00 in Test 75, so there is no
   half-second seek. HALT is 11 us a stream. Restarts by piece are not printed
   (S is a run total: 658, about 8.6 a second). A long call inside the game's
   or the firmware's own disc code: not visible. D is a run total over both
   movies and the loading around them (19BA = 6,586 calls, 9.44 s, 1.43 ms
   each). A 0.47 s call would be 5 percent of it. The G N T pair that timed
   the game's disc calls whole was dropped in this build.

15. (new, certain) What this report cannot show about the hiccup.

   The frame that ended the longest gap, and the skip counter across it. The
   size and place of the other breaks. The buffer's content during the gap.
   Whether the next chunk was missing. The frame call's time in piece 7 (IP is
   a whole-movie total). The number and size of requests by piece (HIT is one
   total for both movies). Restarts by piece. Anything inside the game's own
   disc path. The report file has 33 bytes free before the end marker; a
   three-number course row needs 30.

## Recommendations

- Tell the owner what is measured and leave the cause open. Near 39 to 43 s
the picture went without a new frame for 0.47 s and then for about 0.35 s more
in a few shorter breaks, 24 to 26 frames in all. The card's data was on time
there. Whether the player ran out of data in its own buffer or out of
processor time is not measured yet.
- Smallest measurement that settles it: one more per-piece row from the hook
the game already calls (kui_toy_movie_chunk_observe in
src/loader/native_readahead_aux.c:1132, body compiled out
since Test 75). Record the ticks, or the calls, in which the player's next
chunk was not in its buffer. If piece 7 shows about 0.5 s and pieces 1, 6, 8,
9 show little, it is an underrun and the next question is why no data was
asked for. If piece 7 shows nothing, the player was short of time. The row
fits the 33 free bytes.
- If a row can be given up for it, also latch at the longest gap: the number
of the frame that ended it, the skip counter's gain across it, how many update
calls returned inside it with their ticks, and the least the buffer held. Many
short calls with no skip gain means waiting for data. One or two long calls
means busy or blocked inside a call. A skip gain of about 13 means the gap is
the first run of dropped frames.
- In the Test 76 document: give a full piece as 161.5 frames with the measured
frame period. Say TB piece 9 equals piece 7. Say the 0.47 s gap covers 13 of
about 24.5 missing frames. Record "one 16-sector read per frame" and the
refill threshold as conjecture, not finding. Set the processor-time reading
beside the underrun reading, with piece 9 and Test 75's piece 7 as the
evidence against deciding now.
- Do not build a change on the buffer reading before that measurement. One
measurement-only build keeps the next console run comparable with this one.

## Not verified

- Whether the player has a 16-sector limit on a read, or any refill threshold.
I did not read the game's refill routine (0x8c0989d4) and found no record of
request sizes in the documents or evidence.
- Which frame followed the longest gap, and whether frames were skipped during
it. The report does not print it.
- Whether the player's buffer also runs low when the movie plays from the
disc. No disc run sampled it, so the argument that a ceiling of the player's
own cannot be the whole cause is an inference from the disc runs showing every
frame.
- That the first 386 sectors of each movie bypass the window in this build. I
read it in native_readahead_aux.c:521-535 and did not trace every build switch
of mode 66.
- What D's 9.44 s consists of. I take it to include the opening fills and
reads outside the movies, from the code, not from a measurement.
- Why streams ended on a stray byte more often (3.2 percent of overruns). I
looked at GAP's cache reads in toy_wire_stream.c:956-993 only briefly and saw
no fault; that is not an analysis.
- The cause of the 0.3 percent difference between the movie's frame period and
26,042 ticks (the timer's real rate or the player's clock).
- No host test, build or make target was run, and the module binary was not
inspected. BUILD 7fe7e83c2082 and PT FD are read from the report text only.
- src/loader/toy_movie_exit_hook.inc has a start-time policy that gave the
opening movie's player a larger buffer in suite modes 54 and 56 only. I did
not look up what those tests found. It would be a direct trial of the underrun
reading.
