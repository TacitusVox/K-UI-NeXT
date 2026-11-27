# Test 86: CD music in the Background reader, part 3: music where sound memory is full

Status: packaged 27 November 2026, 16:06 CST (build `0af5d9ce889c`, commit
`0af5d9c`), and run on the console the same day (Console result):
ChuChu Rocket played its music; KOF Dream Match 1999 played it in pieces,
its ring running dry twice a second; and the two reports show TMU0 a
quarter of a percent slow against the sound chip's clock. Test 87
(`cdda-background-test-87.md`) answers both. A test module in the place of
the installed New reader, like Tests 79 to 85, not an accepted component.

It answers the owner's console result of 27 November 2026 for Test 85:
"Didn't work for either King of Fighters nor Chuchu rocket. POD seemed to
work, however."

## Baseline

Test 85 (`cdda-background-test-85.md`, build `b9cff7086258`, SHA-256
`59739533...1f2a`), run on the console on 27 November 2026 under K-UI
`aaaba0ea3640` or `53a33ec708ab` (no log of those runs names it). Its
report file is `docs/evidence/cdda-background-test-85/console-2026-11-27/`.

With this test's switches off, Test 85's recipe is not kept byte for byte:
three of the seven commits change what every sounding build does (the ring
begun again, the level, the lost ring) and one the report. Test 85's way of
keeping the music, 16-bit samples in a place of the bank table's, still
builds and passes its checks at this commit (Verification); it is not
packaged. The recipes without the sound output give the same bytes as
before Test 85: Test 84's, Test 83's and the installed New reader's.

## Why

Test 85's report (its document, Console result) says:

1. ChuChu Rocket and KOF Dream Match 1999 were silent for want of a place:
   the driver's bank table left no 16,456 bytes free from 0x18000 up for
   the smallest ring of samples. Their sound data fills sound memory.
2. POD SpeedZone played, the left plane in the middle and the right plane
   not at all: the game gives one CD mixer level 10 and the other nothing.
3. POD lost one ring to a pair of the table that came to lie in it, and
   that play stayed silent to its end.
4. A heartbeat spent about 0.9 ms on the reads that prove the driver, the
   channels, the whole table and the guards, before any audio.
5. The report's levels read 0: the terminal key-off came first.

## The change

Seven commits on Test 85's recipe, one behaviour each. Two are new
switches of `Makefile.vmu-native-toy-suite`; the others change the sound
output wherever it is built. Changes to shared sources sit under
`#if KUI_TOY_CDDA_*` with Main's text unchanged
(`make test-toy-test-switches`).

| Commit | Switch | What it does |
|---|---|---|
| `30051039` | (OUTPUT) | A channel's address, loop end or pitch found changed by the driver is put back and the ring begun again: the chip's place in the ring is no longer the reckoned one |
| `55461229` | `TOY_CDDA_ADPCM` | The ring holds the sound chip's 4-bit ADPCM, which the worker makes of the card's samples: a quarter of the memory and of the bus traffic. The report says TEST 86 |
| `35fc5d84`, `ced88dbe` | `TOY_CDDA_SPARE` | The ring lies in the end of the driver's own memory that no build of the driver uses, and nowhere else. The bank table is not read |
| `a62f2e9e` | (VOLUME) | Both planes play at the larger of the two CD mixers' levels, the left plane hard left and the right plane hard right |
| `b0860e15` | (OUTPUT) | A ring that is lost is taken again within the play, up to eight times a play |
| `0af5d9ce` | (report) | The levels as last played, the mixers' first words, the ring's kind, and the sound chip's clock beside TMU0 |

The delivered module is built with Test 85's four switches and these two,
without Test 84's survey. Only the native worker's sources change: the
output, its header and the report's rows. Against Test 85's delivered
parts the auxiliary is the same bytes, both low residents and the
observing worker differ in their build label alone (12 bytes each), and
the stage holds the larger worker. `make toy-cdda-parts-check` holds the
residents and the observing worker to Test 83's at this build label. The
worker has 800 bytes left below its stack, the auxiliary 288 below its
own.

