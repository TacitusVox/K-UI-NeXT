K-UI Toy Commander Test 39 — SCI movie exit

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B
   during the countdown to open Card Tools.
2. Select your usual original raw Toy Commander GDI. Open Options,
   Advanced, Tests and select 39 SCI movie exit. This is the only Toy
   test in this Card Tools build. Use your normal display settings.
   CD Music and virtual VMU are off for the test.
3. Let both movies run normally. Note where each ends and whether
   the picture and audio stop at the same time.
4. At the title screen or after an early exit, hold both triggers and
   A+B+X+Y for about three seconds. When REPORT SAVED TO SD appears,
   release the buttons and press Start to reboot. Send /reports/toy/39.txt
   or the newest numbered repeat. If saving fails, capture both report
   pages. If the game locks up, send the last visible scene.

Test 39 uses Test 38's SCI service and checked retries. For the verified
game executable, a temporary hook in RAM calls the original movie
dispatcher and records its startup and terminal results for the two
clips. It keeps the original inputs and returns. The report includes
decoder frame position and the game's audio sample counter. The known
lag status is FFFFF843. The end-of-stream or completed-stop status is
000007DC. The flags in the START row tell which records were captured:
bit 0 is player startup, bit 1 is a terminal update, bit 2 is a stop
command from the normal update, and bit 3 is an internal stop follow-up.
Missing terminal data means the hook did not observe a qualifying return.

Included files

KUI/tools.kui
  Card Tools 8af4a09880bf, payload CRC32 B464780A.

KUI/apps/games/retail-toy-test-39.kui
  Test 39 SCI reader c5cd2b945454, payload CRC32 18F24402.

SHA256SUMS.txt verifies both modules and this README.
