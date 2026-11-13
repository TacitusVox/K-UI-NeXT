K-UI Toy Commander SCI Test 64

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 64 SCI movie read ahead. Use your normal display settings.
   Set CD Music and virtual VMU to Off.
3. Let both movies play from the beginning. After they finish or drop
   out, hold both triggers and A+B+X+Y for about three seconds. After
   the save message, press Start to reboot. Send 64.txt from /reports/toy/
   or the newest numbered copy. If saving fails, photograph both pages.

Test 64 preserves the verified movie read-ahead window during the game's
first GDFS execution call immediately after it accepts a read. The
original timing and request sizes are retained. The archive includes
the same Test 57 comparison and the same physical Test 63 from the prior
package. There is no need to rerun the physical control.

Both report pages and the saved report show hexadecimal counters. OPEN
B and I count successful window initializations for the two movies.
KICK counts bounded setup calls that kept an incomplete window alive.
WAIT counts other positive-wait calls on incomplete windows. FAIL is
the sum of failed movie read commands. AHEAD reports warm-hit sectors
for each movie, warm-hit request count, total verified producer sectors,
and producer errors that trigger fallback. Frame timing is unchanged.

Included software

KUI/tools.kui
  Build e16e724c9b65, payload CRC32 A7FB446A.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-64.kui
  Build 4f213ec141bf, payload CRC32 02C30A52.
KUI/apps/disc/native-toy-trace.kui
  Build 5034eb26898d, payload CRC32 408BFACE.

SHA256SUMS.txt verifies the software files and this README.
