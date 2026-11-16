K-UI Toy Commander Test 78 - reads of any size through the fast reader

This is the first of the changes you told me to go ahead with. It is
still the Toy Commander test, started from the same menu entry as
before. The build for the two Evolution games is Test 79, in its own
archive; it is this build plus one more change.

Your two runs of Test 77 are the reference: the second movie showed
1,959 and 1,965 of its 1,968 frames, the first all 331, and nothing
looked wrong in the level or the loading.

WHAT CHANGED
Up to now the fast reader handed the game only reads of up to 60
sectors. A larger read at the place it had read ahead to made it throw
that away and step aside. Evolution 2 reads in pieces of 73 and 50
sectors by turns, so every second read would have done that.

This build has one change: a read of any size is served by the fast
reader, a slice at a time as the sectors arrive. Reads of up to 60
sectors go exactly the way they went in Test 77. That is every refill
of Toy Commander's movies.

Toy Commander itself has large reads too: the first big read of each
movie, and some of its loading. Those now go through the fast reader,
which they did not before. So this run is to see that nothing you had
got worse.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed file.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

THE RUN
As your second run of Test 77: cold boot the K-UI CD, tap B during the
countdown, Card Tools, your raw Toy Commander GDI, Options, Advanced,
Tests, "66 SCI movie playback", CD Music and virtual VMU Off. Let both
movies play without skipping. Then go on into the game: through the
menus, start a game, and play the first level for about a minute. Then,
at a moment when nothing is loading, hold both triggers and A+B+X+Y for
about three seconds, and after the save message press Start.

WHAT TO TELL ME
- Did both movies look like your last runs, or better or worse? Look at
  the very start of each movie in particular: that is where the large
  read is.
- Did the menus and the level load and play normally? Tell me about any
  long pause, any error screen (it would say IMAGE READ FAILED), any
  wrong picture or sound, and whether loading felt as long as you know
  it, shorter or longer.
- If the screen after the gesture says SAVE FAILED, the game was reading
  at that moment. Film both pages it shows, and run it again.

Send the newest file in /reports/toy/ whose name starts with 66. The
text inside says TEST 78 and BUILD 1d0e5bab78ba. The last value of the
PIPE row is 3BD.

To go back to the build you ran last, copy its file back from the
Test 77 package.

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 78, build 1d0e5bab78ba, payload CRC32 CAC19221. Replaces the
  Test 77 module at the same path. Card Tools loads it from there; its
  menu entry is still called "66 SCI movie playback".

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the two modules and this README.
