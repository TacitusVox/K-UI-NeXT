# Test 89: the reviews

Four rounds of independent review, each by reviewers who did not write the
change and saw only the commits, the sources and the host tests; each
finding was then given to a second reader told to refute it, and only what
that reader could not refute was acted on. Reviewers and readers were
subagents of the session that wrote the change (the Workflow tool), on this
machine; none of it is a console test, and the runs below are the
reviewers', not the repository's tests.

| Round | What was reviewed | Reviewers | What was confirmed | What was done |
|---|---|---|---|---|
| 1 | The step's changes to `c3371aef` (drive clock, report rows, timers, pan) | 4, each finding to a sceptic | With the drive on the music's clock a ring stays its priming behind the play for good: a play that is not repeated ended for the game a tenth of a second before its music. A planted defect in L2 not caught; the drive's host test built without the output's switches; the report tool's wording; gaps in the report's tests | The hold (`308c95e6`); tests and tool (`1a78e36c`) |
| 2 | The hold, `308c95e6` | 2 (the player; the output), 4 sceptics | A hold taken in the tick in which a group of three sectors became whole kept the group's ticks, and the held play moved three sectors at every visit (both reviewers, each by running the player with a clock that moves between reads). A hold had no end but the key-on or the ring's giving up: a play of 3 s, played once, still playing a minute on. A PAUSE and RELEASE between two heartbeats while a ring primes end the hold unseen. The model could not end a play inside a hold | `34a2f4ab`; the second's bound, `9d64a048`; the pause case is left and listed under Known limits (the ring is then behind its play, as every ring under Test 88) |
| 3 | `34a2f4ab` and `9d64a048` | 2, 4 sceptics | A hold refused (its second spent) left the output reckoning the drive from the heartbeat's own, older count against a base the player's visit had moved beyond it: the ring taken up some 413,000 sectors on, or the play given up as over (both reviewers, on the unmodified player and output together). The model's hold did not move the base and could not show it; the scenario of a play over behind a card that never hands on did not look at the output | `d4d58718`: `drive_now()` reads TMU0 itself; the model's hold brings the base up to now; both scenarios made to bite |
| 4 | The whole hold at `d4d58718` | 2: a reader of player and output as one state machine; a run of the two linked together | Nothing | - |

## Round 4's run (the reviewer's, not a test of the repository)

One host program linking the unmodified `src/loader/toy_cdda_player.c`,
`src/core/retail_gd.c` and `src/loader/toy_cdda_output.c`, with the model
of the chip, driver, card and bus generated from
`tests/test_toy_cdda_output.c` (its own model of the hold and the drive
deleted), a clock that counts on every read of TCNT0 and every access of
the bus, and the game driving the real player through its real entry
(PLAY, PLAY2, PAUSE, RELEASE, STOP, REQ_STAT). `joint-harness/` holds the
reviewer's program and its generator as they were run; they are a record,
not a maintained test.

16,500 randomised sequences at ordinary clock costs, 763,483 rings taken
up, no invariant violated; 250 more under ASan and UBSan; 3,000 with the
player's read of the clock made 1,200 and 3,000 ticks long for the paths'
sake. Checked from the real state at every step: the position REQ_STAT
would answer never goes back, never runs faster than the clock, a held
play does not move, no play is held past 781,250 ticks from the first
hold (the most seen was 781,250), no hold stands outside PRIME; at every
take-up the output's sector and passes against the player's answer (exact
under a hold, within a sector without); 246,216 rings keyed on under
their hold began within a sector of their play, and 34,148 of them played
to the play's end with no frame short.

Paths reached: hold granted in a new second 501,349 times, again in the
same second 217,504, refused 44,630, let go by key-on 247,148, by stop
220,314, dropped at a visit at the second's end 24,579, a group whole
between the hold's two reads 128, PAUSE under a hold 97,747, PLAY under a
hold 89,996. The same program on `9d64a048`'s output flags 19 of 1,000
sequences: the defect `d4d58718` corrects.

The other reviewer of round 4, by reading and by a second linking of the
real player into the output's model (about 8,700 seeds): every write of
the play's base leaves it at or before the count just read; no stale hold
on a playing drive; every exit from PRIME lets go, keys on, or leaves the
hold to the player's second; no other reader of the base in the build.

## Seen in round 4 and not the hold's

Not changed in Test 89; listed under Known limits.

- `advance()`'s lap arithmetic (Test 84's): a look that starts with the
  base one or two sectors before the pass's start and finds a pass or more
  gone by counts a lap too many and can answer one sector before the
  play's first. Needs a play of a few sectors (a three-sector loop does it
  at every lap; PLAY2 of 7 sectors repeated, looked at 7 groups later).
- A first heartbeat of a first play on a bus that does not empty, before
  any driver is admitted, leaves the output silent with no reason for
  that play (Test 87's).
- A PAUSE and RELEASE with no heartbeat between while a ring runs leave
  the ring ahead of its play by the pause's length.
