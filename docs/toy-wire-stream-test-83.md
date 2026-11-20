# Toy Commander Test 83: the stream survey, its report saved to the card

Status: built 20 November 2026 (build `3bd4bed2fca2`), host and linked
checks only. It has not run on a console. A test module, not an accepted
component, and not a replacement for the installed New reader: it has no
virtual VMU, no block prefetch and no CD music.

The module is built from `3bd4bed2fca2`: Main `4441e92` with the console
results of Tests 80 to 82 (`719b65f`), the record of the shared sources'
production text (`3ca6410`, tests only), the startup bundler's run over
the CD music games (`05ac704`, a PC tool), and the change (`55ed2f8`),
and the two corrections its review asked for (`3059447`, `3bd4bed`).

It answers the owner's word of 16 November on the photographed pages of
Tests 79 to 81: "this would be a lot more convenient if you didn't break
the report writing feature to the SD card". The reader is Test 81's,
which ran on the console (`docs/toy-wire-stream-test-81.md`, Console
result). What is new is where its report goes.

## Baseline

Test 81, source `684f485644e5`, module `5ee3273b...2db4`, as run by the
owner in Evolution and Evolution 2. On the source this build is made
from, Test 81's recipe with that label still gives that module, byte
for byte.

Test 83's low residents, its auxiliary and its worker are Test 81's.
Built with one label, each is the same bytes as that recipe's
(`make toy-wire-save-parts-check`).

## Why the report was not saved

Up to Test 78 a Toy test was started from Card Tools, which made a
report file of two sectors on the card and patched their place, their
blank checksums and the card's identity into the module: the worker
writes nothing it was not told of. From Test 79 on the module is
started from the Games menu, in the installed New reader's place. The
Games app patches no report seed, the worker had no place to write to,
and the report came as pages to photograph.

## The change

Behind `TOY_WIRE_SAVE=1` on Test 81's recipe. A module is five parts;
one of them changes.

| Part | Test 81 | Test 83 |
|---|---|---|
| Low residents | The Toy tests' since Test 66 | The same bytes |
| Auxiliary (the stream, the window) | Test 81's | The same bytes |
| Worker (the report, at the gesture) | Saves to the two sectors of its seed if it has one; shows pages if not | The same bytes |
| Cold stage (runs once at launch) | Hands the worker the seed Card Tools patched, if any | Looks for the survey's report file itself. Where it finds a blank slot, it writes the launch record there and hands the worker a seed for the slot's two report sectors |

The stage does this once the executable is loaded and the title is known
to be a source for the window or none, before the reader is installed
(`toy_movie_scan`, `src/loader/toy_movie_iso_stage.inc`):

1. The New reader (X), the SCI card, the virtual VMU off. Anything else:
   no survey, and the launch goes on as Test 81's.
2. It looks `0:/KUI/SURVEY/STREAM.TXT` up with the Windows CE data
   probe's read-only FAT32/exFAT lookup (`src/core/wce_probe_fat.c`,
   given another path by `src/core/toy_survey_fat.c`).
3. It reads the file's first sector and takes the first slot whose three
   sectors all read back as that file's blank text for that slot
   (`src/core/toy_survey.c`).
4. Each of the three must lie in the partition, on the card, and in no
   extent of the game's image.
5. It reads the card's identity, writes the launch record into the
   slot's first sector by the byte path the virtual VMU's stage writes
   by, and reads it back.
6. It builds the worker's seed: the slot's other two sectors, the
   CRC-32 of their blank text, the card's identity. The worker checks
   all three again at the gesture, before it writes
   (`src/loader/toy_suite_report_write.inc`, unchanged).

The launch screen gains one line: `SURVEY ON / REPORT SAVES TO SLOT
nnn`, or `SURVEY OFF / ` and the reason. With the survey off the launch
is Test 81's, pages and all.

The survey sends the card two commands Test 81's stage never sends: its
identity, and a write. Each marks the card not ready where it fails,
and the low resident refuses such a card. Both leave the card idle, so
it can be asked: after either failure the card is asked for its status,
twice if need be, as the Windows CE data probe's stage asks after a
failed write, and a card that answers with the port well goes on as
under Test 81 (`toy_survey_settle`).

