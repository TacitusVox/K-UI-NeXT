K-UI Toy Commander Test 46 — SCI first-cut PIO

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Select Options,
   Advanced, Tests, then 46 SCI first-cut PIO. Use your normal display
   settings. CD Music and virtual VMU are off for this test.
3. Let both movies play. Note whether they finish in sync and where any
   pause or early exit occurs.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/46.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the console resets, note the last screen and retain the
   last-launch-a.txt and last-launch-b.txt records in /KUI/apps/games/.

Test 46 keeps Test 44's playback scheduling. Once an exact verified movie
has started and passed its opening fill, a fast receive DMA overrun sends
that same SD block directly to the existing checked PIO recovery window.
It rereads the block from the card, keeps the resulting stream open for
up to two more sequential checked blocks, then returns to DMA. This
removes the half-speed DMA attempt between the cut and PIO recovery.

The report's RDY, WGD and NOGD columns count requests at the decoder
probe, including unready chunks with or without a matching pending GD
read at the sample. PIO and OK count checked-window attempts and full
CRC16 passes for each movie. FFFFF843 is the game's audio/video lag exit.

Included files

KUI/tools.kui
  Card Tools f6b701813b46, payload CRC32 42E990E1.

KUI/apps/games/retail-toy-test-46.kui
  Test 46 SCI reader d87cf2073493, payload CRC32 FCFCDA46.

SHA256SUMS.txt verifies both modules and this README.
