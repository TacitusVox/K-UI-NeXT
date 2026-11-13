K-UI Toy Commander cutoff tests 16 and 17

Copy the KUI folder into the root of your SCI SD card and overwrite the
existing KUI/tools.kui. Safely eject. Power-cycle with the K-UI CD in the
drive, pause the countdown, select SCI, and open Card tools. Check the Card
Tools build against the inventory below. Open Games, choose the original
raw Toy Commander GDI, then Options, Advanced, Tests.

Run test 16 first, then test 17. Both use SCI New (X), CD Music Off, and
Virtual VMU Off. They use the ordinary demand reader and the same verified
mid-movie RAM window that helped in test 8. Both stage a second window at
LBAs 473498 through 473537. Test 16 reads that second window from SD during
playback. Test 17 serves it from RAM where the game requests those blocks.

Let both movies run without skipping. Note whether each reached gameplay
and whether it played smoothly. Hold A+B+X+Y and both triggers for about
three seconds. After REPORT SAVED TO SD, press START to reboot, pause the
CD countdown and select Card tools again. Upload 16.txt and 17.txt from
/reports/toy/. If a name is already in use, the next report gets a suffix.

If a stuck read prevents saving, capture the terminal trace page on screen.
RAM hit counters count 512-byte SD blocks. A zero late-hit count on test 17
means that run did not exercise the proposed change. The byte witness checks
the actual game destination at LBAs 473517 and 473518 when the game completes
reads covering those sectors. Witness mask bits 0 and 1 identify which of
those sectors was checked. The prior raw tests stopped near the second
window. Visibly complete playback into gameplay is the outcome that matters.

Build inventory (file, build, payload CRC32)

KUI/tools.kui  6f344cc7fde5  3C8F7DE7
KUI/apps/games/retail-toy-test-16.kui  4c62921b240b  6C6419F5
KUI/apps/games/retail-toy-test-17.kui  4c62921b240b  FF9E67DD
