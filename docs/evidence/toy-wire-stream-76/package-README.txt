K-UI Toy Commander Test 76 - what the clocks in Test 75 pointed at

Test 75 on the console, from your two reports:

- The second movie showed 1,407 of its 1,968 frames (Test 74: 967). More
  frames shown than skipped, as you saw. The pauses are a quarter as
  long: 34 times the player skipped ahead without showing anything, a
  tenth of a second each (Test 74: 36 times, 0.44 seconds each).
- The second movie's start is clean again: two windows opened where
  Test 74 opened 45, first frame after 0.8 seconds, first skip at frame
  37 instead of 10.
- Reading arrived data through the processor's cache was the gain. The
  two modules differ in nothing else: 33 against 49 millionths of a
  second for each card block, 1,407 frames against 874.
- "Interrupts only keep the wire going" (change 2 of Test 75) did not
  help, and it made the card's stream worse. With it the stream lost
  its bytes closer together, twice as many sectors had to be repaired,
  and a third more streams had to be begun again. It is switched off in
  this build.
- What remains is what you describe and what the other reading of the
  reports says: stutter every few seconds through the first 54 seconds,
  two bad stretches (around 32 to 38 seconds and 49 to 54 seconds), and
  one pause of 1.9 seconds near second 49.

I agree with that reading: it is two different problems.

1. Through most of the movie the data is there in time and the player
   still falls behind. It is short of processor time, and my reader is
   what takes it: 15 percent of the processor through the dense part.
   By the two runs' own arithmetic the player needs about a fifth to a
   quarter of that back to show every frame.
2. Between seconds 43 and 48 the reader does not bring the data fast
   enough: requests waited 1.3 seconds there, and about that much in
   every build since Test 71 however fast the reader was. The 1.9
   second pause follows straight after. Test 67 measured that the
   movie's densest second needs more than the serial wire can carry at
   all, so there the player has to live from what it has buffered.

Test 76 goes after the first with everything the clocks in Test 75's
report pointed at, and helps the second where it can. Six changes. As
before each is its own source commit and its own build switch.

1. Last time's change 2 is off. An interrupt from the card's stream
   reads what has arrived again, as in Test 74.
2. A stream that has to be given up is stopped at once. Until now the
   reader stopped one by waiting, with everything else held off, until
   the card's current run ended by itself: up to 5 thousandths of a
   second, about 480 times in your last run. No clock in the report
   covered that wait; a second reader of my analysis found it in the
   code. Now the reader ends the run itself, in two byte times.
3. The reader's own bookkeeping for each block is kept where the
   processor's cache can hold it. It lay in a part of memory the cache
   is not used for: about fifty slow memory accesses for every card
   block.
4. A block is copied into the reader's window by a hand-written copy,
   32 bytes at a time, that does not first fetch the memory it is about
   to overwrite.
5. The search for the start of each block goes through the cache too,
   as the block's data has since Test 75.
6. Two pieces of the reader's working memory no longer share the same
   places in the processor's cache. The cache has one place for any
   given address, and the buffer each block passes through sat on the
   places the reader's own stack uses.

And the report says three new things: the time spent stopping streams,
and for each 5 seconds of the second movie how empty the player's own
buffer got and how long the wire stood still.

What I expect, and how sure I am. Measured: the reader spent 86
millionths of a second on each card block in Test 75. My estimates for
the six changes add up to between 16 and 36 of those, a fifth to two
fifths of the reader's work. The player needs about a fifth to a
quarter. So the stutter through the first 43 seconds should be much
less, and may be gone. I cannot promise it: three of my last four
estimates of this kind were too hopeful (one was too cautious), and
changes 3 to 6 rest on how the processor's cache is documented to
behave, which my tests can model and not run.

For the stretch near second 48 I expect an improvement and not a cure.
Changes 1 and 2 give the wire back time it lost to restarts, and a
player that is not already behind when it gets there has its buffer to
live from. But nothing in this build makes the wire carry more than it
can. If that pause is still there, the new rows TB and TP will say
whether the player's buffer ran empty, and that decides what to do
about it.

Before sending, five independent reviewers each read one of the changes
for ways it could fail. None found a way for wrong data to reach the
game. What they did find, all fixed in this build:
- Change 2 took the stream's channel off without first checking that it
  was still the stream's. Nothing on your console is known to take that
  channel, but every other part of the reader checks, and now this does.
- Change 6, as I first built it, moved two of the reader's tables onto
  the very places in the cache it was clearing. Time only, no wrong
  data. The layout is redone, and the build now refuses to compile if
  it ever shifts again.
