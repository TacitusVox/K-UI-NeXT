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

## Measurement result: the Maple hook, not the reader (`e73f99a93865`)

Owner, 9 Nov 2026, US raw GDI, SCI, Background reader, CD Music On,
Virtual VMU On: "The video starts solid and progressively gets worse, with
the video becoming a slide show." Two photographs:

| Value | VMU Off `8cdbc0a351f8` | VMU On `e73f99a93865` |
| --- | --- | --- |
| Movie window | 119,525 ms; 30,328 sectors; 507 KB/s; 1,333 reads | 119,950 ms; 29,717 sectors; 495 KB/s; 1,148 reads; status 7 |
| GD calls / interrupt | 3,693 / 17,457 ms | 3,598 / 17,055 ms |
| BUSY / PEND / busiest second | 17% / 75% / 26% | 17% / 81% / 25% |
| DMA-OK / POLLED / OVERRUN / RESTART / CRC-ERR | 136,939 / 2,489 / 5,631 / 5,791 / 22 | 133,953 / 2,621 / 5,690 / 5,725 / 35 |
| TOKBYTES / YIELDS / REPAIR | 1,942,599 / 5,993 / 95 | 1,810,721 / 5,721 / 112 |
| OVRTICKS / MAXTICK / RUN-MS / GAPMAX | 2,749 / 14 / 300 / 525 | 2,961 / 12 / 325 / 75 |
| Maple hook (HOOK-MS / HOOK% / phases / longest) | none | 16,907 ms / 14% / 14,416 / 2,352 us |

GAME RETURN: guard 0, 0xD96D steps, 0x8D89 sectors. A2 page: INFO 1,
ALLINFO 0, MEDIA 1, READ 0x16 (22), FINALIZE 0x16 (22), no fault. SLOW-OK
and SLOWOVR read 0 because the controller was built without the reader
change it reads them under (fixed for later timing builds in `2e4f800f`); the
matching OVERRUN, RESTART and POLLED counts show the slow window ran.

The reader costs what it costs on VMU Off. The hook adds 14% of the movie
window: about 120 phases a second (PRE and POST at every Maple completion),
1.2 ms each on average and up to 2.35 ms, all with interrupts masked. Each
PRE phase ran the full capture (page-table walks, the extension's metadata
through cache-line snapshots, every list word twice), then both RX-slice
ownership queries, then the publisher's final query again, to set one
attachment bit in the controller's reply. The movie decoder gets 14% less
CPU and the reader's interrupt waits behind the masked phases (PEND 81%).
The 67b0df9 module has the same hook, so this also explains its echo.

## Candidate: repeat a proven controller poll (`cb07c645`)

After a full publication of a controller poll without an A2 request, a
completion that provably repeats it repeats only that publication: same
owner context and extension argument, MDSTAR the proven list, every list
word and the parent's reply and request function read again from RAM as
the DMA saw them; then the same MMIO admission and held fence, the same
final MDEN/MDST/MDSTAR and reply check, and the same single store with
read-back. Any difference, any A2 request, fault, diagnostic mode or
pending continuation, and every 64th poll take the full path, which
re-proves and replaces the template. Card requests (INFO, MEDIA, READ,
WRITE, FINALIZE) always take the full path. The earlier readonly console run
(`af18576dba18`) had 1,199 such polls in 1,200 completions.

`ARMADA_A2_STEADY=1` is the combined build's default. With it the helper
outgrew the 48 KiB image by 0x360 bytes, so the combined reservation starts
at `0x8cfc2000`. Below `0x8cfc4000` this layout used only the 2 KiB spill at
`0x8cfc0000`. The fixed P2 handoff, services, stacks and logs keep their
addresses. Proof-stack bound 1,272 of 1,448 bytes (call graphs, every
indirect site taking any admitted callback). `ARMADA_A2_STEADY=0` rebuilds
`7827779dd053` byte-identical; VMU Off `8cdbc0a351f8` is unchanged.

