K-UI Toy Commander SCI Test 60

1. Power off the Dreamcast. Merge the archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Open your usual original raw Toy Commander GDI. In Options, Advanced,
   Tests, run 60 SCI frame timing. Keep your normal display settings.
   Set CD Music and virtual VMU to Off.
3. Let both movies run from the beginning. After they finish or drop out,
   hold both triggers and A+B+X+Y for about three seconds. After the save
   message, press Start to reboot. Send 60.txt from /reports/toy/ or the
   newest numbered copy. If saving fails, photograph both report screens.

Test 60 retains Test 57's playback and adds a scene-submission probe.
Test 57 is included for comparison if Test 60 plays differently.

Report fields

B refers to the first movie and I to the second. Values are in hex.
One second is 781250 timer ticks. Frame observations are one-based.
SUBMIT counts calls that reach and return from the game's scene-submit
function. CADENCE counts consecutive AVI frames submitted within 1.5
nominal frame periods. MAX is the largest gap between submits, AT is
its preceding frame, TAIL is from the last submit to the player's
terminal return. FIRST is from the player's start return to first
submit. LAST is the last submitted frame. CLOCK LOST counts invalid
clock samples. The probe does not directly observe video scanout.
WAIT is the first frame observation with a matching pending read and
no ready video chunk. USED and NEED are bytes already in the AVI ring
and additional bytes needed for the next indexed video chunk. SKIP is
the first observed skipped advance. LBA, READ N and VERIFIED describe
the pending request and how many raw sectors had passed EDC at WAIT.
TICK is elapsed since start at WAIT. FFFFFFFF means unavailable for a
buffer value, VERIFIED or TICK.

Included software

KUI/tools.kui
  Build 47c6fdaef7b5, payload CRC32 A020C0BB.
KUI/apps/games/retail-toy-test-57.kui
  Build e5b30cdd569d, payload CRC32 7D6762C5.
KUI/apps/games/retail-toy-test-60.kui
  Build b0e6305f53f5, payload CRC32 55447ABA.

SHA256SUMS.txt verifies the software files and this README.
