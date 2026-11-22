# One K-UI for both threads: K-UI 64ae07435de5

Status: packaged 21 November 2026, 23:08 CST (build `64ae07435de5`, commit `64ae0743`).
Host checks only; it has not run on a console. It replaces K-UI itself
(`KUI/boot.kui` and `KUI/runtime.kui`, identical) and nothing else: no
reader module, no updater package, no user data.

It answers the owner's choice of 22 November 2026, "Merge both lines": one
K-UI with the owner's Test 83 GUI line merged into the Windows CE line's
current K-UI, delivered as a manual-copy ZIP, then the refused games run
again. And the request that followed it: "make it so I won't screw up any
tests in the future because one thread made a ton more progress than the
other" (`docs/gui-lines.md`: the line ledger, `tools/check_gui_lines.py`
and the AGENTS.md rule, `67cdd635`).

## Why

The owner ran Test 84's module under the Windows CE line's K-UI
`1c71ba3afa17`, which contains Main `4441e92c` but not the owner's Test 83
GUI line `4d4dcff77494`. Its 64-slot Background map refused Bangai-O,
MDK2 and Plasma Sword ("Background map exceeds 64 slots";
`cdda-background-test-84.md`, Console result).

## Source

`64ae0743` merges:

- this line: Main `4441e92c` with the owner's Test 83 reader line
  `1d80f771f081` and GUI line `4d4dcff77494`, and Tests 83 and 84 behind
  their Toy switches;
- the Windows CE line `claude/great-darwin-yjmgfi` to `b7c92e0c`:
  `1c71ba3afa17`'s source (Main `4441e92c` with the CE CD music routes), then
  CD music round 2 (Bust-A-Move 4's group, `9e89e778`), Virtua Cop 2's
  bootstrap-2 repair (`e8b3f744`, `1ac7e667`, `83becb53`) and the fixture
  record `b7c92e0c`.

It took three merges: `2e03b96e` (the CE line to `b616e2db`), `86214640` (to
`83becb53`) and `64ae0743` (to `b7c92e0c`). Before the build,
`python3 tools/check_gui_lines.py` at `86214640` failed, naming `b7c92e0c`
(a fixture commit the CE line had pushed in the meantime): the check doing
its job. At `64ae0743` it passes: Main `4441e92c65bc`, this line and the CE
line `b7c92e0c59c2` contained.

What the GUI line brings that `1c71ba3afa17` lacks (22 commits,
`1c71ba3a..4d4dcff7`): the 160-slot native map inside the compact reader's
lease (`13dc13c1`, `3e6f9342`); the standalone New reader's 128-slot map
and the route of overlapping SDK pools through it with the virtual VMU off
(`bba88f90`, `7c6f19b6`, `2616778e`, `bcbd183b`); verified native mappings
reused from the cache (`e0a8cc6d`, `1c6b498b`, `4d4dcff7`); and more SDK
layouts recognised before a launch (`7a2ec64f`, `2cc57af0`, `234f6788`,
`b38626b4`, `0b908530`, `d0162522`).

## The recipe, proven on `1c71ba3afa17`

Before building the merged K-UI, the owner's running K-UI was rebuilt from
its source until the result was the owner's file byte for byte
(`runtime.kui` from the owner, 3,816,892 bytes, SHA-256 `90227084...91ff7`,
payload CRC `2E38CCC2`; its embedded Smash bundle `cc81e63ca4d2` was taken
from that file). The tries, each from a clean worktree at `1c71ba3a`:

1. Launch timing, the Smash bundle and the Standard binding
   `9e3295bcfaec`, nightly off: same size and layout, but
   `update_work.constprop.0` 8 bytes longer, which moves the code after it.
2. With `KUI_GITHUB_NIGHTLY=1`: further away (`update_work` 4 bytes longer
   than the owner's, its code mostly different). The owner's K-UI is not a
   nightly build.
3. With `KUI_UPDATE_SOURCE_VERIFIED=0` (set here with
   `KUI_CLEAN_HEAD=dirty`; the CE line's build evidently came from a
   checkout that was dirty or whose HEAD was not its `BUILD_ID`): 20 bytes
   differ, the header CRC words and the date in
   KallistiOS's banner (`Tue Oct 27 15:07:25 UTC 2026` in the owner's,
   the day of the local library build in the rebuild).
4. With KallistiOS's kernel library rebuilt here with that banner date
   (`kernel/make_banner.sh` takes `date`; the rebuild used a `date` that
   prints the owner's): byte-identical, `90227084...91ff7`. The same again
   with the bundle outside the worktree and a clean compile receipt, from
   both packagers (`tools/nightly.py build_gui` and
   `tools/runtime_package.py build`).

So `1c71ba3afa17` is: source `1c71ba3a`, `BUILD_ID=1c71ba3afa17`,
`KUI_SMASH_LAUNCH_TIMING=1`, `RUNTIME_MODULE_BUNDLE` with the four Smash
modules (bundle build `cc81e63ca4d2`),
`NATIVE_STANDARD_MANIFEST=resources/updater-bindings/installed-standard-9e3295bcfaec.h`,
`KUI_GITHUB_NIGHTLY=0`, `KUI_UPDATE_REVIEW_FEED=0`,
`KUI_UPDATE_SOURCE_VERIFIED=0`, KallistiOS `fcfa7d8` built on 27 October
2026, GCC 15.2.0.

The merged K-UI is built the same way, with the same KallistiOS library
(banner included), from a clean worktree at `64ae0743` with
`BUILD_ID=64ae07435de5`:

    make -f Makefile.dc build/kui-runtime.elf BUILD_ID=64ae07435de5 KUI_SMASH_LAUNCH_TIMING=1 \
      RUNTIME_MODULE_BUNDLE=<bundle of the four Smash modules> \
      NATIVE_STANDARD_MANIFEST=resources/updater-bindings/installed-standard-9e3295bcfaec.h
    python3 tools/runtime_package.py build build/kui-runtime.elf runtime.kui --build-id 64ae07435de5

One flag differs, because this checkout is clean and its HEAD is the
`BUILD_ID`: `KUI_UPDATE_SOURCE_VERIFIED=1`. It gives the online updater's
nightly list the running build to compare with each nightly's ancestors
(`parse_news`, `src/core/update_nightly.c`: the "newer" note). `64ae0743`
is in no Main nightly's ancestry, so the list shows what it shows with 0.
Nothing else reads it.

## Checks

1. **The packaged file** (`evidence/one-kui-64ae074/delivery-check.txt`):
   `KUIRUN1` format 1, 3,834,412 bytes, build `64ae07435de5`, payload CRC `1431624F`. The
   producer's own GUI member check (`tools/update_package.py`
   `member_info_v2`) passes: one `KUIENG3!` marker and no `KUIENG2!`, one bank
   capability, an empty handoff, not a withdrawn build. Its embedded bundle is
   the owner's byte for byte (413,045 bytes, build `cc81e63ca4d2`, the four
   Smash Pack modules), and its KallistiOS banner is the owner's K-UI's. Both
   packagers (`tools/runtime_package.py build`, `tools/nightly.py build_gui`)
   give the same file; the compile receipt says `source_dirty: false`.