Built with the timing (`ARMADA_CDDA_VMU_TIMING=1`) so one run shows the
result. The sixth row is now `HOOK-MS HOOK% HOOKS HOOKMAX-US STEADY`, where
STEADY counts repeated polls in the window.

| Reader | Build | Payload bytes | Payload CRC32 |
| --- | --- | --- | --- |
| Candidate (installed) | `cb07c6454454` | 208,116 | `F86EA178` |
| Previous VMU On (optional) | `67b0df9469cc` | 206,208 | `073826F5` |

Capability record, card-profile slot and blank `CEX2` seed as in
`7827779dd053`; `test-wince-packaged-prepare --armada` passes with the
owner kernel (raw and 2048 DATA); relaxed-call gate 326 sites, none
mid-function; the reader still ends at `0x8c00d760`. Package
`KUI-Armada-VMU-On-Steady-Poll-09NOV2026-1553-CST-cb07c64.zip` (SHA-256
`bc5fbf3f4f421295120d87b6ace21f3439a7ec85678b1b7371e2f690f6d6eb6b`): the
owner's installation with only `ce-armada-all-on.kui` changed. Test: the
intro and its two report pages; then, only if the intro is good and after
backing up `KUI/vmu-test`, load a save, make a new one and load it after a
power cycle. Not yet run on a Dreamcast.

Expected: HOOK% about 1% (one full proof of about 2.3 ms per 64 polls plus
a short check on the others), STEADY close to 63/64 of the PRE phases
(about 7,000 over the intro), BUSY and the stream counts as on VMU Off, and
the movie as smooth as VMU Off.

- **HOOK% about 1% and a smooth movie:** the hook's per-poll proof was the
  cause. Next: the save/load and power-cycle check, then a build without
  the timing rows.
- **STEADY small and HOOK% still high:** Armada's polls do not repeat as
  the readonly run showed; find what differs between consecutive polls.
- **Smooth movie but the card is missing or a save fails:** the A2 page's
  fault row identifies the refused step.

## Scope across WinCE titles

Two separate costs, each shared differently:

- The reader changes (`KUI_CE_SCI_SLOW_AFTER_OVERRUN`,
  `KUI_CE_SCI_FAST_TAKE`) are in the common CE SCI stream reader, but only
  the Armada builds enable them. Worms, Bust-A-Move 4 and Worms World Party
  build the same reader without them; the owner reports Worms FMVs suffer
  the same way. Enabling them there (each reader must still fit its low
  layout) is the next shared change, checked with a Worms FMV run.
- The steady-state poll is in the CE Maple helper
  (`armada_ce_maple_traffic.c`), which the Smash Pack CE VMU helpers
  (Virtua Cop 2, Sega Swirl) compile too. It is enabled only for the
  combined Armada build until this console check passes. Native games use a
  different VMU implementation; this measurement says nothing about them.

## Console result: steady poll (`cb07c6454454`, 9 Nov 2026)

Same route and GUI. Owner: "Intro is mostly smooth. Maybe a tiny bit of
lag/reverb exists." Two photographs:

| Value | VMU Off `8cdbc0a351f8` | VMU On `e73f99a93865` | VMU On `cb07c6454454` |
| --- | --- | --- | --- |
| Movie window | 119,525 ms; 30,328 sectors; 507 KB/s; 1,333 reads | 119,950 ms; 29,717; 495 KB/s; 1,148 | 119,275 ms; 30,411; 509 KB/s; 1,352; status 7 |
| GD calls / interrupt | 3,693 / 17,457 ms | 3,598 / 17,055 ms | 3,851 / 17,724 ms |
| BUSY / PEND / busiest second | 17% / 75% / 26% | 17% / 81% / 25% | 18% / 75% / 27% |
| DMA-OK / POLLED / OVERRUN / RESTART / CRC-ERR | 136,939 / 2,489 / 5,631 / 5,791 / 22 | 133,953 / 2,621 / 5,690 / 5,725 / 35 | 137,145 / 2,675 / 5,976 / 6,149 / 25 |
| SLOW-OK / SLOWOVR | 98,979 / 2,504 | (not shown) | 95,463 / 2,689 |
| TOKBYTES / YIELDS / REPAIR | 1,942,599 / 5,993 / 95 | 1,810,721 / 5,721 / 112 | 1,962,746 / 6,145 / 95 |
| OVRTICKS / MAXTICK / RUN-MS / GAPMAX | 2,749 / 14 / 300 / 525 | 2,961 / 12 / 325 / 75 | 2,758 / 13 / 325 / 525 |
| Maple hook ms / % / phases / longest / steady | none | 16,907 / 14% / 14,416 / 2,352 us / - | 1,049 / 0% / 14,332 / 2,370 us / 7,056 |

