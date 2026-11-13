K-UI Toy Commander Test 32 — SCI async demand

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B during
   the countdown to open Card Tools.
2. In Games, select the original raw 2352-byte Toy Commander GDI. Open
   Options > Advanced > Tests and select 32 SCI async demand.
3. Let both opening movies play through. Note whether the second movie
   starts smoothly, where any slowdown or early exit occurs, what the
   audio does, and whether the game reaches the title screen.
4. At the title screen or after an early return to the game menu, hold
   both triggers and A+B+X+Y for about three seconds. When REPORT SAVED
   TO SD appears, release the buttons and press Start to reboot to the
   K-UI CD. Send /reports/toy/32.txt or the newest numbered repeat.

If saving fails, film both alternating report pages. If the console freezes
before the report, record the movie, the last visible scene, and what the
audio did.

This build moves ongoing movie reads out of routine game status calls.
SCI interrupts deliver verified data, and a controller callback can restart
the receive when it has stopped. Report 32 measures time inside EXEC and
CHECK, gaps with no SCI receive armed, and whether controller wakeups
restart the receive. Compare those measurements with how the movies play.

Included files

KUI/tools.kui
KUI/apps/games/retail-toy-test-32.kui

Builds

Reader f3aaa6e (payload CRC32 09145154)
Card Tools 62a4916 (payload CRC32 E99BCAD9)
