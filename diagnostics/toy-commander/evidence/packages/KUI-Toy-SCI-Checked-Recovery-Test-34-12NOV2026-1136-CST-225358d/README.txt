K-UI Toy Commander Test 34 — SCI checked recovery

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder on the SCI SD card. Boot the K-UI CD and tap B during the
   countdown to open Card Tools.
2. In Games, select your original raw Toy Commander GDI. Open Options,
   Advanced, Tests and run 34 SCI checked recovery. Use your usual
   display settings. This test uses SCI with CD Music and virtual VMU off.
3. Let both opening movies run. Note whether each makes it past the
   first freeze, whether playback stays smooth, and any audio changes.
4. At the title screen or after an early movie exit to the game menu,
   hold both triggers and A+B+X+Y for about three seconds. After REPORT
   SAVED TO SD, release the buttons and press Start to reboot. Send
   /reports/toy/34.txt or the newest numbered repeat.

If saving fails, capture both alternating report pages. If a hard freeze
prevents capture, note the last scene and whether audio continued.

The test keeps Test 33's nonblocking SDK movie polls. After the first
incomplete SCI DMA in a post-opening movie read, the reader restarts the
card stream at the same physical block and reads that block by programmed
I/O. CRC verification still precedes delivery to the game. DMA resumes on
the next block. This uses more CPU for the rescued block and avoids a
second attempt at the same DMA speed. Hardware playback will show the
effect on movie refill gaps.

Report 34 records transport events for each movie after its opening fill.
REC lists incomplete DMA receptions, CMD18 starts, and programmed reads
for the bumper followed by the intro. The SD row has run-wide totals.
EXEC CHECK N SUM MAX gives the count, total and maximum for EXEC and then
CHECK. GD and BG show foreground and background time. ID1 identifies the
verified original executable. Values are hex at 781,250 ticks per second.

Included files

KUI/tools.kui
  Card Tools 33096173765e, payload CRC32 EBD8B6C3.
  Use the existing base K-UI CD and runtime.

KUI/apps/games/retail-toy-test-34.kui
  Test 34 SCI reader 225358d19816, payload CRC32 54FAB733.
  Test 33 can be selected separately using the previously installed module.
  Exact routes for Tests 26–33 were checked with this Card Tools build.

SHA256SUMS.txt verifies these copy files and the README.
