K-UI Toy Commander Test 31 - SCI checked refill

Power off the Dreamcast and merge KUI onto the SCI SD card. Cold-boot the
K-UI CD and tap B during the countdown for Card Tools. Select the original
raw 2352-byte Toy Commander GDI in Games, then Options > Advanced > Tests >
31 SCI checked refill. Use 60 Hz and let both opening movies play without
skipping. The test sets CD music and virtual VMU Off to isolate movie reads.

At the title screen, or after an early exit to the menu, hold both triggers
and A+B+X+Y for about three seconds. Wait for REPORT SAVED TO SD, release the
buttons, and press Start to reboot to the K-UI CD. Send /reports/toy/31.txt
or its newest numbered repeat. If the console locks up, report which movie,
roughly where it stopped, and whether the audio repeated.

The reader uses the earlier raw single-block CMD18 path. An incomplete
receive is discarded and read afresh. For a receive overrun during movie
streaming, the same SD block gets a 6.25 MHz retry before the existing PIO
fallback. Complete receives still pass the full CRC16 check. Successful
sequential reads use 12.5 MHz and continue within the open CMD18 run. No
new RAM reservation is required. SLOW BLOCK RETRIES BUMPER INTRO counts
actual slow DMA starts, independently for the two movies. Compare that row,
movie end sector, and foreground/IRQ delivery timings with visible playback.

Changed software
KUI/tools.kui  build 637fbd5621f0  payload CRC32 DC34A3FD
KUI/apps/games/retail-toy-test-31.kui  build 54b12486a48a  payload CRC32 99C65D51

Retained routes use the installed modules from their original packages
KUI/apps/games/retail-toy-test-26.kui  build 1e2d51de054a  payload CRC32 DD949789
KUI/apps/games/retail-toy-test-27.kui  build 3ffbed71e1cc  payload CRC32 ACD4B97D
KUI/apps/games/retail-toy-test-28.kui  build 1adeb5c2b65c  payload CRC32 503900B7
KUI/apps/games/retail-toy-test-29.kui  build 3336f9682d6e  payload CRC32 5077773A
KUI/apps/disc/native-toy-trace.kui  build 381c6c8ee270  payload CRC32 FC742962
The retained physical trace listed above is the previously supplied save fix.
