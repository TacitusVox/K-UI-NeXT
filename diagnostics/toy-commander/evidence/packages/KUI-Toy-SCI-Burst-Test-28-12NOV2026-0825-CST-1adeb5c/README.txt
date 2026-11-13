K-UI Toy Commander SCI burst test 28

Copy the KUI folder in this ZIP onto the SCI SD card, merging folders and
copying the two included files. Keep the K-UI CD in the console.

1. Cold-boot the K-UI CD, tap B during the countdown, and open Card Tools.
2. Select the original raw2352 Toy Commander GDI in Games. Open Options,
   Advanced, Tests, and select 28 SCI burst speed recovery.
3. Wait through SCI QUIET MOVIE SAMPLE and let both movies run. Note whether
   the first movie audio repeats around the high-motion section at 3 seconds,
   and where the second movie exits or whether it finishes.
4. Hold both triggers and A+B+X+Y for about three seconds to save the report.
   After REPORT SAVED TO SD, release the buttons and press START to reboot.
   Reports are /reports/toy/28.txt, 28-2.txt, and so on.

After a short fast burst, the loader makes two payload attempts at 6.25 MHz
and returns to 12.5 MHz. The pre-launch comparison runs at full speed.
The report records which reads complete at each speed. Send 28.txt after one
run. If startup stops at SCI QUIET MOVIE SAMPLE or shows a failure screen
before playback, photograph the final screen.
If the report stays pending after playback, note the last screen and
what happened.
This selection uses Background New (X), CD music Off and Virtual VMU Off.
Use the same SCI card, original GDI and 60 Hz display setting as Test 27.

This ZIP replaces KUI/tools.kui and adds KUI/apps/games/retail-toy-test-28.kui.
Tests 26 and 27 remain selectable with their previously installed readers.
The inventory below lists the two files to copy.

Build inventory

KUI/tools.kui  f6848517568d  AF0108CA
KUI/apps/games/retail-toy-test-28.kui  1adeb5c2b65c  503900B7
