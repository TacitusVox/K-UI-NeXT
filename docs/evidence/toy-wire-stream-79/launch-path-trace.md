# The launch path of Test 79 from the Games menu

A second reader's trace of the source, 16 November 2026, made before Test 79 was committed. Where it says
T79-wt it means the working tree of that hour: the change as first committed (fed458d6ef97) without the
report's row of the low reader's totals and without the loan before the pages, both of which this trace led
to, and before the review's corrections. Line numbers are that tree's. What the author checked again is
named in docs/toy-wire-stream-test-79.md, Verification. Scratch paths are shortened.


## Basis and limits

- Read-only. Nothing in `/home/user/K-UI-NeXT` was modified, built, staged or committed. Scratch work is in `<scratch>/t79/trace/`.
- The tree moved while I read it, so two states are described:
  - **T77/78** = committed sources (Test 77 = `ded3083`, Test 78 = HEAD `1d0e5ba`). Delivered Test 77 module sha256 `1c1e54f3fc5ebad7232dd3bb2d158e6e6521eebfd1a25d1f7ed7b9723dc37323`.
  - **T79-wt** = the uncommitted working tree with `KUI_TOY_WIRE_TITLES` (`TOY_WIRE_TITLES=1`) as it stood when read: 11 modified files plus `tests/test_toy_titles_stage.c`; dev build `build/dev79/retail-toy-test-79.kui` (label `aaaaaaaaaa79`, 299,260 bytes, sha256 `c18b654fa7a7cec6d6d78b5e75876688b6fc5d25c78341feaf34e0f938a2fdf1`). It can still change.
- Line numbers are the working tree's when read. Six files have uncommitted edits and their lines will move (`vmu_native_passive.c`, `vmu_native_heap_probe_stage.c`, `native_readahead_aux.c`, `vmu_native_readahead_stage.inc`, `toy_suite_report_save.inc`, `toy_movie_iso_stage.inc`), so I name the function each time.
- Per-component switches came from dry runs of both recipes in a snapshot outside the tree. Differences came from preprocessing each object with its exact switches and diffing suite against the installed compact recipe (listings in the scratch `pp/` folder).
- Not available here: the installed main GUI binary and its source commit (`f8807cb16561` is not in this clone), a console, any game image. Everything about the Games app is the current tree's `src/apps/games_retail.c`. "Inference" marks what is reasoned rather than read.

## What the owner's console would do

### Launch 1: Virtual VMU Off, another game (Evolution 1 or 2), New (X)

- **Games app:** accepts the file. No check identifies a test build. It writes one word into the loaded copy (mode 7), fills the native cache seed from the game's executable, writes the track map and hands off. The launch report records the test module's build id and CRC.
- **Stage:** the same screens as the installed module, including lines that are untrue for this build (`BACKGROUND X - SDK SCI / PIO CACHE`, `PREFETCH AUTO / SPLIT CONTROLLER ALL`). The read-ahead line differs by build:
  - T77/78: `SCI RESUME OWNER UNVERIFIED`.
  - T79-wt: `SCI STREAM READ AHEAD READY` on a raw SCI image, otherwise `NO RAW SCI IMAGE / PLAIN READER`.
- **Game, T77/78:** runs on plain demand reading. No window opens for a non-Toy title, and the installed block prefetch is not compiled into this build. Expect Evolution's pre-prefetch gameplay lag back (inference from `docs/native-general-candidates.md:57`: "prefetch removed the residual gameplay lag in Evolution 1").
- **Game, T79-wt:** a read that begins right after the previous read's last sector, on a raw 2,352-byte Mode 1 track, opens a stream window to the end of that track. Every other read is plain demand. There is still no block prefetch. For Evolution 1 and 2 the stream's wait timer is never selected (certain from code, see 6).
- **Report gesture (both triggers + A+B+X+Y, about 3 s):** works without a report file and ends the run for good.
  - T77/78: two alternating pages, mostly zeros for another game, with `SAVE FAILED / FILM BOTH PAGES`.
  - T79-wt: the report text in pages of twelve lines under `REPORT NOT SAVED / FILM EVERY PAGE`, cycling forever. Start reboots to the K-UI CD.

### Launch 2: Virtual VMU On, another game (the owner forgets)

- **Games app:** does its normal VMU-On work first: selects (and if needed creates) the game's virtual card, may write a native-cache file, and copies the 4,096-byte card profile into the module. If the launch carries the A1 slot it loads `retail-native-dual-a1.kui` instead; the test file is not used and the game runs on the installed A1 module with no message.
- **T77/78 module:**
  - The stage prints `NATIVE VMU: LIVE OWNERSHIP / RECOVERY - KEEP POWER ON` and runs journal recovery.
  - It writes a zeroed marker sector to the card's journal, plus a replay write if a committed record was pending.
  - It loads the game and lets it run to its heap initializer, takes the 0x47000 lease from the game's heap and relocates the worker.
  - The worker's arm call then refuses. Screen: `PERSISTENT VMU PREPARATION STOPPED`, `PHASE 9`, `DETAIL 1`, `PROBE REFUSED - PHOTO THEN POWER OFF`, and `STORAGE WAS READ ONLY`. That last line is false on this path.
