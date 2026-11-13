K-UI Toy Commander Test 51

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests and select 51 SCI wake resume. Keep your normal
   display settings. CD Music and virtual VMU are off in this test.
3. Let both movies play. Note where any slideshow begins or the movie
   exits early. At the title screen or after an early exit, hold both
   triggers and A+B+X+Y for about three seconds. Wait for the save
   message, then press Start to reboot. Send /reports/toy/51.txt or its
   newest numbered copy.
4. If the report fails to save, photograph both report screens. If the
   console resets before you can save, note the last screen and keep
   last-launch-a.txt and last-launch-b.txt from /KUI/apps/games/.

Test 51 lets a verified controller reply resume a paused SD transfer
when the same card block is still needed and the SCI DMA channel is
idle. It resumes the existing card command at the saved byte offset.
The full raw sector still passes its EDC and disc-address checks
before delivery. The foreground refill budget is the same as Test 50.

WK counts resumes triggered by the controller callback. OUT counts
the game's display-callback attempts. SKIP counts frames advanced
without output. FFFFF843 is the game's audio/video lag exit.
EXT counts extra SD blocks fetched by staged refill visits, RSM counts
recovered blocks in fully verified requests, and RET counts requests
that required fallback.

Test 50 is included as the exact comparison build. Its report is separate.

Included files

KUI/tools.kui
  Card Tools d7245817bed4, payload CRC32 CD82E6AF.
KUI/apps/games/retail-toy-test-50.kui
  Test 50 reader 1ad2afff629c, payload CRC32 7160B3BD.
KUI/apps/games/retail-toy-test-51.kui
  Test 51 reader 942509e6fe6e, payload CRC32 415280A5.

SHA256SUMS.txt verifies the software files and this README.
