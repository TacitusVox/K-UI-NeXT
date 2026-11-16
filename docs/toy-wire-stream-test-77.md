# Toy Commander Test 77: the window opened by consecutive reads

Status: delivered 15 November 2026 (build `ded3083ecdf8`) and run twice
on the owner's console the same evening (Console result, below): the
movies as the baseline or better, the game played on into a level, no
error in either report. A test module for one title, not an accepted
component.

This is step 2 of `docs/fmv-stream-reader-plan.md`. After the comparison
run of Test 76 the owner wrote: "Otherwise it's probably safe to adapt
this loader to all other games", and then: "Let's finish this up, yeah?"
The step is one behavior change against a named baseline, with its own
console test, as `AGENTS.md` asks.

Card Tools has no Test 77 entry. The module is installed where it looks
for Test 66 and starts from "66 SCI movie playback".

## Baseline

Test 76 without GAP: source `7fe7e83c2082`, module `7ce3c839...1c67`,
payload CRC32 `C50A20B9`, as run on the owner's console
(`docs/toy-wire-stream-test-76.md`, The variant without GAP on the
console; report 66-11). In short:

- The second movie showed 1,953 of 1,968 frames, 15 skipped in pieces 6
  and 7 of its course, the longest gap 0.288 s. The first movie showed
  all 331.
- A card block cost the reader 65.4 microseconds in pumps. 656 streams
  were begun.
- Two windows were opened for each movie. Two were given up because
  their movie was over and two for a request they did not serve.

The decision this test answers: does a reader that is told neither
which files are movies nor that a movie is playing keep the two movies
as that run has them; and what happens to the game's ordinary loading
when the same rule serves it.

## The change

Behind `TOY_WIRE_SEQ=1` on Test 76's recipe. GAP defaults to off with
it, as in the baseline.

| | Up to Test 76 | Test 77 |
|---|---|---|
| A request may use the window | It lies inside one of Toy Commander's four movie files, 386 sectors in, and the hook in the game's player says that movie is playing | It is a read of 2,048-byte sectors on a raw Mode 1 track that the low reader has begun and delivered nothing of |
| A window is opened | For the first such request | For such a request that begins where the read before it ended, where the resident's SCI route is selected and the low reader is not in the middle of a wait of its own |
| The window reaches | To the file's end | To the track's end |
| The window is given up | When the movie is over; for a request it does not serve, or one in another file, or one that begins elsewhere; for a command that is not a read; when the request state is no finished read; on an error | For a request it does not serve, or one that begins elsewhere, or one that reaches beyond the window's end; for a command that is not a read; when the request state is no finished read; on an error |
| Still decided by the files and the player | | The low reader's policy for a request (`toy_suite_active`), its raw staging path, the report's rows by movie, and the player's update call as a pump while a movie plays |

"Where the read before it ended" is the GD service's own drive
position (`position_lba`, the last sector a read delivered) plus one.
The reader keeps no memory of its own for it. A request must still be
of the verified title on a source the stage found raw, as before
(`toy_poll_owner`, `suite_source`): this is the Toy test module.

Five things follow from windows outside a playing movie. The movie's
rule had kept each from happening or from mattering, and each is part
of the change, behind the same switch:

- **A window with no more to give.** A window that has served its
  track's last sector stands at the first sector of the next track. A
  read there begins at the window's first sector and would wait for
  ever. A read that reaches beyond the window's end is therefore given
  up for like any other read the window does not serve.