- **T79-wt module:** the stage stops at its first look at the mode with `THIS TEST BUILD NEEDS VIRTUAL VMU OFF`, `DETAIL` 0, `LAUNCH STOPPED - PHOTOGRAPH THIS SCREEN`, `POWER OFF AND ON TO RETURN`, `STORAGE WAS READ ONLY`. No stage card access and no game memory touched. The Games app's own VMU-On card steps have already happened.

### Launch 3: Virtual VMU Off, Toy Commander through the Games app (New (X), CD music Off, raw GDI)

- The stage computes the same identity as under Card Tools (IP CRC, executable CRC and size, then the SHA-256). All five player hooks are installed and it prints `SCI MOVIE READ AHEAD READY`. Reading and movies behave as in the Card Tools run of the same build.
- Differences from Card Tools:
  - No report file exists, so nothing is saved. T77/78 shows the two pages with `ID1` and `SAVE FAILED / FILM BOTH PAGES`; T79-wt shows the text pages.
  - The Games app does not force VMU Off (if On, launch 2 applies) and does not check the MK-57020 identity.
  - With CD music On, the Games app routes to the Standard/CDDA module and the test file is not used.

## 1. Games app admission (native GDI, New (X), VMU Off)

All in `src/apps/games_retail.c`, function `prepare_launch` (853-1656), unless another file is named.

**Route and file**
- `native_dual` = not CE, CDDA, excerpt or ZSO, and reader ASYNC or ASYNC_EAGER (902-903). RAM patches are refused on it (910-912). The VMU setting is loaded at 929-937. SCI storage is required (969-971).
- Package: `dual_a1 = native_dual && virtual_a1 && emulation_enabled` (990), then `kui_native_background_package` (991; `include/kui/native_background_route.h:11-15`). VMU Off always takes `0:/KUI/apps/games/retail-native-dual.kui`.
- `kui_runtime_read` (1014) goes through the updater resolver (`src/core/runtime_file.c:24-91`, `src/core/update_boot.c:66-90`):
  - Cold CD/root boot: identity path, the physical ROOT file (`src/dreamcast/main.c:5073-5084`).
  - After Restart K-UI with an active bank: an inherited entry reads the same root file, but a pinned bank replacement reads the bank's file (bytes, build and SHA-256 pinned) and the copied file is silently not used.
  - The embedded provider is consulted only for an unpinned, unredirected path (`runtime_file.c:41-49`). The recorded bundle holds the four Smash modules only (`tools/package_smash_collection_update.py:34-37`).
  - The log says which happened: `Loading <path>` and `Package <build> (<bytes> bytes)` against `Embedded package ...` (`runtime_file.c:46,70,71`).

**Checks of the module's bytes, in order**
1. Envelope: `KUIRUN1` header, sizes, payload CRC32 (`kui_runtime_header`, `src/core/runtime_image.c`). The build id is read and reported, never compared.
2. `layout_codec` (1022, body 125-150): header at 0x100, stage blob at 0x2000, size bounds, resident base and limit.
3. `kui_native_dual_descriptor_find(...) != 1` refuses (1053-1055).
4. `kui_native_background_module_matches` (1056-1058; `native_background_route.h:16-26`): exactly one dual descriptor; with no session descriptor the lease must be 0x47000.
5. `games_card_emulation_mode` (1060; `src/apps/games_vmu_card.inc:126-157`): for Off the toggle capability must be found exactly once; exactly one `KUINVM01` record (version 1, 32 bytes, reserved zero, mode NORMAL or READER_ONLY).
6. `native_cache_prepare` (1484-1487; `src/apps/games_native_cache.inc:115-248`): dual check 121-124, seed slot find 127, arena lease must equal the module's lease 235-239.
7. `kui_native_session_prepare` (1594-1595): no session descriptor in this module, so it returns true and does nothing (`src/apps/native_session_prepare.c:77-78`).
8. Handoff size guard: `probe_handoff_refusal` (`src/dreamcast/main.c:468-482`).

**Patches written into the loaded copy before handoff (VMU Off)**
- Mode word at record+16: 0 becomes 7 (`games_vmu_card.inc:126-157`).
- Native resolution cache seed `KUINVS01`, filled by `kui_vmu_native_cache_seed_fill` (`games_native_cache.inc:240-245`) from an in-RAM scan of the game's executable. With VMU Off, `disk` is false and no cache file is searched or created (179).
- Track map at payload 0x1000: `kui_retail_manifest_encode` (1611).
- Not written with VMU Off: card profile slot (`games_card_route` returns early, `games_vmu_card.inc:85`), session controls (no descriptor), RAM patches (`fill_ram_patches` returns at once when patches are off, 556-572).
- The card's module file is never modified. The launch report (1623-1642) writes `0:/KUI/apps/games/last-launch-a.txt` or `-b.txt` with `package:`, `source module build:`, `source module payload bytes:`, `source module crc32:` (`src/apps/game_launch_report.c:68-73`).

