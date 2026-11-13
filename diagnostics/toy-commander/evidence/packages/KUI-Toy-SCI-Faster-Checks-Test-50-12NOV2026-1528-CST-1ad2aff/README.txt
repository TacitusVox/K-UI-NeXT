K-UI Toy Commander Test 50

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests and select 50 SCI faster checks. Keep your normal
   display settings. CD Music and virtual VMU are off in this test.
3. Let both movies play. Note if they finish in sync, and where any
   visible stutter or early exit starts. At the title screen or after
   an early exit, hold both triggers and A+B+X+Y for about three
   seconds. Wait for the save message, then press Start to reboot.
   Send /reports/toy/50.txt or its newest numbered copy.
4. If a report fails to save, capture both report screens. If a test
   resets before you can save, note the last screen and retain the
   last-launch-a.txt and last-launch-b.txt records in /KUI/apps/games/.

Test 50 speeds up the checksum used to verify staged raw sectors.
It folds two bytes per checksum iteration using a table read through
the CPU cache. Every raw sector still passes its full EDC and disc
address check before the request is delivered. The refill budget and
same-command recovery are the Test 49 design.

OUT counts the original player's display-callback attempts. SKIP counts
its no-output advances. FFFFF843 is the audio/video lag exit.
EXT counts extra SD blocks delivered by staged refill visits, RSM counts
recovered blocks in fully verified requests, and RET counts requests
that required fallback.

Test 49 is included as the exact comparison build. Its report is separate.

Included files

KUI/tools.kui
  Card Tools 7ee0638f0372, payload CRC32 017E8C3C.
KUI/apps/games/retail-toy-test-49.kui
  Test 49 reader befebdbf446f, payload CRC32 6D231579.
KUI/apps/games/retail-toy-test-50.kui
  Test 50 reader 1ad2afff629c, payload CRC32 7160B3BD.

SHA256SUMS.txt verifies the software files and this README.
