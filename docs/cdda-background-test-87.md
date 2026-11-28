# Test 87: CD music in the Background reader, part 4: a ring that keeps up

Status: packaged 27 November 2026, 19:47 CST (build `a54a75c8645b`, commit
`a54a75c`), and run on the console the same evening in all three games
(Console result): KOF Dream Match 1999 played its music steadily, no ring
begun again in 146 s of music; ChuChu Rocket played 26 minutes of it, a
piece of over seven minutes among them, with three rings run dry beside
the game's own reads; POD SpeedZone ran dry six times in 102 s of music,
reading through its race. No count went unborne out in any of them. A test
module in the place of the installed New reader, like Tests 79 to 86, not
an accepted component. What is left of CD music in
regular games is laid out in `universal-loader-roadmap.md` (CD music in
regular games: what is left).

It answers the owner's console result of 27 November 2026 for Test 86:
"ChuChu seemed okay, but KOF was rapidly going normal volume soft volume
every ~0.5 seconds."

## Baseline

Test 86 (`cdda-background-test-86.md`, build `0af5d9ce889c`, SHA-256
`4c5cf34c...9e23`), run on the console on 27 November 2026 under K-UI
`aaaba0ea3640` or `53a33ec708ab` (no log of those runs names it). Its
report file is `docs/evidence/cdda-background-test-86/console-2026-11-27/`.

The recipe is Test 86's, switch for switch. It is not Test 86's bytes at
this commit: every commit below changes what the sounding build does or
reports. The recipes without the sound output give the same bytes as
before: Test 84's, Test 83's, the installed New reader's, and the module
without sound that Tests 85 and 86 delivered (Verification).

## Why

Test 86's reports (its document, Console result) say:

1. KOF Dream Match 1999's ring ran dry 162 times in 83 s. Its SDK is of
   the early family, for which the audio's card work had half the reader's
   soft window, 1.5 ms a heartbeat: 2.27 visits and 1,924 bytes a
   heartbeat, where the game plays about 3,500 between two of its 50
   heartbeats a second. ChuChu Rocket, on the whole window, took what it
   played.
2. TMU0 runs 2,449 parts in a million slow against the sound chip, give or
   take 76, the same in both games. The ring's place was reckoned from
   TMU0 with a margin of 200: the chip gains 108 frames a second on that
   reckoning and passes the writer in a play of about a minute.
3. A kilobyte of frames cost 0.41 ms in ChuChu Rocket and 0.46 ms in KOF
   on its way from the card's visit into the ring, most of it on the
   worker's uncached stack and state.
4. Nothing in a report said how near a ring came to running dry, or how
   far apart the game's heartbeats came.

## The change

Eight commits on Test 86, one behaviour each. No new switch: the changes
are the sound output's wherever it is built, and one line of the card's
heartbeat under `KUI_TOY_CDDA_OUTPUT`. Changes to shared sources sit under
`#if KUI_TOY_CDDA_*` with Main's text unchanged
(`make test-toy-test-switches`).

| Commit | What it does |
|---|---|
| `d4f74cb7` | The ring's clock is the sound driver's own count of the chip's time, read at key-on and at every running heartbeat. TMU0 stays beside it as a witness |
| `21c16e2d` | The frames' way into the ring is one run that keeps its state in registers; the ADPCM coder is written by hand and uses no stack |
| `cdeb7160` | A ring that begins again keys its two channels off before it primes |
| `b856ac9d`, `a54a75c8` | A sound output has the reader's whole soft window at a heartbeat, 3 ms, whatever share of it the game's SDK family has (first only while the ring ran low; then always) |
| `ca80495b`, `e93cdd1d` | The report says TEST 87, counts the starts over for a count TMU0 did not bear out (CLK), and gives the least lead a look found and the longest wait between two looks (LD) |
| `60530a2e` | Silence (a gap between two tracks, a track the card does not hold) is written as fast as music |

The delivered module is built with Test 86's switches. Only the native
worker's sources change: the output, its header, the card's heartbeat and
the report's rows. Against Test 86's delivered parts the auxiliary is the
same bytes, both low residents and the observing worker differ in their
build label alone, and the stage holds the larger worker.
`make toy-cdda-parts-check` holds the residents and the observing worker
to Test 83's at this build label. The worker has 352 bytes left below its
stack, the auxiliary 288 below its own.

### The clock

