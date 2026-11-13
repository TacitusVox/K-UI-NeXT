K-UI Toy Commander Test 42 — SCI graphics priority

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Select Options,
   Advanced, Tests, then 42 SCI graphics priority. Use your normal display
   settings. CD Music and virtual VMU are off for the test.
3. Let both movies play normally. Note whether each finishes in sync
   and where any visible pause or early exit happens.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/42.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, send the last visible scene.

Test 42 uses the Test 41 SCI reader with the game's original graphics
DMA priority. The original priority gives graphics precedence over
serial receive while the game programs graphics DMA.

The verified executable's video chunk call is traced in RAM. Each
movie's CHUNK row counts ready requests, selected chunks still unready
while a matching movie GD read is pending (WAITGD), the same unready
return with no matching pending read (NOGD), and other negative results.
The counts are samples at the decoder call, not elapsed times. The
existing exit record includes the game's frame and audio counters.
FFFFF843 is the game's audio/video lag exit.

Included files

KUI/tools.kui
  Card Tools 24d5ad5c11ff, payload CRC32 5A41B19F.

KUI/apps/games/retail-toy-test-42.kui
  Test 42 SCI reader 5425aca86182, payload CRC32 AB63266B.

SHA256SUMS.txt verifies both modules and this README.
