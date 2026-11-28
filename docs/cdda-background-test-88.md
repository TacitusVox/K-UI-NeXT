# Test 88: CD music in the Background reader, part 5: every MANATEE build

Status: packaged 27 November 2026, 22:49 CST (build `9e105c4e8d25`, commit `9e105c4e`),
and run on the console the same night in six games (Console result).
Music played without a ring begun again in two builds new to the console,
MANATEE 2.02.02 (Mr. Driller) and 1.01.40 (Tetsuman Menkyokaiden), and in
the control (KOF Dream Match 1999, 1.01.3b); Bangai-O was silent with
reason 4, detail 1, as built. Nasubi no Heya ran slowly, by the owner's
account, with 1.5 s of music in its report and nothing counted against
the output: its cause is open. No build of the two older families has run
yet. A test module in the place of the installed
New reader, like Tests 79 to 87, not an accepted component.

It is step 3 of `universal-loader-roadmap.md` (CD music in regular games:
what is left), asked for by the owner on 27 November 2026 after Test 87:
"carry our work further and do an ALL MANATEE build".

## Baseline

Test 87 (`cdda-background-test-87.md`, build `a54a75c8645b`, SHA-256
`0ccfaa60...2094`), run on the console on 27 November 2026 in KOF Dream
Match 1999, ChuChu Rocket and POD SpeedZone. Its recipe built on this
machine at its commit gives the delivered file byte for byte.

The recipe is Test 87's, switch for switch. It is not Test 87's bytes at
this commit: the output's admission and its records change. Step 2 of the
roadmap (Test 88 as first proposed: the player's rough edges) is not in
this build; nothing of the player's timing, its window or its ring changes.
For the three builds Test 87 played, the output decides and writes what it
did.

## Why

Test 87's output is given to three sound-driver builds, each named by its
version bytes and its build date: MANATEE 2.00.09, 1.01.3b and 1.01.37.
The 426 regular games with CD music of the owner's corpus carry 27
MANATEE-lineage builds. Of the 82 games the Background worker takes, 23
carry one of the three; 56 carry MANATEE alone and 16 MANATEE beside
AUDIO64 (`cd-audio-corpus.md`).

What the output takes from a driver was read in the three builds only:
the word its timer interrupt counts, the end of its own memory that it
does not use, and the records that say whether two channels are free.

## The reading: 27 builds, each run

The owner's sound-driver archive of 20 November (178 files, SHA-256
`b35152da...3b0d`; all 28 MANATEE-lineage images of the corpus's list are
in it, one of them a copy damaged by a newline conversion) was read on
this machine. None of its bytes is in the repository.

`tools/manatee_read.py` does not only read an image. It loads it at 0 as
the sd library does, starts it at its reset vector on an ARM7 model
(Unicorn) with the sound chip's registers at 0x00800000, lets the start-up
run into the driver's idle loop, and then raises 3,528 timer interrupts
(eight times 441) the way the chip does: FIQ, with level 2 in register
0x2D00. Its log, one line an image with the full SHA-256, is
`docs/evidence/cdda-background-test-88/manatee-read.log`.

| Family | Builds read | The count's word | Frames a count | Start-up clears | Highest address named below 0x18000 | Spare memory |
|---|---|---|---|---|---|---|
| MANATEE 2 | 15: 2.00.06, .08, .09, .0b, 2.01.00, 2.01.02 (two dates), 2.02.02, .04, .05, .06, .07, .08, 2.50.01, 2.50.04 | 0x12418 | 176.4 | 0x12000..0x16FFF | 0x15000, the host's MIDI ring, indexed under 0xFFF | 0x16000..0x17FFF |
| MANATEE 1 from 1.01.37 | 6: 1.01.37, 1.01.38 (two dates), .3b, .3f, .40 | 0x13418 | 176.4 | 0x13000..0x17FFF | 0x14800, the command history, indexed under 0x7FF | 0x15000..0x17FFF |
| MANATEE 1 before 1.01.37 | 4: 1.01.1d, .28, .32, .36 | 0x13418 | 176 in 1.01.1d and 1.01.28; 176.4 in 1.01.32 and 1.01.36 | 0x13000..0x17FFF | 0x14800, under 0x7FF | 0x15000..0x17FFF |
| NAMIHEI | 2: 1.00.0d, 1.00.12 | 0x13418 | 176.4 | 0x13000..0x17FFF | 0x14800, under 0x7FF | 0x15000..0x17FFF |

