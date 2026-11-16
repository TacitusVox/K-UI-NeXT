K-UI Toy Commander Test 73 - the game's disc polls, and a look at its player

Test 72 on the console: the first movie unchanged, the second showing 567
of its 1,968 frames against 524. Marginal, as you saw.

The change itself did what it was built to do: of 96,961 calls of the
player's, 48 reached the reader. But the reader's time in the game's calls
hardly moved, from 10.0 to 9.7 seconds. The cost was not in the calls I
took out. I had read Test 71's report wrongly on that point: I took every
call of the game's that carried no request for a call of the player's.
About half of them are something else. While a movie plays the game asks
its disc system "anything done yet?" about 1,550 times a second, and each
of those polls ran the reader's whole foreground routine. Those are the
expensive ones: 119,226 of them, 81 microseconds each.

Test 73 is Test 72 with one change: those idle polls get the same rule the
player's calls got. One reaches the reader's routine only when the reader
has work that needs it, or once in four milliseconds. A poll that is
waiting for data runs it every time, as before. So do the card's
interrupts. Every sector and every block is checked as before.

The reader's time in the game's calls should fall from 9.7 seconds to
about 3. That is an estimate. What it does for the second movie I do not
know, and Test 72 gave a reason for doubt: the game polled more often once
each poll cost it less, which is what a program does that is waiting, not
one that is short of time. Time taken from waiting is not time taken from
decoding.

So this test also measures what I have been inferring. The report now
reads the game's player directly:
- how long the player's own calls take when it shows a frame, and when it
  skips one, and how much of that time the reader's interrupts took;
- how full the player's own data buffer is through the movie, and how far
  behind the player itself thinks it is;
- how often the player went for a frame's data and did not find it.
With that the next change can be aimed at what the player is actually
short of. Several older rows have given way to make room.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 73 entry. It replaces the Test 72
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 73. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 73
   and BUILD e623c2a697d5. Please also say how each movie looked and
   sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
What the idle polls were costing, and what the second movie's player is
short of: processor time, data, or neither.
- V and U: the game's visits and their ticks. Test 72 had 1D1EA visits
  from the game taking 734262 ticks (9.7 seconds). In U the last three
  numbers are the idle polls seen and the few passed on.
- I SUBMIT and I SKIP: Test 72 submitted 237 of the second movie's 7B0
  frames and skipped 579.
- IP against BP: the second movie's player against the first's, which
  plays every frame. OT divided by ON is the time of one call that shows a
  frame. A frame has 65BA ticks (33 ms). If the second movie's calls take
  longer than that and the first's do not, the player is short of
  processor time, and OI says how much of it the reader's interrupts take.
  If they are short, it is not.
- TB and IC: if TB climbs to the buffer's size (80 for a megabyte) in the
  pieces that skip, or IC's WGD and NOGD are large, the player is running
  out of data although the reader has it ready.
- TL: how far behind the player counts itself, piece by piece.
- B SUBMIT: the first movie must stay at 14B.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ
  Times the verified window opened for each movie, and runs of the
  reader's routine from the game's calls and from the SCI interrupt.
AHEAD B I HIT EDC STOP
  Sectors served to the game from the verified window for each movie, the
  requests that made up, sectors verified, and windows given up on error.
I SUBMIT CADENCE MAX AT    (and the same row for B)
  Frames the player submitted, how many of them on time and in sequence,
  the longest gap between two in ticks, and the frame it came after.
  From the disc: 14B and 7B0 submitted, all but one or two on time.
I FIRST LAST SKIP EXIT WF SF
  Ticks to the first frame, the last frame's number, frames skipped, the
  player's exit code (7DC is a normal end), the first frame at which a
  read was still pending, and the first frame skipped.
IP ON OT OC OI SN ST    (and BP for the first movie)
  The player's own calls, sorted by what the player's own counters say
  each call did. ON: calls in which it put a frame out. OT: ticks inside
  those calls. OC: ticks from the end of the call before to the end of
  each of those, the whole turn. OI: the reader's interrupt ticks in that
  span. SN: calls in which it skipped a frame and put none out. ST: ticks
  inside those.
IB SKU SKN WTU WTN CAP ALL
  The player's data buffer. Bytes in it, and bytes of the next frame not
  yet in it, at the first skip (SKU, SKN) and the first time it waited
  for a read (WTU, WTN). CAP is the buffer's size. ALL is the ticks inside
  all the second movie's calls of the player. FFFFFFFF is a value that
  could not be read.
IC RDY WGD NOGD OTH
  Times the second movie's player went for the next frame's data: it was
  there; it was not, with a read under way; it was not, with no read
  under way; anything else.
V SCI TMR GAME N TICKS
  Runs of the reader's routine from the SCI interrupt, from the timer and
  from the game's calls, then the ticks spent in each of the three.
U S W D G S W D
  The player's calls seen, passed on to the reader because it had work,
  and passed on because four milliseconds had gone by; then the same
  three for the game's idle disc polls.
D N TICKS
  The game's disc calls that reached the reader at all, and the ticks in
  them from entry to exit, whether the routine ran or not.
TF, TD, TW, TB, TL, and at the end TI, TG
  The second movie in twelve pieces of 5.4 seconds (400000 ticks) counted
  from its start. Each row has three numbers of up to eight digits: two
  digits a piece, four pieces a number, earliest first. A number is
  printed without leading zeros, so fill it up to eight digits on the left
  before reading it. A piece that reaches FF may stand for more.
  TF  frames the player submitted (A1, 161, is every frame)
  TD  sectors served to the game, in sixteens (84 is the movie's own pace)
  TW  time requests waited for data, in steps of 21 ms
  TB  the most of the player's buffer that stood empty, in steps of 8 KB
      (80 is a megabyte)
  TL  the highest the player's own count of its lag stood
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
  was a checksum byte; and blocks the low reader read to open or reopen.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 73, build e623c2a697d5, payload CRC32 0F34C364. Replaces the Test 72
  module 8c00ede90b9a at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
