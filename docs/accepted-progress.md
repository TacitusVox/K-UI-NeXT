# K-UI accepted progress

## RC1 updater and console checklist · 22 November 2026

The owner reported that the repaired RC1 GUI `93f13e84d989` passed the full Dreamcast hardware checklist. The checklist covered installing the complete update and restarting K-UI, cold booting the matching physical SD install, and checking native games, WinCE CD music, virtual saves and Sega Smash Pack handoffs. The repaired full package carries the GUI and the same 103 other managed files as the first RC1 candidate.

The owner specifically confirmed that **4x4 Evolution CD music works** on this build. The earlier report had established that the game launched and its menu WAV audio played. This adds a CD music result for the tested game. Other kernel variants retain their individual evidence in the [World Party group](wince-cdda-wwp-group-test.md) and [Bust-A-Move 4 group](wince-cdda-bam4-group-test.md) records.

## K-UI 3.0 RC1 generic WinCE reader · 22 November 2026

The owner promoted the everyday generic reader `ce-boot.kui` build `91b0540103f0` for RC1 and subsequent full nightlies. It supersedes the previous generic acceptance at `7b216f2e1136`. This build enables video-memory transfers and uses the adaptive 16-reception slow window with two doublings. RAM pooling and read-ahead are disabled. The file is 85,300 bytes with payload CRC32 `6829D405` and SHA-256 `a75e0ce9010708953b707aad5db975bb83f9ee20f5657ddf4fdea8cf014acabd`.

The five original CD music roles retain their accepted `1ba2fa7e7040` payloads. Ducati World and Virtua Cop 2 Japan keep their own fixes. The distribution manifest pins each component at its individual build.

## Standing delivery rule — 2026-11-06

The owner uses manual-copy ZIPs as the default delivery workflow. This persists
across sessions and supersedes historical wording that treats ZIP delivery as
a one-off exception or instructs future tests to use KUPs. KUP delivery or
updater-engine repair requires a separate explicit request. A GUI ZIP includes
both identical physical aliases; normal ZIP delivery excludes activation
records, banks and user data. The current Games/boot implementation is unchanged
by this documentation correction.

## Current console checkpoint — f8807cb16561

The delivered `KUI-Shared-Game-Launch-06NOV2026-2210-UTC-f8807cb.zip`
(SHA256 `dc607c3e89ab6bac5c718b738bca9990ffd85ec39abb308140de280c4a7503e9`)
remains the unchanged native timing candidate. Host/linked qualification is
recorded under `evidence/native-shared-f8807cb`. The owner's Evolution 2 capture
now measures 21.704 seconds before handoff on Background New, VMU On, actual
module payload CRC `9B37B199`; the owner reports another approximately 35 seconds
inside bootstrap, about 55 seconds total. The latter interval has no phase
capture. See [the measured baseline and shared preflight gap](evolution2-f8807cb-measurement.md).
This is a slow console baseline, not performance acceptance. Sonic live-IRQ
acceptance and a same-configuration repeat comparison remain unavailable.
Capture GUI/module identity and both launch phases with usual matching options;
retain the automatically generated launch record. Existing operation-count
improvements must not be presented as measured console speed-ups.

Recipe note of 16 November 2026: the delivered module is reproduced by
`Makefile.vmu-native-background-compact` with this label at the candidate's own
source (public commit `e79da8ce`) alone. On the source that carries the Toy
test sources (imported by `f216fac` and `e1104f4`) the same recipe gives the
candidate's worker, and low residents and a block-cache auxiliary that are not
the candidate's (same sizes; 390, 6 and 3,074 bytes differ). The cold stage
differs already by Main's own work between the candidate and those imports. Do
not build the New reader from that source for delivery before this is resolved.
The trace is in `evidence/toy-wire-stream-82/installed-recipe-on-this-source.log`
and the item in the owed list of `fmv-stream-reader-plan.md`.

The subsequent [read-only review](review-06nov2026.md) confirms that all four
delivered CE/CDDA modules already contain the VRAM-copy and folded-CRC changes;
ordinary `ce-probe.kui` remains omitted. It also confirms that menu Restart
may select a stale bank, while cold ROOT boot bypasses selectors. The physical
card is unavailable, so existing banks are not assumed present or absent.
Claude's original-backed SA1 result is recorded as an external report only.

## Large programs K-UI cannot check are refused: console test pending, aaaba0ea3640, 2026-11-27

Delivered: `KUI-Background-Large-Games-27NOV2026-0918-CST-aaaba0e.zip`, GUI build `aaaba0ea3640` (commit `aaaba0ea`), GUI only
([record](background-launch-checks.md)). It is `53a33ec708ab` with two
launcher changes the owner asked for on 27 November: a large program K-UI
cannot hold, and so cannot check, is no longer started with the Background
reader on (refused as "too large for the Background reader yet", with the
setting that starts it), and a refused launch's reason is logged again
after the launch timing, where it can be read.

Built with the release script; of 888 objects `games_retail.o` and
`main-runtime.o` differ from `53a33ec708ab`. No reader module changed and no
reader input. Host checks only: the launcher suites, 8 mutations, and the
production preflight on the 426 originals against `53a33ec708ab` launch by
launch: with the console's startup memory only Evil Dead - Hail to the
King, Incoming and NBA Hoopz answer differently (refused instead of started
unchecked); with little memory free also Mortal Kombat Gold, Mat Hoffman's
Pro BMX, Aero Dancing i, L.O.L., Expendable and Millennium Soldier.

Not accepted by this delivery: nothing of it has run on a console. The
large games themselves are deferred by the owner ("We can come back to the
big games later"); they start with Background loading Off.

## A program too large for K-UI to hold: console-confirmed for Plasma Sword, 53a33ec708ab, 2026-11-27

Console result of `53a33ec708ab`, 27 November 2026 (the owner's saved
diagnostics log, which names the build): Plasma Sword with Background
loading on is refused by the large-program check, with the setting, and
does not reach the reader's stop screen. The one read in pieces took 12.3 s.
Tokyo Xtreme Racer and ChuChu Rocket were not reported under this build.


Delivered: `KUI-Background-Large-Programs-27NOV2026-0252-CST-53a33ec.zip`, GUI build `53a33ec708ab` (commit `53a33ec7`), GUI only
([record](background-launch-checks.md)). It is `d41a04f40f23` with one
launcher change and its review corrections: a program K-UI cannot hold in
memory is searched, while it is read once for its CRC, for the code the
Background reader's SDK check needs, and the launch is refused where that
code is absent (Plasma Sword, Star Gladiator 2 and, when it cannot be held,
Shenmue, of the 426 programs).

Console result of `d41a04f40f23`, 27 November 2026 (one photograph, no log):

- Tokyo Xtreme Racer boots with Background loading on. First console result
  for the pool rule; the route (the reader-only module, Virtual VMU Off) is
  inferred, and only the boot was reported. That module records no survey.
- Plasma Sword still ended on the reader's stop screen. K-UI could not hold
  its 9.07 MiB program (it has under 8 MiB free on the console), gave no
  answer and started the module. The host checks of `d41a04f40f23` had no
  memory limit and reported a refusal the console does not give.

Built with the release script; of 888 objects only `games_retail.o` differs
from `d41a04f40f23` and from a build of Main. No reader module changed and no
reader input. Host checks only: unit and launcher tests with the allocation
refused, the production preflight on the 426 originals under the console's
two logged memory limits, 45 mutations, an independent review.

Not accepted by this delivery, and not changed by it:

- A large program that has the reader's code still gets no answer and is
  started unchecked (6 to 16 of the 426, among them Evil Dead, Incoming and
  NBA Hoopz). With Virtual VMU Off the reader runs its own check; by the
  host check it refuses all of them.
- With Virtual VMU On every launch without K-UI's answer stops in the reader
  at storage start, for want of a prepared card. By reading and a host run
  of the stage's admission; not seen on a console. Whether K-UI should
  refuse these launches is the owner's decision.
- A program K-UI cannot hold never gets the reader-only module.

## Two checks before a Background launch: on the console 27 November (one of two confirmed), d41a04f40f23, 2026-11-26

Delivered: `KUI-Background-Launch-Checks-26NOV2026-2356-CST-d41a04f.zip`, GUI build `d41a04f40f23` (commit `d41a04f4`), GUI only
([record](background-launch-checks.md)). Its source is Main `3f757bc2` (the
3.0 RC1 source; no K-UI source changed on Main since the RC1 build commit
`93f13e84`) with four launcher commits. Two are the changes the owner's
handoff of 26 November decided:

- the combined Background module is not offered where its lease would take
  more than a quarter of the game's SDK pool (Tokyo Xtreme Racer and eight
  other programs of the 426);
- a Background launch whose executable K-UI's own SDK check does not
  recognize is refused before the launch, with the setting that starts the
  game (Plasma Sword and 60 others; under Legacy (Y) 243 more, which New (X)
  runs with the reader-only module as before).

Two are corrections from an independent review: every refusal names one
whole setting that starts the game, and a combined module is seeded only
with the image's own saved mapping, so that neither check can be passed by
through another game's record.

Built with the release script; of 888 objects only `games_retail.o` differs
from a build of Main. No reader module changed and no reader input: the
distribution fingerprint is unchanged. Host checks only. Until the owner
reports, `64ae07435de5` stays the reference for the regular games on the
test card, and the 3.0 RC1 K-UI `93f13e84d989` for everything this K-UI
shares with it.

Not accepted by this delivery: Tokyo Xtreme Racer through the reader-only
New module (untested), the Standard reader's virtual card for the nine
small-pool programs (unchanged, not examined, untested), and the cost of
the first checked launch of each image with Virtual VMU On (not measured).

## One K-UI for both threads: on the owner's test card since 22 November, 64ae07435de5, 2026-11-21

