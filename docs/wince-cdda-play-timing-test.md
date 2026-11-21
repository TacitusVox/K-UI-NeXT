# Armada CD music On: the gameplay timing test (21 Nov 2026)

**Status: console-tested 21 Nov 2026 ([result](#result-21-nov-2026)): the
lag is the music worker's programmed receptions while the music is urgent,
in steps of 8 ms and more above the game's threads; the game's reads are not
slowed.** A measurement, not a fix. It changes no installed module.

The adaptive CD music modules (`52a946122537`) left CD music On a tiny bit
worse in gameplay than CD music Off ([result](wince-cdda-adaptive-test.md#result-21-nov-2026)).
The owner added the same day that they copy the whole `KUI` folder of each
ZIP, so both Armada modules of that set ran, and that it plays about the same
as the old CD music modules (`7b216f2e1136`), with the movies fixed. The
owner asked to find the lag before CD music On is built for every game.

## What CD music On adds in gameplay

From the code ([transport](../src/loader/armada_ce_stream_transport.c),
[player](../src/loader/armada_ce_stream_player.c)):

- **Card reads.** The music's own reads: 16-sector jobs (37,632 bytes, 213 ms
  of PCM, 75 card blocks) refill a 352,768-byte DirectSound ring, about
  172 KiB/s.
- **Card sharing.** A game read takes the card from the music after the
  music's block in flight (a preemption). While a game read runs, the music
  is granted the card after every 16 of the game's blocks: for 8 blocks, or
  for 75 (a whole job) while its fresh PCM is under one second (urgent, until
  1.5 seconds). Each switch is a CMD12 and a new CMD18 with its token wait.
- **CPU.** Each music block costs an interrupt (about 122 µs each on Armada's
  intro, [measured](armada-movie-timing-test.md#third-result-fast-check-reader-8cdbc0a351f8-9-nov-2026)).
  A priority-2 worker thread (above the game's normal threads) copies the
  checked PCM into the ring, at most 32 receptions per step, by programmed
  transfers while urgent.
- **Mixing.** CE mixes the music's software buffer (`DSBCAPS_LOCSOFTWARE`)
  with the game's sounds. Nothing here measures that.

With CD music Off, Armada's gameplay on the generic adaptive reader read 44
streamed 64 KiB reads in about a minute: median 86.9 ms, 90% 105.0 ms, each
sequential request one frame after the previous read ended, the reader open
17% of the time ([adaptive session](wince-adaptive-window-test.md#result-21-nov-2026)).
The game is read-limited while it streams, so a slower read is lost time.

## The build (source `32b3bbb3`)

`KUI_ARMADA_CE_PLAY_TIMING=1` on the Armada timing reader
(`KUI_ARMADA_CE_MOVIE_TIMING=1 KUI_ARMADA_CE_SLOW_AFTER_OVERRUN=1
KUI_ARMADA_CE_FAST_TAKE=1`): the low reader is byte-identical to the movie
timing reader's (fixed 64-reception slow window, as in the CD music modules
before `52a946122537`); only the high controller changes. `=2` builds the
control, the same module with the music never queued: the game's audio
controls reach the GD service as before, and no worker, buffer or music card
read starts. Both report six rows in place of the movie's, over a window from
the first GD poll after the intro movie to A+B+X+Y+Start
([logic](../include/kui/armada_ce_play_timing.h)):

| Row | Meaning |
|---|---|
| PLAY-MS SECTORS KB/S READS STATUS | Window length, the game's sectors and KB/s, read requests begun, flags (1 movie seen, 2 window open, 4 clock valid, 8 worker timed: run 1 expects 15, run 2 expects 7) |
| CALL-MS IRQ-MS WORK-MS BUSY% PEAK% | K-UI's time in its GD calls, its interrupt and the music worker's steps (each step's time less K-UI's calls and interrupts inside it), their share of the window, the busiest second |
| SOLO SOLO-MS SHARED SHARED-MS MAX-MS | The game's 32-sector (64 KiB) reads, timed from REQUEST to their last card block (CE's count plus TMU0, as the data probe times them): count and mean without a music grant while they ran, the same with one, and the longest |
| MUSICBLK GAMEBLK GRANTS PREEMPT PEND% | The transport's card blocks for the music and the game, music grants while a game read ran, game reads that took the card from the music, the share of the window with a read outstanding at the game's polls |
| DMA-OK POLLED OVERRUN RESTART SLOW-OK | The card stream's counters in the window |
| STEPS MAX-US OVER8MS URGENT LOW-MS | Worker steps, the longest (µs), those of 8 ms or more, those that ended urgent, and the least fresh PCM a playing step saw (ms) |

The checker pins the transport's event function byte for byte; the read
observer runs from `refresh_holds`, that function's first call, so the pinned
body is unchanged. The controller grows by 1,472 bytes (payload 43,992, BSS
10,952; budget 98,304).

| Module | Run | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-armada.kui`, music (`=1`) | 1 | 138,412 | `9E73BA00` | `741f95644c057c98...` |
| `ce-cdda-armada.kui`, silent (`=2`) | 2 | 138,252 | `E9655CBD` | `a9bfd084a774eccc...` |

Both are build `32b3bbb33e39`. Delivered:
`KUI-Armada-CD-Music-Play-Timing-21NOV2026-1304-CST-32b3bbb.zip` (SHA256
`69efd1fee70345c96cb31fbde2eec809d67b5cf2a92d0fd42f510eca0de9065b`): run 1 in
`KUI/apps/games/`, run 2 in `RUN-2-SILENT/`, and the module now on the card
(`52a946122537`, payload CRC `5A83CB23`) in `IF-NEEDED/`. Route: Armada (US),
CD music On, Virtual VMU Off. Full hashes are in its `INVENTORY.json`.

Gates on the exact builds: the stream excerpt checker (`--stream`), the
layout checker and the relaxed-call check pass; the low reader ends at its
`0x8c00d800` limit as before; the background reader's call-graph worst case
is 656 of 2,000 bytes (644 in the movie timing build). With the default
switch the five CD music modules rebuild byte-identical to `7b216f2e1136` and
the movie timing build to its bytes before this source. Host:
`test-armada-ce-play-timing`.

## Reading the result

| Reading | Points at |
|---|---|
| SHARED-MS well above SOLO-MS, with many SHARED reads | The music's grants slow the game's reads: let a non-urgent music job wait for the game's read |
| URGENT above 0, LOW-MS low | The music's reserve runs low in play, and its 75-block grants hold the card |
| WORK-MS a large share, or OVER8MS above 0 | The worker's own CPU time or long steps above the game's threads |
| IRQ-MS well above run 2's | The music blocks' interrupts |
| Run 2 lags like run 1 | Not the music path: the timing reader or the module itself |
| Run 2 smooth, run 1 lags, and the rows differ little | Cost outside what is timed, such as CE's mixing of the music buffer |

## Result (21 Nov 2026)

Armada (US), CD music On, Virtual VMU Off, both runs report build
`32b3bbb33e39`. The owner: run 1 lagged as before; in run 2 "with the music
off any lag that existed before was gone".

| Row | Run 1, music (`9E73BA00`) | Run 2, silent (`E9655CBD`) |
|---|---|---|
| PLAY-MS SECTORS KB/S READS STATUS | 105,150 · 7,212 · 137 · 38 · 15 | 127,775 · 7,212 · 112 · 38 · 7 |
| CALL-MS IRQ-MS WORK-MS BUSY% PEAK% | 258 · 5,557 · 10,142 · 15 · 44 | 158 · 3,203 · 0 · 2 · 28 |
| SOLO SOLO-MS SHARED SHARED-MS MAX-MS | 0 · 0 · 0 · 0 · 0 | 0 · 0 · 0 · 0 · 0 |
| MUSICBLK GAMEBLK GRANTS PREEMPT PEND% | 27,371 · 33,147 · 37 · 176 · 11 | 0 · 33,146 · 0 · 0 · 9 |
| DMA-OK POLLED OVERRUN RESTART SLOW-OK | 51,984 · 8,530 · 1,017 · 1,311 · 8,068 | 33,141 · 0 · 0 · 29 · 0 |
| STEPS MAX-US OVER8MS URGENT LOW-MS | 1,437 · 173,083 · 236 · 239 · 1,012 | 0 · 0 · 0 · 0 · 0 |

1. **The game's reads are not slowed.** Both runs read the same 7,212 sectors
   in the same 38 requests (190 sectors each on average; no 32-sector reads
   fell in this window, so the read-time row is empty), with a read
   outstanding for about 11.5 s in each (11% of 105 s, 9% of 128 s). The
   music took the card during them only 37 times.
2. **K-UI's time: 15% of gameplay with the music, 2% without** (busiest
   second 44% against 28%). The worker accounts for 10.1 s of 105 s, the
   music's card blocks for about 2.4 s more interrupt time.
3. **The long steps are the urgent ones.** 236 steps of 8 ms or more against
   239 that ended urgent; 1,437 steps in all, 7.1 ms on average, the longest
   173 ms. While urgent (fresh PCM under one second, until 1.5), the worker
   receives by programmed transfers, at most 32 per playing step (about 7,600
   for 239 steps) and up to 92 per visit while filling before playback
   starts (the 173 ms step fits two such visits). Run 1's 8,530 programmed
   receptions are those, plus the refetch after each of the 1,017 overruns
   (the music's refetch after an overrun is programmed; run 2 had none).
4. **The reserve never ran low**: the least fresh PCM a playing step ended
   with was 1,012 ms. The urgent band holds the music near one second, where
   each step reaches for the card by programmed transfers.

So CD music On's gameplay cost is the worker's programmed receptions, in
bursts above the game's priority, while the music's reserve is merely below
one second. The interrupt time of its card blocks is the smaller part.

An inference, not measured: with CD music Off this window had no 64 KiB
streamed reads, but on the generic reader Armada's gameplay read 64 KiB
streams at 169 KiB/s ([adaptive session](wince-adaptive-window-test.md#result-21-nov-2026)),
about CD audio's 172 KiB/s. The game may stream its music from the data
track when CD audio is off.

## Not answered by this test

- Bust-A-Move 4, Worms Armageddon and Worms World Party: no timing build of
  their modules.
- The adaptive window under the timing build: its low reader has no room for
  it, so these runs use the fixed 64-reception window. The owner found the
  gameplay lag about the same with either.
