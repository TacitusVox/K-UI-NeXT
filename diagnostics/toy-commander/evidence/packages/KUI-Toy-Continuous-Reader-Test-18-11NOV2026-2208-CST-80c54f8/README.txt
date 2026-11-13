K-UI Toy Commander Test 18: continuous read-ahead

INSTALL
Copy the KUI folder into the root of your SCI SD card and overwrite the two
included files. Safely eject. Power-cycle with the K-UI CD in the drive,
pause its countdown, select SCI, and open Card tools. Card Tools build must
be 6475bd127acf. Go through its Games menu, select your ORIGINAL raw Toy
Commander GDI, then Options > Advanced > Tests.

RUN
Select 18 - Raw continuous read-ahead. The test uses SCI New (X), CD Music
Off and Virtual VMU Off automatically, as the preceding native movie tests
did. Select 60 Hz when Toy asks. Let both movies play without skipping.
The reader's report build must be 80c54f8ddf34.

REPORT
After the intro finishes or returns to the main menu, hold A+B+X+Y and both
triggers for about three seconds. Wait for REPORT SAVED TO SD, then press
START to reboot to the K-UI CD. Upload /reports/toy/18.txt and say whether
each movie was smooth and whether the second one finished. Existing reports
are preserved; repeat runs use 18-2.txt and later suffixes. Diagnostic pages
still cycle every five seconds. If it cannot reach the report, note where
it stopped; no further file verification or image conversion is requested.

WHAT CHANGED
A single producer now puts all checked movie blocks into the existing
192 KiB ring, including urgent reads. Reading ahead continues between game
requests, with retained history for repeated AVI headers. Token waits and
interrupted receives can restart at real GD/controller opportunities;
CRC failures retry the same block without publishing corrupt successors.
The report also observes the movie player's audio/video lag when the intro
stops. It does not change the player or disable its lag limit.

This is a hardware candidate, not a claim of confirmed smooth playback.
Production-code host tests, real SCI/card-model integration, linked memory
and stack checks, and exact Card Tools launch admission passed. The original
Toy startup executable was used for admission and code-guard verification.
No private game files are included.

CHANGED FILES
KUI/tools.kui  build 6475bd127acf  payload CRC32 24F4AFE0
KUI/apps/games/retail-toy-test-18.kui  build 80c54f8ddf34  payload CRC32 9597AA99

All other installed software, games, saves and preferences are retained.
The package contains no updater/KUP or WinCE replacement. Tests 1-17 no
longer appear in this Card Tools test menu; old module files may remain on
the card without being selected.
