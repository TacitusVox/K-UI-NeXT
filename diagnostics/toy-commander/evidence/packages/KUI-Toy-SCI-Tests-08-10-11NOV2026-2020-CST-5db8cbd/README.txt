K-UI Toy Commander SCI tests 8–10

Copy the KUI folder into the root of the same SD card used for tests 1–7.
This update provides Card tools and three additional test readers. Boot the
K-UI CD, tap B during countdown, and select Card tools → Games → Toy Commander
→ Options → Advanced → Tests. Choose the original raw GDI. All three tests
run on SCI New (X), with CD Music Off and Virtual VMU Off.

Suggested order: 10, 8, 9.

10  RAM preload control. Verifies and preloads the same two windows as 8
    during startup. Playback serves all reads through the ordinary SCI
    demand reader. The report should show RAM BLOCKS above zero and both
    hit counters at zero.
8   Two RAM windows. Serves up to 40 logical sectors around the repeated
    cutoff at LBA 469080 and 40 around the newer cutoff at LBA 473539.
9   Late RAM window. Serves up to 80 logical sectors around LBA 473539.
    This tests whether the late window alone can change the result.

Let the second video finish or stop. Hold both triggers and A+B+X+Y for
about three seconds to save the report. After REPORT SAVED TO SD, release
the buttons and press START. Leave the K-UI CD in and tap B on the next
countdown to return to Card tools. Send /reports/toy/8.txt, 9.txt and 10.txt,
or the numbered repeats, plus whether the second video reached gameplay.

RAM BLOCKS and the hit counts use 512-byte physical SD blocks. FIRST and
LAST use logical disc LBAs. Each served window is staged before game start
and verified with SD block CRC and raw sector address/EDC. A zero loaded
count indicates the windows did not pass staging, and a zero hit count
means that region was not served from RAM. Ordinary DMA overlap remains
active during SCI demand reads. The playback fields use hexadecimal and a
781250 Hz clock. END is the exclusive LBA of the last completed read.

Build inventory

KUI/tools.kui  41dc9ec85129  703549FC
KUI/apps/games/retail-toy-test-8.kui  5db8cbd028c9  B8147917
KUI/apps/games/retail-toy-test-9.kui  5db8cbd028c9  2BCC1480
KUI/apps/games/retail-toy-test-10.kui  5db8cbd028c9  A136EE92
