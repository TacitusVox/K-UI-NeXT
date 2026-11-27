# Test 85: CD music in the Background reader, part 2: the music is heard

Status: packaged 27 November 2026, 13:13 CST (build `b9cff7086258`, commit
`b9cff70`). Not yet run on a console. A test module in the place of the
installed New reader, like Tests 79 to 84, not an accepted component. It is
the first build of this line that writes to the sound chip.

It answers the owner's request of 27 November 2026: "I guess make the next
test/zip for CD music." The games are the ones the owner's handoff of 26
November named for the first audible build: ChuChu Rocket and The King of
Fighters Dream Match 1999 as the two driver cases, POD SpeedZone as a
further MANATEE 1 case.

## Baseline

Test 84's recipe (`cdda-background-test-84.md`): the drive that plays in
silence, with KEEP and the read-only survey of the sound chip. Its
delivered module (build `4abc63b0cc58`, SHA-256 `597d8eac...0260`) ran on
the console on 21, 22 and 26 November.

That module no longer builds from the tree as it stands. Main's 3.0 RC1
commit `8cf4bc7c` changed the version text ("K-UI V2.0.0" to "K-UI 3.0
RC1"), which the legacy low resident and the stage carry: the legacy
resident is 15,124 bytes where it was 15,120, and Test 84's recipe gives
`68f9f3de...f22d` (315,748 bytes). With the version header of `4abc63b`
the recipe still gives the delivered file byte for byte. Test 85's modules
carry the RC1 text. With every switch of this test off, Test 84's recipe,
Test 83's and the installed New reader's give the same bytes at this
commit as at `f4cc67dd`, the commit before this test (Verification).

## Why

Test 84 showed on the console what a sounding build needs
(`cdda-background-test-84.md`, Console result): ChuChu Rocket, KOF Dream
Match 1999 and POD SpeedZone took the drive's answers; each runs one
MANATEE build whose records proved their layout in every survey; and the
two channels a sounding test would take stayed free through 2,640, 10,084
and 3,684 surveys. What no survey could say is what card time a second of
audio costs beside the game's own reads, and what the drivers do when two
of their channels sound. Those are this test's questions.

## The change

Four switches of `Makefile.vmu-native-toy-suite`, one behaviour each, each
on the one before it and on Test 84's recipe. Changes to shared sources sit
under `#if KUI_TOY_CDDA_*` with Main's text unchanged
(`make test-toy-test-switches`).

| Commit | Switch | What it does | Parts |
|---|---|---|---|
| `5558f6e5` | `TOY_CDDA_CARD` | The worker's heartbeat asks for a play's audio one segment at a time; the auxiliary reads it with the Background reader's own stream while no read of the game is asked for, and hands on only blocks whose CRC was checked. With no sound output a consumer takes the bytes and drops them. The report says TEST 85 | auxiliary, stage, worker |
| `471c51df` | `TOY_CDDA_YIELD` | An idle read-ahead window is given up for audio that runs low | auxiliary |
| `07d79b21` | `TOY_CDDA_OUTPUT` | The audio is heard: a ring in sound memory, looped by two channels the game's driver leaves free, at full level | worker |
| `b9cff708` | `TOY_CDDA_VOLUME` | The two channels take their level and pan from the sound chip's CD input mixers, which the game's driver sets | worker |

Both low residents and the observing worker are Test 83's, byte for byte
(`make toy-cdda-parts-check`). The auxiliary, the native worker and the
stage are this test's.

The delivered module is built with all four and without Test 84's survey
(`TOY_CDDA_SURVEY=0`): the output reads the driver's identity and the
channels' records itself at every heartbeat, and the survey's 1.6 KiB make
room for it. The worker has 1,088 bytes left below its stack, the auxiliary
288 below its own.

### The audio from the card (CARD, YIELD)

`include/kui/toy_cdda_card.h`, `src/loader/toy_cdda_card.c` (worker),
`src/loader/toy_cdda_aux_card.inc` (auxiliary), `include/kui/toy_cdda_mail.h`
(the block they share).

The consumer asks for sectors [lba, end) of one audio track under a
generation number: a new number starts a segment, the same number goes on
where the last heartbeat's bytes ended, 0 gives the card up. The worker
writes the request into a shared block of 1,312 bytes (41 cache lines) in
its own cleared state, reached by both parts at its uncached address only,
and makes TICK visits to the auxiliary. The worker names the block in its
player's descriptor (now version 2, four words); the stage proves it lies
in the worker's cleared state on whole cache lines, or stops the launch.

At a visit the auxiliary:

- takes the card for the audio only in a TICK visit and only while no read
  of the game stands in any state, the bus is opened and sound, no wait of
  the low reader's own stands, and no read-ahead window is open. With
  YIELD an idle window is given up when the consumer says it runs low;
- opens the Background reader's stream at the segment's next card block:
  one block by programmed transfers first, each checked by the low reader,
  then the stream;
- hands on at most two card blocks a visit (a 1,024-byte sink), and only
  blocks whose CRC16 was checked, a block with one lost byte rebuilt from
  that CRC among them. A block that cannot be handed on ends the stream
  and is read again;
- ends the audio's stream at once for a read of the game's.

The worker makes more than one visit in a heartbeat only after a stream
has just handed bytes on, and only while the longest such visit seen still
fits a soft window of TMU0: 2,344 ticks (3 ms), half of it under the early
SDK's drivers. The first visit of a heartbeat is always made; a visit that
opens a stream is not bounded by the window.

By design there is no audio before the game's first read or after a loan
of the card until the next read (the bus is not opened), none while the
game uses DMA channel 1 or the interrupt's route is not armed, and none
while a wait of the low reader's own stands. A track the map holds no file
for is refused, with the reason in the report.

### The sound (OUTPUT, VOLUME)

`src/loader/toy_cdda_output.c`. It is the consumer: at the worker's two
heartbeat sites, after Test 84's survey where one is built, under the same
rule for the early SDK's drivers.

Whose channels and whose memory. The game's sound driver owns the chip.
Output is given only where the driver running is one of the three builds
whose records were read and whose two candidate channels the console
survey found free all through play:

| Driver (version word at 0x20, a hash of its build date, its running mark at 0xF8) | Game | Channels | Free while |
|---|---|---|---|
| MANATEE 2.00.09 (`0x00090002`, `C46D`, 'EXEC') | ChuChu Rocket | the top two stream ports, 62 and 63 for 2 to 16 streams, by the split bytes 0x12060..62 and the base word 0x12068 | their port records' playing bits (0x13800 + 0x40 x port, bit 0) are clear |
| MANATEE 1.01.3b (`0x003B0101`, `1C1F`, 'SEGA') | KOF Dream Match 1999 | pool entries 13 and 14, channels 61 and 62 | their owner words (0x12080 + 12e + 4) are zero, the entry's register base at +8 proving the layout |
| MANATEE 1.01.37 (`0x00370101`, `E50F`, 'SEGA') | POD SpeedZone | the same | the same |

Any other driver, a driver not running, a sound CPU held in reset or
records that do not prove their layout: nothing is written, and the
report keeps the reason.

The ring. Two planes of 16-bit samples, left and right, of 16,384 frames
each (8,192 or 4,096 where room is short), each plane followed by a
repeat of its first two frames, with a guard line of 32 bytes before and
after: 65,608 bytes at the largest. It is placed at or above 0x18000,
where the driver's own memory ends, clear of every address and size pair
of the driver's bank table (0x14000..0x1429F, 84 pairs: the game's banks,
stream rings and effect memory). A place below 0x1DFE00 is tried first:
from there up a game's CRI library registers its stream slots in that
table when it opens them. The ring is cleared before its first use.
Before a heartbeat writes anything, the driver's identity, the channels'
records, the whole table and the guards are read again. Whatever no longer
holds silences the play and keeps its reason; what is no longer the
output's is not written again.

