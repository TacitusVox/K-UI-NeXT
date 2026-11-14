K-UI Toy Commander Test 74 - the reader's own work, cut down

Test 73 on the console: the change did what it was built to do (of
105,841 idle polls, 5,969 reached the reader) and the second movie did
not change: 513 of its 1,968 frames against 567. As you saw.

That report also printed the game's own player for the first time, and
with it the picture is finally clear. I had been reading the reports
wrongly for three tests, so plainly:

- The reader's time is not in how often it is visited. It is in the
  reading itself: 15.1 seconds over both movies in every test since 71,
  about 112 microseconds for each of the 134,563 card blocks, whoever
  does it. Tests 72 and 73 took visits away, and the visits that were
  left did the same reading.
- In the second movie the player is busy every moment. By my estimate
  it needs about 20 of each frame's 33 milliseconds for itself, and
  through the dense first 54 seconds the reader takes the other 12 to
  13. The movie sits right on the edge. A little too slow, the player
  never gets ahead, so it shows a frame or a few and is late again. The
  last 12 seconds carry a third of the data, the reader takes less, and
  they play smoothly: the fluid ending you described.

So the reader has to do the same reading with less work. Test 74 is five
changes to that end. You asked for one build with everything I can come
up with, so they come together. Each is its own source commit and its
own build switch, so a later build can leave any one out.

1. No hold at the second movie's start. Since Test 57 the game's own
   refill reads were held back when that movie begins, until its buffer
   had run half empty; only then was my reader allowed to serve. It had
   to start, read ahead and keep pace all at once, and every run so far
   skipped first right there, around frame 20 to 27. Now it serves from
   the first request beside a full buffer, as it does in the first
   movie.
2. A lighter second check. Every block from the card has its checksum
   (CRC16) verified, as always. Until now every sector then went
   through the CD format's own check (EDC) as well: about a third of
   all the work on each byte. Now a sector whose blocks all arrived
   whole and passed their CRC16 is accepted on that and on its address.
   A sector that lost a byte to an overrun still gets a second check: a
   single rebuilt byte against the sector's parity, anything more
   through the EDC as before. One sector in sixteen takes the EDC
   anyway, as a watch on the lighter path; a single failure there
   switches the lighter path off for the session and shows in the
   report.
3. A question to the console. The Dreamcast's serial port hands over
   every byte with its bits in reverse order, and the reader turns each
   one round by table. There is a register bit that asks the port for
   the right order. The processor's manual describes it for another
   mode; whether it works in this one I could not find out anywhere. So
   the reader asks, looks at the first byte it gets, and carries on
   either way. The report says what the console answered.
4. A short way for visits with nothing to do. More than 100,000 times a
   run the reader was entered, found nothing to decide, and walked its
   whole decision path to learn that. Those visits now take a few
   instructions.
5. The inner loop by hand. Every byte goes through one small loop. The
   compiler's version takes 49 instructions for four bytes; written out
   by hand it takes 41, or 34 if the port answers yes to the question
   in 3.

What I expect, and how sure I am. By instruction counts, and by a model
with stalls like your console's, the work on each byte falls by about a
quarter to a third and the reader's total from roughly 19 seconds to 14
or 15. In the dense part that hands the player 2 to 3 milliseconds of
every frame. Because the movie sits on the edge, that should show: the
second movie smooth from its start, and smooth for much more of its
length. Whether it is enough for all of it I cannot promise. These are
estimates from counting. The report from your run is the measurement,
and if it is not enough yet it will say how much is still missing.

One thing this build gives up on purpose. If a block were stored wrong
on the card itself, with a checksum that fits the wrong data, Test 73
refused it and Test 74 will normally pass it on (the one-in-sixteen
watch may still catch it). That would be a damaged file on the card,
not a reading error, and the ordinary game readers have never had the
second check. Every block's CRC16 is checked as before.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
   Leave the ALTERNATE folder off the card for now.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 74 entry. It replaces the Test 73
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 74. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 74
   and BUILD e82b31657f69. Please also say how each movie looked and
   sounded, the first seconds of the second one in particular.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.

