K-UI Test 83 - the stream survey: reports saved to the card, no photos

This is Test 81's fast reader, unchanged, with the one thing you asked
for: its report is saved to the SD card from a normal Games launch,
and it names the game. Nothing to photograph and no Card Tools.

READ THIS FIRST
- This is a TEST FILE, not an update. For the test it goes in the
  place of your New reader module, like Tests 79 to 82.
- Before step 1, copy your own KUI/apps/games/retail-native-dual.kui
  from the card to your computer. Put that copy back at the end.
- Virtual VMU must be OFF for every game you run with it. New (X).
  CD Music Off (most games offer nothing else).
- There is a second file in this archive: KUI/SURVEY/STREAM.TXT, a
  blank report file of plain text with 100 slots. Each launch of
  a game with this module fills one slot. It is the only file on the
  card this module writes to. Copy it as it is: do not open and save
  it in an editor first, or the module will not take it for its file.
  Keep no second copy of it anywhere on the card.

WHAT TO DO
1. Power off. Copy your own module to your computer (above). Then
   merge this archive's KUI folder into the card's KUI folder: it
   replaces KUI/apps/games/retail-native-dual.kui and adds the folder
   KUI/SURVEY with STREAM.TXT in it.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Games, a
   game from the list below, New (X), virtual VMU Off.
3. While the game loads, the launch screen has one new line:
     SURVEY ON / REPORT SAVES TO SLOT 000
   (001 on the next launch, and so on). It is on screen for a second
   or so; if you miss it, no matter, the file tells me. If you see it
   start SURVEY OFF instead, note the rest of it for me; the game
   still runs, as under Test 81, and shows its report on pages at
   step 5.
4. Play for a few minutes: the opening movie, then into a level, a
   race or a fight, somewhere music would play and the card light is
   busy.
5. Save the report, at a quiet moment (not while the game is
   loading): controller in port A, both triggers and A+B+X+Y held
   for about three seconds. Two pages headed TOY TEST 81 take turns
   on the screen, each with the line REPORT SAVED TO SD. Press Start:
   the console restarts into K-UI.
6. The next game: from step 2. No file copying in between.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT from the
   card to your computer and send me that file as it is. Put your own
   retail-native-dual.kui back on the card.

WHICH GAMES
Four are enough this time. The long round over the CD music games
comes with my next build, which also records what the sound hardware
is doing, so that you do that round only once.
  1. Evolution 2. Its numbers go beside the pages you photographed
     for Test 81.
  2. to 4. Three regular games (not Windows CE) with CD music, one
     from each line, whichever you have:
       MDK2, Omikron, Ready 2 Rumble or Star Wars Episode I Racer
       GTA 2, Bangai-O or ChuChu Rocket
       Giga Wing, Mr. Driller, Plasma Sword or Unreal Tournament
If the menu does not offer New (X) for one of them, or its launch
stops, take another from the same line and tell me which one.

WHAT TO TELL ME
- Send STREAM.TXT. That is the whole report.
- Any game that did not start, hung, or looked or sounded wrong with
  this file, and roughly when. (No CD music is expected: this build
  has none, like your installed reader with CD Music Off.)
- Any launch line that started SURVEY OFF.

IF SOMETHING GOES WRONG
- SURVEY OFF / NO KUI/SURVEY/STREAM.TXT: the file is not where the
  module looks. It must be KUI/SURVEY/STREAM.TXT on the card.
- SURVEY OFF / FILE FULL - COPY A BLANK ONE: all 100 slots are
  used. Send me the full file and I will send a new blank one (each
  blank file has its own number, so do not reuse this archive's).
- Pages headed REPORT NOT SAVED / FILM EVERY PAGE at step 5: the save
  was refused. Tell me which game. Photograph the pages only if you
  want that run's numbers; Start still restarts the console.
- A stop screen at launch: photograph it. THIS TEST BUILD NEEDS
  VIRTUAL VMU OFF means the game's virtual VMU setting is on.
  RETAIL RESIDENT INIT FAILED under a SURVEY OFF line means the card
  faulted while the report file was being read: power off and try
  once more; if it comes again, take STREAM.TXT off the card, run the
  game without it and tell me.
- A hang or a crash: power off and note the game. A launch that is
  cut short leaves its slot with the game's name and no report; that
  is fine and tells me the game was started.

Do not leave this file installed for normal use: no virtual VMU, no
prefetch, no CD music.

FILES IN THIS ARCHIVE
KUI/apps/games/retail-native-dual.kui
  Changed for the test only. Test 83, build 3bd4bed2fca2, payload
  CRC32 7A90B4AB, 307,256 bytes. Its reader is Test 81's byte
  for byte, so the saved report itself still says TEST 81; the launch
  record above it in the file says SURVEY 83 and BUILD 3bd4bed2fca2.
KUI/SURVEY/STREAM.TXT
  Added. The blank report file, 154,112 bytes of text, ID
  B674BC235B6B5A9B78760CBD6F7E63FF.

Nothing else is in this archive. Your menu, every other reader, your
games, saves and settings are retained. No updater package, game data
or save data is included.

This build passed its host and linked checks. It has not run on a
console. It writes to the card: only inside STREAM.TXT, and only into
places it has just read back as blank parts of that file.

SHA256SUMS.txt verifies the two files and this README.
