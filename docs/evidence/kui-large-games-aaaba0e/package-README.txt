K-UI aaaba0ea3640 - large games are not started with the Background reader

This is K-UI itself (KUI/boot.kui and KUI/runtime.kui): the K-UI you
have now, 53a33ec708ab, with two changes. No reader module is in this
archive. The CD music test comes in its own archive and works with
this K-UI.

WHAT YOUR TEST OF 27 NOVEMBER SHOWED
Your diagnostics log names K-UI 53a33ec708ab and shows the check
working on the console. K-UI read Plasma Sword's 9.5 MB program in
pieces, did not find the code the Background reader needs, and did
not launch:

   Retail boot: Background loading cannot recognize this game's system library.
    Set Background loading Off

You could not read that on the screen because K-UI wrote its launch
timing after it: the reason stood 15 lines above the end of the log.

WHAT IS NEW
1. K-UI no longer starts a large game it cannot check with the
   Background reader on. Until now it started such a game unchecked
   when the reader's code was in the program. Now it does not launch
   and says:

      This game's program is too large for the Background reader yet.
      Set Background loading Off

   (with Virtual VMU On: "Set Background loading Off and Virtual VMU
   Off; saves then need a real VMU").
   Of my 426 game programs this always stops Evil Dead - Hail to the
   King, Incoming and NBA Hoopz. It stops Mortal Kombat Gold, Mat
   Hoffman's Pro BMX, Aero Dancing i, L.O.L., Expendable and
   Millennium Soldier only when K-UI has too little memory free to
   hold them at that moment (a cold boot gives it the most); otherwise
   K-UI checks them in full, as before.
   All of them start with Background loading Off, as before.
2. When K-UI does not launch a game, it writes the reason again at the
   end of the log, after the timing, so it is the last thing you see.

Every other game is checked and started exactly as by the K-UI you
have now.

READ THIS FIRST
- The first line of Settings, Diagnostics will read
      K-UI 3.0 RC1 SD runtime aaaba0ea3640
  Yours now reads K-UI 3.0 RC1 SD runtime 53a33ec708ab.
- This replaces KUI/boot.kui and KUI/runtime.kui, which must stay
  identical. To go back, copy the two files in IF-NEEDED/KUI/ over
  them: they are the K-UI you have now, the same bytes.
- Nothing else changes: the reader modules, games, saves, settings,
  banks and STREAM.TXT stay as they are.

WHAT TO DO
1. Power off. Copy KUI/boot.kui and KUI/runtime.kui from this archive
   over the card's two files.
2. Cold boot the K-UI CD (not Restart K-UI from the menu).
3. Settings, Diagnostics: check the first line (above).
4. Plasma Sword with Background loading On.
   Expected: it does not launch, as on 27 November, and now the log
   on the screen ends with the reason:
      Retail boot: Background loading cannot recognize this game's system library.
       Set Background loading Off
5. Only if one is at hand: Evil Dead - Hail to the King, Incoming or
   NBA Hoopz with Background loading On.
   Expected: it does not launch, and the log ends with "This game's
   program is too large for the Background reader yet." and the
   setting. With Background loading Off it starts as before.

That is all for this K-UI. It needs no report file and no other game.

WHAT TO TELL ME
Only what differs from the above. A photo of the screen is enough.

FILES IN THIS ARCHIVE
KUI/boot.kui
KUI/runtime.kui
  Replaced, identical. K-UI build aaaba0ea3640, payload CRC32
  DA47C74C, 3,837,028 bytes.
IF-NEEDED/KUI/boot.kui
IF-NEEDED/KUI/runtime.kui
  Do not copy these now. The K-UI you have now, 53a33ec708ab (payload
  CRC32 F8B6923C, 3,836,804 bytes): copy them over KUI/boot.kui and
  KUI/runtime.kui only to go back.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive's KUI folder. No updater package,
reader module, game data or save data is included.

This K-UI passed its host checks. It was built with Main's release
script; of its 888 compiled units only the launcher and K-UI's main
program, which repeats the reason, differ from the K-UI you have now.
I ran it on the 426 original game programs with K-UI's memory limit
applied: the only launches that answer differently are the large
programs it used to start unchecked, which it now refuses with the
message above. It has not run on a console.

SHA256SUMS.txt verifies every file in this archive.