- Change 4: nothing wrong in the copy. Its test could not have seen one
  kind of mistake (two saved values put back in each other's place).
  Now it can.
- The report: the player's buffer is not the megabyte I took it for but
  804 KiB, as Test 73 measured. The report now prints the size it finds.
- Changes 3 and 5: nothing.
Before any of it was built, three other readers checked my reading of
your reports and my plan. They found the wait that change 2 removes,
and they talked me out of one change I had planned.

One more thing came up in the last full run of every check, after those
fixes: the card stream's own test failed in the older configurations.
The reader was right and the test was not. The test said that a block
passed on wrong always has its lost bytes listed. By design two rarer
kinds exist, both only where a byte is lost between two blocks and the
start of the next block is misread: a block whose own checksum lost a
byte, and a block whose one lost byte the checksum seemed to explain.
The first is always caught one step later, by the check on the whole
sector. The second must also pass a check on the sector's own parity
there, which a wrong block passes once in 256 times. What is left after
both is as rare as a wrong block passing its checksum in the first
place, and one such sector in sixteen is checked in full as a watch
(row K, LB). That is the design since Test 74. In your four reports
since then LB was 0 every time.

Two more readers then went over the corrected test. Again nothing wrong
in the reader. They found places where the test still shows less than
it says, and I am tightening those. None of this changes the module:
both modules in this package were built before it and are untouched.

Not in this build, on purpose: the faster copy for the data handed to
the game (it would leave the game's memory in the cache in a way the
old reader never does, for a gain I can no longer show to be more than
small), and recovering streams that today have to be begun again. With
change 2 a restart costs much less than it did.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
   Leave the ALTERNATE folder off the card for now.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 76 entry. It replaces the Test 75
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 76. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 76
   and BUILD 7fe7e83c2082. Please also say how each movie looked and
   sounded, and what happened around second 48 of the second one.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.

If the game hangs or resets, or a movie shows garbage, power off and go
to the second module below. Tell me what you saw; there will be no
report from that run.

THE SECOND MODULE (ALTERNATE folder)
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui is not Test 76. It is
Test 75's reader exactly as you ran it, with only last time's change 2
switched off: no other change of this build is in it. It is there for
two cases:
- the main module does something Test 75 did not do (a hang, a reset,
  garbage, movies that do not start, a first movie that no longer plays
  smoothly). Then run this one and send its report with a note. Every
  part of it has run on your console before.
- the main module runs normally and you have the time for a second run.
  Then its report says what switching change 2 off does by itself,
  which no report so far can, and what the other five changes add on
  top. Worth having, not required.
To use it, power off and merge the KUI folder inside ALTERNATE into the
card's KUI folder the same way.

The main module's report says TEST 76 and the second module's TEST 75,
both with BUILD 7fe7e83c2082. The last value of the PIPE row names the
changes a module has: FD for the main module, 5 for the second.

WHAT THE TEST ANSWERS
- How much smoother the second movie is. I SUBMIT: Test 75 submitted 57F
  (1,407) of its 7B0 frames. IP: its player skipped without showing in
  22 (34) calls (SN), and a call that showed a frame took 2A15FB6 / 573
  ticks, 40.5 ms (OT / ON). A frame has 65BA ticks (33 ms). TF: frames
  shown in each 5 seconds; A1 is all of them.
- Whether the pause near second 48 is still there. I SUBMIT's third and
  fourth values: Test 75's longest gap was 16B621 ticks (1.9 s) after
  frame 5A1 (1,441). TW pieces 8 and 9; TB and TP for the same pieces.
- What the reader costs now. V's three tick values added, by W BLK's
  first value: Test 75 was 43 (67) ticks, 86 millionths of a second,
  for each block. Row P says where it went.
- That stopping a stream no longer waits. P HALT by W START: some tens
  of ticks for each stream, where a wait would be thousands.
- Whether the stream is as it was in Test 74 again. W WAIT (Test 75:
  2AB2, Test 74: 199C), W BLK HOLED (1A0B, 10AE), W FILL (F4E, 785),
  W START (1DE, 16D).
- B SUBMIT: the first movie must stay at 14B.
- K MB and LB must stay 0, and AHEAD STOP 0.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ STAY PT
  Times the verified window opened for each movie; runs of the reader's
  routine from the game's calls and from the SCI interrupt; requests the
  game waited for that kept their window; and the changes this module
  has, added: 1 waited requests keep the window (Test 75), 2 interrupts
  leave the reading (Test 75; off here), 4 the ring through the cache
  (Test 75), 8 change 2 of this build, 10 change 3, 20 change 4, 40
  change 5, 80 change 6.
AHEAD B I HIT EDC STOP
  Sectors served to the game from the verified window for each movie, the
  requests that made up, sectors taken into the window (by either check;
  the label is the old one), and windows given up on error.
I SUBMIT CADENCE MAX AT    (and the same row for B)
  Frames the player submitted, how many of them on time and in sequence,
  the longest gap between two in ticks, and the frame it came after.
  From the disc: 14B and 7B0 submitted, all but one or two on time.
I FIRST LAST SKIP EXIT SF
  Ticks to the first frame, the last frame's number, frames skipped, the
  player's exit code (7DC is a normal end), and the frame at which the
  player first skipped, by its own count.
IP ON OT OC OI SN ST    (and BP for the first movie)
  The player's own calls, sorted by what the player's own counters say
  each call did. ON: calls in which it put a frame out. OT: ticks inside
  those calls. OC: ticks from the end of the call before to the end of
  each of those, the whole turn. OI: the reader's interrupt ticks in that
  span. SN: calls in which it skipped a frame and put none out. ST: ticks
  inside those.
K LIGHT EDC MEND MB LB ORD
  Sectors accepted on their blocks' checksums and address; sectors passed
  through the EDC; rebuilt bytes their sector's parity confirmed; rebuilt
  bytes it did not confirm (the EDC then decided); sectors that passed
  the lighter check and failed the EDC watch (must be 0). ORD is the
  serial port's answer on the bit order: 3 as before.
V SCI TMR GAME N TICKS
  Runs of the reader's routine from the SCI interrupt, from the timer and
  from the game's calls, then the ticks spent in each of the three.
P RING FEED CHK OUT SVC HALT
  The reader's ticks by what it was doing. RING: taking blocks out of the
  ring (finding the token, the bytes, the CRC16). FEED: handing them to
  the window (the copy and the sector's check). CHK: the sectors' checks
  alone, a part of FEED. OUT: copying requests out to the game. SVC: the
  stream's own service, a run that ended answered and the next begun.
  HALT: stopping streams (new).
D N T SEEK CAP
  The game's disc calls that took the reader's whole path and the ticks
  in them; the ticks the old reader spent bringing the card to a
  stream's start; and the size of the player's own buffer in bytes as
  this run found it (new; Test 73 found C9120). 0 means it was not
  found, and then TB says nothing.
TF, TD, TW, TB, TP, and at the end TR
  The second movie in twelve pieces of 5.4 seconds (400000 ticks) counted
  from its start. Each row has three numbers of up to eight digits: two
  digits a piece, four pieces a number, earliest first. A number is
  printed without leading zeros, so fill it up to eight digits on the left
  before reading it. A piece that reaches FF may stand for more.
  TF  frames the player submitted (A1, 161, is every frame)
  TD  sectors served to the game, in sixteens (84 is the movie's own pace)
  TW  time requests waited for data, in steps of 21 ms
  TB  the most of the player's own buffer that was free, in steps of
      4 KiB (new). The buffer holds 804 KiB (see CAP), so C9 is empty
      and 0 full. It is looked at when the player has taken a frame.
  TP  time the wire stood still for the stream's timer waits, in steps
      of 21 ms (new)
  TR  the reader's time, interrupt visits and the game's calls together,
      in steps of 21 ms (Test 75 printed the two apart as TI and TG)
W START SEG CUT WAIT FULL
  Streams begun, runs that reached their length, receive overruns inside a
  run, overruns answered with a timer wait, and stops for lack of room.
W BYTES WTICK TMR ERR DET
  Wire bytes received, ticks spent in timer waits, the timer channel used
  (1 or 2; 0 means none was free),
  and the first stream fault with its detail (0 0 if none).
W BLK FIX UNFIX HOLED NOCRC SEEK
  Blocks taken from the stream; of those: one lost byte rebuilt from the
  block checksum, one lost byte the checksum could not explain, blocks
  passed on with lost bytes for the sector to fill, blocks whose only loss
  was a checksum byte; and blocks the old reader read to open or reopen.
S C S F LF AM BC
  Why streams ended: the window closed; a sector failed its check; the
  reader could not follow the stream. Then that last kind by what it met:
  a byte between blocks that is neither idle nor a block's start, a
  block's start lost among lost bytes, a whole block failing its CRC16.
W FILL REF BAD
  Sectors completed from their parity, sectors the parity could not
  complete, and sectors that failed their check. The last two are read
  again.

Gone to make room: the game's disc calls timed whole (D's G N T), the
reasons windows were given up (X), and TI and TG as two rows.

The second module prints Test 75's rows, as described in that package.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 76, build 7fe7e83c2082, payload CRC32 8D0D12DA. Replaces the Test 75
  module bf6f1c703add at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

NOT PART OF THE KUI TREE
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui
  Test 75's reader with TOY_WIRE_LATER=0, build 7fe7e83c2082 (this source,
  built with Test 75's switches), payload CRC32 C3176B73. Copy it only
  in the cases described above. It goes to the same path and replaces
  the main module.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the three modules and this README.
