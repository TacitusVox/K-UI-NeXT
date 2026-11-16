K-UI Toy Commander Test 67 - SCI wire probe on the physical GD-ROM

This run plays the two opening movies from your original Toy Commander
GD-ROM, exactly as Test 63 did. While they play, the SCI pulls data from the
microSD card in the background and throws it away. The game never uses that
data. The report shows how much data the SCI could move beside the running
movie decoder and what that cost. Nothing on the card is written except the
two reserved report sectors, as before.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed files.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

RUN
1. Cold boot the K-UI CD, tap B during the countdown and open Card Tools.
2. Select GD Play and press Right for the Toy Commander physical timing
   test, then follow the disc prompt with your original Toy Commander
   GD-ROM, exactly as for Test 63. Keep the SCI card connected. Choose
   60 Hz when the game asks.
3. Let both movies play from the start without skipping. A short pause
   before the first movie is expected while the card stream is opened.
   Watch whether picture and sound stay as smooth as on a normal disc run.
4. Save the report the same way you saved Test 63: at the title screen hold
   A+B+X+Y+Start, the game's own reset combination, until REPORT SAVED TO
   SD appears. Then power off.
5. Send the newest numbered file in /reports/toy/ whose name starts with 63
   (63-2.txt, 63-3.txt and so on). The installed Card Tools still names the
   file after Test 63; the text inside says TEST 67 and BUILD e5fa67e75f73.
   Also say whether either movie stuttered, lost sync or ended early.

If the save fails, five report pages cycle on screen every five seconds.
Film all five. If the game stops or resets before the title screen, power
off and note which movie and roughly where.

REPORT FIELDS
All values are hexadecimal. One second is 781250 timer ticks (hex BEBC2).
B is the first movie and I the second, each from its first shown frame to
the player's exit.

ST ERR AUX MODE SEGS
  ST 3 means the stream ran and was closed cleanly. 0 means it never
  started, 4 that it stopped on a fault. ERR and AUX name the first refusal
  or fault. MODE 2 or 1 is the timer channel used for waits, 0 none free.
  SEGS counts completed 16 KiB stretches.
SUB CAD MAXGAP SKIP EXIT MAXREQ
  The Test 63 frame counters: frames submitted, frames on time, longest
  gap, skipped frames, the player's exit code (7DC is a normal end) and the
  longest GD-ROM request. Test 63 had SUB 14B and 7B0 with SKIP 0.
SPAN BYTES CUTS IRQ ISR
  Length of the window in ticks, wire bytes clocked from the card, receive
  overruns, calls to the probe's interrupt handler and ticks spent in it.
PAUSE TICKS MAX LATE
  Overruns answered with a wait, total and longest wait, and ticks the
  stopped receiver waited for its interrupt to be served.
GD TICKS BYTES CUTS
  The part of the window, wire bytes and overruns that fell inside GD-ROM
  transfers. An SD reader would not compete with those.
RUN 16 64 256 1K 4K MORE
  Overruns by how many bytes arrived before them.
PC N PC N
  The two 64-byte code regions of the game most often interrupted by an
  overrun, each with its estimated count.
TAIL BYTES OK BAD HOLE
  The last 16 KiB received, checked afterwards: bytes held, SD blocks whose
  CRC agrees, blocks that fail it without a known lost byte (must be 0) and
  blocks containing a known lost byte.

CHANGED FILE
KUI/apps/disc/native-toy-trace.kui
  Build e5fa67e75f73, payload CRC32 2BEF9B88. Replaces the Test 63 module
  5034eb26898d at the same path. To return to Test 63, copy that file back
  from the Test 66 package.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies both modules and this README.
