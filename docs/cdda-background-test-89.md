# Test 89: CD music in the Background reader, part 6: the player's rough edges

Status: packaged 28 November 2026 07:27 CST and delivered to the owner the
same day; not run on a console. Build `d4d5871818df`. A test module in the place of the installed New reader,
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
| `6764ccea` | Room. 768 bytes of the worker's image were zeros, the stream report read in the place of a block the auxiliary never wrote. They are now made when needed, in the shared block's sink, which nothing reads once the report is written. The worker had 192 bytes left; it has 256 at `d4d58718` (960 at this commit, before the rest) |
| `dd61d50b` | **The drive keeps the sound chip's time.** Three sectors are 31,172 of TMU0's ticks where they were 31,250: the chip's time by the nominal ratio of the two clocks (the processor's 199.5 MHz reckoned as 200: 2,500 parts in a million; measured 2,483, give or take 5) |
| `7d6caf2b` | Report: the row `CD W BL AH CU` in XC's place, the audio stream's own counts of why it ended at a block; `L2` in FB's place, the least lead of the looks a second and more into their ring |
| `1f677d9d`, `c3371aef` | Read only: a heartbeat of a ring that runs reads the chip's timers B and C once each, behind the clock's own reads, and the report's `TB` says what each first read and which of its bits were seen to change. Nothing is written to either |
| `37e5a467` | **Each plane has its input's pan where the game pans the two inputs to opposite sides.** The level stays the larger of the two. Where the inputs are not on opposite sides the planes keep their own sides |
| `5d699dad` | The sounding build is `retail-toy-test-89.kui`, and its report says TEST 89 |
| `308c95e6` | **The play stands still while its ring is primed** (found by review: `docs/evidence/cdda-background-test-89/review.md`). The player has a hold: a held play stands where it stood, to the tick, and goes on from there when let go. The output holds the play when it takes a ring up and lets it go when the ring is keyed on or given up |
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

1. Not run on a console. Everything below Verification is host and
   linked checks, a model of the chip and its bus, and the original
   drivers' start-up on an ARM7 model.
2. The drive's clock is the nominal ratio of the two clocks, 2,500 parts
   in a million. On the owner's console that leaves 17, measured over one
   session of one game; another console's crystals may leave more.
3. The hold changes what a game reads. For the tenth of a second a ring
   takes to prime, and for a second at most behind a card that hands
   nothing on, the position does not move, at a play's start and at each
   start over. A ring keyed on more than a second after its first hold is
   that much less a second behind its play, as every ring was under Test
   88. A PAUSE and RELEASE with no heartbeat between, while a ring primes
   or runs, leave the ring behind or ahead of its play by the pause.
4. The sides are a reading of the drivers' words and of ChuChu Rocket's
   mixers on the console, not of a disc in a drive. Where both inputs are
   in the middle the planes keep their own sides; whether a disc then
   plays one channel is not known.
5. Timers B and C are only read, and only in a ring that runs. No MANATEE
   build loads them; whether a read on the console disturbs anything of
   the chip's is not known.
6. "A stream kept through a key-on" is not done (Not in this build).
7. Seen by review and not changed here (`docs/evidence/cdda-background-test-89/review.md`): the
   player's lap arithmetic can answer a sector before a play's first for a
   play of a few sectors; a first heartbeat on a bus that does not empty,
   before any driver is admitted, leaves that play silent with no reason.
   Both are older than Test 89.
8. The output's mutation suite ran on the delivered build alone (way 111);
   the model itself ran all five ways.
9. Everything else in Test 88's Known limits stands, but for the drive's
   clock and the fixed planes.

## Verification

At commit `d4d58718` (the module) on a clean checkout,
`BUILD_ID=d4d5871818df` (`docs/evidence/cdda-background-test-89/summary.txt`
and the logs beside it). The compiler is the one built on this machine for
Test 88 from the pinned sources (GCC 15.2.0, binutils 2.45.1, newlib
4.6.0.20260123).