The survey also reads sectors Test 81's stage never reads. A read that
leaves the card marked not ready (a fault of the port, a stream whose
stop the card did not take) stops the launch, as a read of the stage's
own does: a card whose read was not ended is still sending it, and
would answer a question with the read's bytes.

The stage's stop screen ended `STORAGE WAS READ ONLY`. Once the survey
has begun its write it ends `ONLY THE SURVEY FILE WAS WRITTEN`.

A disc that is no source for the window (2,048-byte sectors, Mode 2)
gets a slot too: the launch record says so (`SOURCE 0`) and the report
is saved as for any other.

## The report file

`KUI/SURVEY/STREAM.TXT`, made blank by `tools/toy_survey.py blank` and
copied onto the card by the owner. Text in 512-byte sectors, each
ending in a line feed, so the returned file opens in any editor:

| Sector | Holds |
|---|---|
| 0 | The file's name, its ID (16 random bytes, in hex) and its slot count |
| 1+3s | Slot s, part 0: the launch record, written by the stage |
| 2+3s, 3+3s | Slot s, parts 1 and 2: the reader's 1,024-byte report, written by the worker at the gesture |

A blank sector reads `==== SLOT 007 PART 1 ==== ID ... ==== EMPTY ====`
and spaces: it names its file, its slot and its part, so no other sector
of the card, and no sector of another survey file, is taken for it.

A launch record:

```
==== SLOT 000 PART 0 ==== ID 5A11...EEFF ==== LAUNCH ====
SURVEY 83 BUILD 3bd4bed2fca2
PRODUCT T-12345N
TITLE EXAMPLE GAME
BOOT 1ST_READ.BIN REGION JUE
IPCRC 12345678 GDICRC 0D0D0D0D BOOTCRC 0B0B0B0B BOOTBYTES 0012D687
EXECCRC 9ABCDEF0 EXECBYTES 003DE7E0
READER 1 TRANSPORT 1 FLAGS 00000004 SOURCE 1
TRACKS 5 RAW 1 COOKED 0 AUDIO 4 HDAUDIO 2 MAPPED 1 OTHER 0 EXTENTS 1
SESSION 0000B05E BOOTLBA 0000B090
```

The title, product, boot file and region are the Games app's, from the
disc's map. `EXECCRC` and `EXECBYTES` are of the executable as loaded.
`HDAUDIO` counts the audio tracks at or above the session: the disc's
CD music. `MAPPED` counts the audio tracks whose place on the card the
launch's map holds: with CD music off the Games app maps them only
where its 64 slots have room.

The report below it is Test 81's, to the byte: the worker is Test 81's,
and its first lines still read `K-UI TOY COMMANDER REPORT`, `TEST 81`.
`tools/toy_survey.py read` lists a returned file's slots; the rows are
read as `docs/toy-wire-stream-test-81.md` reads them.

A slot is one launch. A launch that ends without the gesture (power
off, a hang) leaves its launch record and two blank sectors: the game
was started and no report was saved.

## What can be written, and where

The stage writes one sector a launch, and the worker two at the
gesture. For each of the three:

| Rule | Held by |
|---|---|
| It is a sector of the file the lookup found by following the volume's own directory and allocation, with bounds on every cluster and sector | The lookup, as for the Windows CE data probe |
| The file's first sector is exactly what its ID and slot count give, and the file's size is what that count takes | `kui_toy_survey_file_parse`, `kui_toy_survey_claim_find` |
| It read back, a moment before, as the blank text of this file, this slot and this part | `kui_toy_survey_claim_find`; the worker again, by CRC-32, before its writes |
| It lies in the partition and on the card, and in no extent of the game's image | `toy_survey_place`; `toy_suite_report_prepare` for the worker's two |
| The card is the card of the launch | The worker, by the card's identity, before its writes |

