# Themes engine — first console candidate

Base: Main `c6a07a130791f5b87facc72959ecd87c2c97ebce`, verified unchanged remotely at package time.
Compiled source: `8f6ac75507872362ebb2e6f05dc1458b8a514a2f`, clean.
Branch: `codex/themes-engine`; no merge to Main.

The feature adds Settings > Appearance, independently chosen runtime splash/header/background,
RGB overrides and presets, Preview/Apply/Cancel, CRC-checked dual-slot preferences, strict prepared
assets and a Moonlit sample. All newly loaded image memory is worker-prepared and main-published;
normal navigation performs no theme filesystem I/O. Saved image buffers are released for memory
and executable handoffs and restored asynchronously if the operation returns.

## Validation

- `make test-themes`: pure parser/record/palette tests; actual bundle/publication code with ownership/fault stubs;
  shell transaction/render tests; 19 cases each on real FAT32/exFAT images plus missing/seed cases;
  eight converter/production-C-reader tests. ASan/UBSan enabled for C tests; LeakSanitizer disabled
  because the environment is ptraced. The coordinator tests count all owned allocations explicitly.
- Full existing shell suite passes. Splash decode/release, system settings, UI rate, viewport,
  startup audio, font bounds and actual boot canvas dispatch tests pass.
- Default Home/Games/Gallery output is byte-identical to the archived Main baseline.
- Pinned SH-4 build passes using GCC15.2.0, binutils2.45.1, newlib4.6.0.20260123,
  KOSfcfa7d869471, default admission manifests, no embedded module bundle,
  and `KUI_SMASH_LAUNCH_TIMING=1`.
- Both linked GUI CRC entry points pass 256 actual SH execution cases.
- The exact delivered runtime loads successfully through the production C file consumer on FAT32
  and exFAT without modifying either image. ZIP aliases are byte-identical; payload/header CRC,
  storage handoff, updater engine3 and empty RAM handoff markers are checked.
- Main's boot-only source still compiles with themes excluded. No boot-disc artifact was built.

## Size and timing scope

Runtime payload grows from2350260 to2370076 bytes (+19816).
Static runtime memory extent grows from6643472 to6682416 bytes (+38944).
Additional normal-menu image heap is bounded to742400 bytes (725KiB); Default allocates no
external image buffers. Preview and transactional replacement can temporarily retain old and new
bundles, up to3375KiB including a saved menu plus a displayed full preview and a replacement.

The recorded host benchmark is diagnostic only. It times the host renderer including its
background fill; Main's console path already used a packed fill. Do not interpret the lower host
CPU figures as a measured Dreamcast speedup. Custom startup assets add bounded one-time reads;
the immediate embedded splash, three-second deadline and B skip remain in place.

## Console status

NOT tested on a Dreamcast. Validate default/custom responsiveness, Preview/Cancel/Apply,
independent image choices and RGB edits, cold-reboot persistence, and normal game launch/return.
The sample screenshots use labeled host fixture state and retain the current V2.0.0 release label.

Reader/VMU/updater algorithms and boot-disc assets remain the Main versions. This work does not
resolve or re-qualify Main's existing Windows CE/Smash/physical-disc compatibility gaps.
The package omits all reader files, saves, preference records, banks and activation records.
Install through cold CD/root boot; menu Restart may choose an older saved bank.

See [theme usage/format](../../docs/themes.md), `artifact.json` for the exact ZIP hash,
and `install-manifest.json` for all five installed file paths and hashes.