### The compressed ring (ADPCM)

The sound chip decodes a 4-bit ADPCM of its own: a code moves the signal
by (twice its low three bits, and one) eighths of the step, down with its
top bit set, and multiplies the step by 230, 230, 230, 230, 307, 409, 512
or 614 256ths, between 127 and 24,576. At key-on the decoder stands at
signal 0 and step 127. The worker makes these codes of the card's samples
as they pass, eight frames to a word for each plane, the first frame in
the word's lowest four bits, and keeps for each plane the decoder as the
chip's will stand behind the codes written. That state moves on only when
both planes' words are stored, and begins again with the ring.

The two channels are keyed in the format whose decoder runs on across the
loop's end (PCMS 3, bits 8:7 of the channel's first register): in the
other ADPCM format the chip takes the decoder back to the loop's start at
every lap, which a ring cannot use. Loop start 0, loop end the plane's
frames, as with samples.

That is the games' own driver's way with a compressed stream. Read in
MANATEE 2.00.09 (image 0x5EA8..0x60AC, the start of a stream port): the
port's format bits go into bits 8:7 of the key word 0x4200; a port asked
for either ADPCM format is keyed in format 3; the loop start is 0 and the
loop end the stream ring's bytes times two, the number of codes in it,
held to 0xE000 at most.

Two more things the coder does:

- It never asks for a move of more than 32,767, which the largest steps
  allow: there the published accounts of the chip differ (one holds the
  move to 32,767, others do not).
- A cleared ring holds 0x80808080, a step up and a step down: zeros would
  decode to a rising ramp.

The store path is common to samples and ADPCM: bytes are gathered into
whole units (two frames of samples, eight of ADPCM), a unit that cannot
be stored waits whole, and the frames gathered stay across a piece's end.
A byte that does not reach the ring still starts the play over.

The ring starts over for the two clocks' margin when the margin is a
quarter of the ring (an eighth with samples): the ring is small, and a
start costs a tenth of a second of music.

### The driver's spare memory (SPARE)

| Family | The ring | A plane | Holds |
|---|---|---|---|
| MANATEE 2 (ChuChu Rocket) | 0x16000..0x17FFF | 8,128 frames in 4,064 bytes | 184 ms |
| MANATEE 1 (KOF Dream Match 1999, POD SpeedZone) | 0x15000..0x17FFF | 12,224 frames in 6,112 bytes | 277 ms |

A guard line of 32 bytes, the left plane, the right plane, a guard line:
the whole of the span, ending where the game's memory begins.

Why it is free, read in the three admitted builds (that none names an
address from 0x16000 to below 0x18000 was also matched by pattern in all
28 MANATEE builds of the owner's archive):

- The driver's start-up sets its stack at 0xB000; its interrupt handlers
  keep theirs inside the image.
- The tables at image 0xC0 name nothing higher than the command history
  at 0x14800 (indexed under the mask 0x7FF: to 0x14FFF) and, in MANATEE 2,
  the host's MIDI ring at 0x15000 (mask 0xFFF: to 0x15FFF).
- No build holds an address between there and 0x18000. 0x18000 itself is
  the word the driver gives the game as the start of free memory (stored
  at 0x12408, MANATEE 1 at 0x13408), and the bound below which it refuses
  the effect memory's pair.
- The start-up, and the start again on the game's command, clear
  0x12000..0x16FFF (MANATEE 2) or 0x13000..0x17FFF (MANATEE 1).

So the driver writes there only when it starts, which the guard lines
show at the next heartbeat: the ring is lost, the play takes it again once
the driver runs.

The bank table is not asked at all. A place the table leaves free at one
moment is where the game may load its next sound data, and the table
names that data only after it is loaded: a ring there can be written over
the game's new data, or by it, before the table says so. POD's lost ring
in Test 85 was that. Without the table's 168 reads a heartbeat's fixed
cost falls by about half a millisecond on Test 85's measure.

What stays unknown: whether some game writes below 0x18000 itself. The
guard lines catch a write that crosses them, not one inside a plane.

### The level (VOLUME)

Test 85 gave each plane its mixer register's level and pan. Now both
planes play at the larger of the two registers' levels, the left plane
hard left and the right plane hard right. Two things the registers' own
words do not settle: which of the two is the disc's left (the drivers'
full-level words, 0x1F0F and 0x1F1F, pan the first to the right and the
second to the left), and whether a game that gives them different levels
means the two sides of its music. MANATEE 2's own level bytes stand in as
before where both registers read nothing for eight heartbeats; one of the
two bytes is now enough for full level on both planes.

