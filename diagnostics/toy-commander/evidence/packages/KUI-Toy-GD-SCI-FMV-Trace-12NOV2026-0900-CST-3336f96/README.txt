K-UI Toy Commander GD-ROM and SCI FMV trace

With the Dreamcast powered off, merge this ZIP's KUI folder onto the SCI SD
card. Keep the existing KUI directory and its contents. The ZIP replaces
KUI/tools.kui and adds the two diagnostic modules in the build inventory.
Cold-boot the K-UI CD and tap B during the countdown to enter Card Tools.

Use the original Toy Commander GD-ROM and the raw 2352-byte GDI rip of that
same disc. Use the same 60 Hz display setting and controller configuration
for both runs. Let the first two movies play through, and note when audio
repeats, video stalls, or playback exits early.

SCI image run

In Card Tools, select the raw Toy Commander GDI in Games and open Options,
Advanced, Tests. Select 29 SCI movie trace. The game reads from the SCI SD
card using the Test 28 recovery policy.

After the movies play or the intro exits early, hold both triggers and
A+B+X+Y for about three seconds. Wait for REPORT SAVED TO SD, release the
buttons, and press Start to reboot with the K-UI CD still in the drive.
Send /reports/toy/29.txt or the next numbered repeat.

Physical-disc run

From Card Tools, put the original Toy Commander GD-ROM in the drive. Open
Disc tools, then GD Play, and press D-pad Right (Toy movie trace). The game
reads from the original GD-ROM through the console's firmware.

After both movies, hold A+B+X+Y+Start at the title screen to save the
trace. If the game returns to the title screen, release the buttons and
repeat the combo there. When REPORT SAVED TO SD appears, power off. Send
/reports/toy/30.txt or the next numbered repeat.

If a run shows a refusal or report-save failure, photograph the screen. If
the game locks up and the save shortcut fails, note the last picture and
approximate playback time. A hard lock can strand the report in RAM.

Build inventory
KUI/tools.kui  c0022192fb5f  CRC32 9DAE928D
KUI/apps/games/retail-toy-test-29.kui  3336f9682d6e  CRC32 5077773A
KUI/apps/disc/native-toy-trace.kui  1de96fab36e3  CRC32 C9E98560
