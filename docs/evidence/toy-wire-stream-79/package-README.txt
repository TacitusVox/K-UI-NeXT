K-UI Test 79 - the fast reader on other games (Evolution 1 and 2)

You asked whether the new reader would be better than the prefetch for
the two Evolution games. This is the build that can answer it. It is
Test 78 plus one change: the fast reader is no longer for Toy Commander
alone. Any game on the New reader gets it, on a raw (2,352-byte) image.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of your
  installed New reader module for the test, and your own file goes back
  afterwards. Step 2 below keeps your file.
- Virtual VMU must be OFF. This build has no virtual VMU yet; that is
  the next piece of work. With it on, the launch stops with
  THIS TEST BUILD NEEDS VIRTUAL VMU OFF. Nothing is harmed: power off,
  switch it off, start again.
- So your saves on a virtual card are not there in these runs. Start a
  new game, or use a real VMU that has a save.
- It has no prefetch either. Where the fast reader does not step in, a
  game reads slower with this file than with your installed one. That
  is part of what the test is to show.

THE TEST
Each game twice the same day: first with what you have installed, then
with this file. Same image, New (X), CD Music Off, virtual VMU Off.

1. With your installed files, virtual VMU Off: start Evolution 2 with
   New (X). Let its opening movie play, then play for a few minutes
   where the lag used to be before the prefetch. Then Evolution 1 the
   same way. This is the reference.
2. Power off. Copy the card's own
     KUI/apps/games/retail-native-dual.kui
   to your computer and keep it. Then merge this archive's KUI folder
   into the card's KUI folder, replacing that one file.
3. Cold boot the K-UI CD (not Restart K-UI from the menu). Games,
   Evolution 2, New (X), CD Music Off, virtual VMU Off.
   On the start screens one line must say
     SCI STREAM READ AHEAD READY
   That line is this build's. If it says
     NO STREAM READ AHEAD / PLAIN READER
   the fast reader is not in play (the image is not a raw one, or the
   reader is not New): tell me, and skip that game.
4. Let the opening movie play, then play the same places as in step 1
   for a few minutes.
5. The report. With the controller in port A, at a moment when nothing
   is loading, hold both triggers and A+B+X+Y for about three seconds.
   The game stops and the screen shows
     REPORT NOT SAVED / FILM EVERY PAGE
   and twelve lines of numbers, a new page about every five seconds,
   four pages, round and round. Film it for two rounds. Nothing is
   saved to the card from this menu, so the film is the report.
   Then press Start: the console restarts into the K-UI CD.
6. The same with Evolution 1 (steps 3 to 5).
7. Power off and put your kept retail-native-dual.kui back on the card.

WHAT TO TELL ME
- For each game: with this file, was play as smooth as with your
  installed one, smoother, or was the lag back? Where?
- The opening movies: the same, better or worse than with your file?
- Loading times as they felt: the same, shorter, longer? If you time
  one thing, time choosing the game to its first picture, both ways.
- The films of the report pages, one per game.
- The file KUI/apps/games/launch-timing.txt from the card as it stands
  after the last launch with this file. It names the menu build and the
  module that was actually loaded.
- Any screen that stopped a launch: a photo of it.

IF SOMETHING GOES WRONG
- A stop screen at launch, or a game that does not start: photograph
  the screen, power off, put your own file back. That is a result too.
- RESTART BUSY / TRY AGAIN OR POWER OFF on the report pages: try Start
  once more, then power off. It means the game was reading when you
  asked for the report; the pages are good all the same.
- The gesture does nothing: the controller has to be in port A, and
  some screens of a game do not read it. Try in normal play.
- A1: a game set to use the virtual card in slot A1 with virtual VMU on
  loads another file, not this one. With virtual VMU off this does not
  arise.

Do not leave this file installed for normal use: no virtual VMU, no
prefetch.

CHANGED FILE
KUI/apps/games/retail-native-dual.kui
  Test 79, build 9f183a7f72f5, payload CRC32 44C11C5B,
  299,488 bytes. For the test it takes the place of the installed New
  reader module, build f8807cb16561 (290,104 bytes, SHA-256
  09c1480c6c470166b3e584f54218d98679ef4a34a8765d47bde4d2187889481a in
  the 06 NOV package). The report pages say TEST 79 and BUILD
  9f183a7f72f5; the last value of the PIPE row is 7BD.

Nothing else is in this archive. Your menu (build f8807cb16561), Card
Tools, every other reader, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

Toy Commander needs no run of this build. Test 78, in its own archive,
is the Toy Commander run.

This build passed its host and linked checks. It has not run on a
console. What its two new screens look like (the stop for virtual VMU
on, and the report pages) has never been seen on a console either.

SHA256SUMS.txt verifies the module and this README.