- **A request served in an interrupt visit.** Off its movie path (a read
  outside the files, or in a file's first 386 sectors) the resident
  reports a request complete only from the game's next call, not from
  the interrupt. Until then the request state is that of a read under
  way with nothing left to do. Up to Test 76 that state could only mean
  a request gone wrong, and gave the window up; now it does so only
  once the request is no longer pending.
- **A stop command that fails once.** Giving a window up for a read ends
  the card's command with one CMD12 inside the game's call. Up to Test
  76 a failed CMD12 there failed the game's read, and off the movie path
  a failed read stops the console with its failure report. That branch
  ran about four times a console run; it is now how every run of
  consecutive reads ends. The window is closed all the same and the
  request left to the low reader, which ends the card's command before
  its own read and tries again, as it does for any read. (Review,
  finding 2 of the second reader.)
- **A wait of the low reader's left standing.** A read on the low
  reader's movie path looks for its block's token a little a visit. If
  the game ends that read before the token has come, the search stays
  where it stood. A window opened for the next read at that block would
  have its seek carry the search on as the low reader's own reception,
  which nothing then ends: the request waits for ever. No window is
  opened while the low reader stands in such a wait. The fault is older
  than this change: it reproduces in Test 76's build while a movie
  plays, and is left as it is in builds without the switch. (Review,
  finding 1 of the second reader.)
- **Windows counted when they are gone, and by their cause.** The
  reasons windows are given up are printed again (row `X`). An interrupt
  visit cannot end the card's command and leaves the window to the
  game's next call, which finds the same reason; up to Test 76 both
  visits were counted. And "a request the game waited for" is counted
  only where the window had the sectors to give: off the movie path
  every later call of a request is one the game waits in.

What is not changed: the card stream, the sector checks, the low
residents, the staging path for movie requests the window does not
take, `toy_suite_active` (ordinary reads keep the low reader's stock
policy, movie reads the movie policy), and the rule that a request the
game waits for keeps its window while the window grows (STAY).

### What the game's reads become

- **A movie's start.** The game reads the file's first two sectors,
  then 384 in one request, then refills of about twelve sectors. The
  large request is the low reader's (a window takes 60 sectors at
  most). The first refill begins where it ended and opens the window,
  as it did by the movie's rule. The host model has the stream take the
  card over there with no card command, the low reader having stopped
  behind the block before.
- **A movie's end.** The window stays, filled, until something else is
  read; that read gives it up.
- **A movie played again.** The player's hook sees a movie's first
  start only. A second playing had no window up to Test 76 and has one
  now, pumped by interrupts and the game's disc calls alone.
- **Ordinary loading.** Two reads one behind the other open a window,
  and what follows on is served from it. A read elsewhere gives it up:
  one CMD12 in that call, and the low reader's CMD18. The low reader
  alone would have sent the same two commands for that read.

## Module roles

| Part | Changed | How it is checked |
|---|---|---|
| Auxiliary (`native-ra-aux`) | Yes: selection and window. Text 20,424 of 20,472 bytes (baseline 20,388), image 24,544 of 24,576 (24,512) | Host scenarios, mutations, linked checks |
| Worker (`native-worker`) | Yes: the report's rows | `tests/test_toy_wire_report.c` |
| Low residents | No | Byte-identical with Test 66's build label to Test 66's |
| Stage, entry | No source change. The stage carries the worker and the auxiliary | Its own objects compared with the baseline's |
| Installed readers and GUI | Not touched | The module is the test module at the Test 66 path |

Against the baseline built with this module's label the residents are
the same bytes, and the auxiliary and the worker differ. The stage's own
objects are the same too; its bytes differ where it carries those two
images and in the addresses of their ends (`build-identity.log`).

## The report

Test 76's rows, with these differences:

| Row | Change |
|---|---|
| `PIPE B I CALLS IRQ STAY PT` | `PT` is `1BD`: Test 76 without GAP (`BD`) and SEQ (`100`). `B` and `I` count windows opened by a read inside that movie's file |
| `AHEAD B I HIT EDC STOP OS` | `OS`: sectors served from windows opened by any other read. `B` and `I` are the sectors served from the movies' windows; `HIT`, `EDC` and `STOP` are totals over all windows |
| `X OW E I C W O` | New. `OW`: windows opened by a read outside the movie files. Then windows given up, each counted once, by the first reason that held: error; the request state no finished read; a command that is not a read; a read the game waited for where the window stood, for a quarter second without a new sector or with no clock; any other read the window does not serve |
| `D N T SEEK` | Without `CAP`, the capacity of the player's buffer: `C9120` in three runs |
| `TP` | Left out: the wire's standstill by piece has served |
| `BP ON OT OC OI SN ST` | Written last. With thousands of windows opened by ordinary reads the other rows widen, and this is the row to go for room |