| Check | What ran | Result |
|---|---|---|
| The drivers read | `make toy-cdda-manatee-read` over the owner's 28 MANATEE-lineage images | 27 read, as under Test 88: 15 MANATEE 2, 6 MANATEE 1 from 1.01.37, 4 before it, 2 NAMIHEI; the damaged copy not run |
| The output on the original drivers | `make toy-cdda-output-driver-check`: the output as delivered over sound memory as each image's own start-up leaves it, nothing authored, two minutes of play with the hold | 27 of 27: family, channels, spare memory and count each the image's own; one key-on, no ring begun again |
| The survey on the original drivers | `make toy-cdda-survey-driver-check`, Test 84's, unchanged | 60 images, as under Test 84 |
| The sound output | `make toy-cdda-output-host-test`, five ways. New in the model: the chip's clock against the drive's (`aligned()`), a play held while its ring primes and ends with its music, the second's bound behind a card that hands nothing on for three seconds, a ring taken up again within its second and after it with a group made whole behind each heartbeat's count, a play that the hold's own visit finds over, the pans, L2, the timers' reads and where they stand | As delivered 1,701,712,379 checks; without VOLUME 1,689,080,258; samples in the driver's spare memory without SPARE 2,391,167,656; without ADPCM 1,672,211,304; none of the three 1,662,789,012 |
| The drive | `make toy-cdda-drive-host-test` with the output's switches: the hold to the tick, a group made whole between the hold's two reads of the clock, the second's end at a visit and at a key-on, a play held and never let go ends, the second shared and refused | 12 scenarios, 13,285 checks |
| Tests notice defects | `tests/test_toy_cdda_output_mutations.py --way 111` (234 listed, 221 for the delivered build), and `tests/test_toy_cdda_mutations.py` (56, 15 of them the hold's, built with the output's switches) | 221 of 221 caught; 56 of 56 caught (`mutation-check.log`) |
| Independent review | Four rounds (`review.md`): findings each given to a reader told to refute them | Four defects confirmed and corrected (`308c95e6`, `34a2f4ab`, `9d64a048`, `d4d58718`); the fourth round found nothing in 16,500 randomised sequences of the real player and output linked together, 763,483 rings taken up |
| The parts | `make toy-cdda-parts-check` | Both low residents and the observing worker are Test 83's, byte for byte; the auxiliary keeps its span and stack top |
| As linked | `make toy-wire-linked-check` | Passed: the auxiliary's deepest use 1,052 bytes of 1,776 (1.69 x); the output behind its gate 300 bytes deep, called only with 768 bytes of the private stack below it, 704 clear of the guard words (2.35 x) |
| The window, the audio from the card, the whole reader, the call sites | `toy-cdda-window-host-test`, `toy-cdda-card-host-test`, `toy-wire-integration-host-test`, `toy-survey-mixed-pad-host-test`, `toy-cdda-card-quiet-check` | 278 checks; 90,240,164; 460,586,482; 274 fast and 277 mixed completions each called the consumer once |
| The report | `tests/test_toy_wire_report.c` | Says TEST 89. 582 bytes with nothing counted, 761 with the audio's fields set, 996 of 1,012 at the widest; no row cut; the sink's zeros and the caps tested |
| Card Tools admission, and the launcher's byte checks | `make toy-wire-package-test` on the packaged bytes of both modules, and the header-only capability readers of `include/kui` (`games-admission.log`) | Admitted, one blank report seed, map capacity 160, heap tail, SDK proof; as Test 88's delivered modules but for offsets and sizes |
| The owner's K-UI | `python3 tools/check_gui_lines.py --installed-has 4d4dcff77494`, from the clean checkout | The installed K-UI `aaaba0ea3640` contains it |
| Main | `git fetch` of Main and of the Windows CE line before packaging | Both at `3f757bc2`, contained in this branch |
| Reproducible | The delivered recipe built twice from nothing at `d4d58718` | Module SHA-256 `8c699161...696e` both times |
| The module without the sound | The recipe with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0 TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`: linked, parts, package, report, survey and audio tests | SHA-256 `c58a87fa...f9d0`, 319,396 bytes, 5,152 bytes of the worker left; its auxiliary and both low residents are the delivered module's bytes. Built with Test 88's label it is the module Test 88 delivered, `30650218...37c9` |
| Test 85's way | The recipe with samples in a place of the table's (`TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`) | Built, its linked check and report tests pass; 325,284 bytes, 96 bytes of the worker left. Not packaged |
| The recipes before Test 85 | Test 84's, Test 83's and the installed New reader's | `68f9f3de...f22d`, `30cac0f8...507a` and `e69a6fef...93ac`, the bytes recorded for them |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 10 of 10 pass |
| The reader inputs | `python3 tools/stage_distribution.py --print-reader-inputs` | `ddf351e9...1882`, recorded in `config/distribution.json` (`16c981aa`) |

Not checked by any test here, because only a console has it: how a game
answers a position that stands while its ring primes; the pans by ear;
whether the chip's timers read back from the processor's side.

Host and linked checks are not console tests.

## Package

One archive, prepared 2026-11-28 07:27 CST (America/Chicago), from the clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Music-Test-89-28NOV2026-0727-CST-d4d5871.zip` | 390,900 | `cb230d253e58407a700a4b3c760e95021fc7e047f589e9f19fe694af2bf5dd73` |

