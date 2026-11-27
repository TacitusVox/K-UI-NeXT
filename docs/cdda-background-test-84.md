# Test 84: CD music in the Background reader, part 1: the drive plays

Status: packaged 21 November 2026, 01:13 CST (build `4abc63b0cc58`,
commit `4abc63b`). Run on the console on 21 November under the Windows CE
line's K-UI `1c71ba3afa17`, which refused three games, and again on 22 and
26 November under the one K-UI `64ae07435de5` (Console result). A test
module in the place of the installed New
reader, like Tests 79 to 83, not an accepted component. It makes no sound:
the drive answers as a drive that plays the game's audio tracks, and the
worker looks at the sound chip without writing to it. Test 85 is to add the
sound (`docs/universal-loader-roadmap.md`, Test 84).

It answers the owner's request of 20 November: "Use the attached Test 83
reports for the next background CD-audio build. Start from both Git
bundles in the source ZIP per its README: GUI 4d4dcff77494, reader
1d80f771f081. Preserve fragmented-disc support, cached launch speed,
standalone fallback, and L+R+ABXY reporting during mixed controller/VMU
polls." And Test 83's README promised it: "The long round over the CD
music games comes with my next build, which also records what the sound
hardware is doing, so that you do that round only once."

## Baseline

The owner's Test 83 reader line, `1d80f771f081` (fragmented discs, the
cached launch, the L+R+ABXY save in mixed controller and VMU polls),
integrated on this branch with the owner's GUI line `4d4dcff77494` (whose
routes include the standalone New reader's fallback for games whose SDK
pool overlaps the game; that module is not changed here). Its recipe (Test 83's switches,
`KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1`, label
`1d80f771f081`) builds the owner's module `313ac3bd...5f1c`. With every
switch of this test off, the recipe at this commit still builds it, byte
for byte.

## Why

The games' gdFs library (`docs/cd-audio-corpus.md`) reads the drive's
status and the positions it reports, and today's reader answers 1 or 2 and
a position that moves only with data reads. By the static reading of the
programs: MDK2 sends PLAY again whenever it reads anything but 3, Bangai-O
keeps its CD volume at 0 until it reads 3, Ready 2 Rumble ends its CD
stream at once, and a resume restarts the track when the position after
the pause equals the one after the play (no data read between them). A
player that makes sound must first be a drive that plays.

## The change

Five switches of `Makefile.vmu-native-toy-suite`, one behaviour each, each
on the one before it and on Test 83's recipe. Changes to shared sources sit
under `#if KUI_TOY_CDDA_*` with Main's text unchanged
(`make test-toy-test-switches`).

| Commit | Switch | What it does | Parts |
|---|---|---|---|
| `6fcf280` | `TOY_CDDA_ROOM` | Gives 2,304 bytes of the auxiliary's 4 KiB private stack to its code, at the same stack top (1,792 bytes of stack left); the report says TEST 84 | auxiliary, stage, worker |
| `94a4e98` | `TOY_CDDA_HOOK` | The auxiliary calls the worker's CD-music player after its guard on every visit it admits, and once more at the end of a delivery visit made from one of the game's GD calls. The stage finds the player through a descriptor in the worker and proves it lies in the worker, or stops the launch. The player does nothing yet | auxiliary, stage, worker |
| `d8bfb63` | `TOY_CDDA_DRIVE` | The drive plays: PLAY, PLAY2, PAUSE, RELEASE, STOP, SEEK and INIT move a play kept on TMU0; the status is 3 while playing, 1 paused or ended; a REQ_STAT or GETSCD is answered from the play's sector while it is pending; repeats, laps and ends. With no play nothing is written | worker |
| `e1fcf30` | `TOY_CDDA_KEEP` | The status stays 3 while the game reads data during a play: the player publishes the read's progress at the visit's end as the low reader's report() is about to, with status 3 | worker |
| `5f44706` (review corrections `8894be0`, tests `4abc63b` and `88ec3d6`) | `TOY_CDDA_SURVEY` | While the drive plays, the worker's heartbeat reads the sound chip, and only reads it: the driver, the records of the two channels Test 85 would take, and under AUDIO64 the top 48 KiB of sound memory | worker |

Both low residents and the observing worker are Test 83's, byte for byte
(`make toy-cdda-parts-check`). The auxiliary, the native worker and the
stage are this test's.

### The drive (DRIVE, KEEP)

`src/loader/toy_cdda_player.c`. At each delivery visit before one of the
game's GD calls the player sees a command the service accepted by its new
token (for gdFs, at the EXEC it makes right after REQUEST).