What is written: the ring with its guards, and registers +0x00 to +0x44
of the two channels. Nothing else: no mixer, no monitor select, no other
channel. Every store goes through one function that admits only those
addresses. Starting and stopping keys the two channels on and off with
one execute strobe, as the drivers do at every note.

The bus is used under the survey's rules: TMU0 in the Toy profile, nothing
with SR.BL set, nothing while a G2 DMA channel is enabled or running, the
interrupt mask at 15 throughout, the G2 FIFO empty before every burst of
at most eight accesses, a word the sound CPU may be writing read until two
reads agree, each stretch of bus work within 3 ms of TMU0. The output
works once a sixtieth of a second, and sooner (2 ms after the last
heartbeat's start at the earliest) while the ring has room for 256
frames.

The clock is TMU0; the chip's own position belongs to the driver. The
ring starts at its first frame at key-on, after 4,410 frames (a tenth of
a second) have been written. What has been played since is counted from
TMU0 with a margin for the two clocks' drift (1 part in 5,000 and 64
frames). The play starts over from the drive's position when that margin
reaches an eighth of the ring (after 225 s on the largest ring), when the
driver keyed a channel off (given up at the ninth time in one play), or
when the card did not keep up. A gap between tracks and a refused track
play as silence. When the drive's last pass ends, what is in the ring
plays out.

