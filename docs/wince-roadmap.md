# WinCE roadmap: boot, CD music and VMU for every WinCE game (17 Nov 2026)

The goal is one maintained K-UI that boots every Windows CE game in the
owner's library (127 images, 94 kernels) with CD music and a virtual VMU.
[The scope](wince-universal-scope.md) holds the evidence and the unit counts.
This page orders that work as a dependency graph and names what the data
session ([data probe](wince-data-probe.md)) answers. It changes no plan in the
scope; it adds the order and the prerequisites.

## Rules that shape the order

- **One behaviour change per step**, each with its own console comparison
  against a named baseline (GUI, module path, build, payload CRC). Data steps
  (D) change no game behaviour.
- **Gate G0.** Nothing that changes game behaviour stacks before GUI
  `069d612cadea`'s four console tests and the pending `7b216f2e1136` reports
  (BAM4, WWP, both Armada modules) are in.
- **Accepted bytes stay:** `ce-boot` `7b216f2e1136`, `ce-cdda-*`, the Armada
  modules, the Smash set, the native `f8807cb16561` cohort.
- **Separate requirements:** VMU-only, CD-music-only and combined modules are
  separate steps, and so are physical and virtual cards.

## Gates and data

| ID | Step | Depends on | Status |
|---|---|---|---|
| D1 | **Data session.** The owner swaps in the probe `ce-boot.kui` and runs as many WinCE games as possible with CD music Off, ending each with A+B+X+Y+Start. One file, `KUI/WCEDATA/WCEDATA.BIN`, collects a slot per launch. | nothing | done 19 Nov 2026: 17 games ([session 1](wince-data-session-1.md)) |
| D2 | **Mine the data.** `tools/wce_probe_data.py` and `tools/wce_probe_mine.py` turn the file into a per-game table: boot, disc use, CD audio commands, ports and saves, modules and DirectSound attachment, memory. Results go into the scope evidence. | D1 | done for session 1 (miner corrected: 8-bit CE names, no false saves or low-water mark) |
| D3 | **Timing session.** The probe with a request ring (`KUI_WCE_PROBE=2`, [data probe](wince-data-probe.md)): the latest 190 reads' begin and end to TMU0's tick and the reader's own time, for the movie freezes (Resident Evil 2, Sno-Cross, Super Producers), Maximum Pool's lag, Urban Chaos's lurches and Wild Metal's load. | D2 | done 20 Nov 2026: 8 launches ([timing session 1](wince-timing-session-1.md)) |
| D4 | **Shorter slow window.** The timing build with the 6.25 MHz window after an overrun cut from 64 receptions to 16 (`KUI_CE_SLOW_WINDOW=16`), for the reader-limited streams the timing session found (Super Producers, Maximum Pool, RE2's catch-ups). One change, compared with D3's rings. | D3 | done 21 Nov 2026: Super Producers fixed, reads 12-17% faster, no read slower ([results](wince-slow-window-test.md)); the everyday candidate's Armada launches ran `ce-probe.kui`, not the candidate (Armada's legacy module takes CD music Off when installed), so 16 has no Armada result |
| D5 | **Adaptive slow window.** The timing build with a window that starts at 16 receptions and doubles, up to 64, while overruns recur within 8 receptions of its end (`KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2`), for Armada's intro echo under the 16-reception window ([record](wince-adaptive-window-test.md)). One change, compared with D3's and D4's rings. | D4 | done 21 Nov 2026: Armada on the generic reader for the first time (with `ce-probe.kui` aside), at most a trace of echo and gameplay "great"; Super Producers and RE2 ran near 16, Super Runabout near 64; Maximum Pool still read-limited ([results](wince-adaptive-window-test.md#result-21-nov-2026)); everyday candidate `91b0540103f0` (with the PVR pieces) delivered |
| D6 | **The RAM pool alone.** The adaptive timing build with 64 KiB taken below CE's RAM end on every kernel whose ROM header has CE's layout (`KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64`), unused: whether any game minds 64 KiB less, and the reads unchanged from D5 ([design](wince-unified-runtime.md), [tests](wince-pool-readahead-test.md)). | D5 | done 21 Nov 2026: 7 launches on 7 kernels (CE 2 and CE 3, one not surveyed before); reserved on all, untouched by CE, reads unchanged ([result](wince-pool-readahead-test.md#result-of-test-1-21-nov-2026)) |
| D7 | **Read-ahead.** The candidate `91b0540103f0`'s reader with the pool and read-ahead (`KUI_CE_POOL_KIB=64 KUI_CE_READAHEAD=1`): after two sequential reads the card streams into the pool while no request is active, for the read-limited streams (Maximum Pool, Armada's intro, Worms' intro, Super Producers, RE2). Owner's report, no timings (the probe has no room for it). | D6 | done 21 Nov 2026: Maximum Pool and RE2 the same, Super Producers about the same; not adopted, stays off by default ([result](wince-pool-readahead-test.md#result-of-test-2-21-nov-2026)) |
| G0 | **Thursday's console tests:** GUI `069d612cadea` (four tests) and the `7b216f2e1136` reports. Restore the accepted `ce-boot.kui` first. | D1 finished (the probe is swapped out) | ready |
| B2 | **CHD to GDI import** for the PAL and JPN CHD images (43 images, 19 kernels exist only as CHD). Owner-side, on a PC. | nothing | owner task |

## Shared foundations (each one change)

| ID | Step | Depends on | Unlocks |
|---|---|---|---|
| F0 | Repository recipe matches the installed `ce-boot.kui`: 96 slots with the `KUICEM01` record, staged as `ce-boot.kui` (the repository still stages a 64-slot module as `ce-probe.kui`). Build hygiene; installed bytes unchanged. | nothing | full packages that admit NC2 |
| F1 | GUI: a WinCE VMU-only route, per-game cards, launch timing for CD-music-Off launches (`last-launch-a/b.txt` already names the module path, build and CRC), and the complete extent map for card preparation. | G0 | every VMU step |
| F2 | Per-kernel placement records (stage ROM placements, the helper's owner profile, interrupt words) instead of Armada's compile-time constants. | D2 (which kernels to cover first) | every VMU and CD music step on a new kernel |
| F3 | VMU-only layout that fits with the steady poll, and the two console-proven reader fixes in the VMU-only recipe. | G0 | S1 and every later VMU-only module |
| F4 | WinCE map larger than 64 slots in the VMU and CD music readers and their stage admission. | G0 | NC2 US/PAL VMU and CD music |
| F5 | Memory layout measured from RAM end: CE2 (RAM end 0x8cef0000) and the 9 kernels with other ends. | D2 (heap room) | S16, S18 |
| F6 | Relocated `maple.dll` text bridge on the standalone stage (as Swirl's helper does), and a pool plan with 32 KiB for it. | S1 | S17, WWP x family 2 |
| F7 | Optional: one CD music module per sound group carrying several exact-kernel records, instead of one module per kernel. | S3 or S4 | fewer modules for S3, S4, S11 |
| F8 | One RAM pool for every WinCE service, found by the ROM header's structure instead of one exact header per kernel (`include/kui/ce_ram_pool.h`; 19 surveyed kernels, three RAM ends). Built for the generic module (`c6745ab2`); the CD music and VMU parts move into it one module per change ([design](wince-unified-runtime.md)). | D6 | F5, S18v, S18c without per-kernel pool records |

## Boot (generic reader)

| ID | Step | Depends on | Unlocks (images) | Console test |
|---|---|---|---|---|
| B0 | Boot evidence on the generic reader for CD-driver lines 1, 3, 5 and 6, which have no generic-route result. | D1, D2 | lines 1, 3, 5, 6 (scope step 0) | done in session 1: every line reached a running game (line 3's KISS then stopped, B3) |
| B1 | Reader fix for any refused command the data shows a game issuing (command 26 open tray is the known one; a refused command stops the reader). | D2 | per finding | none seen in session 1 |
| B3 | DMA stream pieces into PVR video memory ([record](wince-vram-stream.md)): KISS Psycho Circus and Virtua Cop 2 stopped on them. Generic module only; the CD music modules follow after its console test and a space audit. Test build `a660354d804c` delivered 20 Nov 2026. | D2 | KISS, Virtua Cop 2 (CD music Off) | passed 20 Nov 2026: both get past where they stopped (owner) |

## Virtual VMU (one family or helper per step)

| ID | Step | Depends on | Unlocks | Console test |
|---|---|---|---|---|
| S1 | Family 7 VMU-only route on Wild Metal's kernel | F1, F2, F3, D2 (slot Wild Metal opens) | 2 | Wild Metal US, VMU On: save, power off, cold boot, load; intro and FMV |
| S2 | Admit HK Lovely Fruit Park, then Biohazard 2 VP (one kernel per change) | S1 | 3 | each: save, power cycle, load |
| S5 | Family 2 profile | S1, D2 (slot, saves) | 17 | Railroad Tycoon II US, Championship Surfer US |
| S6 | Family 6 profile | S1, D2 | 17 | BAM4 US, Caesars Palace 2000 US |
| S7 | Family 1 profile | S1, D2 | 12 | Bang! Gunship Elite US |
| S8 | Family 3 profile | S1, D2 | 7 | Urban Chaos US, Spirit of Speed 1937 US |
| S9 | Swirl's helper on the standalone stage (family 4) | S1, D2 | 5 | GAH 2 US or NFL QBC 2001 US; check the slot |
| S10 | NC2 US with VMU (larger map) | S9, F4 | 1 (+1 PAL after S8, B2) | NC2 US, VMU On |
| S16 | CE2 VMU layout with family 11, then families 10 and 8 | F5, S1, D2 | 11 + 9 + 2 | Worms Armageddon or QBC 2000 US; Super Producers |
| S17 | Relocated-text bridge: Maximum Pool, Starlancer, Virtua Cop 2 JPN, then families 9 and 12 | F6 | 22 | per kernel |
| S18v | Other RAM ends for VMU, one kernel per change | F5 | 18 | per kernel |

## CD music (one sound group or contract per step)

| ID | Step | Depends on | Unlocks | Console test |
|---|---|---|---|---|
| S0 | **The adaptive slow window in the five CD music modules** (`KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2`, source `52a94612`), the reader half of CD music On matching CD music Off ([record](wince-cdda-adaptive-test.md)) | D5 | the five CD music routes | Armada (VMU Off and On), BAM4, Worms, WWP with CD music On: console-tested 21 Nov 2026, gameplay still a tiny bit worse than CD music Off; not at parity ([result](wince-cdda-adaptive-test.md#result-21-nov-2026)) |
| S0t | **Where CD music On costs gameplay**: Armada's gameplay timing diagnostic, run with the music and with the music never queued (source `32b3bbb3`, [record](wince-cdda-play-timing-test.md)) | S0 | the fix for CD music On's gameplay lag, then CD music On for every CD music game | Armada (US), CD music On, VMU Off, two runs: console-tested 21 Nov 2026, the lag is the worker's programmed receptions while the music is urgent ([result](wince-cdda-play-timing-test.md#result-21-nov-2026)) |
| S0f | **The fix**: the worker's programmed receptions as an emergency only (`KUI_CE_CDDA_EMERGENCY_PIO`, source `06341c5a`, [record](wince-cdda-emergency-pio-test.md)) | S0t | CD music On by default, game by game | Armada (VMU Off and On), BAM4, Worms, WWP with CD music On, and one measured Armada run: delivered 21 Nov 2026 |
| S3 | BAM4 adapter on its 10 other kernels | G0, F2, D2 (DirectSound attached; Super Runabout loads it from disc) | 12 | Caesars Palace 2000 US; Super Runabout US; Virtua Cop 2 JPN |
| S4 | WWP adapter on its 8 other kernels with the usual RAM end | G0, F2, D2 (CD audio commands and polling) | 12 | Railroad Tycoon II US, Ducati World US, Net de Para JPN |
| S11 | Worms adapter, then Armada adapter, on their other kernels | G0, F2, D2 (does Wild Metal attach DirectSound?) | 4 + 3 | Sega Rally 2 US; Wild Metal US |
| S13 | NC2-PAL contract, then Sno-Cross (both WWP variants) | S4, F4 (NC2), B2 (NC2 PAL) | 7 + 2 | Spirit of Speed 1937 US, Urban Chaos US; NC2 PAL |
| S14 | NC2-US contract | F4, D2 | 5 | GAH 2 US, NC2 US |
| S15 | Giant Killers, QBC 2000, WebTV/CE2-B, KISS, Sega Rally 2 JPN contracts (one per change; KISS also needs F5) | D2 (DirectSound attachment) | 5 + 2 + 3 + 2 + 1 | one representative each |
| S18c | Other RAM ends for the CD music pool | F5 | 8 | per kernel |

## Combined CD music and VMU (one pair per step)

| ID | Step | Depends on | Unlocks | Console test |
|---|---|---|---|---|
| C1 | Wild Metal combined (Armada's layout) | S1, S11 | 3 | as Armada's acceptance: intro, music, save, power cycle, load |
| C2 | BAM4 x family 6 | S3, S6 | up to 13 | as above, against the VMU-Off CD music module |
| C3 | NC2-US x family 4 | S10, S14 | 5 | NC2 US |
| C4 | NC2-PAL x family 3 | S8, S13 | 7 | NC2 PAL |
| C5 | WWP x family 2 (the largest pair) | S4, S5, F6 (new pool plan) | 19 | Railroad Tycoon II US |
| C6 | Remaining pairs, one per change | their CD music and VMU steps | up to 70 in all | one per pair |

## Research before any build

| ID | Question | Depends on |
|---|---|---|
| R1 | CD music without DirectSound (`MANATEE.DRV`: Resident Evil 2, Biohazard 2 VP, Hundred Swords, Undercover AD2025 Kei; 9 images) | D2 (their dumps show the sound driver's state) |
| R2 | In-game disc swaps (6 multi-disc images) | D2, G0 |
| R3 | PAL and 50 Hz (38 PAL images, all CHD) | B2, D1 on PAL images |

## Critical paths

- **First new VMU game (Wild Metal):** D1 → D2 → G0 → F1 + F2 + F3 → S1.
  Every later VMU step reuses S1's route, cards and layout.
- **Second combined game (Wild Metal):** S1 and S11 → C1. S11 needs D2 to
  show whether Wild Metal attaches DirectSound at all; if it does not, C1
  moves to BAM4 x family 6 (S3 + S6 → C2).
- **The largest pair (WWP x family 2, 19 images):** G0 → S4, and S1 → S5,
  then F6's pool plan → C5.
- **Nightmare Creatures II:** D1 boots NC2 US on the 96-slot reader (B0);
  then F4 → S9 → S10 (VMU) and F4 + D2 → S14 (CD music) → C3. NC2 PAL needs
  B2 first.

## What the data session answers

| Question | Where it comes from | Feeds |
|---|---|---|
| Does each CD-driver line boot to a game on the generic reader? | stage report + end-of-game record per launch | B0, the baseline of every later step |
| Does any game issue a command K-UI refuses? | refused-command counters; a refusal ends the launch with a dump | B1 |
| Which slot does each game poll and open (A1, A2), and does it save? | Maple frames by port, slot and command (block reads and writes) | S1, S5-S9, S16 |
| Which CD audio commands, track ranges and status polling does each game use? | CD audio log and command counters | S3, S4, S11, S13-S15 |
| Which process attaches DirectSound (`dsound.dll`, `sndcore.dll`)? | RAM dump: the kernel's module list and its per-process use bits | S3, S11, S15, R1 |
| Where does VMU traffic's driver live on the 17 kernels without a ROM `mapledev.dll`? | RAM dump: module list | S16, S17 |
| How much memory is left for a 352,768-byte CD buffer or a VMU pool? | RAM dump: pages in use, the kernel's heap bounds | S3-S4, S11-S15, F5 |

Limits: the session covers CD music Off on the background reader only. It
records no timings. Kernel-structure reads (modules, processes, heap) rely on
offsets reviewed for three kernel builds and matched by template on the other
three; free-page counters are unreviewed.
