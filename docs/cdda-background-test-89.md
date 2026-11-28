# Test 89: CD music in the Background reader, part 6: the player's rough edges

Status: in preparation; not packaged, not delivered, not console-run. Build `d4d5871818df`. A test module in the place of the installed New reader,
like Tests 79 to 88, not an accepted component.

It is step 2 of `universal-loader-roadmap.md` (CD music in regular games:
what is left), taken up after Test 88's console round at the owner's word
of 28 November 2026: "Start on the next in the roadmap."

## Baseline

Test 88 (`cdda-background-test-88.md`, build `9e105c4e8d25`, SHA-256
`7f14af33...7cb8`), run on the console on 27 November 2026 in six games.
The recipe is Test 88's, switch for switch; it is not Test 88's bytes at
this commit. The recipes without the sound output give the bytes they
gave: the module without sound, built with Test 88's label, is the one
Test 88 delivered (Verification).

## Why

Test 87's and Test 88's reports left five things open
(`cdda-background-test-87.md`, Console result and Known limits):

1. The drive's own clock is TMU0. The position the game reads runs a
   quarter of a percent behind the music; a play that starts over takes
   the music up 2.5 ms earlier for each second played; a play that is not
   repeated ends for the game that much after its last frame.
2. The planes are on fixed sides. KOF Dream Match 1999 gives the CD's two
   inputs 0F08 and 0F17, each half way out; Tetsuman Menkyokaiden 0F17 and
   0F08.
3. KOF's audio stream ends at a block ten times a second, an eighth of
   the music's card time, and nothing in a report says why.
4. The least lead (56 ms in KOF) fits a play's start, and one least for a
   whole run cannot tell a start from the run.
5. AUDIO64, the next family, keeps no count of the chip's time. Whether
   the chip's two unused timers can be read from the processor's side of
   the bus is not known.

## The change

Eleven commits on Test 88 that change the module, and one of tests and a
host tool. No new switch: the changes are the sound output's wherever it
is built, and shared sources change under `KUI_TOY_CDDA_OUTPUT` with
Main's text unchanged. The module is built at the last, `d4d5871818df`.

| Commit | What it does |
|---|---|
| `6764ccea` | Room. 768 bytes of the worker's image were zeros, the stream report read in the place of a block the auxiliary never wrote. They are now made when needed, in the shared block's sink, which nothing reads once the report is written. The worker had 192 bytes left; it has 288 at `d4d58718` (960 at this commit, before the rest) |
| `dd61d50b` | **The drive keeps the sound chip's time.** Three sectors are 31,172 of TMU0's ticks where they were 31,250: the chip's time by the nominal ratio of the two clocks (the processor's 199.5 MHz reckoned as 200: 2,500 parts in a million; measured 2,483, give or take 5) |
| `7d6caf2b` | Report: the row `CD W BL AH CU` in XC's place, the audio stream's own counts of why it ended at a block; `L2` in FB's place, the least lead of the looks a second and more into their ring |
| `1f677d9d`, `c3371aef` | Read only: a heartbeat of a ring that runs reads the chip's timers B and C once each, behind the clock's own reads, and the report's `TB` says what each first read and which of its bits were seen to change. Nothing is written to either |
| `37e5a467` | **Each plane has its input's pan where the game pans the two inputs to opposite sides.** The level stays the larger of the two. Where the inputs are not on opposite sides the planes keep their own sides |
| `5d699dad` | The sounding build is `retail-toy-test-89.kui`, and its report says TEST 89 |
| `308c95e6` | **The play stands still while its ring is primed** (found by review, below). The player has a hold: a held play stands where it stood, to the tick, and goes on from there when let go. The output holds the play when it takes a ring up and lets it go when the ring is keyed on or given up |
| `1a78e36c` | Tests and the report tool only, from the review |
| `34a2f4ab` | A correction to the hold (second review): a hold taken in the tick in which three sectors became whole kept those ticks, and the held play then moved three sectors at every disc call. The held ticks are now what is left of the group |
| `9d64a048` | **A play stands a second at most for its ring** (second review). The player bounds the hold: a ring not keyed on within a second of the first hold has its play going on without it, as before there was a hold, and the play is not held again until a ring is keyed on |
| `d4d58718` | A correction to the output (third review): where the drive stands is reckoned from a count of TMU0 read there and then, not from the heartbeat's own. The player's visit for a hold brings the play's base up to a later count than the heartbeat's; with the hold then refused, the base could lie beyond the count and the ring was taken up far from the play, or the play given up as over |

### The drive's clock

