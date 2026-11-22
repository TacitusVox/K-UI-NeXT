K-UI 64ae07435de5 - one K-UI for both threads

This is the K-UI you run now (1c71ba3afa17, the Windows CE thread's)
with your Test 83 GUI fixes added back: the 160-slot Background map
for fragmented discs, the standalone New reader for games whose SDK
pool overlaps, the cached mappings, and the extra SDK layouts it
recognises before a launch. Nothing of the Windows CE thread's K-UI is
left out: the CE CD music routes, the Smash Pack modules inside it
(the same bytes as in your copy), your Standard binding and the launch
timing are all as before.

Why: under 1c71ba3afa17, which lacks your Test 83 GUI fixes, Test 84's
module was refused for Bangai-O, MDK2 and Plasma Sword ("Background
map exceeds 64 slots"). With this K-UI the map has its 160 slots again.

How it was built: the same way as 1c71ba3afa17. I first rebuilt
1c71ba3afa17 from its source and got your file byte for byte, then
built this one the same way from the merge of both threads.

READ THIS FIRST
- This replaces K-UI itself: KUI/boot.kui and KUI/runtime.kui, which
  must stay identical. Before step 1, copy your current
  KUI/boot.kui and KUI/runtime.kui to your computer. Putting those
  two back restores 1c71ba3afa17 exactly. (IF-NEEDED/KUI/ in this
  archive has the same two files, in case you have no copy.)
- Nothing else changes: every reader module, your games, saves,
  settings, banks and STREAM.TXT stay as they are.

WHAT TO DO
1. Power off. Copy KUI/boot.kui and KUI/runtime.kui from this archive
   over the card's two files.
2. Cold boot the K-UI CD (not Restart K-UI from the menu).
3. Check it is this K-UI: Settings, Diagnostics; the first line says
   K-UI V2.0.0 SD runtime 64ae07435de5.
4. Test 84 again, as its README says (CD music Off, virtual VMU Off,
   New (X), your list), with Test 84's module as your New reader
   module: if you already put your own retail-native-dual.kui back,
   copy Test 84's KUI/apps/games/retail-native-dual.kui to the card
   again. Keep the same STREAM.TXT on the card; the new launches fill
   its next slots. The games:
     Bangai-O, MDK2, Plasma Sword;
     Omikron / The Nomad Soul: last time it did not go through Test
     84's module; this K-UI sends it there, so its CD rows will say
     whether it plays its track;
     POD SpeedZone and Tokyo Xtreme Racer, after copying each game's
     folder off the card and back (their images did not open last
     time: Tokyo Xtreme Racer's had a damaged track file, POD's
     executable read as zeros).
5. Two or three Windows CE games with CD music On, as you play them
   now (for example Armada, a World Party group game, a Bust-A-Move 4
   group game): they should behave exactly as under 1c71ba3afa17.
6. One Smash Pack game, as you play it now.
7. When you are done, keep this K-UI if everything behaves; put your
   own retail-native-dual.kui back as Test 84's README says.

WHAT TO TELL ME
- STREAM.TXT and your list, as for Test 84.
- Anything that behaves differently from 1c71ba3afa17 in a Windows CE
  or Smash Pack game. One difference is known: if a Windows CE game's
  map is full, the refusal is worded as for the other games ("map
  exceeds N slots: ..."); it refuses in the same cases.

FILES IN THIS ARCHIVE
KUI/boot.kui
KUI/runtime.kui
  Replaced, identical. K-UI build 64ae07435de5, payload CRC32 1431624F,
  3,834,412 bytes.
IF-NEEDED/KUI/boot.kui
IF-NEEDED/KUI/runtime.kui
  Do not copy these now. Your current K-UI 1c71ba3afa17 (payload
  CRC32 2E38CCC2, 3,816,892 bytes), the same bytes as your file:
  copy them over KUI/boot.kui and KUI/runtime.kui only to go back.
INVENTORY.json
  The files above with their full hashes, and what stays on the card.

Nothing else is in this archive's KUI folder. No updater package,
reader module, game data or save data is included.

This K-UI passed its host checks: both threads' modules rebuild byte
for byte from its source, it admits them, and it routes the CD music
games as your Test 83 K-UI did. It has not run on a console.

SHA256SUMS.txt verifies every file in this archive.
