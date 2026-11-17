# Windows CE: universal boot, CD music and virtual VMU (scope, 17 Nov 2026)

The question: is Wild Metal all that universal Windows CE support needs, with
CD music and the virtual VMU, and where does Nightmare Creatures fit?
Per-kernel and per-image data: [scope evidence](evidence/wince-universal-scope-2026-11-17.json).

**Scope.** The owner's library holds 127 Windows CE disc images (US 36, JPN 53, PAL 38). They are 124 releases on 94 distinct kernels, and every one of those kernels was available to the analysis. Everything below is static evidence from code, ROM metadata and scratch builds, unless a console result is named.

## 1. Direct answer

**No.** Wild Metal is the right next test, but it only reaches Armada's own driver family.

Wild Metal (US T42101N and PAL T40501D-50, one kernel `4b01a13620c9`) has Armada's kernel build, controller drivers and sound runtime, placed at different addresses. That is why it is cheap. It is also why it tells us little about the other games:

| | Shared with Armada | Rest of the library |
|---|---|---|
| Virtual VMU (`maple.dll` family) | family 7: 4 kernels, 6 images | 11 other families: 90 kernels, 121 images |
| CD music (DirectSound group) | 3 kernels, 4 images with CD music | 11 other DirectSound groups (8 with no adapter) and 5 kernels with no DirectSound: 75 images with CD music |

**What a Wild Metal pass would establish:**
- The accepted Armada VMU helper can move to another kernel by placement (same code at other addresses), with no new hook code.
- The first VMU-only Windows CE route (CD music Off, Virtual VMU On): admission by kernel, a GUI route, a per-game card, a VMU-only memory layout and a launch record. Every later family reuses these pieces.
- Family 7 opens up. Wild Metal US and PAL come first. Hello Kitty Lovely Fruit Park and Biohazard 2 Value Plus (2 discs) follow, each with its own kernel admission.

**What it cannot establish:**
- Anything for the other 11 VMU families. Their driver code differs. In families 1-6 the worker thread handles sit at +0x180/+0x184, and the +0x178 that Armada's helper uses is the DMA event in those families.
- Anything for the other DirectSound builds, or for the 5 kernels with no DirectSound at all.
- Several memory and routing cases:
  - the CE2 memory layout (RAM end 0x8cef0000: 25 kernels, 28 images);
  - 9 kernels with other RAM ends;
  - 20 kernels with no room for the bridge;
  - maps larger than 64 slots (Nightmare Creatures II);
  - games that open slot A1;
  - PAL and 50 Hz.

**CD music for Wild Metal is unproven.** Its kernel side is placement-only. But its program `wmc_d6.exe` imports no `DSOUND.DLL`, and no module in its ROM does, whereas `ARMADA.EXE` does. Its ROM also adds 13 modules that Armada lacks (DirectShow, MIDI, CEMM and others). The adapter needs a game process with DirectSound attached. A Wild Metal CD music test would answer that for Wild Metal and for 11 other DirectSound rows without a static import: KISS, Super Runabout, Sega Rally 2 JPN, Maximum Pool, Rune Jade and Daisessen.

**Even Wild Metal is not ready in source (verified):**
- `CE_FAMILY7_KERNEL=1` moves only the bridge and one header.
  - A scratch build of the helper still contains Armada's current-process/thread words (`0x8c145890` x4, `0x8c145894` x3) and its bridge return (`0x01dd67c6`), and none of Wild Metal's.
  - The stage's ROM placements and admission are still Armada's.
  - The helper would fail its checks and publish no card.
- The VMU-only build with the steady poll does not link: it overlaps the handoff by 768 bytes. Only the CD music + steady layout gets the larger image (`include/kui/armada_ce_vmu_layout.h:17-31`). Without the steady poll, the 14% hook cost that made Armada's intro a slideshow returns.
- The VMU-only recipe compiles its reader without the two console-proven reader fixes. They are set only in the CD music branch (`Makefile.armada-vmu-a2:94-111`).
- With CD music Off, the GUI reads no VMU setting for WinCE (`src/apps/games_retail.c:960-966`). It also writes no launch record carrying the module's CRC.
- `docs/wince-vmu-driver-families.md`, the comment in `Makefile.armada-vmu-a2` and the Wild Metal plan in `docs/handoff-2026-11-10.md` overstated the family 7 change. They are corrected with this record.

