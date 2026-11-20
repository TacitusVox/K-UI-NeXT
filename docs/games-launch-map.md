# Launch map: up to 99 tracks

## Native SCI Background on a fragmented card

K-UI accepts up to 99 track descriptors. The native compact Background reader
also needs one entry for every contiguous run of card blocks it will read.
The updated GUI reads the installed reader's capacity. A blank capability
record selects the original 64-entry limit. A matching 160-entry reader can
use the existing version 3 disc map and the same 284 KiB game-RAM reservation.
Both New and Legacy use the negotiated capacity on this paired reader.

For example, a 37-track disc with one data track in 49 pieces and two other
data tracks in one piece each already needs 88 entries to read the data.
If each of its 34 audio tracks occupies one piece as well, all tracks and their
data fit in 122 entries. With 99 track descriptors, 61 entries remain for
file pieces. When the native Background path is allowed to omit audio-sector
mappings, it retains the original audio track numbers and table of contents.
An image whose required data pieces still exceed the available slots is
refused before handoff. The GUI reports the required total when it can count
all the data pieces.

The GUI checks the final selected module, including the standalone fallback.
The updated standalone New reader advertises 128 entries in a separate
low-memory layout. It can read a 31-track disc with 39 data extents using
70 entries. When the combined reader's SDK memory pool would overlap the
live executable, New with Virtual VMU Off can select this reader after
checking that the executable supports its SCI callbacks.

The standalone reader supports gameplay without a stream survey. The
survey reader's `SURVEY ON` and `REPORT SAVED TO SD` messages identify a
recorded Test 83 run. The new combined module can also prove a bounded
SDK allocator wrapper that jumps to the allocator using its caller's
return address. Its fixed capability record tells the GUI whether the
installed module can replay that proof, including on cached launches.

Older readers keep their own limits. Windows CE and dedicated CD music
readers have their own map formats or limits. A GUI update alone does not
enlarge an installed reader. Install the matching GUI and compact reader
from the same overlay. For Claude's Stream Survey Test 83 use the survey
overlay on an existing Test 83 installation and keep its report file.

## Version 1.8.5

The current map is `KUIRTI03` (version 3), supporting the image geometry in
[Games formats](games-formats.md). Runtime and game payloads must be installed
together; version 2 payloads cannot be mixed with them. The standard native
reader has 160 shared track/physical-extent slots; the background SCI reader
has 64, with up to 99 tracks subject to the shared budget. Native background
maps may fall back to standard. Windows CE is background-only in the UI and
fails when its 64-slot map cannot fit.

Native code/data and private stack now stay below IP at `0x8c004000..0x8c007d00`;
guest RAM still begins at `0x8c008000`. CE keeps its separate placement. See
[placement evidence](evidence/native-low-resident-2026-10-06.md).

## Historical version 2 expansion

The measurements and package layout below describe the earlier 99-track
expansion. They are retained as development evidence, not current byte budgets.


A launch reads a game's track files straight from card blocks, through a map
K-UI builds from the card's allocation table. The map used to hold 16 tracks
and 128 file pieces (extents), so Bust-a-Move 4 (25 tracks) and MDK2 (31
tracks) were refused. It now holds up to 99 tracks, GD-ROM's limit, in the
same memory.

## How it fits

Tracks and file pieces share one table of 12-byte slots: the tracks first,
then each track's pieces in track order (`union kui_retail_slot`,
`include/kui/retail_image.h`). A track entry holds its start, end, first
piece, piece count and whether it is data or audio: 12 bytes, half the old
24. A track's number is its position. The readers keep the 1,920 bytes they
used for 16 tracks and 128 pieces:

| Reader | Slots | For example |
|---|---|---|
| Standard (SCIF, SCI, IDE, Windows CE) | 160 | 31 tracks and 129 pieces; 99 tracks and 61 pieces |
| Background SCI reader | 64 | 31 tracks and 33 pieces |

No reader grew (CI build `e965e09d10b0`): code is 12 to 52 bytes smaller,
free space is unchanged for SCIF (596 bytes) and SCI (48), and up for IDE
(1,380 to 1,412) and the background reader (36 to 100). The disc service now
reads one track layout instead of two.

Read speed does not depend on the track count.

## Audio tracks

K-UI plays no CD audio: play commands are accepted silently, as before. When
every track's file fits, all of them are mapped, as before, so raw reads of
audio sectors still work. When they do not fit (99 tracks need 198 slots at
one piece each), audio tracks are listed in the table of contents without
their files. A game that tried to read one of those sectors would stop on the
reader's `GD REQUEST REJECTED` screen. That is not expected: games read audio
tracks only to play them. The launch log says when it happens:
`Retail boot: audio tracks listed without their files to fit N tracks`.

With the background reader requested, its 64 slots are tried first, with
audio and then without. If the data tracks alone do not fit, the standard
reader is used, as before.

## Package format

The map travels in the launch package as `KUIRTI02` (version 2). The 320-byte
header is unchanged; the slots follow from byte 320 as 12-byte records. A
track's record holds its start, its end, a control byte, a piece-count byte
and two zero bytes; first pieces follow from the counts. K-UI and the stage
are built together, so no version 1 map is ever read.

## Tests

- `test-retail-image`: 99 tracks in all 160 slots, one piece or track more
  refused, 31 tracks in the background reader's 64, and unmapped audio refused
  before any card read.
- `test-retail-gd` (native, background and Windows CE builds): a 99-track
  table of contents, a seek, and position reports by track number.
- `tests/test_games_retail.py` (CI, FAT32 and exFAT): launches prepared with 31
  and 99 tracks, and the background reader with 31 tracks (audio mapped) and
  40 (audio listed only).
