# Armada Virtual VMU On: rebuilt reader with the movie audio fix

Virtual VMU On (CD Music On) loads `KUI/apps/games/ce-armada-all-on.kui`.
The owner's working build, `67b0df9469cc` (payload CRC32 `073826F5`), was
built by Chat from source that was never pushed; Chat's 8 Nov handoff says
so and keeps only a partial reconstruction of the combined CD music and
virtual A2 build (`recovered-work/02-partial-all-on-reconstruction.patch`).
The two background-reader changes that removed the intro echo on VMU Off
([armada-movie-timing-test.md](armada-movie-timing-test.md)) therefore
could not be applied to the VMU On reader directly.

## Rebuild (`8f35898a`)

`make -f Makefile.armada-cdda-vmu` builds the combined module from the
published source plus that reconstruction, corrected where the delivered
67b0df9 binary shows otherwise:

| Item | Reconstruction | Delivered 67b0df9 / rebuild |
| --- | --- | --- |
| A2 report rows | undefined address, rows at +0/+8 words | the hook's A2 log at `0xacfcff00`, rows at words 5 and 25 |
| Maple ownership check (`kui_armada_ce_maple_traffic_owned`) | VMU reservation only (`0x8cfc4000`) | from the CD music pool (`0x8cf50000`) to the reservation end |
| Card template | made after the layout record that hashes it | made first |

Compared with the 67b0df9 module (same build ID): the Maple hook, the
bridge, the SCIF/SCI/IDE residents and the card-profile template are
byte-identical; so are the stream bus guard (`_bus_valid`, `_low_code`) and
the direct-RAM guard (`_map_guest`, same address). The CD music controller,
the startup stage and the background reader differ: 67b0df9's controller
rewrote `_pump`, `_refill`, `_start` and the excerpt queue/attempt in
unrecovered source. The published source already coordinates card writes
with CD music (the reader's loan claims the audio transport, which returns
BUSY during the loan).

Audits: `check_armada_ce_excerpt.py --cdda-vmu` (low limit `0x8c00dc00`,
0x760-byte region without the spill; the CD-music-only boot-body proof is
reported as not established); the relaxed-call gate (324 sites, none
mid-function). `check_armada_ce_vmu_a2_package.py --cdda-vmu` and the
published 200-byte runtime shim are added, but the A2 audit's other runtime
pins predate the published write runtime and also fail on the unchanged
VMU-only build; that audit is not established here. Every existing CE
CD-music reader (Armada, Armada timing, Worms, Bust-A-Move 4, WWP) and the
VMU-only A2 modules rebuild byte-identical.

## Candidate (`a1aae09a`)

One behavior change on the rebuild: the slow refetch after an overrun and
the fast block check, on by default in this build
(`ARMADA_CDDA_VMU_READER_FIXES=0` rebuilds the restore-only reader).

| Reader | Build | Payload CRC32 | Bytes |
| --- | --- | --- | --- |
| Candidate (installed) | `a1aae09ade29` | `9C409C4B` | 205,308 |
| Restore-only (optional) | `8f35898a510d` | `D4F5A277` | 205,052 |
| Previous (optional) | `67b0df9469cc` | `073826F5` | 206,272 |

Candidate reader: 326 branch sites, none mid-function; memory ends at
`0x8c00d7a0`; worst masked stack 504 bytes. All three modules pass
`test-wince-packaged-prepare --armada` with the owner kernel (raw and
2048 DATA), and each has exactly one valid card-profile slot.

Package: `KUI-Armada-VMU-On-Audio-Fix-09NOV2026-0709-CST-a1aae09.zip`
(SHA-256 `75e17100a96d18ceb2ed56b18d34d68129ff796ee460315b7aeeb0df194273ae`):
the owner's current installation with only `ce-armada-all-on.kui` changed
(GUI `00d8ed67d9ad`, VMU Off reader `8cdbc0a351f8`), plus the two optional
readers in `IF-NEEDED/`, outside the KUI tree.

## Console test

Back up `KUI/vmu-test` first. CD Music On, Virtual VMU On: the intro
without echo, CD music in game, the existing save loads, a new save
survives a power cycle. If it fails, the previous reader restores the
working route; the restore-only reader separates the rebuild from the
fix. Not yet run on a Dreamcast.
