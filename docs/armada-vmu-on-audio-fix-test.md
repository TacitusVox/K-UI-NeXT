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

## First console result: GUI refusal, and the fix (`7827779d`)

The owner's GUI `00d8ed67d9ad` refused `a1aae09ade29` before launching:
"Armada all-on module capability is missing or invalid". The installed
GUI's admission code (disassembled; its source is not recovered) compares
the payload's 16 bytes at 0x150 with `KUIAVM01`, version 1 and `0x1000`
(after the CE package-layout check, which passed). The delivered 67b0df9
module carries exactly that record; the reconstruction omitted it.

The combined build's entry now emits it at `0x8c010150`, and the
`--cdda-vmu` stream audit checks it as the GUI does. The section is empty
elsewhere: the Armada, Armada timing and WWP readers, the VMU-only A2
module and the standard retail and retail-CE modules rebuild with
identical images. The other all-on refusals in that GUI ("no card profile
slot", "no complete CD music seed") concern structures already equal to
67b0df9's: one valid profile slot and the same blank `CEX2` stream seed.

| Reader | Build | Payload bytes | Payload CRC32 |
| --- | --- | --- | --- |
| Candidate (installed) | `7827779dd053` | 205,244 | `D52C4A40` |
| Without the reader fixes (optional) | `7827779dd053` | 204,988 | `F58CFC4C` |

Both pass `test-wince-packaged-prepare --armada` with the owner kernel.
Package `KUI-Armada-VMU-On-Audio-Fix-09NOV2026-0734-CST-7827779.zip`
(SHA-256 `7174c4659d637bf7ca0f96f564881aca111706f34ea0dde16b04cafc7711ebe5`)
supersedes the 0709 package; the console test steps are unchanged.

## Second console result: the rebuild fails; patch the delivered reader instead

Owner, `7827779dd053` (CD Music On, Virtual VMU On): the intro audio
repeated slightly and the video became a slideshow after about 20
seconds. The A+B+X+Y+Start report showed `BIND` 0 and `WORKER` 0, and the
following pages (the CD music history and the two A2 rows) read all
zeros: the virtual card's INFO count was zero, so the rebuilt module's
virtual A2 apparently never answered. The VMU Off reader in that package
was unchanged (`8cdbc0a351f8`).

The rebuild is not the delivered program: its CD music controller, stage
and background reader differ from 67b0df9 in about 1,900, 1,500 and 800
instruction slots respectively (lost source), and the reconstruction even
placed the CE data spill in uncached memory (`0xacfc0000`) where 67b0df9
uses cached (`0x8cfc0000`). `Makefile.armada-cdda-vmu` is therefore not a
delivery recipe.

Instead, `2116a813` patches the delivered module itself
(`tools/patch_armada_all_on_slow_refetch.py`): only its background reader
changes, in two spans at the same addresses as the published reader's
(`start_dma` at `0x8c009028`, rewritten to the same register writes plus
the 6.25 MHz window; the overrun path of `finish` at `0x8c00965c`, whose
two late diagnostic snapshots give way to the window's update). The
window is counted when a reception starts. The polled refetch after a fast
overrun stays (its condition does not fit in place; about 2% of the CPU
on the VMU Off measurements). 67b0df9's own block check is already a
full-table loop, so the faster check is not carried. `make
test-sh4-all-on-slow-refetch` runs the patch under qemu-sh4 at its
addresses, moved below `0x80000000` with its literals, and checks every
register write and window transition; 67b0df9's original `start_dma`
passes the same fast-path checks.

| Reader | Build | Payload bytes | Payload CRC32 |
| --- | --- | --- | --- |
| Candidate (installed) | `2116a813ada6` | 206,208 | `EEABEF94` |
| Previous (optional) | `67b0df9469cc` | 206,208 | `073826F5` |

Only the two spans and the five build ID strings differ from 67b0df9
(225 payload bytes). The GUI capability, the card-profile slot and the
blank `CEX2` seed are unchanged; `test-wince-packaged-prepare --armada`
passes with the owner kernel. Package
`KUI-Armada-VMU-On-Slow-Refetch-09NOV2026-1115-CST-2116a81.zip` (SHA-256
`cd282e78ccae72fac363a529cbc3293d6f6763c5273b4592f38fd5fcf165c6ea`).
Not yet run on a Dreamcast.

## Third console result: the patched delivered reader also fails

Owner, after `2116a813ada6` and its fallback (`67b0df9469cc`, in
`IF-NEEDED/`), in their words: "it keeps breaking everything ... the vmu off
route works PERFECT and the vmu on route is broken. If-needed also fails.
Wicked echo." No photograph. The fallback's echo is the delivered reader's
known fault. What the patched reader did is not recorded beyond that.

The patch itself was re-audited. No code outside the two spans branches
into them, loads a literal from them or holds a pointer to them: only the
two calls to `start_dma` (`0x8c009106`, `0x8c0096e6`) reach the new code. The
slow-state byte `s+75` has no other reader or writer. The handoff after every
reception (inlined in `finish`, `0x8c009596`) restores BRR=0 as the
published reader's does. The five replaced build-ID strings are display text
only. No structural fault was found. So with or without the VMU Off reader
changes, Virtual VMU On has a cost that VMU Off does not have, and two
attempts to carry the reader changes across without measuring it have failed.

## Re-think: measure Virtual VMU On first (`e73f99a9`)

VMU Off became clean only after its movie timing showed where the time went.
Virtual VMU On has never been measured. Beyond VMU Off, it runs the Maple
hook at every Maple completion (60 Hz): the PRE and POST helper phases,
with interrupts masked, its C code cached but its stack and workspace in
uncached P2. An earlier proof-only build of this hook stopped the video
outright ([armada-ce-cached-proof-candidate.md](armada-ce-cached-proof-candidate.md)).
The cached build was only ever compared with the old reader's baseline,
which already echoed.

`make -f Makefile.armada-cdda-vmu armada-cdda-vmu ARMADA_CDDA_VMU_TIMING=1`
builds the failing combined reader (the `7827779dd053` configuration, both
reader changes) with the VMU Off movie timing and the hook's own time. It
changes no behavior:

- the low reader's TMU0 totals and the controller's report rows, exactly as
  in the VMU Off timing readers;
- a wrapper around the helper's C entry adds each PRE/POST phase's TMU0
  ticks, the phase count and the longest phase to three words after the A2
  log (`0xacfcffc0`, inside the zeroed reservation);
- the sixth row becomes `HOOK-MS HOOK% HOOKS HOOKMAX-US HOT>50` over the
  same MOVIE2.SFD window, and the A2 page follows the timing page.

The reader region grows from `0x760` to `0x780` (one line for the timing
words), admitted only by `check_armada_ce_excerpt.py --cdda-vmu --timing`.
The reader passes the relaxed-call gate (326 sites, none mid-function) and
ends at `0x8c00d760`. Without the option, VMU Off `8cdbc0a351f8` and the
combined `7827779dd053` rebuild byte-identical to their delivered modules.

| Reader | Build | Payload bytes | Payload CRC32 |
| --- | --- | --- | --- |
| Measurement (installed) | `e73f99a93865` | 207,220 | `2BE09713` |
| Previous VMU On (optional) | `67b0df9469cc` | 206,208 | `073826F5` |

The GUI capability record, the card-profile slot and the blank `CEX2` seed
match `7827779dd053`, which the GUI admitted. `test-wince-packaged-prepare
--armada` passes with the owner kernel (raw and 2048 DATA). Package
`KUI-Armada-VMU-On-Timing-09NOV2026-1214-CST-e73f99a.zip` (SHA-256
`aa2cab460a4f6b3db3a0ddb57611ad084988125be74579915836cbe4c659a259`): the
owner's installation with only `ce-armada-all-on.kui` changed. Not yet run
on a Dreamcast.

Test: CD Music On, Virtual VMU On. Let the intro play, then A+B+X+Y+Start
(after about a minute if it becomes a slideshow), and photograph both pages.
Compare with VMU Off `8cdbc0a351f8` (third result in
[armada-movie-timing-test.md](armada-movie-timing-test.md)):

- **HOOK% large, or HOOKMAX-US in the hundreds or more:** the hook's own
  work, with interrupts masked, takes the time the movie needs. Cut the
  hook's per-completion cost next. The reader is not the problem.
- **BUSY% well above 17% with a small HOOK%:** the combined reader's own
  additions cost the time (for example the uncached P2 spill). Fix those
  next.
- **Both near VMU Off, but PEND% or GAPMAX high, or A2 READ counts during
  the intro:** card-storage loans or Maple waits stall the movie's reads.
  Look at the loan path next.
