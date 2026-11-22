# CD-audio corpus of 20 November 2026: drivers, disc calls, volume and sound memory

The owner's `--cd-audio` startup-bundle run of 20 November (ten archives and a
sound-driver archive) holds, for 426 regular Dreamcast games with CD audio
tracks, the boot program, IP.BIN, the disc layout and every sound-driver file
the disc carries. Those files are the owner's and are not in the repository.
The record here is metadata only:
[the corpus table](evidence/cd-audio-corpus-2026-11-20/games.json) (title,
product, region, boot program size and SHA-256, track and audio-sector counts,
sound files by name, size and SHA-256, the driver family and build of each
instance, and the route the GUI gives the game) and
[the 193 distinct sound files](evidence/cd-audio-corpus-2026-11-20/sound-files.json)
by SHA-256, names, size, family and build text.

Everything below was read in the files (the boot programs disassembled as
SH-4 code at 0x8c010000, the drivers as ARM7 code at sound address 0) or
counted by scans over them, on 20 November. Nothing here is a console result.
Addresses are load addresses; driver addresses are image addresses.

## The corpus

| | Games |
| --- | ---: |
| Regions | J 148, E 147, U 127, UE 2, JUE 2 |
| Four or more high-density audio tracks | 183 |
| Driver families carried (file, embedded image, or the program's sound library) | MANATEE only 229, AUDIO64 only 91, both 82, AICADRV 15, AICADRV + MANATEE 4, AICADRV + AUDIO64 + MANATEE 2, Visual Concepts' own 3 |

Of the 193 distinct sound files, 28 are MANATEE-lineage drivers (magic
`SDRV`, a 0x20-byte header, the image loaded at 0), 18 AUDIO64 and 6 AUDIO64
hybrids (MIDIDA, ARMCOM), and 8 AICADRV; the other 133 are not drivers (voice
banks, the 16- and 24-byte `SOUND.BIN` files, DLLs). Twelve boot programs embed
a copy of a known driver; Floigan Bros (two) and Ooga Booga embed Visual
Concepts' own ARM code, Monaco Grand Prix 2 an early Manatee-lineage one, and
16 programs only an idle ARM stub.

## Routing with GUI 4d4dcff77494

The route each game gets with Virtual VMU Off: the GUI's production discovery
run on the host over each original program, with the GUI's route rules applied
(the GUI is not run on a console here):

| Route | Games |
| --- | ---: |
| Background reader, mapping proved | 89 |
| Background reader, the stage discovers | 61 |
| Standalone New (discovery fails) | 243 |
| Standalone New (SDK pool overlaps the game) | 28 |
| Refused | 5 |

Of the 89, 59 carry only MANATEE-lineage drivers, 14 only AUDIO64, 15 both and
one all three. A CD-music player in the Background reader reaches these games;
the standalone reader has no lease, auxiliary or worker.

Correction of 22 November 2026: the stage cannot discover what the GUI could
not. It runs the same predicates on the same bytes, and on the host none of
the 61 passes its first steps (58 stop at the Maple step, 3 at the program
extent): each launch ends at the reader's stop screen, as Plasma Sword's did
in the Test 84 re-run (`evidence/cdda-background-test-84/console-rerun-2026-11-22/investigation.md`).
Of the 89, the Tokyo Xtreme Racer discs (448 KiB pools) and four Buzz 2000
discs (pools smaller than the lease) are admitted to a lease their pools
cannot hold.

## How the games ask for CD music

- 423 programs reach the drive only through Sega's gdFs library; the other
  three (Floigan Bros, Ooga Booga) make no GD call. The library's request stub
  is `mov #0,r6; mov.l fn,r7; jmp @(0x8c0000bc)`.
- Every gdFs build can send PLAY (20, by track: start, end, repeat), PLAY2 (21,
  by FAD: start, end, repeat), PAUSE (22), RELEASE (23), SEEK (27), GETSCD (34)
  and REQ_STAT (36). None sends STOP (33), SCAN (32) or GETSES (35). It refuses
  PLAY with a start track below 3 and PLAY2 with a start FAD below 45150. It
  requests the command and runs EXEC once with interrupts masked, returns at
  once and never waits; all CD-DA requests share one handle, so a call made
  while the last one is still in the server fails and the game tries again
  (ChuChu Rocket up to ten times a frame, Bangai-O every frame).
- The reader's GET_VERS answer "1.10" (`src/core/retail_gd.c`) switches on the
  library's version-gated mode: after each PLAY, PLAY2 or PAUSE completes the
  library sends REQ_STAT itself and keeps the position word. On resume it
  sends RELEASE only if the position after the pause differs from the one
  after the play; if they are equal it sends the saved PLAY again and the
  track restarts.
- The library decodes the raw subcode (format 0) for track and position and
  does not read byte 1, the audio status. Today's constant there (0x15) does
  not matter to gdFs titles; the drive status and the positions do.
- Games mostly play one track (start equal to end) with repeat 15 for looping
  music and 0 for a single play. 237 programs reference play by track, 41 play
  by FAD, 119 the resume, 380 the drive status; about 72 compare the drive
  status with 3 ("playing"). Bangai-O keeps its CD volume at 0 until the status
  reads 3. MDK2 sends PLAY again whenever the handle is idle and the status is
  not 3. Ready 2 Rumble's CD stream ends when the status is not 3 after PLAY2
  completed. PenPen and Donald Duck wait without a time limit while the status
  is 3 after they stop the music. KOF Dream Match 99 resumes with PLAY2 at the
  position of the last raw-subcode reply.

So a reader that plays CD music must answer status 3 from the first CHECK
after PLAY completes and keep it through the game's data reads, answer 1 after
PAUSE and something other than 3 after a single play ends, and advance the
positions by 75 FAD a second while playing. Today the reader answers PLAY,
PAUSE and RELEASE as done, plays nothing, and its drive status is only ever 1
or 2: no game found hangs on that, but MDK2 sends PLAY over and over, Ready 2
Rumble's CD stream ends at once, Bangai-O stays muted and every resume
restarts its track.

## The sound drivers and the sound chip

MANATEE lineage (Y. Kashima, K. Suyama; 28 files):

- At start-up every build keys off all 64 channels, zeroes the DSP sends,
  clears the DSP, zeroes both CD input mixers (0x2040, 0x2044) and sets the
  master volume. It owns the monitor select (0x280C) and reads the play
  positions (0x2810, 0x2814) for its own voices.
- Older family (versions 1.01, 1998.08-1999.05): MIDI voices on channels 0-47
  first free from 0; one-shots and streams share a pool 48-63 first free from
  48; from 1.01.37 every stop-all keys off channel 63.
- Newer family (2.x, 1999.05-2000.07) and NAMIHEI: one-shot port q on channel
  48+q, stream port q on 56+q, so 62 and 63 are stream ports 6 and 7. A stop of
  all streams clears every stream channel's key bit, and after every command
  batch the driver rewrites register 0 of channel 63.
- The game's library fills the driver's bank table at 0x14000 with the
  address and size of every bank, stream buffer and effect area; the driver
  keeps its own code and work below 0x18000. The family can be told from
  sound memory (version bytes at 0x20, the state word at 0xF8).

AUDIO64 (Y. Kashima, Tom Miley; 18 builds and 6 hybrids):

- A port is a channel, 0-63, and the game's own program picks it (Toy
  Commander the lowest free, MDK2 round robin over 0-62 with its stream on
  63); the driver reserves nothing. A group stop with full masks rewrites
  register 0 of all 64 channels. The per-port poll owns the monitor select.
