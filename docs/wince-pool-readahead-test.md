# WinCE RAM pool and read-ahead tests (generic `ce-boot.kui`, 21 Nov 2026)

**Status: both console-tested 21 Nov 2026.** Test 1: the pool is safe on seven
kernels and costs nothing ([result](#result-of-test-1-21-nov-2026)). Test 2:
read-ahead made no visible difference on three games, and the owner ended it
there; read-ahead is not adopted ([result](#result-of-test-2-21-nov-2026)).
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

## Result of test 1 (21 Nov 2026)

Seven launches, each ended by the owner with A+B+X+Y+Start after 1.8 to 4.8
minutes, every dump complete ([evidence](evidence/wince-pool-session-2026-11-21.json),
derived fields only). The owner sent no per-launch notes and reported no
problem.

| Game | Kernel | Pool | CE's RAM end while running | Pool pages in use at the end | 64 KiB below it | Top 64 KiB without the pool (D5) |
|---|---|---|---|---|---|---|
| Maximum Pool (US) | CE 3, 1,695,744 bytes | `0x8cfc0000` | `0x8cfc0000` | 0 of 16 | 16 of 16 | 16 of 16 |
| Resident Evil 2 (US) | CE 3, 1,204,224 | `0x8cfc0000` | `0x8cfc0000` | 0 | 16 | 16 |
| Super Producers (JPN) | CE 2, 819,200 | `0x8cee0000` | `0x8cee0000` | 0 | 15 | 15 |
| Armada (US) | CE 3, 1,251,328 | `0x8cfc0000` | `0x8cfc0000` | 0 | 15 | 15 |
| Worms Armageddon (US) | CE 2, 1,107,968 | `0x8cee0000` | `0x8cee0000` | 0 | 15 | 15 |
| Super Runabout (US) | CE 3, 1,064,960 | `0x8cfc0000` | `0x8cfc0000` | 0 | 15 | 15 |
| Worms World Party (US) | CE 3, 1,826,816 (not surveyed before) | `0x8cfc0000` | `0x8cfc0000` | 0 | 15 | not run in D5 |

- **The pool was reserved on all seven kernels**, CE 2 and CE 3, by the
  structural rule (`rom_header_method` 2). Worms World Party's kernel was not
  among the 19 surveyed: the rule found its header too, so it now holds on
  20 kernels.
- **CE honoured it.** In each running kernel the header's RAM end reads the
  lowered value and no other header byte differs. Without the pool (the
  adaptive session, same kernels) CE had used 15 or 16 of the 16 pages at the
  top of its RAM by the end of a launch; with it, those 16 pages, the pool,
  were untouched in every launch, and the 64 KiB just below, CE's new top,
  was in use instead.
- **No game minded 64 KiB less**: every launch played until the owner ended
  it, with no failed request, no refused or unknown command.
- **Reading is unchanged.** The streamed 32-sector reads of the same games
  against the adaptive session: Super Producers median 64.8 ms against 64.8
  (189 reads each), Super Runabout 97.1 against 97.2 (75 each), Armada's play
  86.8 against 86.9, Maximum Pool 78.5 against 81.7. Resident Evil 2's ring
  this time held a later stretch of the launch (146 to 181 s, against 2 to
  38 s), with the game asking less often; its reads, 76.7 against 72.9 ms,
  stay within the run-to-run spread of earlier sessions.

So the pool is safe to use on these kernels; read-ahead (test 2) is the next
behaviour change.

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

## Result of test 2 (21 Nov 2026)

The owner's report, against the candidate `91b0540103f0` (CD music Off, VMU
Off): Maximum Pool (US) the same, Resident Evil 2 (US) the same, Super
Producers (JPN) about the same. With no change in the three games read-ahead
was built for, the owner ended the test there. Nothing was worse.

The timing rings of test 1 and of the adaptive session show that read-ahead's
condition is met in these streams: in Super Producers' movie 98% of reads
begin exactly where the previous one ended (185 of 189), in Resident Evil
2's intro 84 to 93%, and in Maximum Pool's movie 86% (its menus, in test 1's
ring, 17%). So read-ahead either ran without a visible gain or did not run on
the console; no measurement build carries it (the probe's resident has no
room), so the two cannot be told apart from this test.

Decision: read-ahead is not adopted. It stays in the source, off by default
(`KUI_CE_READAHEAD=0`), and no further console time goes to it. The pool
stays: test 1 proved it, and it is what CD music and the VMU will share. The
everyday candidate remains `91b0540103f0`.

## After test 2 (21 Nov 2026)

The owner's card kept the read-ahead build as `ce-boot.kui`. With
CD music Off, 4x4 Evolution crashed on it after its intro was skipped
([report](wince-cdda-wwp-group-test.md#result-21-nov-2026)). The candidate
`91b0540103f0` went back on the card in
`KUI-WinCE-Ducati-Music-Test-21NOV2026-1929-CST-d7fd810.zip`, for the retest.
On it, the skip runs (owner, 21 Nov 2026), so the crash belongs to the
read-ahead build, which stays off.

What the analysis of that crash found (static, plus host models; no
console or emulator run):

- **The page.** GAME RETURN means a game program called the firmware's
  return (menu command 1). In 4x4's files only `DC.EXE` imports
  `ResetToFirmware`, next to its fatal-error strings, so `DC.EXE` (the game,
  past the movie) ended itself: either a fatal error or the reset buttons
  handled by the game. A fault K-UI's reader detects shows its own page and
  halts.
- **The skip sends no disc command.** `DCMOVIE.EXE` stops its DirectShow
  graph, starts `DC.EXE` and exits. The CD driver `wsegacd.dll` sends no
  abort or reset for this; its only ABORT follows a 15-second stall.
- **The counters show a clean card link.** GUARD 0, TOKENERR 0, FOREIGN 0,
  and CRC ERRS 29 against REPAIRED 30: every repaired block is refused and
  read again, so the card returned no bad data.
- **No defect found in the host models.** The reader of `1a5f4677` is
  unchanged at the time of writing. With and without read-ahead, the models
  ran about 26,000 cut movie streams followed by scattered page-in reads,
  with injected CRC errors, overruns, busy channels, aborts and resets. Every
  byte was right, and nothing was written outside the destination or after
  completion.
- **What only this build does.** Read-ahead often completes a whole request
  inside CE's REQUEST call. It also keeps the card streaming while no
  request is active, as during the switch from `DCMOVIE.EXE` to `DC.EXE`.
  The build also has a resident layout shifted by the pool's entry word and
  a 1 KiB stack instead of 2 KiB. The host models cannot represent the first
  two, so the mechanism stays unproven, with one crash on this build and one
  clean skip on the candidate. (A review of 22 Nov 2026 found that CE's
  exception vectors, identical in the 4x4 Evolution, Armada and Virtua Cop 2
  kernels, take TLB misses on banked registers and move to CE's own stack
  for other exceptions at thread level, which makes the 1 KiB stack the
  least likely of these
  ([record](wince-cdda-bam4-group-test.md#test-build-949be40865cf-virtua-cop-2-stream-pieces-into-video-memory)).)
- **If read-ahead or the generic pool is ever revived,** 4x4 Evolution's
  intro skip must be one of its console checks.
- **A latent defect of the shared reader, not this crash.** The GD ABORT
  completes the command without raising the drive or DMA-end interrupt, so a
  driver that aborts after a stall, as `wsegacd` does after 15 s, would wait
  forever. Nothing showed such a stall, and the candidate, which shares this
  code, survives the skip. A fix would be its own behaviour change.

## Not answered by these tests

- Read-ahead's timings, and so whether it ran at all: no measurement build
  carries it.
- CD music On (`ce-cdda-*`) and VMU On (`ce-armada-all-on`): those modules
  have neither change.
- The kernels the owner has not run; the rule is proved on the 19 surveyed.