The level. With VOLUME each heartbeat reads the CD input's two mixers
(0xa0702040 left, 0xa0702044 right) and gives the channels their send
level and pan (mask 0x0F1F). Whether these registers read back on a
console is not known. MANATEE 2 keeps the game's CD level bytes beside
them (0x12470): where both mixers read nothing for eight heartbeats and
those bytes say a level, the channel plays at full level and the report
counts it. MANATEE 1 keeps no such bytes: mixers that read nothing leave
it silent. Without VOLUME the two channels play at full level, hard left
(0x0F1F) and hard right (0x0F0F), as the two CD-music adapters do.

The stack. The completion that calls the consumer guarantees 256 bytes of
the worker's 3,072-byte stack; the output is 312 bytes deep, the card's
heartbeat, the visit's gate and the bytes' way into the ring among them.
It runs behind a gate that asks for 768 bytes and counts the heartbeats
it did not run for want of them (`kui_vmu_native_passive_stack_room`). Entered as the
completion enters it, near the top of the stack, the gate always opens.

The terminal report keys the two channels off before its pages.

## The report

STREAM.TXT is Test 83's format. The saved report says TEST 85; the pages'
heading stays Test 81's and the launch record's first line Test 83's
(`SURVEY 83 BUILD b9cff7086258`). Behind Test 84's rows CD P and CD ST (and CD
SV where the survey is built), in the place of the window stream's rows
from V to W FILL, which have served:

