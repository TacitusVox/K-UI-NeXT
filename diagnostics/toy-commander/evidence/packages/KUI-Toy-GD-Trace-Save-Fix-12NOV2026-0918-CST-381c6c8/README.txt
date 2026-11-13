K-UI Toy Commander physical GD-ROM trace save fix

This manual-copy ZIP requires the paired 09:00 CST Toy Commander trace package
(build 3336f96) already installed. Power the Dreamcast off and merge KUI onto
the SCI SD card. Cold-boot the K-UI CD and tap B for Card Tools.

Only run the physical comparison if another optical timing capture is needed.
Insert the original Toy Commander disc. In Disc Tools > GD Play press D-pad
Right. Use 60 Hz. After both movies, hold A+B+X+Y+Start at the title screen.
When REPORT SAVED TO SD appears, power off. The newest /reports/toy/30*.txt
contains the capture. With the old placeholder still present, expect 30-2.txt.

If the save screen shows a failure, photograph the whole screen before powering
off. It displays the SD failure stage and all 13 timing rows. The left four
columns are the bumper and the right four are the intro. Within each group,
columns are requested sectors, acknowledged sectors, maximum accepted request
latency in ms and maximum acknowledgment gap in ms. Numbers are hexadecimal.
Row 0 starts with the first post-header request. Rows 0 through B are 500 ms
each. Row C is everything from 6 seconds onward.

Changed
KUI/apps/disc/native-toy-trace.kui  381c6c8ee270  payload CRC32 FC742962

Retained from the paired package (already installed)
KUI/tools.kui  c0022192fb5f  SHA-256 9ab088c2fbc50062237b0a62fe06e22643d26c805aad7e735400ce85149f9a17
KUI/apps/games/retail-toy-test-29.kui  3336f9682d6e  SHA-256 1d2a0b399c8ddcc3bbf08e23c8e2d7cb64aa18a645cc446b309821c4eaf50e96
