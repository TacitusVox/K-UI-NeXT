K-UI Toy Commander Tests 47 and 48

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests and start with 48 SCI stream resume. Use your normal
   display settings. CD Music and virtual VMU are off in these tests.
3. Let both movies play. Note if both finish in sync, or which movie
   stalls or exits. At the title screen or after an early exit, hold
   both triggers and A+B+X+Y for about three seconds. Wait for the
   save message, then press Start to reboot. Send /reports/toy/48.txt.
4. To compare DMA arbitration, cold boot again and run 47 SCI DMA bus
   share with the same settings. Save /reports/toy/47.txt the same way.
   Use the latest numbered report if you repeat either test.
5. If a report fails to save, capture both report screens. If a test
   resets before you can save, note the last screen and retain the
   last-launch-a.txt and last-launch-b.txt records in /KUI/apps/games/.

Test 48 can resume a single-byte SCI receive overrun inside the same
SD read command. The original request stays pending while the reader
verifies all its raw Mode 1 sectors by EDC and expected disc address.
The game receives the cooked data only after the whole request passes.
The reader can reread through the ordinary checked path if necessary.
RSM counts recovered card blocks in successfully verified requests.
RET counts quarantined requests that required fallback.

Test 47 uses Test 44's transport and changes the verified game's
DMA arbitration to round-robin. The original Holly channel 2 burst
configuration is retained. DMA counts armed SCI samples and RR
counts those where the expected round-robin register value was live.

RDY, WGD, and NOGD count video chunk probes. WGD means the chunk
was unavailable with a matching GD read pending. FFFFF843 is the
game's audio/video lag exit. Both clips finishing in sync is the
playback success criterion.

Included files

KUI/tools.kui
  Card Tools 7386d06f30aa, payload CRC32 C321C4D2.
KUI/apps/games/retail-toy-test-47.kui
  Test 47 reader e28a31fa0051, payload CRC32 F2D837F7.
KUI/apps/games/retail-toy-test-48.kui
  Test 48 reader 1c90d147e936, payload CRC32 878304B4.

SHA256SUMS.txt verifies both readers, Card Tools and this README.
