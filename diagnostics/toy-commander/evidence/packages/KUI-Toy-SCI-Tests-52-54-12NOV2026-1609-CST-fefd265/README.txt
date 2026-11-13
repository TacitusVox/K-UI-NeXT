K-UI Toy Commander SCI Tests 52–54

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. Go to Options,
   Advanced, Tests. Run 54 SCI intro buffer, 52 SCI faster EDC, then
   53 SCI faster CRC16. Keep your normal display settings. Set CD Music
   and virtual VMU to Off for these tests.
3. Let both movies run from the beginning. Note how long the intro stays
   fluid before it starts skipping, and whether either movie ends early.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. Wait for the save message, then
   press Start to reboot. Send /reports/toy/54.txt, /reports/toy/52.txt
   and /reports/toy/53.txt, or the newest numbered copies.
5. If a report fails to save, photograph both report screens. If the
   console resets before you can save, note the last screen and keep
   last-launch-a.txt and last-launch-b.txt from /KUI/apps/games/.

Test 54 increases the game's intro buffer from 1 MiB to 2 MiB when the
game's heap has room. Its normal opening fill is three quarters of the
buffer, so it reads about 768 KiB more before the audio clock starts.
Expect a longer pause before the intro starts when the larger buffer
is selected. Bit 0x10 in INTRO FLG confirms the larger size was requested.

Test 52 verifies staged raw Mode 1 sectors four bytes at a time. The full
EDC and disc-address checks still run before delivering a request.

Test 53 reduces work in the SD block checksum loop. Every SD block still
gets its full CRC16 check. The original staged sector checks also run.

Each test changes one behavior from Test 51. The exact published
Test 51 is included as a selectable comparison.

OUT is the game's number of display-callback attempts. SKIP counts
frames advanced without output. FFFFF843 is the game's audio/video lag
exit. WK counts SD resumes started by a controller reply.

Included software

KUI/tools.kui
  Build eabe4c9cf2e3, payload CRC32 D9FF6088.
KUI/apps/games/retail-toy-test-51.kui
  Build 942509e6fe6e, payload CRC32 415280A5.
KUI/apps/games/retail-toy-test-52.kui
  Build 8e732f81efd0, payload CRC32 6A751C28.
KUI/apps/games/retail-toy-test-53.kui
  Build aa3ee4a8f9a5, payload CRC32 E2802318.
KUI/apps/games/retail-toy-test-54.kui
  Build fefd26538deb, payload CRC32 E0761AA0.

SHA256SUMS.txt verifies the software files and this README.
