K-UI 53a33ec708ab - Plasma Sword and other large games

This is K-UI itself (KUI/boot.kui and KUI/runtime.kui): the K-UI you
have now, d41a04f40f23, with one change. No reader module is in this
archive.

WHAT YOUR TEST OF 27 NOVEMBER SHOWED
- Tokyo Xtreme Racer boots with Background loading on. That is the
  first check of the last K-UI working on your console: K-UI kept the
  Background worker out of the game's small memory pool and ran the
  game with the reader-only New module instead.
  It gives no report, and cannot. Only the full Background worker
  records the survey, and that worker is what this game has no memory
  for. The reader-only module only reads the disc. Nothing is wrong
  on your side, and I do not need a report from this game.
- Plasma Sword still ended on the reader's stop screen. That was my
  mistake. K-UI checks a game by loading its whole program into
  memory. On the console K-UI has under 8 MB free and Plasma Sword's
  program is 9.5 MB, so K-UI could not load it, could not check it,
  and started it anyway. My tests on the PC had no such limit and
  showed a refusal the console never gave.

WHAT IS NEW
K-UI already reads a game's whole program once from the card, for its
checksum. For a program too large to hold, it now looks during that
same read for the code the Background reader needs. If that code is
not in the program, K-UI does not launch and shows the two lines:

   Background loading cannot recognize this game's system library.
   Set Background loading Off and Virtual VMU Off; saves then need a real VMU

(with Virtual VMU already Off the second line is only "Set Background
loading Off"). It takes about as long as before: it is the same single
read.

A game K-UI can hold is checked exactly as by the K-UI you have now.

READ THIS FIRST
- The first line of Settings, Diagnostics will read
      K-UI 3.0 RC1 SD runtime 53a33ec708ab
  Yours now reads K-UI 3.0 RC1 SD runtime d41a04f40f23.
- This replaces KUI/boot.kui and KUI/runtime.kui, which must stay
  identical. To go back, copy the two files in IF-NEEDED/KUI/ over
  them: they are the K-UI you have now, d41a04f40f23, the same bytes.
- Nothing else changes: the reader modules, games, saves, settings,
  banks and STREAM.TXT stay as they are.

WHAT TO DO
1. Power off. Copy KUI/boot.kui and KUI/runtime.kui from this archive
   over the card's two files.
2. Cold boot the K-UI CD (not Restart K-UI from the menu).
3. Settings, Diagnostics: check the first line (above).
4. Plasma Sword, CD music Off, Background loading On (New (X) or
   Legacy (Y), as you had it).
   Expected: K-UI reads the game for about as long as last time, then
   does not launch and shows the two lines above. No stop screen.
5. Plasma Sword, Background loading Off, Virtual VMU Off.
   Expected: it boots, as on 26 November.
6. Tokyo Xtreme Racer as you ran it on 27 November, and ChuChu Rocket
   with Background loading On - New (X).
   Expected: both start as they do now.

WHAT TO TELL ME
- What step 4 showed. A photo is enough if it differs from the above.
- The diagnostics log, saved after step 4. It names this K-UI and
  shows what K-UI read and found, whichever way step 4 went.
- Anything in steps 5 and 6 that differs from the K-UI you have now.

WHAT THIS DOES NOT COVER
K-UI can rule a large game out only when the reader's code is missing
from it altogether. When the code is there, the rest of the check
still needs the whole program in memory. K-UI then starts the
Background reader unchecked, as it did before.

Of my 426 game programs, 25 are large enough for this to matter
(10 never fit in K-UI's memory; 15 more may not, depending on how
much K-UI has free at that moment):
- Refused by K-UI with the message: Plasma Sword and Star Gladiator 2
  (always), and Shenmue.
- Still started with Background loading On: Evil Dead - Hail to the
  King, Incoming and NBA Hoopz (always); Mortal Kombat Gold, Mat
  Hoffman's Pro BMX, Aero Dancing i, L.O.L., Expendable and
  Millennium Soldier (when K-UI cannot hold them). I expect the
  reader's stop screen for all of them, as before: on my PC, where
  K-UI can hold them, its full check says the Background worker
  cannot run any of them. With Virtual VMU On I expect the stop even
  earlier, on a screen that says "NATIVE VMU PREPARED PROFILE / GAME
  MAP REFUSED": without K-UI's check the reader gets no virtual card.
  Neither screen has been reported from a console for these games.
  They are not affected with Background loading Off.
The last README said 243 games keep running with the reader-only New
module under New (X). On the console that does not hold for the large
ones in this list: K-UI cannot look for that route in a program it
cannot hold.

FILES IN THIS ARCHIVE
KUI/boot.kui
KUI/runtime.kui
  Replaced, identical. K-UI build 53a33ec708ab, payload CRC32
  F8B6923C, 3,836,804 bytes.
IF-NEEDED/KUI/boot.kui
IF-NEEDED/KUI/runtime.kui
  Do not copy these now. The K-UI you have now, d41a04f40f23 (payload
  CRC32 AE58ADA7, 3,836,452 bytes): copy them over KUI/boot.kui and
  KUI/runtime.kui only to go back.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive's KUI folder. No updater package,
reader module, game data or save data is included.

This K-UI passed its host checks, and an independent review that found
no fault in the new check. It was built with Main's release script; of
its 888 compiled units only the launcher differs from the K-UI you
have now. I ran the new check on the 426 original game programs with
K-UI's memory limit applied: it refuses only programs that do not hold
the reader's code, every one of them a program the full check refuses
too, and the checksum it hands to the launch was right every time. It
has not run on a console.

SHA256SUMS.txt verifies every file in this archive.
