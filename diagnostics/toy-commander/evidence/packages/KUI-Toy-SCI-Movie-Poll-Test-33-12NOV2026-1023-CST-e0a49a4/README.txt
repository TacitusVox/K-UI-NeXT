K-UI Toy Commander Test 33 — SCI movie poll

Install and run

1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI SD card. Boot the K-UI CD and tap B during
   the countdown to open Card Tools.
2. In Games, select your original raw Toy Commander GDI. Open Options,
   Advanced, Tests and select 33 SCI movie poll. The test uses SCI with
   CD Music and virtual VMU off. Use your normal display settings.
3. Let both opening movies play. Note when the second movie slows down
   or exits and whether the title screen is reached. Note any repeated
   or missing audio.
4. At the title screen or after an early movie exit to the game menu,
   hold both triggers and A+B+X+Y for about three seconds. When REPORT
   SAVED TO SD appears, release the buttons and press Start to reboot.
   Send /reports/toy/33.txt or the newest numbered repeat.

If saving fails, capture both alternating report pages. If a hard freeze
prevents the report, record the last scene and whether audio continued.

What the test measures

Toy's original scheduled filesystem callback enters GD with interrupt
mask 6. Test 32 treated that as a reason to wait for card data. Test 33
identifies the scheduled and one-pass SDK calls for the verified original
executable. On these paths CHECK returns status and EXEC handles at most
one checked block or a bounded card-framing step. The SD transport uses
the same checked single-block reception and recovery as Test 32.

The report's ID1 means the original executable fingerprint matched.
ID0 means it was unverified and the earlier scheduling rules applied.
POLL lists four counts per movie in this order: other calls, scheduled
0x900 calls, explicit one-pass movie calls, and blocking wait calls.
EXEC/CHECK timings show time inside GD. GD and BG block counts show where
data was delivered. BG includes SCI interrupts and controller restarts.
NOARM measures intervals between visits with no SCI receive armed.
Timer values are hex at 781,250 ticks per second. The device run tests
playback and report saving; linked and host checks passed before packaging.

Component inventory

KUI/tools.kui
  Card Tools ca0f81eb3456, payload CRC32 194EC557.
  Cold CD Card Tools add-on. Use the installed base CD and runtime.

KUI/apps/games/retail-toy-test-33.kui
  Test 33 SCI reader e0a49a4c33d8, payload CRC32 D076555E.
  The installed Test 26–32 modules can still be selected separately.
  Exact SCI selections 26–29 and 31–32 and physical GD Test 30 passed
  production route checks alongside this package.

SHA256SUMS.txt verifies the exact copy files and this README.