A slot that is not wholly blank is passed over, whatever it holds: a
launch record, a report, a write that power cut off, bytes that are not
the file's at all, a sector the card no longer gives (the search gives
up at the fourth such sector). A write that fails leaves the launch as
Test 81's.

One condition is the owner's to keep: one copy of a blank file on a
card, and a file with a new ID for every refill
(`tools/toy_survey.py blank` gives each its own). The blank text names
the file's ID, so a second copy with the same ID holds the same text.
Were the volume damaged so that this file's chain led into such a copy,
the copy's blank slot would be written, and nothing else: no sector of a
game, of a save or of the file system can hold that text.

No directory entry, allocation table or other file is written. The
file's size and date do not change.

## Started from the Games menu

As Test 81: cold boot from the CD, Games, the game, New (X), the
virtual VMU off.

| Setting | What runs |
|---|---|
| New (X), virtual VMU off | This module: Test 81's reader, the survey on if the file is there |
| Virtual VMU on | The stage's stop `THIS TEST BUILD NEEDS VIRTUAL VMU OFF`, as in Test 81 |
| Legacy (Y) | This module's Legacy reader, no survey (`SURVEY OFF / NOT THE NEW (X) READER`) |
| CD Music on, Restart K-UI with a saved bank | Something else, as in Test 79's table |

## Console test

A short round first: Evolution 2, whose numbers go beside the pages
photographed for Test 81, and three regular games with CD music from
different sound-driver families. The long round over the CD-music
games is for the next build, which is to record the sound hardware's
state as well, so that it is run once.

In each game: the launch line, which the stage shows until the game's
bootstrap returns, a second or so; the opening movie and a few minutes
of play; the gesture at a moment the game is not reading (controller in
port A, both triggers and A+B+X+Y for about three seconds). The worker
then shows Test 81's two pages in turn, `TOY TEST 81 1/2 PLAYBACK` and
`TOY TEST 81 2/2`, each with `REPORT SAVED TO SD`, the first with
`START / REBOOT TO K-UI CD`. Start. At the end the file from the card.

Asked of the owner: the file `KUI/SURVEY/STREAM.TXT` as it is; any
launch line that began `SURVEY OFF`; any game that did not start, hung,
or looked or sounded wrong.

| Outcome | What it says |
|---|---|
| `SURVEY ON / REPORT SAVES TO SLOT nnn`, then `REPORT SAVED TO SD`, and the file holds the launch records and the reports | The report is saved from a Games launch again. Evolution 2's rows are comparable with Test 81's pages. Next: the long round with the sound probe |
| `SURVEY OFF / NO KUI/SURVEY/STREAM.TXT` with the file on the card | The lookup does not find the file on this card's volume: its folder or name as the card's file system stores them. The card's format and how the file was copied are the next question |
| `SURVEY OFF / FILE LOOKUP n` or `FILE CHECK n` | The volume, or the file's first sector or its size, is not what the stage accepts, or a sector could not be read. The number says which |
| `SURVEY OFF / CARD WRITE nn` or `WRITE NOT READ BACK` | The stage's write did not hold on this card. Nothing else was written; the game runs as under Test 81 |
| `SURVEY ON`, and at the gesture pages headed `REPORT NOT SAVED` | The worker refused: the card's identity, a sector no longer blank, or the card not to be had at that moment. The first save in a title that is not Toy Commander is this build's |
| `RETAIL RESIDENT INIT FAILED` under a `SURVEY OFF / FILE` line | A read of the survey's left the card not ready. Once: a fault of the port. Every time: a sector of the file or of its folders that the card does not give; without the file the launch is Test 81's |
| A game that ran under Test 81 does not start, or stops at launch | The stage's survey disturbs the launch there. With the launch screen's last lines |

## Known limits, not changed here

Tests 80 and 81's limits stand for the reader. Added by this change:

1. The first write to the card from the Games-launched Toy stage, and
   the first report saved in a title that is not Toy Commander, are this
   build's console run. Both run in host models only until then.
2. The stage is 9,344 bytes larger than Test 81's (its extent ends
   at `0x8cec34a8` where Test 81's ends at `0x8cec1028`). A title
   whose memory was admitted beside Test 81's stage by less than that
   would stop at launch with the placement screen.
