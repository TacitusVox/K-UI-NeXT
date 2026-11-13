K-UI Toy Commander Test 37 — SCI slow retry

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Select your usual original raw Toy Commander GDI in Games. Open
   Options > Advanced > Tests and run 37 SCI slow retry. Use your normal
   display settings. CD Music and virtual VMU are off for this test.
3. Let both movies run. Note whether each finishes. If one stalls or
   skips, note the last visible scene and what the audio does.
4. At the title screen or after an early skip to the game menu, hold
   both triggers and A+B+X+Y for about three seconds. When REPORT SAVED
   TO SD appears, release the buttons and press Start to reboot. Send
   /reports/toy/37.txt or the newest numbered repeat.

If report saving fails, capture both alternating pages. If a hard freeze
prevents capture, send the last visible scene and audio behavior.

Test 37 keeps Test 36's nonblocking movie service and its in-RAM DMA
priority patch for the verified game executable. The GDI stays untouched.
The reader uses 320 extra bytes below IP.BIN for the retry code and keeps
its existing 352-byte guarded stack size.
During ongoing movie reads after the initial fill, an incomplete receive
with a hardware overrun gets one 6.25 MHz DMA retry of the same SD block.
That retry receives the entire block and checks its CRC16 before delivery.
Normal receives use 12.5 MHz. The existing fallback and retry limit apply
if the slower retry fails.

ID1 indicates the exact executable was admitted. Per-movie CUT counts
incomplete receives, ORER counts those with a hardware overrun in eligible
reads, SLOW counts actual slow DMA starts, and OK counts slow reads that
passed CRC. IDLEMAX is the longest interval between observed unarmed
visits for the same movie request. Times are hex at 781,250 ticks/second.

Included files

KUI/tools.kui
  Card Tools 354bfafb6ef4, payload CRC32 D49FEDB8.

KUI/apps/games/retail-toy-test-37.kui
  Test 37 SCI reader 4520d99dd167, payload CRC32 1E18FC2E.

SHA256SUMS.txt verifies both modules and this README.
