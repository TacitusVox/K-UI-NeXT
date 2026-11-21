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