**Nothing on this route is keyed on build id, hash or manifest.** `resources/updater-bindings/preserved-standard-manifest.h` and `native-cdda-manifest.h` (`Makefile.dc:11-12`) feed only Standard-VMU and CDDA admission (`kui_native_vmu_standard_recognize` at 1046, `kui_games_cdda_package_admit` at 1406).

**One replacement that can serve a different file:** if the executable does not resolve and `retail-native-standalone.kui` is installed, the Games app replaces the package (1499-1521, log "New reader: virtual-A2 module cannot admit this executable; using standalone New without virtual VMU"). Inference: this does not happen for Evolution, since Evolution has the prefetch that only the dual module carries.

**Would a Toy-suite build be accepted? Yes.** I ran the Games app's byte predicates on real module bytes with a scratch host program (production headers and sources from a snapshot; three static functions copied). Both builds pass:

| Check | Test 77 module | dev Test 79 module |
|---|---|---|
| envelope, payload CRC | OK, build `ded3083ecdf8`, crc `ab8fe168` | OK, build `aaaaaaaaaa79`, crc `aa2c0759` |
| `layout_codec` | ok | ok |
| dual descriptor | 1 at 0x34320, lease 0x47000 | 1 at 0x341d8, lease 0x47000 |
| session descriptor | 0 | 0 |
| `background_module_matches` | X = 1, Y = 1 | X = 1, Y = 1 |
| toggle capability | 1 at 0x34300 | 1 at 0x341b8 |
| cache seed slot | blank at 0x32880 | blank at 0x32720 |
| card profile slot (VMU On only) | found at 0x202c0, template CID | found at 0x20360, template CID |
| mode write, Off | 0 to 7, one byte changed | 0 to 7, one byte changed |
| mode write, On | stays 0 | stays 0 |
| handoff limit 0x112000 | ok | ok |

Two consequences:
- Legacy (Y) also accepts the file. It would run this build's non-Toy legacy resident with the suite worker and no stream.
- The dev build carries a placeholder label; the final Test 79 bytes need the same run.

Rerun on any module:
`ASAN_OPTIONS=detect_leaks=0 <scratch>/t79/trace/admit/gui_admit <module.kui>`
Without `detect_leaks=0` the output is lost. I could not reconstruct its build line; a plain relink against the snapshot fails on the CRC sources.

**What `tests/test_toy_reader_package.c` covers** (`Makefile.vmu-native-toy-suite:514-518`; `docs/evidence/toy-wire-stream-77/package-admission.log` ends "RESULT: admitted by Card Tools' module checks, report seed bound"): envelope, CRC, a copy of `layout_codec`, dual descriptor, session count, a copy of the X match, toggle find, a copy of the Off mode-slot search, cache-seed find, Card Tools' report-seed slot, the 0x112000 limit.

**What it does not cover of the main GUI:**
- the VMU-On card profile slot and the mode write for On;
- the Legacy (Y) match;
- the cache-seed fill with a real game's record, and arena-lease equality;
- the standalone replacement decision;
- the resolver, bank and embedded-provider path, and the FatFs read;
- the production function bodies themselves (it uses copies).

**Existing host harnesses, and why none answers the question:**
- `python3 tests/test_game_reader_provenance.py --real-dual <dual.kui> --real-standalone <standalone.kui> [--json]` is the nearest. It compiles production `games_retail.c` with `tests/games_retail.c`, builds FAT32 and exFAT images, seeds the real module at `KUI/apps/games/retail-native-dual.kui` and runs `kui_games_retail_prepare_launch*`. Limits:
  - it is built without `-DKUI_VMU_GAME_CARDS=1` (`Makefile:846-849` against `Makefile.dc:176`), so the toggle and mode write, the VMU-Off route (refused at 943-947 in that configuration) and card preparation are compiled out;
  - its authored executable does not resolve, so the real dual is the final module only for Y;
  - it needs both real files and runs `make -n` and git in the tree. I did not run it.
- `build/games-retail <image> <fixture-dir> seed|check <case>` is the same binary used directly (`tests/games_retail.c:448, 1212-1236`).
- `tests/test_native_background_route.c/.py` and `tests/test_games_vmu_card.c` use synthetic payloads only.
- `tools/run_native_foreground_launch_route.py --module` is for the Standard foreground module.
- `tools/check_native_compact_delivery.py` audits a compact build directory against reference worker ELFs, not GUI admission.
- **Nothing existing runs the real (`KUI_VMU_GAME_CARDS=1`) native_dual VMU-Off route on real module bytes.** The scratch program covers the byte predicates only.

