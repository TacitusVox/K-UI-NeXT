# Universal loader integration roadmap

Current delivered candidate is `9e3295bcfaec`. It includes the twelve native
cold families from `f8807cb16561` and the shared extent/ownership corrections
in five ordinary virtual-VMU readers. The accepted Smash collection has its
own qualified component set. The latest owner report suggests another two to
three seconds saved, with COLD 03 revised to seven to eight seconds from
approximately 21. See [the acceptance ledger](accepted-progress.md) and
[current library coverage](universal-library-coverage.md).

## Current owner-library sequence, November 6, 2026

The updated manifests identify 42 game products after excluding the owner's
incomplete Soul Calibur. There are 43 usable startup identities because
Shenmue Passport is a separate startup. Raw/cooked copies and duplicate disc
executables share evidence only when their exact bytes and runtime agree.
The new pair of version 2 bundles and the later version 2.1 supplement supply
Power Stone, the five WinCE runtimes, native audio drivers, Shenmue, Shenmue II,
and secondary Quake III/Le Mans executables. These extend the available
analysis inputs. Console playback and save tests remain separate.

| Order | Deliverable | Completion check |
| --- | --- | --- |
| 1 | Give ordinary WinCE its own installed reader and preserve the exact compatible Armada virtual-VMU route. The current ordinary path shares a filename with that adapter. | Final GUI and ZIP select the intended reader with the inherited module present, absent or damaged. Nightmare Creatures II and Armada reach gameplay using the selected route. |
| 2 | Close bounded native family gaps, beginning with Blue Stinger's initializer and Power Stone's Maple buffers, now using the supplied originals. | Original installer/driver execution, refusal controls and actual package admission pass. One changed family representative boots, loads a demanding area, and handles its selected physical or virtual card on console. |
| 3 | Prove whole-game executable lifetime through secondary loads. Quake III and Test Drive Le Mans now include the main and movie executables. Investigate the distinct Shenmue, Passport and Shenmue II runtime families. | Reader and VMU hooks survive the real intro-to-game transition and later executable loads. Allocations and firmware ownership remain valid across each transfer. |
| 4 | Broaden virtual saving and independent service modes. Keep separate accounting for neither service, VMU only, New/read-ahead only, and combined operation. | The selected modes retain the proved memory, physical cards work, and a game save survives power-off and reload. DOA2's working standalone route and the exact accepted Smash set provide regression checkpoints. |
| 5 | Extend native CD music beyond Toy/MDK2 and improve native/WinCE streaming. Use supplied audio drivers and CE applications to identify shared runtime contracts. | Continuous music, concurrent loading, track changes and the same FMV scene improve on console, with responsive play and durable saves. Each admitted runtime family gets an explicit result. |
| 6 | Close the 42-game coverage ledger, including multi-disc transitions and repeated launches. Then broaden the relevant physical-disc, compressed-format and transport combinations. | Every complete inventoried game has a recorded launch, actual play, loading and available save outcome on its chosen image and reader. Multi-disc games cross a real requested disc boundary with the correct card. |

Windows CE coverage for rows 1, 4 and 5, by `maple.dll` family, sound group
and CD-driver line, with one test game per unit:
[WinCE scope](wince-universal-scope.md). Its work in dependency order, and
the bulk data session that feeds it: [WinCE roadmap](wince-roadmap.md),
[data probe](wince-data-probe.md).

Each source change answers one compatibility or performance question. A small
regression set covers the affected runtime family and its memory contract.
The full library ledger accumulates results as games are played. Record the
actual reader path/build and image variant alongside the GUI identity.
Deliver manual-copy ZIPs and validate the actual final package through cold
CD/ROOT boot.

Further bootstrap work can target the cold GUI's repeated hashes and a fresh
ownership transfer after the last GUI writer. The latter needs a recovery and
invalidation contract across resets and card changes. Keep each timing
comparison isolated. First finish the missing ordinary WinCE deployment and
the newly actionable runtime coverage above.

Goal: one maintained K-UI build which selects the best verified launch backend,
virtual VMU and optional read-ahead for each runtime. Keep successful games
working while extending shared support. A runtime may use demand delivery
without a speculative cache. Physical VMUs remain available where virtual
VMU is not yet supported.

