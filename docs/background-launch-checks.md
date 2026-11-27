# Two checks before a Background launch: K-UI d41a04f40f23

Status: packaged 26 November 2026, 23:56 CST (build `d41a04f40f23`, commit
`d41a04f4`). Host checks only; it has not run on a console. It replaces K-UI
itself (`KUI/boot.kui` and `KUI/runtime.kui`, identical) and nothing else: no
reader module, no updater package, no user data.

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

## Console test

Pending. The README asks for: the Diagnostics line; Tokyo Xtreme Racer under
New (X) with Virtual VMU On (the refusal), with Virtual VMU Off (the
reader-only module: new for this game), and on Standard; Plasma Sword under
New (X) (the refusal, no stop screen) and on Standard; ChuChu Rocket and POD
SpeedZone as controls; a Windows CE game and a Smash Pack game.

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