### The driver's part (two corrections)

- Registers put back. A command of the driver's for an idle port can
  rewrite a channel's registers. For the address, the loop end and the
  pitch, putting them back is not enough: the chip has stepped by the
  changed value in between. The ring begins again from the drive's
  position, counted with the dropped keys (given up at the ninth in one
  play). Not observed on the console: POD's Test 85 report counts no
  register put back.
- A lost ring. A ring given up for a guard line overwritten (or, without
  SPARE, a pair of the table) is taken again at once and the play begins
  there from the drive's position, counted the same way.

## The report

STREAM.TXT is Test 83's format. The saved report says TEST 86; the pages'
heading stays Test 81's and the launch record's first line Test 83's
(`SURVEY 83 BUILD 0af5d9ce889c`). The rows are Test 85's, with these
changes:

| Row | What changed |
|---|---|
| `CD O ST V ID RING N` | N's top byte is the ring's kind: 1 ADPCM, 2 in the driver's spare memory (3 in the delivered module) |
| `CD O HB LONG TICK KB SKIP LV` | LV: the last levels written that were levels, the left plane's and the right's <<16 (Test 85's read 0 behind the terminal key-off). KB still counts the card's audio, 176,400 bytes a second of music |
| `CD O MX M1 FB AT TM LOST` | In the place of `MX FB PRIME SIL LOST`. MX: the CD mixers as last read, the first and the second <<16. M1: as first read with something in them. FB: as before. AT: the driver's count of 4 ms of the sound chip's time since a running heartbeat first looked. TM: TMU0's ticks in that time. LOST: as before |

AT and TM. The drivers count the sound chip's own time in their timer
interrupt, 250 to a second (the word at 0x12418 in MANATEE 2, 0x13418 in
MANATEE 1; read in the three builds). 3,125 of TMU0's ticks go to one
count where the two clocks agree, so TM / AT against 3,125 is the two
clocks' drift, true to one count at either end: over three minutes, to
about 45 parts in a million. The ring's margin allows 200. A count that
fell (the driver started again) begins the measurement again. It costs
one word read twice a heartbeat.

A report of the delivered module holds 577 bytes of text with nothing
counted, of 1,012, 731 with the audio's fields set, and 999 with every
other row at eight digits a value. No row is ever cut
(`tests/test_toy_wire_report.c`).

`python3 -I tools/toy_cdda_report.py STREAM.TXT` prints every saved report
of a file with these rows spelled out, Test 85's and this test's.

## Which games it reaches

The three builds of Test 85, admitted by version, build date and running
mark: MANATEE 2.00.09 (ChuChu Rocket), 1.01.3b (KOF Dream Match 1999) and
1.01.37 (POD SpeedZone). On the owner's card all three have their audio
mapped (KOF: `AUDIO 40 ... MAPPED 40 ... EXTENTS 49` in its Test 85 launch
record). Every other Background-routed game runs in silence, with its
reason in the report.

The spare memory is the family's, not the three builds': the other
MANATEE builds need their two channels shown free by a survey, as before,
and no place.

## Console test

As Test 85: three games, one launch each, three to five minutes where the
game plays CD music, the report saved while music plays. Background
loading On - New (X), CD music Off, Virtual VMU Off, a new blank
STREAM.TXT, K-UI `aaaba0ea3640` or `53a33ec708ab`.

Questions:

1. Is there music in ChuChu Rocket and KOF Dream Match 1999?
2. Does the chip play the compressed ring cleanly round its end, for
   minutes? This is the first time this output keys PCMS 3.
3. Does the spare memory stay the ring's (LOST's third byte, reason 5)?
4. How often does the small ring run dry beside the game's reads (UND)?
   POD's ring held 371 ms in Test 85 and ran dry 9 times in 63 s of music.
