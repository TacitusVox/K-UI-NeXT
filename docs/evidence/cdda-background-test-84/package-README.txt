K-UI Test 84 - CD music, part 1: the drive plays (no sound yet)

This is your Test 83 reader (your build 1d80f771f081: fragmented
discs, cached launch, the L+R+ABXY save during mixed controller/VMU
polls) with three things added for games that ask for CD music:

1. The drive now answers like a drive that is playing the track the
   game asked for: "playing" while it plays, the position moving
   with the music, a track that ends when its time is up, repeats,
   pause and resume. Until now the reader always answered "not
   playing", so, by my reading of their programs, MDK2 kept asking
   for its track again, Bangai-O kept its music volume at zero, and
   a resume could restart the track.
2. It keeps saying "playing" while the game loads data during music.
3. While a track "plays", it looks at the sound chip, reading only:
   which sound driver the game runs, whether the game itself uses
   the two sound channels the music will need in Test 85, and, with
   the AUDIO64 driver (MDK2, Toy Commander), whether the game uses
   the top 48 KiB of sound memory. It writes nothing to the sound
   chip.

To make room, the report leaves out Test 83's rows BIG, K and P; its
new rows begin with CD.

THERE IS NO MUSIC IN THIS BUILD. You hear what you heard with CD
Music Off. Test 85 adds the sound; this round tells me, per game,
how it asks for its music and what its sound driver does, so that
Test 85 is right the first time.

One thing may look new: a game that waits for a piece of CD music to
end (a jingle after a level, an intro) now waits as long as that
piece lasts, in silence, as it would on a real drive. Before, it went
on at once. The wait can be as long as the whole track, several
minutes for some: give it that long before you call it a hang, and
note where it happens. It is expected. If it never ends, save the
report as in step 5, holding until the pages come up, before you
power off.

READ THIS FIRST
- This is a TEST FILE, not an update. It is for the K-UI you
  installed for Test 83 (GUI 4d4dcff77494) and goes in the place of
  your New reader module, like Tests 79 to 83. The games below are
  the ones that K-UI sends to this module with the virtual VMU off.
  Games it sends to the standalone New reader are not touched: that
  file is not in this archive.
- Before step 1, copy your own KUI/apps/games/retail-native-dual.kui
  from the card to your computer. Put that copy back at the end.
- Virtual VMU OFF for every game. New (X). CD Music Off.
- There is a second file: KUI/SURVEY/STREAM.TXT, a new blank report
  file with 100 slots (its ID is below; it is not Test 83's). Copy it
  over the old one, as it is: do not open and save it in an editor.
  Keep your Test 83 file on your computer if you still want it, not
  on the card.
- Keep a list as you go: for each launch, in order, the game and
  which module was on the card (this one, the second, or your own).
  A launch that ends without a saved report (a crash, or one you
  power off) leaves a slot that looks the same for this module and
  the second: your list is how I tell them apart.

WHAT TO DO
1. Power off. Copy your own module to your computer (above). Then
   merge this archive's KUI folder into the card's KUI folder: it
   replaces KUI/apps/games/retail-native-dual.kui and
   KUI/SURVEY/STREAM.TXT.
2. Cold boot the K-UI CD (not Restart K-UI from the menu). Games, a
   game from the list below, New (X), virtual VMU Off.
3. The launch line as in Test 83: SURVEY ON / REPORT SAVES TO SLOT 000
   (001 on the next launch, and so on).
4. Play for a few minutes where the game plays CD music: the title
   screen, a level or a race, a pause and a resume, a level's end if
   there is a jingle, and one load (the next level or race).
5. Save the report while the game's music would be playing (not
   while it loads): controller in port A, both triggers and A+B+X+Y
   for about three seconds. Two pages headed TOY TEST 81 take turns
   (the pages' heading is Test 81's, as in Test 83; the saved report
   says TEST 84), each with REPORT SAVED TO SD. Press Start: the
   console restarts into K-UI.
6. The next game: from step 2.
7. When you are done: power off, copy KUI/SURVEY/STREAM.TXT to your
   computer and send it as it is, with your list. Put your own
   retail-native-dual.kui back.

