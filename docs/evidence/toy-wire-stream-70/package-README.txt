K-UI Toy Commander Test 70 - the reader's working memory moved into the cache

Test 69 on the console was a step back, and its report showed why. The new
rows measured the reader's own time for the first time: about 32 seconds of
the 76 the two movies last. The first movie's data arrived on time and it
still skipped frames, so what the movies lack is processor time, not data.
Moving the reader's work to a timer could not help with that, and the timer
is taken out again.

Test 70 is Test 68, the build whose first movie played fluidly, with one
change. The reader keeps its temporary values on a private stack, and that
stack was used at an address that bypasses the processor's cache. Its
innermost loop touches the stack nine times for every four bytes it reads,
and each touch cost a full memory cycle. The stack is now used through the
cache. Nothing else about the reader differs from Test 68: the same stream,
the same checks on every block and sector, the same way of serving the game.

From the compiled code this should take roughly 40 percent off the reader's
time. That is an estimate, not a measurement; the report's V rows will say
what it really is. It may not yet be enough for the second movie. If it is
not, the report tells how far off it is, and the next reduction (the inner
loop itself) is already identified.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 70 entry. It replaces the Test 69
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 70. Use
   the same settings as before: CD Music and virtual VMU Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66. Card
   Tools still names the file after Test 66; the text inside says TEST 70
   and BUILD b492f00a5478. Please also say how each movie looked and
   sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
How much of the reader's time the cached stack takes away, and what that
does for the movies.
- V: the ticks in the three kinds of visit. Test 69 had 74CB63, 54EADE and
  B8248C (9.8, 7.1 and 15.5 seconds). There is no timer reading in this
  build, so the middle one should be next to nothing; the sum is what to
  compare.
- I SUBMIT and I SKIP: Test 68 submitted 13F of the second movie's 7B0
  frames and skipped 671.
- P CCR MMU: how the game has set the cache up. The cached address is used
  when MMU is an even number. The stack's writes then stay in the cache
  when CCR ends in 1, 5, 9 or D (as in 105 or 909); ending in 3, 7, B or F
  they are also written to memory each time, and only the reads gain.
- Q: requests that waited for data. With the player skipping this is large
  whatever the reader does; it matters once the skipping stops.

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
P CCR MMU BUDGET RT RMAX
  The game's cache control register and MMU control register as they were
  when the movie's window opened, visits that ran out of time with data
  unread, ticks from a stream being given up to the next one receiving
  (all of them together), and the longest such restart.
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
  Test 70, build b492f00a5478, payload CRC32 3A86E651. Replaces the Test 69
  module fb625c09dde6 at the same path. To return to Test 68 or 69, copy
  that file back from its package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