5. Is the level right, and are left and right on their sides (LV, MX, M1)?
6. What do the two clocks differ by (AT, TM)?
7. What does a heartbeat cost now (HB, TICK; Test 85: 2.48 ms in POD)?

| Outcome | What it says |
|---|---|
| Music, clean, in all three | The compressed ring and the spare memory hold. Next: the driver's count as the ring's clock (no starting over, the whole ring as lead), the other MANATEE builds by survey, then AUDIO64 |
| Music with a click or a change at a steady rate, about five a second in ChuChu Rocket (a lap is 184 ms) and four in the others (277 ms) | The loop is not the length reckoned: the decoder runs a code more or less each lap |
| Music that turns rough, louder or softer over seconds | The chip's decoder does not follow the coder's account of it |
| Music that drops out, UND counting | The small ring runs dry: UND against the play's time and HD say how often and behind which reads |
| No music, reason 5 detail 3, LOST's third byte counting | Something writes the spare memory in play: the driver's start, or the game |
| No music, MX and M1 zero | The mixers read nothing in that game: the first if-needed module (full level) is the comparison |
| No music, phase 3 with another reason | 1 to 4: the driver or its records are not as surveyed. 6, 7: the driver uses or stops the channels |
| A game's own sounds wrong, a hang or a crash | The second if-needed module (the card's reading without the sound) says whether the reading or the output is the cause |
| TM / AT within 200 parts in a million of 3,125, steadily | The driver's count can be the ring's clock |

## Console result

The owner's runs of 27 November 2026, with the file from the card,
`KUI/SURVEY/STREAM.TXT` (ID A7E2DEC4944B4B399F8B3C119C906300, two of its
hundred slots used, SHA-256 `70891b39...47e7`). The file, the tool's
listing and the reports with their CD rows spelled out
(`tools/toy_cdda_report.py`) are in
`docs/evidence/cdda-background-test-86/console-2026-11-27/`. The owner's
words: "ChuChu seemed okay, but KOF was rapidly going normal volume soft
volume every ~0.5 seconds." POD SpeedZone was not run.

Both launch records name this build, `0af5d9ce889c`, and both reports the
ring's kind 3 (ADPCM in the driver's spare memory): the main module. No
log of these runs names the K-UI.

| Slot | Game | In the report |
|---|---|---|
| 0 | ChuChu Rocket | 9 PLAY, 15 PAUSE; playing at the save. The ring at 0x16000, 8,128 frames. 9 key-ons; the ring began again once, for a ring run dry. No key dropped, no channel taken, no register put back, no guard lost. 60.0 s of music taken. 3,584 heartbeats, 1.77 ms each on average, the longest 3.50 ms; 3.05 visits and 2,955 bytes a heartbeat. Levels 0F1F and 0F0F; the mixers 0F0F and 0F1F (first 000F and 001F). AT 16,054, TM 50,041,901 |
| 1 | KOF Dream Match 1999 | 3 PLAY, 7 PAUSE; playing at the save; 3,765 positions asked of the play. The ring at 0x15000, 12,224 frames. **165 key-ons; the ring began again 162 times, each for a ring run dry.** Nothing else lost or taken. 45.6 s of music taken. 4,181 heartbeats, 1.43 ms each, the longest 2.10 ms; 2.27 visits and 1,924 bytes a heartbeat. The mixers 0F08 and 0F17. AT 20,825, TM 64,921,781. The card: 15,270 blocks, 452 read again, 513 by programmed transfers |