## 2. The same launch with VMU On

**Games app** (same function):
- Mode word stays 0 (NORMAL).
- `hint_allowed` (1466-1474) and the disk cache become active: a file `0:/KUI/VMU/native-cache/v<epoch>/<sha>.nvr` may be created (`games_native_cache.inc:46-86, 216`).
- `virtual_a2` is true once the resolution is READY (1553-1554).
- `games_card_prepare` (1561-1569; `games_vmu_card.inc:161-210`) finds the profile slot (164), selects the card (188-192; `src/core/vmu_game_card_service.c` can create the pair and write assignment and history files at 38-67, 304-310, 637-640, 686-692, 733-736) and copies the 4,096-byte profile into the module (195).
- A1: `src/core/shell.c:1475-1476` sets the A1 flag, then 990-992 load `retail-native-dual-a1.kui`.

**Stage and worker, T77/78, step by step:**
1. `kui_retail_stage_main` (`src/loader/retail_stage.c:1345`) calls `kui_retail_storage_init` at 1456. In this module that is `native_persist_storage_init` (`src/loader/vmu_native_heap_probe_stage.c:72`; body `src/loader/vmu_native_persist_stage.inc:158-246`).
2. Mode is NORMAL, so: profile decode and admit (171-175), SD init (176), acquire (192), then line 193 `NATIVE VMU: LIVE OWNERSHIP / RECOVERY - KEEP POWER ON`.
3. `kui_vmu_persist_stage_prepare_policy` (196-205) writes through `native_persist_write` and `native_persist_sync` (139-157):
   - `recover()` (`src/core/vmu_store.c:91-116`): if the journal marker is a valid committed record, sync, then rewrite that block into the card image.
   - `kui_vmu_store_checkpoint` (same file, 174-179): sync, then an unconditional write of a zeroed 512-byte marker to journal sector 1, and sync. Reached from `kui_vmu_persist_session_checkpoint` (`src/core/vmu_persist_session.c:460-473`).
4. The stage then proceeds as the installed module does: IP and executable load (`retail_stage.c:1500-1552`), `toy_movie_scan` returns (mode is not READER_ONLY), low resident installed (1619), bootstrap 2 (1638), relay, discovery, heap hook.
5. The game runs to its heap initializer. `kui_vmu_observe_stage_install` (`vmu_native_heap_probe_stage.c:538-599`) takes the lease.
6. `native_passive_run` (`src/loader/vmu_native_passive_stage.inc:345`): mode check passes (362-368), handoff (395-400), `RELOCATING PASSIVE MAPLE WORKER` (460), relocation (464-468), arm call (530-532).
7. The worker's `kui_vmu_native_passive_arm` (`src/loader/vmu_native_passive.c:1649`) returns -2 at 1690-1693 (`config.diagnostics != READER_ONLY` under `KUI_TOY_SUITE_MODE`; `reader_only()` is constant true at 317-321).
8. `native_report(9,1)` (`vmu_native_heap_probe_stage.c:345-367`) prints the allocator rows and calls `stopped("PROBE REFUSED - PHOTO THEN POWER OFF",9)`. `stopped` appends `STORAGE WAS READ ONLY` (`retail_stage.c:630-647`).

**Written to the SD card before that refusal:**
- By the Games app: the card selection files and possibly the `.nvr` cache file.
- By the stage: the zeroed journal marker with syncs, plus one replayed block if a commit was pending.
- Inference: no save data is at risk. This is the designed power-cut recovery and the card image is otherwise untouched. The message is still wrong, and the game's memory has been used.

**Earliest stop point:** `native_runtime_mode()` (`vmu_native_heap_probe_stage.c:211`).
- Its first call is `vmu_native_persist_stage.inc:168`, reached from `retail_stage.c:1456`.
- Before that, `kui_retail_stage_main` only captures and restores the display, decodes the manifest, runs `select_resident()` (1432, pure selection), checks the boot layout and prints lines. No card command, no game memory.
- The existing helper is `static void stopped(const char *message,uint32_t detail)` (`retail_stage.c:630-647`). The function already uses it for four mode refusals.

**T79-wt has put the stop exactly there:** `vmu_native_heap_probe_stage.c:214-222`, `if(mode!=KUI_VMU_NATIVE_MODE_READER_ONLY) stopped("THIS TEST BUILD NEEDS VIRTUAL VMU OFF",(uint32_t)mode);`. I confirmed the order above. It also covers Legacy (Y). Two cautions for the README:
- `stopped()` still prints `STORAGE WAS READ ONLY`. That is true of the stage, but the Games app's VMU-On steps have already run.
- An A1 launch never reaches this module, so it shows no message.

## 3. Suite stage, READER_ONLY, another title