3. The lookup compares 8.3 names on FAT32 and whole names on exFAT, and
   takes 512-byte sectors only, as for the Windows CE data probe. A file
   in more than 128 pieces is refused.
4. One slot a launch, whether a report follows or not. The delivered
   file has 100.
5. The report names Test 81 and Toy Commander: the worker is not
   rebuilt. The launch record names the survey, the build and the game.
6. No survey with the virtual VMU on (Test 82's composition has no
   report), with the Legacy reader, or on another transport than SCI.
7. The launch record's title is cut at 56 characters, its counts at 999.
8. The survey reads the card at launch: the volume's first sector, the
   directories on the way to the file, and one to three sectors a slot
   up to the first blank one. A launch's time from K-UI to the game's
   bootstrap is not comparable with Test 81's.
9. The worker's own fault page still ends `STORAGE WAS READ ONLY`, as in
   Test 81: the worker is not rebuilt. With this build the launch record
   was written before it, and a report if one was saved.

## Verification

The module is built from commit `3bd4bed2fca2` on a clean worktree
(`source_dirty: false`), and every check below ran on that worktree.
Logs are in `docs/evidence/toy-wire-stream-83/`. None of this is a
console result.

| Check | What ran | Result |
|---|---|---|
| The file's text | `tests/test_toy_survey.c`: the first sector, every blank sector and the launch record; slot counts at their limits; counts that are refused; the longest record there can be; text that is no text | Passed. A blank file is printable text, every sector ends its line, no two blank sectors are alike, and the longest launch record still ends with its last line |
| The lookup and the slot search | The same test on RAM disks FatFs formats: FAT32 with 512-byte and 1 KiB clusters and exFAT with 1 KiB, 4 KiB and 32 KiB clusters, the file whole and in pieces, beside the Windows CE data probe's own file and folders with like names | 7 cards, 20 launches on each to the full file. The file in 1 to 8 runs. Each launch claimed the next blank slot; the launch record and, where the gesture came, the report went where the seed said; at the end FatFs read back exactly the expected file, and no other sector of the card had changed (52 sectors written a card) |
| What is refused | No folder, no file, a folder named like the file, the probe's file alone; the file a sector short or long; every byte of the first sector changed in turn; a slot count the size does not give; a first sector of a slot that is another file's blank, another slot's, another part's, a launch record of another slot, zeros, a blank with one byte changed; slots blank in part; a read that fails, wherever it falls; a sector outside the partition | Nothing claimed in any of them but the next wholly blank slot. A changed digit of the file's ID reads as a full file: no slot is that file's blank |
| The stage's own text | `tests/test_toy_survey_stage.c`: `toy_movie_scan`, `toy_survey_prepare` and `toy_suite_report_prepare` as the module compiles them, on a card model that holds them to the card's rules (one acquisition a read or write, no stream left open, the block path off for a write, the write flag set before the card is written) | Passed on FAT32 and exFAT. A slot for a disc that is a source for the window and for one that is none; one sector written a launch, at the slot's first sector, a launch record of this game; the worker's seed names the other two, still blank, on this card |
| The stage's refusals | The Legacy reader; another transport in the map or of the card; the virtual VMU on; a partition past 32 bits; a slot's sector in an extent of the game's, past the map's card, past the card; the card not ready at the start; no identity; the card not to be had at any acquisition; a read failing at any place, with and without leaving the card not ready; the write refused; the port unwell; a write the card took and did not keep; the record written and not read back; a full file; no file; no volume | Each shows its line, gives no seed and writes nothing, or the one sector the case is about. After each the launch can go on: the card is ready, but where it answered its status at neither asking or the port was not well |
| The tool | `tests/test_toy_survey.py` | `tools/toy_survey.py blank` gives the stage's file byte for byte; `read` lists every launch the C test played, with its fields; a truncated file and an edited first sector are refused |
| Tests notice defects | `tests/test_toy_survey_mutations.py`: 62 defects put into the survey's sources one at a time (`mutation-check.log`) | 62 of 62 caught, with the tests without a mutation passing |
| The reader | Test 81's host targets with this build's switches: the stream by itself, the report, the stage's source for the window, the whole reader | Passed: the whole reader's 359,191,981 checks. No host test is given the new switch: they are the tests Test 81's recipe runs on this source |
| The parts | `make toy-wire-save-parts-check`: Test 81's recipe built with this module's label beside it | Both low residents, the auxiliary and both workers with their relocations: the same bytes. The stage alone differs; it looks for `KUI/SURVEY/STREAM.TXT` and not for the probe's file; it still stops a launch that has the virtual VMU on |
| The stage as compiled | Each recipe's stage, preprocessed, Test 81's against this one | 7 places: the survey's sources, the call after the scan, the seed's choice, the stop screen's line and its flag; the rest is headers the survey's sources take in earlier |
| As linked | The five linked checks of Test 77 on the module's auxiliary | Passed |
| Card Tools admission | `tests/test_toy_reader_package.c` on the packaged bytes | Admitted; one blank report seed |
| The Games app's byte checks | The scratch program of Test 79's verification on this module's bytes (`games-admission.log`) | Envelope and CRC32, layout, one dual descriptor with the 0x47000 lease, the New and the Legacy match, the toggle, the mode record, a blank cache seed slot, the handoff limit: all as the Games app asks |
| Reproducible | The same recipe built twice from nothing | Module SHA-256 `a7b1c1cb...11d6` both times |
| Earlier deliveries | Unchanged recipes on this source, each against its recorded hash | Tests 66 to 82 with their second modules and variants, 21 modules: all as recorded |
| Production text | `make test-toy-test-switches` with the lookup's source added to the guarded list | 30 shared sources keep their production text |
| The installed New reader | Its recipe at this commit and at Main `4441e92`, one label | The same module, byte for byte |
| The Windows CE data probe | Its module built at this commit and at Main `4441e92`; its lookup's and its end-to-end tests | The same bytes; both tests pass |
| The repository's asm audit | `tests/test_asm_audit.py` | 9 of 10 pass. The tenth fails at Main `4441e92` as it fails here: raw target macros in `src/core/bba_sockets.c` and `src/dreamcast/shell_font.c`, which this change does not touch |