| Row | Values |
|---|---|
| `CD O ST V ID RING N` | the output's phase (0 off, 1 priming, 2 playing, 3 silent), the first reason a play is silent <<4 (1 no admitted driver, 2 the sound CPU held, 3 another driver came, 4 its records prove nothing, 5 the ring, 6 a channel taken, 7 a key dropped too often, 8 the timer), the driver's family <<8, keys dropped in this play <<12, the reason's detail <<16; the version word of the driver last looked at; its date hash, the left plane's channel <<16, the right's <<24; the ring's place in sound memory; a plane's frames, places taken <<16 |
| `CD O KEY RST UND ANC DRP TK` | key-ons; rings begun again; of those, for a ring run dry; for the clocks' margin; keys the driver dropped, and channels found keyed on before a key-on <<16; channels taken, and registers put back <<16 |
| `CD O HB LONG TICK KB SKIP LV` | heartbeats that used the bus; the longest in ticks, the card's time in it; all of them; KiB written to the ring; heartbeats not made for SR.BL <<24 and a G2 transfer <<16, bus waits that ran out <<8, stores refused; the levels last written, left and right <<16 |
| `CD O MX FB PRIME SIL LOST` | the CD mixers as last read, left and right <<16; MANATEE 2's own level bytes (left, right <<8) and heartbeats they stood in <<16; ticks from the last ring's beginning to its key-on; frames of silence written; rings lost to a pair of the table, segments the card refused <<8, rings lost to a guard <<16, heartbeats not run for want of stack <<24 |
| `CD C BLK RB RR PIO WAIT YLD` | card blocks of the stream handed on; of those, rebuilt from their CRC; blocks read again; blocks read by programmed transfers; visits that left such a read waiting; idle read-ahead windows given up |
| `CD C HD HO HX REF DCL ERR` | streams and waiting reads ended for a read of the game's; by the audio itself; found ended by the reader; segments refused, the last one's reason <<16 (3: an audio track without its file's pieces in the map); streams that could not be begun; the first such stream's error, its detail <<16 |
| `CD C VIS TICK HB LONG RND SEEK` | visits for the audio; their ticks; heartbeats that made one; the longest in ticks; the longest visit that only read the stream; ticks in programmed reads |

The module without the sound output has the CD C rows and Test 84's CD SV
row and no CD O rows.

A report of the delivered module holds 570 bytes of text with nothing
counted, of 1,012, 721 with the audio's fields set, and 989 with every
other row at eight digits a value. No row is ever cut
(`tests/test_toy_wire_report.c`).

## Which games it reaches

Sound: the three builds above. On the owner's card ChuChu Rocket has 16
of its 16 audio tracks mapped and POD SpeedZone 8 of 8 (their Test 84
launch records).

KOF Dream Match 1999 has not been launched with the 160-place map. On 21
November, under the 64-place map, its launch record said `AUDIO 40 ...
MAPPED 0 ... EXTENTS 7`: the map needs a place for each track and one for
each piece of each mapped file (`src/apps/games_retail.c`, `map_track`),
so 43 tracks, 7 data pieces and 40 audio files need 90 places at the
least, and the menu left the audio out. With 160 places the audio is
mapped while its 40 files are in 110 pieces or fewer on the card. If they
are not, every segment is refused (reason 3), the output plays silence,
and a fresh copy of the game's folder is the remedy.

Every other Background-routed game runs as under Test 84, in silence:
Bangai-O's driver is ChuChu Rocket's build with a split of no stream
ports (no candidate pair), MDK2 and Toy Commander run AUDIO64, and the
other MANATEE builds are not admitted until a survey has shown their
channels free.

## Console test

Three games, one launch each, three to five minutes where the game plays
CD music, the report saved while music plays: ChuChu Rocket (title, menus,
a stage, a pause and a resume), KOF Dream Match 1999 (character select, a
fight to its end, the next round), POD SpeedZone (menus, a race with the
load before it). Background loading On - New (X), CD music Off, Virtual
VMU Off, a new blank STREAM.TXT, K-UI `aaaba0ea3640` or `53a33ec708ab`.

Questions:

1. Is there music, the right piece, clean? Gaps, crackle, a wrong pitch,
   left and right exchanged?
2. Does the card keep up beside the game's reads (UND, the CD C rows)?
   POD read data 596 times during one play in Test 84.
3. Do the CD mixers read back (MX), and is the level right against the
   game's effects?
4. Do the drivers leave the two channels and the ring alone (DRP, TK, the
   reasons 3 to 7), and are the game's own sounds unharmed?
5. What does a heartbeat cost (HB, LONG, TICK; VIS, TICK, LONG, RND), and
   is play as smooth as under Test 84?

| Outcome | What it says |
|---|---|
| Music, clean, in all three | The path holds end to end; next the other MANATEE builds by survey, then AUDIO64 |
| Music that breaks up or starts over, UND counting | The card does not keep up in that game: BLK against the play's time gives the rate reached, HD the game's reads in the way, LONG and RND what a visit costs |
| No music, MX zero, MANATEE 1 (KOF, POD); ChuChu Rocket plays with FB counting | The CD mixers do not read back: the first if-needed module (full level) is the comparison, and a level must come from elsewhere |
| No music, phase 3 with a reason | 1 to 4: the driver or its records are not as surveyed. 5: no place for the ring (its detail names the table's pair). 6, 7: the driver uses or stops the channels |
| No music in KOF, REF counting with reason 3 | Its audio files are in too many pieces for the map: a fresh copy |
| A game's own sounds wrong, a hang or a crash | The second if-needed module (the card's reading without the sound) says whether the reading or the output is the cause |
| LOST's top byte counts | The completion ran with less than 768 bytes of stack: the output never ran |

## Known limits

1. Three driver builds. Every other game stays silent.
2. Whether the card sustains 176,400 bytes a second between the game's
   reads is unmeasured: the host model lands each DMA run whole.
3. YIELD ends an idle window with the low reader's own stop, which waits
   out the card's busy with interrupts blocked and is not sliced; the
   audio's first block follows in the same visit. A card with a long busy
   holds that heartbeat.
4. A visit that opens a stream is not bounded by the soft window: it
   holds the stop and start commands, up to 64 wait bytes and one
   programmed block.
5. The heartbeat's TICK visit can run while a GD call of the game's is
   suspended outside its own visit. Its guards are the state's (no read
   in any state, the bus opened and sound, no window, no foreign wait).
