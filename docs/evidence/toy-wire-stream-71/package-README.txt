K-UI Toy Commander Test 71 - the reader's inner loop, and a timeline of the second movie

Test 70 on the console: the first movie now plays exactly as from the disc,
all 331 frames, none skipped. The reader's time fell by half, from 32.4 to
16.7 seconds, which is what moving its stack into the cache was estimated to
do. The second movie showed 436 of its 1,968 frames (319 before) and is
still a slideshow for most of its length.

Test 71 is Test 70 with one change. The largest part of the reader's time
is the loop that takes each block's bytes out of the receive buffer, checks
the card's checksum and puts the bits in order. It ran 79 processor
instructions for every four bytes. It now runs 49. What it produces is the
same, byte for byte; on the PC every run of the new loop is checked against
the old one. From the instruction counts this should take about three more
seconds off the reader's 16.7. That is an estimate, not a measurement.

The report also changes. Test 70's file could not say where in the second
movie the smooth stretches were, or what the reader was doing in between.
This one carries a timeline of that movie in twelve pieces of 5.4 seconds:
frames shown, data served, time requests waited, the reader's time, and
streams that had to be restarted. Some rows that have said what they had to
say are dropped to make room.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 71 entry. It replaces the Test 70
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 71. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 71
   and BUILD 4ee37937a022. Please also say how each movie looked and
   sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
How much the new loop takes off the reader's time, and what the second
movie does over its length.
- V: the ticks in the three kinds of visit. Test 70 had 49A5DF, 79C5 and
  7C7623 (6.2, 0.04 and 10.4 seconds).
- I SUBMIT and I SKIP: Test 70 submitted 1B4 of the second movie's 7B0
  frames and skipped 5FC.
- TF, TD, TW, TI, TG, TX: the second movie's course. Where frames were
  shown (a full piece is A1, 161 frames), and whether the stretches without
  frames go with requests waiting for data (TW), with the reader busy in
  interrupts (TI), or with restarted streams (TX).
- F END HELD TWICE: whether the streams that cannot be followed (137 in
  Test 70) come from one received byte being stored twice.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ
  Times the verified window opened for each movie, and visits to the
  reader from the game and from the SCI interrupt.
AHEAD B I HIT EDC STOP
  Sectors served to the game from the verified window for each movie, the
  requests that made up, sectors verified, and windows given up on error.
I SUBMIT CADENCE MAX AT    (and the same row for B)
  Frames the player submitted, how many of them on time and in sequence,
  the longest gap between two in ticks, and the frame it came after.
  From the disc: 14B and 7B0 submitted, all but one or two on time.
I FIRST LAST SKIP EXIT WF SF    (and the same row for B)
  Ticks to the first frame, the last frame's number, frames skipped, the
  player's exit code (7DC is a normal end), the first frame at which a
  read was still pending, and the first frame skipped.
V SCI TMR GAME N TICKS
  Visits to the reader from the SCI interrupt, from the timer and from
  the game's own calls, then the ticks spent in each of the three.
V MAX SCI TMR GAME
  The longest single visit of each kind, in ticks.
Q B N T MAX I N T MAX
  For each movie: requests the window could not serve when first asked,
  their ticks until they were served, and the longest of them.
P BUDGET RT RMAX
  Visits that ran out of time with data unread, ticks from a stream being
  given up to the next one receiving (all of them together), and the
  longest such restart.
TF, TD, TW, TI, TG, TX
  The second movie in twelve pieces of 5.4 seconds (400000 ticks) counted
  from its start. Each row has three numbers of up to eight digits: two
  digits a piece, four pieces a number, earliest first. A number is
  printed without leading zeros, so fill it up to eight digits on the left
  before reading it. A piece that reaches FF may stand for more.
  TF  frames the player submitted (A1, 161, is every frame)
  TD  sectors served to the game, in sixteens (84 is the movie's own pace)
  TW  time requests waited for data, in steps of 21 ms
  TI  the reader's time in interrupt visits, in steps of 21 ms
  TG  the reader's time in the game's own calls, in steps of 21 ms
  TX  streams given up and started again
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
  was a checksum byte; and blocks the low reader read to open or reopen.
W GAPI GAPT BADCRC FRAME AMBIG
  Lost bytes between blocks taken as idle bytes or as the block's start
  token, and streams that could not be followed further: a whole block
  failing its checksum, an unexpected byte between blocks, or too many
  lost bytes to tell.
W FILL BYTES MOST REF BAD DECL
  Sectors completed from their own error-correction bytes and the bytes
  filled that way, the most lost bytes one sector arrived with, sectors
  that could not be filled, sectors that failed their address or checksum
  afterwards (both kinds were read again), and takeovers declined.
W STOP C J S F X
  Why streams ended: window closed, the file continued elsewhere on the
  card, a sector had to be read again, the stream could not be followed,
  or a hardware fault.
F END HELD TWICE
  Unexpected bytes found directly behind a block's end; of those, how
  many of the blocks had a byte that the reader took from the receiver
  itself after an overrun; and of those, how many such bytes repeat the
  byte before them, as a byte stored twice would.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 71, build 4ee37937a022, payload CRC32 939D6FDC. Replaces the Test 70
  module b492f00a5478 at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