What it shows:

1. **The compressed ring in the driver's spare memory plays.** ChuChu
   Rocket had music for a minute with one start over, "okay" to the
   owner's ear: the chip decodes the codes and loops where the coder
   reckons (a loop a code long or short would click five times a second).
   Neither game lost a guard line: the spare memory stayed the ring's.
2. **KOF's ring starves, by the reader's own rule.** KOF's SDK is of the
   early family, for which the audio's card work has half the reader's
   soft window, 1.5 ms a heartbeat (`kui_toy_cdda_card_window`, after the
   Background reader's rule for its optional work under those drivers). A
   visit hands on a kilobyte at most and took 0.46 ms, so two fitted, at
   times three: 1,924 bytes a heartbeat, where a sixtieth of a second
   plays 2,940. The tenth of a second primed at key-on lasts a third of a
   second; the ring runs dry and begins again: a seek on the card, the
   priming, the key-on. Twice a second, as the owner heard it. ChuChu
   Rocket's family has the whole window, and took 3.05 visits a heartbeat:
   exactly what it played.
3. **What a kilobyte costs: 0.41 ms in ChuChu Rocket, 0.46 ms in KOF**, a
   visit with the bytes' way into the ring. Most of that is not the card
   and not the sound chip. The worker's data, its cleared state and its
   stack are linked at uncached addresses (0xAC10BDA0 to 0xAC112000 in
   this module), and the path of the frames into the ring (`feed`,
   `store`, `put`, `turn`, `drain`, as compiled) lives on them. Run as
   linked on the repository's SH-4 interpreter (`tests/sh4_mini.py`), a
   kilobyte of frames takes 57,710 instructions and 4,879 reads and
   writes of uncached memory, each a cycle on the memory bus: 2,862 of
   the stack, 993 of the output's state, 1,024 of the bytes themselves,
   for 64 words stored in sound memory.
4. **TMU0 runs a quarter of a percent slow against the sound chip.** The
   drivers' count of 4 ms of the chip's time against TMU0's ticks: 2,528
   parts in a million fast in ChuChu Rocket over 64 s (give or take 125),
   2,402 in KOF over 83 s (give or take 96); together 2,449, give or take
   76. The count is exact by the drivers' code: the timer interrupt
   reloads its counter for 44 samples, leaves out every 441st and counts
   every fourth, 250 to a second of 44,100 samples, and nothing else of
   the driver's holds that interrupt off but its start. (A processor clock
   of 199.5 MHz where 200 is reckoned would be 2,500.) The ring's margin
   allowed 200. A play reckoned from TMU0 falls behind the chip by 108
   frames a second, so the chip passes the writer when a play has run
   unbroken for about as many seconds as the lead has hundreds of frames:
   a minute at the most here. ChuChu Rocket's plays were shorter (9
   key-ons in 64 s); Test 85's in POD too.
5. **A stream newly begun wastes its first heartbeats.** Blocks read again
   for each stream begun: 1.7 in ChuChu Rocket, 2.8 in KOF (3.5 in POD
   under Test 85). Each ends the stream and costs a programmed read and a
   new start. In steady play it is rare (17 in 20,665 blocks); behind
   every start over it lengthens the gap.
6. **The mixers read back under MANATEE 2 too**: 0F0F and 0F1F in ChuChu
   Rocket, the driver's full level, the first register to the right and
   the second to the left. KOF gives 0F08 and 0F17: full level, each side
   half way out. The output plays both planes hard left and right.
7. **The heartbeat's fixed part**, the reads that prove the driver, the
   channels and the guards: 0.51 ms in ChuChu Rocket and 0.43 ms in KOF
   (the heartbeats' time less the visits'), where Test 85 had 0.9 ms with
   the table's reads.

Decided with it (Test 87): the ring's clock is the driver's count; the
audio's way into the ring is one loop that keeps its state in registers;
the audio has the whole window while its ring runs low.

