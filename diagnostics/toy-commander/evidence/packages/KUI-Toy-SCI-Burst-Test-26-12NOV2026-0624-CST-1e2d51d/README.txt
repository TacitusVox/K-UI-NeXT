K-UI Toy Commander SCI burst test 26

Copy the KUI folder in this ZIP onto the SCI SD card, merging folders and
replacing the included two files. Keep the K-UI CD in the console.

1. Cold-boot the K-UI CD, tap B during the countdown, and open Card Tools.
2. Select the original raw2352 Toy Commander GDI in Games. Open Options,
   Advanced, Tests, and select 26 SCI burst movie reader.
3. Let the videos play. Note whether the intro stays smooth, ends normally
   and reaches gameplay, or stalls or exits early.
4. Hold both triggers and A+B+X+Y for about three seconds to save a report.
   After REPORT SAVED TO SD, release the buttons and press START to reboot.
   Reports are /reports/toy/26.txt, 26-2.txt, and so on.

This selection uses Background New (X), CD music Off and Virtual VMU Off
to isolate the movie data reader. The report records the reader's actual
burst activity. If the report still
says pending after a freeze, send the last screen and what you saw.

This ZIP replaces KUI/tools.kui and adds or replaces the Test 26 reader.
The existing boot.kui/runtime.kui, game files, other readers, and saves
are retained. No image conversion is needed.

Build inventory

KUI/tools.kui  b8402043053c  6DCA347F
KUI/apps/games/retail-toy-test-26.kui  1e2d51de054a  DD949789
