# Checks before a Background launch: K-UI aaaba0ea3640

Status: K-UI `aaaba0ea3640` (commit `aaaba0ea`) packaged 27 November 2026,
09:18 CST. It is K-UI `53a33ec708ab` (packaged 27 November, 02:52 CST) with
two changes: a large program K-UI cannot check is refused for the Background
reader instead of started unchecked, and a refused launch's reason is logged
again at the end of the log. Those two have host checks only.

Console results so far, both of 27 November 2026: `d41a04f40f23` (packaged
26 November, 23:56 CST; the first four commits below) keeps Tokyo Xtreme
Racer off the combined worker, and its SDK check did not reach Plasma Sword
("Console result" below); `53a33ec708ab` refused Plasma Sword by its
large-program check ("Console test" in its section). Each build replaces
K-UI itself (`KUI/boot.kui` and `KUI/runtime.kui`, identical) and nothing
else: no reader module, no updater package, no user data.

The sections up to "Console result" are the record of `d41a04f40f23` as it
was delivered. Where the console showed one of its statements to hold less
widely than written, a note says so.

It answers the owner's request of 27 November 2026 ("Look at this handoff and
anything that changed with my git. I want you to prepare the next zip") by
the handoff's own decision: keep Tokyo Xtreme Racer off the combined
Background worker when the game cannot spare the memory, say so when the
reader's SDK check fails and point to the Standard reader, and show what
that costs a player who asked for virtual saves. The audible CD music build
(Test 85) is the next delivery, not this one.

## Why

The owner's controls of 26 November 2026, K-UI `64ae07435de5`
(`cdda-background-test-84.md`, Controls and POD SpeedZone):

- Tokyo Xtreme Racer stays on a black screen under the Background reader on
  an image that matches the PC reference, and boots on Standard with Virtual
  VMU Off. The combined module's stage takes its lease, 284 KiB with a
  32-byte header, from the bottom of the game's own SDK pool. This game's
  pool is 448 KiB and its startup asks it for 458,480 bytes.
- Plasma Sword ends on the stage's stop screen (phase 2, detail 0xA) under
  the Background reader and boots on Standard with Virtual VMU Off. Its
  Maple library (pd 1.09) is not one the SDK check recognizes. K-UI ran that
  check before the launch, got the refusal, and started the combined module
  unseeded all the same; the stage then ran the same check on the loaded
  executable and stopped.

## What changed

Four commits, K-UI sources only (`src/apps/games_native_cache.inc`,
`games_native_cache_hint.inc`, `games_native_cache_alias.inc`,
`games_retail.c`). No reader input changed: the distribution fingerprint is
still `2c9b59c3...` (`python3 tools/stage_distribution.py
--print-reader-inputs`).

1. `6a3aecb1`: a pool the lease would starve. K-UI does not offer the
   combined module where its lease and header would take more than a quarter
   of the SDK pool. The pools of the 426 programs are 4 MiB or more, or 512
   KiB or less (200 to 208 KiB, 448 KiB, 512 KiB); none lies between, and
   the quarter falls in that gap for all three lease sizes (1.11 to 1.33
   MiB). A reviewed fixed arena and an arena lease are not SDK pools and are
   not held to the rule. The Standard module's own lease is not changed.
2. `6bed5c43`: an executable the SDK check does not recognize. When K-UI's
   own complete check fails for a combined module (the executable's extent
   is not proved, or SDK discovery fails), the launcher no longer starts the
   module to let its stage stop. A source that could not be read, a module
   without a seed, a launch that is not eligible and a failed allocation
   gave no answer and still leave the decision to the stage.
3. `71efce2d`: every refusal names one whole setting that starts the game
   (review correction; the table below is its result).
4. `d41a04f4`: a combined module is seeded only with the image's own saved
   mapping (review correction). With Virtual VMU On the launcher could skip
   its own check when the cache directory held exactly one record that fit,
   which could be another game's, and then published that record under this
   image's alias. The stage rejects such a record and rediscovers, so
   nothing was loaded wrongly, but the launcher's answer was never asked:
   both checks above, and the older overlap refusal, could be passed by.
   Now only the image's own indexed alias is a hint for a combined module;
   those aliases have names of their own, so an alias written before this
   change is not read. The Standard module's scan and names are unchanged.