The drivers count the sound chip's time in their timer interrupt: it
reloads the chip's timer for 44 frames, leaves out every 441st and counts
every fourth into a word of its own (0x12418 in MANATEE 2, 0x13418 in
MANATEE 1): 250 counts to a second of 44,100 frames, 176.4 frames a count,
exactly. Nothing of the driver's holds that interrupt off but its start
and its start again on the game's command (read in the three admitted
builds). Test 86 read the count for its report; its two runs put 250.6
counts in a second of TMU0's, in both driver generations.

Now the count is the ring's clock:

- It is read just before the key-on, and at every running heartbeat. What
  the chip has played is the difference, 882 frames to five counts, a
  count more or less.
- The margin is one count and the edge, 241 frames (5.5 ms), for the
  whole of a play. Under Test 86 it grew with the play's time and came
  off the lead at both ends. A ring is dry with less than the margin
  ahead of the chip; the writer stays the margin and the edge behind the
  chip's place a lap on. So the lead a heartbeat can leave is 7,823 of
  8,128 frames under MANATEE 2 (177 ms) and 11,919 of 12,224 under MANATEE
  1 (270 ms), at any time in a play.
- No ring starts over for the clocks any more. Test 86's did every 223 s
  or 339 s by its own reckoning, and on the console's clocks the chip
  would have passed the writer well before.
- A heartbeat that asks whether to look sooner than a sixtieth of a
  second reckons from the last count and TMU0's time since.

TMU0 is the witness. A count is not borne out when it differs from TMU0's
account of the time since key-on by more than a sixty-fourth of that time
and 418 frames (two counts and the edge), when it moved by more or less
than that since it was last seen to move, or when it has not moved for 418
frames of TMU0's time (9.5 ms; a count moves every 4 ms). The console's
quarter of a percent is a sixth of the sixty-fourth. A count not borne out
begins the ring again with the count as it is then, counted with the
dropped keys (CLK); the ninth start over in one play gives the play up
with reason 8, detail 2.

The measurement of the two clocks for the report (AT, TM) is kept.

### The frames' way into the ring

The worker's data, cleared state and stack are linked at uncached
addresses, each use of them a cycle on the memory bus. Test 86's way of a
unit of eight frames into the ring (`feed`, `store`, the coder, `put`,
`turn`, `drain`, as the compiler laid them out) lived on them. Now whole
units go in one run, `pour()`, and the coder of eight frames of one side,
`kui_toy_cdda_squeeze`, is written by hand: registers R0 to R7, no stack,
each sample read once as a 16-bit word, the side's coder fetched once and
put back once.

Run as linked on the repository's SH-4 interpreter (`tests/sh4_mini.py`),
the same kilobyte of frames, 64 words stored in sound memory:

| | Instructions | Uses of uncached memory | of the stack | of the state | of the bytes |
|---|---|---|---|---|---|
| Test 86's delivered worker | 57,703 | 4,879 | 2,862 | 993 | 1,024 |
| This test's | 39,065 | 1,145 | 365 | 268 | 512 |