2. **Every delivered module of both lines rebuilds byte-identical from
   `64ae0743`** (`evidence/one-kui-64ae074/modules-*-64ae0743*.txt`):
   - the regular games' line: Test 84's module `597d8eac...` and its second
     module `e74cca71...` (`4abc63b0cc58`), the owner's Test 83 reader
     `313ac3bd...5f1c` (`1d80f771f081`, every Test 84 switch off) and the
     installed New reader's recipe `467be62f...` (`f8807cb16561`); Test 84's
     linked and parts checks and the switches' production-text check pass;
     the assembly audit's one failure is the known one recorded with Test 84
     (`bba_sockets.c`, `shell_font.c`);
   - the Windows CE line, 26 modules: the accepted CD music set
     (`1ba2fa7e7040`, five), World Party's group (`1c71ba3afa17`, eleven),
     Ducati World's repeat test (`d7fd810eb837`), Bust-A-Move 4's group
     (`9e89e778370f`, seven, Virtua Cop 2's with its repair off), the generic
     candidate `ce-boot.kui` (`91b0540103f0`), each identical to its build at
     the first merge `2e03b96e` (which matched the CE line and its records),
     and Virtua Cop 2's test build `1ac7e667c0d3` (135,736 bytes, `1A985F40`),
     identical to the CE line's own build at `b7c92e0c`.
3. **The K-UI's compile inputs did not change after the verification merge.**
   Of the 11 files outside `docs/` that changed between `2e03b96e` and
   `64ae0743`, none is among the K-UI build's 632 repository compile inputs
   (its dependency files; `gui-inputs.txt`): the later commits touch the CE
   readers, tests and tools only. The verification of `2e03b96e` below
   therefore covers this K-UI's source.
4. **Verification of the merge** (four independent reviewers, at `2e03b96e`;
   `evidence/one-kui-64ae074/verification-agents.json`):
   - both lines' modules and gates (as in 2), with each line's host suites;
   - the merge itself: every auto-merged file equals a clean three-way merge;
     the two hand resolutions (`src/loader/retail_entry.S`, the fixture) keep
     both lines' records, each under its own `#if`; the fixed record offsets
     cannot overlap; no recipe defines a macro of both lines. In
     `games_retail.c` only `prepare_launch` was changed by both lines: its
     Windows CE routes decide as on the CE line, its native routes as on the
     owner's GUI line (`git diff 4d4dcff7 dc938fea -- src/apps` is empty);
   - the GUI's admission code on the actual packaged Test 84 modules (the
     main and the second): map capacity 160, a New and Legacy Background
     module, as with `4d4dcff77494` (`admission-test84.txt`);
   - the GUI's production preparation with real module bytes for the Windows
     CE kernel route (Puzzle Bobble 4, Ducati World; synthetic kernel
     fingerprints, the owner's kernels being private), the routing of 426
     original executables (every decision as with `4d4dcff77494`; the route
     checker builds byte-identical from both), `tests/test_games_retail.py` (408 PASS lines,
     including the 99-track and fragmented-map cases on FAT32 and exFAT) and
     the CE and native host suites;
   - `prepare_launch`'s stack frame: 5,652 bytes merged (5,484 on the GUI
     line, 5,616 on the CE line), on the 64 KiB I/O worker stack.