**Walk:** `kui_retail_stage_main` (1345), storage init (1456, plain for READER_ONLY: `vmu_native_persist_stage.inc:168-170`), IP and executable load (1500-1552), `boot_crc` computed by the stage itself (1565), `toy_movie_scan` (1566-1567), resident (1619), bootstrap 2 (1638), relay and discovery, heap hook, lease, `native_passive_run`, `toy_suite_report_prepare` (`vmu_native_passive_stage.inc:410`), worker arm, SDK SCI route bind, `native_readahead_bind`, game continues.

**Every Toy-identity test and its outcome for another title:**

| Test | Where | Another title |
|---|---|---|
| IP CRC `0x38ba2868`, exec CRC `0xcdc493b3`, exec bytes 748,444, reader ASYNC, mode READER_ONLY | `toy_movie_scan`, `src/loader/toy_movie_iso_stage.inc:144-149`; constants `include/kui/vmu_launch_package.h:19-24` | T77/78: returns quietly; ranges, seed and `toy_suite_source` stay 0. T79-wt: new branch 136-142 sets `toy_suite_source=1` when storage is SCI and any data track is raw 2,352 with a 16-byte header (`toy_suite_raw_track`, 119-127); nothing is read |
| Four AVI names in the ISO directory | `movie_iso_ranges`, same file 72-113 | never reached |
| Report seed: `seed->test`, magic, CRC, LBAs | `toy_suite_report_prepare`, `src/loader/toy_suite_report_stage.inc:12-33` | quiet return; `cfg.toy_report` stays zero. With any Games-app launch the seed is blank, Toy included |
| `toy_poll_owner` = source 1, `exec_bytes==748444`, SHA-256 `ee68a326...9bbd` | `src/loader/vmu_native_readahead_stage.inc:194` | false |
| Literal pre-checks `0xac08d950`, `0xac098a88`, `0xac04c568`, `0xac04c774`; `toy_movie_exit_install`; four `toy_movie_probe_install` | same file 247-277, each guarded by `cfg.toy_poll_owner &&` | skipped quietly; nothing written to the game |
| Display line | same file 306-309 | message only: T77/78 `SCI RESUME OWNER UNVERIFIED`; T79-wt `SCI STREAM READ AHEAD READY` or `NO RAW SCI IMAGE / PLAIN READER` |
| Aux: `owner_verified` (189), `movie_variant` (0 without a mask), `toy_suite_active` (559-567), `in_play` (577), `toy_poll_context` (1374-1375), uses at 1183 and 1595 | `src/loader/native_readahead_aux.c` | all false or zero, quietly |
| Window gate | `suite_demand`, same file 583-595 | T77/78: `raw && toy_poll_owner` is false, never a window. T79-wt: `toy_movie_ahead(r,variant,raw,wait)`, window offered |

**Would anything stop a non-Toy title?** Nothing Toy-specific.
- The unconditional checks in `native_readahead_bind` (`native_report(11, ...)`) concern the module's own auxiliary image.
- The general stops are the installed module's (for example the SDK SCI route, `native_report(10,6|7)`).

**Installed into another game's memory when `toy_poll_owner` is false:** no Toy hook, probe or patch. What every title gets, as with the installed module but with different code: the low resident (larger, see 4), the worker and auxiliary inside the 0x47000 lease, the worker's Maple patches, and the SDK ERI/RXI slot callbacks.

**Stage lines that mislead for this build:**
- `BACKGROUND X - SDK SCI / PIO CACHE` (`retail_stage.c:1448`).
- `72 KIB VMU / 284 KIB BACKGROUND` and `PREFETCH AUTO / SPLIT CONTROLLER ALL` (`vmu_native_passive_stage.inc:667, 673`).

## 4. Low residents of the suite build

**Configuration** (dry run):
- New (X) resident: `KUI_TOY_SUITE_MODE=66` plus movie and playback capture, no wire switches.
- Legacy (Y) resident: no Toy switches; it preprocesses identically to the installed one.
- The Test 77 evidence agrees: `docs/evidence/toy-wire-stream-77/build-identity.log:31-33`.

**Toy-only at run time.** `toy_suite_active` is set only for requests at least 386 sectors into a Toy movie file (`native_readahead_aux.c:559-567`) and cleared at every REQUEST (`src/loader/retail_async.c:1582`). Behind it:
- bounded delivery (712-719, 740, 872);
- status-only CHECK (1377-1383);
- `kui_retail_native_demand_wake` (1414-1428), reached from the worker's `reader_cooperative_service`, which itself returns unless `toy_suite_active` (`src/loader/vmu_native_reader_capture.inc:251-265`);
- the DEMAND_GD visits (1436-1443);
- `movie_poll_can_yield` (686-696);
- the suppressed IMAGE READ FAILED fault (`src/loader/retail_resident.c:1720-1723`);
- in-place stream resume in `src/loader/sci_stream.c` (151-155, 843-858).

