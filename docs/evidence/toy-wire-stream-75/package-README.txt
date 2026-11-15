K-UI Toy Commander Test 75 - a clean start, and the reader out of the
player's way

Test 74 on the console, from your two reports:

- The second movie showed 967 of its 1,968 frames (the alternate 931)
  against 513 in Test 73. No slideshow any more, as you saw. The pauses
  you describe are in the report: 36 times the player skipped ahead
  without showing anything, 0.44 seconds each.
- For most of the movie the player was not waiting for data. It was
  short of processor time: a frame it shows costs it 46 milliseconds
  where it has 33. In the first movie a frame costs it 25.
- The reader's own work fell by a seventh. I had estimated a quarter to
  a third. So I was wrong again about where its time goes, and Test 74's
  report could not tell me: it times the reader's visits, not what a
  visit does. This build measures that directly.
- Your console's serial port does deliver bytes the right way round when
  asked (the main module's ORD was 3). That is settled, and worth two
  percent of the reader's time.
- Taking the hold off the second movie's start cost its start. The
  reader opened its window 45 times for that movie where Test 73 opened
  it once; the first frame came a quarter of a second later and the
  first skip at frame 10 instead of 27. Taking the hold off was not the
  mistake. What was missing is change 1 below.

Test 75 is three changes and a better report. As before each change is
its own source commit and its own build switch.

1. A request the game waits for keeps the reader's window. For some
   reads the game stops everything and asks again and again until the
   data is there. Up to now my reader gave its window up at the first
   such call and let the old reader fetch that request, then started a
   new window for the next one: a stream begun and ended for nearly
   every request while the second movie got under way. Now the window
   stays and the stream serves those requests like any other. In my
   model of the game's calls, eight such requests were eight windows
   opened and given up; now they are one window and one stream.
2. Interrupts only keep the wire going. Up to now every interrupt from
   the card's stream also read what had arrived: about three blocks,
   four hundred times a second, each time pushing the player's code and
   data out of the processor's cache and the reader's in. Now an
   interrupt restarts the stream and leaves. The reading is done from
   the game's own calls, every few milliseconds and in larger pieces.
   An interrupt still reads when a request is waiting, or when the
   unread data has piled up.
3. The reader looks at arrived data through the processor's cache. Until
   now it fetched it from memory four bytes at a time, 129 fetches for
   each card block. Through the cache 32 bytes come at once. Because the
   card's data arrives in memory behind the cache's back, every piece is
   thrown out of the cache before it is read, so that nothing old can be
   read. Every block's checksum (CRC16) is taken over what was read, as
   always.

And the report: five clocks inside the reader (taking blocks out of the
ring, feeding the window, checking sectors, copying out to the game,
restarting the stream), and the reasons streams and windows ended.

What I expect, and how sure I am. Change 1 should give the second movie
a clean start: one or two windows where Test 74 opened 45, the player's
buffer full, no quarter second lost. I am fairly sure of that; the
report's PIPE and X rows will say. (The first movie has opened ten
windows in every test for the same reason and played well all the same;
it should open one or two now.)

Changes 2 and 3 give the player processor time: by my estimate a few
percent of the processor together. The player needs about six percent
more than it has (2 of its 33 milliseconds a frame). So I expect fewer
and shorter pauses, and I do not expect them all gone with this build.
My estimates of this kind have been too hopeful three times, so treat
that as a guess. What this build does for certain is say where the
reader's time goes, to the block. That decides the next build, and I
think one or two more after this one is realistic.

Before sending, each change was read by an independent reviewer looking
for ways it could fail. Changes 1 and 3: none found. Change 2: two, both
in fallback paths your console's reports have never shown in use. Both
are fixed in this build.

Not in this build, on purpose: two further changes I looked at (a faster
copy, and recovering streams that today have to be restarted, about 300
times a run). Both need the numbers this build's report will give.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
   Leave the ALTERNATE folder off the card for now.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 75 entry. It replaces the Test 74
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 75. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 75
   and BUILD bf6f1c703add. Please also say how each movie looked and
   sounded: the first seconds of the second one, and whether the pauses
   are fewer or shorter than with Test 74.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.

If the game hangs or resets, or a movie shows garbage, power off and go
back to the alternate below, or to Test 74 (copy its file back from its
package). Tell me what you saw; there will be no report from that run.

