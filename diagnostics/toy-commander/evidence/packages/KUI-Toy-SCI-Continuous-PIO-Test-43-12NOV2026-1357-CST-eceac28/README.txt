K-UI Toy Commander Test 43 — SCI continuous PIO

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Select Options,
   Advanced, Tests, then 43 SCI continuous PIO. Use your normal display
   settings. CD Music and virtual VMU are off for this test.
3. Let both movies play normally. Note whether each finishes in sync
   and where any visible pause or early exit happens.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/43.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, note the last visible scene.

The steady-state movie reader now receives full SD blocks with programmed
I/O while the card's sequential CMD18 read stays open. Each block must pass
CRC16 before the game sees it. This removes the normal fast-DMA and slow-DMA
attempts from that movie path. The scheduled and one-pass EXEC callbacks
can service up to three received or delivered blocks per visit. They check
the 1.5 ms budget before starting further work. Status checks do no I/O. The normal opening fill and stock graphics DMA
priority are retained.

The report's CUT PIO OK rows give incomplete DMA receptions, direct PIO
attempts and full CRC passes for each movie. SLICES and BLKS measure extra
service visits and blocks delivered during them. The original
exit record is retained. FFFFF843 is the game's audio/video lag exit.

Included files

KUI/tools.kui
  Card Tools 2f8c0445c1f7, payload CRC32 D5D49E3B.

KUI/apps/games/retail-toy-test-43.kui
  Test 43 SCI reader eceac2869f9e, payload CRC32 05034B71.

SHA256SUMS.txt verifies both modules and this README.
