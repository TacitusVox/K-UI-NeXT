# WinCE CD music round 2: Bust-A-Move 4's group on seven more kernels (21 Nov 2026)

**Status: test build `9e89e778370f` delivered 21 Nov 2026; console test
pending.** Roadmap step S3 ([roadmap](wince-roadmap.md)): Bust-A-Move 4's CD
music adapter on its other kernels, in round 1's pattern
([World Party's group](wince-cdda-wwp-group-test.md)).

## The group

These kernels carry Bust-A-Move 4's `nk.exe` build and its `coredll.dll`,
`sndcore.dll`, `dsound.dll` and `wsegacd.dll` (the CD driver), with Bust-A-Move
4's RAM end and OEM globals. Each kernel gets its own module, named by the
kernel:

| Module | Game (product) | CD audio tracks | Owner's image |
|---|---|---|---|
| `ce-cdda-9d0ea356bd66.kui` | Puzzle Bobble 4 (JPN, T42301M) | 22 | GDI |
| `ce-cdda-6c09543a5680.kui` | Virtua Cop 2 (JPN, HDR-0061) | 35 | GDI |
| `ce-cdda-25e68cedcd08.kui` | Caesars Palace 2000 (US, T-12504N) | 16 | GDI |
| `ce-cdda-ae7b13b52064.kui` | Caesars Palace 2000 (PAL, 12502D-50) | 16 | CHD |
| `ce-cdda-04f9b8cd872e.kui` | Get!! Colonies (JPN, HDR-0071) | 4 | GDI |
| `ce-cdda-877c91cd3930.kui` | Marionette Company 2 (JPN, T5203M) | 1 | GDI |
| `ce-cdda-8cbfba9fc5e4.kui` | Hello Kitty: Magical Block (JPN, HDR-0070) | 1 | GDI |

Bust-A-Move 4 PAL carries Bust-A-Move 4's own kernel and keeps
`ce-cdda-bam4.kui`. Puzzle Bobble 4's kernel differs from Bust-A-Move 4's in
7 bytes, all in its registry file (`default.fdf`) and that file's ROM table
entry, so every address is Bust-A-Move 4's.

The games' own use, from the startup files available (Bust-A-Move 4, Caesars
Palace US, Super Runabout US; static analysis):

- **Caesars Palace 2000 (US)** imports DirectSound in `GAME.EXE`. `CP2K.EXE`
  starts the intro movie, waits for it, then starts the game, so one sound
  process is alive at its first PLAY. Its music is MIDI. Its CD tracks are
  short jingles at a new player rank (PLAY {t, t, 0}), so it is a weak test.
  Its disc carries `DSOUND.DLL` (Bust-A-Move 4's build, which the binder
  admits by layout and identity) but no `SNDCORE.DLL`: the binder would pair
  the disc's DirectSound with the ROM's sndcore, which no console run has
  exercised.
- **Puzzle Bobble 4, Virtua Cop 2, Get!! Colonies, Marionette Company 2 and
  Hello Kitty: Magical Block:** no startup files, so the applications are
  unchecked. The survey records static DirectSound imports, and Virtua Cop
  2's through `MGAMESOUND.DLL` in the same process.

Not in this round:

- **Super Runabout** (US and San Francisco Edition, PAL, JPN, and the JPN
  trial). Its kernels lack the ROM `sndcore.dll` and `dsound.dll`, except the
  trial's. `SUPERRA.EXE` imports no DirectSound. It plays its sound through
  its own AICA driver, `MANATEE.DRV`, like Resident Evil 2, so CD music there
  needs another kind of module. The survey's "Super Runabout x6 (5 load
  DirectSound from disc)" is refuted for the US disc; the others run the same
  `SUPERRA.EXE`.
- **Tomb Raider IV / The Last Revelation** (other RAM end, no CD tracks).
- **Armada's group** (another `dsound.dll` build: S11).

## What admits a kernel to the group

`tools/wince_cdda_bam4_kernels.py --reference BAM4.BIN NAME=KERNEL.BIN ...
--check-header` checks `include/kui/wince_cdda_bam4_kernels.h`. It uses round
1's method (`tools/wince_cdda_wwp_kernels.py`), with Bust-A-Move 4's US kernel
as the reference:

- `nk.exe`: one data copy record, and every loaded word equal or moved by
  the data block's shift, except the ROM header pointer. Puzzle Bobble 4 has
  shift 0. Each of the other six has 1,018 moved words and no other change.
- The four DLLs have equal code apart from relocation. coredll's
  debug-directory TimeDateStamp may differ, because it is link metadata the
  module never reads. The tool locates it from the module's own debug
  directory and reports it: Caesars Palace (US and PAL) and Get!! Colonies
  carry `0x37f40dfe`.
- RAM end `0x8cfd0000` and the OEM globals are Bust-A-Move 4's, so the pool
  and the controller keep their places.
- The Win32 record and the callback sentinel are unchanged.

It refuses:
- Super Runabout's kernels: no ROM `sndcore.dll`;
- Tomb Raider IV and Daisessen: RAM end;
- Armada's kernel: its `dsound.dll` build;
- World Party's group and Worms Armageddon: their `nk.exe` build.