**Unconditional differences another game also gets:**

| Difference | Evidence | What another game sees against the installed module, VMU Off |
|---|---|---|
| No block prefetch or cache at all | `native_readahead_aux.c:1603-1607` (DELIVER goes to `suite_demand`), 1621-1626 (TICK only counts); `producer` and `seed_completed` (359, 408, 782, 815, 1636) compiled out, absent from the Test 77 auxiliary's symbols; the worker never sends TICK | no read-ahead between requests except the T79-wt window; the installed controller-tick prefetch is gone |
| Strict re-read | `sci_stream.c:50-51`, effect at 860 onward | a receive overrun mid-block is refetched (CMD12 and a new CMD18), not repaired from the CRC as installed; more latency where the game's own DMA cuts receptions |
| Resumable token, CMD12 and CMD18 waits; `kui_sci_stream_token_budget` on every delivery | `retail_async.c:717` | same limits by a different code path; unbounded when not Toy |
| Extra auxiliary visit per completed request and after ABORT, INIT, RESET | `retail_async.c:983-985, 1556-1562` | small fixed cost |
| REQUEST sets `toy_suite_kick=1` | `retail_async.c:1581-1587` | T77/78: no effect outside Toy. T79-wt: the first EXEC after a REQUEST counts as a kick and can select a window (`src/loader/toy_movie_ahead.inc:614, 635`) |
| EXEC/CHECK wait | `retail_async.c:1396-1405` | rule unchanged (`wait=e.target-e.since`); delivery goes through `suite_demand` to `ops->deliver(wait)`, with no cache-hit path |
| Larger low footprint | `include/kui/retail_loader_layout.h:52-56` against 73-74: async limit `0x8c007e80`, hook stack `0x8c007fe0` (installed `0x8c007ba0` and `0x8c007d00`); destination guard `retail_resident.c:439` | 0x2e0 more bytes below IP.BIN in use. Only Toy Commander has run with this placement (inference: untested for other titles) |
| Fault and exit screens | `retail_resident.c:836-857, 1045-1073` | a reader fault goes to the worker's terminal page, not the installed `GD STOP` rows; menu return has no `RET` report and no 900-frame pause |
| Hook entry stores the frame pointer | `src/loader/retail_resident.S:264-265` | none visible |

**Is the installed prefetch present for a non-Toy title? No**, in every suite build, T79-wt included. In T79-wt the only read-ahead another title gets is the stream window, for consecutive reads on a raw track.

## 5. The report gesture in the suite worker

`src/loader/vmu_native_passive.c`.

**Recognition**
- `HELD_REPORT_FRAMES 180` (744). `held_report_gesture` (756-758): A, B, X, Y pressed and both trigger bytes at least 200.
- It is evaluated only in the fast split-controller completion, for port A's GET_CONDITION reply (2313-2323; the `|| reader_only()` at 2315-2317 makes it apply to every driver family in suite builds), and passed to `toy_suite_finish_gesture` (2365).
- So it needs a controller in port A polled through the SDK's standard list for 180 consecutive such completions. It needs neither a seed nor a Toy title.

**Without a seed it ends the run at once.** `toy_suite_finish_gesture` (999-1011) calls `fail(0x4f)` immediately when `!config.toy_report.test`, even mid-read. Inference: the counters are then a snapshot taken mid-request. `fail` (1631) leads to `report()` (1416), which never returns to the game.

**Screen, T77/78, no seed.** `toy_suite_save_report` returns false at once with no card access (`src/loader/toy_suite_report_save.inc:881-884`). Two pages alternate (1463-1478), 250 frames each on PAL or 300 on NTSC/VGA (1450).
- Page 1, `toy_playback_page` (1056):
  - title `TOY TEST 77 1/2 PLAYBACK / 781250 HZ` (or 78) (1062);
  - for B then I: `B SUBMIT CADENCE MAX AT TAIL`, `B FIRST LAST CLK OUT SKIP`, `B ENTER SCENE EXIT FRAME` (1072-1079);
  - then `SAVE FAILED / FILM BOTH PAGES` (1467) and `START / REBOOT TO K-UI CD` (1471);
  - all values come from Toy's player hooks, so they are zero for another title.
- Page 2, `toy_storage_page` (1131):
  - title ends `2/2 / ID1 HEX` for the verified Toy, `ID0` otherwise (1143);
  - `SD BLOCKS PIO OVR CRC` (1168): the low reader's totals, valid for any game;
  - `AHEAD B I REQS VERIFIED FALLBACK` (1177), `PIPE B I CALLS IRQ FAIL` (1182), then `B/I WAIT USED NEED SKIP LBA` and `B/I READ N VERIFIED TICK AGE DONE` (1197, 1201);
  - then `SAVE FAILED / FILM BOTH PAGES`.
