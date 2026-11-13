# Toy Commander FMV investigation

November 11, 2026 · MK-57020, 60 Hz, original raw2352 GDI on SCI

The slideshow and the early return to the menu are two stages of the same failure. Toy clocks the movie from audio played by AICA. When video falls three frames behind, it starts suppressing rendered frames while it catches up. When it falls more than 60 frames behind at 30 FPS, its updater returns error `-1981` and the wrapper cleans up the movie. The reports show 61–63 frames of lag and zero user skip in tests 20, 21, 23, 24 and 25. In tests 16 and 17, the last 14-sector GD read succeeded, the game acknowledged all 28,672 bytes, and the next command was PAUSE. This is consistent with the lag exit, including in runs with zero failed GD reads.

I reran the FPS and lag branches from the original executable in a bounded SH-4 interpreter. Lag 60 continued and lag 61 returned `-1981`. The 29.97 FPS representations in the AVI round to 30 in the original parser. That rounding accounts for about two frames over the entire intro. The interpreter used authored state and the reports captured lag at PAUSE, so neither directly captures the live updater's return value or measures AICA sample-position accuracy. Source: `docs/toy-player-exit-analysis.md`, `toy-decoder-audit/run_player_original.py`, and reports 16–25. Original startup SHA-256: `ee68a326da964fec5892b7226311f2293fd3a0bbf5bac496ec82991964d49bbd`.

## Why the original image falls behind

Toy's steady-state movie poll uses the SDK's nonblocking mode. I executed the original poll at `0x8c097114` with a pending file request through its original GDFS dispatch tables, intercepting only the BIOS boundary. It issued GD EXEC and GD CHECK, returned pending and left the request owned. The K-UI native SCI route treats both entries as opportunities to catch up. The selected X reader can ask for up to 20 physical blocks of foreground work. That is about 6.6 ms of raw wire time if all 20 must be received, before checksum, copy and recovery. When the bytes are missing, the current implementation polls SCI inside the masked call.

That is a specific mismatch between Toy's async movie loop and K-UI's reader. The game expects to get back to decoding while its file read progresses. K-UI can spend that time inside the poll. AICA continues playing during the call. A high maximum lead or a cached first block cannot keep a complete request off the foreground path. Test 4 showed that shortening visits without a producer that keeps up also regresses. Both autonomous progress and a bounded foreground poll are required. Code: `src/loader/retail_async.c` (`tops_up`, `kui_retail_async_call`) and `src/loader/toy_suite_stream.inc` (`toy_stream_produce`, `toy_roll_deliver`).

Tests 20 and 24 are the closest late-intro comparisons. Between samples around 31 and 37 seconds, they cover nearly the same disc addresses. Test 20 delivers about 875 KiB/s of guest data, reports about 84 handled partial receptions per second, and spends 74% of that interval in measured reader dispatch. Test 24 delivers about 868 KiB/s, reports about 102 partial receptions per second, and spends 77% in dispatch. These are sample-to-sample wall-time deltas. Whole-report COST and SPAN cover different scopes because the older SPAN omits the initial movie fill and COST includes opening and idle work. Dispatch time includes wire waiting. It is not a count of CPU instructions.

SCI also really is losing bytes during some payload receives. The fault record sees ORER with the channel still short of its count. `RECOVERY lost` counts one handled class of partial reception. The `SD OVR` column counts a different path, which is why it can be zero while hundreds of partial receives are recorded. The receiver has only a byte time of about 0.64 µs at 12.5 MHz before the next byte arrives. An October console control reproduced premature ORER during ordinary shell rendering, with 51 incidents in 65,422 reads. With the display quiet, 71,641 reads completed without those errors. This is evidence of a transport susceptibility outside Toy. The Toy snapshots sample after the error and cannot name the transaction that held up reception.