| The game's call | The play | Status (function 4, REQ_STAT word 0) | Position (REQ_STAT words 1-2, GETSCD formats 0 and 1) |
|---|---|---|---|
| PLAY 20 {a, b, repeat} | tracks a to b as far as they are audio and follow on; repeat 15 for ever, n for n+1 passes | 3 from the EXEC that saw it | from the range's start, 75 sectors a second |
| PLAY2 21 {from, to, repeat} | from a FAD in an audio track, or in a gap before one that follows another, to `to` or the end of that run | 3 | as PLAY |
| PLAY or PLAY2 the player cannot place | refused, counted; a play that stood ends | 1, then the reader's own | the reader's |
| PAUSE 22 | stands at its sector | 1 | frozen |
| RELEASE 23 | goes on from where it stood | 3 | goes on |
| STOP, SEEK, INIT, the functions INIT and RESET, an ABORT of the play's request | ends; the reader's own answers again | the reader's | the reader's |
| a data read during a play | the music goes on by the clock; KEEP publishes the read's progress with 3 | 3 | the data position (a REQ_STAT or GETSCD is never pending during a read) |
| the end of the last pass | ends, at the range's last sector | 1 | the last sector |
| no PLAY ever | nothing is written | the reader's | the reader's |

The clock is TMU0, read only and trusted only under the Toy timer profile
(781,250 Hz): 31,250 ticks are three sectors exactly, so no sector is lost
to rounding over hours. A profile that stops holding ends the play in the
reader's own answers (counted LOST).

Without KEEP, the low reader's report() publishes the sectors its engine
has delivered after the visit and before the answer, and the service
writes status 1 whenever it adds sectors: through the real low reader,
auxiliary and service, 10 of 15 status calls made between the polls of a
play's data reads answered 1. With KEEP, 15 of 15 answered 3
(`tests/test_toy_wire_integration.c`, `cdda_reads_in_play`).

### The survey (SURVEY)

