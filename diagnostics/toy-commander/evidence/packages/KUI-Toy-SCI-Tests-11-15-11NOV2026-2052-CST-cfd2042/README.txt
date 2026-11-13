K-UI Toy Commander SCI tests 11–15

Copy the KUI folder into the root of your SD card. Boot the K-UI CD, tap B
during countdown, then open Card tools → Games → Toy Commander → Options
→ Advanced → Tests. The original raw GDI shows Tests 11–14. The 2048-byte
GDI shows Test 15. Each launch uses SCI New (X), CD Music Off and Virtual
VMU Off.

11  Raw rolling DMA. Builds a verified buffer during playback.
12  Prefilled rolling DMA. Adds a verified 80-sector intro prefix to 11.
13  RAM prefix only. Serves the same prefix as 12, followed by ordinary
    demand reads for the rest of the intro.
14  Preload control. Stages the same prefix as 12 and 13, then serves all
    reads through the ordinary SD demand reader.
15  2048 rolling DMA. Uses the rolling reader with your 2048-byte GDI.

Suggested order: 14, 13, 11, 12, then 15 if the 2048 copy is handy. If time
is short, start with 14 and 12. Let the second video finish or stop. Hold
both triggers and A+B+X+Y for about three seconds. When REPORT SAVED TO SD
appears, release the chord and press START. Leave the K-UI CD in the drive
and tap B during the countdown to return to Card tools. Send the numbered
files from /reports/toy/ and note whether the second video reached gameplay.
If a filename is already in use, the next run gets a numbered suffix.

Playback values are hexadecimal. END is the last completed read's
exclusive logical disc LBA. Buffer counts are 512-byte SD blocks. EARLY and
LATE HITS count actual buffer delivery in each half of the intro. IRQ FILLS
counts verified blocks published from SCI completion interrupts. MAX LEAD
is the highest ready lead observed. STARVED counts foreground visits that
needed a block beyond the ready buffer. PREFILL zero on Test 12 means it
started without an admitted prefix, for example if it spans fragmented SD
extents. Tests 13 and 14 report the preload count and first/last RAM hits.

Build inventory

KUI/tools.kui  48e3e3833598  A44FDBE8
KUI/apps/games/retail-toy-test-11.kui  cfd2042d9316  D47B36D5
KUI/apps/games/retail-toy-test-12.kui  cfd2042d9316  2585A574
KUI/apps/games/retail-toy-test-13.kui  cfd2042d9316  729DF9F2
KUI/apps/games/retail-toy-test-14.kui  cfd2042d9316  D1E6BAF8
KUI/apps/games/retail-toy-test-15.kui  cfd2042d9316  7914C3E0
