K-UI Toy Commander SCI test suite

Copy the KUI folder in this ZIP to the root of the SD card, merging it with
the existing KUI folder. The suite opens through Card tools and uses your
existing Toy Commander GDI folders and saves.

Each run

1. Cold boot the K-UI CD. Tap B during the countdown and select Card tools.
2. Select Toy Commander in Games, then open Options, Advanced, Tests.
3. Select a numbered test. Use the raw GDI for 1–4, 6 and 7. Select the
   existing 2048-byte GDI for 5. The test uses SCI New (X), CD Music Off and
   Virtual VMU Off for that launch.
4. Let the second video run. Note whether it finishes and reaches gameplay.
   Once the video ends or stops, hold both triggers and A+B+X+Y for about
   three seconds. A 15-second clip captures both blue report pages. They
   switch automatically every five seconds. If the console freezes before
   you can open the report, capture the last screen.
5. Power-cycle and repeat from Card tools. All test modules stay on the card.

Tests

1  Verify bytes. Compare selected 2,048-byte payloads in game RAM against
   raw Mode 1 sectors read and checked during launch. LOADED is the number
   of source sectors admitted. CHECKED counts actual comparisons, including
   repeats. A nonzero BAD LBA identifies the first mismatch. BYTE is the
   offset within that sector. DATA holds the expected byte above the actual
   byte, with the expected byte in bits 8–15 and the actual byte in bits 0–7.
2  RAM window near LBA 469080. Serve a bounded verified region around the
   earlier repeated stop from RAM. HITS, FIRST and LAST show delivery that
   actually used it. Zero hits means that run did not exercise the window.
3  DMA contention guard. Use programmed receive when the graphics transfer
   is already busy at the point of SCI reception. GUARDED PIO counts uses.
4  GD pacing. Yield between work steps after a 4 ms foreground budget when
   the clock is valid. BUDGET YIELDS counts where the reader yielded.
5  2048-byte reference. Repeat the cooked image with the same report and
   reader. The report is TEST 1 with SOURCE 2. The raw-sector witness applies
   only to the raw image. This comparison also reflects the SD file layout.
6  RAM window near LBA 468128. Same RAM control around the recent stop.
7  Non-overlapping DMA. Finish checking and copying the current block before
   starting the next. SERIAL BLOCKS counts when the normal early kick was
   suppressed.

Playback figures are hexadecimal. END is the exclusive end of the last
completed movie request. SOURCE 1 is raw2352, 2 is cooked2048. SD statistics
are run totals. CRC includes rejected receptions with a missing DMA byte.

Start with 1, then 3, 4 and 7. Use 2 and 6 to check the two known regions,
and 5 if the 2048-byte copy is available. Send which runs reached gameplay
and the corresponding reports. One photo of each page works as well as video.

Build inventory

KUI/tools.kui  5aadb758f2d1  B0D71665
KUI/apps/games/retail-toy-test-1.kui  70723b37cebb  824043B6
KUI/apps/games/retail-toy-test-2.kui  70723b37cebb  B5E75E1A
KUI/apps/games/retail-toy-test-3.kui  70723b37cebb  2BFD23C1
KUI/apps/games/retail-toy-test-4.kui  70723b37cebb  1E0A875F
KUI/apps/games/retail-toy-test-6.kui  70723b37cebb  4EE3F408
KUI/apps/games/retail-toy-test-7.kui  70723b37cebb  77598644
