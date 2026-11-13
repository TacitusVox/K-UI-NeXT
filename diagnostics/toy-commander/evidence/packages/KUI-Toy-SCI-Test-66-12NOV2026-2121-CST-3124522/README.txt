K-UI Toy Commander SCI Test 66

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 66 SCI movie playback. Use your normal display settings.
   Set CD Music and virtual VMU to Off.
3. Let both movies play from the beginning. After they finish or drop
   out, hold both triggers and A+B+X+Y for about three seconds. After
   the save message, press Start to reboot. Send 66.txt from /reports/toy/
   or the newest numbered copy. If saving fails, photograph both pages.

Test 66 drives its private verified movie buffer through the established
native low reader, including its original retries and bounded scheduled
refill. The game's request destination, count, audio clock, display mode,
lag limit and movie sequence retain their original behavior.

The report records SD block count, programmed reads, receive overruns and
SD CRC failures. PIPE B and I count read-ahead window initializations.
CALLS and IRQ count foreground and interrupt visits to the native pipe.
FAIL sums failed game reads. AHEAD B and I count sectors delivered by the
verified buffer, and HIT counts all corresponding completed requests.
EDC counts verified raw sectors. STOP counts producer abandonments.
All report values are hexadecimal.

The included 57 and physical 63 are the original comparison modules.
Test 66 is the single new console test. Console playback remains
unverified. The public handoff branch contains the recovered reports,
source histories, compiled modules and host evidence.

https://github.com/TacitusVox/K-UI-NeXT/tree/codex/toy-commander-handoff

Included software

KUI/tools.kui
  Build 09c9a4b686d4, payload CRC32 8111E794.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-66.kui
  Build 3124522ea906, payload CRC32 559D89AB.
KUI/apps/disc/native-toy-trace.kui
  Build 5034eb26898d, payload CRC32 408BFACE.

SHA256SUMS.txt verifies the software files and this README.