**No further input is needed for the analysis.** All 94 kernels were available (0 missing). Only a few disc DLLs are absent from the startup bundles: `DSOUND.DLL` for Next Tetris Online US, The Next Tetris PAL and Kitae White Illumination, plus 15 `SNDCORE.DLL` copies.

**What the whole class needs instead:**
- 12 VMU driver families (hook code exists for 3).
- 12 DirectSound groups with CD music (adapters exist for 4), plus one sound driver that does not use DirectSound.
- 15 pairings of CD music with VMU (1 built).
- Shared work on routing, memory layout and map size (sections 2.2-2.4).

In practice that means one test game per family and per group (section 4), not one title.

## 2. The three layers

### 2.1 Boot with the generic reader (`ce-boot.kui`)

**What exists:**
- One module for every WinCE game with CD music Off. The installed build `7b216f2e1136` has a 96-slot map (the `KUICEM01` record).
- The repository default is 64 slots, and `tools/package.py` still stages the module as `ce-probe.kui`. A full package built from the repository would therefore refuse Nightmare Creatures II.

**Static result (verified):**
- All 94 kernels pass every check the stage applies before handoff (load plan, entry, interrupt table, IP flag, bootstrap jump).
- All CD drivers use the same 11 GD functions and 17 request commands, and SYSINTR 20/21 only.
- Command 26 appears in every driver and K-UI does not serve it; Armada and Worms never issued it.
- RAM end does not limit this reader.

**Console results:** on the generic route, only Armada US and Worms Armageddon US. Bust-A-Move 4 US and Worms World Party US ran only through their CD music modules.

**Units of work: one module and six CD-driver lines.** The six `wsegacd.dll` builds line up with the VMU families:

| CD-driver line | VMU families | Kernels / images | Console-proven member |
|---|---|---|---|
| 1 | 1, 2, 3 | 39 / 58 | WWP US, through its CD music module only |
| 2 | 5, 6, 7 | 20 / 30 | Armada (generic and CD music); BAM4 (CD music) |
| 3 | 4 | 10 / 11 | none: NC2 US, KISS, GAH 1 and 2, Rainbow Six US ... |
| 4 | 9, 11 | 14 / 16 | Worms Armageddon (generic and CD music) |
| 5 | 12 | 1 / 1 | none: Sega Rally 2 JPN |
| 6 | 8, 10 | 10 / 11 | none: Kitahei Gold, Super Producers, Morita, WebTV ... |

**What is missing.** No code change is known to be needed for boot. The gaps are tests, formats and a few title-specific limits:
- **Console tests.** Line 1 has no result on the generic route. Lines 3, 5 and 6 (23 images) have no result at all.
- **Nightmare Creatures II.** It has 89/90 tracks, so it needs the 96-slot module. Its three data-track files must also fit in at most 7 card fragments (US) or 6 (PAL). Every other title has 39 tracks or fewer.
- **CHD images.** 43 images are CHD: all 38 PAL and 5 JPN. 19 kernels exist only as CHD. K-UI needs them imported to GDI on a PC (`docs/games-formats.md`).
- **Disc swaps.** In-game swaps for Resident Evil 2 and Biohazard 2 Value Plus are unsupported (inferred).
- **Silent music.** With CD music Off, PLAY is acknowledged silently. Whether music-heavy games tolerate that is untested.

### 2.2 CD music adapters

**What exists:**
- Four exact-kernel modules: `ce-cdda-armada`, `-bam4`, `-worms` and `-wwp`. Each build carries exactly one profile (`include/kui/wince_cdda_profiles.h`).
- The GUI offers CD music only for a GDI on SCI microSD, and picks the module by exact kernel SHA.
- Console results:
  - Armada: accepted, also with VMU.
  - Bust-A-Move 4: "worked perfectly".
  - Worms Armageddon: "runs fantastic".
  - Worms World Party: a scoped success.
- The PAL Bust-A-Move 4, Worms Armageddon and Worms World Party discs carry the same kernels, but they are CHDs and untested.