The player reckons the play's position from TMU0: `base_counter`,
`base_offset` and a fixed number of ticks for three sectors
(`include/kui/toy_cdda_player.h`). TMU0 counts the processor's peripheral
clock divided by 64, 781,250 Hz if the processor ran at 200 MHz. Over the
1,595 s of ChuChu Rocket's session under Test 87 it ran 2,483 parts in a
million slow against the sound driver's count, give or take 5, and six
other measurements in four games agree: the processor's clock is 199.5
MHz, 2,500 in a million under its name, and the rest is the two crystals'
own difference.

The music has kept the chip's time since Test 87. The drive now does too:
31,250 x 0.9975 = 31,171.9, so three sectors are 31,172 ticks. On the
owner's console that leaves 17 parts in a million between the drive and
the music, a sector in 13 minutes, where there were 2,483, a sector in 5
seconds. The constant is the one place the measure lives: the player's
position, the output's reading of where the drive stands, and the play's
end all take it from there.

What is not changed: TMU0 as the witness of the driver's count, the
heartbeats' spacing and the bus's 3 ms, which are TMU0's time on purpose.

### The hold

A drive on the music's clock showed what the drift had hidden. A ring is
taken up at the sector the play stands at, primed with a tenth of a second
and more from the card, and only then keyed on: the music begins that far
behind the play. By TMU0's measure the play fell back 2.5 ms a second, so
that after 40 s the music had caught up and from then on led; a play that
is not repeated ended for the game after its music, by a silence that grew
with the track. With one rate for both, the music stayed behind for good:
such a play ended for the game with a tenth of a second of it unsounded,
and a game that plays the next track at once (ChuChu Rocket's session
under Test 87 had seven plays end) had it keyed off unheard.

