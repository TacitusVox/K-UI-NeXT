K-UI Toy Commander Test 49

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests and select 49 SCI resume cadence. Keep your normal
   display settings. CD Music and virtual VMU are off in this test.
3. Let both movies play. Note if each finishes in sync, or which movie
   stalls or exits. At the title screen or after an early exit, hold
   both triggers and A+B+X+Y for about three seconds. Wait for the
   save message, then press Start to reboot. Send /reports/toy/49.txt
   or the newest numbered copy if you repeat the test.
4. If a report fails to save, capture both report screens. If the
   console resets before you can save, note the last screen and retain
   the last-launch-a.txt and last-launch-b.txt records in /KUI/apps/games/.

Test 49 retains Test 48's low reader and verified same-command
recovery. It restores bounded multi-block refill while the recovered
data is being staged. A recognized foreground EXEC callback can
deliver up to three SD blocks and checks a 1.5 ms budget before more
work. The reader checks each raw sector and holds the request until
validation passes. It retains the full checked reread on failure.

EXT counts extra SD blocks delivered by these staged refill visits.
RSM counts recovered blocks in fully verified requests. RET counts
requests that required fallback. OUT and SKIP sample the game's
output-callback and no-output advance counters when a movie exits.
OUT measures callback attempts. It does not measure hardware scanout.
FFFFF843 is the game's audio/video lag exit.

Tests 47 and 48 remain available for comparison. Their reader modules
are the exact previously supplied builds and report to separate files.

Included files

KUI/tools.kui
  Card Tools df1d026a1488, payload CRC32 0E0C07B2.
KUI/apps/games/retail-toy-test-47.kui
  Test 47 reader e28a31fa0051, payload CRC32 F2D837F7.
KUI/apps/games/retail-toy-test-48.kui
  Test 48 reader 1c90d147e936, payload CRC32 878304B4.
KUI/apps/games/retail-toy-test-49.kui
  Test 49 reader befebdbf446f, payload CRC32 6D231579.

SHA256SUMS.txt verifies the software files and this README.