**What decides sharing (verified).** The adapter binds the ROM sound runtime and the kernel, not the game: every stream build runs in generic-application mode (`src/loader/armada_ce_excerpt_entry.c:3-6`, `include/kui/armada_ce_live_sound.h`). So the unit of work is a DirectSound group, defined by its `dsound.dll` build and `nk.exe` build. Differences in `sndcore` and `coredll` become compiled constants inside a group.

**Units for the 79 images with CD music.** Here, CD music means a track of 30 s or more. These images make 76 releases and 43 games on 60 kernels. There are 12 DirectSound groups (4 with an adapter, 8 without) plus 1 class with no DirectSound.

| Unit | Status | Images with CD music (GDI) | Titles |
|---|---|---|---|
| 4 profiled kernels | adapters exist | 7 (4) | Armada; BAM4, Worms Armageddon, WWP (US and PAL) |
| BAM4 group, other kernels | placement only | 12 (9) | Puzzle Bobble 4, Caesars Palace 2000 x2, Get Colony, Marionette Company 2, Virtua Cop 2 JPN, Super Runabout x6 (5 load DirectSound from disc) |
| WWP group, other kernels | placement only | 17 (8) | 4x4 Evolution, Championship Surfer, Ducati World, Railroad Tycoon II, Next Tetris Online, Net de Para, Hidden and Dangerous PAL, Starlancer PAL; other RAM end: Q*bert, Tomb Raider Chronicles, Rainbow Six PAL x3 |
| Worms group, other kernels | placement only | 4 (2) | Sega Rally 2 US and PAL x2, Space Griffon VF-9 |
| Armada group, other kernels | placement only | 3 (2) | Wild Metal US/PAL, Hello Kitty Lovely Fruit Park |
| NC2-PAL group | new; WWP's code with a moved vtable | 7 (3) | NC2 PAL, Urban Chaos x2, Spirit of Speed 1937 x3, Taxi 2 |
| NC2-US group | new contract | 5 (5) | NC2 US, GAH 2, NFL QBC 2001, HK Waku Waku Cookies, Rainbow Six US (other RAM end) |
| Giant Killers group | new contract | 5 (2) | Bang! Gunship Elite, Miss Moonlight, Giant Killers x2, Next Tetris PAL |
| WebTV x CE2-B | new contract, unreviewed kernel build | 3 (3) | Kitahei Gold, Eisei Meijin III, Super Producers |
| KISS | new contract, RAM end 0x8cff0000 | 2 (1) | KISS Psycho Circus US/PAL |
| Sno-Cross | new; WWP's code with moved vtables | 2 (1) | Sno-Cross US/PAL |
| QBC 2000 | new; close to Worms' | 2 (1) | NFL Quarterback Club 2000 US/PAL |
| Sega Rally 2 JPN | new contract, unreviewed kernel build | 1 (1) | Sega Rally 2 JPN |
| No DirectSound (`MANATEE.DRV`) | no adapter type exists | 9 (7) | Resident Evil 2 US/PAL (4 discs), Biohazard 2 Value Plus (2), Hundred Swords x2, Undercover AD2025 Kei |

Two of the 8 new groups, NC2-PAL and Sno-Cross, look like constant-level variants of WWP's contract. A stricter grouping, which also splits on `sndcore`/`coredll` class, gives 13 compiled groups.

**What is missing:**
- **Existing groups.** 27 more kernels with CD music (36 images) belong to the four existing groups.
  - Their kernel-side constants follow by placement.
  - But `wince_cdda_profiles.h:16-17` forbids inheriting addresses by a partial match. Each kernel therefore needs an exact-SHA admission, a GUI route and a console result.
  - Today that means one module per kernel. One module per group carrying several exact-SHA records would be a new mechanism.
- **New work.** Eight reviewed sound contracts (27 images) and a design for `MANATEE.DRV` (9 images).
- **Memory.** Where RAM does not end at 0x8cfd0000 or 0x8cef0000, the 512 KiB pool needs a new placement: Q*bert, Tomb Raider Chronicles, Rainbow Six US/PAL and KISS (6 kernels, 8 images).
- **Map capacity.** The CD music readers and their stage admission hold 64 slots (`Makefile.dc:427-429`, `include/kui/wince_cdda_profiles.h:146-150`). NC2 has 89/90 tracks.
- **Unproven for each game:**
  - DirectSound attached in exactly one process before the first PLAY;
  - CD playback through WSEGACD;
  - drive-status polling (WWP needed a projection);
  - heap room for the extra 352,768-byte buffer.

