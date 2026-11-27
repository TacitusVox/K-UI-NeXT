K-UI Test 86 - CD music, part 3: music where sound memory is full

WHAT TEST 85 SHOWED (your report file of 27 November)
- POD SpeedZone played its music: the whole path works, from the SD
  card to two sound channels of the game's own sound driver.
- ChuChu Rocket and King of Fighters were silent for one reason, the
  same in both: the reader looked for free room in the sound chip's
  memory for its music buffer and found none. These two games fill
  that memory with their own sounds. POD leaves room.
- In POD only the disc's left channel was heard, in the middle: the
  game sets one of the two CD volume controls and leaves the other at
  nothing, and Test 85 followed each control for its own side.

WHAT TEST 86 CHANGES
- The music buffer no longer needs room from the game. It is kept in
  the sound chip's own compressed format, a quarter of the size, in a
  small piece of memory that belongs to the game's sound driver and
  that the driver never uses.
- Both sides play at the louder of the game's two CD volume controls,
  the left side on the left and the right side on the right.
- If the game takes the buffer's memory back in the middle of a
  piece, the music picks up again instead of staying silent until the
  next piece.

It makes sound in the same three games as Test 85:
   ChuChu Rocket
   King of Fighters Dream Match 1999
   POD SpeedZone
In every other game it stays silent and the report says why.

WHAT I DO NOT KNOW YET (this test answers it)
- Whether the sound chip plays the compressed format cleanly round
  the buffer's end. The games' own driver sets up its compressed
  streams the same way, but this reader has not done it on a console.
  If it does not, the music crackles, or turns louder, softer or
  rough within the first seconds of a piece.
- Whether the driver really leaves that piece of its memory alone
  while a game runs. I read that from the drivers' code. The reader
  checks it at every step and the report says so if not.
- How often the music drops out with the small buffer (below).

WHAT IS EXPECTED, AND NOT A FAULT
- The music is a little less clean than from a CD: it is compressed
  four to one. A soft hiss on quiet passages is possible.