GAME RETURN: guard 0, 0xDC23 steps, 0x903F sectors. A2 page: INFO 1,
ALLINFO 0, MEDIA 1, READ 0x16 (22), FINALIZE 0x16 (22), no fault.

The hook fell from 16.9 s to 1.05 s of the window (14% to under 1%). Of
7,166 completions, 7,056 repeated the proven poll; the other 110 are the
full proofs, one per 64 polls as designed. Those still take about 2.4 ms,
which is now HOOKMAX-US, about once a second. Every reader and stream value
is now within run-to-run variation of VMU Off, PEND is back to 75%, and the
card answered the same INFO, MEDIA and 22 READ/FINALIZE requests as before
without a fault. The remaining hook time is about 56 us per phase on
average, plus the periodic full proofs.

Established on the console: the Virtual VMU On intro with CD Music On, and
virtual-card detection and reads. Not yet established: CD music in game,
saving, and a save surviving a power cycle with this build (test 2 in its
package), the same without the timing rows, and the other WinCE titles.

## Console result: saves and CD music (`cb07c6454454`, 9 Nov 2026)

Owner, after test 2 of that package (load the existing save, save again,
power off, cold-boot, load): "Tested Armada's saves and it works fine. CD
Music also plays fine." With the intro result above, Armada with Virtual
VMU On and CD Music On now matches VMU Off on the console: the intro, CD
music in game, and virtual-card saves. That build still carries the
diagnostic timing rows.

The same report: "Worms stutters with CD music both on and off." Those runs
used the installed Worms modules, `ce-boot.kui` `3ebb874349dd` (CD Music
Off) and `ce-cdda-worms.kui` `eeed38bd540e` (CD Music On). Neither enables
the reader changes. The owner earlier described the Worms FMVs as failing
the same way as Armada's.

## WinCE reader rollout candidate (`7b216f2e1136`, 9 Nov 2026)

Behavior change: both reader switches are now on by default
(`KUI_CE_READER_FIXES ?= 1` in `Makefile.dc` and `Makefile.armada-stream`).
Every CE SCI background reader therefore does two things:

- after a 12.5 MHz overrun, it reads the next 64 receptions at 6.25 MHz;
- it folds each block's CRC and bit reversal into one pass.

Every block is still CRC-checked. The synchronous CE reader is unchanged.
`42f0f314` adds the generic module's optional 96-slot map and KUICEM01
record (both off by default) so that the installed generic module can be
rebuilt for GUI `00d8ed67d9ad`.

| Module | Previous | Payload / CRC32 | Reader end | Removed to fit |
| --- | --- | --- | --- | --- |
| `ce-boot.kui` (any title, CD Music Off) | `3ebb874349dd` | 84,836 / `04D83892` | `0x8c00d660` | live status meter |
| `ce-cdda-armada.kui` | `8cdbc0a351f8` | 135,092 / `E2B9F52B` | `0x8c00d7a0` | timing rows, history pages |
| `ce-cdda-bam4.kui` | `eeed38bd540e` | 135,076 / `45BFC073` | `0x8c00d7a0` | history pages |
| `ce-cdda-worms.kui` | `eeed38bd540e` | 136,004 / `4CD08180` | `0x8c00d7a0` | history pages |
| `ce-cdda-wwp.kui` | `eeed38bd540e` | 135,716 / `FAFCB488` | `0x8c00d7c0` | history pages |
| `ce-armada-all-on.kui` | `cb07c6454454` | 206,140 / `1E633670` | (combined) | timing rows |

