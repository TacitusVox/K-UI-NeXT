K-UI Test 89 - CD music, part 6: the player's rough edges

WHAT TEST 88 SHOWED (your report file of 27 November)
- Two sound-driver builds the console had never run played their
  music cleanly: Mr. Driller's and Tetsuman Menkyokaiden's. King of
  Fighters was exactly as under Test 87, and Bangai-O was silent with
  the reason I expected.
- Nasubi no Heya was slow, Maken X asked for no CD music, and one
  Pro Yakyu disc left no record. None of those is touched here.

WHAT TEST 89 CHANGES
Three things you may notice, and three you will not.
1. The drive keeps time with the music. The console's own timer runs
   a quarter of a percent slow against the sound chip. Since Test 87
   the music follows the sound chip; the position the game reads back
   from the "drive" still followed the timer, so it fell 2.5
   thousandths of a second behind the music for every second played.
   Now both follow the sound chip, and the "drive" waits the tenth
   of a second the music buffer takes to fill before it starts to
   move, as a real drive waits while it finds the track (it never
   waits more than a second). So a track ends for the game when its
   last note has sounded. In most games
   you will hear no difference. Where a game times something to its
   music, or plays a track once and then the next, it should now be
   right.
2. Stereo as the game sets it. King of Fighters and Tetsuman
   Menkyokaiden tell the sound chip to place the disc's two channels
   half way out to each side, not fully left and right; Tetsuman has
   them the other way round as well. The music now follows that. In
   games that leave both channels in the middle (Mr. Driller, POD
   SpeedZone, Nasubi no Heya) nothing changes: left stays left and
   right stays right, because I do not know whether a real disc then
   plays as one channel.
3. Nothing else about the music changes: the same builds, the same
   buffer, the same volume.
In the report only:
4. Why King of Fighters' music stream from the card breaks ten times
   a second (the reader counted the reasons all along and never
   printed them).
5. How close the music buffer comes to running empty in the middle of
   a piece, apart from its first second.
6. Whether two unused timers of the sound chip can be read. That is
   for the next kind of sound driver (AUDIO64: MDK2, Toy Commander).

WHAT I DO NOT KNOW YET (this test answers it)
- Whether the stereo in King of Fighters and Tetsuman is right by
  ear. If the two sides sound swapped in a game where you know which
  side an instrument or an effect belongs on, that matters: tell me.
- The answers to 4, 5 and 6, from the reports.

WHAT IS EXPECTED, AND NOT A FAULT
- As before: the music is compressed four to one, the buffer is
  small, the music stops for a moment when the game loads, and the
  first fraction of a second of a piece is not heard.
- King of Fighters and Tetsuman sound narrower than under Test 88:
  that is the game's own setting.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of
  KUI/apps/games/retail-native-dual.kui, where your card now has the
  Test 88 file. Your own file is the one you kept on your computer:
  put it back when you are done, as after Test 88.
- K-UI: the first line of Settings, Diagnostics must read
     K-UI 3.0 RC1 SD runtime aaaba0ea3640  (the K-UI archive of 27
                                            November), or
     K-UI 3.0 RC1 SD runtime 53a33ec708ab  (the one before it).
  Both are right for this test. With any other build, stop and tell
  me.
- For every game, in Games / Options, as in Test 88:
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
 1. King of Fighters Dream Match 1999   (stereo half way out; the
                                         stream's reasons)
 2. Tetsuman Menkyokaiden               (stereo half way out, sides
                                         as the game sets them)
 3. ChuChu Rocket                       (should sound as before)
 4. Mr. Driller                         (should sound as before)
 5. POD SpeedZone, one race             (should sound as before)
If you want to add one for a driver build still untested, any of:
Quake III Arena, Sega GT Homologation Special, Jet Coaster Dream 2.

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
4. Play three to five minutes where the game has CD music. If you
   can, let one piece run for two minutes or more.
5. Save the report while music plays, or should be playing (not
   while the game loads): controller in port A, both triggers and
   A+B+X+Y for about three seconds. Two pages headed TOY TEST 81 take
   turns (the heading is Test 81's; the saved report says TEST 89),
   each with REPORT SAVED TO SD. Press Start: the console restarts
   into K-UI.
6. The same with the next game of the list, from step 3.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list and what you heard.

WHAT TO LISTEN FOR
A few words for each game are enough:
1. King of Fighters and Tetsuman: is the music still clearly in
   stereo, only narrower? Does anything sound as if left and right
   were swapped?
2. The other games: the same as under Test 88?
3. Are the game's own sounds (effects, voices) as they should be?
4. Does the game run at its normal speed?
5. Anything that is timed to the music, or a piece that plays once
   and stops: does it end where it should?

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
  Changed for the test only. Test 89, build d4d5871818df, payload CRC32
  2C7E0D17, 325,092 bytes. The saved report says TEST 89; the launch
  record above it in the file still says SURVEY 83 and BUILD
  d4d5871818df (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  6c8b10fdc948cc69dd63e48214f2cdba.
IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the sound output, with
  Test 84's read-only look at the sound chip: payload CRC32 870A02D7,
  319,396 bytes. Its saved report says TEST 85.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive. Your K-UI, every other reader, your
games, saves and settings are retained. No updater package, game
data or save data is included.

This build passed its host and linked checks, and its changes were
read by independent reviewers whose findings were each checked
again; what they found is fixed in this file. The sound output was
run on a model of the sound chip, its bus, its driver's clock and
the card, with the chip a quarter of a
percent ahead of the console's timer as on your console; and the
output as built for this file was run over the memory each of your
27 driver files leaves after its own start-up. It has not run on a
console. It writes to the card only inside STREAM.TXT, as Tests 83
to 88. It writes to the sound chip only where a MANATEE-lineage
driver runs, as Test 88 does; it reads two more of the chip's
registers than Test 88 and writes to neither.

SHA256SUMS.txt verifies every file in this archive.