| In the archive | What | Bytes | Payload CRC32 | SHA-256 |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | the delivered module: Test 88's switches at this commit | 325,092 | `2C7E0D17` | `8c6991612741e423bf58280cc0ccec0efff22b85d39477378df7a434c01d696e` |
| `KUI/SURVEY/STREAM.TXT` | a new blank report file, 100 slots, ID `6C8B10FDC948CC69DD63E48214F2CDBA` | 154,112 | | `2a5ac6325af9dd3c4e261d07b6c7fb704c25fe21771644b4fcc7019293973760` |
| `IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui` | the same without the output and with Test 84's survey: for a game that misbehaves; it reads the audio and drops it, and its report says TEST 85 | 319,396 | `870A02D7` | `c58a87fab9caa153b22677f3aabcba2f916e79f583bdead42b64fc7aad50f9d0` |

The archive also holds `README.txt`, `INVENTORY.json` and
`SHA256SUMS.txt`; they are in the evidence folder as `package-README.txt`,
`package-INVENTORY.json` and `package-SHA256SUMS.txt`, beside the two
modules (`retail-toy-test-89.kui`, `retail-toy-test-89-no-sound.kui`). The
modules replace `KUI/apps/games/retail-native-dual.kui` for the test only;
no K-UI, updater package, activation record, bank, game data or save data
is in the archive. The module needs the 160-place map of the owner's K-UI:
the README has the owner check the Diagnostics page's first line
(`aaaba0ea3640` or `53a33ec708ab`) before the first launch. Checked from a
fresh extraction: every file against `SHA256SUMS.txt`, both modules the
clean worktree's bytes, the report file read back empty with its ID.

Rebuild (the first; the second with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0
TOY_CDDA_VOLUME=0 TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`, which builds
`retail-toy-test-85.kui`):

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 \
  KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 \
  TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1 TOY_CDDA_SURVEY=0 \
  TOY_CDDA_CARD=1 TOY_CDDA_YIELD=1 TOY_CDDA_OUTPUT=1 TOY_CDDA_VOLUME=1 TOY_CDDA_ADPCM=1 TOY_CDDA_SPARE=1 \
  BUILD_ID=d4d5871818df BUILD=build/t89-main build/t89-main/retail-toy-test-89.kui
python3 tools/toy_survey.py blank STREAM.TXT --slots 100 --id 6C8B10FDC948CC69DD63E48214F2CDBA
```