- Start, released then pressed, reboots through `0xa0000000` (`toy_report_pause`, 1358-1376).

**Screen, T79-wt, no seed.** `report()` 1454-1461 formats the saved report's text into the worker's buffer and calls `toy_report_pages` (1381-1397), which never returns.
- Each page: `REPORT NOT SAVED / FILM EVERY PAGE`, twelve text lines, `START / REBOOT TO K-UI CD`.
- The text is `toy_report_format` (`toy_suite_report_save.inc:566`): `K-UI TOY COMMANDER REPORT`, `TEST 79`, `BUILD <id>`, `HEX / 781250 HZ`, then the rows of `toy_report_wire` (69-203), label on one line and values on the next.
- Rows, in order: `PIPE B I CALLS IRQ STAY PT` (85), `AHEAD B I HIT EDC STOP OS` (99), `X OW E I C W O` (102), `BIG N S BACK TOY` (114), the movie rows (127-139), `K LIGHT EDC MEND MB LB ORD` (141), `V SCI TMR GAME N TICKS` (143), `P RING FEED CHK OUT SVC HALT` (148), `D N T SEEK` (155), `W START SEG CUT WAIT FULL` (174), `W BYTES WTICK TMR ERR DET` (176), `W BLK FIX UNFIX HOLED NOCRC SEEK` (178), `S C S F LF AM BC` (181), `W FILL REF BAD` (184), `BP ON OT OC OI SN ST` (198).
- For another title the rows that mean something are OW and OS (windows opened by ordinary reads and sectors served from them), HIT, the drop reasons, `TOY` 0, the stream's `W` rows and `TMR` (timer mode).

Three things to know about that screen:
- The two fixed pages are never shown when nothing was saved, and the text ignores the low reader's counters (`(void)sd;` at 202). The low reader's blocks, programmed transfers, overruns and CRC errors are therefore not visible in a Games-app run of a T79-wt build, yet most of another game's reads may go that way.
- A row that does not fit the 1,012 bytes is left out whole (`toy_report_short`, 28-45). The dev host log says 14 of the 17 essential rows fit at full width. A missing row is not an error.
- `declined` (take-overs refused) is not printed in this branch. `ERR` and `DET` carry only the first start error.

**Where the seed comes from.** Only Card Tools: `toy_report_prepare` in `src/apps/toy_suite_report_prepare.inc` at commit `09c9a4b686d4` (branch `toy-gui-history`), which creates `0:/reports/toy/<test>[-run].txt`. It is called only for its Toy test (its `games_retail.c:1677`, with VMU forced off at 950 and non-Toy refused at 1621-1624). The main GUI's `src/apps`, `src/core` and `src/dreamcast` contain no reference to the seed, the report file or `KUITRS01`. The main Games app can never provide it.

## 6. Other things that could fail or mislead on a first run under another game

**Stream timer: none for Evolution 1 and 2, with certainty.**
- `timer_select` (`src/loader/toy_wire_stream.c:449-463`) sets mode 0 and returns unless the vector base is `0x8c00f400`. It then accepts TMU channel 2 or 1 only if it is stopped, has no underflow interrupt, sits at IPRA level 0, and the SDK table slot for its event equals `SDK_DEFAULT 0x8c0be408` (51-53, 171-173). That is Toy Commander's own default-callback address.
- Evolution uses the same table (`include/kui/native_sdk_sci_route.h:12-15`), but its default callbacks are `0x8c1add0c` and `0x8c1d9f50` (`src/core/native_sdk_sci_route.c:49, 71`).
- No refusal follows. `wire_stopped` (369-391) puts a run cut short before 128 bytes into a wait that only a timer or "the next call that passes by" ends (`kui_wire_service`, 423-447). For another title the only callers are SCI interrupt visits while a run is live and the game's own GD calls; the controller heartbeat does not pump (see 4).
- Consequence: after a short cut the stream stands still until the game's next GD call. On the report: `TMR` 0, with `WAIT` and `WTICK` showing the cost.
- Other titles: inference, the same, since the constant is an address inside Toy's executable.

**DMA channel 1 and the bus.** `kui_wire_start` (516-568) refuses with `KUI_WIRE_E_BUS` (534-538) or `KUI_WIRE_E_CHANNEL` when channel 1 is busy or DMAOR is not exactly enabled and clean (539-542). `toy_wire_take_over` (`src/loader/toy_wire_ahead.inc:468-486`) also needs the resident hooked and `release.armed==0x10`. On refusal the window fills by the low reader's programmed single-block reads in `toy_wire_seek` (508-563). No stop.

**SDK SCI route: structural, with two exact owners.** Evolution 2 and Evolution 1 are admitted by SHA-256 with fixed fingerprints (`native_sdk_sci_route.c:33-78, 94-138`); any other identity goes to structural discovery (141 onward). The callback must lie in `0x8c004000..0x8c007ba0` (110, 155); the suite resident's entry is at `0x8c0072e4` in the Test 77 map. Same code as installed. Inference: Evolution binds as it does today. If the game later changes its vector base or the slots, reading falls back to foreground only and no window opens.

