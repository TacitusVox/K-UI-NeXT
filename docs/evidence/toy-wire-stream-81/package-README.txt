K-UI Test 81 - the fast reader no longer stands still between reads

This is Test 80 plus one change. Run Test 80 first (its own archive),
then this one.

What Test 79's numbers said about both Evolution games: whenever the
game's own graphics or sound work interrupted the card, the reader
stood still until the game's next disc call, about a hundredth of a
second each time (34 seconds in all in your Evolution run). In Toy
Commander a wait is about half a thousandth of a second, because there
it has a spare timer of the game's to wake it. This build lets it use a
spare timer in any game that has one free.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of your New
  reader module for the test, like Tests 79 and 80.
- Your own retail-native-dual.kui is the one in your 06 NOV package
  (KUI-Shared-Game-Launch-06NOV2026-2210-UTC-f8807cb.zip, 290,104
  bytes). Put that one back when the tests are done (step 6).
- Virtual VMU must be OFF. No Card Tools. Normal K-UI boot, Games menu.
- This is the first time the reader borrows a timer from a game other
  than Toy Commander. It only takes one the game is not using, and
  gives it back. If a game misbehaves with this file and did not with
  Test 80, that is the result: tell me which game and when.

THE TEST: Evolution 2, then Evolution 1
1. Power off. Merge this archive's KUI folder into the card's KUI
   folder, replacing KUI/apps/games/retail-native-dual.kui.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Games,
   Evolution 2, New (X), CD Music Off, virtual VMU Off.
3. Let the opening movie play, then play for a few minutes where you
   saw the stutter.
4. The report: controller in port A, both triggers and A+B+X+Y held for
   about three seconds. Three pages, a new one about every five
   seconds. Photograph each. The first page says TEST 81 and
   BUILD 684f485644e5. Then press Start.
5. The same with Evolution 1 (steps 2 to 4).
6. Power off and put your own retail-native-dual.kui from the 06 NOV
   package back on the card.
7. One more look, with your own file back: the same spots in both
   games for a minute or two. Is the stutter there too? You said it
   might have been there all along; this is what tells me whether it
   is the new reader's or the game's.

WHAT TO TELL ME
- For each game: the stutter while the card light is on - still there,
  less, or gone? Against Test 80 for Evolution 2, against Test 79 for
  Evolution 1.
- The opening movies: as before or not.
- Step 7: with your own file, is the stutter there?
- The photos, three per game.
- Anything that looked or sounded wrong.

IF SOMETHING GOES WRONG
- A hang, a crash, or sound or picture going wrong in a game that was
  fine with Test 80: power off, note roughly when it happened. Then
  put Test 80's file or your own back.
- A stop screen: photograph it.
- RESTART BUSY / TRY AGAIN OR POWER OFF on the report pages: try Start
  once more, then power off. The pages are good all the same.

Do not leave this file installed for normal use: no virtual VMU, no
prefetch.

CHANGED FILE
KUI/apps/games/retail-native-dual.kui
  Test 81, build 684f485644e5, payload CRC32 B40B9423,
  300,096 bytes. For the test it takes the place of the New reader
  module. The report pages say TEST 81 and BUILD 684f485644e5; the last
  value of the PIPE row is 1FBD.

Nothing else is in this archive. Your menu (build f8807cb16561), Card
Tools, every other reader, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

Toy Commander needs no run of this build.

This build passed its host and linked checks. It has not run on a
console.

SHA256SUMS.txt verifies the module and this README.