- The driver uses sound memory up to 0x30040 (a 128 KiB DSP ring at
  0x10000-0x3003F); above it is the game's heap, which ends at 0x1F4000 in 72
  programs and at 0x200000 in 49, with type-1 allocations from the top.

AICADRV (AM2, 8 builds): keys off all 64 channels at start and hands out all
64 in first-in first-out order, so any channel is given to a game voice within
64 voice starts; its DSP ring sits at the top of sound memory.

CD volume: no program writes the CD input mixers itself. The sd library sends
driver commands 0x35 (level 0-127) and 0x36 (pan); AUDIO64 has acCdSetVolume
and acCdSetPan; AICADRV its own commands. Every driver writes the result only
into 0x2040 and 0x2044, level in bits 8-11 and pan in bits 0-4, and zeroes
both when it starts, so CD audio is silent until the game sets a level. 147 sd
and 65 AUDIO64 programs change the level at run time (ChuChu Rocket fades out,
MDK2 follows its options menu). Games pan the two inputs both ways round (48 sd
and 25 AUDIO64 programs one way, 11 sd programs and the drivers' defaults the
other); KallistiOS names 0x2040 the left CD input
(`.deps/kos/kernel/arch/dreamcast/hardware/spu.c:307-343`).

Sound memory: the sd library holds the sound CPU in reset, clears all 2 MiB
(278 programs) and loads the driver at 0; everything else comes from the
game's own memory-layout files, which are not in the corpus. CRI's ADX and
Sofdec (284 programs) put up to eight 16 KiB stream buffers at fixed places in
the top 128 KiB (0x1DFE00-0x200000), so that area is in use in 101 of the 183
games with four or more CD tracks. No program writes channel registers from
SH-4 code. The libraries reach the sound bus with interrupts masked, suspend
running G2 DMA channels first, and wait on the G2 FIFO (0xA05F688C).

## What this means for a CD-music player

There is no pair of channels every driver leaves alone, and no part of sound
memory every game leaves alone. A player that works across the library has to
tell the running driver family at run time, take channels and a ring from
what that driver and its tables show to be free at that moment, check them
again before every write, give them up silently when the driver or the game
claims them, and follow the game's CD level and pan from 0x2040 and 0x2044.
Its play clock has to be its own (TMU0), because the drivers own the chip's
position registers.

## What only the console can answer

- Whether the SH-4 reads 0x2040 and 0x2044 back, which CD side feeds which
  input, the sense of the pan bit, and whether a channel at level 15 is as loud
  as the CD input at 15.
- How long a ring placed in a free gap survives a session, and how often a
  driver's stop-all or a game voice takes a player channel.
- The CPU and card time music costs beside the game's own reads.
- When a real drive completes PLAY, what it reports while seeking and after a
  single play ends (1 or 2), and whether PLAY2's end position is inclusive.
