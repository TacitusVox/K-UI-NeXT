K-UI Test 88 - CD music, part 5: every MANATEE sound driver

WHAT TEST 87 SHOWED (your two report files of 27 November)
- King of Fighters played its music steadily, ChuChu Rocket played
  26 minutes of it (a piece of over seven minutes among them), and
  POD SpeedZone still broke up where it loads through a race.
- Test 87 makes sound in three games because it knows three builds
  of the "MANATEE" sound driver, the driver most Dreamcast games with
  CD music use. Your library has 27 builds of that driver.

WHAT TEST 88 CHANGES
- It gives music to every one of the 27 builds, in four generations:
     MANATEE 2            (15 builds; ChuChu Rocket's is one)
     MANATEE 1, newer     (6 builds; King of Fighters' and POD's)
     MANATEE 1, older     (4 builds, from 1998)
     NAMIHEI              (2 builds)
- Nothing else changes. The music's timing, its buffer and its
  volume are Test 87's.
- For this I read all 27 driver files of your sound-driver archive,
  and ran each one on a model of the sound processor on my machine:
  its start-up and 3,528 of its timer interrupts. Every build keeps
  its clock, its free memory and its channel records where its
  generation does. Two of the oldest builds count time a quarter of a
  percent differently from all the others; the reader allows for it.
- A game whose driver gives every channel to sound effects and none
  to streams (Bangai-O) stays silent, and the report now says that
  in so many words.

WHICH GAMES
Of the 82 CD-music games in your library that this kind of reader
can start, about 56 now have a driver it gives music to (23 under
Test 87), and some of 16 more that carry two kinds of driver.
Games with the "AUDIO64" driver stay silent (MDK2, Toy Commander):
that is a later step.

WHAT I DO NOT KNOW YET (this test answers it)
- Whether the music plays in the builds no console has run yet. The
  model says each build looks like its generation; whether a game
  uses the two sound channels the reader takes, only your console
  can say. If a game does, the reader notices, stays silent for
  that piece, and the report says so.
- Which driver runs on discs that carry two.
- For the older MANATEE 1 and NAMIHEI: everything. No build of
  either has been tried on a console at all. Few games the reader
  can start carry one (see the list below).

WHAT IS EXPECTED, AND NOT A FAULT
- As under Test 87: the music is compressed four to one, the buffer
  is small (0.18 or 0.28 seconds), so the music stops for a moment
  when the game loads, and the first fraction of a second of a
  piece is not heard.
- Bangai-O: no music (see above). Its report is still useful.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of
  KUI/apps/games/retail-native-dual.kui, where your card now has the
  Test 87 file. Your own file is the one you kept on your computer:
  put it back when you are done, as after Test 87.
- K-UI: the first line of Settings, Diagnostics must read
     K-UI 3.0 RC1 SD runtime aaaba0ea3640  (the K-UI archive of 27
                                            November), or
     K-UI 3.0 RC1 SD runtime 53a33ec708ab  (the one before it).
  Both are right for this test. With any other build, stop and tell
  me.
- For every game, in Games / Options, as in Test 87:
     Background loading: On - New (X)
     CD music: Off
  and Virtual VMU Off. "CD music: Off" is right: that option selects
  K-UI's older music route, which is not this test. The music here
  comes from this file.
- KUI/SURVEY/STREAM.TXT is a new blank report file with 100 slots (its
  ID is below). Copy it over the old one, as it is: do not open and
  save it in an editor. You have sent me the old one already.
- Keep a short list as you go: for each launch, in order, the game
  and which file was on the card (main or no-sound).

WHICH GAMES TO TRY (in this order; as many as you have time for)
Each is a game this reader can start. A few minutes each is enough.
 1. King of Fighters Dream Match 1999, or ChuChu Rocket
       The control: it should be exactly as under Test 87.
 2. Maken X                       (a MANATEE 2 build new to the console)
 3. Mr. Driller, or Sega GT Homologation Special
                                  (the most common other MANATEE 2 build)
 4. Tetsuman Menkyokaiden         (a newer MANATEE 1 build new to the
                                   console)
 5. Bangai-O                      (expected silent; one short launch
                                   and a report)
 6. Nasubi no Heya                (carries an older MANATEE 1 build
                                   beside a newer one: the report says
                                   which one the game runs)
 7. Pro Yakyu Team de Asobou Net  (carries NAMIHEI beside MANATEE 2:
                                   the same question)
Any other game with CD music is welcome too: the report names its
driver, and says why if it stays silent. If a game of this list
does not start under Background loading: On - New, skip it and note
it.

WHAT TO DO
1. Power off. Merge this archive's KUI folder into the card's KUI
   folder. It replaces two files: KUI/apps/games/retail-native-dual.kui
   and KUI/SURVEY/STREAM.TXT. Leave the IF-NEEDED folder on your
   computer.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Check the
   Diagnostics line (above).
3. Games, the first game of the list, the options above, launch.
   The launch line says SURVEY ON / REPORT SAVES TO SLOT 000 (001 on
   the next launch, and so on).
4. Play three to five minutes where the game has CD music: menus,
   a stage or a match, the next one.
5. Save the report while music plays, or should be playing (not
   while the game loads): controller in port A, both triggers and
   A+B+X+Y for about three seconds. Two pages headed TOY TEST 81 take
   turns (the heading is Test 81's; the saved report says TEST 88),
   each with REPORT SAVED TO SD. Press Start: the console restarts
   into K-UI.
6. The same with the next game of the list, from step 3.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list and what you heard.

WHAT TO LISTEN FOR
A few words for each game are enough:
1. Is there music? Is it the right music, steady, in stereo, and
   loud enough against the game's own sounds?
2. Are the game's own sounds (effects, voices) as they should be?
   A sound effect or a voice that is cut off or missing matters
   most: it would mean the game uses a channel the reader took.
3. Does the game run at its normal speed?
4. The control game: the same as under Test 87?

IF NEEDED (one more file in the IF-NEEDED folder; only for this)
A game hangs, crashes, runs badly, or its own sounds go wrong: if the
game still reacts, save the report first (hold the buttons for up to
ten seconds). Then copy
  IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui
over the card's file and run that game once more, with a report.
This file reads the music from the card but never writes to the
sound chip, so it is silent: it tells me whether the reading or the
sound is the cause. Put the main file back afterwards (this
archive's KUI/apps/games/retail-native-dual.kui).

Do not leave either file installed for normal use: no virtual VMU,
no prefetch.

FILES IN THIS ARCHIVE
KUI/apps/games/retail-native-dual.kui
  Changed for the test only. Test 88, build 9e105c4e8d25, payload CRC32
  49AEAAE1, 325,060 bytes. The saved report says TEST 88; the launch
  record above it in the file still says SURVEY 83 and BUILD
  9e105c4e8d25 (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  B0D310936D1B8EE970AA887B0B79452F.
IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the sound output, with
  Test 84's read-only look at the sound chip: payload CRC32 AA84D60F,
  319,396 bytes. Its saved report says TEST 85.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive. Your K-UI, every other reader, your
games, saves and settings are retained. No updater package, game
data or save data is included.

This build passed its host and linked checks. The sound output was
run on a model of the sound chip, its bus, its driver's clock and
the card, for a driver of each of the 27 builds; and the output as
built for this file was run over the memory each of your 27 driver
files leaves after its own start-up. It has not run on a console.
It writes to the card only inside STREAM.TXT, as Tests 83 to 87. It
writes to the sound chip only where a MANATEE-lineage driver runs:
to the unused end of that driver's own memory, and to two channels
the driver's own records call free at that moment.

SHA256SUMS.txt verifies every file in this archive.