Current scope is **Games only**. Startup/splash, menus, updater implementation
and branding are frozen. Their older follow-up notes below are deferred and
do not authorize work during this integration. Prioritize shared launch work,
complete routed-module coverage and proof of the final reader actually used.
The current operation-level corrections and the remaining allocation-proof
cost are recorded in `shared-game-launch-performance.md`.

The separately authorized `f7229ee` splash repair restores the original RAM
artwork immediately after CD handoff. It is a narrow display exception, not a
Games reader change or evidence of a gameplay/performance improvement.

This work starts at Main `d393b859a9ee23d372daca54ef13c7839072097a` on
2026-11-06, after acceptance of Smash Pack source `cc81e63ca4d2`.
The initial implementation branch is `codex/universal-loader-integration`.
The milestones below are ordered dependencies, not elapsed-time estimates.

The canonical repository is `TacitusVox/K-UI-NeXT`. The account name is
TacitusVox; older TacitusVoxt and TPMJB links are migration work, not alternate
current publishing destinations.

## Owner's direction, November 16, 2026

After the console result of the Toy Commander card stream reader
(`toy-wire-stream-test-77.md`) the owner restated the goals:

> So the eventual goal is
> - Universal VMU
> - Universal reader (or at least a few for the different classes of games -
>   CDDA, WinCE + CDDA, WinCE, and regular DC)
> - Run-ahead or prefetch where possible/needed. Goal is smooth FMVs and very
>   smooth gameplay.
>
> Since the FMV is basically a non-issue at this point, I want those other
> three goals ironed out.

The remaining frame loss in Toy Commander's second movie is accepted as it is
("I don't know that chasing down those 9 frames is value-added"). The stream
reader is a candidate for the read-ahead of the third goal, to be compared
with the installed prefetch in the two Evolution games first. It has run with
CD music and virtual VMU off only. See `fmv-stream-reader-plan.md`.

Built for that comparison and run by the owner on the same day: Test 78
(`toy-wire-stream-test-78.md`, a window serves a request of any size) and
Test 79 (`toy-wire-stream-test-79.md`, build `9f183a7f72f5`, the window for every
title of the family on a raw SCI image, virtual VMU off). Test 79 is a test
module put in the installed New reader's place for the runs and taken out
again; the delivered `f8807cb16561` candidate is not changed. Both Evolution
games ran on it. The window read ahead for Evolution; in Evolution 2 a command
that is not a read gave up nearly every window, and in both the stream had no
timer. Those two are the next tests, each one change, before the virtual VMU
beside the stream. The games were not compared with the installed module on
the same day.