Also read for this record, in the three admitted builds: the start again
on the game's command keys every channel off before it clears memory, as
the start-up does (image 0x83A4 in MANATEE 2.00.09, 0x601C in 1.01.3b), so
a cleared ring is not heard there either (Known limits, 7).

## Known limits

1. Not run on a console. Everything about the chip here is a reading of
   documents, emulators and the drivers' code: that PCMS 3 runs its
   decoder on across the loop, the decoder's arithmetic to the last bit,
   the loop's length. The host model decodes as those say.
2. 4-bit ADPCM is not the disc's 16 bits: the error is noise that follows
   the music's level. No listening test was made; none is possible here.
3. The ring is small: 184 ms (MANATEE 2) or 277 ms (MANATEE 1). While the
   game's own read holds the card longer than the ring's lead, the music
   stops and comes back at the drive's position. POD SpeedZone, which
   reads through a race, will probably do that more often than under Test
   85's 371 ms.
4. The lead shrinks through a play. The clocks' margin grows with the
   time played and comes off the ring's lead from both ends: of 8,128
   frames (184 ms), about 181 ms are lead at a ring's start and 91 ms
   before it starts over. That is the price of TMU0 as the clock.
5. The ring starts over for the margin every 223 s of unbroken play under
   MANATEE 2 and every 339 s under MANATEE 1: a gap of about a tenth of a
   second.
6. A ring that runs dry plays what lay there until the heartbeat that
   notices, a sixtieth of a second where the game calls that often: as
   ADPCM that is old codes behind a decoder they were not made for.
   Likely a click.
7. The driver's start clears the spare memory to zeros, which as ADPCM
   decode to a ramp. Its start-up keys all 64 channels off before it
   clears (read in the three builds: image 0x73C, MANATEE 1 0x650), so
   the cleared ring is not heard there. Its start again on the game's
   command keeps that order (Console result).
8. Which mixer register is the disc's left is not known: the planes are
   on fixed sides, at one level.
9. Three driver builds only. The standalone New reader's games (271 of
   the corpus's 426 CD audio games) are not reached.
10. The card's half is Test 85's, unchanged: its sources and its 189
    mutations were not touched or run again.

## Verification

At commit `0af5d9ce` on a clean checkout, `BUILD_ID=0af5d9ce889c`
(`docs/evidence/cdda-background-test-86/summary.txt` and the logs beside
it).