### 2.3 Virtual VMU

**What exists:**
- Armada only: `ce-armada-all-on.kui` `cb07c6454454`, CD music + VMU, accepted on 9 Nov 2026. No VMU-only WinCE module exists.
- Hook code for two more families, inside the Smash Pack route only. That route was accepted as a collection (`cc81e63ca4d2`), but no Swirl virtual-card save is recorded.
  - Family 4's driver is Sega Swirl's: all 13 + 3 reviewed windows match in all 10 kernels.
  - Family 5's driver is Virtua Cop 2's.

**Units: 12 `maple.dll` families.** Inside each family the code is equal apart from relocation (verified). Hook code exists for 3 families (4, 5 and 7: 18 images). The other 9 need new profiles (109 images).

| Fam. | Kernels / images | Hook code | Ready* | Also needs | Titles |
|---|---|---|---|---|---|
| 1 | 12 / 16 | new, close to 2 | 12 | 4 relocations | Bang! Gunship Elite, Giant Killers, Next Tetris PAL, 12 JPN (Evangelion Typing x2, Happy Lesson, Princess Maker, Miss Moonlight, Yukigatari, Yoshia no Oka, Fragrance Tale, Kaitou Apricot / Cherry Blossom / For Symphony / Sweet Season) |
| 2 | 22 / 35 | new; static audit done | 17 | 6 relocations (WWP, 4x4, Starlancer, Hundred Swords x2, Net de Para); other RAM end: Q*bert, Rainbow Six PAL, TR Chronicles | WWP, Railroad Tycoon II, Championship Surfer, Ducati, Hidden and Dangerous, Hoyle, Atari Anniversary, Millionaire, Next Tetris Online, Sno-Cross ... (19 games) |
| 3 | 5 / 7 | new, close to 2 | 7 | none | Urban Chaos, Spirit of Speed 1937, Taxi 2, NC2 PAL |
| 4 | 10 / 11 | Swirl's (Smash only) | 6 | Maximum Pool relocation; other RAM end: KISS x2, Rainbow Six US, SRW Alpha | NC2 US, GAH 1/2, NFL QBC 2001, HK Waku Waku Cookies ... |
| 5 | 1 / 1 | VC2's (Smash only) | 0 | relocation | Virtua Cop 2 JPN |
| 6 | 15 / 23 | new (hybrid worker) | 17 | 2 relocations; other RAM end: Daisessen, TR Last Revelation/IV | BAM4, Puzzle Bobble 4, Caesars Palace, RE2 US/PAL, Super Runabout x6, Get Colony, Rune Jade ... |
| 7 | 4 / 6 | Armada's | 6 | placement records only | Armada, Wild Metal x2, Biohazard 2 VP x2, HK Lovely Fruit Park |
| 8 | 2 / 2 | new, CE2 | 2 | CE2 layout | Kitahei Gold, World Neverland Plus |
| 9 | 1 / 1 | new, CE2 | 0 | relocation, CE2 layout | Sega Rally 2 US |
| 10 | 8 / 9 | new, CE2 | 9 | CE2 layout | Morita x3, Eisei Meijin III, Super Producers, Kitae, Troia, WebTV x2 |
| 11 | 13 / 15 | new, CE2 | 11 | 4 relocations, CE2 layout | Worms Armageddon, QBC 2000, South Park, Sega Rally 2 PAL, Densha de Go 2, Kitty GP, Space Griffon ... |
| 12 | 1 / 1 | new, distinct; worker not identified | 0 | relocation (76-byte tail) | Sega Rally 2 JPN |

\* Ready means the `maple.dll` tail is free for the bridge and the kernel has its family's usual RAM end: 87 images on 65 kernels.

**What is missing (shared work):**
1. **GUI:**
   - a VMU-only route;
   - per-game cards (Armada uses the dedicated test card `KUIARMCD`);
   - a launch record giving the module path, build and CRC for CD-music-Off launches;
   - a complete extent map for card preparation (`src/apps/games_vmu_card.inc:161-176`).