The cache-flush intervention was too narrow. Test 24 recorded 953 successful barrier entries, 42 with a block drained and published. Its window still showed the same lag exit. The hook covers one audio cache loop. Original code also has a separate video callback cache loop at `0x8c04c252` and CPU-driven sound writes following the hooked audio loop. Neither is covered by the audio hook. No snapshot identifies those operations as the actual fault source. Fencing more callbacks would consume additional decoder time and would still leave the per-block reader design in place.

## What the comparisons establish

| Comparison | Result and implication |
|---|---|
| Original disc, CDDA-off SCI, and silent CDDA experiment | The disc plays correctly. SCI stutters with CDDA inactive and before the first CD-music PLAY. AVI audio and CDDA are separate. Fixing the CDDA adapter is necessary for eventual all-on support, but the intro problem exists in the raw data route by itself. |
| Early AF versus AI/AL | AF recorded 540 small reads before music, about 22 seconds of wait and 97% of that wait inside the reader. Later changes shortened the big opening read by under 1%, with first-sector wait still around 10–15 ms. Faster opening load and repeated small-request latency are separate. Source is the captured analysis supplied in the conversation and the November 10 Claude handoff. |
| Header replay and bounded prefetch before the numbered suite | Header replay recorded zero relevant replays. The bounded read-ahead run showed 150 intro fills and nine intro hits. These variants were not supplying a sustained queue to the game. |
| 1/3, and 20 | The early graphics guard exercised no guarded PIO in the movie and 1/3 stopped at the same LBA. Later test 20 sampled busy on 429 of 101,405 checks and chose eight guarded PIO blocks. Short active-start samples cannot exclude transient or CPU-driven bus work. |
| 2/6/8 and 9/10 | Small verified RAM windows did change timing. 2/6/8 read about ten seconds farther than the corresponding demand controls. 9's late window received zero hits and exactly matched its staging control. |
| 13/14 and 16/17 | The staged opening prefix served 365 physical hits in 13 and zero in 14, yet both finished at LBA 469,990. Test 17 really did serve 162 physical blocks at the later cutoff and verified the bytes in the game's destination at LBAs 473,517 and 473,518. The final read moved just 84 sectors and 0.10 s versus 16. A fixed bad cutoff sector no longer fits the evidence. |
| 5/15 | Cooked demand 5 read to within 38 sectors of the AVI extent end and still looked bad. Cooked rolling 15 stopped much earlier. The easier layout buys transfer headroom. It does not by itself deliver smooth playback, and the two image files can also have different SD layouts. Raw remains the target. |
| 11/12 and 18 | 11/12 mixed urgent reads outside the ring with speculative fills, rebasing the ring and sometimes waiting for an IRQ with no DMA armed. 18 repaired those progress defects and genuinely produced 29,497 blocks from IRQ with 160 KiB maximum forward lead. It stopped at exactly the same LBA as 2. Maximum lead is not sustained ready data. |
| 19/22 | The game reached its own resource allocation error before an in-game report could be saved. Its original startup asks for **5,200,000 bytes** at `0x8c027c6a–0x8c027c76`, then prints that error on allocation failure. These tests added 832 KiB to the reader reservation, from 288 KiB to 1,120 KiB. They never exercised the hypothesis that a larger in-game ring would help. |
| 21 | Resume plus EDC reduced late restarts from about 84/s in 20 to about 3/s in its own late window, but the intro stopped at 17 seconds and full-request readiness stayed about 22%. The implementation stages and EDC-checks every raw sector and then copies its payload again. It checked 10,308 logical sectors for 1,543 quarantined physical blocks. This experiment couples cheaper restarts to more work on the ordinary path. A selective version could leave strictly verified blocks on the fast path and independently check all Mode 1 sectors touched by a reconstructed block. Each 512-byte block spans at most two logical sectors, so the recorded 1,543 quarantines would call for at most 3,086 sector checks. The time saved has not been measured. |
| 7/23/25 | All three serialize copying and SD reception, and all three regress sharply. 23/25 also make a cached demand wait for a different, already-armed receive before serving it. On the same bumper, dispatch totals rise from 6.38 s in 20 to 9.14 and 9.07 s. Whole requests ready fall from 95/293 to 2/290 and 2/288. In the intro both have zero full-request hits and stop after about nine sampled seconds. Test 7 had already given the same direction of result on the demand reader. |
| 24 | The audio cache fence ran as intended. It left playback and reception loss in the same failure regime. Mode 20 also had a bus guard that 24 lacked, so their comparison cannot attribute the difference in loss rate to the fence alone. |

