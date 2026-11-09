# Armada movie timing test

The opening movie's audio repeats because Armada's movie player sometimes
falls a whole 16 KiB ring behind (the November 7 probe: WRITTEN − CONSUMED was
exactly 162 rings plus the queue, and all 1,729 movie reads succeeded). This
test measures why the player falls behind. It changes no game code and no
audio path; it only counts where time goes while the movie streams.

## What the build adds

- The low CE reader adds the TMU0 ticks spent inside its own GD calls and
  inside its SCI interrupt to four words at the end of its own region; the
  controller reads them through its attached bus. TMU0 drives CE's tick and
  is only read. (The first build kept them in the controller's reserved
  tail, which the stage requires to stay zero through bootstrap; that slot
  is no longer used.)
- The controller samples at each of the game's GD EXEC/CHECK polls, through a
  same-named wrapper so the audited controller entry keeps its exact bytes.
  The window opens at the first request inside `MOVIE2.SFD` (LBA 434,063 to
  463,779) and closes at the last.
- The A+B+X+Y+Start report shows four timing rows in place of the CD music
  context, playback, buffer and cursor rows, in decimal digits. The CD music
  history pages are dropped from this build only, to fit the low reader.

Built from published `e20eae43` with `KUI_ARMADA_CE_MOVIE_TIMING=1`; the
normal Armada reader rebuilds byte-identical and the Worms, Bust-A-Move and
Worms World Party readers still pass their audits.

## GUI

Use the owner's working GUI `00d8ed67d9ad` (from
`KUI-Armada-VMU-Off-Fix-07NOV2026-1325-CST-4d893b3.zip`) and replace only
`ce-cdda-armada.kui`; with that GUI, Virtual VMU Off selects this reader.
The first delivery paired the reader with a fresh build of the published
GUI. That GUI predates the November 6 23:48 WinCE track-map change (its
strings lack the "Windows CE background map ... tracks and their data
extents" handling), and the console stopped at Entering Windows CE. The
published GUI is not a valid baseline for WinCE on this card.

## Report rows (decimal)

| Row | Meaning |
| --- | --- |
| MOVIE-MS SECTORS KB/S READS STATUS | Movie window length, sectors read, average KB/s, read requests, flags (1 movie seen, 2 game left the movie, 4 clock valid; expect 7) |
| CALL-MS IRQ-MS BUSY% CALLS IRQS | Time inside K-UI's GD calls and interrupt, their share of the window, and counts |
| PEND-MS PEND% GAPMAX GAPS>90 PENDGAP | Time a read was outstanding and its share; longest interval between the game's polls; intervals of 90 ms or more; longest interval with a read outstanding |
| 1S-WINS PEAK% HOT>50 LOW-KB/S SAMPLES | One-second windows, the busiest window's K-UI share, windows at 50% or more, the slowest window's KB/s, poll samples |

CE's millisecond clock advances in its tick steps, so gaps are coarse;
totals over a minute are not.

## Decision

- **BUSY% high (about 20% or more), or many HOT>50 windows:** K-UI's per-block
  work starves the movie decoder. Cut the cost per SD block next.
- **PEND% near 100% with low BUSY%:** the SD path cannot deliver this movie's
  rate. KB/S is then the ceiling to raise.
- **Both low, with large GAPMAX or GAPS>90:** the game is waiting on something
  else, most likely a completion signal its driver expects.