What the run shows in every one of the 27:

1. **The count.** The timer interrupt reloads the chip's timer with 0xD4
   (44 frames), acknowledges it, and counts into one word of sound memory
   and no other: 0x12418 under MANATEE 2, 0x13418 under every other
   family, NAMIHEI among them. In 25 builds 3,528 interrupts move it by
   880: every 441st is left out and every fourth counted, 882 frames in
   five counts, 250 counts to a second exactly.
2. **Two builds count otherwise.** 1.01.1d (1998.08.31) and 1.01.28
   (1998.09.22) leave no interrupt out: 3,528 move the count by 882, 176
   frames a count. Taken by the later measure, their count would put the
   chip a quarter of a percent ahead of where it is, and the writer would
   meet it within seconds of a play. From 1.01.32 (1998.10.14) on every
   build leaves the 441st out. The corpus has no build between 1.01.28
   and 1.01.32.
3. **The running mark.** After its start-up a MANATEE 2 build has 'EXEC'
   at 0xF8 and every other family 'SEGA', but 1.01.1d, which writes no
   mark.
4. **The records.** MANATEE 2's start-up leaves the split 48/8/8 at
   0x12060 and the stream ports' register base 0x00801C00 at 0x12068.
   MANATEE 1's leaves the pool of 16 at 0x12080 with each entry's
   register base, 0x00801800 + 0x80 x entry, in its third word; so do the
   four builds before 1.01.37. NAMIHEI's leaves sixteen port records of
   0x40 bytes from 0x11000: the last eight, from 0x11200, are the stream
   ports', each with its channel's register base, 0x00801C00 + 0x80 x
   port, in its third word.
5. **The spare memory.** Every word of the image and every immediate of a
   data-processing instruction was looked at: no build can name an address
   from the family's spare memory up to 0x17FFF. The highest ring is
   indexed under a mask that ends where the spare memory begins. 0x18000
   is the word each build gives the game as the start of free memory
   (0x12408, or 0x13408). After the start-up the run touched nothing from
   the spare memory's start to 0x17FFF.
6. **What holds the interrupt off.** The start-up and the start again on
   the game's command, in every build; from 2.02.02 on also a command of
   two stores that sets the interrupt enables (timer A stays enabled
   either way).

The code around the command history is the same in all 27 builds but for
the address of its index word, and the code around the MIDI ring the same
in all 15 MANATEE 2 builds. The sixteen places a MANATEE 2 build uses its
stream-port records at 0x13800 are the same instructions in all 15 (one
command does not exist in 2.00.06; from 2.02.05 on one address is an
immediate where it was a literal). The six builds of MANATEE 1 from
1.01.37 use the pool at the same eight places; the four before differ in
the order of two loads.

