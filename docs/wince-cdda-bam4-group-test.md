# WinCE CD music round 2: Bust-A-Move 4's group on seven more kernels (21 Nov 2026)

**Status: test build `9e89e778370f` delivered 21 Nov 2026. Console result,
21 Nov 2026: Puzzle Bobble 4 works; Virtua Cop 2 stopped in its bootstrap 2.
Virtua Cop 2's fix, test build `1ac7e667c0d3`, delivered 21 Nov 2026; console
test pending.** Roadmap step S3 ([roadmap](wince-roadmap.md)): Bust-A-Move
4's CD music adapter on its other kernels, in round 1's pattern
([World Party's group](wince-cdda-wwp-group-test.md)).

## Result (21 Nov 2026)

The owner tested two games with CD music On, using module build
`9e89e778370f` and GUI `1c71ba3afa17`:
- **Puzzle Bobble 4 (JPN):** "worked fine". Its report page shows build
  `9E89E778370F`, 19 PLAYs and none refused.
- **Virtua Cop 2 (JPN):** stopped before Windows CE started, in bootstrap 2,
  with BOOTSTRAP ALTERED STREAM SOURCE MAP, DETAIL 0. The cause is the disc's
  own bootstrap
  ([below](#virtua-cop-2s-bootstrap-2-uses-the-pools-top-as-scratch)).
- **Not tested:** Caesars Palace 2000 (US and PAL), Get!! Colonies,
  Marionette Company 2 and Hello Kitty: Magical Block.

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

## Virtua Cop 2's bootstrap 2 uses the pool's top as scratch

**Cause.** K-UI runs the disc's bootstrap 2 (IP.BIN+0x6000) before the game
entry. Virtua Cop 2's runs Sega's play-history recorder (`AIPf` 1.07 over
the Katana flash manager `fmMng` 1.04), which records the game in the
console's flash at every boot. Apart from the recorder and its stack, its
IP.BIN matches Bust-A-Move 4's standard one, except for the region text and
the final FPSCR value. The recorder:
- sets its stack to `0x8cfc0000`, the stream map's end, where the standard
  bootstrap uses `0x8c00f400`;
- writes 60 bytes of its work data at `0x8cfc0000`, the start of the work
  page;
- uses buffers at `0x8cfd0000`, `0x8cfe0000` and `0x8cff0000`, in CE's
  driver globals above the pool, which the kernel sets up itself.

Measured with a scratch SH-4 integer interpreter (not committed) from K-UI's
entry state, over several flash states (blank, formatted, repeat boots and a
full partition), its stack reaches:
- `0x8cfbfd44` on a normal boot (map offsets `0xd44` to `0xfff`);
- `0x8cfbfd08` when it compacts a full partition (from `0xd08`).

The BIOS flash routines' own frames below it were not modelled. Nothing it
writes touches the seed words, the audio source bank, the controller, the
stage or the resident. On the console, the relay's checks that run before
the map's passed: boot stack and CPU state, executable, resident,
controller code and its zero tail, and the ROM header. The 4 KiB map lies
under its stack, which explains the stop exactly.
[Evidence](evidence/wince-cdda-vc2-bootstrap-2026-11-21.json) (derived
addresses only).

**The fix (source `e8b3f744`, reviewed and completed in `1ac7e667`).**
After bootstrap 2 has finished, and before the relay checks the pool, the
stage reinstalls the map from its own copy and zeroes the work and guard
pages, as it left them before the bootstrap
(`include/kui/armada_ce_boot2_scratch.h`). Conditions:
- only for this IP.BIN: CRC32 `0x3f7f1c3d`, which the stage has already
  checked against the loaded IP.BIN;
- only after the stage copy passes the proof the stage ran before the boot:
  the seed header, the map's CRC word, the decoded map, the runtime policy
  and a match against the stage's own map.

If that proof fails, the launch stops on its own screen, BOOTSTRAP 2 SCRATCH
COPY REFUSED, with the reason as DETAIL (3 to 8). The relay then checks the
pool exactly as for every other disc: ROM header, seed words, map, source
bank, controller code and its zero tail, executable and resident. For this
disc only, a write inside the map, work or guard pages is not reported.
Nothing but its bootstrap runs before the relay, and those pages are
restored whole.

`KUI_CE_CDDA_BOOT2_SCRATCH` is on by default only in Virtua Cop 2's module,
and a build error anywhere else. With it off, that module rebuilds as
`9e89e778370f`.

**Review.** Two independent reviews of `e8b3f744` (mechanics and semantics)
said ship, and ship after fixes. `1ac7e667` adopts:
- the separate stop and its reason;
- the build guard;
- a linked pin of the resume path;
- compile-time checks of the repaired range;
- the host test's full header list.

Not adopted: bounding the repair to the observed footprint (refusing it
unless the map below a headroom bound and the work page beyond its first 64
bytes are intact). Only the bootstrap runs in that window, and the BIOS
frames were not modelled. A bound could therefore only refuse a launch that
the reinstall makes correct, and the relay's other checks still catch a
write outside the three pages. The repair decodes the map on the stage's
64 KiB stack (3,336 bytes, by the compiler's stack report).

## Test build `1ac7e667c0d3` (Virtua Cop 2 only)

| Module | Game | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-6c09543a5680.kui` | Virtua Cop 2 (JPN) | 135,736 | `1A985F40` | `0f714fed2832ab3a...` |

Gates, from the committed tree:
- **Accepted bytes.** These rebuild byte-identical:
  - the five accepted CD music modules (`1ba2fa7e7040`);
  - the ten other round-1 modules (`1c71ba3afa17`);
  - Ducati World's (`d7fd810eb837`);
  - the six other round-2 modules (`9e89e778370f`).

  Virtua Cop 2's module with the switch off is byte-identical to
  `9e89e778370f`. The GUI does not change. Source `83becb53`, which
  corrects two comments only, rebuilds the delivered module byte-identical
  (build ID `1ac7e667c0d3`).
- **Linked pin.** Each of those builds ran the stream checker with its new
  resume-path pin. Every other module's resume path calls the relay alone
  and carries no repair. Virtua Cop 2's calls the repair and then the relay,
  between the cache switch and the relay's arguments, with no other call,
  branch or return.
  - The checker's self-test adds 13 cases.
  - A scratch test refuses seven mutations of the real linked stage:
    literals swapped, the call removed, its delay slot changed, its literal
    retargeted, and the record flipped both ways.
- **Disassembly.** The repair runs after the cache purge and switch. On a
  refusal it restores the display and stops with BOOTSTRAP 2 SCRATCH COPY
  REFUSED, the result as DETAIL. The relay's pinned body is unchanged.
- **Inputs.** The stage's own CRC routine (`src/loader/stage_fast_crc32.h`,
  built for the host) gives `0x3f7f1c3d` on the owner's IP.BIN.
  `test-wince-packaged-prepare --bam4-kernel` runs the production
  preparation with the real kernel and this module, with raw and 2048-byte
  DATA.
- **Host test.** `test-armada-ce-boot2-scratch` (in
  `test-wince-cdda-bam4-group`) passes 219 checks:
  - the map and work pages reinstalled, and idempotent;
  - another IP.BIN unchanged;
  - each refusal reason, with nothing changed: a seed word, the CRC word,
    wire bytes, the runtime policy, a mismatched stage map and missing
    buffers.
- **Not run:** the bootstrap itself with this module on the console; the
  BIOS frames.
- **Unrelated failure.** `tests/test_wince_release_package.py`'s dry-run
  `make all` fails the same way on the committed tree: it needs a KOS
  network source in a temporary tree.

Delivered: `KUI-WinCE-CD-Music-Virtua-Cop-2-Fix-21NOV2026-2154-CST-1ac7e66.zip`
(175,837 bytes, SHA256
`3757666973d1add6fafbe004eb18dc4b26230740e702d83599cf8703348cd94d`). It holds
the module, plus round 2's module under `IF-NEEDED/` to restore. The GUI
stays `1c71ba3afa17`, and every other file on the card stays.

Questions for the console test, with CD music On:
- Does Virtua Cop 2 reach its title?
- Does its music play and follow the game into a stage?
- Do the controls work?
- Does a second cold boot behave the same? (The recorder updates the flash
  at every boot.)
- If it stops: the message and the BUILD line (`1AC7E667C0D3`).
  - BOOTSTRAP 2 SCRATCH COPY REFUSED: the stage copy failed its proof.
  - BOOTSTRAP ALTERED STREAM SOURCE MAP: the repair did not apply.
  - Any other message: the bootstrap changed something else.

## Not answered by this test

- The single-track discs: whether they play their track at all.
- Caesars Palace: whether the disc's DirectSound with the ROM's sndcore binds
  on the console.
- The applications without startup files, which were not checked.
- Virtua Cop 2 with another IP.BIN (another pressing or region): it keeps
  round 2's stop until its bootstrap is examined.
- Other discs whose bootstrap uses the top of RAM. Only Virtua Cop 2's was
  compared with Bust-A-Move 4's standard one; there is no survey of every
  WinCE disc's bootstrap 2.
- Pools at the top of RAM on this kernel. A unified pool of 64 KiB or more
  below RAM end `0x8cfd0000` (F8), and the combined CD music and VMU spill at
  `0x8cfc0000`, would contain the recorder's stack or block. That needs a
  policy per disc, which is unresolved ([roadmap](wince-roadmap.md), R4).