2. **VMU-only layout:** a steady-poll layout that fits, and the reader fixes in the VMU-only recipe.
3. **Placement records:** a record per kernel instead of compile-time Armada constants. It covers stage ROM placements, the helper's owner profile and the interrupt words.
4. **Smash helpers:** the Swirl and VC2 helpers moved onto the standalone CE stage.
5. **Relocated driver code:** relocated `maple.dll` text (32 KiB, as Swirl's helper does) for 20 kernels (22 images).
6. **RAM end:** a layout measured from RAM end, for CE2 (25 kernels, 28 images) and for the 9 kernels with other RAM ends (18 images).
7. **Per title:** which slot the game opens (A1 or A2; fixed per module at build time, `include/kui/ce_vmu_target.h`), and whether the game saves at all.

### 2.4 CD music and VMU in one module (the owner's goal)

Games with both CD music and DirectSound fall into 15 pairs of sound group and VMU family, covering 70 images. Armada's pair is the only one built so far. The largest pairs:

| Pair | Images |
| --- | --- |
| WWP x family 2 | 19 |
| BAM4 x family 6 | 13 |
| NC2-PAL x family 3 | 7 |
| Worms x family 11 | 5 |
| NC2-US x family 4 | 5 |
| Giant Killers x family 1 | 5 |
| Armada x family 7 (Armada, Wild Metal x2, HK Lovely Fruit Park) | 4 |

Memory is the constraint. The accepted combined layout leaves about 9 KiB free in its 512 KiB pool. A kernel whose `maple.dll` tail is occupied, WWP's included, needs 32 KiB for relocated text, so it needs a new pool plan.

## 3. Nightmare Creatures

- **The library has Nightmare Creatures II, US and PAL.** The first game never came out on Dreamcast, and the library has no other release by that name.
- **It is a Windows CE game with CD audio.** Its 85-86 audio tracks are short music cues of 6-32 s each, about 21 minutes in total, not full songs. Only two tracks per disc reach 30 s.
- **Other Nightmare titles:**
  - KISS Psycho Circus: The Nightmare Child is WinCE (VMU family 4, its own DirectSound group, 29 audio tracks).
  - Alone in the Dark: The New Nightmare and Plasma Sword: Nightmare of Bilstein are native, not WinCE.

| | US T9504M V1.000 | PAL T-9502D-50 V1.004 |
|---|---|---|
| Image | GDI (a CHD copy also exists) | CHD only: needs a per-track GDI import |
| Kernel | `5908b5d30c5f`, 2,424,832 B | `1955aeed9d30`, 2,437,120 B |
| Program | `nc2_wce.exe` in the kernel ROM; imports DirectSound | same |
| Tracks | 89: 3 data, 86 audio (85 high-density) | 90: 3 data, 87 audio (86 high-density) |
| RAM end | 0x8cfd0000, 0x30000 driver globals | same |
| CD-driver line | 3 (no proven member) | 1 (WWP's) |
| VMU family | 4: Swirl's driver, 412-byte free tail | 3: Urban Chaos family, not audited, free tail |
| CD music group | NC2-US: new DirectSound build (buffer methods -0x1c, singleton probably 0x810c); `sndcore` 0x3da0; WSEGACD as KISS | NC2-PAL: WWP's DirectSound code with the device vtable moved (0x117c, not 0x112c); `sndcore` 0x3da0; WSEGACD as WWP |
| Kernel build | WWP's, data +0x90000 | WWP's, data +0x94000 |
| Group mates with CD music | GAH 2, NFL QBC 2001, Rainbow Six US, HK Waku Waku Cookies | Urban Chaos x2, Spirit of Speed 1937 x3, Taxi 2 |
| CD music profile / console result | none / none | none / none |

**What it needs, layer by layer:**
1. **Boot with CD music and VMU Off.** The US disc should work now but is untested.
   - It needs the installed 96-slot `ce-boot.kui`, with the three data-track files in at most 7 card fragments. A 64-slot module refuses the disc.
   - The music will be silent.
   - This launch is the completion check for row 1 of `docs/universal-loader-roadmap.md`.
   - The PAL disc must be imported to GDI first, and its data files must fit in at most 6 fragments.
2. **VMU.**
   - US: Swirl's helper on the standalone CE stage, the NC2 placements, a check of which slot the game opens, and a map larger than 64 slots in the VMU module. In a scratch build the VMU-only reader has about 288 bytes spare; 96 slots need 384.
   - PAL: a new family 3 profile and the same map change.
3. **CD music.**
   - PAL: WWP's contract with the moved vtable and new `sndcore` constants. This is the closest to existing work.
   - US: a new reviewed contract.
   - Both: the map change in the CD music reader and stage. Both admit at most 64 tracks today. The complete audio map would need about 178 slots (US) or 180 (PAL), which fits the GUI's 256-slot map (inferred).
   - Many short cues stress track changes and restarts.
4. **CD music and VMU together:** the NC2-US x family 4 pair (US) or the NC2-PAL x family 3 pair (PAL).

**Where it falls in the roadmap below:** US boot at step 0, US VMU at step 10, PAL CD music at step 13, US CD music at step 14.

## 4. Roadmap to every WinCE game with CD music and VMU

**Ground rules (from `AGENTS.md` and the records):**
- **Pending reports first.** Stack nothing new until GUI `069d612cadea`'s four console tests and the pending `7b216f2e1136` reports are in. The latter cover BAM4, WWP and both Armada modules. The handoff already schedules Wild Metal after the GUI test.
- **Native checkpoint.** Leave the native `f8807cb16561` checkpoint untouched.
- **One change at a time.** Each later change names its baseline (GUI, module build and CRC) and is one behavior change with its own console comparison.
- **Accepted bytes.** These stay identical: the Armada modules, `ce-cdda-*`, `ce-boot` `7b216f2e1136` and the Smash Pack set.
- **Delivery.** Use manual-copy ZIPs and test by cold ROOT boot.
- **What each test records:**
  - the GUI, plus the module path, build and CRC;
  - K-UI-to-bootstrap and bootstrap-to-game times, separately;
  - first and repeat launches;
  - for VMU steps: save, power off, cold boot, load.
- **Separate requirements.** VMU-only and combined modules are separate requirements.
- **Repository roadmap.** VMU work falls under row 4 of the repository roadmap and CD music under row 5.

**Ordering.** Steps are ordered by titles unlocked per unit of work, and the unit sizes are estimates (inferred). There are two exceptions, both dependencies:
- Step 1 builds the shared VMU-only pieces.
- Step 12 is the cheapest second game with both CD music and VMU.

| # | One behavior change | Unlocks (images) | Console test |
|---|---|---|---|
| 0 | None: observe installed software | boot evidence for lines 1, 3, 5, 6 | GUI `069d612cadea`'s four tests (NC2 US can be its CD-music-Off game). On `ce-boot` with CD music and VMU Off: NC2 US (line 3, 96 slots), Railroad Tycoon II US (line 1), Kitahei Gold or Super Producers (line 6), Sega Rally 2 JPN (line 5), and Wild Metal US as the baseline for step 1 |
| **Tier 1: existing code moved by placement** | | | |
| 1 | Family 7 VMU-only route for Wild Metal's kernel | 2 | Wild Metal US, VMU On against step 0: save, power cycle, load; intro and FMV; which slot it opens |
| 2 | Admit HK Lovely Fruit Park, then Biohazard 2 VP (one kernel per change) | 3 | each: save, power cycle, load; boot each BH2VP disc separately |
| 3 | BAM4 adapter for its 10 other kernels | 12 | Caesars Palace 2000 US; Super Runabout US (DirectSound from disc, no static import); Virtua Cop 2 JPN (DirectSound through a DLL) |
| 4 | WWP adapter for its 8 other kernels with the usual RAM end | 12 | Railroad Tycoon II US, Ducati World US, Net de Para JPN (new `sndcore` constants) |
| **Tier 2: new hook profiles and the Smash helpers on CE3 kernels** | | | |
| 5 | Family 2 profile | 17 | Railroad Tycoon II US, Championship Surfer US |
| 6 | Family 6 profile | 17 | BAM4 US, Caesars Palace 2000 US |
| 7 | Family 1 profile | 12 | Bang! Gunship Elite US |
| 8 | Family 3 profile | 7 | Urban Chaos US, Spirit of Speed 1937 US |
| 9 | Swirl's helper on the standalone stage (family 4) | 5 (NC2 US after step 10) | GAH 2 US or NFL QBC 2001 US; check the slot (Swirl's program opens A1) |
| 10 | WinCE map larger than 64 slots in the VMU and CD music modules | NC2 US/PAL (2) | NC2 US with VMU On |
| **Tier 3: sound contracts and the first combined modules** | | | |
| 11 | Worms adapter, then Armada adapter, for their other kernels (two changes) | 4 + 3 | Sega Rally 2 US; Wild Metal US (first question: does a DirectSound owner appear?) |
| 12 | Combined modules, one pair per change: Wild Metal (Armada's layout), then BAM4 x family 6 | 3 + up to 13 | as for Armada's acceptance: intro, music, save, power cycle, load, against the VMU-Off CD music module |
| 13 | NC2-PAL contract, then Sno-Cross (both WWP variants) | 7 + 2 | Spirit of Speed 1937 US, Urban Chaos US; NC2 PAL after GDI import |
| 14 | NC2-US contract | 5 | GAH 2 US, NC2 US |
| 15 | Giant Killers, QBC 2000, WebTV/CE2-B, KISS and Sega Rally 2 JPN contracts (one per change) | 5 + 2 + 3 + 2 + 1 | one representative each |
| **Tier 4: memory** | | | |
| 16 | CE2 VMU layout with the family 11 profile, then families 10 and 8 | 11 + 9 + 2 | Worms Armageddon US or QBC 2000 US; Super Producers |
| 17 | Relocated-text bridge on the standalone stage (as Swirl's) | 22 | Maximum Pool US, Starlancer US, Virtua Cop 2 JPN; then families 9 and 12 |
| 18 | Other RAM ends, one kernel per change: KISS, Rainbow Six (3.75 MiB of CE RAM), Q*bert, TR Chronicles, SRW Alpha, Daisessen, TR Last Revelation | VMU 18, CD music 8 | per kernel |
| 19 | Remaining combined pairs; WWP x family 2 needs a new pool plan first | up to 70 in all | one pair per change |
| **Tier 5: research before any build** | | | |
| 20 | CD music without DirectSound (`MANATEE.DRV`); disc swaps; PAL and 50 Hz | 9; 6 multi-disc images; 38 PAL | none until a design exists |

**A compact test set, one game per unit (all GDI).** Whether each game saves to the VMU still has to be checked.

| VMU family | Test game |
| --- | --- |
| 1 | Bang! Gunship Elite |
| 2 | Railroad Tycoon II |
| 3 | Urban Chaos |
| 4 | GAH 2 or NC2 US |
| 5 | Virtua Cop 2 JPN |
| 6 | Bust-A-Move 4 |
| 7 | Wild Metal |
| 8 | Kitahei Gold |
| 9 | Sega Rally 2 US |
| 10 | Super Producers |
| 11 | Worms Armageddon or QBC 2000 |
| 12 | Sega Rally 2 JPN |

| DirectSound group | Test game |
| --- | --- |
| BAM4 | Caesars Palace 2000 |
| WWP | Railroad Tycoon II |
| Worms | Sega Rally 2 US |
| Armada | Wild Metal |
| NC2-PAL | Spirit of Speed 1937 |
| NC2-US | GAH 2 or NC2 US |
| Giant Killers | Bang! Gunship Elite |
| WebTV | Kitahei Gold |
| KISS | KISS US |
| Sno-Cross | Sno-Cross US |
| QBC 2000 | QBC 2000 US |
| Sega Rally 2 JPN | Sega Rally 2 JPN |

## 5. Risks and unknowns

**Application side (unproven for every new title):**
- **DirectSound attachment.** 18 survey rows carry only the DirectSound GUIDs, with no static import. 12 of them are on kernels that have DirectSound: Wild Metal, Super Runabout x5, KISS x2, Maximum Pool, Sega Rally 2 JPN, Rune Jade and Daisessen. Whether they attach DirectSound is unknown.
- **Playback and polling.** Whether each game plays its tracks through WSEGACD, and how it polls drive status (WWP needed a projection).
- **Heap room** for the extra 352,768-byte buffer.
- **Card slot.** Which slot each game opens. Berserk ignored a virtual A2 card on the native route.
- **Saving.** Which games save at all. South Park imports no `MAPLEDEV`.

**Unreviewed code:**
- 8 DirectSound builds and 3 kernel builds (CE2-B, CE2-C, CE3-C) are compared only by alignment.
- In families 8-12 the extension layout is not decoded. Family 12's worker has not been found.
- 17 kernels have no ROM `mapledev.dll` (16 ship one on disc). Where their VMU traffic comes from is not established.
- The infinite worker wait in families 1-6 and 8-11 has not been reviewed against the fence design.
- The identifications family 4 = Swirl and family 5 = VC2 rest on the reviewed windows only; the Smash kernels are not in the local set.

**Memory:**
- Kernels with other RAM ends leave 1-10.8 MiB that CE does not manage, and Rainbow Six has only 3.75 MiB of CE RAM.
- The check that the stage ends below the reservation fails when the reservation sits below the stage.
- The combined pool has about 9 KiB free.
- The CD music readers have 64-96 bytes spare for a larger map.

**Peripherals:**
- A physical device in the virtual slot blocks the card for the whole session; a jump pack in slot 2 blocks virtual A2.
- A keyboard or mouse in port A blocks the card (Evangelion Typing, WebTV).
- The virtual card is storage only.
- No WinCE peripheral test exists.

**Formats and regions:**
- No PAL WinCE disc has a console result. PAL and 50 Hz behaviour is unanalysed; Maple DMA runs on vblank.
- The CD music route needs every track in its own file at offset 0, so a CHD import must produce a per-track GDI.
- For 3 games the disc copy of DirectSound differs from the ROM copy, and which one loads is not established: Atari Anniversary, Caesars Palace PAL and Marionette Company 2.
- Biohazard 2 Value Plus labels its discs 1/3 and 2/3. Whether a third disc exists is unknown.
- Command 26 is unserved.

**Evidence limits:**
- Console results exist only for Armada, BAM4, Worms Armageddon and WWP, all US GDIs.
- The figure of 78 distinct games is an inventory title grouping.
- The similarity percentages between families are heuristic.

**Stale or overstated records:**
- `docs/universal-library-coverage.md` said NC2 depends on `ce-probe.kui`. GUIs `00d8ed67d9ad` and `069d612cadea` route it to `ce-boot.kui`. Corrected with this record.
- `docs/wince-vmu-driver-families.md` mixes two family numberings. Its family 7 paragraph, the comment in `Makefile.armada-vmu-a2` and the Wild Metal plan in `docs/handoff-2026-11-10.md` overstated the family 7 change; they are corrected with this record.
- The library tables' Windows CE reader label is a placeholder, not an admission result.

**Sources:**
- Repository:
  - `AGENTS.md`
  - `docs/universal-loader-roadmap.md`, `docs/accepted-progress.md`, `docs/wince-gui-routing.md`, `docs/handoff-2026-11-10.md`, `docs/wince-vmu-driver-families.md`
  - `docs/evidence/wince-controller-driver-survey-world-2026-11-11.json`, `docs/evidence/wince-family7-profiles-2026-11-17.json`, `docs/evidence/wince-maple-family2-audit-2026-11-17.json`
  - `include/kui/wince_cdda_profiles.h`, `include/kui/armada_ce_vmu_layout.h`, `include/kui/ce_maple_family7.h`, `include/kui/ce_maple_owner_profile.h`, `include/kui/ce_vmu_target.h`, `include/kui/retail_image.h`, `include/kui/retail_loader_layout.h`
  - `src/apps/games_retail.c`, `src/apps/games_vmu_card.inc`
  - `Makefile.dc`, `Makefile.armada-vmu-a2`, `Makefile.armada-cdda-vmu`
- Evidence: [scope evidence](evidence/wince-universal-scope-2026-11-17.json), joined from the run's
  metadata-only outputs (inventory, sound groups, CD-driver lines, track lists). The scratch builds
  behind "does not link" were made in a copy of this tree with `Makefile.armada-vmu-a2`
  (`ARMADA_A2_WRITE_LAYOUT=1 ARMADA_A2_WRITE_STAGING=1 ARMADA_A2_WRITE_RUNTIME=1 ARMADA_A2_STEADY=1
  CE_FAMILY7_KERNEL=1`, no CD music).