The useful target is **enough verified data ready to satisfy whole game requests, with enough time left for Toy to decode**. These reports establish that neither condition was reached reliably. They do not partition the remaining time into individual DMA arbitration, recovery, cache, and decoder costs. The exact bus owner at the point of an overrun remains unknown.

## The unused transport option

All numbered tests that use SCI receive DMA use a single 512-byte card block per DMA. The current driver transfers 513 bytes, keeps the final CRC byte in the receive register, and deliberately overruns on the next byte to stop reception. Each successful block pays the handoff, SCI reset and rearm. Keeping CMD18 open across requests reduced command overhead, but did not change this service grain.

There is already a console proof that the wire can cross block boundaries. Build `97b590137b9b` captured 16,384 bytes in one DMA in 10.488 ms. It checked 31 complete blocks against both their CRCs and a separate read. The capture ended 388 bytes into the next payload, then CMD12 succeeded and ordinary reads recovered. This was a quiet diagnostic. It establishes that one mid-payload stop and a continuous capture worked, with game bus traffic and arbitrary stop phases still unqualified. The historical 512-byte quiet streaming benchmark measured 344.6 µs reception and 30.4 µs serial turnaround per block. Its 1,202 KiB/s measured rate included an extra test CRC32. The reported estimate excluding that extra work was 1,323 KiB/s. Claude's October SCI design already proposed multi-block DMA. No numbered Toy test implemented it.

A host prototype now checks how to retain useful work when such a burst is cut short. It passes 32,770 cut/restart cases and 32,768 single-bit fault cases using authored frames and both constant and varying token gaps. It accepts only the ordered, complete, CRC-checked prefix. It assumes that the DMA count and ownership are fenced correctly and that CMD12 and the new command succeed. It does not establish those hardware conditions. The model is in `toy-investigation-design/sci_burst_prefix.py`.

A candidate can use two aligned 8 KiB wire buffers inside the existing 192 KiB data arena. The remaining 176 KiB can retain 160 KiB of forward lead and 16 KiB of history. The receiver should run while the game can execute. At a complete or interrupted burst, fence the owned channel, validate whole frames in order and keep the good prefix. After a verified stop, restart from the first unchecked card LBA. A short or corrupted frame never reaches the game's buffer. Maintain the normal GD token, status, acknowledge and cancellation rules. Serve ready requests immediately while unrelated DMA remains active. A burst that merely busy-waits under the old foreground call would worsen the problem.

The burst primarily reduces the number of returns to the CPU and the amount of good work lost on a fault. Its sustained-rate advantage alone can be modest. A sensitivity model using 300 µs command/stop overhead and 150 independent faults per second of active wire gives roughly 1,255 KiB/s physical for the one-block control and 1,361 for a nominal 15-block burst. The actual fault rate, restart cost, checksum work and scheduling are not modeled by those two values. A long burst can also delay the first block's publication. Urgent misses need a bounded service path, and long bursts belong to speculative lead. These are design constraints, not a playback prediction.