**Image format.**
- A window needs a 2,048-byte-sector request on a raw 2,352-byte track with a 16-byte header (`suite_raw`, `native_readahead_aux.c:451-461`). A cooked 2,048-byte image is not refused: it reads plainly, and with no block prefetch in this build it gets no read-ahead of any kind.
- T79-wt's stage line is per disc (any raw data track, `toy_movie_iso_stage.inc:119-127`) while windows are per track. Inference: an image with a raw track 1 and a cooked track 3 would print `SCI STREAM READ AHEAD READY` and never open a window; OW would stay 0.

**The window checks each sector; the installed reader does not.** `toy_wire_sector` (`toy_wire_ahead.inc:399-459`) requires the Mode 1 byte, the consecutive header address, and the EDC (every sector off the light path, one in sixteen on it). There is no EDC code in the ordinary reader. An image whose raw sectors carry stale or zero EDC (patched or rebuilt images) would lose its window at those sectors after up to eight retries (`TOY_WIRE_STALL_LIMIT`, same file 20, 346) and fall back each time. That is slower than the installed reader, not a failure. It shows as `BAD` and `E`. I found no ready host tool that scans an arbitrary GDI's EDC (`tools/verify_dump.py` checks K-UI's own captures only).

**Maple.** Stage discovery and the worker's run-time validation are the installed module's source in VMU-Off mode. The worker diff is limited to the constant `reader_only()`, the terminal workspace, the pages, the gesture path, the arm rule and `reader_cooperative_service`. A game that runs under the installed module with VMU Off passes the same checks. A run-time mismatch ends in `fail(code)` and the row `FAULT CODE AT VALUE EXPECT RESULT` (1501).

**Clock.** The pacing rules need the game's TMU0 in the profile of `include/kui/toy_pilot_clock.h:8-12`; otherwise fixed fallbacks apply, not a refusal. Inference: tick columns reading 0 beside non-zero call counts would indicate this.

**Comparison confound.** The Test 79 question is the window against the installed prefetch (`docs/fmv-stream-reader-plan.md:327`). The test module differs from the installed one in more than that: no prefetch at all, strict re-read, no timer, no controller-tick servicing, larger resident. Returned lag would not by itself say which.

**Routing and reading traps.**
- Restart K-UI can select a bank whose pinned reader replaces the copied file.
- An A1 launch with VMU On loads the other module.
- New (X) can fall to the standalone module for an unresolved executable.
- Legacy (Y) is accepted silently and has no stream.
- CD music On routes Toy Commander to Standard/CDDA.
- Old `last-launch-*.txt` and `launch-timing.txt` can be stale.
- The gesture is terminal.

## Not settled by the code, and what would settle it

- **Which main GUI is installed, and whether it behaves as this tree's source.** After the first launch: the log lines `Loading KUI/apps/games/retail-native-dual.kui` and `Package <build> (<bytes> bytes)`, and `last-launch-a.txt` or `-b.txt` showing the test module's build, bytes and CRC32.
- **Whether the final Test 79 bytes still pass the Games app's predicates.** Rerun `toy-wire-package-test` and the scratch `gui_admit` on the delivered file.
- **Whether Evolution's reads are consecutive often enough to open windows, and how the stream fares without a timer.** A console run: OW, OS, HIT, `W START SEG CUT WAIT FULL`, `WTICK`, `TMR`, and the drop reasons.
- **How the low reader fares under strict re-read in Evolution.** Not visible in a T79-wt Games-app run as written. It needs `SD BLOCKS PIO OVR CRC` (or the fixed page 2/2) added to the no-file cycle.
- **Whether the owner's Evolution images have a raw track 3 with valid Mode 1 EDC.** The `.gdi` listing (2352 against 2048) and a host scan of the track, or `BAD` and `E` on the console.
- **Whether `0x8c007d00..0x8c007fe0` is untouched by other titles, and whether their TMU0 matches the clock profile.** Console only.

## Files

Scratch artifacts:
- `<scratch>/t79/trace/admit/gui_admit.c`
- `<scratch>/t79/trace/admit/gui_admit`
- `<scratch>/t79/trace/pp/stage-diff.txt`
- `<scratch>/t79/trace/pp/diff-resident-async.txt`
- `<scratch>/t79/trace/pp/diff-resident-scistream.txt`
- `<scratch>/t79/trace/pp/diff-worker.txt`
- `<scratch>/t79/trace/make-np-77.txt`
- `<scratch>/t79/trace/make-np-compact.txt`

Modules examined:
- `<scratch>/clean77/build/toy-suite-77/retail-toy-test-77.kui`
- `build/dev79/retail-toy-test-79.kui`