K-UI Toy Commander SCI Test 61

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 61 SCI bumper read latency. Keep your normal display settings.
   Set CD Music and virtual VMU to Off.
3. Let both movies run from the beginning. Note whether the first movie
   stays fluid through the end. After the movies finish or drop out, hold
   both triggers and A+B+X+Y for about three seconds. After the save
   message, press Start to reboot. Send 61.txt from /reports/toy/ or the
   newest numbered copy. If saving fails, photograph both report screens.

Test 61 caps qualifying low-buffer reads in the first movie at 8 sectors
so each command can finish and publish its data sooner. The original AVI
parser chooses when to request data and advances by the actual length.
The second movie uses Test 57's read policy. Test 57 is included for
comparison. Both movies have entry and scene-submit counters.

Report fields

B is the first movie. I is the second. Values are hex and the clock runs
at 781250 ticks per second. Observed frame indices are one-based.
CAP gives the number of capped requests, first and last frame observations,
and the total sectors deferred to subsequent requests.
WAIT and SKIP are the first matched read shortage and first skipped advance.
USED and NEED are ring bytes present and extra bytes required for the next
indexed video chunk. LBA, N and VERIFIED give the pending command location,
length and raw sectors verified at that wait. TICK is elapsed from movie
start, AGE is from read start to wait, and DONE is from wait to publication
of completion for that read. FFFFFFFF marks an unavailable measurement.
ENTER counts calls into the game's movie renderer. SCENE counts returns
from the original scene-submit function during the clip. SUBMIT counts
those tied to the tracked renderer call. CADENCE counts consecutive AVI
frames submitted within 1.5 nominal frame periods. MAX is the longest gap
between submissions, AT is its preceding frame, and TAIL is last submit to
player exit. OUT is the SDK's attempt count. SKIP on the same row is its
total of skipped advances. Scene submission does not measure scanout.

Included software

KUI/tools.kui
  Build cb4d69803453, payload CRC32 BC8A9AD1.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-61.kui
  Build e0cc7c13ac3e, payload CRC32 C6BE5E8E.

SHA256SUMS.txt verifies the software files and this README.