The method applied to Armada's kernel at its shift reproduces every reviewed
Armada constant. Derived values:
[evidence](evidence/wince-cdda-bam4-kernels-2026-11-22.json).

## The modules (source `9e89e778`)

`KUI_WINCE_CDDA_BAM4_KERNEL=NAME` on Bust-A-Move 4's stream build
(`Makefile.bam4-stream`) selects a record through
`include/kui/wince_cdda_bam4_kernel.h`, as World Party's switch does:

- **Moved by the shift:** the NK globals and interrupt words.
- **The kernel's own:** the ROM header, its CRC, RAM start and free, the
  sound DLLs' ROM table entries, the kernel's size, its SHA-256 and the load
  CRCs.
- **The KUICDK01 record** carries sound profile 2 (Bust-A-Move 4) and pool
  `0x8cf50000`. GUI `1c71ba3afa17` already admits it, so the GUI does not
  change.
- **The checker.** `tools/check_armada_ce_excerpt.py --stream --bam4
  --bam4-kernel NAME` applies the record to the owner and boot pins and
  audits the record.

Against `ce-cdda-bam4.kui`:
- **Puzzle Bobble 4's module** has an identical controller and differs only
  in the body CRC, the SHA-256, the record and the build ID.
- **The six shifted modules** also differ in the controller's kernel
  constants and in the stage's kernel loader, as in round 1.

Every controller is 40,992 bytes, and every stack worst case is 512 of 2,000
bytes, as for Bust-A-Move 4's module.

## Test build `9e89e778370f`

| Module | Game | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-9d0ea356bd66.kui` | Puzzle Bobble 4 (JPN) | 135,396 | `EB9CC023` | `02eb9a0db4e539f4...` |
| `ce-cdda-6c09543a5680.kui` | Virtua Cop 2 (JPN) | 135,396 | `4236DAEC` | `06ff4312034bdca1...` |
| `ce-cdda-25e68cedcd08.kui` | Caesars Palace 2000 (US) | 135,396 | `80EEC206` | `68011ca1fe73385d...` |
| `ce-cdda-ae7b13b52064.kui` | Caesars Palace 2000 (PAL) | 135,396 | `9BA9C4DD` | `c239f98263583337...` |
| `ce-cdda-04f9b8cd872e.kui` | Get!! Colonies (JPN) | 135,396 | `F44C11B1` | `54475949844cc920...` |
| `ce-cdda-877c91cd3930.kui` | Marionette Company 2 (JPN) | 135,396 | `ED494C26` | `5d0e4470c35bada9...` |
| `ce-cdda-8cbfba9fc5e4.kui` | Hello Kitty: Magical Block (JPN) | 135,396 | `61B17703` | `bea93e8c0400ff32...` |

The modules use the accepted CD music recipe (the adaptive window,
emergency-only programmed receptions and the chain restart). The repeat
clamp stays Ducati World's only.

Delivered: `KUI-WinCE-CD-Music-BAM4-Group-21NOV2026-2037-CST-9e89e77.zip`
(579,284 bytes, SHA256
`fec9388bb1ceb259e6b8e87592c768bce8c51780c55d66c1143ffa56bc3e88e9`). Full
hashes are in its `INVENTORY.json`. It holds the seven modules only. The
GUI stays `1c71ba3afa17`, and every other file on the card stays.

## Gates

- **Records:** `--check-header` passes on the owner's kernels for the eight
  records. A changed header value fails it.
- **Accepted bytes:** from the committed tree, these rebuild byte-identical:
  - the five accepted CD music modules (`1ba2fa7e7040`);
  - the ten other round-1 modules (`1c71ba3afa17`);
  - Ducati World's test module (`d7fd810eb837`);
  - the GUI `1c71ba3afa17`, which compiles the profile table that now takes
    Bust-A-Move 4's record.
- **Each module:** the stream checker with its record, the layout checker
  and the call checks pass.
- **GUI admission of the real bytes:** `test-wince-packaged-prepare
  --bam4-kernel SHA BYTES KERNEL MODULE` runs the production preparation with
  each of the seven modules and its real kernel, with raw and 2048-byte DATA.
  It refuses a kernel with one byte changed. World Party's mode still passes
  with round 1's modules.
- **Host:**
  - `test-wince-cdda-bam4-group`: runtime admission, generic sound binder and
    native lease with each of the eight records;
  - the launch fixture: a profile-2 record with its eleven faults and a
    missing seed refused, and Bust-A-Move 4's reviewed kernel keeping its
    module;
  - `test-wince-cdda-wwp-group`, `test-wince-runtime-families`,
    `test-armada-ce-stream` and the checker's self-test.
- **Not run:** the games' own executables; this is no host emulation.

## Questions for the console test

- With CD music On:
  - **Puzzle Bobble 4:** title, menus and a stage.
  - **Virtua Cop 2:** a stage or two.
  - **Optionally:** Get!! Colonies, or Caesars Palace 2000 US up to a rank-up
    jingle.

  For each: does the music play and follow the game, and is there any lag
  against CD music Off?
- The report's first page in Puzzle Bobble 4 or Virtua Cop 2.

## Not answered by this test

- The single-track discs: whether they play their track at all.
- Caesars Palace: whether the disc's DirectSound with the ROM's sndcore binds
  on the console.
- The applications without startup files, which were not checked.
