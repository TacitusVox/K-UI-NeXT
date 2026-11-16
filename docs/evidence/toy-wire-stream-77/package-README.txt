K-UI Toy Commander Test 77 - the reader decides by itself

Your last run (report 66-11) is the reference: the second movie showed
1,953 of its 1,968 frames, the first all 331.

WHAT CHANGED
Up to that build the fast reading was switched on by Toy Commander
itself. The reader knew the names of the game's four movie files and
listened to the game's own movie player, and it read ahead only while
that player said a movie was running. None of that can be taken to
another game.

This build has one change. The reader now watches the game's reads and
decides by itself: when a read begins exactly where the one before it
ended, it starts reading ahead from there, and it stops when the game
reads somewhere else. The movie files and the movie player no longer
decide it. Everything else is the build you ran last: the same reader,
the same check on every sector, and change 5 left out, as your two runs
showed it gains nothing.

It is still the Toy Commander test, started from the same menu entry. It
is the first step toward other games, not a build for them yet.

WHY THERE ARE TWO RUNS
Because the reader no longer asks whether a movie is playing, the same
rule now also serves the game's ordinary loading: menus and levels. So
there are two things to see. Run 1: do the movies still play as in your
last run. Run 2: do the menus and a level load and play normally.

INSTALL
1. Power off the Dreamcast. Merge this archive's KUI folder into the KUI
   folder at the root of the SCI microSD card, replacing the listed file.
2. Card Tools must be build 09c9a4b686d4, the one from the Test 66 package.
   The copy in this archive is that same file, byte for byte.

RUN 1 - the movies
Exactly as before: cold boot the K-UI CD, tap B during the countdown,
Card Tools, your raw Toy Commander GDI, Options, Advanced, Tests,
"66 SCI movie playback", CD Music and virtual VMU Off. Let both movies
play without skipping. Then hold both triggers and A+B+X+Y for about
three seconds, and after the save message press Start.

RUN 2 - into a level
Start the same test again and let both movies play without skipping.
Then go on into the game as you normally would: through the menus,
start a game, and play the first level for about a minute. Then, at a
moment when nothing is loading, hold both triggers and A+B+X+Y for
about three seconds, and after the save message press Start.

WHAT TO TELL ME
- Run 1: did both movies look like your last run, or better or worse?
- Run 2: did the menus and the level load and play normally? Tell me
  about any long pause, any error screen (it would say IMAGE READ
  FAILED), any wrong picture or sound, and whether loading felt as long
  as you know it, shorter or longer.
- If the screen after the gesture says SAVE FAILED, the game was reading
  at that moment. Film both pages it shows, and run that part again.
- If the gesture does nothing inside the level, leave the level to the
  game's own menu and do it there.

Send the two newest files in /reports/toy/ whose names start with 66,
and say which is which. The text inside says TEST 77 and BUILD
ded3083ecdf8. The last value of the PIPE row is 1BD.

If anything goes wrong in Run 2, the report of Run 1 alone is still
worth sending.

To go back to the build you ran last, copy its file back from its
package (Test 76 without change 5).

CHANGED FILE
KUI/apps/games/retail-toy-test-66.kui
  Test 77, build ded3083ecdf8, payload CRC32 AB8FE168. Replaces the
  Test 76 module at the same path. Card Tools loads it from there; its
  menu entry is still called "66 SCI movie playback".

INCLUDED UNCHANGED
KUI/tools.kui
  Card Tools 09c9a4b686d4, payload CRC32 8111E794, identical to the Test 66
  package. Needed only if the card holds a different Card Tools.

All other installed software, games, saves, reports and settings are
retained. No updater package, game data or save data is included.

This build passed its host and linked checks. It has not run on a console.

SHA256SUMS.txt verifies the two modules and this README.
