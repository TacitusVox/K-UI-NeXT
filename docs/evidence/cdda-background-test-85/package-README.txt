K-UI Test 85 - CD music, part 2: the music is heard

This is the Test 84 reader with the sound added. When a game asks the
drive to play a music track, the reader now reads that track from the
SD card between the game's own reads and plays it through two sound
channels that the game's sound driver leaves free.

It makes sound in three games, the three whose sound driver and free
channels your Test 84 runs measured on your console:
   ChuChu Rocket
   King of Fighters Dream Match 1999
   POD SpeedZone
In every other game it stays silent, as Test 84 did, and the report
says why.

WHAT I DO NOT KNOW YET (this test answers it)
- Whether the card delivers the music fast enough beside the game's
  own reading. If not, the music breaks up or starts over. POD is the
  hard case: it loads from the card all through a race.
- Whether the sound chip lets the reader read the game's CD volume.
  If it does not, a game stays silent with the main file: see IF
  NEEDED, 1.
- Whether the games' sound drivers put up with a guest on two of
  their channels. If one does not, the music stops and the report
  says so. In the worst case a game's own sounds go wrong: see IF
  NEEDED, 2.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of
  KUI/apps/games/retail-native-dual.kui, where your card now has the
  Test 84 file. Your own file is the one you kept on your computer:
  put it back when you are done, as after Test 84.
- K-UI: the first line of Settings, Diagnostics must read
     K-UI 3.0 RC1 SD runtime aaaba0ea3640  (the K-UI archive of 27
                                            November), or
     K-UI 3.0 RC1 SD runtime 53a33ec708ab  (the one before it).
  Both are right for this test. With any other build, stop and tell
  me.
- For every game, in Games / Options, as in Test 84:
     Background loading: On - New (X)
     CD music: Off
  and Virtual VMU Off. "CD music: Off" is right: that option selects
  K-UI's older music route, which is not this test. The music here
  comes from this file.
- KUI/SURVEY/STREAM.TXT is a new blank report file with 100 slots (its
  ID is below). Copy it over the old one, as it is: do not open and
  save it in an editor. You have sent me the old one already.
- Keep a short list as you go: for each launch, in order, the game
  and which file was on the card (main, 1 or 2). All three files
  carry the same build label; your list is how I tell them apart.

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
   turns (the heading is Test 81's; the saved report says TEST 85),
   each with REPORT SAVED TO SD. Press Start: the console restarts
   into K-UI.
6. The same with King of Fighters Dream Match 1999, then POD
   SpeedZone: from step 3.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list and what you heard.

WHAT TO LISTEN FOR
A few words for each game are enough:
1. Is there music? Is it the right piece?
2. Is it clean? Or are there gaps, crackle, a wrong pitch or speed,
   left and right exchanged, or a piece that starts over?
3. How loud is it against the game's effects? If the game has a music
   volume option, does the music follow it?
4. Are the game's own sounds (effects, voices) as they should be?
5. Does the music stop and come back where it should: at a pause, at
   a round's or a race's end, on a new screen?
6. Does the game run as smoothly as under Test 84?

WHAT IS EXPECTED, AND NOT A FAULT
- The first fraction of a second of a piece is not heard (up to about
  half a second for the first piece after a launch): the reader
  starts where the drive would be by then.
- In unbroken music there is one very short gap every three to four
  minutes: the reader lines its clock up with the sound chip's.
- While the game loads from the card, the game's reading comes first.
  If the music runs out, it stops and comes back at the place the
  drive would have reached, not where it stopped.

KING OF FIGHTERS: ITS MUSIC FILES ON THE CARD
The reader's map has 160 places. KOF needs 43 for its tracks, 7 for
its data, and one for every piece of its 40 music files on the card:
110 are left for those. On 21 November, with the old 64-place map,
its music did not fit and was left out; KOF has not been launched
with the 160-place map yet. If KOF has no music: save the report as
usual, then copy STREAM.TXT to your computer and look at the copy
(open it, do not save it). Under the last "TITLE THE KING OF FIGHTERS
DREAM MATCH 1999" is a line like
   TRACKS 43 RAW 3 COOKED 0 AUDIO 40 HDAUDIO 39 MAPPED 40 ...
MAPPED 40 means the music files fit. MAPPED 0 means they are in too
many pieces: delete KOF's folder from the card, copy it to the card
again and run KOF once more.

IF NEEDED (two more files in the IF-NEEDED folder; only for this)
1. A game runs fine but has NO music at all: save the report first
   (step 5; it holds the reason). For KOF, check MAPPED first (above).
   Then copy
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
  Changed for the test only. Test 85, build b9cff7086258, payload CRC32
  A96C75CF, 324,164 bytes. The saved report says TEST 85; the launch
  record above it in the file still says SURVEY 83 and BUILD
  b9cff7086258 (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  AC0C9D85004568C20442C57C37EBABA3.
IF-NEEDED/1-full-volume/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the volume reading:
  payload CRC32 7EFCCF72, 323,972 bytes.
IF-NEEDED/2-no-sound/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the sound output, with
  Test 84's read-only look at the sound chip: payload CRC32 1EA9590E,
  319,396 bytes.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive. Your K-UI, every other reader, your
games, saves and settings are retained. No updater package, game
data or save data is included.

This build passed its host and linked checks. The sound output was
run on a model of the sound chip, its bus and the card, which takes
no write but the permitted ones and compares every sample played
with the track's. It has not run on a console. It writes to the card
only inside STREAM.TXT, as Tests 83 and 84. It writes to the sound
chip only in the three games above: to a piece of sound memory that
the driver's own table shows unused, and to the two free channels.

SHA256SUMS.txt verifies every file in this archive.
