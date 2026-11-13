K-UI Toy Commander SCI and physical GD-ROM tests

Power off the Dreamcast. Merge the archive's KUI folder into the KUI
folder at the root of the SCI SD card. Boot the K-UI CD and tap B
during the countdown to open Card Tools. Use your usual display mode.

Test 62 uses the original raw GDI from the SCI card. Open the usual
Toy Commander image, then Options, Advanced, Tests, 62 SCI movie read
ahead. Set CD Music and virtual VMU to Off. This build keeps reading
ahead of the game's current request, holding up to 80 raw sectors in
the loader's existing memory. It checks the disc address and EDC of
each sector before using it. The game still decides which sectors
to request, when to acknowledge them and which frames to show. It
keeps Test 57's intro startup behavior and uses stock request sizes.
Test 57 is included as the previous comparison build.

Test 63 uses the original Toy Commander GD-ROM. From Card Tools select
GD Play, press Right for the Toy Commander physical timing test, then
follow the disc prompt. Keep the SCI card connected for the report.
Test 63 uses the console's GD firmware and records the same frame
timing as Test 62.

For each test, let both movies play from the start. At the title screen
or after an early exit, hold both triggers and A+B+X+Y for about three
seconds. Save 62.txt and 63.txt from /reports/toy/ or their newest
numbered copies. If a report cannot save, photograph both report pages.

Both reports use hex and a timer of 781250 ticks per second. B is the
first movie and I the second. SUBMIT counts completed renderer scene
calls. CADENCE counts consecutive frame submissions within 1.5 movie
frame periods. FFFFFFFF means the movie rate was unavailable. MAX is
the longest gap between scene submissions, AT the preceding frame,
and TAIL the interval from the last submission to the player's exit.
OUT is the game's output-attempt count. SKIP counts advances without
output. Scene submissions measure the game renderer rather than scanout.

In Test 62, AHEAD B I HIT EDC STOP gives the first and second movie's
sectors supplied entirely from verified look-ahead, total requests
served that way, sectors verified by the new producer, and producer
failures that triggered fallback. The WAIT and READ rows describe the
first observed shortfall and the remaining time to read completion.

Included software

KUI/tools.kui
  Build 01c2ec1ac285, payload CRC32 219CB534.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-62.kui
  Build 7beafbe8da44, payload CRC32 AD53B6E7.
KUI/apps/disc/native-toy-trace.kui
  Build 5034eb26898d, payload CRC32 408BFACE.

SHA256SUMS.txt verifies the software files and this README.
