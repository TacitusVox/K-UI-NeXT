K-UI Toy Commander Test 38 — SCI refill slice

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Select your usual original raw Toy Commander GDI. Open Options,
   Advanced, Tests and select 38 SCI refill slice. This is the only Toy
   test in this Card Tools build. Use your normal display settings.
   CD Music and virtual VMU are off for the test.
3. Let both opening movies run. Note whether each finishes. If one
   freezes, skips, or crashes, note the scene and what the audio does.
4. At the title screen or after an early skip, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/38.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, send the last visible scene.

Test 38 keeps Test 37's DMA priority patch and its CRC-checked slower
retry for the same SD block. The new behavior is limited to the exact
game's scheduled EXEC callback during ongoing movie reads. The loader
can start extra bounded service steps within about 1.5 ms of entry and
stops after three checked blocks or a PIO receive. A step already started
can finish beyond the timer threshold. CHECK remains status-only and
the explicit one-pass movie call keeps its earlier service limit.

Per-movie SLICES counts visits that started extra work and BLKS counts
additional checked 512-byte blocks delivered by those steps. CUT counts
incomplete receives, SLOW counts actual half-speed retries, OK counts
those that passed CRC, and IDLE is the longest gap sampled between
unarmed visits for the same request. EXEC and CHECK each list count,
total ticks, and maximum ticks. Timer values use 781,250 ticks/second.

Included files

KUI/tools.kui
  Card Tools e65c2e2af347, payload CRC32 F6F53949.

KUI/apps/games/retail-toy-test-38.kui
  Test 38 SCI reader 8203e8e86dad, payload CRC32 E83FF458.

SHA256SUMS.txt verifies both modules and this README.