6. Each segment's start costs one programmed block and a new stream:
   every track, every lap, every piece of a fragmented file.
7. A block rebuilt from one lost byte has 8 bits of its CRC16 left as
   evidence.
8. The play starts over, with a short gap, every 225 s of unbroken play
   on the largest ring (109 s and 51 s on the smaller ones), and whenever
   the ring runs dry. The first fraction of a second of a piece is not
   heard: the ring is cleared once (17 heartbeats for the largest) and primed, and
   the play begins where the drive stands by then.
9. The bank table's contents in the three games are known only from the
   static reading. A ring that finds no place is silent with reason 5.
10. With mixers that do not read back, VOLUME leaves MANATEE 1 silent.
11. The driver facts are static readings confirmed by Test 84's survey
    for identity and free channels only. That a driver puts up with
    foreign key-ons on its channels has not been observed.
12. The stage's proof of the shared block is not executed on the host; it
    is covered by the shared predicate's test, the linked parts check and
    reading.
13. All three delivered modules carry the build label `b9cff7086258`; the
    owner keeps a list of each launch's game and module.
14. The output was not read by a second reader before delivery. The
    card's half was written and tested apart from the output's, against
    the one interface.

## Verification

The modules are built from commit `b9cff70` (label `b9cff7086258`) on a clean
worktree, and every check below ran on that worktree with the pinned
toolchain. Logs are in `docs/evidence/cdda-background-test-85/`. None of
this is a console result.