| Check | What ran | Result |
|---|---|---|
| The parts | `make toy-cdda-parts-check`: Test 83's recipe, Test 84's and this one without YIELD beside this one | Both low residents and the observing worker are Test 83's, byte for byte. The auxiliary keeps its span and stack top; its code ends at 0x8c4277e0 below the stack at 0xac427900, as Test 85's, whose bytes it is. The worker's player descriptor names the shared block in its fourth word: 1,312 bytes at 0xac110a40 in the cleared state |
| As linked | `make toy-wire-linked-check`: Test 77's linked checks, the auxiliary's stack bound, and `tests/test_toy_cdda_card_linked.py` on the worker | Passed. The auxiliary's deepest use is 1,052 bytes with the gate, of 1,776 usable (1.69 x). The worker: the consumer is the output's gate; the output behind it is 344 bytes deep, the card's heartbeat, the visit's gate and the bytes' way into the ring among them, and is called only with 768 bytes of the private stack below it: 704 clear of the guard words (2.05 x). Called from the completion only, at 2 sites, each after `stack_valid`. The check refuses 18 altered readings of itself |
| The sound output | `make toy-cdda-output-host-test`: `tests/test_toy_cdda_output.c` built five ways (VOLUME, ADPCM, SPARE: 111 as delivered, 011, 110, 100, 000). The model is Test 85's, with a chip that keeps each channel's own position from its key-on, goes back to the loop's start by the loop end register as it stands at that step, and decodes ADPCM from signal 0 and step 127 at each key-on. Every code played is held against the one the test's own coder gives the play's frame, every code stored against a decoder run over all the codes stored, and what the chip decodes against the play's audio. With SPARE the model takes stores to exactly the family's span | As delivered 891,655,900 checks; without VOLUME 885,383,321; with ADPCM in a place of the table's 1,288,831,802; with samples 784,700,908 and 779,987,287. As delivered: a play on each of the three builds in the spare memory (at 0x16000, 0x15000 and 0x15000), 1.6 million frames each, every one the play's, through pause, release and the end. Tracks, a gap, a refused track and laps. The chip's clock 150 parts in a million fast and slow over five minutes: every frame the play's, the ring begun again once, and the two clocks' counts for the report true to one count at either end. Nothing written for another driver, one not running, a held sound CPU, unproved records, a used channel, no clock, a G2 transfer, SR.BL. The ring given up when a guard says so and taken again in the same play, eight times; the driver started again. A taken channel left alone; a dropped key; changed registers put back and the ring begun again where the play was moved. A slow card, a card that hands nothing on, a bus that does not answer. Both planes at the larger of the mixers' two levels, each on its side, POD's two words among them; MANATEE 2's bytes; the terminal key-off with the levels kept |
| The audio from the card | `make toy-cdda-card-host-test` | 90,240,164 checks, as under Test 85: its sources are unchanged |
| The whole reader | `make toy-wire-integration-host-test` | 460,586,482 checks. A play beside 5 reads: function 4 answered 3 all 15 times |
| Asked for nothing | `make toy-cdda-card-quiet-check` | Folds to `f8565e87` with the audio's card work built in and without |
| The call sites | The gesture test (`toy-survey-mixed-pad-host-test`) | 274 fast and 277 mixed completions each called the consumer once |
| The drive | `tests/test_toy_cdda_drive.c` | 12 scenarios, 11,704 checks, as under Test 84 |
| Tests notice defects | `tests/test_toy_cdda_output_mutations.py` in three ways (`--way 111`, `110`, `100`); Test 84's drive mutations | 137 of 137 caught as delivered, 139 of 139 with ADPCM in a place of the table's. With samples 125 of 126 in the full run: the one left, "a byte gathered twice", could not change that build as written (it took a byte again at every sixteenth gathered, and a unit of samples is eight); written to hold in both (`1c581014`) and run by itself, it is caught in all three ways. 41 of 41 of the drive's. The card's 189 were not run again: its sources are unchanged |
| The report | `tests/test_toy_wire_report.c` | 577 bytes with nothing counted, 731 with the audio's fields set, 999 of 1,012 with every other row at eight digits a value; the audio's rows all written, no row cut |
| The card | Test 83's save and survey-file tests with this recipe | Passed on FAT32 and exFAT, the volume at sector 0 and 63; `tools/toy_survey.py`'s blank file is the stage's |
| Card Tools admission | `make toy-wire-package-test` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The launcher's admission helpers, compiled against the K-UI's sources, on the packaged bytes of the three modules (`games-admission.log`) | As Test 85's modules, line for line but for offsets: envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New and the Legacy match, the 160-place map, the toggle, the mode records, a blank cache seed slot, the heap-tail and SDK-proof capabilities |
| The owner's K-UI | `python3 tools/check_gui_lines.py --installed-has 4d4dcff77494` | The installed K-UI `aaaba0ea3640` contains the 160-place map's line; so does `53a33ec708ab` |
| Main | `git fetch origin main` and the Windows CE line before packaging | Both at `3f757bc2`, contained in this branch |
| Reproducible | The delivered recipe built twice from nothing | Module SHA-256 `4c5cf34c...9e23` both times |
| The module at full level | The recipe with `TOY_CDDA_VOLUME=0`: its linked, parts and package checks, its report and gesture tests | SHA-256 `9b0467c4...b0bc`. Its auxiliary and both low residents are the delivered module's bytes. The output is 348 bytes deep behind the same gate (2.02 x). 324,164 bytes; 1,024 bytes of the worker left |
| The module without the sound | The recipe with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0` and neither new switch: its linked, parts and package checks, its report, gesture, survey and audio tests | SHA-256 `cb4c31e8...c74e`. Its auxiliary and both low residents are the delivered module's bytes. The consumer that drops the bytes is 104 bytes deep and stays 88 clear of the guard words on the least stack (1.85 x). Its report says TEST 85: 457 bytes with nothing counted, 971 of 1,012 at the widest. 319,396 bytes; 5,152 bytes of the worker left. Built with Test 85's label it is Test 85's delivered no-sound module byte for byte (`1bd6e044...b411`) |
| Test 85's way | The recipe with samples in a place of the table's (`TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`): built, its linked check and its report tests | Passed; 324,676 bytes, 576 bytes of the worker left. Not packaged |
| The recipes before Test 85 | Test 84's, Test 83's and the installed New reader's, built at this commit and at `f4cc67dd` | The same bytes at both: `68f9f3de...f22d`, `30cac0f8...507a` and `e69a6fef...93ac` |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 10 of 10 pass |

Read in the owner's driver files, on this machine only (none of their
bytes is in the repository): the stream port's key word, format and loop
end in MANATEE 2.00.09; the stacks, the tables' highest addresses, every
use of 0x18000 and the start-up's order (every channel keyed off, then
the clear) in the three admitted builds.

Not checked by any test here, because only a console has it: whether the
chip decodes as the model does and loops where it does; whether the spare
memory is left alone in play; how often the small ring runs dry; whether
the mixers read back in ChuChu Rocket and KOF; what the two clocks differ
by.

Host and linked checks are not console tests.

## Package

One archive, prepared 2026-11-27 16:06 CST (America/Chicago), from the clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Music-Test-86-27NOV2026-1606-CST-0af5d9c.zip` | 583,496 | `1123c5f880d75c46fbbdeda96e6e0f74133035c5081c8ced7a951fd2ee064139` |