WHICH GAMES
This is the long round promised with Test 83: as many of these as
you have on the card, one launch each (two for the first two, the
second launch is the warm one):
  1. ChuChu Rocket        (MANATEE driver, the newer family)
  2. Bangai-O             (the same family)
  3. MDK2                 (AUDIO64 driver; see the note below)
  4. Omikron / The Nomad Soul (one CD music track on its disc; I
     do not know whether the game plays it: its CD rows will say)
  5. Toy Commander        (AUDIO64, older)
  6. King of Fighters Dream Match 1999 (MANATEE, older)
  7. Tokyo Xtreme Racer   (MANATEE, oldest)
  8. POD SpeedZone        (MANATEE, older)
  9. Plasma Sword         (MANATEE, older)
 10. Evolution 2          (as in Test 83, to compare; it has no CD
     music track this reader plays, so its CD rows should stay at 0)
If a game does not offer New (X), or its launch stops, go on with the
next one and tell me which.

MDK2: in Test 83 its audio tracks (the disc has 31 tracks, 28 of them
audio, 27 of those its CD music) did not fit in the reader's map: the
launch record said MAPPED 0. Their files are on your card, but in
more pieces than the map has room for. This build does not need the
audio, so run it anyway: it shows what MDK2 asks for. For Test 85
MDK2's files would need to be in fewer pieces; copying its folder to
the card again may do it, but leave that until Test 85.

WHAT TO TELL ME
- Send STREAM.TXT and your list. That is the whole report.
- For each game: did it start, play, pause and resume, and load as
  under Test 83 (or, for a game you did not run in Test 83, as with
  your own module)? Anything that waited where it used to go on, and
  for how long. Any hang, crash, stutter, or wrong picture.

IF SOMETHING GOES WRONG
- The SURVEY OFF lines, the stop screens at launch and the pages
  headed REPORT NOT SAVED are Test 83's; its README says what each
  means. If the pages say REPORT NOT SAVED, that run's numbers are
  not on the card: note it on your list. You need not photograph the
  pages.
- A hang: first hold the save as in step 5 until the pages come up,
  for up to ten seconds. If they say REPORT SAVED TO SD, the report
  is saved: note it on your list and press Start. If nothing comes,
  power off and note the game and roughly when (in music, at a
  pause, at a load).
- A crash: power off and note the same.
- After a hang or a crash, if you want, try the same game with the
  second archive (below). For a game you did not run in Test 83,
  also try it once with your own module, so that I know whether the
  reader itself is the cause. Then put this archive's module back on
  the card before you go on with the list.

THE SECOND ARCHIVE (only if a game misbehaves)
KUI-CD-Drive-Test-84-without-survey-21NOV2026-0113-CST-4abc63b.zip is the
same module without point 3: it never reads the sound chip while the
game runs. Its launch line still says SURVEY ON / REPORT SAVES TO
SLOT nnn: that is the report file, as here. It holds only the
module: keep this archive's STREAM.TXT on the card, both modules
save into it. If a game misbehaves with
the first and not with the second, the sound-chip reading is the
likely cause; if with both, the cause is in what both add to Test 83,
most likely the drive's answers. After that game, put this archive's
module back on the card before you go on with the list.

Do not leave this file installed for normal use: no virtual VMU, no
prefetch, no CD music.

FILES IN THIS ARCHIVE
KUI/apps/games/retail-native-dual.kui
  Changed for the test only. Test 84, build 4abc63b0cc58, payload CRC32
  BAFAAF7C, 315,744 bytes. The saved report says TEST 84; the launch
  record above it in the file still says SURVEY 83 and BUILD 4abc63b0cc58
  (that line's text is Test 83's).
KUI/SURVEY/STREAM.TXT
  Replaced. A new blank report file, 154,112 bytes of text, ID
  B5829D27CAF13A94DEAFD593D1FDC00A.

Nothing else is in this archive's KUI folder. Your menu, every other
reader, your games, saves and settings are retained. No updater
package, game data or save data is included.

This build passed its own host and linked checks. It has not run on
a console. It writes to the card only inside STREAM.TXT, as Test 83.

SHA256SUMS.txt verifies the two files and this README.