5. **Two host failures that are not the merge's**, both also on Main
   `4441e92c`, on `1c71ba3afa17`'s source and on `4d4dcff77494`:
   `foreground-map-tests` asserts that flag 3 of the foreground capability is
   refused, but `include/kui/native_standalone_foreground.h` has defined 3 as
   `KUI_NATIVE_FOREGROUND_CAP_VC2_PERSIST` since `eddbb01f` (5 Nov 2026); and
   `test_game_reader_provenance.py` compares against commit `2aac9af8`, which
   a shallow clone lacks (fetched, it runs). Neither is changed here.

Known differences from the parent K-UIs:

- Windows CE: when a CE game's map is full, the refusal reads as on the
  regular games' line ("... map exceeds N slots: T tracks + D data pieces
  need S. A fresh copy may reduce fragmentation."), not as on
  `1c71ba3afa17` ("Windows CE background map exceeds ..."). It refuses in
  the same cases.
- Regular games: everything `4d4dcff77494` added over Main, so a native
  result under `1c71ba3afa17` is not this K-UI's (64-slot map, no
  pool-overlap route, no cached mappings).
- Built as `1c71ba3afa17`, not as `4d4dcff77494` was (owner-source README:
  nightly `build_gui` with `KUI_GITHUB_NIGHTLY=1`, the default preserved
  Standard manifest, no bundle): the owner's Standard binding and the Smash
  bundle stay, the nightly flag stays off.

## Package

`KUI-One-KUI-Both-Threads-21NOV2026-2308-CST-64ae074.zip` (11,287,803 bytes, SHA-256 `f1c56130b54f57614d987708cfda321353ca7b7ccba586972f2804b79d57fbaa`):

| Path | Bytes | Payload CRC | SHA-256 | |
|---|---|---|---|---|
| `KUI/boot.kui` | 3,834,412 | `1431624F` | `d572510ce3d5804e...` | K-UI `64ae07435de5`, replaced |
| `KUI/runtime.kui` | 3,834,412 | `1431624F` | `d572510ce3d5804e...` | the same file |
| `IF-NEEDED/KUI/boot.kui`, `runtime.kui` | 3,816,892 | `2E38CCC2` | `902270842a277221...` | the owner's `1c71ba3afa17`, the rebuilt bytes of the owner's file, copied only to go back |
| `INVENTORY.json`, `README.txt`, `SHA256SUMS.txt` | | | | the inventory with full hashes, the instructions, the sums of every file |

Retained on the card: every reader module and helper (the New reader module
the owner has there, Test 84's or the owner's own), the Windows CE CD music
modules, `ce-boot.kui`, `ce-probe.kui`, banks and their selection records,
settings, virtual VMU cards, saves, `KUI/SURVEY/STREAM.TXT` and the games.
No updater package, activation record, bank, reader module or user data is
in the archive. Extracted, every file matches `SHA256SUMS.txt`, the two
`KUI/` files are identical and pass the delivery check. The README, the
sums, the inventory and `package.json` are in `evidence/one-kui-64ae074/`.

## Console test

The README asks for, with this K-UI on the card (Diagnostics' first line
`K-UI V2.0.0 SD runtime 64ae07435de5`):

1. Test 84 again with Test 84's module, its README's options and the same
   `STREAM.TXT`: Bangai-O, MDK2 and Plasma Sword (refused last time);
   Omikron / The Nomad Soul (it did not reach Test 84's module under
   `1c71ba3afa17`; the routing check sends it there under this K-UI); POD
   SpeedZone and Tokyo Xtreme Racer after copying their folders again.
2. Two or three Windows CE games with CD music On, as the owner plays them
   now: they should behave as under `1c71ba3afa17`.
3. One Smash Pack game.

A console result is read against the K-UI build its logs name
(`docs/gui-lines.md`, rule 4).

