# WinCE CD music: restart the music's card stream where it stops (21 Nov 2026)

**Status: console-tested on Armada 21 Nov 2026 ([result](#result-21-nov-2026)):
the repeats are no longer clear; gameplay, the other games and the measured
run not yet reported.** One behaviour change
on top of the worker's emergency-only programmed receptions
([`06341c5a3681`](wince-cdda-emergency-pio-test.md)), which made Armada's
gameplay smooth but left its music repeating
([result](wince-cdda-emergency-pio-test.md#result-21-nov-2026)).

## The change (source `1ba2fa7e`)

The music's card blocks arrive in a DMA chain: each completion interrupt
takes the block and starts the next. The chain stopped, with no DMA armed,
at every restart made inside an interrupt, a GD call's idle visit or a data
block's yield:

- after CMD18 the card's data token takes about 330 bytes, more than the
  256-byte token slice those calls allow;
- after an overrun the music refetched the block by programmed transfers.

The music then waited for the worker's next visit (73 ms apart on average in
the gameplay timing). The worker's urgent programmed receptions had covered
those stops, at the cost of the gameplay lag.

`KUI_CE_CDDA_CHAIN=1` (Makefile.armada-stream and Makefile.armada-vmu-a2;
controller only, [transport](../src/loader/armada_ce_stream_transport.c)):

- a restart's token search goes on for up to eight more 256-byte slices
  inside the pump that needs it, so its DMA starts there;
- an overrun at the fast clock is refetched by DMA at 6.25 MHz while the
  reader's slow window is armed, as the game's reads are; an overrun at the
  slow clock, and a second failure at a block, keep the programmed refetch;
- the worker keeps one slice per visit (its own loop continues a search).

The controller reads the reader's slow window (`shared.stream.slow`, read
only); the combined module's controller is compiled with the reader's
slow-window switches for that field, whose place is the same in every build.

The default (0) builds every accepted module unchanged: the five CD music
modules rebuild byte-identical to `7b216f2e1136`, and with the emergency
switch alone to the delivered `06341c5a3681` set. The new switch is used
only in the transport, so the card readers do not change.

## Test build `1ba2fa7e7040`

The five CD music modules with the adaptive window, the emergency-only
programmed receptions and this change, and the Armada gameplay timing with
both changes:

| Module | Route | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-armada.kui` | Armada, CD music On, VMU Off | 135,412 | `830BD7EA` | `f39ed7df4863297b...` |
| `ce-cdda-bam4.kui` | Bust-A-Move 4, CD music On | 135,396 | `23A93E11` | `ac9aa157f0a4a6d6...` |
| `ce-cdda-worms.kui` | Worms Armageddon, CD music On | 136,324 | `0074D23C` | `3063469fb42e93fd...` |
| `ce-cdda-wwp.kui` | Worms World Party, CD music On | 136,036 | `F219336E` | `991e4b9ea745cdc8...` |
| `ce-armada-all-on.kui` | Armada, CD music On, VMU On | 206,428 | `8C21F910` | `2e3a79ed891002b4...` |
| `MEASURE/ce-cdda-armada.kui` | the gameplay timing with both changes (fixed 64-reception window) | 138,604 | `A2F1FE09` | `79b383abbf40862b...` |

Delivered: `KUI-WinCE-CD-Music-Chain-Fix-21NOV2026-1415-CST-1ba2fa7.zip`
(SHA256 `b932e05403cd410446097d1d2b9c6311e1302fd541c9712d547bde164f8ec134`),
with the `52a946122537` set (before the lag fixes) in `IF-NEEDED/`. Full
hashes are in its `INVENTORY.json`.

Gates on the exact builds: the stream excerpt checker (each title's profile,
`--cdda-vmu` for the combined module), the layout checker and the call
checks pass; stack worst cases unchanged (512 to 516 of 2,000; 504 of 976;
the timing build 656). Host: the transport test runs in three
configurations (`test-armada-ce-stream`): default, emergency only, and
emergency with the chain (continued token slices in a generic pump and an
interrupt, the eight-slice bound, the worker's single slice, the DMA
refetch while the window is armed, and the programmed refetch at the slow
clock and on a second failure).

## Result (21 Nov 2026)

The owner, Armada (US), CD music On, Virtual VMU Off, the normal module
`1ba2fa7e7040`: "Can't tell if the music is repeating or that's just the
nature of the song" (the last build's repeats were plain), and at one point a
different song was playing and it changed abruptly back to the first song.
The measured run was not made.

The report's first page:

| Row | Values |
|---|---|
| GD BIND SOURCE GAPS WORKER | 31 · 0 · `044E6B00` (72,248,064 bytes, 6.8 minutes of music) · 0 · 1 |
| AUDIO WHY NODE VALUE EXPECT DETAIL | all 0 (no fault) |
| STREAM ATTEMPT PLAY REFUSED HR | 2 (playing) · 20 · 17 · 3 · 0 |
| BUFFER CAPS BYTES VOICES CRC | `0103D2AC` · `1E8` · `56200` · 13 · `EE6CA949` |
| CMD FIRST LAST PLAYCUR WRITECUR | 20 (play) · track 4 · 4 · `14F00` · `11B00` |

- No model underrun (GAPS 0) and no fault over 6.8 minutes of music.
- **The switch back to the first song was the game's request.** The GD
  service accepts a CD audio command at once and keeps no play state, so it
  never reports a song's end to Armada; the player changes songs only when
  the game's command arrives, and the last command was PLAY track 4.
- The three refusals change no song: the player refuses a start while the
  game holds all but one of its sixteen voices and retries at its next
  visit, and refuses a command the model cannot apply.

Not answered yet: the gameplay with this build, the other CD music games,
and the measured run (whether the music's reserve stays above the 155 ms CE
mixes ahead).

## Questions for the console test

- Armada with CD music On (VMU Off and On): no gameplay lag and no music
  repeats? Then BAM4, Worms and WWP.
- The measured Armada run against gameplay timing run 1: POLLED, URGENT,
  OVER8MS, MAX-US and WORK-MS should fall; LOW-MS shows whether the music's
  reserve now stays above the 155 ms CE mixes ahead.
