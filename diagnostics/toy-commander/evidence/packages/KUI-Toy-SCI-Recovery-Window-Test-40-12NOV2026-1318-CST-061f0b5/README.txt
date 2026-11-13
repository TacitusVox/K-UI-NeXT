K-UI Toy Commander Test 40 — SCI recovery window

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Select Options,
   Advanced, Tests, then 40 SCI recovery window. Use your normal display
   settings. CD Music and virtual VMU are off for the test.
3. Let both movies play normally. Note whether each finishes, whether
   audio stays in sync, and where any visible pause happens.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/40.txt
   or its newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, send the last visible scene.

Test 40 keeps Test 39's scheduled refill and movie exit trace. If a
half-speed retry overruns during ongoing playback, the reader uses
programmed I/O (PIO) for up to three sequential SD blocks, starting with
the failed block. It keeps the card read open between consecutive blocks
when possible. The reader checks the full CRC before giving each block
to the game. The short window then returns to DMA.
The reader uses 128 more bytes below IP.BIN and keeps its 352-byte stack.

The report records SD restarts, incomplete receives, recovery window
activity and the original movie exit codes. FFFFF843 is the game's
audio/video lag exit. The original frame lag limit remains in effect.
A good result means both movies play to completion with synchronized
audio, then do so again on a cold boot.

Included files

KUI/tools.kui
  Card Tools 44878b9cb9e4, payload CRC32 CCC5B602.

KUI/apps/games/retail-toy-test-40.kui
  Test 40 SCI reader 061f0b5a4f0b, payload CRC32 72126AE2.

SHA256SUMS.txt verifies both modules and this README.