Historical sources: `docs/evidence/sci-async-speed-cmd18-2026-10-02.md`, `docs/evidence/sci-async-cmd18-stream-2026-10-02.md`, `docs/evidence/sci-async-overrun-retry-2026-10-02.md`, `src/loader/sci_stream.c`, `src/loader/toy_suite_stream.inc`, and [Claude's SCI design note](https://github.com/TacitusVox/K-UI-NeXT/blob/claude/modest-galileo-hpjv79/docs/sci-loading-theory.md).

## Toy already has the large movie buffer

The original wrapper stores `0x100000` at movie context `+0x1dc`. The type 6 stream allocates this 1 MiB buffer for the shared interleaved movie data. Packet metadata and alignment reduce its usable payload. The startup code asks for three quarters of the allocation, 768 KiB, before parsing the first filled window. That accounts for the 384-sector opening request. The input ring is separate from the 5.2 MB resource allocation that failed in 19/22.

In steady state, the game reserves a contiguous free span of its own ring, checks the previous request, and submits a read sized by an adaptive quantum. It advances its write pointer only after completion. The original sizing code divides remaining compressed bytes by remaining video frames and rounds to sectors. Actual refill size is also clamped by ring space, wrap, descriptor availability and the file tail. Some of the tiny reads go through a separate header or scratch path.

I checked the existing sizing and submission bounds by executing the original instructions with authored state. An example based on the intro's file size selected 14 sectors, the same size as the last observed reads in 16/17. With a candidate 64 KiB quantum, the original clamps still reduced it to 6 KiB near wrap or near full, to 10 KiB with a five-sector file tail, and suppressed submission at EOF, without room, or while a previous request was owned. All 17 host cases passed. This proves room for a different refill policy without reserving more heap. It does not show that a larger request completes quickly enough. The game only commits a request as a unit, so larger refills need adequate ready lead and can increase latency when the queue is empty.

Preserve the game's ring ownership. The loader can complete its existing authorized reads from verified data. Writing speculatively into game memory would bypass its file, pending request, wrap and consumer accounting. Keep a refill policy change separate from the transport evaluation until there is a timing reason to change it.

Original code anchors: `0x8c04c728` allocation size, `0x8c09938c` allocation, `0x8c09759a` initial fill, `0x8c0987a2` adaptive quantum, `0x8c09894e` refill bounds, `0x8c098448` publication. Host check: `toy-player-refill-audit/check_original_refill.py`.

## Capacity limits and the WinCE comparison

The supplied PC scan measured 798.2 KiB/s average for the 60 Hz intro, equivalent to about 917 KiB/s of raw track data. Its 1,335.9 KiB/s one-second encoded payload peak needs about 1,534 KiB/s on raw2352. With 516 wire bytes per 512-byte SD block, the ideal 12.5 MHz ceiling is about 1,514 KiB/s raw or 1,318 KiB/s of 2048-byte payload. Even the ideal wire needs some prior lead at that peak. The game has room for lead if the reader maintains it. The packet peak alone does not establish the lead needed across a series of bursts. The original AVIs are unavailable in this workspace, so there is no measured packet-by-packet demand curve here.

Armada's successful path had a lower movie demand of about 507 decimal KB/s. Its 6.25 MHz recovery window was feasible at that rate, and a faster checked receive path reduced IRQ time further. Separately, the VMU poll fast path cut about 16.9 seconds of hook time to 1.05 over a roughly 120-second movie. One successful Armada run still recorded over 5,600 receive overruns. The improvement was that the game got enough time to play. Slowing all Toy movie reception to 6.25 MHz tops out at about 659 KiB/s of raw-track guest payload, below its 798 KiB/s average. Even its existing game and loader buffers cannot absorb that average deficit over the whole intro. A short, bounded recovery window is a different proposition.

Sources: `docs/armada-movie-timing-test.md`, `docs/armada-vmu-on-audio-fix-test.md`, and [Claude's November handoff](https://github.com/TacitusVox/K-UI-NeXT/blob/claude/great-darwin-yjmgfi/docs/handoff-2026-11-10.md).

## Criteria for another build

Start with the original raw image and the existing memory budget. Reuse the continuous receive diagnostic to implement bounded transport and checked-prefix recovery behind the current GD contract. First show safe stop/restart from an incomplete frame, exact CRC validation and correct fallback at extent boundaries in the real state machine. The host parser and original request checks provide partial coverage of that work.

A PC pass over the existing image can extract only packet byte offsets and timestamps. That gives a demand curve for testing how much lead is needed. Evaluate cumulative demand across intervals against measured service, including command waits and checksum/copy cost. Compare verified data ready at whole-request deadlines and time left for the player. Keep the game's clock and lag limit intact. Visible smoothness through both movies, normal completion, and responsive gameplay are the outcome that matters. A higher first-block hit rate, more bytes read before a lag exit, or fewer restarts is insufficient.

The working diagnosis is a deadline failure caused by the current SD delivery and CPU service regime. The hardware failure initiator is still unmeasured. A bounded burst reader is a concrete path with prior wire evidence and a partial host proof. It is not a completed Dreamcast fix. This audit produced no replacement build.

## All numbered console reports

Each span is measured between sampled intro reads. It excludes the opening fill and is not stopwatch playback time. The endpoint is the last completed request's exclusive LBA. All 23 populated reports have zero sampled read failures and zero clock faults. Tests 19 and 22 contain only the reserved report placeholders. Visual playback and normal EOF require independent evidence.

| Test | Reader change | Intro read span | Sectors | Last exclusive LBA |
|---:|---|---:|---:|---:|
| 1 | Raw demand, guest payload witness | 38.77 s | 15,927 | 469,080 |
| 2 | Verified RAM window at LBA 469080 | 48.73 s | 20,407 | 473,560 |
| 3 | DMA guard before SCI reception | 38.81 s | 15,927 | 469,080 |
| 4 | 4 ms foreground delivery budget | 20.81 s | 7,919 | 461,072 |
| 5 | Cooked 2048, demand control | 65.14 s | 25,827 | 478,980 |
| 6 | Verified RAM window at LBA 468128 | 48.71 s | 20,365 | 473,518 |
| 7 | Serialize receive and copy | 10.95 s | 3,453 | 456,606 |
| 8 | Two verified RAM windows | 48.71 s | 20,365 | 473,518 |
| 9 | Late RAM window | 40.75 s | 16,823 | 469,976 |
| 10 | Stage windows, serve from SD | 40.75 s | 16,823 | 469,976 |
| 11 | Raw rolling DMA | 18.83 s | 7,015 | 460,168 |
| 12 | Raw rolling DMA with prefix | 20.86 s | 7,911 | 461,064 |
| 13 | Play early prefix from RAM | 40.66 s | 16,837 | 469,990 |
| 14 | Stage prefix, serve from SD | 40.79 s | 16,837 | 469,990 |
| 15 | Cooked 2048, rolling DMA | 38.66 s | 15,913 | 469,066 |
| 16 | Mid-window RAM, stage cutoff control | 48.65 s | 20,337 | 473,490 |
| 17 | Mid-window RAM plus verified cutoff window | 48.75 s | 20,421 | 473,574 |
| 18 | Continuous raw producer | 48.72 s | 20,407 | 473,560 |
| 19 | 1 MiB reader arena | Allocation failed | | |
| 20 | Small arena with graphics/G2 guard | 40.80 s | 16,837 | 469,990 |
| 21 | Small arena with resume and Mode 1 EDC | 17.03 s | 6,113 | 459,266 |
| 22 | 1 MiB arena, graphics/G2 guard and EDC | Allocation failed | | |
| 23 | Small arena, serialize receive and copy | 9.22 s | 2,585 | 455,738 |
| 24 | Small arena, fence audio cache flush | 38.21 s | 15,633 | 468,786 |
| 25 | Small arena, serialization and fence | 8.83 s | 2,403 | 455,556 |

Inputs were the 25 original text reports, delivery READMEs and archived code at reader commit `e6eb8f5f2f0f` and its ancestors. The earlier controls were cross-checked with the supplied screenshots, raw/cooked PC scan, and the two Claude documents above. `toy-investigation-design/summarize_reports.py` reproduces the numbered table and JSON counters from the report directory. The workspace contains the private startup executable for instruction checks. No executable bytes or movie assets are in this report.