Built and delivered for that on 16 November, and run by the owner the same day
(results in the two documents: no window given up for a command in Evolution 2,
and the stream's waits down from 10 to 15 ms to under 0.2 ms in both games): Test 80
(`toy-wire-stream-test-80.md`, build `e58dd2e6b641`, a window stands through a
command that only asks the drive something) and Test 81
(`toy-wire-stream-test-81.md`, build `684f485644e5`, the stream's timer in every
title by the title's own admitted SDK table). Both are test modules for the
installed New reader's place, virtual VMU off; the delivered `f8807cb16561`
candidate is not changed.

Behind them, the same day, and run by the owner that afternoon ("everything
seems fine"; a save's survival of power off is not yet reported): Test 82
(`toy-wire-stream-test-82.md`, build `af5ebae4f0a5`), the virtual VMU beside the
stream reader. It is the installed New reader's worker, byte for byte with that
module's label, in the test module's place of a worker, with Test 81's low
residents and auxiliary unchanged; for every title but Toy Commander, virtual
card in slot A2, no report pages.

Found beside it, for this roadmap: built from the source that carries the Toy
test sources (imported by `f216fac` and `e1104f4`), the installed New reader's
own recipe gives the candidate's worker and not the candidate's low residents
and block-cache auxiliary. The delivered candidate stays what is installed, and its recipe
reproduces it at the candidate's own source alone, until that is resolved
(`fmv-stream-reader-plan.md`, owed list;
`evidence/toy-wire-stream-82/installed-recipe-on-this-source.log`).

## Owner's direction, November 20, 2026

> Go ahead with the survey build. I want to add as much compatibility as I
> can this week. Let's go in order of what I need to do. I'm having Opus 5.5
> work on WinCE compatibility as I think we're almost done there. I think I
> have you working on just CDDA games

Windows CE is another session's work (`wince-roadmap.md`). This sequence is the
regular games, with CD music first.

Built for it: Test 83 (`toy-wire-stream-test-83.md`), Test 81's stream reader
with its report saved to the card from a Games launch, into a text file the
owner copies onto the card blank. No photographs. And a run of the startup
bundler over the regular games with CD music only
(`tools/kui-library-startup-bundle-README.md`, `--cd-audio`), because the
repository holds only the hashes of their programs and sound drivers.

### CD music in regular games: what there is

Read from the sources on 20 November, by file:

- Two titles have CD music, Toy Commander and MDK2, by one adapter each on the
  Standard reader, with no virtual VMU and no read-ahead
  (`src/loader/toy_pilot_*`, `src/loader/mdk2_cdda_*`). The disc calls for
  play, pause, resume and stop go to a worker; the worker reads the audio
  track's raw sectors from the card, writes them into a ring of 131,072 bytes
  it takes from the game's own sound heap, and has the game's AUDIO64 driver
  play that ring on two of its ports, by packets in the driver's own queue. It
  runs from a hook on the game's sound update.
- What that costs a title: its executable's size, CRC-32 and SHA-256; 17 to
  20 addresses in it (the sound update, the driver's installation, heap and
  shutdown calls, the port picker); the game-heap words that put the worker at
  `0x8cfd0000`; and, for its driver build, the SHA-256 and five offsets
  (`include/kui/cdda_audio64_profile.h`, `include/kui/native_cdda_compat.h`).
  The list of titles is compiled into the menu: a new module file by itself
  adds none (`include/kui/native_cdda_compat.h:54`,
  `src/apps/games_retail.c:1494`).
- Both drivers are AUDIO64 builds. MDK2's serves 24 surveyed titles and Toy
  Commander's 3. Nothing in the sources knows MANATEE.DRV (143 distinct
  titles in 22 builds), MANATEE2.DRV or NAMIHEI.DRV
  (`library-cd-audio-us.md`, `evidence/world-cd-audio-survey-2026-11-11.json`).
- With CD music off, every reader answers play, pause and resume as done and
  plays nothing; the drive never reports playing, and the subcode position
  stands still (`src/core/retail_gd.c:288`, `:481`). The New reader's launch
  map lists the audio tracks, with their place on the card where its 64 slots
  hold them (`src/apps/games_retail.c:361`).
- Not known: which games ask for a track at all; what card time a sector of
  audio costs beside the game's reads; which sound channels and how much
  sound memory a game's driver leaves free.

### CD music in regular games: the order proposed

An adapter a title does not reach the library: each is a table of addresses
found by hand, and a change to the menu. What reaches it is the New reader
playing the tracks itself, for whatever driver the game has: the disc calls
and a play position in the low resident, the audio read by the card stream
(a real drive cannot read data while it plays, so a game that plays a track
leaves the card to it), and two sound channels and a ring of the loader's own.
Whether a game's driver leaves two channels and the memory alone is the open
question, and a console can answer it for many games in one round.

1. Test 83 on a few games: the report saved from a Games launch.
2. The owner's `--cd-audio` run: the programs and sound drivers.
3. A sound probe on Test 83's ground: at the report gesture, the sound
   chip's channel registers, its mixer and effect settings, which driver is
   loaded and which parts of sound memory are in use, saved with the report.
   One long round over the CD music games.
4. From 2 and 3: the first CD music in the New reader's place, as a test
   module, in the games the probe shows to be safe. No menu change is needed
   for a test module; the menu's own CD music setting is a later question.

None of this is built yet beyond steps 1 and 2's tools, and none of it is a
console result.

### The sound probe (step 3): what the sources give it

Read on 20 November for the probe's design; it is built after Test 83's
console round, because it saves through Test 83's file.

- A bounded way onto the sound bus exists and ran on the console in the two
  adapters: `src/loader/toy_pilot_bus.c`. It masks interrupts, refuses while
  exceptions are blocked or any of the bus's four DMA channels is enabled or
  started, waits for the bus FIFO to drain, reads a word twice until the two
  agree, and gives up after 2 ms. It admits sound memory only; the registers
  need their own list, as MDK2's probe has for seven of them.
- The record a probe would fill is there in part: `include/kui/retail_observe.h`
  holds the first and last play commands, and a mask of the channels ever
  keyed on. The code that sampled for it is gone from the resident
  (`observe_call` is called and defined nowhere), so that build no longer
  links.
- The reader keeps nothing of CD music today: play, pause and resume are
  answered and forgotten (`src/core/retail_gd.c`). In the Toy test module
  the auxiliary is visited from the game's disc calls, also from those that
  are no read (Tests 73 and 80), so the play commands may be countable
  there without a change to the low resident. Not yet read: whether every
  command reaches it, and with its parameters.
- The report is formatted and saved by the Toy worker
  (`toy_suite_save_report`, `toy_suite_report_write`): two sectors, each
  checked blank by its CRC-32 before the first write. More sectors a slot
  mean a new file version and a seed that names them.
- What to record, for one long round: the channels keyed on over the session
  and each channel's registers at the gesture; the mixer's CD input and
  master settings; checksums of the driver's first bytes, to tell which
  driver is loaded; and which 4 KiB pages of sound memory changed since
  launch, from checksums the stage takes before the game starts. Nothing is
  written to the sound chip or its memory.
- Not answerable by a probe: what card time a second of audio costs beside
  the game's own reads. That is the first sounding test's question.

### Test 84 (20 November 2026): the drive plays, and the sound chip is read

Step 3 as built (`cdda-background-test-84.md`, build `4abc63b0cc58`), on the
owner's Test 83 lines (GUI `4d4dcff77494`, reader `1d80f771f081`). Not a
probe at the gesture: two things the first sounding test needs anyway, in
the Background reader's Toy test module, one switch each.

- The drive plays. The worker's player answers the game's disc calls as a
  drive that plays its audio tracks: status 3 while playing, a position that
  moves at 75 sectors a second on TMU0, repeats, pause and resume, an end
  when the piece's time is up, and status 3 kept while the game reads data
  during a play. No sound.
- The sound chip is read, and never written, while the drive plays: which
  driver is loaded (its version bytes and build date), whether the two
  channels the first sounding test would take are in use, and under AUDIO64
  the top 48 KiB of sound memory, on the console-used pilot's bus rules.
  Run over the 60 driver images of the owner's archive, it names each as the
  drivers' static reading does.
- 150 of the corpus's 426 CD audio games take the Background reader with
  that GUI and the virtual VMU off; the 271 on the standalone New reader are
  not reached.

Next, after its console round: Test 85, the sound (the track's audio read
from the card, two channels, the game's CD volume), in the games whose
survey shows those channels left alone.

### Test 85 (27 November 2026): the music is heard

Step 4 as built (`cdda-background-test-85.md`, build `b9cff7086258`), on Test
84's recipe, one switch each. Delivered, and run on the console the same
day: POD SpeedZone played its music; ChuChu Rocket and KOF Dream Match
1999 stayed silent, because their sound data leaves no place in sound
memory for a ring of samples.

- The audio from the card. The worker's heartbeat asks for a play's
  sectors a segment at a time; the auxiliary reads them with the
  Background reader's own stream while no read of the game is asked for,
  gives the card back at once when one is, and hands on only blocks whose
  CRC was checked. An idle read-ahead window is given up for audio that
  runs low.
- The sound. A ring in sound memory, clear of everything the driver's bank
  table names, looped by two channels the driver leaves free; the clock is
  TMU0; the level is read from the CD input's mixers. Before every write
  the driver's identity, the channels' records, the table and the ring's
  guards are read again.
- Three driver builds, the ones Test 84's survey covered on the console:
  MANATEE 2.00.09 (ChuChu Rocket), 1.01.3b (KOF Dream Match 1999) and
  1.01.37 (POD SpeedZone). Every other game stays silent with its reason
  in the report.

What its console round said: the path holds end to end in POD (no key
dropped, no channel taken, no store refused); the card keeps up except
behind the game's own reads (9 rings run dry in 63 s of music); the CD
mixers read back, and POD gives them unlike words; a heartbeat cost
2.48 ms, 0.9 ms of it the reads that prove the driver, the table and the
guards.

### Test 86 (27 November 2026): music where sound memory is full

`cdda-background-test-86.md`, build `0af5d9ce889c`, on Test 85's recipe.
Delivered; no console result yet.

- The ring holds the sound chip's own 4-bit ADPCM, made of the card's
  samples as they pass and keyed in the format whose decoder runs on
  across the loop: a quarter of the memory and of the bus traffic.
- The ring lies in the end of the driver's own memory that no MANATEE
  build uses (8 KiB under MANATEE 2, 12 KiB under MANATEE 1), and the bank
  table is not asked: a place the table leaves free is where a game may
  load its next sound data.
- Both planes at the larger of the two CD mixers' levels, each on its own
  side; a lost ring taken again within the play; the ring begun again
  where the driver moved the play.
- The report measures the sound chip's clock, as the driver counts it,
  beside TMU0.

Next, by its console round: whether the chip plays the compressed ring
cleanly round its end, whether the spare memory stays the ring's, and how
often the small ring runs dry. Then the driver's count as the ring's
clock (no starting over, the whole ring as lead), the other MANATEE
builds by survey, AUDIO64, and the standalone New reader's games.

## 1. Shared correctness and reproducible deployment — active

Shared GUI preparation is also an immediate integration requirement. The
owner's Power Stone 2 pre-bootstrap report is tracked in
`shared-gui-launch-preparation.md`. Audit the actual GUI path before attributing
delay to cold-stage work; apply fixes to common operations across all routes.
The owner reports preparation is now about four seconds. The next actual
five-service bootstrap rollout and remaining console check are recorded in
`shared-native-bootstrap-rollout.md`; later GUI builds must retain its exact
default admission bindings.

Apply the demonstrated lessons across the actual affected reader families:

- Reject lost-byte-marked SCI receptions before payload delivery; preserve
  full block CRC, bounded same-LBA retries and existing PIO fallback.
- Publish the shared CE callback-cancellation and active-DMA-error completion
  corrections already present in Main source into qualified rebuilt modules.
- Close generic CE byte-write coverage for valid CPU mappings backed by VRAM,
  including synchronous and asynchronous delivery, partial callbacks and odd
  boundaries. Preserve mappings, permissions, completion order and main-RAM
  copy performance; do not broaden address admission merely to fix copying.
- Audit every storage-loan control operation and completion owner. The exact
  VC2 scheduler gate is source-specific; it is not a universal callable thunk.
- Build ordinary, standalone, compressed, native CDDA and WinCE CDDA cold stages
  through a single recorded build procedure. Inventory Standard, compact
  Background, physical disc and collection components separately so omission
  cannot be described as a global rollout.
- Qualify staged output before invoking the current Main KUP producer. Refuse
  stale extra files, user data, mixed components and unqualified substitutions.

Completion evidence: meaningful old-source failing controls, production-code
and linked checks, clean-source compiler receipts, complete installed-file
hashes, actual GUI admission and current C-consumer validation. Record source,
native-build, package and hardware results separately. An unbuilt or failed
family remains explicitly pending.

Accepted Smash modules/helpers are preserved as an independent component set.
Do not rebuild them to obtain a matching build label or retune VC2. Existing
worker memory contracts, save menus, return behavior and updater revision 3
remain preservation boundaries.

The first integration provides these repeatable entry points:

```sh
python3 tools/build_universal_loader.py plan
python3 tools/native_service_inventory.py
make test-universal-loader
source .deps/kos/environ.sh
python3 tools/build_universal_loader.py build --jobs 4
```

The build command requires committed source and records each phase. It creates
the 12-role cold-reader subset and a generated native CDDA admission manifest,
not an installable update. The current GUI must be linked with that manifest
and the independently preserved Standard/Smash bindings before qualification:

```sh
python3 tools/build_universal_loader.py check \
  --runtime-elf build/kui-runtime.elf --output build/universal-cold-rollout
```

The output directory must be new or empty. A successful subset check does not
qualify the separately listed Standard/Background/disc/collection components
or their final composition. The current producer is used only after those
remaining delivery gates pass.

The first test composition must inherit `apps/games/ce-probe.kui`: an existing
personalized Armada writable-VMU adapter uses that same path. Replacing it with
the ordinary CE build would remove a working service. The ordinary build is
still compiled and qualified, but its installation stays deferred until the
VMU-aware route is separated and admitted. Do not publish the personalized
adapter or its prepared card identity to recover this capability. Report the
deliberate omission as partial deployment rather than global completion.

## 2. Independent services and memory

Establish actual routing and retained memory for all four configurations:

| Requested configuration | Required retained objects |
| --- | --- |
| Neither | Ordinary reader; no optional VMU/cache lease |
| VMU only | VMU code/state/stacks and its storage adapter; no speculative cache |
| Reader/read-ahead only | Reader/cache and required stacks; no virtual-card authority |
| Combined | Both services under a checked memory and storage ownership contract |

The first pass inventories actual Make configuration and production geometry.
The compact Background VMU-Off route currently retains 284 KiB including a
192 KiB cache; do not report it as a smaller prefetch-only design. Standard's
72 KiB VMU service remains distinct. Changing reservation size requires new
linker bounds, relocation/stack checks, allocator and complete lifetime proofs.

Completion evidence: actual route selection and rejection behavior, all
retained owners accounted for, unchanged physical VMU behavior, and save/reload
checks on the changed configurations. A disabled setting alone proves none
of these properties.

## 3. Extend shared native runtime coverage

| Runtime | Existing working route | Shared-service work |
| --- | --- | --- |
| DOA2 | Standalone demand reader | Prove memory ownership before adding virtual VMU or cache; earlier apparent free arenas were contradicted or incomplete |
| Sonic Adventure | Historical ordinary/standalone New play reports; exact original now passes linked plain/nesting standalone callbacks | Current combined immutable resolution passes; live service lifetime and actual console callback behavior need evidence |
| Silver | Exact Standard VMU adapter; current original passes immutable combined resolution | Qualify live executable reclamation, exit consumers and lock behavior |
| Crazy Taxi | Standalone New | Reviewed retained arena |
| Zombie Revenge | Standalone New | Six-input Maple layout and allocator/reservation |
| Blue Stinger | Shared early-family admission incomplete | Earlier interrupt-table initializer and associated ownership |
| House of the Dead 2 | Virtual VMU saving and reload | Residual lag; bounded scheduling work without disturbing persistence |
| PSO / Spawn | Booting; recorded intro problems | Resolve delivery/scheduling evidence; Spawn audio distortion remains an observation |

Keep successful image results for Time Stalkers and EGG. Preserve exact
Standard adapters until their shared replacement qualifies. Admit structural
SDK families through source-bound contracts rather than game-title guesses.
No percentage of universal game compatibility is implied by the current corpus.

The delivered standalone New reader exported the early plain handler in both
roles, losing an already proved nesting alternative. Public IRQ source
`0d1691d` corrects this structurally. Clean standalone build `a0dc9a62e79f`
passes linked instruction models using original EGG, The House of the Dead 2
and Time Stalkers executables in both modes, plus actual-module GUI preparation
on FAT32/exFAT. The later `f8807cb16561` standalone also passes the same
linked test on the owner's exact Sonic Adventure original in both modes,
including old-reader failure and refusal controls. Actual console callbacks
and the selected runtime remain a separate checkpoint. The Background 7c route
already handled the alternative.
See `native-zr-family.md` and the exact module/evidence record in
`shared-game-launch-performance.md`.

Completion evidence per changed family: launch, loading-heavy gameplay, virtual
card detection, game-requested save, power-cycle reload and appropriate exit.
Use one representative runtime to establish a shared mechanism, then record
the remaining coverage honestly.

## 4. Coordinated FMV and CDDA streaming

A proposal for bringing the Toy Commander card stream reader (Test 76,
`docs/toy-wire-stream-test-76.md`) to the compact Background family is in
`docs/fmv-stream-reader-plan.md`. It is not approved and changes nothing
in this sequence. Its second step, still inside the Toy test module, is
Test 77 (`docs/toy-wire-stream-test-77.md`).

Investigate a safely reserved sequential streaming ring, with optional full
movie preload when the asset fits. Reuse the shared stream and verification
primitives; native Toy/MDK2 and CE still require their own scheduling and
completion adapters. Preserve working audio playback and save durability.

Measure request sizes, sequentiality, buffer occupancy, underruns, command
count, completion cadence, audio-refill deadlines and masked execution windows.
Retained bad Toy/Armada intros did not always have an active CDDA worker, so
CDDA traffic alone is not an established cause. A finite buffer can absorb
stalls but cannot repair sustained undersupply.

The delivered CE CRC16 table folding preserves all checked bits but adds
per-byte conditional work. Armada FMV timings from that cohort are therefore
not a clean baseline for the original full-table receive loop. Restoring the
256-entry table with unchanged resident/stack guards remains blocked. The
four CE music owners overflow by 192 bytes without compaction and by 64 bytes
after the final narrow lossless compaction/alignment trial. Experiments have
stopped; no CE replacements were promoted. The generic full-table fit is not
an exception, and all installed CE bytes retain the folded cost. All-family
fit, integrity and admission must pass before claiming restoration. Keep
coordinated native CDDA/WinCE streaming, service combinations and hardware
acceptance outstanding.

Keep demand traffic and audio deadlines ahead of speculative reads. Deferred
VMU commits must retain journal/readback and acknowledgement ordering. Do not
grow a reservation from apparent RAM headroom or silently suppress saves.

Completion evidence: improved same-scene playback on hardware, continuous
music under concurrent data reads, seek/track changes and save/reload, with
unchanged gameplay responsiveness. Keep the four existing CE kernel/sound
profiles explicit; unfamiliar kernels need their own contracts.

## 5. Broaden routes and complete the lifecycle

- Physical discs: preserve Power Stone 2 SCI save/reload; resolve Power Stone 1
  buffer recognition and Time Stalkers' different startup-clear lifetime.
  Qualify SCIF, CE and CD/MIL-CD independently.
- Formats: integrate existing CHD/CSO experiments with actual launcher routing;
  qualify compression together with VMU, cache and CDDA where supported.
- Transports: carry source fixes into each affected reader, retain SCI/SCIF/IDE
  boundaries and qualify future ATA DMA on actual hardware.
- Return/disc change: extend verified owner retirement, current save recovery
  and authenticated reload. Check the game's real disc-change boundary rather
  than merely switching a map. Preserve original engine save/options menus.
- CE protocol: complete presently unsupported endpoint operations only after
  their caller, scheduling and completion contracts are reviewed.

Completion evidence: the requested combination works from cold launch and
after another game returns. Disc change retains the current virtual card and
retires stale reads/cache before rebinding. Report unsupported combinations
explicitly; existing boot success is retained independently.

## Delivery and acceptance ledger

Each candidate records base commit, intended behavior, rebuilt and preserved
roles, effective service/reservation inventory, actual artifact hashes,
host/linked/native/package results and the smallest outstanding console check.
Keep generated build evidence separate from hardware acceptance.

The standing delivery is a manual-copy ZIP with both identical GUI aliases,
the intended physical software tree, and an exact changed/retained inventory.
Validate cold CD/ROOT boot and actual module admission. When a KUP is explicitly
requested, use the current `tools/make_update.py` producer and actual C consumer.
A complete composition retains accepted components with their real identities
and matches the GUI's admission/provider decisions.

User-facing test filenames include purpose, delivery date/time with an explicit
zone, and the actual binary build identity, for example
`KUI-Unified-Test-06NOV2026-1133-CST-72db614.kup`. Use Chicago local time with
the applicable CST/CDT suffix. A new delivery filename does not imply rebuilt
software; preserve the original binary identity and checksum when renaming.

The accepted cc81 package and first consolidated test both contain updater
engine 3. Menu Restart can select a saved bank by activation sequence, so manual
ZIP comparisons use the cold ROOT entry and its actual reader receipt. The
earlier updater-and-test bootstrap is historical delivery evidence.

## Branding and documentation follow-up

Keep the account migration separate from loader qualification. Update active
source links, download/release tooling, shell credits and their tests to
TacitusVox. Regenerate current branding from its source assets and update any
derived images, embedded arrays and hash checks together. Inspect image pixels
before deciding which artwork needs changing; a text search alone is not an
artwork audit. Review hardware artwork and generator output in the same pass.

Preserve historical release records, original credits and provenance. Historical
TPMJB references require individual review rather than a repository-wide text
replacement. Current publishing targets must resolve to TacitusVox even while
that historical material remains.

Reference contracts: `accepted-progress.md`, `global-launch-policy.md`,
`shared-loader-audit-2026-11-06.md`, `shared-loader-vmu-regressions.md` and
`loader-integration-baseline.md`.