A drive stands while it seeks. `kui_toy_cdda_player_hold` holds a play
that is playing: the player brings it up to now, keeps the ticks its base
lies behind now, and from then on every reckoning of its position
(`advance()`, the output's `drive_now()`) finds it where it stood. It
answers as a play that plays (status 3). Let go, it goes on from there,
the part of a sector that had passed not lost. A pause and its release,
and a new play, end a hold by themselves. (A hold is taken a tick or more
after the play was brought up to now. Where a group of three sectors
became whole in between, the base takes the group once and the hold keeps
the rest: `34a2f4ab`.)

The output holds the play in the heartbeat that takes a ring up, after
the ring is cleared, and takes the ring's first sector from the held
play; it lets the play go when the two channels are keyed on, and in
`stop()`, which every way of giving a ring up goes through. So the
position the game reads begins with the music and ends with it, and a
ring that begins again (it ran dry, a key was dropped) costs the play the
time it costs the music.

What a hold changes for a game: for the tenth of a second a ring takes to
prime, and for a second at most, the position does not move, at a play's
start and at each start over. A play of T seconds ends T seconds after
its music began, not T seconds after PLAY.

**A second at most.** As first written (`308c95e6`) a hold ended only at
the ring's key-on or its giving up, and a ring can stay unprimed for as
long as something keeps it so: the card is the game's while a read of the
game's stands, and a heartbeat is not made under SR.BL, with a G2
transfer enabled or on a short stack. The play the game reads would have
stood for all of it with status 3: a play of 3 s, played once, was still
playing a minute on in the model, where under Test 88 it ended by its
clock, unheard. The player now bounds the hold itself, because the game's
disc calls go through the player whether or not a heartbeat runs. From
the first hold since a ring was last keyed on (or the play began) a play
stands 781,250 ticks at most, a second of TMU0. It has then gone on from
the second's end, at whichever visit that is looked at, and is not held
again until a ring is keyed on: a ring given up before it sounded and
taken up again has what is left of the same second, so a ring that never
primes cannot stand its play a second at a time. The output says which
way it lets a play go: the ring was given up, or it sounds. A play that
is not held has its ring where it stands when the hold is asked, reckoned
from a count read behind the player's visit (`d4d58718`).

So the worst a hold costs a game is a second for each ring keyed on. A
ring keyed on later than that (behind a load of the game's, say) is that
much behind its play, as every ring was under Test 88 by its whole
priming.

Not measured on another console. A console whose two crystals differ by
100 parts in a million more would have that much left.

### The sides

A mixer's low five bits pan its input as a channel's pan its channel: bit
4 the side (set: left), the other four how far out. The drivers' own
words, and ChuChu Rocket's on the console (0F0F and 0F1F), have the first
input (0xa0702040) hard right and the second hard left: the first input
is the disc's right. So the left plane takes the second mixer's pan and
the right plane the first's, where the two lie on opposite sides and
neither in the middle:

| Game | Mixers | Planes under Test 88 | Planes now |
|---|---|---|---|
| ChuChu Rocket | 0F0F, 0F1F | left 1F, right 0F | the same |
| KOF Dream Match 1999 | 0F08, 0F17 | left 1F, right 0F | left 17, right 08: half way out |
| Tetsuman Menkyokaiden | 0F17, 0F08 | left 1F, right 0F | left 08, right 17: half way out and crossed, as the game has them |
| Mr. Driller, Nasubi no Heya | 0F00, 0F00; 0D00, 0D00 | left 1F, right 0F | the same |
| POD SpeedZone | 0A00, 0000 | left 1F, right 0F | the same |

Where both inputs are in the middle, a disc in a drive would play both of
its channels from the middle: one channel. Whether those games do sound
so from a disc is not known, and the planes keep their own sides until it
is. That is a choice, not a reading.

### The report

STREAM.TXT is Test 83's format. The saved report says TEST 89. The rows
are Test 88's, with two changes:

| Row | What changed |
|---|---|
| `CD W BL AH CU TB` | New, in `XC KEPT LO HI`'s place. The audio stream's own counts, 16 bits each, the second of a pair above the first. BL: parse stops for a block whose CRC16 fails, and for a lost start token. AH: stops for a block two readings fit, and blocks with lost bytes the CRC16 does not mend. CU: overruns inside a run, and runs not started again for want of room. TB: the chip's timers B and C; byte 3 the count B first read, byte 2 the bits of it seen to change since, bytes 1 and 0 the same of C |
| `CD O MX LD L2 AT TM LOST` | L2 in FB's place. Its low 16 bits: the least frames a look found the ring ahead of the chip among the looks a second and more into their ring (FFFF: no such look). Its high 16 bits: FB's count, the times MANATEE 2's level bytes stood in for mixers that read nothing; the two level bytes are no longer printed |

A report holds 582 bytes of text with nothing counted and 996 of 1,012 at
its widest. `python3 -I tools/toy_cdda_report.py STREAM.TXT` prints every
saved report of a file with these rows spelled out, Tests 85 to 89.

### Not in this build

"A stream kept through a key-on" was part of the roadmap's step. Test 87
read the least lead of 56 ms as the card handing nothing on for two or
three frames of video behind a key-on, "its stream being begun again".
Nothing in the output ends the stream at a key-on: the heartbeat that
keys on is the one whose fill reached the prime, and the next asks for
the same segment. What a newly begun stream loses to reads again (1.7 to
3.5 blocks a start) is at every start of a stream, not at the key-on. L2
and the row CD W are what will say where the lead goes; a change made
before they have is a guess.

## Which games it reaches

As Test 88: every MANATEE-lineage build, by its family.

## Console test

One launch a game, three to five minutes where the game plays CD music,
one piece left to run for two minutes or more where the game allows, the
report saved while music plays. Background loading On - New (X), CD music
Off, Virtual VMU Off, a new blank STREAM.TXT, K-UI `aaaba0ea3640` or
`53a33ec708ab`. KOF Dream Match 1999, Tetsuman Menkyokaiden, ChuChu
Rocket, Mr. Driller, POD SpeedZone.

Questions:

1. KOF and Tetsuman: is the music in stereo, narrower, with nothing that
   sounds crossed where the owner knows a side?
2. Why does KOF's stream end at a block ten times a second (BL, AH, CU)?
3. How near does a ring come to running dry in its run (L2), and at its
   start (LD)?
4. Do the chip's timers B and C read back as counts (TB)?
5. Is everything else as under Test 88 (key-ons against plays, UND, CLK,
   DRP, TK; the visits' cost)?
6. Does anything a game times to its music, or a piece that plays once,
   now end where it should? (No game of the six so far is known to show
   the old quarter of a percent.)

| Outcome | What it says |
|---|---|
| Stereo as described, counts as under Test 88 | The pans hold. Next by the report: the stream's breaks by their reason; then the installed reader (step 5) or AUDIO64 (step 4) |
| KOF or Tetsuman sounds crossed | The first input is not the disc's right, or that game's pans are not what a disc would play: the mixers and the planes in the report say which |
| TB's changing bits not zero | A timer of the chip reads back as a count: a clock for AUDIO64 that no driver has to keep, once its rate is set |
| TB's changing bits zero | They do not read back, or do not run unloaded: AUDIO64's clock is TMU0 corrected by the ratio |
| CLK counting, UND up against Test 88 | The drive's new measure moved a play's position in a way the ring's clock does not bear: AT and TM say by how much |
| A game's own sounds wrong, a hang or a crash | The if-needed module says whether the reading or the output is the cause |

## Known limits

To be written with the package.

## Verification

In progress at `d4d58718`: the full host and linked checks on a clean
checkout, the mutation run of the delivered build, and a fourth review of
the hold. Nothing is claimed here until they are in.

## Package

Not made yet.