`PIPE`'s `STAY` counts requests the game waited for that kept their
window. Up to Test 76 those were the player's own waiting loop; now
nearly every ordinary read that had to wait for the stream is one, the
resident knowing no polling caller off its movie path.

A window ended by the card being lent (the report's own save) or by
INIT, RESET or an abort is in no column of `X`. The rows by piece (`TF`,
`TD`, `TW`, `TB`, `TR`) and the player's rows cover the movies only, as
before. `W`, `S`, `K`, `V`, `P` and `D` are totals from the game's
start to the save, ordinary reads included.

## Console test

Two runs, in the package's README:

1. **The movies.** As every run since Test 66: both movies without
   skipping, then the report. Compared with report 66-11.
2. **Into a level.** Both movies, then through the menus into the first
   level, a minute of play, then the report.

Reading them:

| Outcome | What it says |
|---|---|
| Run 1: frames, skips and the rows by piece as in 66-11; `PIPE` `B` and `I` one to three each | The trigger keeps the movies. Step 3 (the player's hook no longer pumps) has its baseline |
| Run 1: the second movie's first frame later than 0.80 s, or frames missing in piece 0 | The movie's start differs. `PIPE`, `X` and `TW` say whether the window opened later or was given up there |
| Run 1: frames missing elsewhere, `X` `W` or `E` not zero | Windows were lost during the movie: by a wait that outlasted a quarter second without a sector, or by an error |
| Run 2: plays normally; `OW` and `OS` not zero; `X` mostly `O`, some `C` | Ordinary loading goes through windows and ends them with reads elsewhere, as designed |
| Run 2: `X` `W` large | Loads the game waits for outrun the stream: windows are given up and the low reader reads, which is the old path and costs the opening each time |
| Run 2: `X` `E` not zero, or `K`'s `LB` not zero | Sectors failed their check ahead of ordinary reads (Known limits, 3) |
| Run 2: an error screen, a hang, wrong picture or sound | The change does not hold for ordinary loading. Next step: the files' rule back for opening outside the movies, by a switch, and the report's rows to say why |

The owner's observation of loading times in Run 2 is part of the
result: nothing in the report times a load.

## Console result

The owner's two runs of 15 November 2026, by the package's README, with
CD Music and virtual VMU off. Two saved reports of 1,024 bytes, both
headed `TEST 77`, `BUILD ded3083ecdf8`, `PT` `1BD`:

| Run | File | SHA-256 |
|---|---|---|
| 1, the movies | `docs/evidence/toy-wire-stream-77/console-report-66-run-1.txt` | `cbae8cef...6c3d` |
| 2, into a level | `docs/evidence/toy-wire-stream-77/console-report-66-run-2.txt` | `5dfd7e60...638a` |

Decoded by `decode_report.py` in the same folder
(`console-report-decoded.txt`), which reads the baseline's report too.

Owner's observation: "First run lost 9 frames. I don't think the second
run even missed any frames so it might be random chance." The message
names no fault in the menus or in the level and says nothing of loading
times.

| | 66-11, the baseline | Run 1 | Run 2 |
|---|---|---|---|
| First movie: frames submitted / on time | 331 / 330 | 331 / 330 | 331 / 330 |
| Second movie: frames submitted of 1,968 | 1,953 | 1,959 | 1,965 |
| Of those on time and in sequence | 1,947 | 1,953 | 1,960 |
| Frames skipped, first at the player's frame | 15, 1,089 | 9, 1,080 | 3, 1,180 |
| Longest gap between frames | 0.288 s after 1,130 | 0.100 s after 1,133 | 0.065 s after 1,177 |
| Second movie's first frame | 0.801 s | 0.806 s | 0.809 s |
| Calls that put a frame out, each | 1,952, 30.2 ms | 1,958, 30.2 ms | 1,964, 30.2 ms |
| Windows opened, first movie / second | 2 / 2 | 3 / 3 | 3 / 3 |
| Sectors served from them | 3,421 / 25,824 | 3,421 / 25,824 | 3,421 / 25,824 |
| Windows opened by other reads, sectors served from them | none can be | 21, 106 | 28, 133 |
| Requests served from a window | 2,416 | 2,450 | 2,457 |
| Requests the game waited for that kept their window | 53 | 81 | 90 |
| Windows given up: for a read elsewhere / for a command | | 23 / 4 | 29 / 5 |
| Given up on an error / on the request's state / on a wait | | 0 / 0 / 0 | 0 / 0 / 0 |
| Sectors verified into windows / served | 29,288 / 29,245 | 29,914 / 29,351 | 30,149 / 29,378 |
| Delivery visits from the game's disc calls | 6,823 | 10,772 | 46,531 |
| All pumps | 8.88 s | 9.17 s | 9.20 s |
| Card blocks, pump time for each | 135,696, 65.4 us | 138,695, 66.1 us | 139,684, 65.9 us |
| Streams begun | 656 | 700 | 685 |
| Ended on a byte between blocks neither idle nor token | 628 | 655 | 627 |
| The same for every hundred overruns in a run | 3.17 | 3.22 | 3.10 |
| Blocks handed on with lost bytes / sectors completed from parity | 2,843 / 1,867 | 2,786 / 1,775 | 2,704 / 1,716 |
| Rebuilt bytes not confirmed / sectors failing the EDC watch | 0 / 0 | 0 / 0 | 0 / 0 |
| Stream errors | 0 | 0 | 0 |

The second movie by piece of its course (5.4 s each):

| Piece | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Frames submitted, 66-11 | 136 | 164 | 161 | 162 | 161 | 162 | 153 | 154 | 162 | 162 | 161 | 215 |
| Run 1 | 136 | 163 | 162 | 161 | 162 | 161 | 157 | 157 | 161 | 163 | 161 | 215 |
| Run 2 | 136 | 163 | 162 | 161 | 162 | 161 | 162 | 158 | 161 | 163 | 161 | 215 |
| Requests waited, s, Run 1 | 0.06 | 0 | 0 | 0 | 0 | 0 | 0.04 | 0 | 0.02 | 0.48 | 0 | 0.10 |
| Run 2 | 0.06 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.15 | 0.57 | 0 | 0.04 |
| Player's buffer at its emptiest, KiB of 804, Run 1 | 256 | 32 | 144 | 68 | 40 | 36 | 32 | 28 | 100 | 68 | 736 | 496 |
| Run 2 | 256 | 32 | 144 | 124 | 208 | 208 | 32 | 28 | 152 | 56 | 740 | 508 |
| Reader, s, Run 1 | 0.52 | 0.65 | 0.67 | 0.65 | 0.63 | 0.65 | 0.65 | 0.71 | 0.82 | 0.71 | 0.29 | 0.59 |
| Run 2 | 0.52 | 0.65 | 0.65 | 0.65 | 0.61 | 0.65 | 0.59 | 0.71 | 0.84 | 0.75 | 0.29 | 0.59 |

What the runs say, by the outcomes above:

1. **The trigger keeps the movies.** Both movies get the same sectors
   from windows as in the baseline, 3,421 and 25,824, with nothing in
   the reader that knows a movie's file or asks the player whether one
   is playing. The second movie begins at the same time (0.81 s against
   0.80), a frame costs the player's call the same 30.2 ms, and a card
   block costs the reader 66 microseconds against 65.4. No window was
   given up on an error, on a wait or on the request's state in either
   run. That is the first row of the outcomes: step 3 has its baseline.
2. **The frames lost are fewer, and that is not claimed as a gain.** 9
   and 3 against the baseline's 15 and Test 76's 26. All of them lie
   where they lay before, in pieces 6 and 7 of the course (32 to 43 s,
   the player's frames 1,080 to 1,180), and the reader does nothing
   differently while a movie plays. Four runs of what is one reader
   there have now lost 26, 15, 9 and 3 frames in that scene: its size
   is the run's, as the owner says. Run 1 has requests waiting in piece
   6 for 0.04 s in all; Run 2 lost its three with no request waiting
   between the 5th and the 43rd second, as both earlier runs did. The
   longest gap is three frame periods in Run 1 and two in Run 2.
3. **Each movie has three windows where it had two.** The sectors
   served are the same, so one window more was opened by a read inside
   each file and given up again. Where in the movie is not in the
   report. The second movie's first frame is not later for it.
4. **Ordinary loading went through windows and came to no harm.** 21
   windows were opened by reads outside the movies in Run 1 and 28 in
   Run 2, every one given up for a read elsewhere or for a command that
   is not a read, none for an error. No sector failed its check (`LB`
   0), no rebuilt byte went unconfirmed (`MB` 0), the stream reported
   no error, and the level was reached and played. That is the fourth
   row of the outcomes, with the limit of point 5.
5. **Toy Commander's loading hardly reads that way.** Run 2 has 35,759
   more delivery visits from the game's disc calls than Run 1 and seven
   more windows, which served 27 more sectors in seven more requests. A
   window outside the movies serves five sectors on average, in fewer
   than two requests, before the game reads elsewhere. So this game
   says that such windows do no harm; it cannot say what they gain or
   cost a game that loads in many small consecutive reads. That
   question goes to the games of step 4.
6. **What the windows cost here.** Every window given up is a stream
   ended and a CMD12 (Known limits 2): 27 and 34 of the 700 and 685
   streams begun ended that way, against 4 of the baseline's 656. More
   sectors were read ahead than served: 563 and 771 against the
   baseline's 43, about 1.3 and 1.8 MB of card reading over the whole
   run that nobody asked for, checked in interrupt time like the rest.
   Part of it is the movies' own windows, which now read on behind
   their file's end. No frame count and no row by piece shows a price
   for it.
7. **A loan with a window open has not run on the console.** In both
   runs the windows given up equal the windows opened (27 and 34), so
   none was open when the card was lent for the report's save. That
   path has its host scenario (`sequence_lent()`) and no console run.
   The same holds for a stream standing under a filled window for a
   long time (Known limits 1): the report cannot say how long one stood.

Not answered by these runs: loading times in the menus and the level
(nothing in the report times a load, and the owner's message does not
speak of them), and anything about another title.

## Known limits, not changed here

1. **A window's stream stands until the next read.** After the last
   consecutive read the window fills and its stream stands still until
   a read elsewhere, a command that is not a read, INIT, RESET, an
   abort, an error, or the card being lent. Until then the card stands
   inside its read command, the resident keeps the SCI level, DMA
   channel 1 holds the stream's addresses with its enable bit off, and
   the stream's timer channel keeps its level and the reader's callback
   in the SDK's table. When the stream goes on or ends it writes the
   channel and the timer without looking at them first. Up to Test 76
   that lasted as long as a movie; now it lasts as long as the game does
   not read elsewhere. It matters only to a game that takes channel 1
   or that timer for itself in play, which is not known of Toy Commander
   (the stream found TMU1 free and TMU2 in use during the movies). A
   rule that ends a standing stream after a time and keeps its sectors
   is the remedy, and belongs before this reader meets another game
   (Review, finding 3 of the second reader).
2. Every window given up costs one CMD12 inside the game's call, and
   the next read a CMD18. A command that is not a read between two
   consecutive reads, or a read of more than 60 sectors among smaller
   ones, pays both each time, where the low reader alone keeps the
   card's command open. In the host model: 17 consecutive reads with a
   NOP between each, 16 CMD18 and 16 CMD12 against 1 and 0 (the second
   reader's measurement). `X`'s `C` and `O` will show whether Toy
   Commander's loading does this.
3. A sector that fails its check ahead of ordinary reads (an image with
   a stale EDC) ends its window after eight tries, and each consecutive
   read tries again. One such sector found by the one-in-sixteen EDC
   sample switches the lighter check off for the rest of the session,
   which would cost the movies time. The owner's movies have shown none
   (`LB` 0 in every run).
4. Windows opened outside a movie's first playing have no pump from the
   player's hook: interrupts and the game's disc calls only. That is
   step 3's question, met here first for ordinary reads.
5. "Consecutive" is taken from the service's drive position. INIT,
   RESET, an abort and commands that are not reads leave it where it
   was; a seek moves it, and a read behind the sector a seek named
   counts as consecutive. That only decides whether read-ahead is
   spent; what a window serves is checked sector by sector as before.
6. Older than this change and not touched: the hang of the fourth point
   above in builds without the switch; and after the card has been lent
   once the player's update call no longer pumps (`quiesced` is set and
   never cleared). In this module the only loan is the report's save,
   which ends the run.
7. `test-sci-stream-quarantine` stops on its fingerprint of
   `src/loader/sci_stream.c` as it has since before Test 73; Tests 68 to
   77 do not touch that file or that test.

## Verification

The module is built from commit `ded3083ecdf8` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree.
Logs are in `docs/evidence/toy-wire-stream-77/`. None of this is a
console result.

| Check | What ran | Result |
|---|---|---|
| Whole reader | `tests/test_toy_wire_integration.c`: the real resident, low reader, GD service, auxiliary and stream on the card model, the scenario list twice (a receiver that ignores the bit order request and one that honours it) | Passed: 275,694,403 checks; every request completed with the image's exact bytes |
| Ordinary reads one behind the other | `sequence_plain()` | The first is the low reader's. The second opens a window and the stream takes the card over with no card command. Six of nine requests served from three windows, 106 sectors; a window given up for a read elsewhere, for one of 61 sectors, and at the track's end, each by the request's own call. Nothing counted as the movies' |
| The track's end | `sequence_track_end()`, the game's EXEC first and its CHECK first | A read that reaches into the next track is never the window's, at an open window's first sector or with a window opened for itself; counted as a read the window does not serve, not as one waited for |
| Extents apart, with stalls | `sequence_scattered()`, three bus conditions | 27 requests, 26 from the window in each; the stream stopped at every jump (47 on a quiet bus) and begun again |
| The movie's file | `sequence_file()` | A window opened ahead of the file serves into it; one opened inside it is that movie's in the report and serves on behind its end; one opened by a read over the file's end is counted with the others |
| Commands | `sequence_commands()` | A command that is not a read gives the window up; INIT, RESET and an abort end it; none of them moves the drive, and the next consecutive read has a window. A seek moves it. Whole 2,352-byte sectors at a window's first sector are the low reader's |
| Reads the game waits for | `sequence_waited()`: its own loop, interrupts held off | Eight reads: the first the low reader's, then one window and one stream for seven, no interrupt getting through. Without STAY: seven windows opened and given up, the low reader reads them all |
| A token search left standing | `sequence_pending()`, three ways of polling | No window on top of it; the low reader completes the read (63 to 70 rounds); the read behind it has its window |
| Served in an interrupt visit | `sequence_interrupt()` | Three requests served in an interrupt visit and reported by the game's next call; through the visits in between the window is kept, no stream stopped, no CMD12 |
| The card lent with a window open | `sequence_lent()`: the real loan the report's save makes, stream standing and stream receiving | The window given up, the stream stopped, one CMD12, channel, timers and the SDK's table as before; given back, the game reads on and has a window again |
| A stop command that fails once | `sequence_stop_failed()` | The window closed, the command sent again by the low reader, the read exact |
| No window | `sequence_nowhere()` | Without the SCI route, in another title, on a source the stage did not find raw, and on a track of 2,048-byte sectors: ten consecutive reads each, all the low reader's, one CMD18 |
| A movie's start, and its second playing | `sequence_movie()` | A read of 61 sectors by the low reader, then the first refill a window with no card command; after the movie's end the window stays; played again, a second window; twelve refills served |
| What rested on the movie's end | `ended_in_interrupt()`, `window_gone()`, `other_read()`, `quit_foreign()` | The movie's end leaves the window, in a call and in an interrupt visit. A window in error under a waiting request is stopped in the interrupt visit and counted when the next call has ended the card's command. After a read elsewhere the movie's next read is the low reader's and the one after has a window |
| The report | `tests/test_toy_wire_report.c` on the production formatter | Passed. With nothing counted the body is 685 bytes. At a full run's values with the movies' windows alone every row fits (999 of 1,000 bytes; Test 76's real report was 980). With 12,032 windows opened by ordinary reads beside them every row through `TR` fits (967 bytes) and the first movie's player, the last row, is left out |
| All configurations | Both host targets in Tests 68 to 76 with their variants and in Test 77, Test 77 with GAP, with LATER, without STAY, QUICK, MSB, RING, CLEAN, and with no part of Test 76 (`matrix.log`) | 76 of 76 jobs passed: both targets in 38 configurations |
| Tests notice defects | `tests/test_toy_wire_stream_mutations.py` with `TOY_WIRE_SEQ=1` (`mutation-check.log`) | 218 of 218 caught, 37 of them of the lines this test adds. The tests pass without a mutation, which the script now checks first |
| The list is of compiled code | Each entry's change against the preprocessed sources of its configuration | 218 of 218 for Test 77 and 190 of 190 for Test 76 change the code that is compiled |
| As linked | `tests/test_toy_wire_cached_stack.py`, `test_toy_wire_linked_loops.py`, `test_toy_wire_linked_ring.py`, `test_toy_wire_linked_hot.py`, `test_toy_wire_linked_copy.py` on the module's auxiliary | Passed. The window's state is used at its cached address only: 22 constants in the linked code |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| Low residents | `cmp` against the Test 66 build | Byte-identical with Test 66's build label |
| Against the baseline | Test 76 without GAP built with this module's label | Residents the same bytes. The worker and the auxiliary differ. The stage's own objects are the same; it differs where it carries those two images |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `1c1e54f3...7323` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66, 67, 68, 69, 70, 71, 72, 73, 74 and its second module, 75 and its second module, 76, its second module and its variant without GAP: all as recorded |
| Other suites | The Test 66 family's own fixture in every suite mode, and the suites that share the wire model (`other-suites.log`) | The Test 66 family's fixture passes in every suite mode. 17 of the 18 other suites pass; `test-sci-stream-quarantine` stops as before (Known limits 7) |

Not run for this test: the stream test over 96 courses (the card
stream's source is unchanged since Test 76), and Test 76's own mutation
list (its entries are in Test 77's list, but for four on lines this
configuration does not compile, whose successors are).

What the verification itself found and changed, before the commit:

- A window that had served its track's last sector kept a read at the
  next track's first sector waiting for ever: found while the scenario
  for the track's end was written. The give-up test has the window's
  end since.
- The scenario for a read elsewhere showed windows given up counted
  twice where an interrupt visit found the reason first. They are
  counted when they are gone.
- After the second reader's findings the reason "waited" was narrowed,
  and one mutation, a read beyond the window's end left waiting, was
  then no longer caught: a game that waits gives the window up after a
  quarter second, which hid it. The scenario now requires the window to
  be given up by the request's own call.

## Review

Three independent readers were given the uncommitted change, each with
one angle: everything outside the new behaviour; the window's logic with
the switch on; the tests of the new behaviour. Two returned, with
fourteen findings between them. The third was cut short by the
session's usage limit. Their findings as returned, and what was done
about each, are in `docs/evidence/toy-wire-stream-77/review.md`.

**The first reader** compared builds without the switch token for
token: the auxiliary, the stream and the entry preprocess to what they
were in thirty wire configurations and 44 suite modes, and the worker
differs in two constant expressions that fold to the same value, with
byte-identical objects. Its seven findings were none of them in a build
without the switch nor in the default Test 77 module:

| Finding | Done |
|---|---|
| The whole-reader test failed for Test 77 built without STAY | The scenarios of ordinary reads that wait run where the build has STAY. The mutation script now runs the tests without a mutation first |
| The mutation script built its stream test without GAP when a Test 77 build asked for GAP | Fixed |
| The report test's full run had counts that did not go together; made to, the last row no longer fitted | The first movie's player row is written last and is the one to go; the test checks a full run with 12,032 windows and one with the movies' alone |
| `TOY_WIRE_SEQ=1` without `TOY_WIRE_STREAM=1` was silently ignored | Refused |
| The worker's screen pages called a Test 77 module TOY TEST 75 | TOY TEST 77. Test 76's worker is left as delivered |
| `HIT` and `OUT` count every window, `B` and `I` the movies' only; Test 76's decoder divides one by the other | Said in The report; a decoder for these rows comes with the console result |
| Three comments said more than the code does | Reworded |

**The second reader** walked `toy_movie_ahead` for every source of a
visit and ran 4,000 random walks of game behaviour over the fixture in
copies of its own: no wrong byte, no failed read, no index out of
bounds, no wrap in the give-up test, and no hang from the rules the
switch adds. It found:

| Finding | Done |
|---|---|
| A token search the low reader left standing is carried on by the next window's seek as a reception nothing ends: the request waits for ever. Older than this change (it reproduces in Test 76's build while a movie plays); the switch widens which read can be the second | No window is opened while the low reader stands in a wait of its own. Its reproduction is `sequence_pending()`. Builds without the switch are unchanged |
| One failed CMD12 as a window is given up fails the game's read, and off the movie path that stops the console. An old branch that ran about twice a run and now ends every run of consecutive reads | The window is closed and the request left to the low reader, which sends the command again. `sequence_stop_failed()` |
| A stream standing under a filled window keeps DMA channel 1 and a timer channel until the game reads elsewhere, and writes both without looking when it goes on or ends | Not changed: Known limits 1. Owed before this reader meets another game |
| A command that is not a read, or a read of more than 60 sectors, between consecutive reads costs a CMD12 and a CMD18 each time | Not changed: Known limits 2 |
| `X`'s `W` and `PIPE`'s `STAY` take in ordinary polled reads; a read over the track's end was counted as waited or as other by the order of the game's calls | `W` is counted only where the window had the sectors to give. The report section says what `STAY` counts now |
| The fixture reached none of: a pending token search under a window, a failed CMD12 at a give-up, the controller's wake, a foreign use of channel 1 or the timer under a standing stream | The first two have scenarios. The wake is used by one of them. The last has none: Known limits 1 |
| After the card has been lent once the player's update call no longer pumps. Older, and without effect here | Not changed: Known limits 6 |

**The third reader** left the logs of six experiments on a copy of the
tree. Read by the author, they gave three things: a variant of one
defect (a request served in an interrupt visit taken for a failed one,
by interrupt visits only) that the fixture did not catch, which it now
does; that the drive position placed in `open_movie()` changes nothing
in this fixture, which its comment now says; and what a track of
2,048-byte sectors does to a build without the track's format tests
(nine windows opened and abandoned, the reads exact), which is a
scenario now. Its remaining questions, among them which real callers
the fixture's ordinary game does not model, had no reader. Each of the
37 mutations of the new lines is caught by a test, which answers part
of what it was asked and not all.

## Package

`KUI-Toy-Wire-Stream-Test-77-15NOV2026-2058-CST-ded3083.zip`, 1,800,377 bytes, SHA-256
`c35b4cf99c9c19d85155b6f87ee857a03282c76f6584a33717b0f226d352a49f`, prepared 2026-11-15 20:58 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-toy-test-66.kui` | `ded3083ecdf8` | `AB8FE168` | `1c1e54f3...7323` | Changed: Test 77, replaces the Test 76 module at the path Card Tools loads |
| `KUI/tools.kui` | `09c9a4b686d4` | `8111E794` | `04b8657a...c800` | Unchanged copy from the Test 66 package |

Module SHA-256 in full: `1c1e54f3fc5ebad7232dd3bb2d158e6e6521eebfd1a25d1f7ed7b9723dc37323`.

Retained on the card: everything else, including the Test 67 trace
module, Test 57, reports, saves and settings. No KUP, bank or user data
is included.

The report names the module by its title (`TEST 77`), its build
(`ded3083ecdf8`) and `PIPE`'s last value (`1BD`).

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 BUILD_ID=ded3083ecdf8 build/toy-suite-77/retail-toy-test-77.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 toy-wire-host-tests toy-wire-integration-host-test toy-wire-package-test \
  toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 python3 tests/test_toy_wire_stream_mutations.py
```

Run on the console: cold CD boot, Card Tools, the raw Toy Commander GDI,
Options, Advanced, Tests, "66 SCI movie playback" (the entry now starts
Test 77), CD Music and virtual VMU Off; the two runs of Console test.
Confirm `TEST 77` and `BUILD ded3083ecdf8` in the saved
`/reports/toy/66-N.txt`, and `PIPE`'s last value.
