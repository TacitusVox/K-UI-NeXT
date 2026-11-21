# WinCE slow-window test (21 Nov 2026): results

Build `9313dad06594` (`KUI-WinCE-Slow-Window-Test-20NOV2026-1847-CST-9313dad.zip`):
the timing probe `5c1cdd89387b` with one change, the 6.25 MHz window after a
DMA overrun cut from 64 receptions to 16 (Makefile.dc `KUI_CE_SLOW_WINDOW=16`).
CD music and Virtual VMU Off, no VMU, cold boot; seven launches, every dump
complete, every stage report naming `9313dad06594`. Compared with the same
games' rings of 20 Nov ([timing session 1](wince-timing-session-1.md)). Derived
values only.

## The owner's results

| Game | 20 Nov (64) | 21 Nov (16) |
|---|---|---|
| Super Producers (JPN) | movie freezes, at times until it ends | "That seemed to fully fix Super Producers" |
| Maximum Pool (US) | slow openings and menus | "still slow in menus" |
| Resident Evil 2 (US) | run 1 few issues, run 2 "WAY worse" | run 1 "hang massively at multiple points"; run 2 "only started hanging at the very end by the start of a game" |
| Sno-Cross (US) | movie freezes | "still had issues" |
| Super Runabout (US) | (session 1: no complaint) | "flawless" |
| Bust-A-Move 4 (US) | smooth | smooth ("doesn't have intro movies and is a very lightweight game") |

## What the rings show

The streamed 64 KiB reads (`DMAREAD` and `DMAREAD_STREAM_EX` of 32 sectors;
clean, such a read takes 52.3 ms):

| Game, session | Reads | Median ms | 90% ms | Longest ms | Overruns per 1,000 blocks | Blocks at 6.25 MHz | Time lost against clean |
|---|---|---|---|---|---|---|---|
| Super Producers, 64 | 189 | 74.1 | 82.1 | 105.5 | 13.0 | 41% | 29% of read time |
| Super Producers, 16 | 189 | 64.8 | 76.1 | 87.1 | 22.3 | 21% | 21% |
| Maximum Pool, 64 | 163 | 91.3 | 101.8 | 110.6 | 32.5 | 65% | 42% |
| Maximum Pool, 16 | 142 | 76.8 | 87.1 | 94.9 | 41.3 | 38% | 32% |
| RE2 run 1, 64 | 185 | 89.6 | 104.4 | 112.6 | 30.8 | 61% | 40% |
| RE2 run 1, 16 | 180 | 74.4 | 81.3 | 96.1 | 36.0 | 29% | 27% |
| RE2 run 2, 64 | 187 | 91.6 | 104.1 | 112.1 | 31.4 | 66% | 42% |
| RE2 run 2, 16 | 186 | 77.3 | 85.3 | 97.2 | 40.0 | 37% | 32% |
| Sno-Cross, 64 | 138 | 80.3 | 93.1 | 104.2 | 22.0 | 51% | 36% |
| Sno-Cross, 16 | 139 | 68.5 | 77.2 | 136.0 | 25.4 | 26% | 25% |
| Super Runabout, 16 | 75 | 72.4 | 85.0 | 88.6 | 44.5 | 24% | 28% |

(Blocks are 512-byte card blocks, 4.59 a sector: the images hold 2,352-byte
sectors. Bust-A-Move 4 has no 32-sector reads and no overrun in either session.)

1. **The change does what it was built for.** With the shorter window the reader
   returns to 12.5 MHz sooner, so overruns per block rise by 15 to 70%. But
   the share of blocks read at half clock roughly halves, so streamed reads are
   12 to 17% faster at the median and 7 to 22% at the 90th percentile. No read
   failed, and none was left open at a press.
2. **Super Producers is fixed because its player now has slack.** On 20 Nov,
   184 of its 188 requests came on the first frame tick after the previous read
   ended (the next request one frame later on the 16.64 ms grid): the
   player was reading flat out and still falling behind, at 622 KiB/s. On
   21 Nov, at the same 625 KiB/s, 69 of 188 requests waited a further two
   frames (median) on the player's own schedule. The movie's rate is about what
   the old reader could just give, and the faster reads give it headroom.
3. **Maximum Pool's animations are still read-limited.** Their loop still asks
   flat out (136 of 141 requests), with reads about 16% faster (median 77
   against 91 ms) but still well above clean (52 ms). Its menus stay slow.
