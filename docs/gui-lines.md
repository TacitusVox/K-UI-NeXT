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
4. **Before reading a console result**, compare the K-UI build in the owner's
   logs with `installed`. A result from another K-UI is that K-UI's result.
5. **A new thread** that changes K-UI on its own branch adds its branch to
   `lines` in its first commit.

## Building the owner's K-UI

The owner's K-UI carries the launch timing, the four Smash Pack modules in
its embedded bundle and the owner's Standard binding. This recipe rebuilds
`1c71ba3afa17` byte for byte and built `64ae07435de5`
(`one-kui-both-threads.md`):

    make -f Makefile.dc build/kui-runtime.elf BUILD_ID=<HEAD, 12 hex> KUI_SMASH_LAUNCH_TIMING=1 \
      RUNTIME_MODULE_BUNDLE=<directory with the Smash bundle, build cc81e63ca4d2> \
      NATIVE_STANDARD_MANIFEST=resources/updater-bindings/installed-standard-9e3295bcfaec.h
    python3 tools/runtime_package.py build build/kui-runtime.elf runtime.kui --build-id <same>

Build from a clean checkout (keep the bundle outside it, so the compile
receipt says `source_dirty: false`); `KUI/boot.kui` is the same file. The
nightly settings stay off. If no copy of the four Smash modules is at hand,
take them from the running K-UI's bundle
(`tools/build_runtime_module_bundle.py`: `locate`, `decode`); re-encoding
them must give the embedded bundle byte for byte. KallistiOS is `fcfa7d8`;
its banner carries the date its library was built (27 October 2026 in the
owner's K-UIs).

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
