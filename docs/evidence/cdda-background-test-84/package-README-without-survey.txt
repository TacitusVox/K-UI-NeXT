K-UI Test 84, second archive: the same module without the sound-chip reading

Use this only if a game misbehaves with the first archive,
KUI-CD-Drive-Test-84-21NOV2026-0113-CST-4abc63b.zip. It is that archive's
module with point 3 of its README taken out: it never reads the sound
chip while the game runs. The drive answers exactly as in the first.

The first archive's steps, with these differences:
- Do not copy the card's retail-native-dual.kui to your computer
  again: it is the first archive's module. Keep the copy of your own
  that you made before the first archive.
- Its launch line still says SURVEY ON / REPORT SAVES TO SLOT nnn:
  that is the report file, as with the first module, not the
  sound-chip reading.
- This archive replaces only KUI/apps/games/retail-native-dual.kui.
  Keep the first archive's KUI/SURVEY/STREAM.TXT on the card: this
  module saves its reports into the same file. Its reports have no
  row CD SV, but a launch that ends without a saved report looks the
  same for both Test 84 modules: put each launch on your list with
  the module it used. A hang: hold the save first, until the pages
  come up, as in the first archive's README.
- If the game misbehaves with the first archive and not with this
  one, the sound-chip reading is the likely cause; if with both, the
  cause is in what both add to Test 83, most likely the drive's
  answers.
- When you are done with that game, put the first archive's module
  back on the card before you go on with the list.
- Put your own retail-native-dual.kui back at the end.

THERE IS NO MUSIC IN THIS BUILD EITHER.

FILES IN THIS ARCHIVE
KUI/apps/games/retail-native-dual.kui
  Changed for the test only. Test 84 without its survey, build
  4abc63b0cc58, payload CRC32 E00000C9, 313,920 bytes.

Nothing else is in this archive's KUI folder. Your menu, every other
reader, your games, saves and settings are retained. No updater
package, game data or save data is included.

This build passed its own host and linked checks. It has not run on a
console. It writes to the card only inside STREAM.TXT, as Test 83.

SHA256SUMS.txt verifies the file and this README.
