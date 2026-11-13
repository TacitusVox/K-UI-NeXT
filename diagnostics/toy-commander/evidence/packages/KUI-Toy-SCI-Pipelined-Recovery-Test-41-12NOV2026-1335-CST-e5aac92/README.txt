K-UI Toy Commander Test 41 — SCI pipelined recovery

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Select Options,
   Advanced, Tests, then 41 SCI pipelined recovery. Use your normal
   display settings. CD Music and virtual VMU are off for the test.
3. Let both movies play normally. Note whether each finishes in sync
   and where any visible pause or early exit happens.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/41.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, send the last visible scene.

Test 41 uses a pipelined serial read for the three-block recovery window.
It prepares SCI once for the 514 wire bytes of each SD block, including
both CRC bytes, and starts clocking the next byte after draining the
previous byte from the receive register. K-UI checks the full block CRC
before delivery. The report counts window PIO reads and CRC-accepted
blocks, and records foreground/background timing and movie exit codes.
FFFFF843 is the game's audio/video lag exit.
The pipelined code uses 288 more bytes below IP.BIN and keeps the existing
352-byte guarded service stack.

Included files

KUI/tools.kui
  Card Tools 9d4535c53b97, payload CRC32 B36F357B.

KUI/apps/games/retail-toy-test-41.kui
  Test 41 SCI reader e5aac92d707f, payload CRC32 53B270AE.

SHA256SUMS.txt verifies both modules and this README.
