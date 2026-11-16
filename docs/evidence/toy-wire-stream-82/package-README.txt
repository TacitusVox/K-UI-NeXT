K-UI Test 82 - the fast reader with the virtual VMU ON

This is the third of the three changes: the virtual VMU beside the new
reader. Run Tests 80 and 81 first (their own archives). This build has
both of their changes in it.

Until now the new reader only ran with the virtual VMU off. This build
puts your installed build's VMU part together with Test 81's reader.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of your New
  reader module for the test, like Tests 79 to 81.
- Your own retail-native-dual.kui is the one in your 06 NOV package
  (KUI-Shared-Game-Launch-06NOV2026-2210-UTC-f8807cb.zip, 290,104
  bytes). Put that one back when the test is done (step 9).
- Virtual VMU must be ON for this test, with the virtual card in slot
  A2 (the way your Evolution 2 launch of 06 NOV had it). A game set to
  slot A1 would quietly run your installed A1 file instead.
- BACK UP YOUR VIRTUAL VMU FILES FIRST. This is the first build that
  reads and writes them through the new reader. Copy the card's
  KUI/vmu-test and KUI/vmu folders to your computer before step 1.
- THERE ARE NO REPORT PAGES in this build. The trigger gesture does
  nothing. Nothing to photograph but what looks wrong.
- Not for Toy Commander: in this build Toy Commander gets the plain
  reader, with nothing read ahead for its movies, whether the virtual
  VMU is on or off.
- No Card Tools. Normal K-UI boot, Games menu.

THE TEST: Evolution 2, then Evolution 1
1. Back up KUI/vmu-test and KUI/vmu (see above). Power off.
2. Merge this archive's KUI folder into the card's KUI folder,
   replacing KUI/apps/games/retail-native-dual.kui.
3. Cold boot the K-UI CD (not Restart K-UI from the menu). Games,
   Evolution 2, New (X), CD Music Off, virtual VMU ON.
4. While the game loads, the loader's screen should show the lines
     TEST 82 / STREAM READER / INSTALLED VMU
     SCI STREAM READ AHEAD READY
   (Your installed build says BOUNDED READ AHEAD READY / 192 KIB
   CACHE there.)
5. In the game: does it find the memory card? Load a save you made
   before today.
6. Play for a few minutes where you saw the stutter. Watch a movie if
   one comes.
7. Save. Then power off, boot again the same way, and load that save.
8. The same with Evolution 1 (steps 3 to 7).
9. Power off and put your own retail-native-dual.kui from the 06 NOV
   package back on the card.

WHAT TO TELL ME
- For each game: did it start? Was the memory card found, and did
  your old save load?
- Play and movies: like Test 81, or different? How?
- Did saving work, and was the save there after power off?
- Anything that looked or sounded wrong, and when.
- If you can: the file KUI/apps/games/launch-timing.txt from the card
  after one of the launches. It names the build that ran.

IF SOMETHING GOES WRONG
- A stop screen: photograph it. It says where it stopped.
- A page headed VMU / A2 with SCI JOURNAL and PHOTO / POWER OFF in
  the middle of play: photograph it and tell me what the game was
  doing. In this build that page can mean the reader stopped.
- The game does not find the card, hangs, or freezes while saving:
  power off, note what you were doing, and put your own file back.
- A save that will not load or looks damaged: STOP. Put your own file
  back, restore your backup of the two folders, and tell me. Do not
  save anything else with this build.

Do not leave this file installed for normal use: no prefetch for
games the new reader does not cover, and nothing read ahead for Toy
Commander.

CHANGED FILE
KUI/apps/games/retail-native-dual.kui
  Test 82, build af5ebae4f0a5, payload CRC32 4962DB2D,
  309,936 bytes. For the test it takes the place of the New reader
  module. launch-timing.txt names it after a launch:
  source_module_build: af5ebae4f0a5

Nothing else is in this archive. Your menu (build f8807cb16561), Card
Tools, every other reader, the A1 module, games, saves, reports and
settings are retained. No updater package, game data or save data is
included.

This build passed its host and linked checks. It has not run on a
console, and neither have Tests 80 and 81, which it is built on.

SHA256SUMS.txt verifies the module and this README.
