K-UI Toy Commander Test 72 - fewer visits to the reader from the player

Test 71 on the console: the first movie again played exactly as from the
disc, all 331 frames. The second movie showed 524 of its 1,968 frames (436
before), now three or four in a row before it stops, as you saw. The new
loop took 1.2 seconds off the reader's time, less than the three estimated.

The timeline in that report says why, and where the rest of the time goes.
The player is not waiting for data: over the whole second movie its
requests waited 4.6 seconds in all. It is short of processor time. While a
movie plays the game calls the player about 2,500 times a second, and the
reader answered every one of those calls. It was visited 193,622 times
from the game, most of them these calls, and that took 10 of its 15.4
seconds, almost all to find that there was nothing for it to do. The cost
was the same in the pieces of the movie where hardly any data was read.

Test 72 is Test 71 with one change. One of those calls now reaches the
reader only when the reader has work that needs it (a stream to start, a
stream standing still because its buffer is full, or an interrupt that is
due and has not come) and otherwise once in four milliseconds. Requests for
data and the card's interrupts reach the reader as before, and every sector
and every block is checked as before.

The reader's visits from the game should fall from 193,622 to a few tens
of thousands and their time from 10 seconds to perhaps 2 or 3. That is an
estimate from Test 71's numbers, not a measurement; the report measures
it. How much smoother the second movie gets with that time given back to
the player is what the test is for.

One row of the report changes. Test 71 answered the question of the row
F END HELD TWICE: of 201 unexpected bytes behind a block, 2 were in blocks
with a byte taken from the receiver after an overrun and none repeated the
byte before it. No byte is being stored twice. That row is replaced by a
count of the player's calls.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 72 entry. It replaces the Test 71
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 72. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 72
   and BUILD 8c00ede90b9a. Please also say how each movie looked and
   sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
How much of the reader's time the player's calls were costing, and how the
second movie plays with that time given back.
- V: the game's visits and their ticks. Test 71 had 2F456 visits from the
  game taking 7710F9 ticks (10.0 seconds); from the interrupt 8BD8 visits
  and 406C1E ticks (5.4 seconds).
- U SEEN WORK DUE: how many of the player's calls there were, and how many
  reached the reader because it had work or because four milliseconds had
  passed. WORK and DUE together should be a small part of SEEN.
- I SUBMIT and I SKIP: Test 71 submitted 20C of the second movie's 7B0
  frames and skipped 5A4.
- TF, TD, TW, TI, TG, TX: the second movie's course. Test 71's TF row was
  2E02180C B070703 E15A2D7: 46 frames in the first 5.4 seconds, then
  between 2 and 24 a piece, and 162 and 215 in the last two.
- B SUBMIT and B SKIP: the first movie must stay at 14B and 0.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ
  Times the verified window opened for each movie, and visits to the
  reader from the game and from the SCI interrupt. From this test on the
  game's visits no longer include every call of the player's.
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
U SEEN WORK DUE
  The player's update calls while a movie was open; of those, the calls
  passed on to the reader because it had work only such a call can do,
  and the calls passed on because none had reached it for four
  milliseconds.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 72, build 8c00ede90b9a, payload CRC32 FC7BBF9F. Replaces the Test 71
  module 4ee37937a022 at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
