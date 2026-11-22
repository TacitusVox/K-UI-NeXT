# WinCE CD music round 1: World Party's group on eleven more kernels (21 Nov 2026)

**Status: test build `1c71ba3afa17` delivered 21 Nov 2026; console result
21 Nov 2026 ([result](#result-21-nov-2026)): Railroad Tycoon II and 4x4
Evolution run with CD music On. Ducati World plays its first race with the
repeat test `d7fd810eb837` ([test](wince-cdda-ducati-repeat-test.md)); in
round 1 its repeat count was refused. The other eight games are not yet
reported.** Roadmap step S4 ([roadmap](wince-roadmap.md)): World Party's CD
music adapter on its other kernels with the usual RAM end. The owner asked to
"allow all WinCE games to run CDDA that have it", after the accepted CD music
set `1ba2fa7e7040` ([record](wince-cdda-chain-test.md)).

## The group

These kernels carry World Party's `nk.exe` build and its `coredll.dll`,
`sndcore.dll`, `dsound.dll` and `wsegacd.dll` (the CD driver), with World
Party's RAM end and OEM globals. Each kernel gets its own module, named by the
kernel:

| Module | Game (product) | CD audio tracks | Owner's image |
|---|---|---|---|
| `ce-cdda-41b0fe340c3a.kui` | Ducati World (US, T-8113N) | 11 | GDI |
| `ce-cdda-28ed98cbb3b9.kui` | Championship Surfer (US, T41403N) | 10 | GDI |
| `ce-cdda-4a6246a5f39b.kui` | The Next Tetris: Online Edition (US, T40214N) | 10 | GDI |
| `ce-cdda-0974f36c0dfe.kui` | Railroad Tycoon II (US, T41902N) | 9 | GDI |
| `ce-cdda-76e49e9f4cb7.kui` | 4x4 Evolution (US, T41903N) | 7 | GDI |
| `ce-cdda-3c7b808ed105.kui` | Starlancer (US, T40209N) | 1 | GDI |
| `ce-cdda-5a02ed6b8ad7.kui` | Hidden and Dangerous (US, T-40502-N) | 1 | GDI |
| `ce-cdda-3b2dd28facb6.kui` | Hidden and Dangerous (PAL, T40503D 50) | 1 | CHD |
| `ce-cdda-3408e1ee7e3f.kui` | Atari Anniversary Edition (US, T15130N) | 1 | GDI |
| `ce-cdda-774e2e170096.kui` | Hoyle Casino (US, T11008N 00) | 1 | GDI |
| `ce-cdda-24b52a5953d2.kui` | Who Wants to Beat Up a Millionaire (US, T11011N) | 1 | GDI |

Track counts are the high-density audio tracks of the startup survey; the
single-track discs may not play theirs. Atari Anniversary carries a different
`DSOUND.DLL` build on its disc (`.text` 0x6bf6 bytes against the ROM's
0x6bd6); if the game loads that copy, the binder refuses it and the game runs
without CD music.

Not in this round: Net de Para (its `sndcore.dll` layout differs), Q\*bert,
Rainbow Six PAL and Tomb Raider Chronicles (other RAM ends, S18c), Happy
Lesson (no CD audio), and the groups with other DirectSound builds.

## What admits a kernel to the group

`tools/wince_cdda_wwp_kernels.py --reference WWP.BIN NAME=KERNEL.BIN ...
--check-header` derives each record from the kernel itself (the kernels are
the owner's supplied images, private inputs) and checks
`include/kui/wince_cdda_wwp_kernels.h`:

- `nk.exe`: World Party's section layout and one data copy record with the
  same source and sizes. Every loaded word is World Party's or moved by exactly
  the data block's shift (the copy destination's difference), except the ROM
  header pointer, which must be the kernel's own. Stored padding past a
  section's loaded size is not compared. Each of the eleven has 1,014 moved
  words and no other.
- The four DLLs: World Party's section layout, and code equal apart from
  addresses that move with a module's base or the data block. Their
  compressed data sections are compared by layout; the module checks what it
  reads of them at run time.
- The ROM header: its address and CRC32, RAM start and free, and World Party's
  RAM end `0x8cfd0000` and OEM globals, so the pool and controller
  placements hold.
- The Win32 record and callback sentinel, unchanged.

It refuses Net de Para (`sndcore.dll` layout), Giant Killers (`sndcore.dll`
code), Rainbow Six PAL and Q\*bert (RAM end), Bust-A-Move 4's kernel
(`nk.exe` build) and a one-byte change in any of the five modules of
Railroad Tycoon II's kernel. Derived values:
[evidence](evidence/wince-cdda-wwp-kernels-2026-11-21.json).

## The modules (source `2f19afa2`)

`KUI_WINCE_CDDA_WWP_KERNEL=NAME` on World Party's stream build
(`Makefile.wwp-stream`) selects a record. The NK globals and interrupt words
move by the shift; the ROM header, its CRC, RAM start and free, the sound
DLLs' ROM table entries, the kernel's size, SHA-256 and load CRCs are the
record's. World Party's own record is the default and holds exactly its
reviewed values.

Each such module carries a 64-byte record at payload `0x300`: `KUICDK01`,
version 1, 64, the sound profile (4, World Party), the kernel's size, the
stream pool's start `0x8cf50000`, zero and the kernel's SHA-256. The shared
entry linker script places it only when present.

`tools/check_armada_ce_excerpt.py --stream --wwp --wwp-kernel NAME` applies
the record to the complete-body pins: the native owner reader's current
process and (negated) process base, and the stage relay's ROM header. The
instructions of every pinned body stay World Party's. It also audits the
record, and refuses one in any other stream module.

Against World Party's module, a group module differs in its kernel
constants, its record and its build ID, and in `_ce_load`'s code (the stage's
kernel loader, which builds a different kernel size with different
instructions). Hoyle Casino's controller is 32 bytes larger (41,664 bytes);
every other controller is World Party's 41,632. Every stack worst case is
World Party's: 516 of 2,000 bytes.

## The GUI route (source `1c71ba3a`)

With CD music On, a WinCE GDI whose kernel is none of the four reviewed
kernels looks for `KUI/apps/games/ce-cdda-<first 12 hex digits of its
SHA-256>.kui`. The GUI admits it only by its record (version, size, a known
sound profile, the kernel's exact size, that profile's pool, zero and the
complete SHA-256), then fills the stream seed as for the reviewed kernels,
with the map checked against the kernel's own size. The stage admits the
kernel again by its SHA-256 before Windows CE starts.

Without the module the launch refuses as before ("unrecognized WinCE
runtime"), and the log names the missing file. A reviewed kernel keeps its own
module. The report's CD music profile reads, for example, "Worms World Party
sound, kernel 0974f36c0dfe". Later groups need no GUI change: their modules
carry the same record with their own profile.

## Test build `1c71ba3afa17`

| Module | Game | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-41b0fe340c3a.kui` | Ducati World (US) | 136,036 | `702BE589` | `e639a9ac0e987d33...` |
| `ce-cdda-28ed98cbb3b9.kui` | Championship Surfer (US) | 136,036 | `E3ECE95B` | `e02bc6cfe1921881...` |
| `ce-cdda-4a6246a5f39b.kui` | The Next Tetris: Online Edition (US) | 136,036 | `72CA764E` | `852f042df9cc1fb4...` |
| `ce-cdda-0974f36c0dfe.kui` | Railroad Tycoon II (US) | 136,036 | `FBA74081` | `5bf8fad80abcccbb...` |
| `ce-cdda-76e49e9f4cb7.kui` | 4x4 Evolution (US) | 136,036 | `3F954344` | `6e5579fd169e6803...` |
| `ce-cdda-3c7b808ed105.kui` | Starlancer (US) | 136,036 | `2416DB88` | `b988333be643cdf1...` |
| `ce-cdda-5a02ed6b8ad7.kui` | Hidden and Dangerous (US) | 136,036 | `D5857DC0` | `3345e1b4406b6192...` |
| `ce-cdda-3b2dd28facb6.kui` | Hidden and Dangerous (PAL) | 136,036 | `381D423B` | `0b6510a00876a284...` |
| `ce-cdda-3408e1ee7e3f.kui` | Atari Anniversary Edition (US) | 136,036 | `548A2594` | `522a73b6c2d73726...` |
| `ce-cdda-774e2e170096.kui` | Hoyle Casino (US) | 136,068 | `EF354FFE` | `757906a8baccd162...` |
| `ce-cdda-24b52a5953d2.kui` | Who Wants to Beat Up a Millionaire (US) | 136,036 | `D9ED879E` | `5988a576f413b881...` |

The modules use the accepted recipe (the adaptive window, emergency-only
programmed receptions and the chain restart). The GUI: `KUI/boot.kui` and
`KUI/runtime.kui`, identical, 3,816,892 bytes, payload CRC `2E38CCC2`, SHA-256
`902270842a277221...`. It is Main `4441e92c` with only this route added,
built as `069d612cadea` was: launch timing, the Smash Pack bundle
`cc81e63ca4d2`, and the Standard binding of the owner's `retail-boot.kui`
`9e3295bcfaec`. One engine-3 marker, no engine-2 marker.

Delivered: `KUI-WinCE-CD-Music-World-Party-Group-21NOV2026-1550-CST-1c71ba3.zip`
(10,640,266 bytes, SHA256
`03305dfdf0ffaea49d032661676028465315d97e92d22973ae39e99862542814`), with the
reference GUI `00d8ed67d9ad` (payload CRC `4C5B0D7D`) in `IF-NEEDED/KUI/`.
Full hashes are in its `INVENTORY.json`. Retained on the card: the five
accepted CD music modules, `ce-boot.kui`, `ce-probe.kui`, the native readers,
banks, settings, cards and user data.

## Gates

- **Records:** the derivation's `--check-header` passes on the owner's twelve
  kernels; a changed header value fails it.
- **Accepted bytes:** from the committed tree the five accepted CD music
  modules rebuild byte-identical to `1ba2fa7e7040`. The 19 retail entry objects
  without the record (native Standard, `ce-boot`, VMU, CD music and timing
  builds) link to identical ELF files with the old and new linker script.
- **Each module:** the stream excerpt checker with its record, the layout
  checker and the call checks pass.
- **GUI admission of the real bytes:** `test-wince-packaged-prepare --kernel
  SHA BYTES KERNEL MODULE` runs the production preparation with each of the
  eleven modules and its real kernel (the GUI computes the SHA-256; no
  substitution), raw and 2048-byte DATA, and refuses a kernel with one byte
  changed without reading a module. The existing modes pass with the accepted
  World Party, Worms and Armada modules and kernels.
- **Host:** `test-wince-cdda-wwp-group` (runtime admission, generic sound
  binder and native lease with each of the twelve records); the launch fixture
  (the route, eleven record and installation faults and a missing seed, the
  reviewed kernels' precedence, and the size-based map check agreeing with
  every reviewed profile); `tests/test_games_retail.py`; the seven production
  cases with the accepted `ce-boot.kui` on FAT32 and exFAT.
- **Not run:** the games' own executables (no host emulation of these
  titles). The console test is the first run of these modules.

## Result (21 Nov 2026)

The owner's report, CD music On, GUI `1c71ba3afa17`. The other games and the
four GUI checks were not reported.

- **Railroad Tycoon II (US): plays.**
- **4x4 Evolution (US):** the owner heard music, with minor, infrequent lag
  during the full-motion videos. The report's first page, photographed during
  play, counted no PLAY. It showed only the two STOPs counted at start-up:
  `wsegacd`'s INIT and REQUEST(24), which the audio observer counts as STOP.

  From the game's files (static analysis):
  - **Music.** The menus play the game's own WAV files through DirectSound.
    CD tracks 4 to 10 play from a race's start, the attract demo, replays
    and restarts (IOCTL `0x25020` {t, t, 15}). The music heard was not CD
    music, and whether a race plays its track is still open.
  - **Video.** The intro video (`DCMOVIE.EXE` through DirectShow,
    `\CD-ROM\4x4.mpg` on the data track) plays before any CD audio
    request, so its lag is a question for the reader, to compare with
    CD music Off.
  - **Processes.** `DCMOVIE.EXE` starts `DC.EXE` and exits before the first
    PLAY, so the player binds `DC.EXE`, which lives until reset.
- **Ducati World (US): silent.** Its only CD audio request asks for 1,000
  repeats, which the stream model refused. Each race also runs in a new
  process ([test of the repeat change](wince-cdda-ducati-repeat-test.md)).
  The owner was unsure which US image they had chosen. With CD music On,
  though, the GUI refuses a kernel that has no module, so the image that ran
  used this module, and its request is the same.
- **With CD music Off, 4x4 Evolution crashed after its intro was skipped.**
  The report page reads GAME RETURN, build `1a5f46776c0c`. That build is the
  read-ahead test, which was not adopted
  ([test 2](wince-pool-readahead-test.md)); the owner's card still held it
  as `ce-boot.kui`. The timing probe of the same build never shows this page.
  - **Counters:** GUARD 0; 2,255 sectors in 6,136 steps; CRC ERRS 29 against
    REPAIRED 30; OVERRUNS 20; TOKENERR and FOREIGN 0.
  - **Analysis so far:** a host model of the skip cut a sequential stream at
    random points and then read elsewhere, with injected CRC errors and
    overruns. It found no defect that the skip reaches in the shared reader
    of 1a5f4677, the everyday candidate `91b0540103f0`'s, with or without
    read-ahead.
  - **The CD driver on a skip.** 4x4's `wsegacd.dll` sends no abort or reset
    when the video is skipped. Its only ABORT follows a 15-second stall.
  - **Next:** the retest on the candidate is part of the
    [repeat test](wince-cdda-ducati-repeat-test.md).
- **Retest (21 Nov 2026, the candidate `91b0540103f0` as `ce-boot.kui`):**
  - 4x4 Evolution runs with CD music Off, intro skipped, and with CD music
    On. So the crash belonged to the read-ahead build.
  - The video lag stays, still very infrequent, with CD music Off too. It is
    the reader's, not CD music's.

## Questions for the console test

- Railroad Tycoon II and Ducati World, then Championship Surfer, 4x4 Evolution
  or The Next Tetris Online, with CD music On: does the music play and follow
  the game's song changes; any lag, repeats, skips or cuts against CD music
  Off?
- The report's first page (A+B+X+Y+Start) in one of them.
- The GUI: World Party with CD music On, Armada with CD music On (Virtual VMU
  Off and On), a WinCE game with CD music Off, one native game with Virtual
  VMU On (the four checks of `069d612cadea`).

## Not answered by this test

- Which CD audio commands and status polling each game uses; the data session
  covered none of these games with CD music On.
- Whether the single-track discs play their track at all.
- Which `DSOUND.DLL` Atari Anniversary loads.
