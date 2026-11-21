# The unified WinCE runtime: one RAM pool, one reader (21 Nov 2026)

The owner asked for read-ahead, CD music and the virtual VMU to be designed
together rather than one at a time: the VMU switchable per game, and CD music
on by default for every CD-music game once it plays as well as CD music Off.
This page records the memory every Windows CE kernel surveyed leaves K-UI, the
one pool plan those services share, what is built (the generic module's pool
and read-ahead, both off by default), and the console tests the universal
reader needs. It changes no accepted module: `ce-boot` `7b216f2e1136`,
`ce-cdda-*`, `ce-armada-all-on` and the candidate `91b0540103f0` rebuild
byte-identical from this source with the default switches.

## Memory: what each kernel leaves

Windows CE takes its RAM from the ROM header (`ROMHDR`) of its kernel image:
`ulRAMStart` to `ulRAMEnd`, with the driver globals from `ulRAMEnd` to the
top of main RAM (`ulDrivglobStart` equals `ulRAMEnd`). NK builds its page
allocator from those words when it starts, so RAM above a lowered `ulRAMEnd`
is never given to the game. The CD-music modules already reserve their
512 KiB pool this way, gated on one exact header per kernel
(`include/kui/armada_ce_excerpt_pool.h`).

The data probe's RAM dumps of 19 kernels (sessions of 19 and 21 Nov 2026,
[evidence](evidence/wince-kernel-ram-layouts-2026-11-21.json), derived header
fields only) show three layouts:

| RAM end | Driver globals | Kernels | CE's RAM (ulRAMFree to ulRAMEnd) |
|---|---|---|---|
| `0x8cfd0000` | 192 KiB | 12 (CE 3) | 12.5 to 14.7 MiB |
| `0x8cff0000` | 64 KiB | 1 (CE 3, 1,200,128 bytes) | 14.7 MiB |
| `0x8cef0000` | 1,088 KiB | 6 (CE 2: Worms Armageddon, Super Producers and four more) | 10.7 to 14.1 MiB |