Not read: the driver under a game's commands. The start-up, the idle loop
and the timer interrupt ran; no command of a host was sent. That a build
leaves the two channels alone in play is known from a console for three
builds (Test 84's survey), and for no build of the two older families.

## The change

Five commits on Test 87, one behaviour each. No new switch: the changes
are the sound output's wherever it is built.

| Commit | What it does |
|---|---|
| `6c021b11` | Output is given to the family, not to a build: MANATEE 2 (2.x under 'EXEC') and MANATEE 1 from 1.01.37 (1.01, build byte 0x37 or more, under 'SEGA'). The build admitted is still one build: a driver of the same version bytes and another date ends the play as another driver does |
| `84ef047e` | MANATEE 1 before 1.01.37: the pool's entries 14 and 15, channels 62 and 63. A count of 1.01.1d or 1.01.28 is taken for 176 frames. 1.01.1d is asked for no mark; builds 1.01.29 to 1.01.31, which were not read, are given no output |
| `25aefc75` | NAMIHEI (1.00 under 'SEGA'): stream ports 6 and 7, channels 62 and 63, each record proved by the register base in its third word, as a pool entry is |
| `c50469e4` | A split of MANATEE 2's 64 channels that is proved and has fewer than two stream ports (Bangai-O: none) is silent with reason 4, detail 1: no pair to take. It was reason 4 as records that prove nothing |
| `9e105c4e` | The sounding build is `retail-toy-test-88.kui`, and its report says TEST 88 |

What each family gives the output:

| Family | Known by | The two channels | Free while | Proved by | The ring |
|---|---|---|---|---|---|
| MANATEE 2 | 2.x, 'EXEC' | 62 and 63, the top two stream ports | their records' bit 0 (0x13800 + 0x40 x port) is clear | the split's bytes adding up to 64, 16 stream ports at most, the base word 0x12068 | 0x16000, 8,128 frames, 184 ms |
| MANATEE 1 from 1.01.37 | 1.01, build 0x37 or more, 'SEGA' | 61 and 62, pool entries 13 and 14 | the entry's owner word is 0 | the entry's register base | 0x15000, 12,224 frames, 277 ms |
| MANATEE 1 before 1.01.37 | 1.01, build 0x28 or less or 0x32 to 0x36, 'SEGA' (no mark asked before 0x28) | 62 and 63, pool entries 14 and 15 | the same | the same | the same |
| NAMIHEI | 1.00, 'SEGA' | 62 and 63, stream ports 6 and 7 | their records' bit 0 (0x11200 + 0x40 x port) is clear | the record's register base | the same |

The channels of the two older families are the ones Test 84's survey
reads for them. MANATEE 1's pool hands its entries out from the first, so
the last are the last it comes to; from 1.01.37 on every stop of all
sounds strobes the last entry's channel, 63, which is why those builds
have the two before it.

The worker has 192 bytes left below its stack (Test 87: 352), the
auxiliary 288 as before: its bytes are Test 87's.

## The report

STREAM.TXT is Test 83's format and the rows are Test 87's. The saved
report says TEST 88. Two readings are new:

| Row | What is new |
|---|---|
| `CD O ST V ID RING N` | The family in the first word can be 3 (MANATEE 1 before 1.01.37) or 4 (NAMIHEI). Reason 4 with detail 1: the driver's split leaves no two stream ports. The second word is the driver's version bytes and the third's low half its date's hash: together they name the build that ran |

`python3 -I tools/toy_cdda_report.py STREAM.TXT` prints every saved report
of a file with these rows spelled out, the version bytes as a version.

## Which games it reaches

Counts over the 82 games of the corpus the Background worker takes
(structural readings on the host, upper bounds, not console results):

| | Games |
|---|---:|
| Carry MANATEE alone, every driver file on record one of the 27 builds read | 44 |
| Carry MANATEE alone by their program's sound library, no driver file in the bundle run | 12 |
| Carry MANATEE and AUDIO64 (music sounds where MANATEE is the driver that runs) | 16 |
| Carry AUDIO64 alone (silent, reason 1) | 10 |

Under Test 87 music could sound in 23 of them. By family, the games of the
82 with a build of it on the disc: MANATEE 2 in 41 (other builds than
2.00.09 among them in 27), MANATEE 1 from 1.01.37 in 23, MANATEE 1 before
1.01.37 in 2 (Nasubi no Heya and Dreamcast Magazine GD Vol. 1, each beside
a later build), NAMIHEI in 2 (the two Pro Yakyu Team discs, each beside a
MANATEE 2 build). Which driver of a disc runs, the report says.

The 15 games that carry only a build before 1.01.37 and the two that carry
only NAMIHEI are not among the 82: the Background worker does not take
them. Their families are admitted for the reader's wider reach of the
roadmap's later steps, and for the discs that carry them beside another.

## Console test

One launch a game, three to five minutes where the game plays CD music,
the report saved while music plays. Background loading On - New (X), CD
music Off, Virtual VMU Off, a new blank STREAM.TXT, K-UI `aaaba0ea3640` or
`53a33ec708ab`.

Suggested, a game of each generation (all of them games the Background
worker takes; the README lists them in this order):

| Game | Driver on the disc | What it asks |
|---|---|---|
| KOF Dream Match 1999, or ChuChu Rocket | 1.01.3b, 2.00.09 | Unchanged: the control. One of the two is enough |
| Maken X | MANATEE 2.00.0b | Another build of MANATEE 2 |
| Mr. Driller, or Sega GT Homologation Special | MANATEE 2.00.08 | The same, the most common other build |
| Tetsuman Menkyokaiden | MANATEE 1.01.40 | Another build of MANATEE 1 from 1.01.37 |
| Bangai-O | MANATEE 2.00.08 and 2.00.09 | Silent by design: reason 4, detail 1 |
| Nasubi no Heya | 1.01.38 and 1.01.28 | Which of the two runs; if 1.01.28, the first run of the older family and of the 176-frame count |
| Pro Yakyu Team de Asobou Net | MANATEE 2.00.08 and NAMIHEI 1.00.12 | Which of the two runs; if NAMIHEI, its first run |

Questions:

1. Does music play in a game whose build was never on the console (the
   key-ons against the plays, UND, CLK)?
2. Are the two channels left alone in those builds (TK, DRP, the game's
   own sounds by ear)?
3. Does the spare memory stay the ring's (LOST's guard count, reason 5)?
4. Which driver runs on the discs that carry two (the version bytes and
   the date's hash in the report)?
5. Is a count of 176 frames the chip's clock in 1.01.28 (CLK 0, a piece
   of a minute or more with no ring begun again)?
