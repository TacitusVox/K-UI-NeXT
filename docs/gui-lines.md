# One K-UI for every thread

Two threads change K-UI at the same time: the regular games' line (the
Background reader, CD music, virtual VMU, with the owner's Test 83 reader and
GUI lines) and the Windows CE line (CE CD music, the generic reader, CE routes).
Each builds K-UI from its own branch. A K-UI built from one line alone drops the
other line's K-UI work, and the owner's next test of that work fails for a
reason no test of the work can show.

That happened on 21 November 2026. The owner ran the Windows CE line's K-UI
`1c71ba3afa17` (Main `4441e92` with the CE routes), which has none of the
owner's Test 83 GUI line `4d4dcff77494`. Test 84's module needs that line's
160-slot Background map, so the old 64-slot map refused Bangai-O, MDK2 and
Plasma Sword (`cdda-background-test-84.md`, Console result).

## The rule

1. **Before building a K-UI for delivery**, from the clean checkout it is
   built from:

       python3 tools/check_gui_lines.py

   It fetches every line `docs/gui-lines.json` names (Main and each thread's
   branch) and fails if the build lacks a commit of theirs with code in it
   (docs-only commits only warn). Merge the line it names, rebuild each line's
   delivered modules from the merge and compare their bytes, run both lines'
   K-UI tests, and only then build K-UI. Never deliver a K-UI that drops
   another line's work. `make check-gui-lines` runs the same check.
2. **After delivering a K-UI**, record it as `installed` in
   `docs/gui-lines.json` (build, source commit, the delivery), in the
   delivery's docs commit, on the delivering line. The other line takes the
   record with its next merge.
3. **A module-only test** (a reader module without a K-UI) names the K-UI it
   was admitted with and checks the owner's K-UI has what it needs:

       python3 tools/check_gui_lines.py --installed-has <commit of the K-UI change the module needs>

   If that fails, the delivery brings a K-UI that has it. The test's README
   tells the owner to check the first line of the Diagnostics page
   (`K-UI V2.0.0 SD runtime <build>`) before the first launch. When a line
   moves on after a K-UI is built and the K-UI built from the merge is proven
   to be the installed one apart from its build-ID text, the merge is
   recorded under `installed`/`equivalent` with its evidence, and its commits
   count as contained (64ae07435de5 and the merge `89a241cd`).
   Since K-UI `d41a04f40f23` a reader module that recognizes more games than
   K-UI's own SDK check is such a case: K-UI refuses a Background launch its
   check does not admit, before the module's stage is asked
   (`background-launch-checks.md`). That module brings its K-UI.
   Since K-UI `53a33ec708ab` this also holds for a program too large for
   K-UI to hold: there K-UI refuses by its own copy of the Maple discovery's
   32-word table. A reader whose discovery accepts another family, skips its
   check in some mode or takes a longer program changes that copy in the
   same commit and brings its K-UI; `tests/test_games_native_witness.py`
   fails until it does.
   Since K-UI `aaaba0ea3640` a program too large to hold that has the
   table's words is refused as well: a reader that can run such a game
   brings a K-UI that can check it.
4. **Before reading a console result**, compare the K-UI build in the owner's
   logs with `installed`. A result from another K-UI is that K-UI's result.
5. **A new thread** that changes K-UI on its own branch adds its branch to
   `lines` in its first commit.

## Building the owner's K-UI

Since the 3.0 RC1 release (Main `9c380d74`, 22 November 2026) the recipe is a
script, the same one the release and the nightlies use:

    bash tools/build_distribution_gui.sh --candidate
    python3 tools/runtime_package.py build build/kui-runtime.elf runtime.kui --build-id <HEAD, 12 hex>

run from a clean checkout of the commit to deliver (`--nightly` only for
Main's own nightlies). It links the four Smash Pack stages of
`resources/distribution` (staged by `tools/stage_distribution.py --gui-inputs`),
the Standard binding `resources/updater-bindings/vga-standard.h` (including
the RC1 reader) and launch timing, with `BUILD_ID` the commit's first 12 hex digits. `KUI/boot.kui` is the
same file as `KUI/runtime.kui`.

These are the settings that rebuilt `1c71ba3afa17` byte for byte and built
`64ae07435de5` (`one-kui-both-threads.md`): the script only fixes them in one
place. The bundle it stages is the one those K-UIs carry (build
`cc81e63ca4d2`). KallistiOS is `fcfa7d8`; its banner carries the date its
library was built (27 October 2026 in the K-UIs built here; the release's
own build has its builder's date).

Main's nightlies and release candidates are built this way from Main, and
Main holds every line since 22 November, so they are the owner's K-UI too:
the 3.0 RC1 K-UI `93f13e84d989` passed the owner's hardware checklist. A
nightly installed as an update goes into a bank that Restart K-UI selects;
a manual-copy ZIP replaces the files a cold boot reads. Which K-UI a result
came from is still read from its logs (rule 4).

## Why every commit, not only K-UI files

The check asks for each line's whole head, not a list of K-UI paths. Readers,
admission records and K-UI share headers and layout constants
(`include/kui/`, `src/loader/retail_*`), so a "loader-only" change can still
change what K-UI admits; and each line keeps its test code behind its own
switches, so merging a line is inert for the other line's production builds.
The merge that made one K-UI for both lines (22 November 2026, `2e03b96e`)
found two textual conflicts, both additions at one place, and four shared
sources whose production text is the clean union of both lines' changes.

## Files

- `docs/gui-lines.json`: the lines and the installed K-UI.
- `tools/check_gui_lines.py`: the check; `tests/test_check_gui_lines.py` holds
  it to its cases on a repository of its own.