Every one of the 19 headers obeys one structural rule, and exactly one header
in each image does (`include/kui/ce_ram_pool.h`): `physfirst` is the image's
load address, `physlast` lies inside the image, `physlast <= ulRAMStart <=
ulRAMFree < ulRAMEnd <= 0x8d000000`, `ulRAMEnd` is page aligned, and the
driver globals fill the RAM above it. The probe's own `ECEC` method found no
CE 2 kernel; this rule finds all six. The pool session of 21 Nov 2026 added a
20th kernel (Worms World Party's), which obeys the same rule
([evidence](evidence/wince-pool-session-2026-11-21.json)).

Games leave much of that RAM unused. The session-1 dumps' page counters
(unreviewed, see the [roadmap](wince-roadmap.md)) put the RAM still free at
the end of a launch at about 3.5 MiB (Urban Chaos, the least) to about 12 MiB.
A pool of a few hundred KiB is small against either figure; the stage still
refuses one that would leave CE less than 4 MiB above `ulRAMFree`.

### Why real CD music needs no RAM and K-UI's does

On a GD-ROM the drive plays CDDA itself: its audio goes from the drive to the
AICA's CD input, and neither the CPU nor main RAM carries it. K-UI has no
drive. It reads the track's PCM from the SD card and feeds it to a DirectSound
buffer in the game's process (352,768 bytes in the Armada module), so it needs
a ring in RAM for the card's bursts (the 512 KiB pool) and a share of the card
bus between the game's reads and the music's refills.

## One pool plan

One lowering of `ulRAMEnd`, before NK runs, takes one pool for all of K-UI's
WinCE services; each service owns a fixed part:

| Part | Size | Status |
|---|---|---|
| Read-ahead | 64 KiB (128 card blocks) | built in the generic module (`KUI_CE_POOL_KIB=64 KUI_CE_READAHEAD=1`), off by default |
| CD music ring and state | 512 KiB | in the CD-music modules today, each with its own per-kernel pool |
| Virtual VMU | the VMU helper's reservation | in `ce-armada-all-on` today (`0x8cfc4000`, inside its 512 KiB pool, about 9 KiB free) |

The universal reservation (`KUI_CE_POOL_KIB`, `src/loader/retail_stage.c`)
finds the header by the rule above, lowers `ulRAMEnd` by the pool, checks the
84 header bytes, clears the pool and has the relay prove after bootstrap 2
that the executable (with its patched word) and the pool are unchanged. A
kernel whose header the rule does not find keeps all its RAM and the launch
goes on without the pool. The one kernel whose RAM ends at `0x8cff0000` keeps
all its RAM: its pool would overlap the stage's own stack while the stage
runs.

## One reader, one scheduler

The background reader already serves the game's requests from the card by
interrupt and by the game's own GD calls. Read-ahead adds one state to it.
(Console-tested 21 Nov 2026 without a visible gain and not adopted: the
order below stands for CD music and the VMU without it.)
The order of claims on the card bus, as the CD-music and VMU services join it:

1. **The game's request.** Always first. Its first blocks may already be in
   the pool; the rest come from the stream.
2. **CD music refills** (CD-music modules): the music's ring is refilled
   between the game's blocks, as the stream bus does today.
3. **Read-ahead** (only while no request is active and, once combined, only
   while the music's ring is above its low mark).
4. **The VMU's loans** (an idle reader lends the card bus to the VMU helper).

Read-ahead as built (`src/loader/retail_async.c`, `KUI_RETAIL_CE_READAHEAD`):

- After two sequential reads (the second beginning where the first ended),
  the reader goes on streaming from the block after the last read into the
  pool while no request is active, to the end of that extent or the pool. The card's CMD18 stream is not stopped: the next read takes the
  blocks that arrived from RAM, then the stream's own block in flight.
- A 2352-byte track's next sector usually begins in the last block the read
  fed; the stream still keeps that block, so the pool starts with it.
- Only blocks whose CRC16 checked are stored, and a card block never changes,
  so what the pool holds stays valid after a read elsewhere. Any failure
  (CRC, overrun, a token not found) only stops the read-ahead; the next read
  fetches that block itself, with the reader's usual retries.
- Only by DMA. A fetch while channel 1 is not free receives by programmed
  transfers with interrupts masked; the read-ahead stops instead.
- The stage publishes the pool's bounds from the relay, after its checks, so
  nothing can fill the pool while bootstrap 2 runs.
- Its resident keeps a 1 KiB stack, as the writable VMU reader does (limit
  `0x8c00dc00`); the call-graph worst case is 416 of 976 bytes.

What the timing sessions showed it for: the read-limited streams ask for
their next read about 24 ms after a 67 to 82 ms read ends (Armada's intro,
Maximum Pool, Worms' intro, about 70% of Super Producers' movie). That gap is
what read-ahead fills.

## Built, designed, not built

| Item | State |
|---|---|
| Universal pool (generic module) | built `c6745ab2`, host-tested on the surveyed layouts; console-tested 21 Nov 2026 on 7 kernels: reserved on all, untouched by CE, reads unchanged ([result](wince-pool-readahead-test.md#result-of-test-1-21-nov-2026)) |
| Read-ahead (generic module) | built `1a5f4677`, host-tested (306,260 checks with every existing CE case); console-tested 21 Nov 2026: no visible difference on Maximum Pool, RE2 and Super Producers; not adopted, off by default ([result](wince-pool-readahead-test.md#result-of-test-2-21-nov-2026)) |
| Adaptive slow window in the CD-music modules | built `52a94612` (the same switches, default off); console-tested 21 Nov 2026: movies fixed, gameplay with CD music On still a tiny bit worse than Off ([record](wince-cdda-adaptive-test.md)) |
| Where CD music On costs gameplay | Armada gameplay timing, two runs (music, and the music never queued), build `32b3bbb33e39`: console-tested 21 Nov 2026, the music worker's urgent programmed receptions ([record](wince-cdda-play-timing-test.md)) |
| The CD music worker fix | programmed receptions only in an emergency (`KUI_CE_CDDA_EMERGENCY_PIO`, source `06341c5a`), test build `06341c5a3681` delivered 21 Nov 2026 ([record](wince-cdda-emergency-pio-test.md)) |
| Read-ahead in the CD-music modules | designed: claim 3 above, yielding to the music's refills |
| CD music On by default | designed: a GUI default per CD-music game, once that game's CD-music module matches CD music Off |
| Virtual VMU switchable per game | exists for Armada's family; every other family needs its own profile ([VMU families](wince-vmu-driver-families.md)) |

## Testing the universal reader

Each tier is one behaviour change against a named baseline, with the module
path, build and payload CRC recorded. CD music Off and VMU Off unless a tier
says otherwise. Armada with CD music Off runs `ce-probe.kui` whenever that
file is installed ([routing](wince-gui-routing.md)), so it is moved aside for
these tests.

**Tier 0: the pool alone, every kernel.** A timing build of the adaptive
reader with the pool (`KUI_WCE_PROBE=2 KUI_CE_SLOW_WINDOW=16
KUI_CE_SLOW_ADAPT=2 KUI_CE_POOL_KIB=64`). As many WinCE games as possible,
each to a minute of play. Each launch's record shows whether the pool was
reserved (and for which header) and the reads' timing, which must match the
adaptive session's (D5) for the same games. A game that fails here fails
because CE lost 64 KiB.

**Tier 1: read-ahead** (run 21 Nov 2026 on three games, no visible
difference; not adopted). The read-ahead candidate on the read-limited and the
low-RAM games: Maximum Pool (US), Armada (US) intro and gameplay, Super
Producers (JPN) to the movie's end, Worms Armageddon (US) intros, Resident
Evil 2 (US) intro, Super Runabout (US) (the heaviest card traffic), KISS Psycho
Circus (US) and Virtua Cop 2 (JPN) (video-memory streams), Urban Chaos (US)
(least free RAM), NFL QB Club 2000 (US), Sega Rally 2 (JPN) (the only line-5
game). Better, the same or worse than the candidate `91b0540103f0`.

**Tier 2: CD music On**, for each CD-music game (Armada, BAM4, Worms, WWP),
once its module carries the reader changes: the music plays, gameplay and
movies as with CD music Off. Then CD music On with VMU On for Armada.

**Tier 3: the virtual VMU per controller family**: format, save, power off,
cold boot, load; and the same with CD music On where the game has it.

**Combinations** (repository rule), each a separate result for every game
that supports them: neither service, read-ahead only, CD music only, VMU
only, and combined. A service switched off does not prove its RAM released.
