K-UI Toy Commander Test 36 — SCI DMA priority

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. In Games, select the same original raw Toy Commander GDI. Open Options,
   Advanced, Tests and run 36 SCI DMA priority. Use your usual display
   settings. CD Music and virtual VMU are off for this test.
3. Let both movies run. Note whether each finishes and, if one stalls or
   skips, the last visible scene and what the audio does.
4. At the title screen or after an early movie exit to the game menu,
   hold both triggers and A+B+X+Y for about three seconds. When REPORT
   SAVED TO SD appears, release the buttons and press Start to reboot.
   Send /reports/toy/36.txt or the newest numbered repeat.

If report saving fails, capture both alternating report pages. If a hard
freeze prevents capture, send the last visible scene and audio behavior.

Toy Commander's graphics setup programs the shared DMA controller with
graphics transfers ahead of the SCI receive channel. This test uses Test
33's nonblocking SCI reader and changes two constants in the verified
game executable in RAM so SCI receive ranks ahead of graphics. The
change applies throughout this Toy Commander session. The GDI stays
untouched.

The report's ID1 indicates the exact executable was admitted. BMP DMA
and INTRO DMA give counts of the four priority modes sampled during
armed receive service and the number of incomplete receives (CUT).
The report also records sector progress and the largest completion gaps.
Times are hexadecimal at 781,250 ticks per second.

Included files

KUI/tools.kui
  Card Tools 5f93e5244f6b, payload CRC32 847056F0.

KUI/apps/games/retail-toy-test-36.kui
  Test 36 SCI reader 5ecb06066b27, payload CRC32 AA088B65.

SHA256SUMS.txt verifies both modules and this README.
