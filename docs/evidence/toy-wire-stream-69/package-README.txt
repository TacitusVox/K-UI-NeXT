K-UI Toy Commander Test 69 - the card stream is read between the game's calls

Test 68 on the console: the continuous card stream and its checks worked,
and the first movie played through. The second movie fell behind after two
seconds. The report showed why. The stream's data was mostly read only when
the game asked for it, once a frame, with the movie player waiting, and the
stream stood still in between because its buffer was full.

Test 69 changes one thing against Test 68. When the reader runs out of its
half millisecond with data still unread, it has a spare timer call it again
three quarters of a millisecond later, and goes on reading in the same
short pieces while the game runs. The frame's data should then be there
when the game asks. Everything else is Test 68: the same stream, the same
checks on every block and sector, the same way of serving the game.

The report has new rows that say where the reader's time went and how long
the game's requests waited, so that if the second movie still stutters the
file tells which of the remaining causes it is.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 69 entry. It replaces the Test 68
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 69. Use
   the same settings as for Test 68: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 69
   and BUILD fb625c09dde6. Please also say how each movie looked and
   sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
Whether the second movie plays in step once its data is read in the
background. If it does not, the report separates what is left:
- Q large: requests still waited for data. P and W then say whether the
  reader was short of time or streams had to be started again.
- Q small, I SKIP still large: the data was there in time and the player
  still fell behind. Then the reader takes too much of the CPU (V), and
  the next change is to make reading a block cheaper.
- F rows: what the bytes looked like where a stream could not be followed.
  Test 68 lost 74 streams that way without saying why.

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
P TICK BUDGET RT RMAX
  Timer calls arranged to go on reading, visits that ran out of time with
  data unread, ticks from a stream being given up to the next one
  receiving (all of them together), and the longest such restart.
W START SEG CUT WAIT FULL
  Streams begun, runs that reached their length, receive overruns inside a
  run, overruns answered with a timer wait, and stops for lack of room.
W BYTES WTICK TMR ERR DET
  Wire bytes received, ticks spent in timer waits, the timer channel used
  (1 or 2; 0 means none was free, and then nothing is new in this test),
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
W DROP E N I C W O
  Why verified windows were given up: reader error, movie over, an
  unfinished earlier request, a command that is not a read, a read the
  game waited for with interrupts held off, or a read outside the window.
F BIT03 BIT47 KIND
  The unexpected bytes between blocks, counted two ways. By the lowest
  zero bit of the byte as received: bits 0 to 3 in the four bytes of the
  first word, lowest bit in the lowest byte, bits 4 to 7 in the second.
  And by the block before: none yet, clean, rebuilt, or handed on with
  lost bytes, lowest byte first.
F1, F2
  The first two such places: the four bytes before the unexpected byte;
  that byte and the three after it; then one word with the distance from
  the previous block's end in its low half, above it one bit for each of
  the eight bytes that was a lost position, and at the top how many of
  the last four had arrived; the distance back to the nearest lost byte
  (FFFFFFFF if none); and the blocks the stream had delivered, with the
  kind of the last in the top digit.
SD BLK PIO START CRC
  The low reader, which opens the card and re-reads what the stream could
  not deliver: blocks by DMA, blocks by programmed transfers, CMD18 starts
  and block checksum failures.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 69, build fb625c09dde6, payload CRC32 ADB4AA80. Replaces the Test 68
  module 806015566fc7 at the same path. To return to Test 68, copy that
  file back from the Test 68 package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
