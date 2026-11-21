# WinCE RAM pool and read-ahead tests (generic `ce-boot.kui`, 21 Nov 2026)

**Status: two test builds delivered 21 Nov 2026; console tests pending.**
Two behaviour changes to the generic Windows CE module, each with its own
console test, in this order ([design](wince-unified-runtime.md)):

1. **The pool alone**, on the timing probe, so its rings compare with the
   adaptive-window session ([record](wince-adaptive-window-test.md)).
2. **Read-ahead into the pool**, on the everyday candidate `91b0540103f0`'s
   reader. The probe's resident has no room for it, so this test reports what
   the owner sees, not timings.

Both builds are from `1a5f4677` (build `1a5f46776c0c`); their switches and
payload CRCs tell them apart.

| | Test 1: pool | Test 2: read-ahead |
|---|---|---|
| Switches | `KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64` | `KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64 KUI_CE_READAHEAD=1` (`KUI_CE_VRAM_DMA=1` by default) |
| Module | 93,816 bytes, payload CRC `FCE53735`, SHA-256 `ac6600b1ca16e046efc365b6719b85e86cee9d0157b7c8edba2c8aaaed95e446` | 86,884 bytes, payload CRC `2C8248D8`, SHA-256 `2c6bf8d2cbebc984bf3b1a470f0ba8aa6fc80eeed3f4d900781eed3016cc4958` |
| Baseline | the adaptive timing build `871c4c7784b4` (same reader, no pool) | the candidate `91b0540103f0` (same reader, no pool or read-ahead) |
| ZIP | `KUI-WinCE-Pool-Timing-Test-21NOV2026-0006-CST-1a5f467.zip` (SHA256 `0099aa561f62f04be3176eec5b0efe64726a9b08d1453a0f828a31e5c28b1ce4`), blank 64-slot `WCEDATA.BIN` file id `bcd16a2e664c9574b66cfcc07c0e384c` | `KUI-WinCE-ReadAhead-Test-21NOV2026-0006-CST-1a5f467.zip` (SHA256 `ca7c2205ea4c6c6bd870211fc5bfaef3371c5232ece9a852e8d8b9878afeed6d`) |

Both ZIPs carry the candidate `91b0540103f0` and the installed `7b216f2e1136`
in `IF-NEEDED/` to restore. Test 1's background reader is the adaptive
probe's, unchanged (the pool is the stage's); test 2's ends at `0x8c00da40`
(limit `0x8c00dc00`, the read-ahead profile's 1 KiB stack), with a call-graph
stack worst case of 416 of 976 bytes. Test 1 keeps the probe's reader without
the PVR stream pieces: the probe's resident has no room for them either, so
KISS Psycho Circus and Virtua Cop 2 are in test 2 only.

## Test 1: what the pool costs

The stage lowers CE's RAM end by 64 KiB before NK runs, on every kernel whose
ROM header has CE's layout, and the relay checks after bootstrap 2 that the
pool is still zero and the patched header word unchanged. Nothing uses the
pool in this build. Questions:

- Does any game mind 64 KiB less? Each launch's stage record says whether the
  pool was reserved: `rom_header_method` 2 and `flags` bit 2, with the header
  as found (`ram_end` the original RAM end).
- Do the reads stay as fast as on `871c4c7784b4`? The rings of the same games
  compare directly.
- Which kernels does the rule miss? One is known: RAM ending at `0x8cff0000`
  keeps all its memory (the pool would overlap the stage's stack; the screen
  shows `CE RAM POOL OFF 00000010`).

Run list: Maximum Pool (US), Resident Evil 2 (US) disc 1, Super Producers
(JPN), Armada (US) with `ce-probe.kui` aside, Worms Armageddon (US), Super
Runabout (US), then every other WinCE game the owner has, a minute each.

## Test 2: read-ahead

After two sequential reads, the reader keeps the card's stream going while
no request is active and stores what arrives in the pool; the next read takes
it from there ([how](wince-unified-runtime.md#one-reader-one-scheduler)). The
boot screen shows `CE RAM POOL FROM` and `CE READ-AHEAD INTO` with the pool's
address (`8CFC0000` on CE 3's usual RAM end, `8CEE0000` on CE 2's).
Questions, each against the candidate `91b0540103f0`:

- Maximum Pool's openings and menus, Resident Evil 2's intro, the end of
  Super Producers' movie, Armada's intro and play, Worms' intros: better, the
  same or worse.
- Super Runabout (the heaviest card traffic), KISS Psycho Circus and Virtua
  Cop 2 (video-memory streams): unchanged.

A game that fails in test 2 but ran in test 1 points at read-ahead; one that
fails in both points at the pool.

## Not answered by these tests

- Read-ahead's timings: no measurement build carries it.
- CD music On (`ce-cdda-*`) and VMU On (`ce-armada-all-on`): those modules
  have neither change.
- The kernels the owner has not run; the rule is proved on the 19 surveyed.