4. **RE2's hangs are not read speed.** Its reads are about 17% faster, yet the
   hangs remain and vary from run to run. Run 1's ring holds gaps of 2.8 to
   19.6 s with no read outstanding in the reads before the intro movie; run 2's
   holds three gaps of 0.7 to 0.8 s about 10 s before the end, near the movie's
   end. As on 20 Nov, each gap begins after an ordinary read with nothing open.
5. **Sno-Cross is unchanged, as expected.** Its stream is paced by the game
   (222 against 236 KiB/s) with reads faster than before, and its freeze remains.
6. **Super Runabout, the heaviest contention seen, did not regress.** 44.5
   overruns per 1,000 blocks; its largest read (a 7 MiB level load, 3,584
   sectors) took 7.6 s against 5.8 s clean, with 598 overruns and nothing failed.

## What it settles, and what it does not

- Settled: a 16-reception window reads faster than 64 under the contention these
  games produce and fixes Super Producers' movie. No read got slower in these
  seven launches; RE2's hangs vary from run to run as they did on 20 Nov.
- Not tested: Armada US with CD music Off runs on this generic module. Its movie,
  measured on the CD-music route, had about 22% of receptions overrun at
  12.5 MHz, where the 64-reception window was chosen; at that level the two
  windows cost about the same per block, but the shorter one means about three
  times as many overruns, each a restart with interrupts held off. It must be
  checked before 16 becomes the generic module's default.
- Not answered: RE2's and Sno-Cross's freezes, which these data place away from
  read speed, and what slows Maximum Pool beyond the overruns.

## The candidate: Armada with CD music Off

The everyday candidate `41e3450bc94d`
(`KUI-WinCE-Reader-Candidate-20NOV2026-2002-CST-41e3450.zip`) carries both
console-tested changes: the PVR stream pieces and this 16-reception window.
The owner's results, 21 Nov 2026 (CD music Off, no data recorded):

| Game | Result |
|---|---|
| Armada (US) intro movie | "back to how it was before, bad echo and slightly choppy" |
| Armada (US) gameplay | the earlier "bad lag in the gameplay" is "erased", "super smooth" |
| Super Producers (JPN) | the same echo; the picture relatively smooth until the last 5 or so seconds |
| KISS Psycho Circus (US), Virtua Cop 2 (JPN) | no problems |
| Worms Armageddon (US) | no problems, "maybe slight lag in the audio with the intro movies" |
| Super Runabout (US) | no problems |

Armada's intro echo is the fault the 64-reception window was chosen to cure
on the CD-music route, where about 22% of its receptions overran at 12.5 MHz.
Under that much contention the shorter window returns to the fast clock while
the bus is still held, so overruns, each followed by a fresh read of its
block, come about one and a half to three times as often (an estimate from
those rates; this build records no ring).
Its gameplay, like Super Producers' movie, gains from the faster reads.
Super Producers' echo was not reported on the slow-window test, where the
owner judged the movie fixed; the two runs differ in the probe and the
PVR-piece change, neither of which touches its reads.

What the rings show: overruns cluster. Super Producers' most common read on
the 16-reception build had two counted overruns and 32 blocks at 6.25 MHz (62
of 189 reads), and on the 64-reception build one or two: a regular count per
read, as from a periodic source. A fit of both sessions with overruns spread
evenly over receptions does not hold: in the same five games (32-sector reads)
the fitted rate at 6.25 MHz is 1.4 to 2.7 times higher with the 16-reception
window than with the 64-reception one, so overruns come more often just after
an overrun. (The ring counts an overrun that ends a reception: one in a
block's CRC bytes, or a second in the same block. One resumed mid-block, whose
block is then refused and read again, also starts or ends a window but is not
counted; on Armada's CD-music route these were 95 of 5,726,
[movie timing](armada-movie-timing-test.md#third-result-fast-check-reader-8cdbc0a351f8-9-nov-2026).)

Next: a window that grows only while overruns keep recurring. It starts at
16 receptions and doubles, up to the accepted 64, when an overrun at 12.5 MHz
follows the end of the last window within a few receptions (the bus is still
held), and falls back a step when one comes later
([adaptive-window test](wince-adaptive-window-test.md)).

