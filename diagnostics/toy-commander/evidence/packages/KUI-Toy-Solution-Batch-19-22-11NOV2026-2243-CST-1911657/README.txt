K-UI Toy Commander solution batch: Tests 19-22

INSTALL
Copy the KUI folder into the root of your SCI SD card and overwrite the
included files. Safely eject. Power-cycle with the K-UI CD inserted, pause
its countdown, select SCI and open Card Tools. Its build must be
bb58959f9425. Enter Games, select Toy Commander and choose Original if a
version chooser appears. At the launch screen choose Y Options > Advanced
> Developer tests. Tests 19-22 replace the old test rows in this menu.

RUN
Run each once, choosing 60 Hz in Toy Commander. Let both movies play without
skipping. Each test automatically uses New (X), CD music Off and virtual
VMU Off; no manual settings changes or image conversion are needed.
All four reader reports must show build 19116570a5a0.

19  1 MiB read-ahead
    Larger continuous ring to absorb bitrate peaks; strict SD recovery.
20  Contention-aware DMA
    Original 192 KiB arena. Defer speculative receive starts during active
    graphics/G2 DMA; bounded programmed-I/O fallback for urgent reads.
21  Mode 1 verified recovery
    Original 192 KiB arena. Resume interrupted reception; quarantine the
    complete raw sector until its own Mode1 address and EDC checksum pass.
22  Combined reader
    Larger ring, contention guard and independently checked recovery.

Tests 21/22 reserve 4 KiB of the arena for sector quarantine. They require
original raw2352 Mode1 sectors with their stored EDC. The source game,
movie clock, frame-lag cutoff and audio are not patched.

SAVE THE TEXT REPORT
After the second movie finishes or exits to the menu, hold A+B+X+Y and both
triggers fully for about three seconds. Do not hold Start. Wait for
REPORT SAVED TO SD, release the controls, then press Start to reboot through
the boot ROM to the inserted K-UI CD.

Upload /reports/toy/19.txt, 20.txt, 21.txt and 22.txt together, and describe
which looked best and which completed. Repeat runs preserve earlier files
as 19-2.txt etc. Pages still cycle every five seconds, but photos are not
needed when the text report saves. If a hard freeze prevents the gesture,
note the test number and approximate point; run the remaining tests.

WHAT THIS BATCH MEASURES
Request-by-request cache readiness, foreground/SCI-IRQ/controller-TICK
costs, losses and restarts, verified recovery, DMA-busy observations, and
the final twelve intro samples with lead, cost and player lag. These now
fit in one 3072-byte numbered report per run.

Test 18 reached the same intro cutoff as Test 2. It did read ahead, but also
recorded 5,294 interrupted receptions. The original player aborts above
60 frames of lag; Test 18 recorded 61. These tests target the delivery and
CPU costs that can create that lag, rather than disabling the cutoff.

Host validation covers production SCI/reader integration, malicious and
corrupted sectors, all six report sectors, exact original-startup launch
admission, and linked memory/stack bounds. These checks do not establish
smooth console playback; all four are hardware candidates.

CHANGED SOFTWARE
KUI/tools.kui                           bb58959f9425  CRC32 1015F0A2
KUI/apps/games/retail-toy-test-19.kui     19116570a5a0  CRC32 0573C3D2
KUI/apps/games/retail-toy-test-20.kui     19116570a5a0  CRC32 ED6BDD59
KUI/apps/games/retail-toy-test-21.kui     19116570a5a0  CRC32 B97F2721
KUI/apps/games/retail-toy-test-22.kui     19116570a5a0  CRC32 B4B6F522

All other installed software, games, saves and preferences are retained.
No updater/KUP, WinCE replacement, game data or VMU image is included.
Old test modules may remain on the card; this menu does not select them.