- The buffer is small: 0.18 seconds of music in ChuChu Rocket, 0.28
  in King of Fighters and POD (Test 85's held 0.37). When the game
  loads from the card, the game's reading comes first: the music
  stops for a moment and comes back at the place the drive would have
  reached. POD loads all through a race, so its music will probably
  break up more often than under Test 85. That is this test's price
  for needing no room; the next step is a longer buffer.
- In unbroken music there is one very short gap about every three and
  a half minutes in ChuChu Rocket, and every five and a half in the
  other two: the reader lines its clock up with the sound chip's.
- The first fraction of a second of a piece is not heard.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of
  KUI/apps/games/retail-native-dual.kui, where your card now has the
  Test 85 file. Your own file is the one you kept on your computer:
  put it back when you are done, as after Test 85.
- K-UI: the first line of Settings, Diagnostics must read
     K-UI 3.0 RC1 SD runtime aaaba0ea3640  (the K-UI archive of 27
                                            November), or
     K-UI 3.0 RC1 SD runtime 53a33ec708ab  (the one before it).
  Both are right for this test. With any other build, stop and tell
  me.
- For every game, in Games / Options, as in Test 85:
     Background loading: On - New (X)
     CD music: Off
  and Virtual VMU Off. "CD music: Off" is right: that option selects
  K-UI's older music route, which is not this test. The music here
  comes from this file.
- KUI/SURVEY/STREAM.TXT is a new blank report file with 100 slots (its
  ID is below). Copy it over the old one, as it is: do not open and
  save it in an editor. You have sent me the old one already.
- Keep a short list as you go: for each launch, in order, the game
  and which file was on the card (main, 1 or 2).

WHAT TO DO
1. Power off. Merge this archive's KUI folder into the card's KUI
   folder. It replaces two files: KUI/apps/games/retail-native-dual.kui
   and KUI/SURVEY/STREAM.TXT. Leave the IF-NEEDED folder on your
   computer.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Check the
   Diagnostics line (above).
3. Games, ChuChu Rocket, the options above, launch. The launch line
   says SURVEY ON / REPORT SAVES TO SLOT 000 (001 on the next launch,
   and so on).
4. Play three to five minutes where the game has CD music:
   - ChuChu Rocket: the title screen and menus, a stage, a pause and
     a resume.
   - King of Fighters Dream Match 1999: the character select, a fight
     to its end, the next round.
   - POD SpeedZone: the menus, one race with the load before it.
5. Save the report while music plays, or should be playing (not
   while the game loads): controller in port A, both triggers and
   A+B+X+Y for about three seconds. Two pages headed TOY TEST 81 take
   turns (the heading is Test 81's; the saved report says TEST 86),
   each with REPORT SAVED TO SD. Press Start: the console restarts
   into K-UI.
6. The same with King of Fighters Dream Match 1999, then POD
   SpeedZone: from step 3.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list and what you heard.

WHAT TO LISTEN FOR
A few words for each game are enough:
1. Is there music? Is it the right piece?
2. Is it clean? Or does it crackle, hiss loudly, or turn louder,
   softer or rough after a few seconds? (That would be the compressed
   format going wrong, and is the thing I most need to know.)
3. How often does it drop out, and when: only while the game loads,
   or also in the middle of quiet play?
4. Is it stereo, with left and right the right way round, if you can
   tell? How loud is it against the game's effects?
5. Are the game's own sounds (effects, voices) as they should be?
6. POD: better, worse or the same as under Test 85?

IF NEEDED (two more files in the IF-NEEDED folder; only for this)
1. A game runs fine but has NO music at all: save the report first
   (step 5; it holds the reason). Then copy
     IF-NEEDED/1-full-volume/KUI/apps/games/retail-native-dual.kui
   over the card's file and run that game once more, with a report.
   This file plays the music at full volume instead of reading the
   game's CD volume from the sound chip.
2. A game hangs, crashes, runs badly, or its own sounds go wrong: if
   the game still reacts, save the report first (hold the buttons for
   up to ten seconds). Then copy
     IF-NEEDED/2-no-sound/KUI/apps/games/retail-native-dual.kui
   over the card's file and run that game once more, with a report.
   This file reads the music from the card but never writes to the
   sound chip, so it is silent: it tells me whether the reading or
   the sound is the cause.
After either, put the main file back (this archive's
KUI/apps/games/retail-native-dual.kui) before the next game.

Do not leave any of these files installed for normal use: no virtual
VMU, no prefetch.

FILES IN THIS ARCHIVE
KUI/apps/games/retail-native-dual.kui
  Changed for the test only. Test 86, build 0af5d9ce889c, payload CRC32
  789692C6, 324,420 bytes. The saved report says TEST 86; the launch
  record above it in the file still says SURVEY 83 and BUILD
  0af5d9ce889c (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  A7E2DEC4944B4B399F8B3C119C906300.
IF-NEEDED/1-full-volume/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the volume reading:
  payload CRC32 F6205516, 324,164 bytes.
IF-NEEDED/2-no-sound/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the sound output, with
  Test 84's read-only look at the sound chip: payload CRC32 B8CFB1A1,
  319,396 bytes. Its saved report says TEST 85.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive. Your K-UI, every other reader, your
games, saves and settings are retained. No updater package, game
data or save data is included.

This build passed its host and linked checks. The sound output was
run on a model of the sound chip, its bus and the card, which takes
no write but the permitted ones, decodes the compressed music as the
chip is documented to, and compares what is played with the track.
It has not run on a console. It writes to the card only inside
STREAM.TXT, as Tests 83 to 85. It writes to the sound chip only in
the three games above: to the unused end of the sound driver's own
memory, and to the two channels your Test 84 runs found free.

SHA256SUMS.txt verifies every file in this archive.
