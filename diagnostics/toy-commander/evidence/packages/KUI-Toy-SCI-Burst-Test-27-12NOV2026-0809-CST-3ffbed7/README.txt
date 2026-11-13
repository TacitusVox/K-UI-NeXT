K-UI Toy Commander SCI burst test 27

Copy the KUI folder in this ZIP onto the SCI SD card, merging folders and
copying the two included files. Keep the K-UI CD in the console.

1. Cold-boot the K-UI CD, tap B during the countdown, and open Card Tools.
2. Select the original raw2352 Toy Commander GDI in Games. Open Options,
   Advanced, Tests, and select 27 SCI burst quiet vs live.
3. Wait through SCI QUIET MOVIE SAMPLE and let both movies run. Note where
   the second movie first stutters or exits and whether the menu responds.
4. Hold both triggers and A+B+X+Y for about three seconds to save the report.
   After REPORT SAVED TO SD, release the buttons and press START to reboot.
   Reports are /reports/toy/27.txt, 27-2.txt, and so on.

The report compares the quiet pre-launch sample with reads from the same
card ranges during playback. If startup stops at SCI QUIET MOVIE SAMPLE
or shows a failure screen before playback, photograph the final screen.
If the report stays pending after playback, note the last screen and
what happened.
This selection uses Background New (X), CD music Off and Virtual VMU Off.
Use the same SCI card, original GDI and 60 Hz display setting as Test 26.

This ZIP replaces KUI/tools.kui and adds KUI/apps/games/retail-toy-test-27.kui.
Your installed Test 26 stays selectable. This ZIP leaves the existing
boot.kui/runtime.kui, game files, other readers, saves and Test 26 reports intact.

Build inventory

KUI/tools.kui  f8c8ac6c7591  FF643B6E
KUI/apps/games/retail-toy-test-27.kui  3ffbed71e1cc  ACD4B97D