What the launcher does when the combined module is not offered is one table
(`native_cache_refusal`, `games_native_cache.inc`). The reader-only New
module (`retail-native-standalone.kui`) takes nothing from the game's pool
and needs no SDK check; it runs under New (X) where the game has the SDK
SCI family and the module is installed ("runs" below).

| Answer | Virtual VMU | The reader-only module runs | New (X), SCI family, module absent | Legacy (Y), or no SCI family |
|---|---|---|---|---|
| Pool too small | On | refused: "Set Virtual VMU Off; saves then need a real VMU" | refused: "Set Background loading Off and Virtual VMU Off; saves then need a real VMU" | the same |
| Pool too small | Off | starts the reader-only module | refused: "Install the standalone New reader, or set Background loading Off" | refused: "Set Background loading Off" |
| SDK not recognized | On | starts the reader-only module, without a virtual card (as before) | refused: "Set Background loading Off and Virtual VMU Off; saves then need a real VMU" | the same |
| SDK not recognized | Off | starts the reader-only module (as before) | refused: "Install the standalone New reader, or set Background loading Off" | refused: "Set Background loading Off" |

The first sentence of a refusal is "This game's memory pool is too small for
the Background worker." or "Background loading cannot recognize this game's
system library." Each is 63 characters, so with "Retail boot: " it fills one
76-column log line and the setting is the next line. A refused launch writes
nothing to the card: no mapping record, no alias, no launch report. An
executable whose extent is not proved is refused even with the SCI family
(three of the 426); offering it the reader-only module would be a route of
its own.

## Checks

Recorded in `docs/evidence/kui-launch-checks-d41a04f/`.

- **Unit tests** (`make test-games-native-cache`,
  `test-games-native-cache-hint`): the pool rule at the three lease sizes and
  its edge; the exclusions; saved records; the preflight's answer for both
  Background readers, the Standard module and the launches without an
  answer; the table above in all 56 states; for a combined module, no hint
  from a scanned record, from an alias under the earlier name or from a
  starved pool's record, and a hint from the image's own alias.
- **The launcher on card images** (`python3 tests/test_games_retail.py`,
  FAT32 and exFAT; `make test-games-native-route`): the real refusal of a
  synthetic executable in five cases; an authored 448 KiB pool and the real
  refusal on an empty card and on a card that holds one saved record of
  another game (22 launches). Every refusal leaves the card byte for byte as
  it was. The host launcher is built without the virtual-card code and
  refuses a Background launch with Virtual VMU Off before any of this, so
  these rows are Virtual VMU On; the Off rows are held by the table's unit
  test.
- **Existing cases changed on purpose.** The route harness's executable is
  synthetic and never passes the SDK check. Its generic Background cases
  relied on K-UI starting such a game through the combined module; they now
  model a module without a seed, which K-UI does not ask. The seeded cases
  are the reader-only route, the failed read, the cancel and the refusals.
  The hint tests that seeded a combined module from a scan now seed it from
  the image's own alias.
- **Mutations:** 29 deliberate breakages of the delivered sources, each
  alone, in a clean checkout: all 29 fail a test (`host-checks.txt`).
