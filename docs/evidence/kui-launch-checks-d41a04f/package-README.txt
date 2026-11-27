K-UI d41a04f40f23 - two launch checks (Tokyo Xtreme Racer, Plasma Sword)

This is K-UI itself (KUI/boot.kui and KUI/runtime.kui), built from Main
as it is now, which is your 3.0 RC1 source, with the two changes your
26 November handoff decided and one more that makes them reliable. No
reader module is in this archive.

WHAT IS NEW
1. Tokyo Xtreme Racer. The Background reader takes 284 KiB of memory
   out of the game's own pool. This game's pool is only 448 KiB and the
   game needs nearly all of it, so it stayed on a black screen. K-UI
   now looks at the pool before the launch and does not start the
   Background worker where it would take more than a quarter of it.
   Nine games of my 426 are affected: Tokyo Xtreme Racer, Tokyo
   Highway Challenge, Shutokou Battle, Buzz 2000 (four regions) and
   two Pro Yakyu Team games.
2. Plasma Sword. The Background reader does not know this game's
   system library, so the game used to start and then end on the
   reader's stop screen. K-UI now says so before the launch. 61 games
   of my 426 are like this.

In both cases K-UI does not launch and shows two lines: the first says
why, the second names the setting that starts the game. For example:

   This game's memory pool is too small for the Background worker.
   Set Virtual VMU Off; saves then need a real VMU

   Background loading cannot recognize this game's system library.
   Set Background loading Off and Virtual VMU Off; saves then need a real VMU

"Saves then need a real VMU" is what the working setting costs: with
Virtual VMU Off there is no virtual card, so the game saves only to a
real VMU in the controller.

3. A third change makes the first two reliable. With Virtual VMU On,
   K-UI could skip its own check of a game and reuse a saved mapping
   that belonged to a different game. It now reuses only a mapping it
   saved for that same game after checking it.

READ THIS FIRST
- The first line of Settings, Diagnostics will read
      K-UI 3.0 RC1 SD runtime d41a04f40f23
  Your current one reads K-UI V2.0.0 SD runtime 64ae07435de5. The name
  changes because Main is the 3.0 RC1 source now.
- This replaces KUI/boot.kui and KUI/runtime.kui, which must stay
  identical. To go back, copy the two files in IF-NEEDED/KUI/ over
  them: they are your current K-UI 64ae07435de5, the same bytes.
- Nothing else changes: the reader modules, games, saves, settings,
  banks and STREAM.TXT stay as they are. Whichever New reader module
  is on the card (Test 84's or your own) can stay.

WHAT TO DO
1. Power off. Copy KUI/boot.kui and KUI/runtime.kui from this archive
   over the card's two files.
2. Cold boot the K-UI CD (not Restart K-UI from the menu).
3. Settings, Diagnostics: check the first line (above).
4. Tokyo Xtreme Racer, CD music Off:
   a. Background loading On - New (X), Virtual VMU On.
      Expected: no launch, and the "memory pool" line. The second line
      is "Set Virtual VMU Off; saves then need a real VMU" if the
      card has the reader-only New module
      (KUI/apps/games/retail-native-standalone.kui), and "Set
      Background loading Off and Virtual VMU Off; saves then need a
      real VMU" if it has not.
   b. The same with Virtual VMU Off.
      Expected with the reader-only module on the card: the game
      starts. K-UI runs it with that module, which takes no memory
      from the game (the log says "using standalone New without
      virtual VMU"). This is new for this game and has not run on a
      console: please tell me whether it boots and plays.
      Expected without it: "Install the standalone New reader, or set
      Background loading Off". Go on with c.
   c. Background loading Off, Virtual VMU Off.
      Expected: it boots, as on 26 November.
5. Plasma Sword, CD music Off:
   a. Background loading On - New (X).
      Expected: no launch and no stop screen, and the two "system
      library" lines above. With Virtual VMU already Off the second
      line is only "Set Background loading Off".
   b. Background loading Off, Virtual VMU Off.
      Expected: it boots, as on 26 November.
6. Two games that must behave as before, Background loading On -
   New (X), set as you ran them: ChuChu Rocket and POD SpeedZone (or
   KOF Dream Match 1999). They should launch as on 26 November.
7. If you have time: one Windows CE game with CD music and one Smash
   Pack game, as you play them now.

WHAT TO TELL ME
- For each step, what you saw. A photo of the screen is enough where
  the text differs from the above.
- The launch reports (last-launch-a.txt, last-launch-b.txt) and the
  diagnostics log, as last time.

WHAT THIS DOES NOT COVER
- Background loading Off with Virtual VMU On: the Standard reader's
  virtual card needs memory of its own too. I did not change or
  examine that route, and it is untested for the nine games; use
  Virtual VMU Off for them.
- Legacy (Y): for these games K-UI answers "Set Background loading
  Off". The reader-only module runs under New (X) only.
- Speed: with Virtual VMU On, the first Background launch of each
  game under this K-UI takes a little longer in K-UI, once, because
  it checks the game itself instead of trusting an older shortcut.
  Later launches of that game are as quick as before.
- 243 other games of the 426 already run with the reader-only New
  module under New (X) and still do. Under Legacy (Y) the reader
  would stop on them after the launch; K-UI now refuses them there
  with the message.

FILES IN THIS ARCHIVE
KUI/boot.kui
KUI/runtime.kui
  Replaced, identical. K-UI build d41a04f40f23, payload CRC32
  AE58ADA7, 3,836,452 bytes.
IF-NEEDED/KUI/boot.kui
IF-NEEDED/KUI/runtime.kui
  Do not copy these now. Your current K-UI 64ae07435de5 (payload CRC32
  1431624F, 3,834,412 bytes): copy them over KUI/boot.kui and
  KUI/runtime.kui only to go back.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive's KUI folder. No updater package,
reader module, game data or save data is included.

This K-UI passed its host checks, and an independent review whose
findings are corrected in it. It was built with Main's release script;
of its 888 compiled units only the launcher differs from a build of
Main without these changes. Run on the 426 original game programs with
Test 84's module and with the release's, the launcher decides every
launch as before except the ones described above. It has not run on a
console.

SHA256SUMS.txt verifies every file in this archive.
