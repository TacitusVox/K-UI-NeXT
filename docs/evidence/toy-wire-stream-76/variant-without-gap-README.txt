K-UI Toy Commander Test 76 without change 5 - one switch, for comparison

This is the Test 76 reader you ran today (report 66-10) with one of its
six changes switched off and nothing else different: change 5, "the
search for the start of each block goes through the cache". Same source,
same build label.

WHY
Your Test 76 run was the best so far: 1,942 of 1,968 frames in the
second movie, the first movie whole. One number got worse. The reader
had to begin the card's stream again 658 times (Test 75: 478), 640 of
them because it met a byte between two blocks that is neither an idle
byte nor a block's start. Each restart costs about 2 thousandths of a
second and some of the wire's time. Change 5 is the only one of the six
that touches how those bytes are read, and your report shows no gain
from it (the reader's loop over a block took 33.7 millionths of a second
against 33.1 before). So it is the first suspect.

WHAT THIS RUN SAYS
- Row S, the fourth value (LF), and W START. If they fall back toward
  Test 75's 1CC (460) and 1DE (478), change 5 is the cause and comes
  out for good. If they stay near 280 (640) and 292 (658), it is not
  change 5, and the cause is in the timing, which I then look at.
- TF: whether the piece of the second movie that lost 24 frames
  (37.6 to 42.9 seconds: the eighth pair of digits, 89 in your report)
  is whole (A1 or A2) this time. If the hiccup comes back in the same
  place, it belongs to that part of the movie. If it moves or is gone,
  it was a matter of narrow timing.
- Everything else should read as in report 66-10.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed file.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

RUN
Exactly as before: cold boot the K-UI CD, tap B during the countdown,
Card Tools, your raw Toy Commander GDI, Options, Advanced, Tests,
"66 SCI movie playback", CD Music and virtual VMU Off. Let both movies
play without skipping. Then hold both triggers and A+B+X+Y for about
three seconds, and after the save message press Start.

Send the newest file in /reports/toy/ whose name starts with 66. The
text inside says TEST 76 and BUILD 7fe7e83c2082, as the main module's
does. The last value of the PIPE row tells them apart: BD for this
module, FD for the Test 76 you ran.

To go back to Test 76 as delivered, copy its file back from its package.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 76 built with TOY_WIRE_GAP=0, build 7fe7e83c2082, payload CRC32
  C50A20B9. Replaces the Test 76 module at the same path.

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the two modules and this README.