The low reader limit is `0x8c00d800`: above it, the retail resident
overlaps its guarded service stack. Without the removals, the generic
reader and the CD music readers each ended 96 bytes past it.

- Every one of these readers drops the async call ring, which only the
  synchronous reader displays.
- The generic reader also drops its live status meter, which drew a status
  line from inside the masked reader every half second.
- The CD music readers keep the first terminal report page. They drop the
  history pages.
- The combined VMU On reader already carried both changes. Its new build
  only drops the timing rows that `cb07c6454454` showed.

`ce-boot.kui` is built with `KUI_RETAIL_CE_ASYNC_SLOTS=96
KUI_RETAIL_CE_MAP_RECORD=1`. Its KUICEM01 record (v1, 96, at payload
0x140) matches the installed module's. With the switches off, its reader is
byte-identical to the installed `3ebb874349dd` reader. Its stage is built
from this repository, because the source of the installed stage is not
here. The KUIAVM01 record (v1, 4096, at payload 0x150) is unchanged in the
VMU On module.

Host checks:

- the relaxed-call gate: 308 branch sites (generic), 322 (CD music) and 326
  (combined), none mid-function;
- `check_retail_loader_layout --ce`;
- `check_armada_ce_excerpt` `--stream` for Worms, BAM4 and WWP with
  `--reader-fixes`, and `--cdda-vmu`;
- `test-wince-packaged-prepare`: Armada CD music and VMU On with the owner
  kernel, and Worms and WWP with the synthetic fingerprint fixture (BAM4
  has no profile);
- `test-ce-fast-take`, `test-ce-slow-after-overrun`,
  `test-sh4-check-words` and `test-check-relaxed-calls`.

With `KUI_CE_READER_FIXES=0`, the installed `eeed38bd540e` Worms, BAM4 and
WWP modules and the published generic module rebuild byte-identically, and
`8cdbc0a351f8` and `7827779dd053` are unchanged. The Smash Pack CE modules
(`cc81e63ca4d2`) and `ce-probe.kui` are not rebuilt. The steady-state
poll stays enabled only in the combined Armada build.

The package is `KUI-WinCE-Reader-Fixes-09NOV2026-1633-CST-7b216f2.zip`
(SHA-256 `45acb5d2c3d46c4269952d68dd1664a67a8366694c6c8fa8e531e52350202807`).
It contains the owner's installation after the steady-poll package, with
only these six files changed. GUI `00d8ed67d9ad` is retained, and its
`boot.kui` and `runtime.kui` are identical. The six previous files are in
`IF-NEEDED/`.

The question for the console: Worms Armageddon FMVs with CD Music Off and
On, then a quick check of BAM4, WWP and other WinCE titles, and of Armada
with VMU Off and On. Not yet run on a Dreamcast.

## Console result: Worms Armageddon (`7b216f2e1136`, 9 Nov 2026)

Owner, with the rollout package and GUI `00d8ed67d9ad`: "Okay, tested Worms
Armageddon. Works with both CD Music on and off." Later in the same message:
"it runs fantastic."

| Route | Module | Payload CRC32 |
| --- | --- | --- |
| CD Music Off | `ce-boot.kui` `7b216f2e1136` | `04D83892` |
| CD Music On | `ce-cdda-worms.kui` `7b216f2e1136` | `4CD08180` |

Before this package, both routes stuttered on the installed `3ebb874349dd`
and `eeed38bd540e` modules. The owner did not report the on-screen build
line; the table lists the package's modules for those routes.

Not yet reported for `7b216f2e1136`:

- Bust-A-Move 4;
- Worms World Party;
- other titles on the generic module;
- Armada with VMU Off or On.

The owner also notes that Worms has no virtual VMU. That is expected, not a
regression. A WinCE virtual card has been built only for Armada and the
Smash Pack titles (Virtua Cop 2, Sega Swirl). Each one starts from an audit
of that game's own kernel Maple driver.