- **Original executables** (426 boot programs of the owner's CD-audio
  archives, kept outside the repository): the production preflight, through
  the repository's fixture I/O, with the owner's New reader module of 26
  November (Test 84's `4abc63b0cc58`, CRC `BAFAAF7C`) and with the release's
  `retail-native-dual.kui` (`f8807cb16561`), for both Background readers and
  both Virtual VMU settings: 1,704 launches each (`corpus.txt`).
  - Pool rule: nine programs (Tokyo Xtreme Racer, Tokyo Highway Challenge,
    Shutokou Battle, Buzz 2000 in four regions, two Pro Yakyu Team titles).
  - SDK rule: the 304 programs that were started unseeded. 243 have the
    reader-only route under New (X) and keep it where the module is
    installed; the other 61 (Plasma Sword in two regions, three with an
    unproved extent) are refused; under Legacy (Y) all 304 are refused.
    (This run had no memory limit. On the console K-UI cannot hold a
    program larger than its free memory and gave such a program no answer:
    Plasma Sword was started and stopped in the reader. Of the 426, 10
    programs are always that large and 15 more may be. See "Console result"
    and the large-program check below.)
  - No launch that K-UI's check admits changes: 320 launches stay ready
    with the owner's module, 308 with the release's (which is too old for
    three programs, as before), 132 keep the overlap answer.
- **The build:** `bash tools/build_distribution_gui.sh --candidate` and
  `tools/runtime_package.py` from a clean checkout of `d41a04f4`, after
  `python3 tools/check_gui_lines.py` (Main `3f757bc2`, this line, the Windows
  CE line `603c53e9`: contained). Main's K-UI was built the same way from
  `f41f9744` (Main with one documentation commit). Of the 888 objects of the
  two builds, without their debug sections, 885 are identical, two differ
  only in the build ID text and one, `games_retail.o`, differs otherwise
  (`objects.txt`).
- **The file:** envelope, updater engine 3, bank capability, the embedded
  Smash bundle `cc81e63ca4d2` identical to `64ae07435de5`'s, the KallistiOS
  banner (`delivery-check.txt`); `tools/stage_distribution.py --verify-gui`
  against the release's staged readers (the Standard binding, 1,418 checks);
  the launcher's admission helpers and its seed slot on the actual
  `retail-native-dual.kui` of both builds, `retail-native-dual-a1.kui`,
  `retail-native-standalone.kui` and `retail-boot.kui` (`admission.txt`).
- **A clean checkout** of `d41a04f4`: `make test-shared-game-launch`,
  `test-games-native-cache` and the route harness pass (`host-checks.txt`).
- **Review:** one independent reviewer read the first two commits and the
  code around them, without this document (`review.md`). It found the hint
  path of commit 4 and the two refusals of commit 3 that did not lead to a
  launch in one step; all three are corrected and tested, and the K-UI was
  rebuilt from the corrected source. Its smaller remarks are answered there.
  The two correction commits were not reviewed a second time.

Main's K-UI source has not changed since the 3.0 RC1 build commit `93f13e84`
(`git diff 93f13e84 3f757bc2 -- src include Makefile.dc` is empty), so this
K-UI is the RC1 source with the four commits. Against `64ae07435de5`, the
K-UI on the owner's test card, it also carries RC1's K-UI changes: the
version line now reads `K-UI 3.0 RC1 SD runtime <build>`.

## Package

`KUI-Background-Launch-Checks-26NOV2026-2356-CST-d41a04f.zip` (manual copy, 11,310,218 bytes, SHA-256 `10e0cb4b...a80d`):
`KUI/boot.kui` and `KUI/runtime.kui` (identical, 3,836,452 bytes, payload
CRC32 `AE58ADA7`, SHA-256 `7f638682...03d4`), `IF-NEEDED/KUI/` with the
owner's current K-UI `64ae07435de5` to go back, `README.txt`,
`INVENTORY.json`, `SHA256SUMS.txt`.

A first build of the two checks alone, `6bed5c43b48a`, was packaged and held
back for the review; it was not sent.

## Console result, 27 November 2026 (K-UI d41a04f40f23)

The owner's report, with one photograph and no log
(`docs/evidence/kui-large-program-53a33ec/console-2026-11-27.md`):

> Plasma sword triggered this with background reader on. Tokyo Xtreme racer
> boots though, but I couldn't get it to trigger a report

- **Tokyo Xtreme Racer boots** with Background loading on, where it stayed
  on a black screen under `64ae07435de5`. This is the first console result
  for the pool rule. The route is inferred: with Virtual VMU On this K-UI
  refuses the launch, so the game ran with Virtual VMU Off through the
  reader-only New module. That module records no stream survey, which is
  the "no report": the survey is the combined module's, and that module is
  what this game's 448 KiB pool has no room for.
- **Plasma Sword still ends on the stage's stop screen** (Test 84's module
  `4abc63b0cc58`, phase 2, detail 0xA), the launch this K-UI was built to
  refuse. K-UI's check needs the whole program in memory. Plasma Sword's is
  9,514,104 bytes; K-UI has 7,706,808 bytes available at startup and
  6,046,096 after some use (the owner's log of 22 November). The allocation
  fails, the preflight returned "no answer" without a log line, and the
  combined module was started unseeded. The host checks above ran without
  that limit and reported a refusal the console never gives. No log of
  this run shows the failed allocation; the reading is from the code and
  the earlier log. Phase 2 is the stage's SDK check, so this was a Virtual
  VMU Off launch: with it On the stage stops before that (limit 9).

## A program too large to hold: K-UI 53a33ec708ab

### What changed

Two commits on `d41a04f4`, K-UI sources only (`src/apps/games_native_cache.inc`,
the new `games_native_witness.inc`, one line of `games_retail.c`). No reader
input changed: the distribution fingerprint is still `2c9b59c3...`.

5. `0c7a9db7`: when K-UI cannot hold the program of a combined-module launch,
   it reads it once in 16-sector pieces, as the launch read it for its CRC
   in any case. That pass proves the CRC and looks for the witness of the
   SDK check's first step: one even offset, from 0x53c to the file's last
   0x1b8 bytes, at which the Maple discovery's 32 instruction words match
   (`words` and `sdk_at`, `src/core/vmu_native_discovery.c`).
   - No such site in the file: the discovery fails for the whole file and
     for every proved prefix of it, the stage would stop, and the launcher
     refuses with the SDK message of the table above. The reader-only
     module is not offered: its SCI family is not looked for in a program
     that is not held.
   - A site: no answer, as before. The rest of the check needs the whole
     program.
   - The CRC of that pass is the launch's; the program is not read twice.
   - A program K-UI holds is checked exactly as before. The Standard
     module, and a launch that needs no CRC, are never read in pieces.
   The launcher's table is a copy of the discovery's, because the
   discovery's source is a reader input. `tests/test_games_native_witness.py`
   holds the copy and its four bounds to the source and pins the discovery,
   the program extent, the layout step and the stage's check whole.
6. `53a33ec7`: review corrections. One log line, a compile-time check that
   a piece's destination keeps the allocation's alignment, tests and the
   Makefile. No launch decides differently.

### Checks

Recorded in `docs/evidence/kui-large-program-53a33ec/`.

- **Unit tests** (`make test-games-native-cache`, 149,262 checks): a program
  of six pieces without a witness is refused for both Background readers
  and Virtual VMU settings in one pass with the right CRC; a witness at 60
  places (the first and last site, around every piece's edge) and at eight
  program lengths gives no answer; a place the discovery never tests and
  each word changed are no witness; the Standard module and a launch that
  needs no CRC are not read; a piece that fails, a cancel at every visit
  and each failed allocation give no answer and no CRC. The real
  `kui_vmu_native_discover` runs beside the search on 851 driver bodies it
  counts, written from its source: the search finds every one, and both
  refuse each with one word changed.
- **The launcher on card images** (`make test-games-native-route`, 34
  launches on FAT32 and exFAT): the whole-program allocation is refused, as
  on the console. Without a witness the launch is refused with the SDK
  message whatever is installed, the card stays byte for byte as it was,
  and the reader-only module is not asked. With a witness planted across
  the program's two sectors the launch goes on with the CRC of the one
  read, recomputed independently, and no second pass.
- **The guard** (`tests/test_games_native_witness.py`): the table, the
  bounds, the discovery's loop, the stage's use of it, and five pinned
  pieces of that code.
- **Mutations:** 45 deliberate breakages of the search, the pass, its use in
  the launcher, the discovery and the stage, each alone in a clean
  checkout of `53a33ec7`, run against all three tests: 44 fail a test. The
  one that passes is no change in behaviour (`host-checks.txt`).
- **Original executables** (the 426 boot programs, `corpus-large.txt`): the
  production preflight with the owner's New reader module and the
  whole-program allocation refused above a limit, 1,704 launches a run.
  - Every program read in pieces: 73 have no witness and are refused; all
    73 are programs the whole-program check refuses too; none of the 80
    programs it admits, nor any with a pool answer, is refused. An
    independent probe that includes the discovery's source finds no site
    with all 32 words in exactly those 73. The CRC handed to the launch
    equals an independent CRC of the file in all 1,704 launches.
  - Limit 7,704,576 bytes (K-UI's memory at startup): 416 programs are held
    and answer exactly as under `d41a04f40f23`; 10 are read in pieces. Four
    are refused (Plasma Sword and Star Gladiator 2, two regions each); six
    have the witness and are started unchecked (Evil Dead, Incoming, NBA
    Hoopz).
  - Limit 4,730,880 bytes (after some use): 401 held, 25 in pieces. Nine
    refused (those four and Shenmue in five releases); 16 started
    unchecked (those six, Mortal Kombat Gold, Mat Hoffman's Pro BMX, Aero
    Dancing i, L.O.L., Expendable, Millennium Soldier). The whole-program
    check refuses all 16 for the combined module.
- **The build:** `bash tools/build_distribution_gui.sh --candidate` and
  `tools/runtime_package.py` from a clean checkout of `53a33ec7`, after
  `python3 tools/check_gui_lines.py` (Main `3f757bc2`, this line, the Windows
  CE line at Main: contained). Of the 888 objects, without their debug
  sections, 885 equal those of `d41a04f40f23` and of Main's K-UI, two
  differ only in the build ID text and one, `games_retail.o`, differs
  otherwise (`objects.txt`).
- **The file:** envelope, updater engine 3, bank capability, the embedded
  Smash bundle `cc81e63ca4d2` identical to `d41a04f40f23`'s, the KallistiOS
  banner (`delivery-check.txt`); `tools/stage_distribution.py --verify-gui`
  against the release's staged readers (1,418 checks); the launcher's
  admission helpers and its seed slot on the actual modules
  (`admission.txt`).
- **A clean checkout** of `53a33ec7`: twelve launcher suites, fifteen
  source-policy tests and `make test-shared-game-launch` pass
  (`host-checks.txt`).
- **Review:** one independent reviewer read `0c7a9db7` and the code around
  it and wrote its own harnesses (`review.md`). It found no defect in the
  pass, its CRC or the refusal. Its remarks on the tests, a log line and
  the Makefile are `53a33ec7`. Its main finding is older than this change
  and is limit 9 below.

### Package

`KUI-Background-Large-Programs-27NOV2026-0252-CST-53a33ec.zip` (manual copy,
11,319,841 bytes, SHA-256 `bc0d09a8...3acf`): `KUI/boot.kui` and
`KUI/runtime.kui` (identical, 3,836,804 bytes, payload CRC32 `F8B6923C`,
SHA-256 `aca70820...2060`), `IF-NEEDED/KUI/` with the owner's current K-UI
`d41a04f40f23` to go back, `README.txt`, `INVENTORY.json`, `SHA256SUMS.txt`.

A first build of the check alone, `0c7a9db755c1`, was made and held back for
the review; it was not sent.

### Console test

27 November 2026, the owner: "The Background reader did reject Plasma Sword.
Couldn't really read what it said so I saved the diagnostics." The saved log
names K-UI `53a33ec708ab` and module `4abc63b0cc58`
(`docs/evidence/kui-large-games-aaaba0e/console-2026-11-27-53a33ec708ab.md`):

    Native reader: 9514104-byte program is too large to hold; reading in pieces
    Native reader: the SDK's Maple code is not in it; the stage would stop
    Retail boot: Background loading cannot recognize this game's system library.
     Set Background loading Off

- The check works on the console: no stop screen, the launch refused with
  the setting, Virtual VMU Off. The one read in pieces took 12.3 s of the
  launch's 19.4 s.
- K-UI had 7,700,480 bytes unclaimed at startup and 4,530,176 when the log
  was saved; the two limits of the host run are within 4,096 and 200,704
  bytes of these.
- The reason was not readable on the screen: thirteen lines of launch
  timing followed it. That is the second change of the next build.
- Tokyo Xtreme Racer, ChuChu Rocket and the Standard launch of the README
  were not reported.

## Large programs K-UI cannot check are refused: K-UI aaaba0ea3640

The owner, 27 November 2026, with that result: "We can come back to the big
games later [...] Please make it so they don't launch with background reader
on until we figure that out."

### What changed

Two commits on `53a33ec7`, K-UI sources only (`src/apps/games_native_cache.inc`,
`src/apps/games_retail.c`, the new `include/kui/games_retail_refusal.h`,
`src/dreamcast/main.c`). No reader input changed: the distribution
fingerprint is still `2c9b59c3...`.

7. `ab7fbaa3`: a program read in pieces in which the Maple witness is found
   had no answer, and the combined module was started unchecked (limit 8 as
   it stood). It now has the answer "unchecked", and the launch is refused
   before anything is written to the card:
   "This game's program is too large for the Background reader yet." with
   "Set Background loading Off", or with Virtual VMU On "Set Background
   loading Off and Virtual VMU Off; saves then need a real VMU". The
   reader-only module is not offered (limit 10). A program K-UI holds is
   checked exactly as before.
8. `17f33afc`: the launcher keeps the reason of a refused launch, and K-UI's
   main program logs it again after the launch timing, for the three Games
   launch actions. Only `Operation ended. Diagnostics page: Y saves the log
   to SD.` follows it.

The K-UI's source is `aaaba0ea`, one commit later, which adds
`include/kui/toy_cdda_card.h` for Test 85; no K-UI source includes it. The
line check reads the pushed branch, so the K-UI was built from its head. It
equals the build of `17f33afc` in everything but its build ID.

### Checks

Evidence: `docs/evidence/kui-large-games-aaaba0e/`. Host checks only.

- **Launcher suites** (`host-checks.txt`), clean checkout of `17f33afc`:
  all pass; unit test 149,278 checks; route test 38 launches on FAT32 and
  exFAT, the large rows now refusals with their exact text.
- **Mutations** (`host-checks.txt`): 8 on the two commits; 7 fail a test.
  The eighth, the reason not cleared at the launcher's entry, passed: no
  test ran two launches in one process. Commit `72efef94` adds that to the
  route test, which now fails with it. `src/dreamcast/main.c` is not built
  for the host; its part is read, not run.
- **Original programs** (`corpus-large.txt`): the preflight of `aaaba0ea`
  on the 426 boot programs, 1,704 launches a run, against `53a33ec7`
  launch by launch. With the console's startup memory 24 launches change
  (6 programs: Evil Dead - Hail to the King, Incoming, NBA Hoopz, two
  versions each), from started unchecked to refused as too large; with
  the low-memory limit 64 launches (16 programs, adding Mortal Kombat
  Gold, Mat Hoffman's Pro BMX, Aero Dancing i, L.O.L., Expendable and
  Millennium Soldier). Every other launch answers exactly as before:
  result, route, both messages, saved record, reads and CRC.
- **The file** (`delivery-check.txt`, `objects.txt`): engine 3, the Smash
  bundle and the Standard binding of the owner's K-UI; of 888 objects two
  differ from `53a33ec708ab` (`games_retail.o`, `main-runtime.o`) and none
  from the build of `17f33afc` but for the build ID.
- **Reader admission** (`admission.txt`): the launcher's helpers on the
  owner's module `4abc63b0cc58` and the release's modules, line for line
  as recorded for `53a33ec708ab`; `--verify-gui` 1,418 checks.
- **Lines** (`objects.txt`): `tools/check_gui_lines.py` from the clean
  checkout before the build: Main, the Windows CE line and this line
  contained.

### Package

`KUI-Background-Large-Games-27NOV2026-0918-CST-aaaba0e.zip` (manual copy,
11,321,169 bytes, SHA-256 `ae4f29b2...8fbd`): `KUI/boot.kui` and
`KUI/runtime.kui` (identical, 3,837,028 bytes, payload CRC32 `DA47C74C`,
SHA-256 `b79f53ab...42ed`), `IF-NEEDED/KUI/` with the owner's current K-UI
`53a33ec708ab` to go back, `README.txt`, `INVENTORY.json`, `SHA256SUMS.txt`.

Its README says the log on the screen "ends with the reason". The closing
line `Operation ended. Diagnostics page: Y saves the log to SD.` comes after
it; the owner was told so with the delivery.

A build of `17f33afc`, `17f33afc5e1f`, was made first and not sent.

### Console test

Pending. The README asks for the Diagnostics line, Plasma Sword with
Background loading on (the refusal, now readable at the end of the log)
and, only if one is at hand, Evil Dead - Hail to the King, Incoming or NBA
Hoopz with Background loading on (refused as too large).

## Known limits

1. Tokyo Xtreme Racer through the reader-only module is untested on a
   console. The module takes no lease; the failure the host reading named
   cannot occur there, but nothing else about that route is proved.
2. The Standard reader with Virtual VMU On has a worker lease of its own.
   Where its module takes it was not examined here; that route is not
   changed and is untested for the nine games. The refusals therefore name
   Virtual VMU Off.
3. The pool rule is a size class, not a measurement of what a game needs.
   A 448 KiB pool fails on the console; 512 KiB and 200 KiB pools are
   refused by the same rule without a console run.
4. K-UI's answer is its own check's. A module whose stage recognizes more
   than K-UI's check must be delivered with its K-UI (`gui-lines.md`,
   rule 3).
5. Speed, Virtual VMU On, combined module. Under this K-UI the first launch
   of each image reads and checks the executable in K-UI once more (its
   saved record is still found by the executable's hash) and then writes
   the new alias; a card on which the scan used to find exactly one record
   loses that shortcut for good. A cooked image never gets an alias from a
   fresh check, so K-UI checks it on every launch; before, that was so only
   where the directory held more than one fitting record. Not measured on
   the console.
6. An alias is the image's own by its path, boot position, boot length and
   descriptor CRC. An image replaced in place with those four unchanged
   keeps its alias; the stage's hash check still rejects a record of other
   bytes and rediscovers.
7. A launch with the boot encoding set to Plain is not eligible for the
   preflight, as before: K-UI gives no answer there and the stage decides.
8. K-UI checks a program whole only when it can hold it. The console leaves
   it 7.7 million bytes at startup and less after use, so of the 426
   programs 10 are never held and 15 more may not be. For those K-UI can
   only rule a program out, where the Maple witness is missing. With the
   witness it could give no answer and, up to `53a33ec708ab`, started the
   combined module unchecked: 6 to 16 programs of the 426, all of which
   the whole-program check refuses for that module. Since `aaaba0ea3640`
   it refuses these launches as too large. It still cannot check them:
   a large game the Background worker could run is refused with them.
9. With Virtual VMU On, a launch without K-UI's answer cannot start at all.
   K-UI prepares a virtual card only for a game it has checked, so the
   combined module carries its build's template profile and its stage
   stops at storage start ("NATIVE VMU PREPARED PROFILE / GAME MAP
   REFUSED"), before the program is loaded. This is older than these
   builds and holds for every launch without an answer; for a large
   program with the witness it is where a Virtual VMU On launch ends. By
   reading and by a host run of the stage's admission on the template
   profile (`review.md`, F1); not seen on a console. K-UI could refuse
   these launches with a message; that is a change of its own, not made.
   Since `aaaba0ea3640` a large program is refused before this point; the
   limit stands for every other launch without an answer.
10. A program K-UI cannot hold never gets the reader-only module: its SCI
    family is not looked for. The refusal names the Standard reader.
    `d41a04f40f23`'s count of 243 programs that keep the reader-only route
    holds on the console only for the programs K-UI can hold.
11. The launcher's witness is a copy of the discovery's table. A reader
    whose discovery accepts another Maple family, skips its check in some
    mode or takes a longer program must change the launcher's search in
    the same commit; the guard test fails until its pins are answered.
12. Which programs fall under limit 8 depends on K-UI's free memory at the
    launch, which no log of a refused launch has shown yet. The two limits
    used on the host are the two values of one earlier log.
