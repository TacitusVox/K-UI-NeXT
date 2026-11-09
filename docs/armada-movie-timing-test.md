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

The first console result used four rows (reader `689b6572affb`); later
timing readers show six, all supplied by the controller:

| Row | Meaning |
| --- | --- |
| MOVIE-MS SECTORS KB/S READS STATUS | Movie window length, sectors read, average KB/s, read requests, flags (1 movie seen, 2 game left the movie, 4 clock valid; expect 7) |
| CALL-MS IRQ-MS BUSY% PEND% PEAK% | Time inside K-UI's GD calls and interrupt, their share of the window, share of the window with a read outstanding, busiest one-second window |
| DMA-OK POLLED OVERRUN RESTART CRC-ERR | Movie-window stream counts: completed DMA receptions, programmed (CPU) receptions, unrecovered overruns, CMD18 starts, CRC failures |
| SLOW-OK SLOWOVR TOKBYTES YIELDS REPAIR | 6.25 MHz receptions completed and overrun (trial only), bytes clocked searching for data tokens, searches paused at a slice, in-place resumes |
| OVRTICKS MAXTICK RUN-MS GAPMAX LOWKB/S | CE clock steps (25 ms) with any overrun, most overruns in one step, longest run of consecutive steps with overruns, longest interval between polls, slowest one-second KB/s |
| CALLS IRQS SAMPLES HOT>50 1S-WINS | GD calls and interrupts in the window, poll samples, one-second windows at 50% or more, one-second windows |

CE's millisecond clock advances in 25 ms steps on this kernel, so gaps and
runs are coarse; totals over a minute are not.

## First result: reader 689b6572affb (8 Nov 2026)

US raw GDI, SCI, Background reader, CD Music On, Virtual VMU Off, GUI
`00d8ed67d9ad`, intro played once:

| Value | Result |
| --- | --- |
| Movie window | 140,500 ms; 31,077 sectors; 442 KB/s; 1,976 reads; status 7 |
| K-UI time | 38,623 ms in GD calls, 31,371 ms in the interrupt: 49% of the window |
| Calls / interrupts | 78,116 / 143,121 |
| Read outstanding | 100,225 ms (71%); longest poll gap 325 ms; one gap of 90 ms or more |
| One-second windows | 140; busiest 62%; 72 at 50% or more; slowest 352 KB/s |

K-UI's reader takes about half the CPU for the whole movie, so the movie
player is starved; the decision table points at the cost per SD block.
The raw image's 31,077 sectors are about 142,800 card blocks; the 143,121
interrupts are one per DMA reception.

Why each block is expensive (code and earlier counters): under continuous
clocking the card leaves one byte between CMD18 blocks
([2 Oct capture](evidence/sci-async-speed-cmd18-2026-10-02.md)), so the
normal per-block token search is short. The 4 Oct Armada counters had
about 22% of receptions overrun, one CMD12/CMD18 restart per overrun
(18,893 starts) and 6,524,422 token bytes, about 345 per restart: the
card's first-token delay after CMD18 (about 280 us at 12.5 MHz). Each
overrun therefore costs a restart, that wait and a 514-byte programmed
refetch, all with interrupts held off; an immediate 12.5 MHz DMA retry
almost always overran again (2.01 overruns per polled block before the
first-overrun fallback). The new rows count these for the movie window.

## Trial: 6.25 MHz after an overrun (reader 5d92a111b0fa)

One behavior change on the measured reader, built with
`KUI_ARMADA_CE_MOVIE_TIMING=1 KUI_ARMADA_CE_SLOW_AFTER_OVERRUN=1`: an
overrun at 12.5 MHz moves the next 64 receptions, the refetch included, to
6.25 MHz (1.28 us per byte); an overrun at 6.25 MHz returns to 12.5 MHz and
the old programmed refetch. Commands and token searches stay at 12.5 MHz.
Every block keeps its CRC check; blocks that lost a byte are still refused.
To fit, the trial reuses the two late-snapshot counters and drops the
background reader's call-history ring, which only the synchronous reader
displays. Payload CRC32 `27EC7CAE`;
`KUI-Armada-Slow-Refetch-Trial-08NOV2026-2249-CST-5d92a11.zip`.

Expected readings: if stalls are mostly shorter than 1.28 us, POLLED and
RESTART fall well below OVERRUN's first-attempt count, SLOWOVR stays small
and BUSY falls; if SLOWOVR approaches SLOW-OK, the stalls are longer and
the next step is to avoid the restart's token wait or the refetch itself.

## Second result: slow refetch, reader 5d92a111b0fa (8 Nov 2026)

Same route and GUI. Owner: no echo; the video is cleaner too; a slight
audio distortion remains. Gameplay not yet compared.

| Value | 689b6572 | 5d92a111 |
| --- | --- | --- |
| Movie window | 140,500 ms | 119,325 ms (the movie now runs at its own speed) |
| GD calls / interrupt | 38,623 / 31,371 ms | 3,867 / 19,985 ms |
| BUSY / PEND / busiest second | 49% / 71% / 62% | 19% / 75% / 35% |
| Seconds at 50% or more | 72 of 140 | 0 of 118 |
| Calls / interrupts | 78,116 / 143,121 | 20,865 / 142,662 |

Stream counts in the movie window: 136,693 DMA receptions and 2,640
programmed ones; 5,898 unrecovered overruns; 6,042 CMD18 starts; 10 CRC
failures; 1,995,011 token bytes (330 per start) with 6,212 yields; 73
in-place resumes. 98,520 receptions ran at 6.25 MHz and 2,646 of them
overran (2.6%), so most stalls lie between 0.64 and 1.28 us. Overruns
touched 2,797 of about 4,770 CE clock steps (at most 13 in one step,
longest run 450 ms). The slowest one-second window read nothing (the
player was not reading); the longest poll gap was 525 ms.

Estimated remainder of the 19%: the per-block CRC and bit reversal about
6%, other per-block work (interrupt entry, SCI turnaround, the copy into
the game's buffer, the controller's hooks) about 7%, restarts and
programmed refetches about 3%, other GD-call work about 3%.

## Trial: faster block check (reader 8cdbc0a351f8)

One change on 5d92a111 (`KUI_ARMADA_CE_FAST_TAKE=1`): the CRC table is
folded on bit 0 instead of bit 7 (the same 256 bytes;
table[x ^ 1] = table[x] ^ 0x1021), so on SH-4 one r0 value serves the
offset mask and the indexed load; each byte is a 12-instruction step with
no stack spill and the reversal is inline
([`sci_stream_check.h`](../src/loader/sci_stream_check.h)). About 93
instead of 118 instructions per word. Identical bytes and CRC: the host
suites pass, every one of the 16,777,216 state/byte steps matches a
bitwise reference, and `make test-sh4-check-words` runs the SH-4 assembly
on 4,000 blocks under qemu-sh4. The controller is byte-identical to
5d92a111's. Payload CRC32 `4E747363`;
`KUI-Armada-Fast-Check-Trial-09NOV2026-0611-CST-8cdbc0a.zip`.

Expected: IRQ-MS falls by roughly a tenth and BUSY to about 17-18%; the
stream counts stay similar.

## Decision

- **BUSY% high (about 20% or more), or many HOT>50 windows:** K-UI's per-block
  work starves the movie decoder. Cut the cost per SD block next.
- **PEND% near 100% with low BUSY%:** the SD path cannot deliver this movie's
  rate. KB/S is then the ceiling to raise.
- **Both low, with large GAPMAX or GAPS>90:** the game is waiting on something
  else, most likely a completion signal its driver expects.
