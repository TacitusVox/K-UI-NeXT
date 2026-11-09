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
normal Armada reader rebuilds identical except its build ID, and the Worms,
Bust-A-Move and Worms World Party readers still pass their audits.
`KUI_ARMADA_CE_MOVIE_TIMING_PARTS=low|high` builds one half alone.

## Why the first two timing readers stopped

`df69c40` and `5cd7014` both stopped at Entering Windows CE, with a fresh
published GUI and with the owner's working GUI `00d8ed67d9ad`. The GUI was
not the cause. In both, eleven calls into the low reader's SD delivery chain
(`_place`, `_drain`, `_piece`, `_deliver`) landed four bytes into the callee,
skipping its first register pushes, so the callee popped registers it never
saved. The counters moved a literal word that relaxation deletes to just
before those functions, and `sh-elf` ld 2.45.1 left section+addend calls to
them stale. It is the same fault as Chat's `67b0df9` VMU-Off reader: its
`4d893b3` repair changed exactly eleven such displacements by four bytes and
booted.

When the low-reader counters are built in, the low reader's LTRANS is
assembled with `--renesas` (as the WWP adapter already does) so named callee
relocations relax correctly, the link keeps its relocations (`-Wl,-q`), and
[`tools/check_relaxed_calls.py`](../tools/check_relaxed_calls.py) fails the
build if any real `bra`/`bsr` enters another function mid-way. Reader
`689b6572affb` (payload CRC32 `B285267E`): 323 branch sites, none affected;
it differs from `5cd701429f0a` only in those eleven displacements and the
build ID. `make test-check-relaxed-calls` covers the gate.

The same gate finds eight such calls (`_step`, `_file_read`, `_data_header`,
`_read_run`) in the published synchronous SCI resident embedded in the
Armada CD-music reader. That resident serves only the Standard reader on
SCI; the Background reader (`resident-scia`) has none. It is unchanged here
and not yet console-checked; a Standard-reader WinCE CD-music launch would
be expected to fail.

## GUI

Use the owner's working GUI `00d8ed67d9ad` (from
`KUI-Armada-VMU-Off-Fix-07NOV2026-1325-CST-4d893b3.zip`) and replace only
`ce-cdda-armada.kui`; with that GUI, Virtual VMU Off selects this reader.
`KUI-Armada-Movie-Timing-08NOV2026-2208-CST-689b657.zip` is that set with
only this reader changed.

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
