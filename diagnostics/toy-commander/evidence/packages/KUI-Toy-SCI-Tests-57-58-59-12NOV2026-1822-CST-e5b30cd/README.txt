K-UI Toy Commander SCI tests

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open the usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 57, 58, 59 in that order. Keep normal display settings.
   Set CD Music and virtual VMU to Off.
3. Let both movies play from the beginning. Note when the second movie
   first breaks up and whether either movie ends early.
4. Hold both triggers and A+B+X+Y for about three seconds after each run.
   After the save message, press Start to reboot. Send 57.txt, 58.txt, 59.txt from
   /reports/toy/, or the newest numbered copies. If saving fails,
   photograph both report screens.

57 SCI decode window
Temporarily defers new AVI read submissions at the beginning of
the intro while sufficient data is already buffered. The report
records how often deferral occurred and when ordinary reads resumed.

58 SCI table CRC16
Uses a lookup table to reduce CPU time verifying each 512-byte SD
block. The full SD CRC16 and staged raw-sector checks remain in use.

59 SCI fast copy
Uses faster integer copies for private staging and delivery of
verified movie reads. It leaves the SD CRC16 and raw-sector checks in use.

The exact Test 55 is included as a selectable comparison. OUT counts
the original game's display attempts. SKIP counts advances without
output. The timing report's frame observations are one-based.
INTRO DEFER reports the count of withheld requests, reason normal reads
resumed, and the first and ending frame observations. WHY 1 means the
time/frame limit, 2 means the buffer or next chunk was not ready,
3 means a read was already pending, and 4 means the context or clock
could not be verified. A zero count means the test window did not engage.

Included software

KUI/tools.kui
  Build f51f08a04131, payload CRC32 534D70A3.
KUI/apps/games/retail-toy-test-55.kui
  Build fdb2da073851, payload CRC32 BCACAB28.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-58.kui
  Build e5b30cdd569d, payload CRC32 F549F21E.
KUI/apps/games/retail-toy-test-59.kui
  Build e5b30cdd569d, payload CRC32 32ACB464.

SHA256SUMS.txt verifies the software files and this README.
