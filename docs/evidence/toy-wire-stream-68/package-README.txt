K-UI Toy Commander Test 68 - continuous card stream for the opening movies

Test 67 showed that the SCI can pull enough data from the microSD card in
the background for both opening movies while the game runs at full speed,
provided it is not stopped and restarted for every 512-byte block. This
build puts that into the SD reader. The game is started from the card, with
no GD-ROM, and the two opening movies are read through one continuous
background stream. Every movie sector still has its address and checksum
verified before the game gets it; a byte the SCI loses is rebuilt from the
card's block checksum or from the sector's own error-correction bytes, and
a sector that cannot be completed is read again. Apart from this one change
the reader is Test 66.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

The reader module is installed where Card Tools looks for Test 66, because
the installed Card Tools has no Test 68 entry. It replaces the Test 66
module there.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run "66 SCI movie playback". That entry now starts Test 68. Use
   your normal display settings. Set CD Music and virtual VMU to Off.
3. Let both movies play from the beginning without skipping. Watch whether
   picture and sound are as smooth as from the disc, and where they are not.
4. After the movies finish or drop out, hold both triggers and A+B+X+Y for
   about three seconds. After the save message press Start to reboot.
5. Send the newest file in /reports/toy/ whose name starts with 66 (66.txt,
   66-2.txt and so on). Card Tools still names the file after Test 66; the
   text inside says TEST 68 and BUILD 806015566fc7. Please also say how
   each movie looked and sounded.

If the save fails, two report pages alternate on screen. Film both. They
carry the playback rows only; the stream rows exist only in the file.
If the game stops or resets before the title screen, power off and note
which movie and roughly where.

WHAT THE TEST ANSWERS
Whether the SD reader plays both movies at full speed when it is fed by the
continuous stream, and, if not, which of these held it back: the wire
(W START..FULL, W BYTES), lost bytes that could not be repaired (W FILL..BAD,
W STOP), the reader of the stream falling behind (W IRQ), or the game's
requests not being served from the verified window at all (AHEAD, W DROP).

REPORT FIELDS
All values are hexadecimal, written without leading zeros. One second is
781250 timer ticks (hex BEBC2). B is the first movie and I the second.

PIPE B I CALLS IRQ FAIL
  Times the verified window opened for each movie, visits to the reader
  from the game and from the SCI interrupt, and failed game reads (0).
AHEAD B I HIT EDC STOP
  Sectors served to the game from the verified window for each movie, the
  requests that made up, sectors verified, and windows given up on error.
SD BLOCKS PIO START CRC
  The low reader, which opens the card and re-reads what the stream could
  not deliver: blocks by DMA, blocks by programmed transfers, CMD18 starts
  and block checksum failures.
W START SEG CUT WAIT FULL
  Streams begun, runs that reached their length, receive overruns inside a
  run, overruns answered with a timer wait, and stops for lack of room.
W BYTES WTICK TMR ERR DET
  Wire bytes received, ticks spent in timer waits, the timer channel used
  (1 or 2; 0 means none was free), and the first stream fault with its
  detail (0 0 if none).
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
W FILL BYTES MOST REFUSE BAD
  Sectors completed from their own error-correction bytes and the bytes
  filled that way, the most lost bytes one sector arrived with, sectors
  that could not be filled, and sectors that failed their address or
  checksum afterwards. REFUSE and BAD sectors were read again.
W STOP CLOSE JUMP SECT FRAME FAULT
  Why streams ended: window closed, the file continued elsewhere on the
  card, a sector had to be read again, the stream could not be followed,
  or a hardware fault.
W DROP ERR END IDLE CMD WAIT OTHER
  Why verified windows were given up: reader error, movie over, an
  unfinished earlier request, a command that is not a read, a read the
  game waited for with interrupts held off, or a read outside the window.
W IRQ TICKS MAX PARITY DECL
  SCI interrupt visits while streaming, ticks spent in them in total and
  at most in one, whether an undamaged sector's error-correction bytes
  were consistent (1 yes, 2 no, 0 not seen), and takeovers the stream
  declined.
SUBMIT CADENCE MAX AT TAIL / FIRST LAST CLK OUT SKIP / ENTER SCENE EXIT FRAME
  The Test 63 and 66 frame counters for each movie: frames submitted and
  on time, longest gap and where, frames output and skipped, and the
  player's exit code (7DC is a normal end). From the disc, Tests 63 and 67
  had SUBMIT 14B and 7B0 with SKIP 0.
WAIT USED NEED SKIP LBA / READ N VERIFIED TICK AGE DONE / I LAG LIMIT
  Test 66's first-wait rows, written only as far as room is left.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 68, build 806015566fc7, payload CRC32 23676B7E. Replaces the Test 66
  module 3124522ea906 at the same path. To return to Test 66, copy that
  file back from the Test 66 package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