What is stored is word for word what Test 86 stored. The bus's rule is
kept as before: the G2 FIFO is found empty, and the stretch's 3 ms not run
out, before each eight stores, which are four units; a unit's two words
are stored together or not at all; the stores go to the two planes of the
placed ring at the write position and nowhere else. Bytes that lie at an
odd address go by way of the unit's own room, as a unit cut by a chunk's
end always did. 16-bit samples (Test 85's way) keep their store, unit by
unit.

What that saves on the console is not known: the interpreter counts
instructions and accesses, not time.

### The window

The audio's card work at a heartbeat is a number of visits to the
auxiliary, each handing on a kilobyte at most: the first always, another
only for a stream that has just handed bytes on and only while the longest
such visit seen still fits a soft window of TMU0's time. The window was
the reader's own for optional work, 3 ms, and half of it under the early
SDK's drivers, whose completion runs on the game caller's schedule.

Music that sounds is not optional work: what its ring does not get in time
is not heard, and the time goes into the next start over instead. A sound
output now has the whole 3 ms whatever the family's share. `b856ac9d` gave
it that only while the ring ran low (under a third); by Test 86's numbers
the ring of an early-SDK title would then hover about that mark, a third
of the ring ahead of a stall of the game's calls where the whole of it can
stand, and `a54a75c8` took the condition out. What a second of music takes
is the same either way, the bytes it plays; the window bounds what one
heartbeat may carry, and a ring that is full asks for nothing.

For KOF that is the regime ChuChu Rocket played in under Test 86: visits
of 0.41 ms, three to a heartbeat on average, the longest heartbeat 3.5 ms.

The consumer that drops the bytes keeps the family's share, and with it
the module without sound keeps its bytes. What the output says of its ring
(low or not) still goes to the auxiliary: an idle read-ahead window is
given up on that word (Test 85's YIELD).

### Starting over, and silence

- A ring that began again when the bus's stretch had run out could leave
  its channels keyed on, looping what lay in the ring while the new ring
  was primed. A heartbeat that finds a ring to begin and channels keyed on
  now keys them off first and primes nothing before that has gone
  through. Not seen on the console (both reports count no bus wait run
  out); found by the model with sound memory turned slow.
- A heartbeat wrote 512 frames of silence at most, the measure of 16-bit
  samples. A game's heartbeats come a frame of video apart, in which the
  chip plays 737: through the two seconds between two tracks the ring ran
  dry. As ADPCM, silence costs what the card's frames cost; a heartbeat
  now writes as much of it, and on through a gap's sectors as far as its
  room goes. Not seen on the console as such (the games of Test 86 played
  one track at a time); found by the model once it was paced like a game.

## The report

STREAM.TXT is Test 83's format. The saved report says TEST 87; the pages'
heading stays Test 81's and the launch record's first line Test 83's
(`SURVEY 83 BUILD a54a75c8645b`). The rows are Test 86's, with two changes:

| Row | What changed |
|---|---|
| `CD O KEY RST UND CLK DRP TK` | CLK in ANC's place: rings begun again for a count TMU0 did not bear out. (ANC counted the starts over for the clocks' margin, which are no more.) |
| `CD O MX LD FB AT TM LOST` | LD in M1's place (the mixers' first words, which have answered). Its low 16 bits: the least frames a look found the ring ahead of the chip since the first key-on, the look that found it dry among them (under 241 the ring was dry; 0: the chip had passed the writer; FFFF: no look yet). Its high 16 bits: the longest TMU0 time between two looks at a ring that runs, in units of 64 ticks (82 microseconds) |

A report holds 577 bytes of text with nothing counted and 999 of 1,012 at
its widest, as Test 86's. No row is ever cut
(`tests/test_toy_wire_report.c`).

`python3 -I tools/toy_cdda_report.py STREAM.TXT` prints every saved report
of a file with these rows spelled out, Tests 85 to 87.

## Which games it reaches

As Test 86: MANATEE 2.00.09 (ChuChu Rocket), 1.01.3b (KOF Dream Match
1999) and 1.01.37 (POD SpeedZone), admitted by version, build date and
running mark. Every other Background-routed game runs in silence, with
its reason in the report.

## Console test

As Test 86: one launch a game, about five minutes where the game plays CD
music, the report saved while music plays; and in each game one piece left
to play for three minutes or more without a pause or a load. Background
loading On - New (X), CD music Off, Virtual VMU Off, a new blank
STREAM.TXT, K-UI `aaaba0ea3640` or `53a33ec708ab`. KOF first, then ChuChu
Rocket; POD SpeedZone if there is time (it was not run under Test 86).

Questions:

1. Does KOF's music play steadily (UND, the key-ons against the plays)?
2. Is the driver's count a clock for minutes on end (CLK 0; a piece of
   three minutes with no start over)?
3. What does a kilobyte cost now (the visits' time: 0.41 ms and 0.46 ms
   under Test 86)?
4. How many visits does KOF's heartbeat take on the whole window, how long
   is its longest heartbeat, and does the game keep its speed?
5. How near do the rings come to running dry, and how far apart do the
   games' heartbeats come (LD)?