Delivered: `KUI-One-KUI-Both-Threads-21NOV2026-2308-CST-64ae074.zip`, GUI build `64ae07435de5` (commit `64ae0743`), GUI only
([record](one-kui-both-threads.md)). Its source merges Main `4441e92c`, the
Windows CE line to `b7c92e0c` (the source of `1c71ba3afa17`, then CD music
round 2 and Virtua Cop 2's bootstrap-2 repair) and the regular games' line
(the owner's Test 83 GUI line `4d4dcff77494` and reader line
`1d80f771f081`, Tests 83 and 84 behind their switches).

It is built as `1c71ba3afa17` is (that recipe rebuilds the owner's file byte
for byte): the owner's Standard binding `9e3295bcfaec`, the Smash bundle
`cc81e63ca4d2` (identical to the owner's), launch timing, nightly off. It
is not built as `4d4dcff77494` was (owner-source README: nightly
`build_gui` with `KUI_GITHUB_NIGHTLY=1`, the default preserved Standard
manifest, no bundle). Native routes decide as `4d4dcff77494` (160-slot
Background map, standalone New for overlapping SDK pools, cached
mappings); Windows CE routes decide as `1c71ba3afa17`, with one text
changed: a CE map-full refusal uses the regular line's wording. Every
delivered module of both lines rebuilds byte-identical from `64ae0743`.

State of the regular games' line: the owner's reader `1d80f771f081`
(`313ac3bd...5f1c`) with the GUI line `4d4dcff77494` ran on the console in
Test 83 (20 Nov 2026), a test configuration, not accepted components
([Test 83](toy-wire-stream-test-83.md)). Test 84's module `4abc63b0cc58`
(`597d8eac...`) ran on 21 Nov under `1c71ba3afa17`, whose 64-slot map
refused three games ([Test 84](cdda-background-test-84.md)). Its re-run
under this K-UI came on 22 November and the controls on 26 November (the
same document, Console result): the owner's logs name `64ae07435de5`, the
160-slot map admits Bangai-O, MDK2 and Omikron, and the launches decide as
recorded there. Test 85's module `b9cff7086258` (the CD music heard) ran
on the console on 27 November 2026: POD SpeedZone played its music, ChuChu
Rocket and KOF Dream Match 1999 stayed silent for want of a place for the
ring in sound memory ([Test 85](cdda-background-test-85.md), Console
result). Test 86's module `0af5d9ce889c` (the ring as ADPCM in the sound
driver's own spare memory) was delivered the same day: host- and
linked-qualified, a test configuration, with no console result yet
([Test 86](cdda-background-test-86.md)). Neither is an accepted
component. The owner has reported no Windows CE or Smash Pack run
under it; for those `1c71ba3afa17` and the 3.0 RC1 K-UI `93f13e84d989`
remain the references.

## Repository GUI with the installed WinCE routes: console test pending, 069d612cadea, 2026-11-17

The owner's GUI `00d8ed67d9ad` was built from source in no repository. Its
Windows CE routes are now in this repository ([routing record](wince-gui-routing.md)):
the generic `ce-boot.kui` with its map record, Armada's legacy `ce-probe.kui`
and the all-on route with its dedicated card. Built as `00d8ed67d9ad` is,
3,408 of 3,412 functions are byte-identical to it.

Delivered: `KUI-WinCE-GUI-Routes-16NOV2026-1928-CST-069d612.zip`, GUI build
`069d612cadea`, GUI only. It binds the owner's Standard module
`retail-boot.kui` `9e3295bcfaec`, as `00d8ed67d9ad` does. `00d8ed67d9ad`
remains the reference until the owner reports the package's four tests.

## WinCE CD music for Bust-A-Move 4's group: Puzzle Bobble 4 and Virtua Cop 2 (JPN) console-tested, 9e89e778370f, 1ac7e667c0d3 and 949be40865cf, 2026-11-22

**Result (21 Nov 2026, [record](wince-cdda-bam4-group-test.md#result-21-nov-2026)):**
- **Puzzle Bobble 4:** works with CD music On.
- **Virtua Cop 2 (JPN):** stopped in bootstrap 2 with BOOTSTRAP ALTERED
  STREAM SOURCE MAP.
  - Cause: its IP.BIN runs Sega's play-history recorder with its stack at
    the stream map's end.
  - Fix: test build `1ac7e667c0d3` (source `e8b3f744` and `1ac7e667`)
    reinstalls the map and work pages after that bootstrap, for that
    IP.BIN only.
  - A stage copy that fails its proof stops on BOOTSTRAP 2 SCRATCH COPY
    REFUSED.
  - Every other module is byte-identical. Virtua Cop 2's, with the switch
    off, rebuilds as `9e89e778370f`.
  - Delivered: `KUI-WinCE-CD-Music-Virtua-Cop-2-Fix-21NOV2026-2154-CST-1ac7e66.zip`.
  - Console result (21 Nov 2026): the repair works. Windows CE starts and
    the game passes its memory-card check. The reader then stops on GD
    function 6, a stream piece into video memory, which only the generic
    module takes (`KUI_CE_VRAM_DMA`).
  - Test build `949be40865cf` (source `0414c2d0` and `949be408`; review
    follow-up `1ac6f424`, no module change): Virtua Cop 2's CD music reader
    takes those pieces, with the Armada all-on reader's 1 KiB stack for room.
    - Every other module is byte-identical.
    - The GUI sources of K-UI `1c71ba3afa17` and `64ae07435de5` admit it
      with the owner's kernel.
    - Delivered:
      `KUI-WinCE-CD-Music-Virtua-Cop-2-Video-Memory-22NOV2026-0004-CST-949be40.zip`,
      with the previous module and a layout-only control under
      `IF-NEEDED/`.
    - Console result (22 Nov 2026): Virtua Cop 2 ran perfectly with CD
      music On (owner). Accepted: `ce-cdda-6c09543a5680.kui`
      `949be40865cf`, payload CRC `C1FEC7A6`
      ([result](wince-cdda-bam4-group-test.md#console-result-of-949be40865cf-22-nov-2026)).
- **Not tested:** the other five games.

CD music On for seven more WinCE kernels that carry Bust-A-Move 4's sound
system unchanged ([record](wince-cdda-bam4-group-test.md)):
- Puzzle Bobble 4;
- Virtua Cop 2 (JPN);
- Caesars Palace 2000 (US and PAL);
- Get!! Colonies;
- Marionette Company 2;
- Hello Kitty: Magical Block.

The method is round 1's. Each kernel's record is derived from the kernel
itself (`tools/wince_cdda_bam4_kernels.py`). Each module carries a
`KUICDK01` record of sound profile 2, which GUI `1c71ba3afa17` already admits
(source `9e89e778`).

Rebuilt from that tree, all byte-identical:
- the five accepted CD music modules (`1ba2fa7e7040`);
- the ten other round-1 modules (`1c71ba3afa17`);
- Ducati's test module (`d7fd810eb837`);
- the GUI `1c71ba3afa17`.

Super Runabout is excluded. It plays its sound through `MANATEE.DRV`
without DirectSound.

Delivered: `KUI-WinCE-CD-Music-BAM4-Group-21NOV2026-2037-CST-9e89e77.zip`,
the seven modules only.

## WinCE CD music: Ducati World's repeat count, console-tested, d7fd810eb837, 2026-11-21

**Result (21 Nov 2026, [record](wince-cdda-ducati-repeat-test.md#result-21-nov-2026)):**
- The first race plays its CD track: the PLAY was accepted and started
  once, with no refusal, recovery or fault.
- The owner found the music itself a ten-second loop, judged it the game's
  own, and asked not to pursue it.
- A second race, which is expected to stay silent, was not reported. The
  player rebind it would need is not pursued for now.


Ducati World asks for 1,000 repeats of its race track. The stream model
refused any count above 15, so the game was silent in round 1. Source
`d7fd810e` saturates such a count to 15 (endless), only in Ducati World's
module ([record](wince-cdda-ducati-repeat-test.md)). Rebuilt from that tree:
- the five accepted CD music modules are byte-identical to `1ba2fa7e7040`;
- the ten other round-1 modules are byte-identical to `1c71ba3afa17`;
- Ducati's module with the switch off is byte-identical to `1c71ba3afa17`.

Known limit: the player binds one process per console session, and Ducati
runs each race in a new process, so only the first race can play.

Delivered: `KUI-WinCE-Ducati-Music-Test-21NOV2026-1929-CST-d7fd810.zip`, with:
- Ducati's module `d7fd810eb837` (payload CRC `3FB89B1C`);
- the everyday candidate `ce-boot.kui` `91b0540103f0`, in place of the
  read-ahead test build still on the owner's card;
- round 1's module in `IF-NEEDED/`.

The GUI stays `1c71ba3afa17`.

## WinCE CD music for World Party's group: console result, 1c71ba3afa17, 2026-11-21

**Result (21 Nov 2026,
[record](wince-cdda-wwp-group-test.md#result-21-nov-2026)):**
- Railroad Tycoon II plays.
- 4x4 Evolution's menus play the game's own WAV files, and its race music is
  not yet observed.
- Ducati World was silent: the model refused its repeat count of 1,000
  ([test](wince-cdda-ducati-repeat-test.md)).
- With CD music Off, 4x4 Evolution crashed after its intro was skipped, on
  the read-ahead test build `1a5f46776c0c` still installed as `ce-boot.kui`.
  On the candidate `91b0540103f0` it runs, with CD music Off and On.
- 4x4's occasional video lag is the same with CD music Off, so it is the
  reader's, not CD music's.
- Ducati World plays its first race with `d7fd810eb837`.

CD music On for eleven more WinCE kernels that carry World Party's sound
system unchanged ([record](wince-cdda-wwp-group-test.md)): Ducati World,
Championship Surfer, The Next Tetris Online, Railroad Tycoon II, 4x4
Evolution, Starlancer, Hidden and Dangerous (US, PAL), Atari Anniversary,
Hoyle Casino and Who Wants to Beat Up a Millionaire. Each kernel has its own
module `ce-cdda-<first 12 hex digits of its SHA-256>.kui`, built from the
accepted recipe with that kernel's derived record (source `2f19afa2`), and the
GUI admits such a module by its `KUICDK01` record (source `1c71ba3a`). The five
accepted CD music modules rebuild byte-identical to `1ba2fa7e7040`.

Delivered: `KUI-WinCE-CD-Music-World-Party-Group-21NOV2026-1550-CST-1c71ba3.zip`,
the GUI `1c71ba3afa17` (Main `4441e92c` with the route, the owner's Standard
binding `9e3295bcfaec`, the Smash bundle, launch timing) and the eleven
modules; the reference GUI `00d8ed67d9ad` in `IF-NEEDED/`. Until the owner's
report, the CD music set `1ba2fa7e7040` and the GUI `00d8ed67d9ad` stay the
references.

## WinCE CD music set accepted: 1ba2fa7e7040, 2026-11-21

**Accepted (21 Nov 2026, owner):** smooth gameplay with CD music On in Armada
and in the other CD music games; Armada's music no longer plainly repeats (one
abrupt switch back to the first song was the game's own PLAY command; the GD
service keeps no play state). The measured Armada run: worker time 1.8% of
play (9.6% before), 10 worker steps of 8 ms or more in 244 s (236 in 105 s
before), the busiest second as with the music silent, and the music's fresh
PCM never below 566 ms ([result](wince-cdda-chain-test.md#measured-run-21-nov-2026)).
The five modules on the card (`830BD7EA`, `23A93E11`, `0074D23C`, `F219336E`,
`8C21F910`) replace `7b216f2e1136` as the accepted CD music modules. The
makefiles now build them by default; the previous set rebuilds byte-identical
with `KUI_CE_SLOW_WINDOW=64 KUI_CE_SLOW_ADAPT=0 KUI_CE_CDDA_EMERGENCY_PIO=0
KUI_CE_CDDA_CHAIN=0` ([the pin](wince-cdda-chain-test.md#the-pin)).


On top of the worker fix below (smooth gameplay, but Armada's music
repeated): the music's card stream restarts where it stops
([record](wince-cdda-chain-test.md)). Source `1ba2fa7e` adds
`KUI_CE_CDDA_CHAIN` (controller only): a restart's token search goes on for up
to eight more slices inside the interrupt or GD call that needs it, and a fast
overrun is refetched by DMA at 6.25 MHz while the reader's slow window is
armed. Default 0: the five CD music modules rebuild byte-identical to
`7b216f2e1136`, and with the emergency switch alone to `06341c5a3681`. Test
build `1ba2fa7e7040` (adaptive window, emergency and chain): payload CRCs
`830BD7EA`, `23A93E11`, `0074D23C`, `F219336E`, `8C21F910`, and the Armada
gameplay timing `A2F1FE09`. Delivered as
`KUI-WinCE-CD-Music-Chain-Fix-21NOV2026-1415-CST-1ba2fa7.zip` (SHA256
`b932e05403cd410446097d1d2b9c6311e1302fd541c9712d547bde164f8ec134`) with the
`52a946122537` set in `IF-NEEDED/`. Gates pass; stack worst cases unchanged.

## WinCE CD music worker fix: console-tested, music repeats in Armada, 06341c5a3681, 2026-11-21

**Result (21 Nov 2026):** Armada: smooth gameplay, but its music repeated;
Worms: fine. No model underrun (GAPS 0): CE mixes 155 ms ahead of the play
cursor, and without the worker's programmed receptions the music's DMA chain
stopped at each restart inside an interrupt or GD call until the worker's next
visit ([result](wince-cdda-emergency-pio-test.md#result-21-nov-2026)).


The fix for the CD music On gameplay lag ([record](wince-cdda-emergency-pio-test.md)).
Source `06341c5a` adds `KUI_CE_CDDA_EMERGENCY_PIO` (Makefile.armada-stream,
Makefile.armada-vmu-a2; controller only): the music worker receives by
programmed transfers only while the music plays with at most half a second of
fresh PCM (until one second), not whenever it is urgent; DMA otherwise,
prefill included. Default 0: the five CD music modules rebuild byte-identical
to `7b216f2e1136`, the adaptive set to `52a946122537`. Test build
`06341c5a3681`: the five with the adaptive window and the change (payload
CRCs `91A2A1AE`, `69175F62`, `D841F413`, `D87C83F5`, `EE540C64`), and the
Armada gameplay timing with the change (`C9D6F9C6`). Delivered as
`KUI-WinCE-CD-Music-Worker-Fix-21NOV2026-1345-CST-06341c5.zip` (SHA256
`c80bb79a35fe104ae401b768b075ffd2900c6af26de6c864f2c82f85e04ec9f9`) with the
card's `52a946122537` set in `IF-NEEDED/`. Gates pass; stack worst cases
unchanged. `52a946122537` stays the card's set until the owner reports.

## Armada CD music On gameplay timing: console-tested, cause found, 32b3bbb33e39, 2026-11-21

**Result (21 Nov 2026):** run 1 (music) lagged; run 2 (the music never
queued) had no lag. K-UI took 15% of gameplay with the music, 2% without; the
worker 10.1 s of 105 s, 236 of its 1,437 steps 8 ms or longer (239 ended
urgent), the longest 173 ms; 8,530 of the music's 27,371 card blocks were
programmed receptions. The game read the same 7,212 sectors in both runs, for
the same time. The cause is the worker's programmed receptions while the music
is urgent ([result](wince-cdda-play-timing-test.md#result-21-nov-2026)).


A measurement of where CD music On costs Armada's gameplay, not a fix
([record](wince-cdda-play-timing-test.md)). Source `32b3bbb3` adds
`KUI_ARMADA_CE_PLAY_TIMING` to Makefile.armada-stream. With the default (0)
the five CD music modules rebuild byte-identical to `7b216f2e1136`, and the
movie timing build to its earlier bytes. Two modules of build `32b3bbb33e39`
on the movie timing reader (fixed 64-reception window, low reader
byte-identical): run 1 with the music (payload CRC `9E73BA00`), run 2 the same
with the music never queued (`E9655CBD`). Their six report rows cover the
window after the intro movie: K-UI's time with the music worker's, the game's
64 KiB reads with and without music grants while they ran, the transport's
counts and the worker's steps. Delivered as
`KUI-Armada-CD-Music-Play-Timing-21NOV2026-1304-CST-32b3bbb.zip` (SHA256
`69efd1fee70345c96cb31fbde2eec809d67b5cf2a92d0fd42f510eca0de9065b`) with the
card's `52a946122537` module in `IF-NEEDED/`. Route: Armada (US), CD music On,
Virtual VMU Off. Gates: the stream excerpt, layout and relaxed-call checks;
stack worst case 656 of 2,000 bytes. Host: `test-armada-ce-play-timing`.

## WinCE CD music readers, adaptive slow window: console-tested, not at parity, 52a946122537, 2026-11-21

**Result (21 Nov 2026, owner):** with CD music On, Armada's intro keeps a tiny
lag and its gameplay lag returns (VMU On: intro lag gone, the same minor
gameplay lag); Bust-A-Move 4 okay, maybe a tiny lag; both Worms games a tiny
gameplay lag, movies fine. Gameplay is a tiny bit worse with CD music On than
Off, so CD music On stays opt-in. The owner then reported that the whole set
ran (they copy each ZIP's whole `KUI` folder) and plays about the same as the
installed `7b216f2e1136` set, with the movies fixed ([result](wince-cdda-adaptive-test.md#result-21-nov-2026)).


The generic module's adaptive slow window (16 receptions doubling to 64,
console-run with CD music Off) in the five CD music modules: `ce-cdda-armada`,
`-bam4`, `-worms`, `-wwp` and `ce-armada-all-on` ([record](wince-cdda-adaptive-test.md)).
Source `52a94612` adds the switches to Makefile.armada-stream and
Makefile.armada-vmu-a2; the defaults rebuild all five accepted modules
byte-identical to `7b216f2e1136`. Test build `52a946122537` (`KUI_CE_SLOW_WINDOW=16
KUI_CE_SLOW_ADAPT=2`), delivered as
`KUI-WinCE-CD-Music-Adaptive-Test-21NOV2026-0852-CST-52a9461.zip` (SHA256
`79b257caf083395542c66362a454736cde01c1c0f50c6eec8da1a94eba24b4ee`) with the
installed five in `IF-NEEDED/`. Gates: the four readers end exactly at their
`0x8c00d800` limit and the combined one at `0x8c00d7e0`; regions unchanged
(`0xf60`, `0x760`); stack worst cases 512 to 516 of 2,000 and 504 of 976; the
stream excerpt, layout and relaxed-call checks pass for each. Host:
`test-ce-cdda-reader`, 50,229 checks in each configuration. `7b216f2e1136`
remains accepted until the owner reports CD music On for these games.

## WinCE RAM pool and read-ahead: console-tested, read-ahead not adopted, two builds of 1a5f46776c0c, 2026-11-21

**Test 2 result (21 Nov 2026):** against the candidate `91b0540103f0`, Maximum
Pool the same, RE2 the same, Super Producers about the same (owner); the
owner ended the test there. The rings show read-ahead's condition met in
those streams (Super Producers 98% exactly sequential), so it either ran
without a visible gain or did not run; no measurement build carries it.
Read-ahead is not adopted and stays off by default; the pool stays for CD
music and the VMU; `91b0540103f0` remains the everyday candidate
([result](wince-pool-readahead-test.md#result-of-test-2-21-nov-2026)).

**Test 1 result (21 Nov 2026):** seven launches on seven kernels (two CE 2,
five CE 3, one of them outside the 19 surveyed), each played until the owner
ended it, no failed request, no refused command. The pool was reserved on
all seven; each running kernel kept the lowered RAM end, and the pool's 16
pages stayed untouched where CE had used them without the pool. The streamed
reads match the adaptive session's ([result](wince-pool-readahead-test.md#result-of-test-1-21-nov-2026),
[evidence](evidence/wince-pool-session-2026-11-21.json)).


Two behaviour changes to the generic `ce-boot.kui`, off by default, each with
its own test build ([tests](wince-pool-readahead-test.md),
[design](wince-unified-runtime.md)):

- **Universal RAM pool** (`c6745ab2`, Makefile.dc `KUI_CE_POOL_KIB`): the stage
  finds CE's ROM header by its structure and lowers its RAM end by the pool
  before NK runs; the relay proves the pool and the patched word survived
  bootstrap 2. Test build `1a5f46776c0c`: the adaptive timing probe with a
  64 KiB pool, unused (`KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16
  KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64`), 93,816 bytes, payload CRC
  `FCE53735`, SHA-256 `ac6600b1ca16e046efc365b6719b85e86cee9d0157b7c8edba2c8aaaed95e446`. Delivered:
  `KUI-WinCE-Pool-Timing-Test-21NOV2026-0006-CST-1a5f467.zip` (SHA256 `0099aa561f62f04be3176eec5b0efe64726a9b08d1453a0f828a31e5c28b1ce4`), with a blank 64-slot
  `KUI/WCEDATA/WCEDATA.BIN` (file id `bcd16a2e664c9574b66cfcc07c0e384c`), both restore modules
  in `IF-NEEDED/` and the PC tools.
- **Read-ahead** (`1a5f4677`, `KUI_CE_READAHEAD`): after two sequential reads
  the background reader streams on into the pool while no request is active;
  the next read takes it from there. Test build `1a5f46776c0c`: the candidate
  `91b0540103f0`'s reader with the pool and read-ahead (`KUI_CE_SLOW_WINDOW=16
  KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64 KUI_CE_READAHEAD=1`), 86,884
  bytes, payload CRC `2C8248D8`, SHA-256 `2c6bf8d2cbebc984bf3b1a470f0ba8aa6fc80eeed3f4d900781eed3016cc4958`. Delivered: `KUI-WinCE-ReadAhead-Test-21NOV2026-0006-CST-1a5f467.zip`
  (SHA256 `ca7c2205ea4c6c6bd870211fc5bfaef3371c5232ece9a852e8d8b9878afeed6d`), with both restore modules in `IF-NEEDED/`.

Gates on the exact bytes: the GUI's seven production admission cases on FAT32
and exFAT for each module; the PVR write-width check on both residents of
each; the layout checker (read-ahead resident ends at `0x8c00da40`, limit
`0x8c00dc00`, stack worst case 416 of 976 bytes; the probe's resident is the
adaptive probe's). Every delivered module rebuilds byte-identical from
`1a5f4677` with its own switches: `ce-boot` `7b216f2e1136`, the data, VRAM,
timing and window-16 probes, the candidates `41e3450bc94d` and `91b0540103f0`,
the four `ce-cdda` modules and `ce-armada-all-on`. Host suite at
`1a5f4677`: every C suite and script passes; the unittest gaps are the known environment ones (`test_asm_audit`'s console macro, `test_cover_png_rows`, `test_internet_preview`, `test_wince_release_package` without `.deps/bearssl`), and `test_splash_assets`' known stall was run around, as before.

Not answered: read-ahead's timings (no measurement build has room for it), CD
music On and VMU On (their modules have neither change), kernels not yet run.
`7b216f2e1136` remains the accepted generic module and `91b0540103f0` the
pending everyday candidate.

## WinCE reader candidate 2: console test pending, 91b0540103f0, 2026-11-21

The generic `ce-boot.kui` for everyday use with both console-tested changes:
PVR stream pieces (`a660354d804c`) and the adaptive slow window
(`871c4c7784b4`). Recipe: the generic build at `91b0540103f0` with
`KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2` (`KUI_CE_VRAM_DMA=1` is that build's
default); its sources are `871c4c77`'s (the later commits are documents).
Delivered: `KUI-WinCE-Reader-Candidate-20NOV2026-2208-CST-91b0540.zip` (SHA256
`fc8d440f3b964f97feaed588bf3bd11d5c6f191a9423e7ee3c24d9616a513cea`): the module
(85,300 bytes, payload CRC `6829D405`, SHA-256
`a75e0ce9010708953b707aad5db975bb83f9ee20f5657ddf4fdea8cf014acabd`) and the
installed `7b216f2e1136` in `IF-NEEDED/`. Gates: background resident ends at
`0x8c00d740` (limit `0x8c00d800`), stack worst case 348 of 2,000 bytes, the PVR
write-width check on both residents, the GUI's seven production admission
cases. Host suite at `871c4c77`: every make target passes; the unittest
failures are the known environment gaps (`test_asm_audit`'s console macro,
`test_cover_png_rows`, `test_internet_preview`, `test_wince_release_package`)
and `test_splash_assets`, whose boot art is not RGB565-exact since `3c80b58e`
(the failing comparison's diff of 307,200 pixels stalls the run); none is in
the reader. The README explains Armada's `ce-probe.kui` route with CD music
Off and leaves the owner the choice between Armada's virtual VMU there and
this reader. The window stays 64 by default; the switch is a build setting.

## WinCE adaptive slow window: console result, 871c4c7784b4, 2026-11-21

A measurement build, not a candidate: the timing probe with one change, the
6.25 MHz window after a DMA overrun starting at 16 receptions and doubling,
up to 64, while overruns at 12.5 MHz come within 8 receptions of a window's
end; a later one halves it again (Makefile.dc `KUI_CE_SLOW_WINDOW=16
KUI_CE_SLOW_ADAPT=2`, the generic module's background reader only;
[record](wince-adaptive-window-test.md)). Delivered:
`KUI-WinCE-Adaptive-Window-Test-20NOV2026-2111-CST-871c4c7.zip` (SHA256
`bb31ca45ae44966a9c99c521233cfdfd2809590fca89d3add08816f7af12b62a`): the module
(93,096 bytes, payload CRC `C3140C14`, SHA-256
`b5932b2a83bd00472345349d162d1e6fa3866126250d49a5ec9130b29463548e`), a blank
64-slot `KUI/WCEDATA/WCEDATA.BIN` (file id `1333bb630d269579d578c20d04aa86f3`),
the installed `7b216f2e1136` in `IF-NEEDED/`, and the PC tools.

Evidence: host tests of the policy in two shapes and of the CE background
reader with it, nine policy mutations each caught; the GUI's production
admission of the exact bytes (seven cases, FAT32 and exFAT); the background
resident ends at `0x8c00ddc0`, the probe layout's limit, stack worst case 392
of 528 bytes. From the same commit with the switch at 0, the generic module's
six delivered configurations and the five CD-music and Armada modules rebuild
their delivered or installed bytes. The run list leads with Armada US (CD
music Off): its intro, for the echo, and its gameplay, for the lag.

Result (the owner, seven complete slots, every stage report naming
`871c4c7784b4`): Armada "may have had some incredibly small echo with the intro
video but it was largely unnoticeable", gameplay "worked great"; Super
Producers' audio better, its movie still "heavily lagged at the end"; Worms
Armageddon and Super Runabout fine; Maximum Pool "trash"; RE2 stuttered during
its first movie. Armada's first launch showed the old text screen at the
combination: with CD music Off the GUI sends Armada to its legacy module
`ce-probe.kui` whenever that is installed ([routes](wince-gui-routing.md)).
With `ce-probe.kui` moved off the card, both Armada slots ran this module.
The window followed the contention: Super Producers and RE2 ran near the
shortest window (25% and 30% of blocks at 6.25 MHz, against 21% and 29% at a
fixed 16), Super Runabout near the longest (66%, against 24%), Maximum Pool
between (49%, against 38% at 16 and 65% at 64). No read failed. Armada's intro,
Maximum Pool and Super Producers read flat out or nearly (the next request one
frame after each read ends) ([record](wince-adaptive-window-test.md#result-21-nov-2026)).

## WinCE reader candidate: console result, 41e3450bc94d, 2026-11-21

The generic `ce-boot.kui` for everyday use with both console-tested changes:
PVR stream pieces (`a660354d804c`) and the 16-reception slow window
(`9313dad06594`). Recipe: the generic build at `41e3450bc94d` with
`KUI_CE_SLOW_WINDOW=16` (`KUI_CE_VRAM_DMA=1` is that build's default). Its
payload differs from that commit's 64-reception build in the one window byte,
and that build's payload is `a660354d804c`'s with the build ID swapped.
Delivered: `KUI-WinCE-Reader-Candidate-20NOV2026-2002-CST-41e3450.zip` (SHA256
`4adf42ae05416b060b36edabe579ae77406a3e9f959832b8730157e701d47d01`): the module
(85,236 bytes, payload CRC `47EEC0FF`, SHA-256
`ace07b8e9af93191662fe028774b96c16f5e23d54f6fe8f0ec34e96f36dce697`) and the
installed `7b216f2e1136` in `IF-NEEDED/`. Gates: background resident ends at
`0x8c00d700` (limit `0x8c00d800`), stack worst case 348 of 2,000 bytes, the PVR
write-width check on both residents, the GUI's seven production admission
cases. The run list leads with Armada US, CD music Off. The window stays 64 by
default until this candidate's result.

Result (the owner, CD music Off): Armada's intro movie is "back to how it was
before, bad echo and slightly choppy"; Armada's gameplay lag is gone ("Actual
gameplay is super smooth"); Super Producers' movie had the same echo, its
picture relatively smooth until the last 5 or so seconds; KISS Psycho Circus
and Virtua Cop 2 no problems; Worms Armageddon no problems, "maybe slight lag in
the audio with the intro movies"; Super Runabout no problems.

Correction (21 Nov 2026): Armada did not run this module. With CD music Off
the GUI sends Armada to its legacy module `ce-probe.kui` whenever that is
installed, as the owner's card has it ([routes](wince-gui-routing.md)); the
run list overlooked the route, and the adaptive-window test showed it (its
first Armada launch ended on `ce-probe.kui`'s text screen). Armada's two
results therefore describe `ce-probe.kui`, not the 16-reception window:
plausibly the echo because that module predates the slow refetch, and the
smooth gameplay because no CD music streams with CD music Off (inferences).
The 16-reception window has no Armada result. The window stays 64 by default.

## WinCE slow-window test: console result, 9313dad06594, 2026-11-21

The owner: "That seemed to fully fix Super Producers"; Super Runabout
"flawless"; Maximum Pool still slow in menus; Sno-Cross and RE2 still hang
(RE2 differently each run). The rings: streamed reads 12-17% faster at the
median, half-clock blocks roughly halved, overruns per block up 15-70%, no
read failed ([results](wince-slow-window-test.md)). Armada US was not in the
run (with CD music Off it uses this module only when `ce-probe.kui` is absent).

### Delivery

A measurement build, not a candidate: the timing probe `5c1cdd89387b` with
one change, the 6.25 MHz window after a DMA overrun cut from 64 receptions to
16 (Makefile.dc `KUI_CE_SLOW_WINDOW=16`, the generic module's background reader
only). Its payload differs from a 64-reception build with the same label in
that one byte. Delivered:
`KUI-WinCE-Slow-Window-Test-20NOV2026-1847-CST-9313dad.zip` (SHA256
`12be4232ca56333edea18bcabc355f36409409b14397a85b62a9bbbe66b73226`): the module
(93,000 bytes, payload CRC `41969797`, SHA-256
`5876c63d292231cd38970fee4626e33d5cb16cdb62ed552fa36f9a38ba4d0863`), a blank
64-slot `KUI/WCEDATA/WCEDATA.BIN` (file id `49201ca448c8f448cd43f6514335f36d`),
the installed `7b216f2e1136` in `IF-NEEDED/`, and the PC tools.

Evidence: the SCI stream and CE background reader host tests at 16 and 64
receptions; the GUI's production admission of the exact bytes (seven cases,
FAT32 and exFAT); the background resident ends at `0x8c00dd60` (limit
`0x8c00ddc0`), stack worst case 392 of 528 bytes. From the same commit, with
the default window, the generic module in all four configurations and the five
CD-music and Armada modules rebuild their delivered or installed bytes.
Console behaviour is untested: the session is the test
([timing session 1](wince-timing-session-1.md#the-next-test-a-shorter-slow-window)).

## WinCE timing session probe: data received, 5c1cdd89387b, 2026-11-20

The owner ran eight launches with no VMU inserted; results:
[timing session 1](wince-timing-session-1.md). No read stalled in any window.
Overruns, from bus contention during streams, slow the paced movie and
animation reads 1.4 to 1.8 times, and RE2's halts begin with nothing
outstanding. Wild Metal's load ran at the clean rate.

### Delivery

A measurement build, not a candidate: the generic `ce-boot.kui` with
`KUI_WCE_PROBE=2` ([timing session](wince-data-probe.md#timing-session-kui_wce_probe2)),
for the movie freezes (Resident Evil 2, Sno-Cross, Super Producers), Maximum
Pool's lag, Urban Chaos's lurches and Wild Metal's load. It reads exactly as
the installed `7b216f2e1136` does (`KUI_CE_VRAM_DMA=0`), so KISS and Virtua
Cop 2 still stop on the PVR refusal in it. The owner swaps it in for one
session and puts `7b216f2e1136` back afterwards.

Delivered: `KUI-WinCE-Timing-Session-20NOV2026-0158-CST-5c1cdd8.zip` (SHA256
`652f8a627816322a2e81c5dd0fe1943e3848a4ae3d1fffb1c41e041787b5d2bd`): the module
(93,000 bytes, payload CRC `9AAC3E84`, SHA-256
`0a3bc111a5485892fee080497275c154f74233d64cbf65cc2152772acd0d0037`), a blank
64-slot `KUI/WCEDATA/WCEDATA.BIN` (1,075,314,688 bytes, file id
`968e4806ba74f2f47e03e0c6f383cb8f`), the installed `7b216f2e1136` in
`IF-NEEDED/`, and the decoder and miner in `PC-TOOLS/`.

Evidence: the end-to-end host test (take conditions, underflows, failed,
aborted and reset reads, a wrapped ring decoded with its saturations and
carries, a dump cut inside the ring), the miner test, mutation checks on each
new behaviour, two adversarial reviews (no defect left in the reader; the
decoder, miner, docs and test findings fixed), the GUI's production
admission of the exact bytes on FAT32 and exFAT, and the build gates
(background resident ends at `0x8c00dd60`, limit `0x8c00ddc0`; stack worst
case 392 of 528 bytes). From the same commit, `KUI_WCE_PROBE=1` rebuilds
`88ff969d505c` and the default build `a660354d804c` byte for byte. Console
behaviour is untested: the session is the test.

### Rechecked against Main and the Test 83 reader line, 2026-11-20

Main is still `4441e92c`, which both test builds already contain. A trial merge
of `claude/hopeful-dijkstra-r2g7c4` at `9fd8e907` (the Test 83 survey and the
owner's reader and GUI lines, not on Main) rebuilds, with each delivered
label: the generic module with the PVR change off (`7b216f2e1136`), on
(`a660354d804c`), the timing probe (`5c1cdd89387b`) and the data probe
(`88ff969d505c`), and the four CD-music modules and `ce-armada-all-on.kui`
(`7b216f2e1136`), each byte for byte. That line changes native readers only;
the two delivered ZIPs stand as they are. The owner's GUI line (`4d4dcff7`,
the GUI the Test 83 runs used) admits both delivered modules in the GUI's
seven production WinCE cases on FAT32 and exFAT (`tests/test_wce_probe_admission.py`
built from that line's source); its WinCE launches are not console-tested.

## WinCE PVR stream test: both games pass on the console, a660354d804c, 2026-11-20

The owner's result: "the video memory fix worked for both of the games" (KISS
Psycho Circus (US) and Virtua Cop 2 (JPN), CD music Off). No launch report was
sent, so the GUI and the module's payload CRC as launched are not confirmed by
the console. The CD-music modules keep the refusal; whether the change can take
them still needs their resident space audited.

One behaviour change to the generic `ce-boot.kui` against the installed
`7b216f2e1136`: DMA stream pieces into PVR video memory, which stopped KISS
Psycho Circus and Virtua Cop 2 in session 1 ([record](wince-vram-stream.md)).
CPU pointers into video memory stay refused, as before. The CD-music and
Armada modules are unchanged: from the same tree they rebuild their installed
`7b216f2e1136` bytes, as does `KUI_CE_VRAM_DMA=0`.

Delivered: `KUI-WinCE-VRAM-Stream-Test-20NOV2026-0035-CST-a660354.zip` (SHA256
`744e817bae8eb2f6f471de731a9f7511d3d076321aeb9880f976397eb12cd13b`): the module
(85,236 bytes, payload CRC `9CF2A176`, SHA-256
`72dc006d86650b24daaec15e6a8ecb1575aa1cf7de8be409cc027658511dc8d5`) and the
installed `7b216f2e1136` in `IF-NEEDED/`. Console test: KISS Psycho Circus
(US) and Virtua Cop 2 (JPN) with CD music Off and Virtual VMU Off. With CD
music On they still use the unchanged `ce-cdda-*` modules.

## WinCE data session 1: data received 2026-11-19, 88ff969d505c

The owner ran 17 games with this probe on 19 Nov 2026; the control run on
`7b216f2e1136` behaved about the same. Results:
[session 1](wince-data-session-1.md). KISS and Virtua Cop 2 stopped on the
PVR refusal above; the movie freezes and lag led to the timing session.

### Delivery

Rebuilt on main `cdfc131b` (merge `88ff969d`, after the artwork/theme changes):
`KUI-WinCE-Data-Session-18NOV2026-1406-CST-88ff969.zip`
(SHA256 `76720dfdf28dabb61664e0e6ade4c84d4e7bd4409f0caa60f68249893fa968f1`),
module 92,456 bytes, payload CRC `F20C6F56`. Its payload equals `e154b0b315a1`'s
and `19ff02faaec4`'s except the build ID; probe-off still rebuilds `7b216f2e1136`
byte for byte, and the GUI admission passes on the merged harness. This is the
package to use; it replaces `KUI-WinCE-Data-Session-17NOV2026-1311-CST-e154b0b.zip`
(SHA256 `531d1e15...`, payload CRC `705B8B72`).

### Earlier build of the same probe: 19ff02faaec4

A measurement build, not a candidate: the generic `ce-boot.kui` with
`KUI_WCE_PROBE=1` ([data probe](wince-data-probe.md)). The owner swaps it in
for one bulk session and puts the installed `7b216f2e1136` back before the
GUI `069d612cadea` tests. No GUI change; both GUIs admit it as they admit
`ce-boot.kui`.

Delivered: `KUI-WinCE-Data-Session-17NOV2026-0150-CST-19ff02f.zip` (SHA256
`ed1a5598010d1209ce9f6a1f3ea9cd2a53d8cf7d864c192afb43a0d17601df40`): the probe
module (92,456 bytes, payload CRC `E74008FC`, SHA-256
`5c1d82902426c82fb110880fac651e9111726760ffb39d97cb6ea356ad1d72f3`), a blank
64-slot `KUI/WCEDATA/WCEDATA.BIN`, and the installed `7b216f2e1136` in
`IF-NEEDED/`.

Superseded the same night: `KUI-WinCE-Data-Session-17NOV2026-0041-CST-c25e9f5.zip`
(SHA256 `cf03119e73ae3819148f515d20cde9aa76035d64dd15717b683c611278a3aad8`,
module `c25e9f5e203b`, CRC `FFD43455`) was sent before the review finished; it
lacks the second status read after a failed stage write and the full RAM
clear. The first build (`a257f13ec7d1`) was never sent. Console results belong
to whichever module the session's stage reports name (their build field).

Host evidence: the FAT32/exFAT lookup and its refusals, the end-to-end
claim/session/dump on FatFs cards (contiguous and fragmented, red, yellow,
interrupted claim, damaged seeds, a reader fault, a cut-off dump, a retried
write), the GUI's production admission of the exact module bytes, the miner,
and the build gates (resident ends by its 0x8c00ddc0 limit; stack worst case
392 of 528 bytes, calls into IPA-split clones counted).
`KUI_WCE_PROBE=0` rebuilds `7b216f2e1136` byte for byte. Console behaviour is
untested: the first session is the test.

## Pending console test: branch build 5eece4b72a35, 2026-11-17

`KUI-Branch-Test2-17NOV2026-1105-CST-5eece4b.zip`
(SHA256 `b67abd8e37c00a67cb227b5a53e47d91a62e9cf36164fe51fa922991f62db24b`) tests
branch `claude/cool-pasteur-kxjg6b` at `5eece4b72a35c1f3a8c028cc3cffdae2ff3cbfbc`.
Since test 1 (`f094fa8f40f3`) it changes only three things:

- the startup splash art, stored compressed and inflated once;
- the launcher artwork, stored compressed and inflated on first use;
- the boot disc, now [K-UI Bootloader v3](boot-disc-v3.md).

`git diff f094fa8..5eece4b7` touches no Games, reader, admission, manifest or
updater source.

It contains only:

- `KUI/runtime.kui` and `KUI/boot.kui`: identical, 2,350,288 bytes (test 1:
  3,149,564), build `5eece4b72a35`, payload CRC `501F990A`,
  `KUI_SMASH_LAUNCH_TIMING=1`, no embedded module bundle;
- the test-1 `zso-boot.kui` (`5B99D20D`) and `lzo-boot.kui` (`C87D6A7C`), byte for byte.

Optional: the Bootloader v3 CD in the redundant layout (copies at FAD
11881/44891/77901, SHA256 `c58ead09…`), the same bootstrap in the single-file
layout (`79aafb87…`), and `tools.kui` (`F16A3520`). Everything else on the
card is retained. Test 1's exclusions still apply: no Windows CE games and no
Smash Pack with this GUI.

Evidence is host and emulator only. Flycast boots both CDIs and the GUI;
the stage-1 damage cases are in [the boot disc record](boot-disc-v3.md). This
does not replace or re-baseline the `f8807cb16561` timing candidate.

## Pending console test: branch build f094fa8f40f3, 2026-11-17

`KUI-Branch-Test-17NOV2026-0743-CST-f094fa8.zip`
(SHA256 `52326c0fd1e90d98c9843cbe93c226b6bb0731af76a530bd0ecab1f2fc9ae3a8`) tests
branch `claude/cool-pasteur-kxjg6b` at `f094fa8f40f38754eb96cbf4f77f4ecc75fb35e5`:
menu redraw/vblank, Games paging/letter steps/tiles/catalogue/cover lookups,
single-pass launch hash, music session sharing and nonblocking song steps,
ZSO/LZO span streaming, the boot RAM canvas and the GitHub-link header credit.
It contains only `KUI/runtime.kui` and `KUI/boot.kui` (identical, build
`f094fa8f40f3`, payload CRC `41595CFA`, `KUI_SMASH_LAUNCH_TIMING=1`, no embedded
module bundle), `zso-boot.kui` (`5B99D20D`) and `lzo-boot.kui` (`C87D6A7C`), plus
an optional bootstrap CDI and `tools.kui` (`BAD48C5D`). Everything else on the
card is retained untouched. `e79da8ce`, Main `d69b7b01` and `e2761a9a` each rebuild
the delivered ZSO/LZO loaders byte for byte with build `f8807cb16561`, so the
loaders' only change is `7775e61`.

The GUI is test-only. It lacks the Windows CE routes of the installed
`00d8ed67d9ad` ([routing record](wince-gui-routing.md)): the test excludes Windows
CE games and the owner restores the previous GUI afterwards. It also omits the
embedded same-commit Smash stage bundle (`retail-smash-menu`, `-native-menu`,
`-vc2`, `-swirl`): built identically, Main `d69b7b01` gives a 3,141,340-byte
`runtime.kui`, this GUI 3,149,564 (the branch adds 8,224 bytes) and the delivered
`f8807cb16561` GUI 3,554,464, so the 405 KB difference is the bundle. Smash Pack
would read the card's stages instead, the unestablished-cohort case that earlier
gave slow collection startup and VC2 stops, so the test excludes Smash Pack too. This does not
replace or re-baseline the `f8807cb16561` timing candidate.

## WinCE Armada: Virtual VMU On accepted, cb07c6454454, 2026-11-09

The setup was the owner's US raw Armada GDI with CD Music On, GUI
`00d8ed67d9ad`, booted cold from ROOT. With it, `ce-armada-all-on.kui`
`cb07c6454454` (payload CRC `F86EA178`) does three things:

- plays the intro as smoothly as VMU Off ("mostly smooth. Maybe a tiny bit
  of lag/reverb exists");
- plays CD music in game;
- saves on the virtual card. The test was: load the existing save, save
  again, power off, cold-boot, then load. The owner replied: "Tested
  Armada's saves and it works fine. CD Music also plays fine."

VMU Off `ce-cdda-armada.kui` `8cdbc0a351f8` (CRC `4E747363`) is the
owner-approved VMU Off reader ("overall it sounds great"). Both are
timing-diagnostic builds. See
[the VMU On record](armada-vmu-on-audio-fix-test.md) and
[the movie timing record](armada-movie-timing-test.md).

Pending console test: `7b216f2e1136`. It puts the same two reader changes
into every WinCE module:

- the generic module;
- the Armada, BAM4, Worms and WWP CD music modules;
- Armada VMU On, without the timing rows.

It was delivered as `KUI-WinCE-Reader-Fixes-09NOV2026-1633-CST-7b216f2.zip`.

Owner result, 9 Nov 2026: Worms Armageddon works with CD Music On and Off
("it runs fantastic"). That run used `ce-cdda-worms.kui` and the generic
`ce-boot.kui`. Not yet reported:

- Bust-A-Move 4;
- Worms World Party;
- other titles on the generic module;
- the package's two Armada modules.

Until they are, the modules they replace remain their reference. Those
modules are in the package's `IF-NEEDED/`. The Smash Pack CE modules and
`ce-probe.kui` are unchanged.

## Sega Smash Pack accepted: cc81e63ca4d2, 2026-11-06

The owner accepts the collection integration as complete after this exact
sequence succeeded in one uninterrupted console session:
`VC2 -> Shining Force -> PS2 -> Sega Swirl -> VC2 -> ALL OTHER GAMES -> VC2`.
This includes repeated VC2 launches after both the saving games and the rest
of the collection, the condition that previously failed. This acceptance
supersedes the pending/rejected status of the earlier candidates below; their
individual failures remain historical evidence.

The accepted software is local source
`cc81e63ca4d29f51b02bdd49e95d8cedbe689d7d`, identical in source tree
`6f4814f643f5a39fd4728e79d3703167bb40e741` to published commit
`6d489a90276776a4d62e4f68b9efdc3161c6c1c0`. The tested archive is
`K-UI-Smash-cc81e63ca4d2-SCI-DAMAGED-BLOCK-CANDIDATE.zip`, SHA256
`eb469340a2a6732a356a77536ca8a04a261740c3bca0620e84266f06c204ca6b`.
Its embedded/standalone KUP SHA256 is
`3c85ac9927a7ade59f69001babbb33a88175c54b592589b86e59a9c13e476209`.
Retain these exact tested bytes; a documentation or merge commit is not a
new tested binary, and no replacement package is required for this acceptance.

Preserve the current VC2/Swirl helper components, the native72KiB worker,
independent background-reader ownership, original Genesis options/Save Game,
working returns, shared-card admission and loading improvements. The shared
SCI lost-byte quarantine, bounded same-LBA retry, normal CMD18 batching and
full block CRC checks are now part of this accepted baseline. Do not restore
an earlier helper/decoder, bypass validation or retune individual games to
recreate it. This collection acceptance does not qualify untested loader
families or turn the separate generic CE audit into an installed global rollout.

VC2 still takes a long time to load; the owner supplied no new duration or
VMU-on/off comparison in this acceptance. Record that as an optional future
performance investigation, not a remaining Smash completion blocker. Do not
start another VC2 change or request another console run without a new request.
If revisited, preserve this accepted baseline and measure the complete phases
before applying the existing shared batching/card-admission lessons. Work
counts alone do not establish a user-visible speedup. See
[the shared policy](global-launch-policy.md) and
[the accepted read-recovery repair](sci-damaged-block-reread.md).

## Universal loader integration started: 2026-11-06

The owner requested an ordered roadmap and implementation toward one broadly
compatible maintained build. Start from Main `d393b859a9ee`; the roadmap is
`universal-loader-roadmap.md`. Shared correctness and reproducible deployment
come before new service memory layouts, native SDK extensions, coordinated
CDDA streaming and broader lifecycle/format/transport coverage.

The first integration targets generic CE payload-write width, a recorded
ordinary/CE/CDDA build plan, fail-closed cold-rollout output and actual native
service geometry reporting. It preserves the accepted collection components
and existing worker reservations. The already merged CE completion fixes and
SCI lost-byte guard remain required in affected rebuilt readers.

This entry records authorized scope, not console acceptance, completed global
deployment, new virtual-card support for DOA2 or a CDDA/FMVs performance result.
Record build/package outcomes and any refused family before offering a new
installation. Keep accepted Smash gameplay and return results above intact.

### Shared pre-bootstrap follow-up

The owner reports almost twenty seconds before bootstrap in Power Stone 2 on
the first consolidated test and explicitly rejects per-game performance fixes.
The follow-up is in `shared-gui-launch-preparation.md`: common GUI card
admission and snapshot batching, plus launch-scoped reuse of a freshly verified
frontend recovery checkpoint. It preserves all twenty non-GUI files from the
72db614 test, the accepted collection and game-time worker geometry.

Existing executable/history batching and aligned GUI decoders are preservation
requirements, not new achievements. New tests bind those actual defaults and
compare the new shared paths with single-read/full-checkpoint controls.
The owner now reports Power Stone 2 reaches bootstrap in about four seconds
on 9d6680a, versus almost twenty seconds before. Record this as the reported
preparation improvement, separately from bootstrap and save/return coverage.

### Native bootstrap services: 7fca3e3 candidate

The owner asks to carry the faster Smash bootstrap lessons into shared native
stages while retaining VMU hooks, and recalls about two seconds through Smash
bootstrap. Exact dbadd source/binary comparison established three later changes:
cached whole-executable CRC, fresh metadata/snapshot batching and removal of
native informational holds. Older cached extent/replay/FAT work was already
installed and must not be claimed again as missing.

`shared-native-bootstrap-rollout.md` records the new 26-file candidate,
five-service coverage, preserved workers/geometry/Smash bytes and qualification.
The GUI's exact Standard and retained native-CDDA manifests are promoted to
the default resource paths without rebuilding these binaries. The owner now
reports no noticeable improvement with the new build ID. The exact final
reader was not captured, so this neither establishes a bootstrap speedup nor
diagnoses an updater failure. Treat the bootstrap performance change as
unaccepted. No universal two-second target or new compatibility claim follows
from the owner's Smash comparison.

The owner's explicit scope is Games only. Freeze startup/splash, menu,
branding and updater implementations during this integration. Prioritize
shared preparation and bootstrap mechanisms across every applicable route;
record their actual installed coverage and final reader identity. Do not send
another performance candidate based only on a changed GUI label or model counts.

### Shared Games-only follow-up: eeed38bd540e

The branch now implements shared CE kernel-read reuse, indexed advisory native
hints, cached immutable discovery on a miss, same-invocation native role-proof
reuse, zero default cold photo holds and final-reader source reporting.
All13 cold build roles/16 aliases qualify. Five current native workers and nine
additional live components preserve their complete initialized code/geometry
against the retained 7fc cohort, apart from permitted build labels. Accepted
Smash files remain exact. No startup/menu/splash/updater source changed.

`shared-game-launch-performance.md` and `evidence/games-eeed38b` record scope
and implementation evidence. Default Standard/CDDA inputs now bind this exact
cohort. The manual test avoids KUP installation and explicitly backs up the
two saved selection records so copied physical files cannot be masked by a
bank. Those records, banks and user data are not overwritten by the archive.
Ordinary CE's `ce-probe.kui` path collision remains an explicit deployment gap.
The new binaries have no console speed or compatibility acceptance yet.

Delivered manual test: `KUI-Games-Manual-06NOV2026-1424-CST-eeed38b.zip`,
SHA256 `17ff15b9fc3ac08255e52549ade2b9936f70e427e99a0643bb09ed3777ae3d28`.
The archive contains28 software files and its README/BUILD/LICENSE, with no KUP,
activation records or user data. `evidence/games-eeed38b/delivery.json` records
the actual final source-reader fingerprints. Back up the two selection records
outside their active directory for this manual ROOT test; keep them for restore.

## 0a hardware report: damaged reconstructed block reaches the sector parser

The owner's three0a2ddd9bb9b6 photos show FN12 PIO_TRANSFER in the original VC2
driver, loading `VC2/BIN/COMMON/F_COMMON.BIN`. The sector parser rejects a header
after0x69800 bytes. The accepted source block retains a DMA lost-byte marker;
the mapper succeeded and the bus fault flag is clear. The blue pages are the
now-visible terminal report. This package retained91fa's actual current helper
and arbitration code; it did not roll them back. Fresh-first startup is still
not reliable.

An old-production negative control now demonstrates that CRC erasure repair
can accept a wrong block with an invalid raw-sector header. Shared SCI source
now rejects all reconstructed blocks and uses its existing bounded same-LBA
retry/programmed-I/O fallback before game delivery. Normal batching, CRC checks,
buffer geometry and current helper implementations remain required. This is
a supported repair to the observed failure path, not proof of the console's
exact corruption or a declaration that every freeze is solved. See
[the repair and evidence](sci-damaged-block-reread.md).

## 91fa hardware rejection: repeated VC2 fails before its VMU message

The owner tested the delivered91fa package and reported this exact sequence:
`VC2 -> Menu -> Play -> return to main -> Sega Swirl -> Shining Force -> PS2 -> Vectorman -> VC2 -> VC2 loading screen -> Dead`.
The first VC2 visit reached play and returned. The later visit froze after the
bottom-right Loading message disappeared, before the visible VMU message.
On a working run, that interval contains another approximately5–7 seconds of
SD activity before the VMU message and opening credits. No new LED behavior,
error code, framebuffer color or failing instruction was reported for the
failed run. Do not infer those details.

91fa is not accepted as a reliable VC2 fix. Its reproduced drive-status/VMU
loan defect remains a valid software correction; this report does not establish
that defect as the freeze cause. The latest failure is specifically before
the visible VMU check, which does not prove virtual-card code is inactive.
Trace the loading-to-VMU handoff and repeated-launch state against this exact
sequence. Do not request another console run merely to recheck a speculative
reset or a code path already excluded by actual compiled configuration.

The separate audit's generic CE completion fixes do not alter this native
foreground/MMU/PIO configuration. Shipping those alone would not constitute
a new VC2 test. Preserve current workers, options/Save Game, graphics policy,
loading improvements and working returns while obtaining new evidence.

## Broader audit follow-up; delivered91fa remains unchanged

The owner requested a systematic shared loader/VMU review after the repeated
VC2 regressions. The delivered91fa package fixes the verified VC2 GD-control
arbitration race and preserves its actual embedded/staged four-module quorum.
It is still awaiting console confirmation; no later audit fix is in that ZIP.

The separate audit source corrects stale GUI bundle-object selection, a shared
CE removed-callback flag that blocks VMU loans, and a missing DMA-end wake on
failure of an active CE transfer. Old-source controls reproduce all three;
new regressions are part of the normal test targets. Native GD preprocessing,
current workers and all collection gameplay/return code remain preserved.
Affected installed CE cohorts have not been rebuilt by this audit. See the
[findings, evidence and open gaps](shared-loader-audit-2026-11-06.md) before
assembling another update or claiming these source fixes have rolled out.

## Current hardware failure: VC2 also fails as the first selection

The owner now reports VC2 loading its VMU and then freezing on a cropped white
screen even as the first game after boot. Later selections can freeze black.
Repeated earlier successful launches do not establish reliable startup. The
native RAM-retirement candidate946 is not accepted as a solution; a
cleanup-only explanation cannot account for this fresh first-selection case.

An active-runtime defect has been identified independently of that cleanup:
while the virtual-card worker borrows the SD reader, the low GD gateway
returns -1 for non-request control calls. The original VC2 WSEGACD drive-status
call at return RVA23CE converts any nonzero result to C0000185 without retry.
The existing request-only wait gate therefore leaves a permanent-error race
in a normal, otherwise idle reader. Source-admitted control calls must wait
for the actual borrower to release ownership and then execute normally; they
must not receive invented results. CHECK's status4 already sleeps/retries in
this exact driver and EXEC retains its existing busy behavior.

The correction is limited to VC2's high GD-wait helper and its component
identity. Swirl's helper, collection return controllers, native72KiB worker,
Genesis options/Save Game, graphics copy policy and read batching are outside
this change. The original driver error branch and the old linked wait gate
are required regression controls. A software reproduction of that race does
not establish that it caused every photographed console freeze.

## Later-reentry candidate: retire collection-native game RAM

The next candidate preserves075e's working initial sequence and cache fix.
Only the collection-native terminal return gains the existing CE return's
dead-RAM clear. The call occurs after successful worker retirement, original
argument capture and hardware/cache quiescence, before any new capsule read.
It never executes on BUSY/refusal and does not apply to generic session or
background-reader returns, which may retain live high-RAM objects.

The accepted profile's complete remaining reader/code/data/stack closure is
below AC010000. The old high game/worker frame is never resumed after this
boundary. This closes a retirement difference: SAMPLE leaves upper RAM
intact, and the CE fixed graphics allocator can return its prior contents
without clearing them. No actual bad graphics-buffer read was captured; the
change is a supported clean-retirement candidate, not a proven freeze cause.

CE controllers, immutable helpers, raw readers, native worker/reservation,
original options/Save Game and game read paths are preserved. The native
terminal controller grows only to add the clear; its low bounds/stack and
captured context must still pass linked checks. See
[the retirement note](smash-native-ram-retirement.md).

## 075e hardware result: initial sequence passes; later VC2 freezes after Start

After delivery of075e785e9308, the owner reports playing Sega Swirl, Shining
Force, Phantasy Star II and VC2 successfully, then every other game in the
collection successfully. On the subsequent VC2 run, pressing Start to enter
its main menu caused a small diagonal picture shift and a freeze.

Preserve the one-bit direct cache-entry writeback correction and the working
games, native options/Save Game, return paths, workers and helpers. The initial
Swirl → SF → PSII → VC2 sequence is now reported working. The remaining symptom
is a later VC2 title-to-menu transition; do not call it the same before-title
failure, infer an exact order for the other eight games, or claim a captured
PVR register fault from the picture shift alone. No new loading time was given.

The source package remains075e; no additional gameplay fix is established by
this report. Its current source is available for review in PR31; live Main
still has the preceding eddbb01 source at this checkpoint. Further changes
must address a supported later-reentry defect and retain this hardware progress.

## Warm-entry follow-up: correct terminal cache writeback

The current follow-up starts from the exact published Main eddbb01 tree. It
changes one bit of the terminal cache-array address: F4000008 to F4000000.
The former selects associative writes. CE calls this cleanup before disabling
its MMU, so writing a physical tag back through the associative interface can
miss or mismatch the game's virtual translation and silently do nothing.
The following CCR invalidation can then discard dirty data. Direct entry
writes avoid that translation dependency and retire every dirty cache entry.

This is a confirmed cleanup defect, not a captured cause of the console black
screen. The low64KiB firmware/IP area is deliberately preserved across returns
and is not all covered by the earlier bounded low-reader publication. Losing
dirty bytes there violates that preservation contract. Which live bytes were
affected in the reported console run remains unknown.

The source change preserves all instruction bytes, loop length and linked
controller geometry; only the literal's association bit changes. No worker,
helper, native options/save flow, game read path or return routing changes.
The new linked-instruction cache test models the documented SH7750 hardware
semantics and includes the old literal as a failing control. Console warm-order
acceptance still requires the exact Swirl → Shining Force → PSII → VC2 check.
See [the cache-retirement note](smash-cache-retirement.md).

## Current review handoff: VC2 fails after the multi-game sequence

The owner reports that Sega Swirl → collection → Shining Force → collection →
Phantasy Star II → collection → VC2 fails with a black screen **before VC2's
title** on the current ca96612c27fc candidate. This is a warm-start failure,
not the later VC2 Return action. The precise failing instruction and logo
boundary are unknown. Do not describe the current package as fully accepted.

The owner requested publishing the current work to Main for independent review.
No production game fix was made after ca966; preserve its Genesis options/Save
Game flow, the existing VC2 return work, current workers and all integrity
checks. Read [the review handoff](vc2-warm-launch-review.md) for established
results, exact preservation boundaries and limits of the read-only audits.

## e371 hardware result and required Genesis save menu

After the requested VC2-first/title/Return check, the owner reports that it
"actually worked" on e3714617ad28. Do not invent a separate after-Swirl result,
a new timing, or an all-game matrix. Preserve the CE controller/startup layout,
both helpers and the72KiB worker.

The owner identifies a functional regression: Shining Force and Phantasy Star
II need the original emulator options menu to commit cartridge SRAM to the
VMU. The direct A+B+X+Y+Start shortcut skips that menu, so instructions to finish
its Save Game action before that combo were unusable. The follow-up removes
the2-byte combo hijack. The original combo must open options with Save Game;
only explicit Return to Menu reaches the existing checked117F0 reset hook.
Retain the original dirty-save confirmation. Draining VMU I/O does not create
a cartridge save which the engine has not requested.

The RPG flow is: save within the RPG, A+B+X+Y+Start to open options, select Save
Game and wait for completion, then Return to Menu. Source/linked checks prove
those paths; durable progress after power loss remains a console check.

## cd3f hardware result: black screen; startup preservation failed

The owner reports that the next passive-reader candidate now gives a black
screen and rejects further startup regressions during return-only work. The
exact black-screen boundary was not supplied; do not silently claim it occurs
at a specific logo, on a fresh launch, or only after Return. This candidate is
not accepted. No successful startup or return result supersedes that report.

The earlier claim that the assembler installer matched eb6 was too narrow.
Its linked bounds changed. Compared with the console-booted3337 baseline,
the candidate writes456 more initialized/BSS bytes before NK, relocates its
saved previous-E0 cell and runtime, and changes the high copy entry. Identical
raw512 and helper41152 bodies do not establish equivalent startup behavior.
No concrete low-RAM owner conflict or failing instruction has been found;
the black-screen cause remains unproved. The bounded original NK replay stops
at its first unauthored device access and cannot certify later boot behavior.

Return-only follow-ups must hold the actual3337 startup write-address trace,
copy entry, runtime window, initialized extent and BSS/previous-hook addresses
fixed. Verify actual linked state and forwarding behavior, not an assembler
source hash alone. Reclaiming space inside exit-only code is permitted only
with the same full IEEE CRC and all integrity predicates intact; gameplay
workers, immutable helpers and ordinary read implementations remain pinned.
An exact footprint is a containment boundary, not hardware acceptance or a
diagnosis of the reported black screen.

The bounded follow-up fits the complete current return repair inside the
accepted7340-byte VC2 and7500-byte Swirl images. Only the controller's exit-time
CRC changes to a complete bitwise IEEE implementation; ordinary reader/worker
and helper bytes stay unchanged. The linker pins copy/runtime/BSS addresses,
and the new actual-instruction startup contract compares cache publication,
write addresses, forwarding and preserved CPU state against3337. Both baseline
self-controls and candidates pass; the expanded cd3f image fails that gate.
This restores an explicit startup containment contract while retaining the
canonical raw repair, all13 guards and the full destination CRC. It is not a
rollback to older workers or an assertion that console startup is repaired.

## 02ec hardware result: white display during VC2 startup

The owner reports a white, reduced display area after a logo described as
"VM2" while testing the protected-reader candidate. The photo contains no
RET diagnostic. Whether this was VC2 first after power-on or after Swirl was
not supplied. Do not silently identify that logo or transfer the earlier3337
fresh-boot acceptance to this candidate. Neither raw-reader return repair nor
the after-Swirl transition is hardware accepted on02ec.

The bounded installer audit found no register/SR clobber or new graphics MMIO.
However,02ec added a512-byte startup write at B400..B600 and validated it after
helper loading. A full-game ownership contract for that extra saved copy was
not established. This is a changed startup behavior, not a proved explanation
of the photographed display.

The next candidate removes that capture and its startup CRC: its installer is
byte-exact eb6. The authenticated initialized return controller instead carries
the current reader's260-byte prefix; its252-byte zero tail is reconstructed.
Every build must match the fresh linked resident and raw512 file against that
asset and the reviewed full-image SHA. No old worker supplies these bytes.
Both validation boundaries check the complete logical512 bytes against the
original launch-sealed CRC, while the prefix also belongs to controller code
CRC. After freeze/quiesce/cache-off/MMU-off the controller restores all512
bytes at8100 and checks the actual destination before executing it. No new
B400 capture, memory lease or changed integrity expectation remains.

Both profiles pass linked recovery, refusal, late-install and original-source
checks. These establish software behavior, not console startup or return
acceptance. The next console check is full power-off, VC2 first through its
title and Return, then Swirl Return followed by VC2. Preserve the72KiB native
worker, immutable helpers, native/Swirl return fixes and all sector CRCs.

## eb6 hardware result: VC2 return fails the raw512 integrity check

The owner reports `RET CE010004`, detail `00000800` with the eb6 candidate.
The CE01 prefix confirms the revised controller executed. Only predicate11,
the full512-byte raw reader at8100, fails; the other12 predicates pass. The
refusal precedes helper retirement, hardware quiescence and outer-capsule I/O.
This identifies the failing integrity domain, not a bad SD card or a particular
writer. No result for the after-Swirl AM2 freeze was supplied for eb6.

The bootstrap raw reader was authenticated before NK and during helper
hydration, but its continued immutability through a complete CE game had no
separate preservation contract. The hardware result disproves that lifetime
assumption. Exact-source review found WSEGACD reads of8100 and the original
game's reset route, but no established writer; missing middleware and firmware
internals limit attribution. Do not claim a particular game write was proved.

The02ec follow-up captured the complete admitted512 bytes into presumed unused space in
the existing low return lease. It validates that reserved copy against the
original sealed CRC before publishing the return hook. Both return integrity
checks validate that copy. After quiesce/cache-off/MMU-off, it restores8100 and
checks the complete destination against the same sealed CRC before any reader
instruction can execute. The old execution check is moved to its execution
boundary, not removed or refreshed from changed live bytes. Changed saved
code or padding still refuses; a failed restore refuses before SD I/O.
That extra capture is superseded by the initialized-image design above.

## 3337 hardware result: Swirl returns; subsequent VC2 freezes

The owner confirms `3337e554b21c` returns from Sega Swirl to the collection
menu. Preserve that accepted private-callback dispatch. The owner then launched
Virtua Cop2 in the same session: its VMU loads, Sega and then AM2 logos appear,
then the display becomes white and freezes before the main screen. The owner
explicitly confirmed this was **after Swirl return**, not the first game after
a fresh boot. A subsequent full-power-cycle comparison on this same build
**successfully boots VC2 first**. Preserve that accepted fresh boot and treat
the after-Swirl startup failure as a separate transition failure.

VC2's own return still freezes with `RET CE000004`, detail `00000000` on that
fresh run. Unlike the old d04 predicate, the3337 mask path should only call
that failure with a nonzero mask. Audit the executed controller, diagnostic
arguments/display and actual delivery selection before treating the displayed
zero as an identified failed predicate. No exact new loading time was supplied.

Actual linked replay passes the Swirl(owner4)→outer(owner1)→VC2(owner3) receipt
sequence. The
new VC2 helper and low reader independently clear their declared mutable
workspace before use. Those checks exclude the demonstrated stale receipt or
private-state hypotheses within their modeled boundaries; they do not prove
hardware, interrupt, audio or graphics state is safe after a CE return.

The source audit also found omitted PVR-interface/sort-DMA retirement steps in
the shared return quiesce. However, VC2's original SAMPLE executes the same
original cleanup again before loading CE. This is an independent ownership
gap, not an established explanation of the later white freeze. Do not ship a
speculative device reset as a diagnosed VC2 repair. See
[the bounded source audit](evidence/vc2-after-swirl-render-state-source-2026-11-06.json).
The same3337 fresh-first comparison is now accepted as described above. Do not
repeat that test or call the after-Swirl failure a fresh-start regression.

The follow-up restores original MAIGO's complete main-RAM retirement clear
after quiescence/MMU-off and before loading the outer capsule. It also checks
the previously omitted PVR-interface/sort DMA owners before RAM replacement.
This restores an original return contract; it is not yet a hardware-confirmed
white-screen repair. The exact packaged3337 diagnostic chain renders nonzero
failure masks in replay, and package/ROOT/helper checks found no supported
stale selection. A separate CE01 error prefix will identify the executing
controller revision without dropping any guard. VC2 return remains open.
See [the RAM lifetime record](smash-ce-return-ram-lifetime.md).

## d04 hardware result: both CE returns fail

The owner reports `d04dd36fdc7b`: selecting Return in Sega Swirl reboots the
entire console, rather than returning to the collection menu. The Virtua Cop2
combo freezes with `RET CE000004`, detail `00000000`. That is the controller's
initial runtime/integrity validation refusal, before idle-state retirement or
outer-capsule I/O. It currently combines several checks and does not identify
which one failed. Do not call this a working CE-return package or infer that
Swirl completed the same path. Preserve the native-return/loading baseline
and the restored CE PIO helper-loading policy while investigating.

The original Swirl frontend now reproduces d04's reboot in37 executed SH
instructions: its privileged wrapper calls the private MAIGO callback at8300
with argument0 / return address419a6. The controller incorrectly treated that
private argument as BIOS function0 and forwarded it to real firmware. The
follow-up recognizes the source-owned private callback separately, including
the original fallback thunk, while unrelated firmware commands still forward.
This repairs the demonstrated dispatch error; the later3337 console result is
recorded above.

The actual frozen helper initializer and three original raw config reads were
also composed with d04's controller validation. All controller/runtime, raw512
and helper41152 bytes remained unchanged, and validation passed. That does not
locate VC2's later hardware refusal. The follow-up preserves all13 predicates
and reports their failure bitmask on the second RET line. It does not bypass
integrity validation or claim a VC2 repair without the resulting live evidence.

The owner's current scope is to finish the four demonstrated save paths and
their loading/return behavior, then close Smash Pack. The owner reports that
Google lists no save support for the other eight Genesis titles. This is an
owner report, not an independent source/runtime verification; it is not a
request to continue title-by-title save integration. Reopen that work only if
contrary evidence appears. Preserve the lessons in the shared loader policies
and reusable checks; the eventual target remains a universal loader.

## d70 hardware result: native return works; CE regressions remain

The owner reports that `d70c4cf1acc0` works with the speed improvements
"more or less" retained. This accepts the previously failing native return
as reported; there is no new exact timing or phase-TXT capture. Preserve its
strict original-video transport and the shared allocation epoch.

Virtua Cop 2 is reported broken again. The failure point and whether this was
a fresh launch or a launch after another game were not supplied. Do not mark
VC2 accepted from helper hashes alone. The audit found a separate unqualified
change in the helper-hydration raw reader: it switched from the accepted
forced-PIO supply to DMA/poll. Ordinary CE reader and frozen helper bodies
match the accepted route, but that does not prove hardware equivalence of the
complete loader. The proposed narrow repair restores the accepted hydration
policy while retaining the outer loader's current batched DMA reads.

Swirl return briefly blacks the display, then shows a distorted frozen image.
The existing normal CE E0 exit enters a terminal diagnostic renderer; it does
not implement a menu reload. This is an unfinished return path, not evidence
that a completed CE return was tested successfully. A return controller must
prove complete helper/reader idle before freezing traffic and retiring CE.

Red RET flashes on otherwise successful returns are diagnostic progress drawn
by the loader, not game errors. The follow-up records these checkpoints in RAM
and retains visible fault reports only for failures. VMU Tools entry also
unconditionally reads SD preferences while blocking navigation; the follow-up
loads those settings when their rows are selected. Neither UI change is a
new native game-loading speed claim.

## 9f hardware result: 18-second selection; returned-outer video refusal

The owner reports about 18 seconds from native selection to the Start Game /
Resume menu on build `9f470e548ad2`. Native return still fails: the owner reports
`RET B4C2CD76`, detail `00000000`. This is a reported whole-selection time,
not a new phase-TXT total or acceptance of the loading/return goal.

The reason exactly matches `CRC32("SMASH TIMING ORIGINAL VIDEO REFUSED") |
0x80000000`. The returned outer reached its loader preparation and attempted a
KOS-only VID1 capture from the game's current PVR state. The source regression
was reproduced: the original KOS framebuffer-zero state passes, while nonzero,
blanked and 320×240 game states fail that guard. This identifies the 9f refusal;
it does not retroactively identify the individual stage behind 5ca's broad105.
See [the evidence record](evidence/smash-native-return-9f470e548ad2.json).

The d70 candidate carried the strict original 80-byte VID1 instead. Its
diagnostic return runtime grows from 516 to 596 bytes, all covered by its CRC;
the restored outer has a blank VID1 slot immediately after its 368-byte return
seed. The gate copies the validated record only after the outer capsule CRC
passes. Initial GUI capture remains unchanged; returned outer uses the carried
KOS record rather than recapturing game video. No video-mode guard is widened,
no timing is synthesized, and the worker/reader code remained unchanged. Its
later native console result is recorded above. The current follow-up makes
successful progress markers RAM-only.

## Historical 5ca result: broad return progress window

The owner tested build `5ca9697` with Shining Force and selected Return to Menu.
The photographed last display was `RET 00000105`, detail `00000000`. Startup
seemed marginally better, but no exact new elapsed time was reported. Native
return still fails; this build is not accepted as a return or loading solution.

`105` is a progress marker, not an error code. In 5ca its uninstrumented span
covers the raw-reader probe, outer-capsule reads, complete CRC checking and
transfer into the outer entry. The photo does not identify which individual
stage stopped or prove that all those operations completed.

The subsequent 9f visibility build added granular return progress and made outer-stage
progress visible on the current framebuffer. Return-only outer markers110→116 distinguish C entry through the BOOT2 relay.
Outer error reason is `CRC32(message) | 0x80000000`, with the original detail. It retains the 5ca allocation-epoch behavior and
accepted 72 KiB worker, CE helpers and ordinary readers. This is a visibility
diagnostic, not a return repair or an additional speed claim. Its later console
result is recorded above. See the [marker map](smash-card-session-build.md#return-progress-interpretation)
for the new105→10A boundaries.

## c375 hardware result — 2026-11-05: incomplete improvement; native return unresolved

The owner tested Shining Force only: selection to the next Start Game / Resume
screen was about22 seconds rather than30. Returning from Shining Force to the
Smash Pack menu was broken, so the proposed multi-game sequence was not run.
This candidate fails acceptance: native return is broken, and selection
latency remains excessive. The owner said the preceding ZIP may also have
frozen on return; its first affected build is uncertain. Earlier working
switching is a historical baseline, not proof that858 was good. Do not attribute
the failure specifically to the new GIN/session path.
Do not treat the linked transport proofs as console
return acceptance or the8-second reduction as completion of the speed target.

The attachment `smash-genesis-saves(1).txt` identifies buildc3758c3f826a and lists
ROM SRAM eligibility. It is not the phase trace and cannot establish which
runtime attach path executed or where the22 seconds were spent. The requested
file is `KUI/apps/games/smash-native-phases.txt` from this run. If absent, report
generation itself needs investigation. The owner then clarified the return
failure: the current screen freezes and the SD activity light stops; there is
no reported visible error. This does not locate the exact failing transition.

The read-only return audit found no demonstrated CRT overwrite of the low
return gate. Original return-parser helpers produce the same valid result
from old synthesized and actual GIN Shining Force contexts. Return checks1–4
can park silently before their stop-renderer pointer is trusted; never reaching
the reset hook is also unexcluded. Existing linked transport tests model worker
retirement/quiescence and do not execute gameplay through the real button/reset
sequence. That audit did not diagnose the c375 return failure. The later 5ca candidate
changed the exact native button edge but still failed return, as recorded above.

The correct4096-byte `smash-native-phases(1).txt` has now arrived. It identifies
c3758c3f826a / Shining Force and proves the new route executed: full_admissions0,
attaches1, receipt valid / sequence2, all semantic filesystem counters0,
native_gin_route1 and boot2_calls0. All23 raw endpoints have consistent clocks;
the17.437-second nominal total agrees with17 RTC seconds. See
`evidence/smash-native-phases-c3758c3f826a.json` for exact endpoints and hashes.

The old "Filesystem ownership" label is4.963s: about0.278s precedes witness
replay, and4.685s is fresh replay/identity validation. The costly semantic
traversal was bypassed, but its replacement still costs seconds. Existing
vmu_prep_* counts exclude witness/sidecar direct I/O;515 checked sectors must
not be called total attach traffic. Witness record/run counts were not exported.
Original MDE/GIN is5.678s; capsule SD1.713s; prefix verification/stage entry1.708s;
actual SD/card initialization only0.011s; SDK/allocator proof1.263s. The measured
total excludes pre-callback time, report I/O and later game work; it does not
assign every second of the owner's22-second observation to VMU.

## Pre-c375 comparison baseline — 2026-11-05

Before c375, the owner observed **about 8 seconds with VMU Off versus 30 seconds
with VMU On** from selecting Phantasy Star II or Shining Force to the Start Game /
Resume screen. The credits screen remains visible during the wait. The previous
manual72 build did not resolve this delay. Host read-command reductions are not
accepted as elapsed console improvement. The extra approximately 22 seconds is
the performance target. The later c375 capture and 5ca observation above
supersede this as the latest result; neither met the loading or return target.

Virtua Cop 2 now boots and works on the owner's manual-copy baseline. Preserve
that path and the accepted virtual-VRAM helper. Sega Swirl works, but returning
to the collection menu still crashes; Virtua Cop 2 menu return is also unresolved.
Native menu switching preserved the VMU before the failedc375 candidate above. Other Genesis games' high-score
persistence has not been demonstrated. See `smash-genesis-save-scope.md`.

In that comparison VMU Off used the physical route, while VMU On selected the unified native-child
route. The difference includes GIN/capsule transfer, fresh native cold admission,
card/journal validation, BOOT2, MDE initialization and worker installation, then
subsequent game VMU traffic. Do not equate the difference with a proven BUP-only
bottleneck. A retained credits image does not locate execution within that path.

Build 09a delivered **SMASH_PHASE_DIAGNOSTIC=2**, an automatic TXT diagnostic that
continues into the original game. It times the first MDE payload callback through completion
of native worker installation. Initial wait before that first callback is outside
the measurement. It does not time subsequent SGA/BUP/options work.
It writes `KUI/apps/games/smash-native-phases.txt` after the measured interval.
Unavailable or inconsistent clocks are reported as NA. This is not a speed fix;
its purpose is to distinguish cold-entry overhead from later game VMU work.
See `smash-native-phase-text-test.md` for the one-run procedure and exact scope.

## Current design retained by the video-provenance follow-up: v2 allocation epoch

The owner authorized one collection-lived card admission. The c375 witness
candidate bypassed the semantic traversal but still spent4.685s replaying
metadata/identity. The current source replaces that per-child replay with a
version2 allocation epoch: initial outer entry performs complete filesystem
admission, then a96-byte live receipt carries that admitted allocation authority
through checked owner transitions. Children use `kui_vmu_persist_stage_rebind`;
they do not load or replay the witness sidecar. Direct adoption of the original
GIN-loaded MDE remains, avoiding duplicate native MDE/BOOT2 loading. See
`smash-collection-card-lifetime.md` for the exact boundaries and failure policy.

The normal worker already retains its admitted image/journal extents and current
block CRCs during gameplay. What is new is retaining allocation authority across
the collection's owners. Each owner still builds a fresh backend, checks current
CID, recovers the real journal, validates current save data, checkpoints and
checks final identity before a new durable handoff. No active backend, stale
save snapshot or continuously acquired SCI lease is carried across owners.

The earlier native A+B+X+Y+Start shortcut went directly to the outer-return
gate, bypassing the engine Save Game menu. This policy is superseded by the
original-options restoration above. Complete the RPG save and options menu
Save Game action, then explicitly Return to Menu. The worker must quiesce and retire
before outer reload. The 5ca test failed at the broad105 window; 9f then exposed the returned-outer
video refusal. The carried-VID1 d70 follow-up now has the owner's native-return
acceptance reported above.

All four module roles carry the new configuration; legacy all-absent modules
retain the old path and mixed cohorts are refused. Accepted72KiB worker geometry,
canonical CE helpers and current Main2b4b8c673b7c updater code are preservation
requirements. The 9f test reported about18 seconds to Start Game / Resume and
failed return; d70's later result supersedes that native-return failure.
The owner reports retained speed improvements without a new exact timing.
CE menu return and other Genesis high-score persistence remain unresolved on
the last console-tested build. The new CE controller is a separate follow-up,
with hardware acceptance still pending.
The requested deliverable is a manual-copy ZIP with both identical GUI aliases,
both checked ROOT selectors and the four matched stages/two preserved helpers;
no generated session, save or game data belongs in that ZIP.

The updated TXT distinguishes prefix verification5→10, SD initialization10→11
and allocation-epoch validation12→13. A successful native rebind reports
THIS_OWNER full_admissions0, attaches1, allocation_epoch1, witness sectors/runs0,
native_gin_route1 and boot2_calls0. Receipt sequence advances across valid native
returns. SD/CPU timing remains NA when no SD callback timing was collected;
zero witness traffic is not a measured zero-duration I/O result.

## First automatic hardware timing capture — 2026-11-05

The owner returned the complete 4096-byte `smash-native-phases.txt` from build
`09a098074029`, selecting Shining Force. Worker installation completed. All 24
raw endpoints have consistent PMCR/FRQCR, monotonic cycle/RTC values and valid
clock flags. Nominal total 21.576 seconds agrees with the 22-second RTC span.
Evidence: `evidence/smash-native-phase-hardware-09a098074029.json`.

Largest measured costs: original MDE first payload callback to post-GIN 6.896s;
capsule load 2.091s plus validation 0.669s; filesystem ownership 5.667s;
MDE restore/load 1.377s; SDK/allocator proof 1.289s; base relay 1.125s.
The two journal/image-validation intervals total 0.427s. The original allocator
and CRT interval is 0.776s, and worker allocation/relocation/arm total 0.106s.

The real ownership traversal has 396503 chain steps/FAT entries and 2469 directory
sectors, unlike the tiny earlier fixture. Overall preparation used 2329 single
commands, 341 runs and 8048 CRC-checked sectors. Do not use the historical
1096-to29 fixture result as a representation of this card or elapsed proof.

Initial pre-callback wait, subsequent SGA/BUP/options work and report I/O remain
outside these timings. The 21.576s includes original-game work also present with
VMU Off; it is not a measurement of 22 seconds of VMU-only traffic. Fix the
measured setup costs while retaining original complete ownership/hash/CRC checks.

## 858 startup candidate — marginal hardware improvement, unresolved

The owner reports the858 package is at most marginally faster. The primary
internal-selection delay remains unresolved. No updated phase capture was
provided with that observation; the attached858 file is the Genesis save report.
Do not claim the startup fixes passed hardware acceptance.

The follow-up preserves the measured09 worker and addresses concrete work:
compact aligned outer copying within its existing guard; reuse of verified live
FAT windows without reducing the256-sector metadata cache; a strictly guarded
cached whole-executable CRC with bounded private-stack publication; and removal
of the two informational30-frame holds from normal native startup globally.
All ownership, complete executable CRC, per-block SD CRC and fresh image
validation epochs remain. None of this establishes elapsed console savings yet.
See `smash-measured-startup-next-test.md`. Keep the automatic phase report enabled
for the next single native selection; do not repeat the Off baseline or settled
VC2 compatibility matrix for these temporary-stage changes.

## Why retain collection allocation authority

The GUI selects one backing card/profile and patches it into native, Virtua Cop2
and Sega Swirl children. The858 path rebuilt filesystem ownership for each
child and reloaded MDE/BOOT2 for native selections. The c375 path retained a
metadata witness and GIN continuation, but fresh witness replay remained costly.
Neither candidate met the hardware performance goal.

The current v2 candidate retains admitted allocations through the authenticated
RAM receipt and explicitly rebinds each owner's adapter after quiescence. It
does not retain a mounted filesystem or backend. Do not copy active structs or
reuse the first sealed handoff after saves:
objects contain owner pointers, commits update block CRCs and invalidate the
initial seal. Retain all CRC/durability and identity checks. Do not enlarge the
72KiB worker or steal any stack reservation without an explicit resource design.

See `smash-collection-card-lifetime.md` for the source audit and implementation
boundaries. The historical858 package did not contain this architecture; its
hardware result is not evidence for or against the new v2 path.

The858 Genesis report now covers the actual ten ROMs: Phantasy StarII and Shining
Force each declare8192 SRAM bytes; the other eight declare0 through the original
engine's cartridge-save path. Controller settings remain separate. Evidence:
`evidence/smash-genesis-save-capabilities-858ca7ee3db1.json`. No additional header
extraction is required to answer that eligibility question.

## Source, worker and deployment preservation

Current Main was checked at `2b4b8c673b7c71a58c41e8a79252313940ea944e`.
All 14 production updater files in this test source match that Main byte for byte.
The test branch descends from the recovered 433 updater adoption and 338/e134
Smash integration; the prior 72 source commit itself is not recovered. Its cache
and save-report changes were reconstructed and checked against the accepted 72
behavior. Do not represent that reconstruction as an exact recovered source tree.

The linked native worker and low resident must reproduce accepted 72 code/data
apart from their actual twelve-byte source build label. Native VMU reservation
remains **73728 bytes**. Background and its 192KiB cache are separate existing
components; this test does not replace Standard, Background, disc or CDDA readers.
CE helpers retain their exact canonical SHA256 and actual `ab1b145b33b4` component
label. That unchanged helper is not an older replacement VMU worker.

Historically the owner requested a **manual ZIP** for this test; the standing
ZIP rule above now applies beyond that individual request. That historical
package required identical GUI bytes at `KUI/runtime.kui` and
`KUI/boot.kui`, BOTH valid ROOT activation slots, four matching Smash modules,
and the two exact accepted prepared CE helpers. Keep banks and all user data.
The GUI must embed those same four modules: an older embedded bundle can take
precedence over newer physical files. Validate actual packaged module bytes
through the production GUI admission path before delivery.

The GUI preserves the 338 removal of the separate pre-splash status painting and
shared mutable session/settings files, and uses current Main's engine 3 updater.
No new updater, worker-geometry, universal Background support or CE menu-return
fix is claimed by this diagnostic.

## Historical notes — superseded where contradicted above

The sections below record earlier work and failed candidates. They do not select
the current source, hardware acceptance result, updater engine or delivery recipe.

## Historical updater-only integration and preserved baseline

The source baseline is published main
`0045dd96937ee7485ecd4fe0432b28babea8646a`. This integration ports the updater
without merging the recovered ab1 loader/VMU/Smash development. The owner's
2026-11-04 instruction explicitly limits this merge to updater code.

| Area | Included change | Preservation boundary |
| --- | --- | --- |
| Installed versions | Checked banks, selection records, restart and rollback | Existing root readers and their absence remain shared |
| Package admission | Bounded namespace, sparse inheritance, old26/old34 compatibility | No exact-count gate and no new optional modules |
| Test selection | Folder browsing and one embedded .kup per ZIP | Legacy manual-copy ZIPs require conversion |
| GUI restart | Fully verify/reload selected GUI directly | Updates action120; restart action126 |
| Module paths | Three bank-aware existence checks in the existing launcher | No title-specific routing or new backend selection |
| Reader admission | Exact verified Standard/CDDA identities in the GUI | No reader code, build flags, memory geometry or binary changes |
| Root migration | Preserve system/optional boot override, then copy runtime | No timing-sensitive button required; all user files retained |

`docs/evidence/updater-preserved-main-source.json` records main's protected
source blobs and the four deliberately reviewed GUI glue paths. All other
protected sources and reader recipes must match main byte for byte. The four
binding files under resources/updater-bindings retain exact admission for the
already installed 233,964-byte Standard reader and native CDDA aliases.
These manifests are not permission to replace readers or infer an allowlist
from arbitrary SD files.

The owner confirmed that the earlier root repair GUI06cbd036bf0d booted and
resolved the reported cohort error. That binary remains their working repair;
this source-only main integration is a separate narrow port. No replacement
reader or new console test matrix is required. The owner reported a longer
than desired installed-version selection screen; this merge does not claim
to resolve its latency. A completed console CDDA-package round trip and
universal in-game return are not established by the root-boot confirmation.

The failed full-cohort updater substituted a wrong Standard reader and added
presence-sensitive modules. Its release/build recipes are not included here.
GUI-only packages and the manual root repair contain no KUI/apps files.
Readers, VMUs, journals, game metadata and settings are left in place.

The accepted compact 72-KiB VMU and combined284-KiB/cache192-KiB installations
remain distinct from VMU-only operation. This updater does not change any
service reservation, promote session-controls variants or add a universal
cacheless profile. Native/disc/CDDA/WinCE support remains route-specific.

## Current Smash test branch

The latest tested candidate failed hardware acceptance on 2026-11-05
(the running build ID was not provided): collection startup improved to 8 seconds, but Phantasy Star II
took 29 seconds and Shining Force 30 seconds to reach Start Game. Virtua Cop 2
loading and collection-menu return both failed. No unified four-game success,
native selection speedup, VC2 transition fix or CE menu return is accepted.
Host packaging, CRC and execution-model checks do not supersede these results.
See the hardware result at the top of `shared-launch-next-test.md` before using
the historical candidate description below.

Main remains758f62c43d76; the following is test-branch work, not a claim that
these game changes have been merged or accepted on the console. The owner
reported15-second collection startup,15–20-second native selection delays,
VC2 stopping and Swirl parking on return with the136 GUI-only delivery.
That delivery inherited root modules without establishing the actual cohort.

The next Main26 GUI update contains four compressed same-commit Smash stages;
each CE stage contains its matched raw helper. Root Standard/Background/disc
readers and raw helper files remain unchanged. Actual linked bundle references,
module/helper bodies and GUI routing must be checked, not just package headers.
The old26-role root dispatcher must still cold-boot the new GUI receipt.

Retain the confirmed10ef virtual-VRAM halfword-copy helper. Its ab1 runtime
body differs only in its build label. Normal CE children restore that direct
E0 terminal context and omit the later unqualified low return observer. CE
collection-menu return remains unimplemented; VC2 cold entry remains pending
console confirmation. Do not claim either was fixed by host admission tests.

Native cold transitions can reuse an original CRC-checked MDE read through
temporary1,848,288-byte scratch, followed by fresh complete source SHA admission.
Persistent72-KiB worker geometry is unchanged. Hot GUI mapping deferral requires
the new stage's complete-source-owner contract; old/missing/ambiguous proofs
retain fresh preflight. Capsule I/O now uses32KiB windows with4KiB fallback.
Refer to docs/shared-launch-next-test.md for the test scope and evidence limits.

## Robust updater v2 follow-up

The next updater-only change fixes the contract rather than adding another
catalog exception. The source remains the published-main updater; no reader,
stage, memory geometry or reader recipe is rebuilt or merged here.

Partial packages materialize a checked snapshot of the actual running ROOT or
BANK plus intended changes. FULL packages are independent of the prior bank,
can replace corrupt prior files/receipts, and preserve absence for omitted
software. Component builds are independent; configuration/content determines
package and snapshot identities. Raw helpers, official audio/assets and data
catalogues are supported. The current C consumer preflights every generated
KUP before delivery; default test/release producer paths create v2.

Root engine2 is a one-time setup requirement. Its startup floor skips older
GUI banks instead of reinstalling their decoder. Normal tests then install a
standalone KUP and restart directly. The current136 GUI adoption preserves
all its game/UI code and startup reporting, ports only Main updater code,
and uses the exact26 original module/helper binaries without rebuilding them.
That adoption is a test deliverable, not a merge of its game code into Main.

## Startup latency follow-up

The owner reported about five seconds at the pre-splash version-selection
screen. There is no fixed delay in that function. Current code could reread
one refused multi-MiB bank GUI for both activation slots, rescan the EMPTY
slot after validation, and remount an already pinned boot device for discovery.
The updater-only follow-up deduplicates refused GUI sources after each slot's
handoff attempt, reuses the checked slot offset, uses the existing equivalent
fast CRC only in updater paths, and opens the pinned transport directly.
KUI/updater-startup.txt records phase timings so another video is unnecessary.
Cold selection still reads and verifies a distinct bank GUI once; no console
timing improvement is asserted from host read-budget or integrity tests.
