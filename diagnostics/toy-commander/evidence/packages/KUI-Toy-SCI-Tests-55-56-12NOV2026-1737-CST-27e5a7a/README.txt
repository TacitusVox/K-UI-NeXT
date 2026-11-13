K-UI Toy Commander SCI Tests 55–56

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests. Run 56 SCI intro prefill, then 55 SCI movie timing.
   Keep your usual display settings. Set CD Music and virtual VMU to Off.
3. Let both movies run from the beginning. Note how long the intro stays
   fluid and whether either movie ends early.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. Wait for the save message, then
   press Start to reboot. Send /reports/toy/56.txt and /reports/toy/55.txt,
   or their newest numbered copies.
5. If a report fails to save, photograph both report screens. If the
   console resets before you can save, note the last screen and keep
   last-launch-a.txt and last-launch-b.txt from /KUI/apps/games/.

Test 56 raises the second movie's opening fill target from 768 to
960 KiB in its existing 1 MiB allocation. The game caps the fill at
the usable ring capacity, so the additional lead is up to 192 KiB.
Expect a longer pause before the intro. Bit 0x20 in INTRO FLG confirms
the larger fill was requested.

Test 55 keeps the Test 53 reader and records when the movie first waits
for bytes after starting output, how much of its media ring is occupied,
the byte shortfall for the requested chunk, and the first skip and last
output observations. A zero frame means unobserved. Frame values are
one-based. The intro row also samples the buffer at the first observed
skip. FFFFFFFF in a byte field means unavailable.

Both tests retain SD CRC16 and raw sector EDC/address verification.
The exact published Test 53 is included as a selectable comparison.

Included software

KUI/tools.kui
  Build 2e9301147c33, payload CRC32 FAE6EDA8.
KUI/apps/games/retail-toy-test-53.kui
  Build aa3ee4a8f9a5, payload CRC32 E2802318.
KUI/apps/games/retail-toy-test-55.kui
  Build fdb2da073851, payload CRC32 BCACAB28.
KUI/apps/games/retail-toy-test-56.kui
  Build 27e5a7af0985, payload CRC32 9181173A.

SHA256SUMS.txt verifies the software files and this README.
