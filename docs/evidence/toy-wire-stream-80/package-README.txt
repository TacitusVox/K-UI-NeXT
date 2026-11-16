K-UI Test 80 - the fast reader keeps reading ahead in Evolution 2

Your Test 79 run showed the new reader working in both Evolution games,
and two places where it fell short. This archive fixes the first, the
other archive (Test 81) the second. Run this one first.

What Test 79's numbers said about Evolution 2: the game asks the drive
a status question between its reads, and the reader threw its
read-ahead away at every one of them (969 times out of 972). So nothing
was ever read ahead there. This build keeps the read-ahead through such
questions.

READ THIS FIRST
- This is a TEST FILE, not an update. Like Test 79 it goes in the place
  of your New reader module for the test.
- Your own retail-native-dual.kui is the one in your 06 NOV package
  (KUI-Shared-Game-Launch-06NOV2026-2210-UTC-f8807cb.zip, 290,104
  bytes). Put that one back when the tests are done.
- Virtual VMU must be OFF, as with Test 79. With it on the launch stops
  with THIS TEST BUILD NEEDS VIRTUAL VMU OFF. Nothing is harmed.
- No Card Tools. Normal K-UI boot, Games menu.

THE TEST: Evolution 2 only
1. Power off. Merge this archive's KUI folder into the card's KUI
   folder, replacing KUI/apps/games/retail-native-dual.kui.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Games,
   Evolution 2, New (X), CD Music Off, virtual VMU Off.
3. Let the opening movie play, then play for a few minutes where you
   saw the stutter with Test 79.
4. The report: controller in port A, both triggers and A+B+X+Y held for
   about three seconds. The screen shows
     REPORT NOT SAVED / FILM EVERY PAGE
   It is THREE pages now, not four, a new page about every five
   seconds, round and round. Photograph each page. The first page
   says TEST 80 and BUILD e58dd2e6b641.
   Then press Start: the console restarts into the K-UI CD.

Evolution 1 does not need this build. Its Test 79 run had no such
question from the game, so this change does nothing there. It is run
with Test 81.

WHAT TO TELL ME
- The stutter you saw while the card light was on: still there, less,
  or gone?
- Anything else that changed: the movie, loading, sound.
- The three photos.

IF SOMETHING GOES WRONG
- A stop screen, a hang, or a game that does not start: photograph the
  screen, power off. That is a result too.
- RESTART BUSY / TRY AGAIN OR POWER OFF on the report pages: try Start
  once more, then power off. The pages are good all the same.
- The gesture does nothing: the controller has to be in port A, and
  some screens of a game do not read it. Try in normal play.

Do not leave this file installed for normal use: no virtual VMU, no
prefetch.

CHANGED FILE
KUI/apps/games/retail-native-dual.kui
  Test 80, build e58dd2e6b641, payload CRC32 F8F514E4,
  299,840 bytes. For the test it takes the place of the New reader
  module. The report pages say TEST 80 and BUILD e58dd2e6b641; the last
  value of the PIPE row is FBD.

Nothing else is in this archive. Your menu (build f8807cb16561), Card
Tools, every other reader, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a
console.

SHA256SUMS.txt verifies the module and this README.
