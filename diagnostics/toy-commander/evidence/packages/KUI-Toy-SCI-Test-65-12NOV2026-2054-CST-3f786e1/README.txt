K-UI Toy Commander SCI Test 65

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 65 SCI resilient movie read ahead. Use your normal display
   settings. Set CD Music and virtual VMU to Off.
3. Let both movies play from the beginning. After they finish or drop
   out, hold both triggers and A+B+X+Y for about three seconds. After
   the save message, press Start to reboot. Send 65.txt from /reports/toy/
   or the newest numbered copy. If saving fails, photograph both pages.

Test 65 retries failed SD blocks at the same card address and keeps the
verified sectors already in the read-ahead buffer. A fresh block must
pass its SD CRC before entering the private movie buffer. Each complete
movie sector still needs its disc address and EDC checked before the
original game read can complete. The timing, game requests and image
format stay the same. The included 57 and physical 63 are the original
comparison modules. Test 65 is the only new console run needed.

All report counters are hexadecimal. FIX counts retried card blocks
accepted by the SD CRC check. POLL, TAKE and FETCH count errors from
DMA completion, block validation and starting the read. CARD and WHY
record the first failing card address and its packed stage and result.
RAW counts raw-sector assembly or validation failures. SERVED counts
requests that the producer filled after entry. AHEAD B and I count
sectors served from already verified data. HIT counts those requests,
EDC counts verified sectors and STOP counts producer abandonments.

Included software

KUI/tools.kui
  Build d735df0a6d75, payload CRC32 1B51FF34.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-65.kui
  Build 3f786e1926e2, payload CRC32 733A69B1.
KUI/apps/disc/native-toy-trace.kui
  Build 5034eb26898d, payload CRC32 408BFACE.

SHA256SUMS.txt verifies the software files and this README.
