K-UI Toy Commander Test 35 — SCI RAM window

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B during
   the countdown to open Card Tools.
2. In Games, select the same original raw Toy Commander GDI. Open Options,
   Advanced, Tests and run 35 SCI RAM window. Use your usual display
   settings. CD Music and virtual VMU are off for this test.
3. Let both movies run. For the first movie, note whether the early freeze
   still happens, where it happens, and whether playback continues.
4. At the title screen or after an early movie exit to the game menu, hold
   both triggers and A+B+X+Y for about three seconds. When REPORT SAVED TO
   SD appears, release the buttons and press Start to reboot. Send
   /reports/toy/35.txt or the newest numbered repeat.

If report saving fails, capture both alternating report pages. If a hard
freeze prevents capture, note the last visible scene and what audio did.

Tests 33 and 34 both logged their longest first-movie completion gap at
disc sector 0x69263. Before the game starts, Test 35 tries to fill the
existing reader cache with 80 raw sectors around that point. The stage
checks the SD data and the Mode 1 sector EDC. It serves a fully covered
request of up to 24 sectors from RAM on its first delivery visit. The
rest of playback uses Test 33's nonblocking-poll SCI path.

The SD report distinguishes the gap between completed requests from the
service time of the request at 0x69263. TARGET 69263 shows seed size in
physical SD blocks, target requests completed entirely from RAM, target
request service time, and the completion gap to the target. RAM REQ
shows all fully RAM-served requests and their first and last addresses.
Times are hex at 781,250 ticks per second. If TARGET RAM is zero, the
known slow request did not run entirely from RAM, so this comparison
cannot isolate the cause of that gap.

Included files

KUI/tools.kui
  Card Tools a4c5b883208a, payload CRC32 FD3B6F36.
  Cold CD Card Tools add-on for the installed K-UI base.

KUI/apps/games/retail-toy-test-35.kui
  Test 35 SCI reader f1dc87f6bed5, payload CRC32 B84BDC61.
  Exact previously installed Tests 26–34 were also checked with this menu.

SHA256SUMS.txt verifies the copy files and this README.
