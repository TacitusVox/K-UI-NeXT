# Maintaining the full Universal Loader nightly

A full nightly builds its GUI from the selected commit. Its external reader programs are the reviewed binaries in `resources/distribution`. `config/distribution.json` binds each path to its SHA-256 and records the source inputs that last received reader review. The same staged software feeds the SD ZIP and the full update package. Factory themes, music and dump databases come from their tracked source collections.

## RC1 reader provenance

Use the full member hashes in the distribution lock to identify software. The embedded build label alone cannot identify compiler settings, VMU profiles or role-specific builds.

| Shipped role | Origin and identity | Qualification record |
| --- | --- | --- |
| Generic WinCE, `ce-boot.kui` | RC1 selection `91b0540103f0`, SHA-256 `a75e0ce9010708953b707aad5db975bb83f9ee20f5657ddf4fdea8cf014acabd`. 96-slot map, video-memory transfers, 16-block window with two adaptive doublings. Pool and read-ahead disabled. Earlier accepted build `7b216f2e1136` remains the historical comparison. | [Generic reader observations](wince-pool-readahead-test.md) and [route contract](wince-gui-routing.md) |
| Four reference CD-music modules and Armada all-on | `1ba2fa7e7040`. Armada, Worms Armageddon, World Party and Bust-A-Move 4 each have a reference module. Armada all-on also carries a public VMU template. | [CD-music chain](wince-cdda-chain-test.md) |
| World Party and Bust-A-Move kernel variants | Eleven World Party-group modules from `1c71ba3afa17`, including the later Ducati fix at `d7fd810eb837`. Seven Bust-A-Move-group modules from `9e89e778370f`, including the later Virtua Cop 2 JPN fix at `949be40865cf`. The per-kernel records identify their full SHA-256 and image length. | [World Party group](wince-cdda-wwp-group-test.md), [Ducati](wince-cdda-ducati-repeat-test.md), [Bust-A-Move group](wince-cdda-bam4-group-test.md) |
| Native Standard, A1, disc readers, compression and legacy native CD music | Ordinary software package `KUI-Armada-VMU-Off-Fix-07NOV2026-1325-CST-4d893b3.zip`, SHA-256 prefix `a0ff7a4aa3b1d122`. Standard `retail-boot.kui` has build `9e3295bcfaec` and hash prefix `13791d32f34d6597`. The original MDK2 and Toy Story raw/cooked CD-music aliases have byte-identical partners. | [Delivery inventory](evidence/reader-corrections/delivery.json) and [Standard VMU binding](../resources/updater-bindings/installed-standard-9e3295bcfaec.json) |
| Native A2 and standalone MDK2 | A2 `f8807cb16561`, hash prefix `09c1480c6c470166`, from `KUI-Shared-Game-Launch-06NOV2026-2210-UTC-f8807cb.zip` (archive `dc607c3e89ab6bac`). Standalone 128-slot `e5af01888606`, hash prefix `a80bddb8eae8570b`, from `KUI-Test83-MDK2-Omikron-20NOV2026-1109-CST-2cc57af0a920.zip` (archive `aeacc57a471f8337`). The selected member is the standalone reader. Test 84's survey reader remains experimental. | [Regular-games console and host results](cdda-background-test-84.md) |
| Smash stages and helpers | `cc81e63ca4d2`, from `K-UI-Smash-cc81e63ca4d2-SCI-DAMAGED-BLOCK-CANDIDATE.zip` (archive `eb469340a2a6732a`). Four stages go both into the GUI and onto the card. Two distinct 48 KiB VMU helpers remain external. | [Delivered member hashes](evidence/reader-corrections/delivery.json) |
| Loader probe, selected-image probe and tools | Source-built from `d3c8472d8302`. `probe.dat` is the deterministic G3 fixture from `tools/make_loader_probe.py`. | [Loader probe ABI](games-loader-probe.md) and [selected-image probe](games-image-probe.md) |

## Replay or prepare a WinCE change

Use the pinned SH-4 toolchain and KallistiOS from the normal console build. With those dependencies installed, the replay runs offline. Commit the proposed source change first. The tool makes a temporary worktree at the selected revision and compares it with the distribution lock and binaries in the checkout where the tool was invoked.

```sh
source .deps/kos/environ.sh
python3 tools/rebuild_distribution_wince.py ../ce-replay
python3 tools/rebuild_distribution_wince.py ../ce-candidates --candidate
python3 tools/rebuild_distribution_wince.py ../vc2-candidate --only ce-cdda-6c09543a5680.kui --candidate
```

The default builds all 24 readers, including the generic reader, from clean objects and requires byte equality. Pass `--revision COMMIT` to test another local commit or repeat `--only FILENAME` to select roles. The group recipes come from the tracked kernel evidence and each family's makefile. Candidate mode first compares at the shipped build identity, then relinks changed readers under the selected source commit's identity. Its new output directory contains modules, individual build logs and `rebuild.json` with the source commit, build labels, old and new hashes, sizes and CRCs. It leaves the shipping inventory in place.

## Promote a reader change

1. Review which reader roles use the changed code, flags, layout or toolchain. Replay the whole WinCE cohort for shared changes. For native or Smash, use the role-specific makefile and release settings recorded with the original delivery. When retaining historical bytes, verify the full shipped hash and explain why the affected change leaves that role's qualified behavior intact. A newly compiled module can differ even with the same embedded build ID.
2. Run the relevant linked-image checks and host tests. For group WinCE changes run `make test-wince-cdda-wwp-group test-wince-cdda-bam4-group`. Check generic map capacity, kernel records, untouched CD music, and the default settings. For native or Smash changes check the launch path, both VMU positions where supported, normalized admission data and any helper's save layout. Distribute clean launch maps and canonical public VMU placeholders. A session-specific Armada `ce-probe.kui` can contain a real card identity and extents.
3. Record the actual console result for a behavior change, including title, image format, option settings and what played, launched, saved or returned. Label any remaining host-only coverage. A later candidate or survey with unresolved audio does not replace a qualified production reader until selected. Keep the old hashes and test conditions in the evidence.
4. Copy the qualified modules into their existing paths under `resources/distribution` and update the affected full member hashes in `config/distribution.json`. Update the matching GUI admission bindings, helper inventory or kernel route records when changing those contracts. Replacing a Smash stage also changes its embedded GUI copy. After the review, record the output of `python3 tools/stage_distribution.py --print-reader-inputs` as `reader_inputs_sha256` and commit these changes together.
5. Run the full release staging and build workflow. Its checks must validate the linked GUI against the same external readers, the update inventory, and the SD archive. Preserve the replay report and route-specific console results alongside the promotion record. The next full nightly can then consume the reviewed source and binaries.