6. POD: how often does the 277 ms ring run dry beside the game's reads
   (Test 85's 371 ms ring: 9 times in 63 s of music)?

| Outcome | What it says |
|---|---|
| KOF steady, UND at most a few, CLK 0, a long piece unbroken | The clock and the window hold. Next: the drive's own clock by the chip's, the mixers' pan, the other MANATEE builds by survey, a longer ring for games that read through their music, then AUDIO64 |
| KOF still drops out, UND counting, LD's wait long (tens of milliseconds and more) | The game's heartbeats come too far apart for the ring at times: the audio needs a heartbeat of its own (the stream's timer) or a longer ring |
| KOF still drops out, UND counting, LD's wait short, few visits a heartbeat | The card does not hand on enough within the window: the visits' time and the blocks read again say where it goes |
| CLK counting, or a play given up with reason 8 detail 2 | The driver's count is not the chip's clock in that game, or not always: AT and TM say by how much |
| Clean for a minute or more, then crackle or drop-outs at a steady rate with UND 0 | The chip is not where the count says: LD's lead against the margin |
| KOF slows down or stutters in the picture | The whole window is too much of that game's schedule: its share has to be met with a cheaper visit, not a longer window |
| A game's own sounds wrong, a hang or a crash | The if-needed module (the card's reading without the sound) says whether the reading or the output is the cause |

## Console result

The owner's runs of 27 November 2026, with the file from the card,
`KUI/SURVEY/STREAM.TXT` (ID 9C05737ABA3BD4DF58FDFE369A06868C, two of its
hundred slots used, SHA-256 `8bcabe55...2c60`). The file, the tool's
listing and the reports with their CD rows spelled out
(`tools/toy_cdda_report.py`) are in
`docs/evidence/cdda-background-test-87/console-2026-11-27/`. The owner's
words: "Seems like King of Fighters worked." Both slots are KOF Dream
Match 1999; ChuChu Rocket and POD SpeedZone were not run.

Both launch records name this build, `a54a75c8645b`, and both reports say
TEST 87 and the ring's kind 3: the main module. No log of these runs names
the K-UI.

| Slot | In the report |
|---|---|
| 0 | 3 PLAY, 8 PAUSE; playing at the save. **3 key-ons, no ring begun again**: none run dry, no count not borne out, no key dropped, no channel taken, no guard lost. 73.8 s of music taken. 4,391 heartbeats, 1.58 ms each on average, the longest 3.23 ms; 3.52 visits and 2,966 bytes a heartbeat, 59.5 heartbeats to a second of music. LD: the least lead 2,491 frames (56 ms), the longest wait between two looks 16.7 ms. The mixers 0F08 and 0F17. AT 21,536, TM 67,136,178. The card: 24,723 blocks, 712 read again, 715 by programmed transfers, 714 streams ended by the audio |
| 1 | 5 PLAY, 8 PAUSE; playing at the save. **5 key-ons, no ring begun again.** 72.8 s of music taken. 4,313 heartbeats, 1.60 ms each, the longest 3.23 ms; 3.55 visits and 2,976 bytes a heartbeat. LD: 2,547 frames (58 ms), 16.7 ms. AT 21,386, TM 66,667,241. The card: 24,330 blocks, 736 read again, 740 by programmed transfers, 740 streams ended by the audio |

What it shows:

1. **KOF plays steadily.** Eight plays and 146 s of music with no ring
   begun again for any reason, where Test 86 began 162 again in 83 s.
2. **The whole window carries it.** A heartbeat took 3.5 visits on average
   and 2,970 bytes, what a sixtieth of a second plays; the heartbeats come
   a frame of video apart (the longest wait between two looks is 16.7 ms
   in both runs). The 50 heartbeats a second reckoned from Test 86's
   report were a play in pieces.
3. **The count holds as the clock.** No count went unborne out. TMU0 ran
   2,434 and 2,454 parts in a million slow against it, give or take 94:
   with Test 86's two, four measurements in two games, 2,447 give or take
   50 by weight. Not shown yet: one play of several minutes unbroken (the
   longest here is under 74 s).
4. **A visit costs 0.38 ms** where it cost 0.46; without the visits that
   read a block by programmed transfers, 0.33 where it was 0.41. The
   frames' cheaper way into the ring saved 0.08 ms a kilobyte, a fifth of
   a visit, about what its fewer instructions are worth: the uncached
   stack and state were not the cost they were taken for, and most of a
   visit is the card's block. A heartbeat's fixed part is 0.26 ms. In all
   the music took 6.9 s of the console's 86 s, 8 percent.
5. **The least lead, 56 and 58 ms, fits the start of a play**, not its
   run: a ring is keyed on with a tenth of a second or a little more
   written, and 2,491 frames are what is left of that after two or three
   frames of video in which the card hands nothing on, its stream being
   begun again behind the key-on. In its run a full ring stands 270 ms
   ahead. One least for a whole run cannot tell the two apart: the next
   report keeps a second one from a play's first second on.
6. **The audio's stream breaks eight times a second in KOF.** 712 and 736
   blocks came out of the stream neither clean nor rebuilt; each ends the
   stream, is read again by programmed transfers (1.22 ms) and the stream
   begun anew: 871 ms and 902 ms, an eighth of the music's time. ChuChu
   Rocket under Test 86 had 17 in 64 s. Not heard, with the ring full;
   why KOF's stream loses bytes so often is not known.
7. Not answered by these runs: ChuChu Rocket and POD SpeedZone under this
   build; a play of minutes; whether the game keeps its speed, beyond the
   owner's "worked". (The later runs below answer the first two.)

### The later runs: ChuChu Rocket and POD SpeedZone

The owner's second file of the same evening, the same card file with four
more slots used (SHA-256 `7fa9ddd0...dbdd`; its first two slots are the
first file's, byte for byte), in
`docs/evidence/cdda-background-test-87/console-2026-11-27-later/`. The
owner's words: "Got those game reports for you. Also got the 7 minutes+ of
music in the main menu of ChuChu rocket." All four launch records name
build `a54a75c8645b`.

| Slot | Game | In the report |
|---|---|---|
| 2 | POD SpeedZone | Launched, no report saved. Nothing says why: the owner reported no fault |
| 3 | ChuChu Rocket | 11 PLAY, 17 PAUSE. 12 key-ons, **1 ring begun again, for a ring run dry**; no count unborne out. 128.3 s of music. 7,646 heartbeats, 1.25 ms each, the longest 3.23 ms; 3.19 visits and 2,960 bytes a heartbeat. The longest wait between two looks 33.3 ms. AT 33,062, TM 103,061,028. The card: 44,100 blocks, 36 read again, 47 by programmed transfers; 2 streams ended for a read of the game's |
| 4 | POD SpeedZone | 3 PLAY. 9 key-ons, **6 rings begun again, each for a ring run dry**; no count unborne out. 101.6 s of music. 594 data reads of the game's during a play; 73 streams ended for them, 41 read-ahead windows given up for the audio. 7,122 heartbeats, 1.07 ms each, the longest 3.41 ms; 2.87 visits a heartbeat. The mixers 0A00 and 0000, the planes at 0A1F and 0A0F. AT 29,738, TM 92,699,654 |
| 5 | ChuChu Rocket | 38 PLAY, 83 PAUSE, 7 plays ended, 10 laps. 40 key-ons, **3 rings begun again, each for a ring run dry**; no count unborne out. **1,556.1 s of music** in a session of 1,595 s by the driver's count. 93,115 heartbeats, 1.24 ms each, the longest 3.21 ms; 3.17 visits and 2,948 bytes a heartbeat. AT 398,754, TM 1,243,012,349. The card: 535,301 blocks, 379 read again, 426 by programmed transfers; 4 streams ended for a read of the game's |

What they show:

8. **The count is a clock for minutes on end.** 26 minutes of music in one
   session, a piece of over seven minutes in the menu among them by the
   owner's account (10 laps counted), and no ring begun again for the
   clock in any game. By Test 86's reckoning from TMU0 the chip would have
   passed the writer within a minute of each such play. POD's build
   (1.01.37) keeps its count where KOF's does.
9. **The two clocks, to five parts in a million**: over ChuChu Rocket's
   1,595 s TMU0 runs 2,483 slow against the chip, give or take 5; the six
   other measurements agree with it (2,402 to 2,528, each give or take 60
   to 125). A console's constant, and near the 2,500 of a processor clock
   of 199.5 MHz reckoned as 200.
10. **Like for like, the frames' way into the ring is three tenths
    cheaper.** ChuChu Rocket under Test 86 and here: a visit 0.41 ms and
    0.29 ms, a heartbeat 1.77 ms and 1.24 ms, for the same 3 kilobytes.
    (KOF's 0.46 and 0.38 of finding 4 compare a play in pieces with a
    whole one.) The music takes 7.2 percent of the console's time in
    ChuChu Rocket and 6.4 in POD.
11. **What still runs a ring dry is the game's own reading.** ChuChu
    Rocket: 4 rings run dry in 28 minutes of music, with 6 streams ended
    for a read of the game's; KOF, which reads nothing while it plays,
    none. POD reads through its race, 594 reads in two minutes: 6 rings
    run dry in 102 s of music, one in 17 s, where Test 85's larger ring of
    samples ran dry once in 7 s. A read that holds the card longer than
    the ring's 0.18 s or 0.28 s is heard as a short break.
12. **KOF's broken streams are KOF's.** Blocks read again for a second of
    music: about 10 in KOF, 0.24 in ChuChu Rocket, 0.55 in POD.
13. POD's first launch saved no report (slot 2). Nothing in the file says
    whether the game was left without the gesture or stopped.

## Known limits

1. Run on a console in the three games (Console result), by one owner on
   one console. The hand-written coder agrees with the interpreter there
   in what is heard.
2. The count is true to one count, 4 ms, either way: that is the margin,
   and it does not shrink.
3. The drive's own clock is still TMU0. The play's position as the game
   reads it runs a quarter of a percent behind the music; a play that
   starts over takes the music up that much earlier (2.4 ms for each
   second played), and a play that is not repeated ends for the game that
   much after its last frame, in silence.
4. 4-bit ADPCM is not the disc's 16 bits. No listening test is possible
   here.
5. The ring is small: 184 ms (MANATEE 2) or 277 ms (MANATEE 1). While the
   game's own read holds the card longer than the lead, the music stops
   and comes back at the drive's position. POD SpeedZone reads through a
   race.
6. A ring that runs dry plays what lay there until the heartbeat that
   notices: as ADPCM that is old codes behind a decoder they were not made
   for. Likely a click.
7. A stream newly begun loses its first blocks to reads again (1.7 to 3.5
   a start under Tests 85 and 86): unchanged, and part of every start
   over's gap.
8. A heartbeat's fixed part, the reads that prove the driver, the
   channels and the guards, is unchanged (0.26 ms in KOF's steady play).
9. Under the early SDK's drivers the audio now takes up to the whole
   window of the game caller's schedule, as under the newer ones.
10. The planes are on fixed sides at one level. KOF gives its mixers 0F08
    and 0F17, each side half way out.
11. Three driver builds only. The standalone New reader's games are not
    reached.
12. Test 85's way (16-bit samples in a place of the table's) still builds
    and passes its checks; its silence keeps the old measure, and the
    model holds it to one track where its heartbeats are paced like a
    game's.

## Verification

At commit `a54a75c8` on a clean checkout, `BUILD_ID=a54a75c8645b`, and for
the output's tests at `7fc140c3`, which changes tests only
(`docs/evidence/cdda-background-test-87/summary.txt` and the logs beside
it).

| Check | What ran | Result |
|---|---|---|
| The parts | `make toy-cdda-parts-check`: Test 83's recipe, Test 84's and this one without YIELD beside this one | Both low residents and the observing worker are Test 83's, byte for byte. The auxiliary keeps its span and stack top; its code ends at 0x8c4277e0 below the stack at 0xac427900, as Test 86's, whose bytes it is. The worker's player descriptor names the shared block in its fourth word: 1,312 bytes at 0xac110c00 in the cleared state |
| As linked | `make toy-wire-linked-check`: Test 77's linked checks, the auxiliary's stack bound, `tests/test_toy_cdda_card_linked.py` and `tests/test_toy_cdda_output_linked.py` on the worker | Passed. The auxiliary's deepest use is 1,052 bytes with the gate, of 1,776 usable (1.69 x). The worker: the consumer is the output's gate; the output behind it is 300 bytes deep (344 under Test 86), the card's heartbeat, the visit's gate and the frames' way into the ring among them, and is called only with 768 bytes of the private stack below it: 704 clear of the guard words (2.35 x). Called from the completion only, at 2 sites, each after `stack_valid`. The hand-written coder has no frame of the compiler's: the check holds it to no stack and no call, and refuses 19 altered readings of itself |
| The coder and the run, as linked | `tests/test_toy_cdda_output_linked.py`, on the repository's SH-4 interpreter | `kui_toy_cdda_squeeze`: 20,000 words of eight samples equal to a coder of the test's own, every code, both ends of the signal and of the step and the moves that must not be asked for among them; 776 instructions for eight samples at the most; no stack, no register beyond R0 to R7, nothing beside its samples and its coder touched. `feed()`: the words stored are the coder's, in their planes and round the ring's end, the write position and both coders in step, the G2 FIFO asked before each eight stores, a stretch out of time stopped between units; a kilobyte of the check's frames takes 32,947 instructions and 1,145 uses of uncached memory, held to a bound |
| The sound output | `make toy-cdda-output-host-test`: `tests/test_toy_cdda_output.c` built five ways (VOLUME, ADPCM, SPARE: 111 as delivered, 011, 110, 100, 000). The model is Test 86's with the driver's count: 250 to a second of the chip's own time, which runs a set number of parts in a million off TMU0. It keeps its own account of where each plane's next word belongs and of the decoder behind the codes stored, and holds the output's write position and coders to it after every heartbeat; no access of the bus may come later than a burst behind a stretch's 3 ms | At `7fc140c3`, as delivered 1,252,149,822 checks; without VOLUME 1,243,033,897; with ADPCM in a place of the table's 1,811,138,564; with samples 1,116,418,482 and 1,110,273,843. As delivered: a play on each of the three builds in the spare memory, 1.6 million frames each, every one the play's, through pause, release and the end. The chip's clock 2,450 parts in a million fast and 6,000 slow against TMU0 over five minutes in each ring: every frame the play's and no ring begun again. A count that stands still, fell or jumped, early and five minutes in, begins the ring again before anything else is heard; one that is no clock is given up. A game that calls once a frame of video, the chip a quarter of a percent ahead of TMU0: three kilobytes a heartbeat, and four, hold the ring for a minute, out of one track, through the gap and into the next; two, KOF's under Test 86, run it dry 128 times in the minute. A play's last frames at 150 phases of the count and of the heartbeats: every one played, and 144 to 1,116 frames of silence behind them before the key-off. A card that hands on 999 bytes at a time, and 3; sound memory turned slow under a play. And Test 86's: tracks, a gap, a refused track and laps; nothing written for another driver, one not running, a held sound CPU, unproved records, a used channel, no clock, a G2 transfer, SR.BL; the ring given up when a guard says so and taken again, eight times; a taken channel, a dropped key, changed registers; the levels; the terminal key-off |
| The window | `make toy-cdda-window-host-test`: the worker's card heartbeat as compiled with a sound output, the auxiliary, the clock and the family's share in the test's hands | 278 checks. Seven visits of 300 ticks under either share, whatever the output says of its ring (the share's half would end them at three); no more than the ring's room; none behind a visit that handed nothing on or handed the segment's last bytes on; the longest visit seen reckoned with; nothing with no auxiliary bound; the output's word of its ring passed to the auxiliary as it is |
| The audio from the card | `make toy-cdda-card-host-test` | 90,240,164 checks, as under Tests 85 and 86: the consumer that drops the bytes keeps the family's share |
| The whole reader | `make toy-wire-integration-host-test` | 460,586,482 checks. A play beside 5 reads: function 4 answered 3 all 15 times |
| Asked for nothing | `make toy-cdda-card-quiet-check` | Folds to `f8565e87` with the audio's card work built in and without |
| The call sites | The gesture test (`toy-survey-mixed-pad-host-test`) | 274 fast and 277 mixed completions each called the consumer once |
| The drive | `tests/test_toy_cdda_drive.c` | 12 scenarios, 11,704 checks, as under Test 84 |
| Tests notice defects | `tests/test_toy_cdda_output_mutations.py` in three ways (`--way 111`, `110`, `100`) at `7fc140c3`; `tests/test_toy_cdda_card_mutations.py`; Test 84's drive mutations | 163 of 163 caught as delivered (`--way 111`) and 143 of 143 with samples. With ADPCM in a place of the table's 163 of 165: the two left, silence's measure, cannot be seen in that build (its ring of a second and a half holds through a gap's two seconds at either measure) and are asked of the spare ring since `7a4eaf05`. The first full run, at `a54a75c8`, had shown the list behind the source in four places (three alive, one not applied); list and model were corrected in `7fc140c3`, with the scenario of a play's last frames. The card's: 197 of 197, 182 by the audio's scenarios, 7 by the gesture test and 8 by the window test. 41 of 41 of the drive's |
| The report | `tests/test_toy_wire_report.c` | 577 bytes with nothing counted, 731 with the audio's fields set, 999 of 1,012 with every other row at eight digits a value; the audio's rows all written, no row cut |
| The card | Test 83's save and survey-file tests with this recipe | Passed on FAT32 and exFAT, the volume at sector 0 and 63; `tools/toy_survey.py`'s blank file is the stage's |
| Card Tools admission | `make toy-wire-package-test` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The launcher's admission helpers, compiled against the K-UI's sources, on the packaged bytes of the two modules (`games-admission.log`) | As Test 86's modules, line for line but for offsets: envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New and the Legacy match, the 160-place map, the toggle, the mode records, a blank cache seed slot, the heap-tail and SDK-proof capabilities |
| The owner's K-UI | `python3 tools/check_gui_lines.py --installed-has 4d4dcff77494`, from the clean checkout | The installed K-UI `aaaba0ea3640` contains the 160-place map's line; so does `53a33ec708ab` |
| Main | `git fetch origin main` and the Windows CE line before packaging | Both at `3f757bc2`, contained in this branch |
| Reproducible | The delivered recipe built twice from nothing | Module SHA-256 `0ccfaa60...2094` both times |
| The module without the sound | The recipe with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0` and neither of Test 86's switches: its linked, parts and package checks, its report, gesture, survey and audio tests | SHA-256 `2106e418...0445`. Its auxiliary and both low residents are the delivered module's bytes. The consumer that drops the bytes is 104 bytes deep and stays 88 clear of the guard words on the least stack (1.85 x). Its report says TEST 85: 457 bytes with nothing counted, 971 of 1,012 at the widest. 319,396 bytes; 5,152 bytes of the worker left. Built with Test 85's label and with Test 86's it is the no-sound module each of those tests delivered, byte for byte (`1bd6e044...b411`, `cb4c31e8...c74e`) |
| Test 85's way | The recipe with samples in a place of the table's (`TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`): built, its linked check and its report tests | Passed; 325,092 bytes, 192 bytes of the worker left; the output behind its gate 352 bytes deep (2.00 x). Not packaged |
| The recipes before Test 85 | Test 84's, Test 83's and the installed New reader's, built at this commit and at `f4cc67dd` | The same bytes at both: `68f9f3de...f22d`, `30cac0f8...507a` and `e69a6fef...93ac` |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 10 of 10 pass |

Measured beside the checks, with the same kilobyte of frames on Test 86's
delivered worker and on this one (The frames' way into the ring): 57,703
instructions and 4,879 uses of uncached memory before, 39,065 and 1,145
now.

Read in the owner's driver files, on this machine only (none of their
bytes is in the repository): the timer interrupt in the three admitted
builds, its reload for 44 frames, the 441st left out, the count of every
fourth and the word it is kept in; and where the drivers hold that
interrupt off (their start, and the start again on the game's command).

Not checked by any test here, because only a console has it: whether the
whole window brings KOF's music in and leaves the game its speed; what the
hand-written coder saves in time; whether the count holds as the clock
over minutes in each game; how near the rings come to running dry.

Host and linked checks are not console tests.

## Package

One archive, prepared 2026-11-27 19:47 CST (America/Chicago), from the clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Music-Test-87-27NOV2026-1947-CST-a54a75c.zip` | 390,626 | `3e6deb31251354daa2984fc62c37c2f4a9571a76ce45fc72c44de46eb4d1f25e` |