Not checked by any test here, because only a console has it: that the
lookup finds the file on the owner's card as their computer wrote it;
the write and the identity command from this stage on the real port;
the worker's save in a title that is not Toy Commander.

The stage's text is 298,980 bytes where Test 81's is 291,820, and
its zeroed memory 500,904 where Test 81's is 498,728.

## Review

Four readers beside the author, each with one question and without the
author's conclusions, on 20 November 2026, on the change as it stood
(`55ed2f8`, then `3059447`). Each worked on scratch copies outside the
tree. Three of them built host programs from the repository's own
sources (the stage's text, the lookup, the card command code, the
identity read, the worker's save) over a card modelled byte for byte on
the wire, and put faults into them. Their full record is in
`docs/evidence/toy-wire-stream-83/review.md`. None of it is a console
result.

| Reader's question | Found | What was done |
|---|---|---|
| Can a launch that Test 81 boots stop or change? | A failed identity command left the card marked not ready, and the low resident then refused it: `RETAIL RESIDENT INIT FAILED` where Test 81 boots. The stage test's card could not show it: its identity read left the card ready | Before the first commit: the card is asked for its status after a failed identity command, as after a failed write. The test's card now loses its readiness as the real code takes it |
| | A read that left the card not ready stopped the launch | Kept, and stated: see the next row |
| Does the unchanged worker save with this stage's seed? | It does: the reader ran the worker's own save code with the stage's seed on FAT32 and exFAT cards, block- and byte-addressed, and FatFs and the tool read the reports back. Nothing in the save is particular to a title | Nothing to change |
| | The status was asked of any card left not ready, also after a read that was not ended. Such a card is still sending the read; the answer was the read's own bytes, and where those were zeros the launch went on with the card inside a read | `3059447`: the status is asked only after the identity command and the write, which leave the card idle. A read that leaves the card not ready stops the launch. The test's card stays inside a read that was not ended and refuses to be asked there |
| | No document told the owner what the screens show | This document and the package's README: the worker's two pages, `REPORT SAVED TO SD`, the launch line's second on screen, the gesture at a quiet moment |
| Can any sector be written that is not the file's blank slot? | No: with a fault at every byte of the stage's and the worker's card traffic (a port fault, a wrong bit from the card, a wrong bit to it; about 1.3 million runs), with power lost at every byte, and on 600,000 damaged volumes, no sector was written that did not hold this file's blank text for that slot and part a moment before | Nothing to change in the writes |
| | A sector the card no longer gives after a power cut in its write ended the slot search at every later launch: the survey was off for good | `3bd4bed`: such a slot is passed over; the search gives up at the fourth sector not read |
| | On a damaged volume whose chain leads from this file into a second copy with the same ID, that copy's blank slot is written | No sector of a game, a save or the file system can be reached this way. The condition is stated: one copy of a blank file on a card, a new ID for every refill |
| Do the tests and the build hold what they claim? | This reader had not finished when the build was delivered. Its questions were put to the build directly instead: the defects of `tests/test_toy_survey_mutations.py`, the parts check, the guarded production text and the probe's module bytes (Verification) | Its result is added to `review.md` when it is in |

Checked by the readers and found in order, among other things: every
acquisition of the card released on every path; no read stream left
open; the block path given back after the write; the stage's deepest
call chain about 1 KiB on a stack of 64 KiB; no loop without a bound;
27 sector reads for a launch with a blank file on a model card, 126
with 99 slots used; the seed's two checksums and the worker's one and
the same function; the seed kept in the stage's memory until the worker
is configured; `KUITRS01` with its version and size still once in the
module, so that a patcher of the old seed sees no second match.

## Package

`KUI-Stream-Survey-Test-83-20NOV2026-0042-CST-3bd4bed.zip`, 183,794
bytes, SHA-256
`4e09f97a4a7bea51f1878a4ced9b19e737902858237dae046454e887b7f9a897`,
prepared 2026-11-20 00:42 CST (America/Chicago).

| Path | Build | Payload CRC32 | SHA-256 | Status |
|---|---|---|---|---|
| `KUI/apps/games/retail-native-dual.kui` | `3bd4bed2fca2` | `7A90B4AB` | `a7b1c1cb...11d6` | Changed for the test only: Test 83 in the place of the installed New reader module, which the owner copies away first and puts back afterwards |
| `KUI/SURVEY/STREAM.TXT` | none: text | none | `3e32647c...14e3` | Added: the blank report file of this test, 100 slots, ID B674BC235B6B5A9B78760CBD6F7E63FF; no software |

Module SHA-256 in full:
`a7b1c1cb97b1c40f79bf0d6f7137fc0aec21976bfe499ef803ad359a98bb11d6`.

Retained on the card: everything else. The owner's own New reader
module is copied from the card before the test and put back after it;
it is not in this archive. No KUP, bank or user data is included.

Rebuild:

```
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 BUILD_ID=3bd4bed2fca2 \
  build/toy-suite-83/retail-toy-test-83.kui
make -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 \
  TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 \
  TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 BUILD_ID=3bd4bed2fca2 toy-wire-host-tests \
  toy-wire-integration-host-test toy-wire-package-test toy-wire-save-parts-check \
  toy-wire-save-host-test toy-wire-save-mutation-check \
  toy-wire-linked-check TOY_WIRE_LOOP_REFERENCE=<a Test 73 native-ra-aux.elf>
python3 tools/toy_survey.py blank STREAM.TXT --slots 100 --id B674BC235B6B5A9B78760CBD6F7E63FF
```

Run on the console: the steps of Console test, with the archive's
README. Cold CD boot, not Restart K-UI. Confirm the stage's line
`SURVEY ON / REPORT SAVES TO SLOT nnn`, and in the returned file the
launch record's `BUILD 3bd4bed2fca2`.