## After delivery: the Windows CE line to `949be408`

**The Windows CE session's check on the owner's kernels** (its report of 22
Nov 2026, attributed to that session, not run here): at `86214640362c`,
whose K-UI compile inputs equal `64ae0743`'s, `build/test-wince-packaged-prepare`
passes with all 21 delivered CD music modules and the owner's real kernels,
raw and 2048-byte DATA, with check counts identical to its own tree's; so do
`test-wince-cdda-bam4-group`, `test-wince-cdda-wwp-group` and
`test-wince-runtime-families`. Virtua Cop 2's module rebuilds byte-identical
to `1ac7e667c0d3`. Not in that check: `ce-cdda-bam4.kui`, `ce-armada-all-on`
and the generic `ce-boot` route.

**The line moved on after the build** (`beeef062` host tests; `0414c2d0`
Virtua Cop 2's CD music reader keeps the all-on reader's 1 KiB stack;
`949be408` that reader takes video-memory stream pieces), so
`tools/check_gui_lines.py --commit 64ae0743` fails on them. They are merged as
`89a241cd` (the fixture's two shared sources, `retail_loader_layout.h` and
`retail_resident.S`, are clean unions; fingerprints recorded with `--write`).
From `89a241cd`:

- the K-UI built with this recipe equals the delivered `64ae07435de5` apart
  from its build-ID text: five places (three whole, two split into an
  8- and a 4-character literal) and the two header CRC words that follow
  from them; every other byte is identical (`evidence/one-kui-64ae074/merge-89a241cd.txt`).
  The delivered K-UI is therefore this source's K-UI; no new one is needed;
- the four regular-games modules rebuild byte-identical (Test 84's two, the
  owner's Test 83 reader, the installed New reader's recipe), with Test 84's
  linked and parts checks passing;
- the 27 Windows CE module builds (the 26 above and the new reader's defaults)
  are byte-identical to the same recipes at `949be408`, and every one but the
  new reader is identical to its build at `64ae0743`; Virtua Cop 2 with its
  new switches off is `1ac7e667c0d3` exactly, and with every switch off round
  2's `9e89e778370f`.

Three independent reviewers checked the merge (`evidence/one-kui-64ae074/verification-89a241cd.json`;
host checks, not console runs):

- **The merge itself:** the three files both lines changed (`Makefile`,
  `retail_loader_layout.h`, `retail_resident.S`) equal a clean three-way
  merge; every other path equals the side that changed it; `git merge-tree`
  differs only in the fixture, whose 30 fingerprints the reviewer recomputed.
  The four new switches can be defined only by `Makefile.armada-stream` for
  Virtua Cop 2's module, and its guards refuse them anywhere else. Every K-UI
  translation unit preprocesses identically at `64ae0743` and `89a241cd` (the
  two that include `retail_loader_layout.h`, `games.c` and `games_retail.c`,
  with identical macro tables). The K-UI's CD music admission functions are
  identical on both lines and the merge, and read only the package header,
  the `KUICDK01` record and the seed, none of which the new reader changes.
- **The K-UI bytes:** the same identity as above, found independently: same
  6,589 symbols at the same addresses and sizes, every section equal once the
  build-ID text (two split by the compiler around `&update_pending`) is
  replaced; both ELFs flatten to their `runtime.kui` byte for byte.
- **Host tests at the merge and at `949be408`:** the Windows CE groups, the
  runtime families, the Armada stream suite, the new video-memory CD music
  reader test, `test_retail_ce_map_guest.py`, the excerpt checker's self-test,
  the switches' production text, the line check's test and
  `tests/test_games_retail.py` (408 PASS lines, line for line as before) pass
  with identical output.

The delivered K-UI's admission of the actual new Virtua Cop 2 reader needs
the owner's private kernel, so it was not run here. **The Windows CE session
ran it** (its report of 22 Nov 2026, attributed to it):
`test-wince-packaged-prepare --bam4-kernel` with the owner's Virtua Cop 2
kernel, raw and 2048-byte DATA, from the K-UI source `64ae0743`, passes for the
new reader `949be40865cf` (920,757 checks) and the layout-only control
`0414c2d08b02` (920,053), with the same counts from its own tree; `64ae0743`
also admits all 21 earlier CD music modules with counts identical to
`86214640`'s. Its independent build of `949be40865cf` matches the one here
byte for byte. It then delivered that reader to the owner as a module-only test
(`KUI-WinCE-CD-Music-Virtua-Cop-2-Video-Memory-22NOV2026-0004-CST-949be40.zip`,
for the installed K-UI). The line ledger records `89a241cd` under
`installed`/`equivalent`, so `--installed-has` counts the Windows CE line's
commits up to `949be408`.

## Console result

Pending.