6. Is Bangai-O silent with reason 4, detail 1, and otherwise as before?

| Outcome | What it says |
|---|---|
| Music, clean, in the games of MANATEE 2 and MANATEE 1 from 1.01.37 | The families' rules hold in builds the console had not seen. Next: the player's rough edges (the roadmap's step 2), then AUDIO64 or the installed reader |
| A game silent with reason 6 (a channel taken), or its own sounds wrong | That game uses the family's two channels: a build or a game for which the pair has to be chosen otherwise |
| A game silent with reason 4, detail 0 | That build's records are not where its family's are, or a split this reading does not know: the report's words say which |
| Reason 5 detail 3, or LOST counting | Something writes the spare memory in play in that build or game |
| CLK counting, or reason 8 detail 2 | That build's count is not the clock it was read to be |
| Music that drifts into crackle within seconds, UND counting (1.01.28) | The measure of its count is not 176 frames |
| A hang, a crash | The if-needed module (the card's reading without the sound) says whether the reading or the output is the cause |

## Console result

The owner's runs of 27 November 2026, with the file from the card,
`KUI/SURVEY/STREAM.TXT` (ID B0D310936D1B8EE970AA887B0B79452F, six of its
hundred slots used, SHA-256 `54725be0...7ee0`). The file, the tool's
listing and the reports with their CD rows spelled out
(`tools/toy_cdda_report.py`) are in
`docs/evidence/cdda-background-test-88/console-2026-11-27/`. The owner's
words: "So Nasubi no Heya ran really badly, was pretty slow. Pro Yakyu I
couldn't trigger the report. It showed a memory address on the top and
constantly changing numbers. Was strange", and then: "Other games ran
smoothly, though."

All six launch records name this build, `9e105c4e8d25`, and all six
reports say TEST 88. No log of these runs names the K-UI.

| Slot | Game | The driver the report names | In the report |
|---|---|---|---|
| 0 | KOF Dream Match 1999 (T3101N) | MANATEE 1.01.3b (`1C1F`), channels 61 and 62 | 4 PLAY, 8 PAUSE; playing at the save. **4 key-ons, no ring begun again**; no key dropped, no channel taken, no guard lost. 110.6 s of music. 6,584 heartbeats, 1.58 ms each, the longest 3.22 ms; 3.52 visits and 2,965 bytes a heartbeat. The least lead 60 ms. The mixers 0F08 and 0F17. The card: 37,043 blocks, 1,078 read again |
| 1 | Maken X (MK-51050) | none looked at | **0 PLAY**: the game asked for no CD music in this session. The drive idle, the output never begun, no bus used. Nothing learned of MANATEE 2.00.0b |
| 2 | Mr. Driller (T1402N) | **MANATEE 2.02.02** (`8AF6`), channels 62 and 63 | 6 PLAY, 6 PAUSE; playing at the save. **6 key-ons, no ring begun again**; no key dropped, no channel found keyed on or taken, no register put back, no guard lost. The ring at 0x16000. **134.6 s of music.** 8,052 heartbeats, 1.27 ms each, the longest 3.19 ms; 3.17 visits and 2,949 bytes a heartbeat. The least lead 106 ms. The mixers 0F00 and 0F00. AT 33,583, TM 104,686,222: TMU0 2,484 parts in a million slow, give or take 60. The card: 46,354 blocks, 25 read again |
| 3 | Tetsuman Menkyokaiden (T18701M) | **MANATEE 1.01.40** (`2CF9`), channels 61 and 62 | 2 PLAY, 1 PAUSE; playing at the save. **2 key-ons, no ring begun again**; nothing dropped, taken or lost. The ring at 0x15000. 35.5 s of music. 2,114 heartbeats, 1.23 ms each, the longest 3.40 ms. The mixers 0F17 and 0F08. TMU0 2,558 slow, give or take 205. The card: 12,227 blocks, 8 read again |
| 4 | Bangai-O (T40217N) | MANATEE 2.00.09 (`C46D`) | 7 PLAY, 6 PAUSE, 1 RELEASE. **Silent with reason 4, detail 1**: the split leaves no two stream ports. No ring placed, no store; 3,834 heartbeats of 0.04 ms each read the driver and its split |
| 5 | Nasubi no Heya (T14305M) | **MANATEE 1.01.38** of 1998.11.06 (`6EDE`), channels 61 and 62 | 1 PLAY (track 4, repeat for ever), 2 PAUSE; paused at the save. 1 key-on, no ring begun again; nothing dropped, taken or lost. **1.5 s of music**, 83 heartbeats a frame of video apart (the longest wait between two looks 16.6 ms), 1.39 ms each. The mixers 0D00 and 0D00. The reader: 159,821 calls, 10,462 blocks; 330 commands that are no reads met by a window |
| | A Pro Yakyu disc | | No slot: the file holds no launch record of it |

What it shows:

1. **Two builds new to the console play, each as its family's rule has
   it.** MANATEE 2.02.02 for 135 s over six plays and MANATEE 1.01.40 for
   36 s over two: no ring begun again for any reason, no key dropped, no
   channel taken, no guard lost, the count borne out by TMU0 throughout.
   The owner: the games ran smoothly.
2. **Mr. Driller's driver is one the corpus had no file for.** Its US disc
   is one of the 12 games that name MANATEE by their sound library alone;
   the report names the build that runs, 2.02.02 of 1999.11.24, by its
   version bytes and its date's hash.
3. **The control is Test 87's.** KOF: 3.52 visits and 2,965 bytes a
   heartbeat, 1.58 ms each, as under Test 87 (3.52, 2,966, 1.58).
4. **Bangai-O is silent as built**, with the reason that says why.
5. **Nasubi no Heya runs the later of its two drivers**, 1.01.38, not
   1.01.28: the older family and the count of 176 frames were not run.
   Its one play was keyed on and paused after 1.5 s with nothing counted
   against the output, and its heartbeats came a frame of video apart
   while the music ran. The report holds nothing that says why the game
   was slow: the sound output worked for a second and a half of the
   session, and after the pause it touches nothing. What ran for the
   whole session is the test module's drive that plays (Test 84's), the
   audio's card work while a play is on, and the Background reader
   itself; this game was run under none of the earlier tests. Open.
6. **Maken X asked for no CD music** where the owner played: 2.00.0b is
   still unrun.
7. **Pro Yakyu left no launch record.** The stage writes the record
   before the game starts, with the line SURVEY ON / REPORT SAVES TO
   SLOT; a launch through this module would have used slot 6. Eight
   entries of the owner's library carry that name: by the corpus's host
   reading K-UI gives two of them a Background reader with a proved
   mapping (HDR-0091, HDR-0173), one the standalone New reader, and
   could check the program of the other five for none. Which disc was
   launched, whether that line
   was shown, and what the screen with an address and changing numbers
   was, the file does not say.
8. The CD mixers once more unlike from game to game: 0F00 and 0F00 (Mr.
   Driller), 0F17 and 0F08 (Tetsuman, the mirror of KOF's), 0D00 and 0D00
   (Nasubi no Heya). For the roadmap's step 2.

The owner, afterwards: the Pro Yakyu disc was "Pro Yakyuu Team de Asobou
Net! (Japan) (Rev A)" (HDR-0091, one of the two with a proved mapping:
why it left no launch record stays open); Maken X was played "for a
while" with no music asked for ("perhaps it's like Omikron and only plays
music during key parts of the story?"); Nasubi no Heya's slowness is set
aside for now ("Not going to worry about the slow right now"). No other
game the Background worker takes carries Maken X's build, 2.00.0b.
Suggested in its place, for the MANATEE 2 builds still unrun: Quake III
Arena (2.02.04, beside AUDIO64), Sega GT Homologation Special (2.00.08)
and Jet Coaster Dream 2 (2.02.05, beside 2.00.09).

For Nasubi no Heya, when it is taken up, two launches that tell the three apart: with
the if-needed module (the drive that plays and the card's reading, no
sound), and with the owner's own New reader file (neither). Slow with
both: the Background reader and this game. Slow with the first only: the
drive that plays, or the audio's reading. Slow with neither: the sound
output.

## Known limits

1. Run on a console in six games by one owner (Console result); two
   builds new to it played. The other 22 builds are unrun.
2. The readings are of each driver's start-up, idle loop and timer
   interrupt on a model. No build was read under a game's commands beyond
   what Tests 84 to 87 did by hand in three.
3. No build of MANATEE 1 before 1.01.37 and no NAMIHEI build was surveyed
   or run on a console. Their two channels are the ones Test 84's survey
   reads, chosen by the static reading.
4. A rule admits builds that were not read: a MANATEE 2 build after
   2.50.04, a MANATEE 1 build after 1.01.40. None is in the owner's
   corpus. Builds 1.01.29 to 1.01.31 are refused.
5. Under the two older families' top channels the driver's own strobes
   may key the pair off, as MANATEE 2's do under ChuChu Rocket: counted as
   dropped keys, the ring begun again.
6. Everything in Test 87's Known limits stands: the small ring, the
   drive's own clock on TMU0, ADPCM for the disc's 16 bits, the planes on
   fixed sides, a click where a ring runs dry.
7. AUDIO64 games stay silent (reason 1), and a game outside the
   Background worker's reach is not reached.

## Verification

At commit `9e105c4e` (the module) on a clean checkout, `BUILD_ID=9e105c4e8d25`;
the checks ran on a clean worktree of `6740125c`, which changes tests,
host tools and the suite's test targets only and builds the same module
bytes, and the output's model once more at `6948da60`, tests only
(`docs/evidence/cdda-background-test-88/summary.txt` and the logs beside
it). The compiler was built on this machine from the pinned sources (GCC
15.2.0, binutils 2.45.1, newlib 4.6.0.20260123): Test 87's recipe at its
commit gives Test 87's delivered module, `0ccfaa60...2094`.

| Check | What ran | Result |
|---|---|---|
| The drivers read | `make toy-cdda-manatee-read` (`tools/manatee_read.py`) over the owner's 28 MANATEE-lineage images | 27 run, each meeting what the output takes from its family: 15 MANATEE 2, 6 MANATEE 1 from 1.01.37, 4 before it, 2 NAMIHEI. The damaged copy is not run (`manatee-read.log`) |
| The output on the original drivers | `make toy-cdda-output-driver-check`: the output as delivered over sound memory as each image's own start-up leaves it, nothing authored; its family held against Test 84's static reading, the count's word and measure against what the image's timer interrupt writes, the ring against the first address above the image's highest ring; then two minutes of play | 27 of 27: the family, the two channels, the ring from the spare memory's start to 0x17FFF, one key-on, no ring begun again, the frames played reckoned within 150 to 162 frames of the model chip's (a count is 177). With a planted defect it fails: the first builds' measure wrong (1 image), MANATEE 1's ring 0x800 lower (12), MANATEE 1 from 1.01.37 on channel 63 (6) (`output-drivers.log`) |
| The survey on the original drivers | `make toy-cdda-survey-driver-check`, Test 84's, unchanged | 60 images, as under Test 84 |
| The sound output | `make toy-cdda-output-host-test`: `tests/test_toy_cdda_output.c` built five ways. New in the model: a driver of each of the 24 builds beside the three of the console, by its version bytes and date hash; the older MANATEE 1's pool entries and the first builds' count of 176 frames; NAMIHEI's port records | At `6948da60`, as delivered 1,575,830,144 checks; without VOLUME 1,564,586,800; with ADPCM in a place of the table's 2,234,176,441; with samples 1,521,896,050 and 1,513,952,549. Every one of the 24 builds plays on its family's channels and in its family's spare memory, every frame the play's: MANATEE 2 and the later MANATEE 1 for a second and a half, the older builds for half a minute, NAMIHEI and a later build held to the later measure for two and a half minutes. Refused, nothing written: 17 sets of version bytes and marks of no family (3.x, 0.x, 1.02, AUDIO64's 1.50, a fourth byte, each family under another's mark or none where it leaves one, builds 1.01.29, .2d and .31). A driver of the same version and another date, or the same date and another version, ends the play as another driver. Older MANATEE 1: entry 14 or 15 owned, silent with reason 6; entry 15's base wrong, reason 4; entry 13 owned, music. NAMIHEI: port 6 or 7 playing, reason 6; a record that does not name its channel, reason 4; port 5 playing or the pool's words set, music. A split of no or one stream port: reason 4, detail 1; of 17, or not adding up to 64, or with a wrong base: detail 0; of two: music on 62 and 63. And everything Test 87's model held |
| Tests notice defects | `tests/test_toy_cdda_output_mutations.py --way 111`, the delivered build: 195 defects, 45 of them of the families' rules | At `64689590`: 194 of 195 caught. The one left alive, NAMIHEI's count taken by the first builds' measure, puts the output's reckoning behind the chip, which then needs a ring's length, two minutes, to pass the writer; the model played NAMIHEI for half a minute. At `6948da60` (tests only) it plays two and a half, and that defect and its neighbour are caught. The runs with the ring in a place of the table's (`--way 110`, `--way 100`), which this test does not deliver, were not made (`mutation-check.log`) |
| The parts | `make toy-cdda-parts-check` | Both low residents and the observing worker are Test 83's, byte for byte. The auxiliary keeps its span and stack top; its code ends at 0x8c4277e0 below the stack at 0xac427900, as Test 87's, whose bytes it is. The worker's player descriptor names the shared block in its fourth word: 1,312 bytes at 0xac110ca0 in the cleared state |
| As linked | `make toy-wire-linked-check` | Passed, with Test 87's figures: the auxiliary's deepest use 1,052 bytes of 1,776 (1.69 x); the output behind its gate 300 bytes deep, called only with 768 bytes of the private stack below it, 704 clear of the guard words (2.35 x); the hand-written coder as before; a kilobyte of frames 32,947 instructions and 1,145 uses of uncached memory. The frames' check knows the output's new state word (the admitted build's date hash) |
| The window, the audio from the card, the whole reader, the call sites, the drive | `make toy-cdda-window-host-test`, `toy-cdda-card-host-test`, `toy-wire-integration-host-test`, `toy-survey-mixed-pad-host-test`, `toy-cdda-drive-host-test`, `toy-cdda-card-quiet-check` | 278 checks; 90,240,164; 460,586,482; 274 fast and 277 mixed completions each called the consumer once; 12 scenarios, 11,704 checks; folds to `f8565e87`: all as under Test 87 |
| The report | `tests/test_toy_wire_report.c` | Says TEST 88. 577 bytes with nothing counted, 731 with the audio's fields set, 999 of 1,012 at the widest; no row cut |
| The card | Test 83's save and survey-file tests with this recipe | Passed; `tools/toy_survey.py`'s blank file is the stage's |
| Card Tools admission, and the launcher's byte checks | `make toy-wire-package-test` on the packaged bytes of both modules, and the header-only capability readers of `include/kui` (`games-admission.log`) | Admitted, one blank report seed. As Test 87's delivered modules, line for line but for offsets and sizes: envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New reader's match, the toggle, the mode record, a blank cache seed slot, the 160-place map, the heap-tail and SDK-proof capabilities |
| The owner's K-UI | `python3 tools/check_gui_lines.py --installed-has 4d4dcff77494`, from the clean checkout | The installed K-UI `aaaba0ea3640` contains the 160-place map's line; so does `53a33ec708ab` |
| Main | `git fetch origin main` and the Windows CE line before packaging | Both at `3f757bc2`, contained in this branch; the regular games' line to `7dc84a6b` is contained |
| Reproducible | The delivered recipe built twice from nothing at `6740125c`, and once at `9e105c4e` | Module SHA-256 `7f14af33...7cb8` each time |
| The module without the sound | The recipe with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0` and neither of Test 86's switches: its linked, parts and package checks, its report, gesture, survey and audio tests | SHA-256 `30650218...37c9`, 319,396 bytes, 5,152 bytes of the worker left. Its auxiliary and both low residents are the delivered module's bytes. Built with Test 87's label it is the no-sound module Test 87 delivered, byte for byte (`2106e418...0445`): nothing of this test reaches a build without the output |
| Test 85's way | The recipe with samples in a place of the table's (`TOY_CDDA_ADPCM=0 TOY_CDDA_SPARE=0`): built, its linked check and its report tests | Passed; 325,316 bytes; the output behind its gate 352 bytes deep (2.00 x). No byte of the worker is left: the cleared state ends where the stack's room begins. Not packaged |
| The recipes before Test 85 | Test 84's, Test 83's and the installed New reader's, built at this commit | The bytes recorded for them: `68f9f3de...f22d`, `30cac0f8...507a` and `e69a6fef...93ac` |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 10 of 10 pass |

Read and run in the owner's driver files, on this machine only (none of
their bytes is in the repository): every MANATEE-lineage image, as The
reading says.

Not checked by any test here, because only a console has it: whether a
game of a build new to the console leaves the two channels and the spare
memory alone in play; which driver runs on a disc that carries two; the
older families at all.

Host and linked checks are not console tests.

## Package

One archive, prepared 2026-11-27 22:49 CST (America/Chicago), from the clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Music-Test-88-27NOV2026-2249-CST-9e105c4.zip` | 390,338 | `a6ad91277c5f849d780842af58b98e694f2914e8b275a6b4f6d5e93e9c32c790` |

| In the archive | What | Bytes | Payload CRC32 | SHA-256 |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | the delivered module: Test 87's switches at this commit | 325,060 | `49AEAAE1` | `7f14af334f73a03e8cf1ee622792f91301fb5ceff96cfd5b94e52dea5b217cb8` |
| `KUI/SURVEY/STREAM.TXT` | a new blank report file, 100 slots, ID `B0D310936D1B8EE970AA887B0B79452F` | 154,112 | | `2b7f25b4fa349a9e79765f84b67d355ab1aef7ba04fe4b1448333eb1deb53872` |
| `IF-NEEDED/no-sound/KUI/apps/games/retail-native-dual.kui` | the same without the output and with Test 84's survey: for a game that misbehaves; it reads the audio and drops it, and its report says TEST 85 | 319,396 | `AA84D60F` | `30650218d3ab6ea84ef1fa4d4c1aed96e8f61aaf0cd9736e4496d29e26a337c9` |

The archive also holds `README.txt`, `INVENTORY.json` (the files with
their full hashes, what stays on the card and what the module needs of
K-UI) and `SHA256SUMS.txt`; they are in the evidence folder as
`package-README.txt`, `package-INVENTORY.json` and
`package-SHA256SUMS.txt`, beside the two modules
(`retail-toy-test-88.kui`, `retail-toy-test-88-no-sound.kui`). The modules
replace `KUI/apps/games/retail-native-dual.kui` for the test only; no
K-UI, updater package, activation record, bank, game data or save data is
in the archive. The module needs the 160-place map of the owner's K-UI:
the README has the owner check the Diagnostics page's first line
(`aaaba0ea3640` or `53a33ec708ab`) before the first launch.

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
  BUILD_ID=9e105c4e8d25 BUILD=build/t88-main build/t88-main/retail-toy-test-88.kui
python3 tools/toy_survey.py blank STREAM.TXT --slots 100 --id B0D310936D1B8EE970AA887B0B79452F
```