`src/loader/toy_cdda_survey.c`. Called after the worker's heartbeat work on
both completion paths (the split controller completion, after the
cooperative service and under its rule for the early SDK's drivers; the
mixed controller and VMU batch, after the gesture's sample), at most once a
sixtieth of a second, and only while the drive plays. Each surveying
heartbeat first reads TMU0's set-up again and makes no survey unless the
Toy timer profile still holds (counted).

The bus is used as the console-used pilot used it
(`src/loader/toy_pilot_bus.c`): nothing with SR.BL set; nothing while any
of the four G2 DMA channels is enabled or running; the interrupt mask at
15 throughout, the caller's SR given back exactly; the G2 FIFO empty
before every burst of at most eight reads; a word the sound CPU may be
writing read until two reads agree; the whole survey within 2 ms of TMU0
and 10,000 polls. The only hardware access the file has is a
`volatile const` read; it has no way to store to the chip.

What it reads, all in sound memory except the sound CPU's reset bit
(`0xa0702c00` bit 0):

| Driver (by its version bytes at 0x20 and state word at 0xF8) | Candidate channels and their records |
|---|---|
| MANATEE 2 (`02 ..`, 'EXEC') | the top two stream ports by the split bytes 0x12060..62 (which must add up to 64): records 0x13800 + 0x40 x port, bit 0 playing |
| MANATEE 1 from 1.01.37 (`01 01`, build byte 0x37 or more) | the channel pool's entries 13 and 14 (channels 61, 62): owner word 0x12080 + 12e + 4, proved by the entry's register base at +8 |
| MANATEE 1 before 1.01.37 | entries 14 and 15, the same records and proof |
| NAMIHEI (`01 00`, 'SEGA') | stream ports 6 and 7: records 0x11200 + 0x40 x port, bit 0 |
| AUDIO64 (`01 50` or `01 5A`) | the playing bytes of channels 62 and 63, in the table at header word 0xE0, proved by the ports' base word before it (0x00800000); the hybrids' (base 0x00801800) host ports 14 and 15 |
| anything else, or not running | the identity only |

Under AUDIO64 it also reads the window 0x1F4000..0x1FFFFF, 256 bytes a
survey (a pass of 48 KiB in 192 surveys): whether the first pass met a
non-zero word, and which KiB changed in a later pass.

The driver facts are static readings of the drivers
(`docs/cd-audio-corpus.md` and the notes it names), not console results.
Where a family's records can prove their own layout (the split bytes, the
pool's register bases, the AUDIO64 base word) a survey that finds no proof
counts as unproved and not as free.

## The report

STREAM.TXT is Test 83's format. The saved report says TEST 84; the pages'
heading stays Test 81's, and the launch record's first line Test 83's
(`SURVEY 83 BUILD 4abc63b0cc58`). Behind the rows X, SD and XC, in the place of
Test 83's rows BIG, K and P, which have served:

| Row | Values |
|---|---|
| `CD P P2 PA RE END LAP` | PLAYs, PLAY2s, PAUSEs, RELEASEs the game sent; plays that ended; laps played |
| `CD ST RD KEEP REF LOST PJ` | the play's state (1 playing, 2 paused, 3 ended, 4 clock lost) with its generation's low byte <<8, the repeat word's low byte <<16 and its first track <<24; data reads made while a play stood; read progress kept at 3; plays refused; plays the clock lost; positions answered from the play |
| `CD SV DT B N WIN SKIP` | the driver's version bytes; its family <<28 (1 MANATEE 2, 2 MANATEE 1 from 1.01.37, 3 older MANATEE 1, 4 NAMIHEI, 5 AUDIO64, 6 AUDIO64 hybrid, 7 the sound CPU in reset), identity changes (to 15) <<24, for the MANATEE families the state word's first byte <<16, and a 16-bit hash of its build date; surveys that found candidate 1 and 2 in use, 16 bits each; surveys that read the candidates <<16 and those whose records proved no layout; under AUDIO64 a non-zero word in the window's first pass <<31, passes (to 32,767) <<16, KiB changed; surveys not made for SR.BL <<24, for the timer profile <<16, for the bus's budget <<8, and for G2 DMA, 8 bits each (to 255) |

A report holds 583 bytes of text with nothing counted, of 1,012. With a
full session's values and every CD SV field at its widest it holds 979:
the first movie's player, the last row, is left out and every other row
is written. With every value of every row as wide as a word can be, 15 of
the 20 essential rows are written. No row is ever cut
(`tests/test_toy_wire_report.c`).

## Which games it reaches

With GUI `4d4dcff77494` and Virtual VMU Off, 150 of the corpus's 426 CD
audio games take the Background reader (89 with the mapping proved, 61
unseeded, left to the stage's discovery), and with it this module's player.
On the host none of the 61 passes the stage's first steps (re-run of 22
November, `console-rerun-2026-11-22/investigation.md`): they stop at the
reader's stop screen, as Plasma Sword did. 271 take
the standalone New reader, which has no lease, auxiliary or worker and is
not changed here; 5 are refused (`docs/cd-audio-corpus.md`, Routing).

On the owner's card, by the Test 83 launches: ChuChu Rocket has 16 of its
16 audio tracks mapped, Bangai-O 34 of 34, MDK2 none of its 28. MDK2's audio
files are on the card (an image opens only with every track's file,
`src/core/game_image.c`), but the menu maps the audio tracks only when every
track's pieces fit in the map's 160 places, and otherwise lists them without
their files (`src/apps/games_retail.c`, `map_tracks`): its audio is in more
pieces than fit. This build needs no audio data; Test 85's sound in MDK2
needs its audio files in fewer pieces.

## Console test

The long round promised with Test 83: the Background-routed CD music
games the owner has, one launch each, two for ChuChu Rocket and Bangai-O
(the second launch is the warm one). Virtual VMU Off, New (X), CD Music
Off, a new blank STREAM.TXT. In each: a few minutes where the game plays
CD music, a pause and a resume, a level's end with its jingle where there
is one, one load; the report saved while music would be playing.

Suggested, by driver: ChuChu Rocket and Bangai-O (MANATEE 2), MDK2 and
Toy Commander (AUDIO64), Omikron / The Nomad Soul (AUDIO64 and MANATEE 2 on
its European disc, the one in the corpus; one CD music track, and whether
the game plays it is not known), King of Fighters Dream Match 1999, POD
SpeedZone and Plasma Sword (MANATEE 1 from 1.01.37), Tokyo Xtreme Racer
(older MANATEE 1), and Evolution 2 to compare with Test 83. The owner keeps
a list of each launch's game and module (Known limits, 7); a hang is first
saved with the gesture, which the worker keeps for a game that stops
answering while it still polls its controller.

Questions:

1. Does every game still start, play, pause, resume, load and save its
   report as under Test 83 (for a game not run under Test 83, as with the
   owner's own module)? A game that waits for a piece of music to end now
   waits as long as the piece lasts, up to a whole track (expected, and
   noted per game).
2. CD P and CD ST: what each game asks for; in MDK2, does P stay small (no
   PLAY sent again and again)? Is KEEP non-zero where RD is?
3. CD SV: which driver each game runs; whether the candidates are ever in
   use (B against N); under AUDIO64 the window (WIN); and whether the
   survey runs at all (SKIP: SR.BL, the budget, G2 DMA).
4. Is play as smooth as under Test 83?

| Outcome | What it says |
|---|---|
| The rows CD P and CD ST count the game's commands, the game behaves | The drive's answers are what the game wants; Test 85 can add sound behind them |
| A game hangs or misbehaves with this module and not with the second archive's | Most likely the survey's reads disturb it; Test 85 must not read there either |
| A game misbehaves with both | What both add to Test 83: most likely the drive's answers (or KEEP), else the auxiliary's smaller stack or the hook; its CD rows and the moment say which. A game not run under Test 83 is also tried with the owner's own module |
| SKIP shows every survey refused for SR.BL | The heartbeat runs with SR.BL set in that game: the survey, and Test 85's output, need another place |
| N counts unproved surveys for a family | Its records are not where the static reading put them in that build |

## Console result

The owner's runs of 21 November 2026 (evening, America/Chicago), from the
Games menu, with the file from the card, `KUI/SURVEY/STREAM.TXT` (ID
B5829D27CAF13A94DEAFD593D1FDC00A, six of its hundred slots used, SHA-256
`a38ad034...07f5`), and three diagnostics logs. The file, the tool's
listing, the reports with their CD rows decoded, and the logs are in
`docs/evidence/cdda-background-test-84/console/`. No page was photographed.

The final reader in every launch that reached it was this module: the
logs load `/KUI/apps/games/retail-native-dual.kui`, package `4abc63b0cc58`,
315,680 payload bytes, and every slot's launch record names that build.
The K-UI that launched it was not the one it was built and admitted for:
the logs name build `1c71ba3afa17`, the Windows CE line's GUI of 21
November (`claude/great-darwin-yjmgfi`). That line contains Main
`4441e92` but neither the owner's Test 83 GUI line `4d4dcff77494` nor its
reader line `1d80f771f081` (`git merge-base --is-ancestor`), so it has
Main's 64-slot Background map, not the 160-slot map this module's
admission declares, and none of that GUI line's routing changes.

| | Game | The owner | In the files |
|---|---|---|---|
| 1 | ChuChu Rocket | "Worked fine" | Slots 0 (no report) and 1. 12 PLAY, 18 PAUSE, no RELEASE, 1 play ended; at the save track 5 playing, repeat 15 (for ever); 2 data reads during a play, 21 read steps kept at 3; 28 positions from the play. MANATEE 2.00.09, the one original image of that version and date (`11c89eaa2b20`); both candidate stream ports free in all 2,640 surveys; none unproved; no survey skipped |
| 2 | Bangai-O | Failed to launch | Refused by the GUI: "track 3 is in 49 pieces"; "Background map exceeds 64 slots" |
| 3 | MDK2 | Failed to launch | Refused by the GUI: "track 1 is in 28 pieces"; "Background map exceeds 64 slots" |
| 4 | Omikron / The Nomad Soul | "Fine"; its song (David Bowie) "plays halfway through the game" | No slot: it did not pass through this module's report stage. How the GUI routed it is not in the logs |
| 5 | Toy Commander | "Worked fine", but "dropped about 5 frames in the intro movie, again" | Slots 2 and 3. The first: 2 PAUSE, no PLAY. The second: 1 PLAY of track 14, once, and 4 PAUSE; 195 positions from the play. AUDIO64, the one original image of that version and date (`477ede3766c2`); channels 62 and 63 free in all 196 surveys; the top 48 KiB of sound memory all zero over the first full pass; none unproved; no survey skipped |
| 6 | King of Fighters Dream Match 1999 | "Worked fine" | Slot 4. 7 PLAY, 16 PAUSE; at the save track 6 playing, once; 3,420 positions from the play. MANATEE 1.01.3b, the one original image of that version and date (`906e3b8a1336`); pool entries 13 and 14 free in all 10,084 surveys; none unproved; no survey skipped |
| 7 | Tokyo Xtreme Racer | Failed to inspect: "Invalid track length" ("may be my mistake") | The image: a track file is empty or not a whole number of sectors (`KUI_GAME_FILE_SIZE`, `src/core/game_image.c`) |
| 8 | POD SpeedZone | Failed to launch | The module loaded and the map held (11 tracks, one piece each), then the GUI's check of the executable's first sector (LBA 548141, track 11) read 24 zero bytes where a sector's sync belongs: "Invalid or unsupported data sector" |
| 9 | Plasma Sword | Failed to launch | Refused by the GUI: 66 tracks, more than its 64 slots |
| 10 | Evolution 2 | "Worked fine I guess" | Slot 5. Every CD row 0, as expected |

What it shows:

1. The drive's answers were taken by the three games that played music.
   No game hung or refused a play; no play lost its clock; the data reads
   ChuChu Rocket made during music kept status 3. Whether the answers are
   right for each game's music is Test 85's question: there is no sound.
2. The survey ran on hardware in three driver families and never had to
   hold back: no survey skipped for SR.BL, the timer, the bus's budget or
   G2 DMA. Each driver is the one original image the host check named for
   that version and build date, and its records proved their layout in
   every survey. That confirms on the console, for these three builds, the
   start-up stores the host check could only author: MANATEE 2's EXEC mark
   (the state byte `45`) and stream split, MANATEE 1's pool and its
   register bases, AUDIO64's port base (family 5, not the hybrid).
3. The two channels a sounding test would take were free in every survey
   of all three games.
4. Four launches failed before the game: three by the running GUI's
   64-slot map (with the 160-slot map of the Test 83 GUI line, Bangai-O
   launched in Test 83 and MDK2 did too), one by an executable sector the
   GUI read as zeros (POD). Tokyo Xtreme Racer's image did not open.
   Omikron did not run through this module.
5. Not this test's: Toy Commander's intro movie still drops frames; the
   movie path is Test 83's.

### Re-run under the one K-UI `64ae07435de5` (22 November 2026)

The owner's runs of 22 November 2026 (after midnight, America/Chicago) with
K-UI `64ae07435de5` (the log's first line; `one-kui-both-threads.md`) and this
module, New (X), virtual VMU Off, CD music Off: the same `STREAM.TXT`, now 11
of 100 slots (slots 6 to 10 new; SHA-256 `1d7d04d3...e144`), the K-UI log of
POD SpeedZone's launch attempt and a photograph of Plasma Sword's stop screen
(transcribed). Everything is in `docs/evidence/cdda-background-test-84/console-rerun-2026-11-22/`.
The owner's words: "Plasma Sword hit an issue after boot, that's the picture.
Pod Speed Zone is still failing to boot, and Tokyo Xtreme Racer I re-converted
and it gets past the bootstrap and just hangs on a black screen. I got a few
more from that list."

The causes were worked out on the host by four independent investigations
and a cross-check, from the games' own programs and drivers in the owner's
CD-audio archive (whose executables match the launch records' CRCs) and from
the K-UI and module sources (`console-rerun-2026-11-22/investigation.md`).
They are host results; no console run has confirmed them.

| Slot | Game | On the console | Cause (host) |
|---|---|---|---|
| 6 | Bangai-O | Report saved. 9 PLAY, 6 PAUSE; track 28 playing (repeat 15) at the save; 3 data reads during a play, 12 kept at 3; 15 positions from the play. MANATEE 2.00.09 (ChuChu Rocket's driver); the candidates read in 0 surveys | The game sets its driver's split to 0 MIDI voices, 64 one-shot ports and 0 stream ports. With fewer than two stream ports there is no candidate pair, by design (`8894be0`); the survey run over its own driver with that split reproduces the console's row exactly |
| 7 | MDK2 | Report saved. 3 PLAY, 6 PAUSE; paused on track 17 at the save; 1,778 positions from the play. AUDIO64 1.90b.1; channel 62 in use in 57 and 63 in 74 of 1,377 surveys. MAPPED 0 | The game's own code puts its stream on channel 63 and hands its effects out round robin over 0 to 62. Its 28 audio files are in more than 90 pieces on the card |
| 8 | Plasma Sword | The reader's stop screen: allocator probe, PHASE 2, DETAIL 0xA, every SDK field 0 | Built with the first Katana libraries (Maple library pd 1.09, December 1998); the reader recognises pd 1.46 and later, and the first recognition step fails. The K-UI launches a game whose discovery fails unseeded (Main since 1 and 5 November, `34e0ddce`, `eddbb01f`); the 64-slot map used to refuse it before that |
| 9 | Omikron | Report saved; no CD command | Its program holds no play reference; its song may be streamed from data files |
| 10 | Tokyo Xtreme Racer | Black screen after the bootstrap; launch record only | Memory, not the drive (likely). The reader's 284 KiB lease is taken from the bottom of the game's 448 KiB SDK pool; the game's graphics library then asks that pool for 458,480 bytes, 167,872 remain, and the allocation fails unchecked, so no frame can be built (that last step is inferred). The installed New reader and Test 83's module take the same lease. The game sends no CD play before its logos. Its re-converted image is checked only as far as its executable |
| — | POD SpeedZone | Refused by K-UI: the executable's first sector (LBA 548141, track 11) reads 24 zero bytes, as on 21 November | The card's copy of the track 11 file holds zeros there. K-UI reads the right file at the right offset (64-bit, through FatFs); MDK2, Toy Commander and KOF read their executables at the same or larger offsets on the same card |

What it settles:

1. With the 160-slot map, Bangai-O, MDK2 and Omikron launch through this
   module (Omikron for the first time) and save their reports.
2. Plasma Sword and Tokyo Xtreme Racer need the Standard reader (Background
   loading Off) until the Background reader recognises the early libraries
   (Plasma Sword) or leaves the game room (Tokyo Xtreme Racer).
3. POD needs a good copy on the card; nothing in K-UI.
4. For Test 85: Bangai-O has no channel under the planned rule (every
   channel is a one-shot port, which its driver stops at a loop's end);
   MDK2 leaves no fixed pair free without a patch of the game, as the
   existing MDK2 adapter makes; ChuChu Rocket (MANATEE 2) and KOF Dream Match
   1999 (MANATEE 1) remain the titles whose channels were always free.

### Controls and POD SpeedZone (26 November 2026)

The owner's runs of 26 November 2026, K-UI `64ae07435de5` (each launch
report's `build`), brought as a handoff with the survey file (13 of 100
slots, slots 11 and 12 new; SHA-256 `4baa5e4d...1699`) and four launch
reports, sequences 651 to 654
(`docs/evidence/cdda-background-test-84/console-2026-11-26/`). They are the
two Background-Off controls the host reading asked for, and POD on a good
copy.

| Seq. | Game | Reader and module (launch report) | On the console (the owner) | In the files |
|---|---|---|---|---|
| 651 | POD SpeedZone | Background New (X), `retail-native-dual.kui` `4abc63b0cc58`, CRC `BAFAAF7C` | Works. The owner found the card copy differed from the PC reference, recopied it and reported a full match | Slot 11, report saved. Executable CRC `E41C2053` (the archive's). 8 of 8 audio tracks mapped. 3 PLAY, none refused or lost; track 7 playing, repeat endless, at the save; 596 data reads during a play, 1,567 kept at 3. MANATEE 1.01.37; pool entries 13 and 14 free in all 3,684 surveys; none unproved or skipped |
| 652 | Tokyo Xtreme Racer | the same | Black screen after the bootstrap, no visible card activity; every track matches the PC reference | Slot 12, launch record only; IP `B601BEB1` and executable `F129E73B` as slot 10 |
| 653 | Tokyo Xtreme Racer | Standard, `retail-physical.kui` `f8807cb16561`, CRC `6A85A797` | Boots | The same image, IP and executable CRCs as 652 |
| 654 | Plasma Sword | Standard, the same module | Boots | IP `BB964C80`, executable `0B9BD71C` (slot 8's) |

The launch reports record the preparation up to the handoff; that the two
games booted on Standard is the owner's observation, and gameplay and saves
were not part of it. CD music was Off and RAM patches off in all four.

What it settles:

1. POD's zero sector was the card's copy, as read on the host. On a good
   copy POD launches through this module, and it is a second MANATEE 1 title
   (1.01.37, KOF's is 1.01.3b) whose two pool entries stayed free. It is
   also the first title seen to read data all through a play (596 reads):
   the hard case for Test 85's card sharing, where ChuChu Rocket made 2 such
   reads and KOF none.
2. Tokyo Xtreme Racer boots on Standard and stays black under the
   Background reader on an image that matches the PC reference. That is the
   difference the host reading predicted (the reader's 284 KiB lease leaves
   the game 167,872 bytes of the 458,480 its graphics library asks its 448
   KiB pool for). The failed allocation itself was not traced on the
   console.
3. Plasma Sword boots on Standard. Its Background stop is the reader's SDK
   discovery, not the image.

Decided with these results (the handoff's last section): before K-UI offers
the combined Background worker, check that the game keeps enough of its pool;
when the reader's SDK check fails, say so before the launch and point to the
Standard reader, and say what that means for a player who asked for virtual
saves; and for the first audible build keep ChuChu Rocket and KOF Dream Match
1999 as the two driver cases, with POD as a further MANATEE 1 case.

The two K-UI checks were delivered on 26 November 2026 as K-UI
`d41a04f40f23` (`background-launch-checks.md`). On the console, 27 November:
Tokyo Xtreme Racer boots with Background loading on (through the reader-only
module, which records no survey); Plasma Sword still reached this module's
stop screen, because K-UI cannot hold its 9.07 MiB program to check it. K-UI
`53a33ec708ab` of 27 November checks such a program while it reads it, and
refused Plasma Sword on the console the same day. K-UI `aaaba0ea3640`
refuses every large program it cannot check. The audible build is Test 85.

## Known limits

1. No sound. The survey writes nothing; Test 85 is to write the chip.
2. Only Background-routed games get the player (above).
3. The player is a drive model: no seek time, no spin-up, and the audio
   runs on while the game reads data, as no real drive does. A play's
   position is only as true as TMU0 under the Toy profile.
4. GETSCD's byte 1 (the audio status) stays the reader's constant: gdFs
   never reads it.
5. The survey's facts about the drivers are static readings; its proofs
   say where they do not hold. Run over the 60 original driver images of
   the owner's corpus, it names each image's version and date from the
   image's own bytes and its family as the static reading does; 19 of the
   59 families rest on the image's bytes alone, the others also on a
   start-up store the check authors from the static reading (MANATEE 2's
   mark and split, NAMIHEI's copy of its mark, the AUDIO64 ports' base
   word). The console's DT values are the first observation of a running
   driver.
6. The report's pages still say TOY TEST 81, and the launch record SURVEY 83.
7. Both modules carry the build label `4abc63b0cc58`. A report the second
   saves has no row CD SV, but a launch that saves no report leaves the
   same launch record under either: the owner keeps a list of each launch's
   game and module.

## Verification

The modules are built from commit `4abc63b` (label `4abc63b0cc58`) on a
clean worktree, and every check below ran on that worktree with the pinned
toolchain (sh-elf GCC 15.2), the report test's fix at `88ec3d6`. Logs are in `docs/evidence/cdda-background-test-84/`. None of this
is a console result.

| Check | What ran | Result |
|---|---|---|
| The parts | `make toy-cdda-parts-check`: Test 83's recipe beside this one | Both low residents and the observing worker are Test 83's, byte for byte. The auxiliary keeps its span and stack top, its code ends at 0x8c427040 below the stack at 0xac427900; the worker's player descriptor is where the stage looks for it |
| As linked | `make toy-wire-linked-check`: Test 77's five linked checks, the auxiliary's stack bound with the worker's player charged to every indirect call, and the survey's linked check | Passed. The auxiliary's deepest use is 1,052 bytes with the gate, of 1,776 usable (1.69 x). The survey: three fixed frames (88, 20 and 4 bytes) that call only each other, 112 bytes deep; called by the completion only, twice (one a tail call after the completion's frame is popped), each after `stack_valid` on every path, the survey's address loaded for those calls only. On the least stack `stack_valid` admits it stays 80 bytes clear of the guard words (192 usable, 1.71 x); entered as the completion hook enters it, at the top of the 3,072-byte stack, its deepest point is 236 bytes down. Its source reaches the hardware only through three `volatile const` reads and SR. The check refuses 12 altered readings of itself |
| The drive | `tests/test_toy_cdda_drive.c` on the player, the service's own code and an engine model in the low reader's call order | 12 scenarios, 11,704 checks. With no play every answer and every service word is the twin service's without a player; through a play's reads (KEEP) status 3 and every other word the twin's |
| The drive through the real reader | `tests/test_toy_wire_integration.c` | The whole reader's 370,346,318 checks. A play beside 5 reads (270 sectors): function 4 answered 3 all 15 times. The player's hook: 3,325,746 visits handed on first, 1,257,722 ended with KEEP; the 2 the guard refused reached no player |
| The survey | `tests/test_toy_cdda_survey.c`: the survey on a bus model that admits each read by address, width, interrupt mask, SR.BL, G2 DMA and the FIFO | 12 scenarios, 448,548 checks; 109,034 reads of the chip, none outside the admitted addresses, none unmasked, none with G2 DMA on, none more than eight after the FIFO was empty; nothing written |
| The survey on the original drivers | `make toy-cdda-survey-driver-check` over the 60 driver images of the owner's sound-driver archive, read in place and never copied (`driver-check.log`: one line an image with its full SHA-256) | 60 run, 59 held against the static reading's family (the 60th is the damaged 1999.03.18 build). Every version and build date the image's own. 19 of the 59 families rest on the image's bytes alone; the rest also on a start-up store the check authors from the static reading, so the AUDIO64 against hybrid lines check the model, not the image |
| The call sites | The gesture test with the survey on (`toy-survey-mixed-pad-host-test`), built with the worker's cooperative pump | 274 fast and 277 mixed completions each surveyed once; an early-SDK discovery batch none, its GETCOND batch one; a batch without the port-A controller none; the completion that ends the hold none |
| Tests notice defects | The drive's 41 mutations, the survey's 50, and 5 put into the heartbeat's call sites | 41 of 41, 50 of 50 and 5 of 5 caught |
| The report | `tests/test_toy_wire_report.c` | 583 bytes with nothing counted, 979 with a full session's values and the widest CD SV row; no row is ever cut |
| The card | Test 83's save and survey-file tests with this recipe | Passed on FAT32 and exFAT, the volume at sector 0 and 63; `tools/toy_survey.py`'s blank file is the stage's |
| Card Tools admission | `make toy-wire-package-test` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The scratch program of Test 79's verification on the packaged bytes of both modules (`games-admission.log`) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New and the Legacy match, the toggle, the mode records, a blank cache seed slot: as the Games app asks |
| Reproducible | The same recipe built twice from nothing; both recipes again at `88ec3d6` (tests only since) with the label `4abc63b0cc58` | Module SHA-256 `597d8eac...0260` both times; at `88ec3d6` both delivered modules byte for byte |
| The second module | The same recipe with `TOY_CDDA_SURVEY=0`: its linked, parts and package checks, and every host target above with its switches (`alt-host-tests.log`) | Module SHA-256 `e74cca71...3014`. Its auxiliary, both low residents and the observing worker are the first module's bytes. Its linked, parts and package checks pass; the save, drive, gesture and whole-reader host tests and the drive's 41 mutations pass. The report test did not compile with the survey off (a test-only defect, fixed in `88ec3d6`); at `88ec3d6` it passes: 549 bytes with nothing counted, 952 at a full session's values, with no CD SV row |
| Test 83 | This commit with every Test 84 switch off, labelled `1d80f771f081` | The owner's Test 83 module, byte for byte: `313ac3bd...5f1c` |
| The installed New reader | Its recipe at this commit, labelled `f8807cb16561` | `467be62f...e537`, as at `9fd8e90` before Test 84 |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 9 of 10 pass. The tenth fails at Main `4441e92` as it fails here: raw target macros in `src/core/bba_sockets.c` and `src/dreamcast/shell_font.c`, which this change does not touch |

Not checked by any test here, because only a console has it: whether the
games accept the drive's answers as a real drive's; what the sound CPU's
drivers have in their records while the games play; whether the survey's
reads, inside the game's own Maple completion, disturb a game's sound or
timing.

## Review

Three readers beside the author, each with one question and without the
author's conclusions, on 20 November 2026, on the survey as committed
(`5f44706`; the third re-ran its work on `8894be0`, which came in during the
review). Each worked on scratch copies outside the tree. Their findings,
shortened, and what was done with each are in
`docs/evidence/cdda-background-test-84/review.md`. None of it is a console
result.

| Reader | Question | Found | Done |
|---|---|---|---|
| The drivers | Does the survey read each original driver as the drivers' own code does? It ran the survey over the 60 images of the owner's sound-driver archive | 2 defects: MANATEE 1.01.1d (Tokyo Xtreme Racer and four more discs) writes no state word, so the survey called it unknown; AICADRV's word at 0xF8 is a live voice count, which the survey took for identity. 1 risk: the build date was read only when the version or state word changed. 2 nits: a hybrid reported as AUDIO64 after a survey cut short; a MANATEE 2 split with fewer than two stream ports counted as unproved. 1 test gap: nothing ran the survey on the original images | All in `8894be0`: MANATEE 1 told by its version; identity by the version and a build date read at every survey, the state byte reported for the MANATEE families only; the family of the last survey that finished kept; such a split has no candidates; the original-driver check |
| The bus | Can the survey disturb the game's bus, its masked time or its stack? | 1 risk: the 2 ms bound and the spacing hold only under the Toy timer profile, which the survey did not check itself | In `8894be0`: each surveying heartbeat reads TMU0's set-up and surveys only under the profile (SKIP's timer field) |
| The integration | Do the tests catch what can go wrong at the call sites, in the report and in the records? | 7 test gaps: the early-SDK rule and the cooperative pump at the fast call site; the mixed call site's pad guard; the 2 ms budget pinned only to about 8 ms; seven of the report row's caps; what a survey whose records proved nothing must not count; NAMIHEI's bit 0; MANATEE 1's first proof. 1 risk: the original-driver check authors the AUDIO64 ports' base word, so that family comparison is circular. 3 nits: the FIFO mask's two other bits; this document missing; the first survey near the clock's wrap. And: no linked check bounds the survey's stack margin | All in `4abc63b` and this document: the gesture test's survey variant built with the pump, with an early-SDK pass and the pad guard pinned; the 2 ms budget held; the widest report row; eight more survey mutations, five call-site mutations; each original-driver line labelled with what it rests on; `tests/test_toy_cdda_survey_linked.py` |

The readers quote frame sizes from their own builds (one with GCC 13);
the delivered build's are under Verification.

Before the archives were sent, two more readers checked them on 21
November 2026: one read both READMEs against the source, one audited the
archives as packages. Their 26 findings were in the READMEs, the
archives' container and the evidence; none was in a module. The largest:
MDK2's audio is on the owner's card but did not fit in the reader's map
(the first README had said it was not on the card), and the two modules'
launch records are alike, so the owner keeps a list. The audit also found
that no host test had been built with the second module's switches: the
report test did not compile with the survey off, a test-only defect fixed
in `88ec3d6`, after which every target passes. A third reader checked the
revision and added four points, the largest that a stuck game that still
polls its controller can save its report, which the README had not said;
a last reader found the second revision ready, with one nit on how long
to hold the save. The record, with what was done with each finding, is
`docs/evidence/cdda-background-test-84/predelivery.md`.

## Package

Two archives, prepared 2026-11-21 01:13 CST (America/Chicago), from the
clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Drive-Test-84-21NOV2026-0113-CST-4abc63b.zip` | 192,055 | `428a4ba43653682e00967588b3d5c29222ddcd8629200192c2a78848b0aac012` |
| `KUI-CD-Drive-Test-84-without-survey-21NOV2026-0113-CST-4abc63b.zip` | 186,361 | `05ecd4b4279bed8dc3636b90827141585a0e9d33324b1d1bbf85e15cf8cad2ed` |

| Archive | Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|---|
| first | `KUI/apps/games/retail-native-dual.kui` | `4abc63b0cc58` | `BAFAAF7C` | `597d8eac...0260` | Changed for the test only: Test 84 in the place of the installed New reader module, which the owner copies away first and puts back afterwards |
| first | `KUI/SURVEY/STREAM.TXT` | none: text | none | `15e12c77...c006` | Replaced: the blank report file of this test, 100 slots, ID B5829D27CAF13A94DEAFD593D1FDC00A; no software |
| second | `KUI/apps/games/retail-native-dual.kui` | `4abc63b0cc58` | `E00000C9` | `e74cca71...3014` | Changed for the test only: the same module without its survey (`TOY_CDDA_SURVEY=0`), used only if a game misbehaves with the first |

Module SHA-256 in full: the first
`597d8eac96b0238045e7bde9c642653a0bb5e138d6763993cabd0c156c210260` (315,744 bytes), the second
`e74cca711600ecd8ddd38a62839fda7c6dda837740331fb3f0f458b761f03014` (313,920 bytes). Both carry the build label `4abc63b0cc58`;
a report the second saves has no row CD SV. Each archive has its README
and SHA256SUMS.txt (`docs/evidence/cdda-background-test-84/`).

Retained on the card: everything else. The owner's own New reader module
is copied from the card before the test and put back after it; it is not in
either archive. No KUP, bank or user data is included.

Rebuild (the first; the second with `TOY_CDDA_SURVEY=0`):

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 \
  KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 \
  TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1 TOY_CDDA_SURVEY=1 \
  BUILD_ID=4abc63b0cc58 BUILD=build/t84-main build/t84-main/retail-toy-test-84.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 \
  KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 \
  TOY_CDDA_ROOM=1 TOY_CDDA_HOOK=1 TOY_CDDA_DRIVE=1 TOY_CDDA_KEEP=1 TOY_CDDA_SURVEY=0 \
  BUILD_ID=4abc63b0cc58 BUILD=build/t84-alt build/t84-alt/retail-toy-test-84.kui
python3 tools/toy_survey.py blank STREAM.TXT --slots 100 --id B5829D27CAF13A94DEAFD593D1FDC00A
```

Run on the console: the steps of Console test, with the archive's README.
Cold CD boot, not Restart K-UI. Confirm the stage's line
`SURVEY ON / REPORT SAVES TO SLOT nnn`, and in the returned file the launch
record's `BUILD 4abc63b0cc58`.