THE SECOND MODULE (ALTERNATE folder)
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui is the same build without
change 3: it fetches arrived data from memory as Test 74 does. Change 3
is the one that depends on how the processor's cache behaves on real
hardware, which my tests can model and not run. The alternate is there
for two cases:
- the main module does something Test 74 did not do (a hang, a reset,
  garbage, movies that do not start, a first movie that no longer plays
  smoothly). Then run the alternate and send its report with a note.
- the main module runs normally and you have the time for a second run.
  Then the two reports side by side measure change 3 exactly, which no
  single run can. Worth having, not required.
To use it, power off and merge the KUI folder inside ALTERNATE into the
card's KUI folder the same way.

Both modules say TEST 75 and BUILD bf6f1c703add. The last value of the
report's PIPE row tells them apart: 7 for the main module, 3 for the
alternate.

WHAT THE TEST ANSWERS
- Whether the second movie now starts cleanly. PIPE: Test 74 opened A
  (10) windows for the first movie and 2D (45) for the second. X: why
  windows were given up; its fifth value (W) is the waited requests that
  still cost a window. I FIRST and SF: Test 74's first frame came after
  D2CBD ticks (1.1 s) and the first skip at frame A.
- How much smoother the second movie is. I SUBMIT: Test 74 submitted 3C7
  of its 7B0 frames. IP: its player skipped without showing in 24 (36)
  calls (SN) and a call that showed a frame took 20C7354 / 3BC ticks,
  46 ms (OT / ON). A frame has 65BA ticks (33 ms).
- Where the reader's time goes: row P against V and W BLK.
- What each change did. V and P LATER for change 2: how many interrupt
  visits there were and how many left the reading. P RING for change 3,
  against the alternate's.
- B SUBMIT: the first movie must stay at 14B.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ STAY PT
  Times the verified window opened for each movie; runs of the reader's
  routine from the game's calls and from the SCI interrupt; requests the
  game waited for that kept their window (change 1); and the changes
  this module has: 1 for change 1, 2 for change 2, 4 for change 3, added.
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
  serial port's answer on the bit order: 3 as in Test 74's main run.
V SCI TMR GAME N TICKS
  Runs of the reader's routine from the SCI interrupt, from the timer and
  from the game's calls, then the ticks spent in each of the three. With
  change 2 the first count still has every interrupt visit; those that
  left the reading are P's last value.
P RING FEED CHK OUT SVC LATER
  The reader's ticks by what it was doing. RING: taking blocks out of the
  ring (finding the token, the bytes, the CRC16). FEED: handing them to
  the window (the copy and the sector's check). CHK: the sectors' checks
  alone, a part of FEED. OUT: copying requests out to the game. SVC: the
  stream's own service, a run that ended answered and the next begun.
  LATER: interrupt visits that only did that and left the reading.
D N T G N T SEEK
  The game's disc calls that took the reader's whole path and the ticks
  in them; all of the game's disc calls while a movie's request was the
  last one, each timed from its entry to its exit, and their ticks; and
  the ticks the old reader spent bringing the card to a stream's start.
TF, TD, TW, and at the end TI, TG
  The second movie in twelve pieces of 5.4 seconds (400000 ticks) counted
  from its start. Each row has three numbers of up to eight digits: two
  digits a piece, four pieces a number, earliest first. A number is
  printed without leading zeros, so fill it up to eight digits on the left
  before reading it. A piece that reaches FF may stand for more.
  TF  frames the player submitted (A1, 161, is every frame)
  TD  sectors served to the game, in sixteens (84 is the movie's own pace)
  TW  time requests waited for data, in steps of 21 ms
  TI  the reader's time in interrupt visits, in steps of 21 ms
  TG  the reader's time in the game's calls, in steps of 21 ms
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
X E N I C W O
  Why windows were given up: an error, the movie over, the request state
  no longer a finished read, a command that is not a read, a request the
  game waited for (with change 1 only one waited for in vain), other.
W FILL REF BAD
  Sectors completed from their parity, sectors the parity could not
  complete, and sectors that failed their check. The last two are read
  again.

Gone to make room: IC, U, R, WF and the byte counts of W FILL.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 75, build bf6f1c703add, payload CRC32 59E5F4C4. Replaces the Test 74
  module e82b31657f69 at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

NOT PART OF THE KUI TREE
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui
  Test 75 without change 3, build bf6f1c703add (the same source, built with
  TOY_WIRE_RING=0), payload CRC32 3C95285E. Copy it only in the cases
  described above. It goes to the same path and replaces the main module.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the three modules and this README.