THE SECOND MODULE (ALTERNATE folder)
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui is the same build without
change 3, the question to the serial port. It never sets that register
bit and treats the port exactly as Test 73 does. It is there for one
case: the main module does something Test 73 did not do, such as a hang,
a reset, movies that do not start, a garbled picture, or a first movie
that no longer plays smoothly. Then power off, merge the KUI folder
inside ALTERNATE into the card's KUI folder the same way, run again, and
send that report too, with a note which is which. If the main module
runs normally you do not need it.

Both modules say TEST 74 and BUILD e82b31657f69. The last value of the
report's K row tells them apart: it is 0 only with the alternate.

WHAT THE TEST ANSWERS
Whether the second movie now has the processor time it needs, and what
each change was worth.
- I SUBMIT and I SKIP: Test 73 submitted 201 of the second movie's 7B0
  frames and skipped 5AF. SF, the first frame skipped, was 1B.
- B SUBMIT: the first movie must stay at 14B.
- V and W BLK: the reader's ticks (the last three values of V, added)
  divided by the blocks read. Test 73: 5EAB12, 57F4 and 553586 ticks for
  20DA3 blocks, which is 57 ticks (112 microseconds) a block.
- K: how many sectors took the lighter check and how many the EDC; LB
  must be 0; ORD is the serial port's answer (see below).
- IP against BP, as in Test 73: the second movie's player against the
  first's. OT divided by ON is the time of one call that shows a frame;
  a frame has 65BA ticks (33 ms).
- D: what the game's disc calls cost from entry to exit, all of them.

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second. A
row that does not fit the file is left out whole, last rows first.

PIPE B I CALLS IRQ
  Times the verified window opened for each movie, and runs of the
  reader's routine from the game's calls and from the SCI interrupt.
AHEAD B I HIT EDC STOP
  Sectors served to the game from the verified window for each movie, the
  requests that made up, sectors taken into the window (by either check;
  the label is the old one), and windows given up on error.
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
IC RDY WGD NOGD
  Times the second movie's player went for the next frame's data: it was
  there; it was not, with a read under way; it was not, with no read
  under way.
K LIGHT EDC MEND MB LB ORD
  Sectors accepted on their blocks' checksums and address; sectors passed
  through the EDC; rebuilt bytes their sector's parity confirmed; rebuilt
  bytes it did not confirm (the EDC then decided); sectors that passed
  the lighter check and failed the EDC watch (must be 0; one switches the
  lighter check off). ORD is the serial port's answer to change 3:
  1 it ignores the request and everything is read as in Test 73;
  3 it delivers bytes the right way round, and a block has confirmed it;
  4 its first byte looked right and the first block did not: the reader
    went back to the old way;
  2 or 0 not decided (0 always with the alternate module).
V SCI TMR GAME N TICKS
  Runs of the reader's routine from the SCI interrupt, from the timer and
  from the game's calls, then the ticks spent in each of the three.
U S W D G S W D
  The player's calls seen, passed on to the reader because it had work,
  and passed on because four milliseconds had gone by; then the same
  three for the game's idle disc polls.
D N T G N T
  The game's disc calls that took the reader's whole path and the ticks
  in them; then all of the game's disc calls while a movie's request was
  the last one, each timed from its entry to its exit, and their ticks.
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
  was a checksum byte; and blocks the low reader read to open or reopen.
R C8 32 128 W8 32 128
  Receive runs that an overrun cut again within 8, 32 and 128 bytes of
  their start: first those begun at once behind a cut, then those begun
  after a timer wait.
W FILL BYTES MOST REF BAD
  Sectors completed from their parity, the bytes filled, the most lost
  bytes one sector arrived with, sectors the parity could not complete,
  and sectors that failed their check. The last two are read again.

Gone to make room: IB, TB, TL and the D row's old form.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 74, build e82b31657f69, payload CRC32 BC747EF0. Replaces the Test 73
  module e623c2a697d5 at the same path. To return to an earlier test, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

NOT PART OF THE KUI TREE
ALTERNATE/KUI/apps/games/retail-toy-test-66.kui
  Test 74 without change 3, build e82b31657f69 (the same source, built with
  TOY_WIRE_MSB=0), payload CRC32 3085DC70. Copy it only in the case
  described above. It goes to the same path and replaces the main module.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the three modules and this README.