| Check | What ran | Result |
|---|---|---|
| The parts | `make toy-cdda-parts-check`: Test 83's recipe, Test 84's and this one without YIELD beside this one | Both low residents and the observing worker are Test 83's, byte for byte. The auxiliary keeps its span and stack top; its code ends at 0x8c4277e0 below the stack at 0xac427900, 1,952 bytes of its room used by the audio. The worker's player descriptor names the shared block in its fourth word: 1,312 bytes at 0xac110980 in the cleared state. Without YIELD the worker keeps its bytes and the auxiliary differs by 64 bytes |
| As linked | `make toy-wire-linked-check`: Test 77's linked checks, the auxiliary's stack bound, and `tests/test_toy_cdda_card_linked.py` on the worker | Passed. The auxiliary's deepest use is 1,052 bytes with the gate, of 1,776 usable (1.69 x), the audio's paths among them. The worker: the consumer is the output's gate (it asks for the stack's room first and calls the output only then; nothing else calls the output); the output behind it is 312 bytes deep, the card's heartbeat, the visit's gate and the bytes' way into the ring among them, and is called only with 768 bytes of the private stack below it: 704 clear of the guard words (2.26 x). Called from the completion only, at 2 sites, each after `stack_valid`. The check refuses 18 altered readings of itself |
| The sound output | `make toy-cdda-output-host-test`: `tests/test_toy_cdda_output.c`, the output on a model of sound memory, the channel registers, the CD mixers, the sound CPU's reset, the G2 FIFO and DMA, TMU0, SR and a stand-in for the card. The model takes no store but the ring's and the two channels' registers while their records are free, each with the mask at 15, SR.BL clear, G2 DMA idle and at most eight accesses since the FIFO was empty; its chip plays what is in sound memory, and every frame played is held against the play's audio as the test reckons it by itself | With VOLUME 826,844,293 checks, without 822,360,865. A play on each of the three builds: 1.6 million frames, every one the play's, through pause, release and the end. Tracks, a gap, a refused track and laps. The chip's clock 150 parts in a million fast and slow over five minutes: every frame the play's, the ring begun again once. Nothing written for another driver, one not running, a held sound CPU, unproved records, a used channel, no room, no clock, a G2 transfer, SR.BL. The ring's place; a taken channel left alone; a dropped key; changed registers put back; a slow card, a card that hands nothing on, a bus that does not answer. The levels from the mixers and from MANATEE 2's bytes; the terminal key-off |
| The audio from the card | `make toy-cdda-card-host-test`: the audio's scenarios on the reader's integration model (the real low reader, stream, auxiliary and service on the shared card model) | 90,240,164 checks |
| The whole reader | `make toy-wire-integration-host-test` | 460,586,482 checks, the audio's scenarios behind everything else. A play beside 5 reads: function 4 answered 3 all 15 times |
| Asked for nothing | `make toy-cdda-card-quiet-check`: the integration test ahead of the audio's scenarios, with the audio's card work built in and without | Every programmed byte's place, every visit's event, argument and answer and the service's fields fold to `f8565e87` both ways |
| The call sites | The gesture test (`toy-survey-mixed-pad-host-test`) | 274 fast and 277 mixed completions each called the consumer once, behind the survey where one is built and with none |
| The drive | `tests/test_toy_cdda_drive.c` | 12 scenarios, 11,704 checks, as under Test 84 |
| Tests notice defects | `tests/test_toy_cdda_output_mutations.py`, `tests/test_toy_cdda_card_mutations.py`, Test 84's drive mutations | 104 of 104 of the output's, 189 of 189 (182 by the audio's scenarios, 7 by the gesture test) of the card's and 41 of 41 of the drive's caught |
| The report | `tests/test_toy_wire_report.c` | 570 bytes with nothing counted, 721 with the audio's fields set, 989 of 1,012 with every other row at eight digits a value; the audio's rows all written, no row cut |
| The card | Test 83's save and survey-file tests with this recipe | Passed on FAT32 and exFAT, the volume at sector 0 and 63; `tools/toy_survey.py`'s blank file is the stage's |
| Card Tools admission | `make toy-wire-package-test` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The launcher's admission helpers, compiled against the K-UI's sources, on the packaged bytes of the three modules (`games-admission.log`) | As Test 84's module: envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New and the Legacy match, the 160-place map, the toggle, the mode records, a blank cache seed slot, the heap-tail and SDK-proof capabilities |
| The owner's K-UI | `python3 tools/check_gui_lines.py --installed-has 4d4dcff77494` | The installed K-UI `aaaba0ea3640` contains the 160-place map's line; so does `53a33ec708ab` |
| Reproducible | The delivered recipe built twice from nothing | Module SHA-256 `59739533...1f2a` both times |
| The module at full level | The recipe with `TOY_CDDA_VOLUME=0`: its linked, parts and package checks, its report and gesture tests | SHA-256 `111ff0d0...6522`. Its auxiliary and both low residents are the delivered module's bytes. 323,972 bytes; 1,248 bytes of the worker left |
| The module without the sound | The recipe with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0`: its linked, parts and package checks, its report, gesture, survey and audio tests | SHA-256 `1bd6e044...b411`. The consumer that drops the bytes is 104 bytes deep and stays 88 clear of the guard words on the least stack (1.85 x); the survey as under Test 84. Its report: 457 bytes with nothing counted, 971 of 1,012 at the widest. 319,396 bytes; 5,152 bytes of the worker left |
| The recipes before this test | Test 84's, Test 83's and the installed New reader's, built at this commit and at `f4cc67dd` side by side | The same bytes at both: `68f9f3de...f22d`, `30cac0f8...507a` and `e69a6fef...93ac` |
| Production text | `make test-toy-test-switches` | 30 shared sources keep their production text |
| The repository's asm audit | `tests/test_asm_audit.py` | 10 of 10 pass |

Not checked by any test here, because only a console has it: whether the
card keeps up; whether the mixers read back; what the three drivers do
when two of their channels sound; what the bank table holds in play;
whether the heartbeat's masked time disturbs a game.

## Package

One archive, prepared 2026-11-27 13:13 CST (America/Chicago), from the clean worktree's modules:

| Archive | Bytes | SHA-256 |
|---|---|---|
| `KUI-CD-Music-Test-85-27NOV2026-1313-CST-b9cff70.zip` | 582,897 | `836ef9f2d86a1b7023f6ab6de5b32c186bb3d48fee231c313da3b7fe9ac7f25e` |

| In the archive | What | Bytes | Payload CRC32 | SHA-256 |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | the delivered module: CARD, YIELD, OUTPUT, VOLUME, no survey | 324,164 | `A96C75CF` | `59739533dd95e0fe1faedd0b2eda9037b1d8d1fc1bf29b21fc26701eccea1f2a` |
| `KUI/SURVEY/STREAM.TXT` | a new blank report file, 100 slots, ID `AC0C9D85004568C20442C57C37EBABA3` | 154,112 | | `3466c3902faa33385456227667d52aa5fa3223d86e1d467585e22b37929a83ef` |
| `IF-NEEDED/1-full-volume/KUI/apps/games/retail-native-dual.kui` | the same with `TOY_CDDA_VOLUME=0`: for a game that runs but stays silent | 323,972 | `7EFCCF72` | `111ff0d0c9a1d5a1596b5f64042ebefda9da3d790cf6d944af9b7e78d1806522` |
| `IF-NEEDED/2-no-sound/KUI/apps/games/retail-native-dual.kui` | the same with `TOY_CDDA_SURVEY=1 TOY_CDDA_OUTPUT=0 TOY_CDDA_VOLUME=0`: for a game that misbehaves; it reads the audio and drops it | 319,396 | `1EA9590E` | `1bd6e044b3c42a2f8a55cc36759492b48ee3229da5421a8d5b0494d81cfab411` |

The archive also holds `README.txt`, `INVENTORY.json` (the files with
their full hashes, what stays on the card and what the module needs of
K-UI) and `SHA256SUMS.txt`; they are in the evidence folder as
`package-README.txt`, `package-INVENTORY.json` and
`package-SHA256SUMS.txt`, beside the three modules
(`retail-toy-test-85.kui`, `-full-volume.kui`, `-no-sound.kui`). The
modules replace `KUI/apps/games/retail-native-dual.kui` for the test only;
no K-UI, updater package, bank, game data or save data is in the archive.
The module needs the 160-place map of the owner's K-UI: the README has the
owner check the Diagnostics page's first line (`aaaba0ea3640` or
`53a33ec708ab`) before the first launch.