| In the archive | What | Bytes | Payload CRC32 | SHA-256 |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | the delivered module: Test 85's switches with ADPCM and SPARE, no survey | 324,420 | `789692C6` | `4c5cf34c3f29675c2932a33b2819f9abdc32cefa8d6e66a061a9d59f0ef89e23` |
| `KUI/SURVEY/STREAM.TXT` | a new blank report file, 100 slots, ID `A7E2DEC4944B4B399F8B3C119C906300` | 154,112 | | `49cc8f7cbfe1b94c5572b281d6843fcf79cdfda65becc1c9d3661f0065ae4e3b` |
| `IF-NEEDED/1-full-volume/KUI/apps/games/retail-native-dual.kui` | the same with `TOY_CDDA_VOLUME=0`: for a game that runs but stays silent | 324,164 | `F6205516` | `9b0467c494a650bf47184aa0b92bbf682966407ec47c58535a647abebacdb0bc` |
| `IF-NEEDED/2-no-sound/KUI/apps/games/retail-native-dual.kui` | the same without the output and with Test 84's survey: for a game that misbehaves; it reads the audio and drops it, and its report says TEST 85 | 319,396 | `B8CFB1A1` | `cb4c31e89c9a7d2f96569921b0625d6f236b210037d4b9b7c613a8039e8cc74e` |

The archive also holds `README.txt`, `INVENTORY.json` (the files with
their full hashes, what stays on the card and what the module needs of
K-UI) and `SHA256SUMS.txt`; they are in the evidence folder as
`package-README.txt`, `package-INVENTORY.json` and
`package-SHA256SUMS.txt`, beside the three modules
(`retail-toy-test-86.kui`, `-full-volume.kui`, `-no-sound.kui`). The
modules replace `KUI/apps/games/retail-native-dual.kui` for the test only;
no K-UI, updater package, activation record, bank, game data or save data
is in the archive. The module needs the 160-place map of the owner's K-UI:
the README has the owner check the Diagnostics page's first line
(`aaaba0ea3640` or `53a33ec708ab`) before the first launch.
