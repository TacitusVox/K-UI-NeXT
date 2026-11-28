K-UI Test 87 - CD music, part 4: steady music in King of Fighters

WHAT TEST 86 SHOWED (your report file of 27 November)
- ChuChu Rocket played its music for a minute with one short break.
  The compressed music buffer in the sound driver's spare memory
  works, and nothing else touched that memory.
- King of Fighters: what you heard as "normal, soft, normal, soft"
  was the music stopping and starting twice a second. Its report
  counts 165 starts in 83 seconds, every one for the same reason:
  the music buffer ran empty. The reader allowed this game's music
  half the time per call that it allows ChuChu Rocket's (an old rule
  of the reader for games built with the early SDK). That brought in
  little more than half of the music the game plays, so the buffer
  ran empty, started over, and ran empty again.
- Both reports show something I did not know: the console's timer
  runs a quarter of a percent slow against the sound chip's clock.
  The reader timed the music by that timer. In a piece that plays
  for more than about a minute without a break, the sound chip
  would have overtaken the reader. Your ChuChu Rocket pieces were
  shorter than that.

WHAT TEST 87 CHANGES
- The music has the full time allowance in every game.
- The music is timed by the sound chip's own clock, as the game's
  sound driver counts it, no longer by the console's timer. The
  short gap every few minutes that Test 86 needed is gone.
- Bringing the music into the sound chip takes less of the console's
  time than under Test 86: about a third fewer steps of the processor
  for the same music. What that saves on the console, the report
  will say.
- When the music does have to start over, the old sound is switched
  off first, and silence between two tracks is written as fast as
  music.

It makes sound in the same three games:
   ChuChu Rocket
   King of Fighters Dream Match 1999
   POD SpeedZone
In every other game it stays silent and the report says why.

WHAT I DO NOT KNOW YET (this test answers it)
- Whether King of Fighters now plays its music steadily. By the
  numbers of your report it should: the full allowance carries about
  twice what the game plays.
- Whether the sound driver's count is a good clock for minutes on
  end. Your two reports say it is. If it were not, the music would
  start over again and again, and after the ninth time in one piece
  stay silent until the next piece (the report then says so).
- Whether King of Fighters still runs at its normal speed. The music
  takes more of the console's time per call than under Test 86,
  about as much as it took in ChuChu Rocket there.

WHAT IS EXPECTED, AND NOT A FAULT
- The music is a little less clean than from a CD: it is compressed
  four to one. A soft hiss on quiet passages is possible.
- The buffer is still small: 0.18 seconds of music in ChuChu Rocket,
  0.28 in King of Fighters and POD. When the game loads from the
  card, the game's reading comes first: the music stops for a moment
  and comes back at the place the drive would have reached. POD
  loads all through a race, so its music will still break up there.
- The first fraction of a second of a piece is not heard.

READ THIS FIRST
- This is a TEST FILE, not an update. It goes in the place of
  KUI/apps/games/retail-native-dual.kui, where your card now has the
  Test 86 file. Your own file is the one you kept on your computer:
  put it back when you are done, as after Test 86.
- K-UI: the first line of Settings, Diagnostics must read
     K-UI 3.0 RC1 SD runtime aaaba0ea3640  (the K-UI archive of 27
                                            November), or
     K-UI 3.0 RC1 SD runtime 53a33ec708ab  (the one before it).
  Both are right for this test. With any other build, stop and tell
  me.
- For every game, in Games / Options, as in Test 86:
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

WHAT TO DO
1. Power off. Merge this archive's KUI folder into the card's KUI
   folder. It replaces two files: KUI/apps/games/retail-native-dual.kui
   and KUI/SURVEY/STREAM.TXT. Leave the IF-NEEDED folder on your
   computer.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Check the
   Diagnostics line (above).
3. Games, King of Fighters Dream Match 1999, the options above,
   launch. The launch line says SURVEY ON / REPORT SAVES TO SLOT 000
   (001 on the next launch, and so on).
4. Play about five minutes where the game has CD music, as in Test
   86: the character select, a fight to its end, the next round.
   If you can, let one piece of music run for three minutes or more
   without a pause or a load (a fight or a menu left alone will do):
   that is the test of the new clock.
5. Save the report while music plays, or should be playing (not
   while the game loads): controller in port A, both triggers and
   A+B+X+Y for about three seconds. Two pages headed TOY TEST 81 take
   turns (the heading is Test 81's; the saved report says TEST 87),
   each with REPORT SAVED TO SD. Press Start: the console restarts
   into K-UI.
6. The same with ChuChu Rocket (the title screen and menus, then a
   stage left running for three minutes or more), from step 3.
7. If you have the time, the same with POD SpeedZone (the menus, one
   race with the load before it). It was not run under Test 86.
8. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list and what you heard.

WHAT TO LISTEN FOR
A few words for each game are enough:
1. King of Fighters: is the music steady now? If it still drops
   out: how often, and when (all the time, at the start of a piece,
   only around loading)?
2. In a piece that runs for minutes: does it stay clean to the end,
   or does it start to crackle, jump or drop out after a minute or
   two?
3. Does the game itself run at its normal speed, King of Fighters
   above all? Any slowdown or stutter in the picture?
4. ChuChu Rocket: the same as under Test 86, better, or worse?
5. Are the game's own sounds (effects, voices) as they should be?
6. Is the music loud enough against them, and in stereo?

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
  Changed for the test only. Test 87, build a54a75c8645b, payload CRC32
  0B47EDEE, 324,900 bytes. The saved report says TEST 87; the launch
  record above it in the file still says SURVEY 83 and BUILD
  a54a75c8645b (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  9C05737ABA3BD4DF58FDFE369A06868C.
IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui
  Do not copy this now. The same build without the sound output, with
  Test 84's read-only look at the sound chip: payload CRC32 6CE6E39D,
  319,396 bytes. Its saved report says TEST 85.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive. Your K-UI, every other reader, your
games, saves and settings are retained. No updater package, game
data or save data is included.

This build passed its host and linked checks. The sound output was
run on a model of the sound chip, its bus, its driver's clock and
the card, which takes no write but the permitted ones and compares
what is played with the track. With calls a picture frame apart,
the model's buffer runs dry twice a second at the rate King of
Fighters had under Test 86, as on your console, and holds at this
test's. It has not run on a console. It writes to the card only
inside STREAM.TXT, as Tests 83 to 86. It writes to the sound chip
only in the three games above: to the unused end of the sound
driver's own memory, and to the two channels your Test 84 runs
found free.

SHA256SUMS.txt verifies every file in this archive.
