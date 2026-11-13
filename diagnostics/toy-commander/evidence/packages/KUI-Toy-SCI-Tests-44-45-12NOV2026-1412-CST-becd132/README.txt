K-UI Toy Commander Tests 44 and 45

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests. Start with 45 SCI bounded DMA. Use your normal
   display settings and let both movies play. CD Music and virtual
   VMU are off in these two tests.
3. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds to save /reports/toy/45.txt. Press
   Start after the save message to return to the K-UI CD.
4. Cold boot again and run 44 SCI sequential PIO with the same settings.
   Save /reports/toy/44.txt in the same way. For repeats, use the newest
   numbered report. If a save fails, capture both report pages.
5. If either test resets before you can save, note which test and the
   last screen you saw. Copy last-launch-a.txt and last-launch-b.txt
   from /KUI/apps/games/ if present. These identify the reader handed off
   by Card Tools. The other test can still be run independently.

Both readers use Test 42's console-booted low-level reader, byte-for-byte
apart from the build ID. The stock movie opening fill runs first. The
changes apply only after the verified original player starts and reaches
the established movie read range.

Test 45 lets the scheduled game callback wait for up to six checked DMA
blocks at a time. It retains the existing PIO fallback and releases the
ordinary reader target after each batch.

Test 44 uses the existing checked PIO routine when the card is between
blocks in an open sequential read. Its recognized foreground callbacks
can receive or deliver up to three blocks per visit and check a 1.5 ms
budget before further work. Seeking and retrying use the established
reader.

RDY counts video chunk requests with a selected chunk available. WGD and
NOGD count unready chunks with or without a matching pending GD read at
the sample. Test 44 ARM counts reads selected by its new PIO policy and
OK counts CRC passes across all PIO windows. Test 45 RUN and BLK count
capped-wait visits and the card blocks they delivered. FFFFF843 is the
game's audio/video lag exit.

Included files

KUI/tools.kui
  Card Tools d5102f34611e, payload CRC32 EF518465.
KUI/apps/games/retail-toy-test-44.kui
  Test 44 reader becd132dfdc9, payload CRC32 6843C3EA.
KUI/apps/games/retail-toy-test-45.kui
  Test 45 reader becd132dfdc9, payload CRC32 EC770CBB.

SHA256SUMS.txt verifies both readers, Card Tools and this README.
