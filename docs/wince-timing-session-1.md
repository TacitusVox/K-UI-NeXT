# WinCE timing session 1 (20 Nov 2026): results

The owner ran the timing probe ([wince-data-probe.md](wince-data-probe.md#timing-session-kui_wce_probe2))
on eight launches: build `5c1cdd89387b` from
`KUI-WinCE-Timing-Session-20NOV2026-0158-CST-5c1cdd8.zip`. It reads exactly as
the installed `7b216f2e1136` does. CD music and Virtual VMU were Off, no VMU was
inserted at any time, and every launch was a cold boot. All eight slots hold a
complete dump. Each ring kept the last 190 reads, which cover the last 19 to
58 s before A+B+X+Y+Start; nothing before that window is visible. This record
keeps derived values only. An analysis of the rings was checked by two
reviewers who recomputed its numbers; their corrections are applied here.

| Slot | Game | Owner's note |
|---|---|---|
| 1 | Resident Evil 2 (US), run 1 | "there weren't many issues" |
| 2 | Resident Evil 2 (US), run 2 | "WAY worse" |
| 3 | Sno-Cross (US) | largely as on 19 Nov (movie hangs, audio continues) |
| 4 | Super Producers (JPN) | largely as on 19 Nov (movie hangs, slightly) |
| 5 | Maximum Pool (US) | largely as on 19 Nov (slow openings and menus) |
| 6 | Urban Chaos (US) | largely as on 19 Nov (the car lurches) |
| 7 | Wild Metal (US) | "load time wasn't bad at all" (it was long on 19 Nov with VMUs in A1 and A2) |
| 8 | Bust-A-Move 4 (US) | the smooth comparison |

## What the data shows

1. **No stuck read.** Within each window every read ended, none failed or was
   replaced, and none was open at the press. No launch had a refused command.
2. **One clean rate.** A read with no overrun takes 0.86 ms + 1.61 ms per
   2,048-byte sector (about 1,244 KiB/s) in every game; clean 64 KiB reads take
   51.5 to 53.2 ms. The images hold 2,352-byte sectors: about 4.6 card blocks
   per sector, 147 per 64 KiB read.
3. **Overruns come from the bus, not from the CPU.** The reader receives by
   DMA channel 1 from the SCI's one-byte receive register
   (`src/loader/sci_stream.c`); an overrun means that channel was kept off the
   bus for longer than one byte (0.64 us at 12.5 MHz). Masked GD calls and the
   reader's interrupt do not cause them: PIO reads, received inside masked
   calls, overran least (none in Sno-Cross, Super Producers and Bust-A-Move 4).
   What holds the bus is not recorded.
4. **They depend on the phase, not the command.** They are dense in paced
   streams (movies, animations: 22 to 34 per 1,000 card blocks, 66 to 68% of
   blocks at 6.25 MHz), rare after them, and absent from large single loads
   (Wild Metal's 512-sector and Maximum Pool's 646-sector reads) and from all
   of Bust-A-Move 4. Within one stream `DMAREAD` and `DMAREAD_STREAM_EX`
   overrun alike. In Maximum Pool and Sno-Cross a new stream's first two or
   three reads are clean.
5. **What they cost.** Slower reads lost 34.2 s of the 115.3 s that reads were
   open (12.8% of the windows): 86% from the 6.25 MHz window that follows each
   overrun (0.33 ms per slow block), 14% from the overruns themselves (1.1 to
   1.4 ms each). Read latency in the busy phases is 1.4 to 1.8 times clean (RE2's
   movie: median 91 to 92 ms against 52).
6. **Frame-locked loops feel it directly.** Super Producers' movie, Maximum
   Pool's animations and RE2's catch-ups ask for the next 64 KiB one or two
   frames after the last read ends, so read latency sets their rate. Maximum
   Pool's animation files took 7.5 and 7.7 s against 5.5 and 5.7 s at the
   clean rate (1.36 times); Super Producers' stream got 636 KiB/s against 769.
7. **RE2's halts are not slow reads.** Its movie is a run of 64 KiB `DMAREAD`s on
   the 16.64 ms frame grid at about 341 KiB/s, with margin: the slowest took
   112.6 ms against about 187 ms of movie per read. Each window has two 2 to 3 s
   stalls in which the game asks for almost nothing, and each begins after an
   ordinary read with nothing outstanding. The shortfall pattern favours the
   player pausing consumption then (inferred; what stops it is not visible).
   The catch-ups that follow run at 539 to 570 KiB/s because of overruns, about
   twice as slow as clean reads would make them.
8. **RE2 run 1 against run 2.** The same service within the windows (median
   91.0 against 92.2 ms, about 5 overruns per read). The rings cover adjacent
   stretches of the movie, so they never compare the same scene; run 2's worse
   episode is not placed. Before its ring, run 1 had twice run 2's overruns per
   sector.
9. **Reader CPU does not separate the smooth game.** Reader time while a read
   was open: Bust-A-Move 4 13.2% of its window, RE2 10.4 and 10.9%, Super
   Producers 15.3%, Maximum Pool 14.9%. Bust-A-Move 4 alone had no overruns and
   only short PIO reads.
10. **Masked PIO reads.** The only near-continuous stretches of reader CPU are
    long `PIOREAD_STREAM_EX` reads, received in masked GD calls of at most 3.5 ms
    with about 28 us between them: Sno-Cross 601.5 ms (96.6% in calls), Super
    Producers 295.1 ms (96.3%), Urban Chaos 295 ms (about 95%). Bust-A-Move 4's
    are at most 102 ms.
11. **Wild Metal's load was not the reader.** With no VMU its reads ran at the
    clean rate (512-sector reads at 1,242 KiB/s) and no read was open in its
    8.3 s and 3.7 s gaps. On 19 Nov, with VMUs in A1 and A2, the same disc work
    reached the combination 21.2 s later; the counters put the reading time at
    about 18 s on both days, so the extra time was probably not spent reading.
    That fits the owner's VMU explanation; it is not proven.

## Per game, where the data leaves each complaint

- **Sno-Cross:** a 138-read movie stream at 235 KiB/s, served with large idle
  margins (median 80 ms per read, then 200 ms idle; no request gap over
  609 ms). It stopped 4.9 s before the press after a clean read; the game then
  read other files, ending with the 601.5 ms masked PIO read 2.1 to 1.5 s before
  the press. The freeze is not placed.
- **Super Producers:** a frame-locked stream that took data as fast as the
  reader gave it (636 KiB/s, 1 to 4 overruns per read, exactly 2 in 107 of 186).
  If the movie needs more than that, overruns explain its small hangs; its
  bit rate is not known. The stream ended on schedule 2.5 s before the press,
  followed by the 295 ms masked PIO read.
- **Maximum Pool:** its animation streams ran 1.36 times slower than clean reads
  would allow, which fits "slow during animation sequences" if the animations
  play as their data arrive (not recorded; the files fit in its free RAM). At
  the press the reader had been idle for 0.64 s, so part of the slowness is the
  game's own.
- **Urban Chaos:** a load, a 36 s quiet stretch with one small read every 4.5
  to 5.9 s, a second load, then bursts of 154 to 166-sector reads at 1.4 to 1.6
  times clean. One periodic read took 180 ms with 109 overruns, 78% of it reader
  time (73 ms in masked calls). The lurch is not placed.
- **Bust-A-Move 4:** 8-sector `DMAREAD`s and short PIO stream reads, no overruns
  in either session; its window holds no movie-shaped stream, so it is not yet
  a control for a streamed movie.

## Not answered

- Where each freeze, halt or lurch falls on the ring's clock: the ring records
  no picture, sound or controller event, and its end is the BIOS menu call, not
  the combination's first button.
- Whether the movie streams' stops 1.7 to 4.9 s before each press are natural
  ends or skips caused by the combination's first button.
- What holds DMA channel 1 off the bus during streams; Super Producers' regular
  2 overruns per read suggests a periodic source.
- Whether a shorter 6.25 MHz window would help. That depends on how the bus
  hold-offs are spaced over 1 to 40 ms; a model that assumes independent
  overruns does not fit Super Producers. Not shown.

## Next, in order

1. Owner checks, no build: whether Start alone or A alone skips the RE2,
   Sno-Cross and Super Producers movies; per freeze, the scene, seconds into the
   movie, how long it lasts and what the sound does; the original discs, if at
   hand, at the same spots; Wild Metal timed from Start to the level with VMUs in
   A1 and A2, with none, and with each alone.
2. A measurement build, one change against `5c1cdd89387b`: the time of the
   combination's first button and of a mark button; a ring of overrun events
   with the DMA and bus state; GD calls between reads and CD audio commands; the
   longest interrupt entry per read.
3. Reader changes after that, one per console test: the 6.25 MHz window's
   length; the PIO path, if a freeze lands in a long masked PIO read;
   read-ahead for paced 64 KiB streams, which needs RAM the CE route may not
   have.