| In the archive | What | Bytes | Payload CRC32 | SHA-256 |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | the delivered module: Test 86's switches at this commit | 324,900 | `0B47EDEE` | `0ccfaa602486bcf8b91541c739cd7af83099553f81c1e208b9e916c2b1462094` |
| `KUI/SURVEY/STREAM.TXT` | a new blank report file, 100 slots, ID `9C05737ABA3BD4DF58FDFE369A06868C` | 154,112 | | `ca74bf2439b383a15af07916fb8403997ee3d7ac6096766313a5e312202d63ef` |
| `IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui` | the same without the output and with Test 84's survey: for a game that misbehaves; it reads the audio and drops it, and its report says TEST 85 | 319,396 | `6CE6E39D` | `2106e4185b37891496c53943169c608bf192e26b53cdd9f6b380efa4a8c10445` |

Test 86's module at full level is not repeated: the CD mixers read back
in all three games, and no run needed it.

The archive also holds `README.txt`, `INVENTORY.json` (the files with
their full hashes, what stays on the card and what the module needs of
K-UI) and `SHA256SUMS.txt`; they are in the evidence folder as
`package-README.txt`, `package-INVENTORY.json` and
`package-SHA256SUMS.txt`, beside the two modules
(`retail-toy-test-87.kui`, `retail-toy-test-87-no-sound.kui`). The modules
replace `KUI/apps/games/retail-native-dual.kui` for the test only; no
K-UI, updater package, activation record, bank, game data or save data is
in the archive. The module needs the 160-place map of the owner's K-UI:
the README has the owner check the Diagnostics page's first line
(`aaaba0ea3640` or `53a33ec708ab`) before the first launch.
