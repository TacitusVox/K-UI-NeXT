# WinCE CD music: the worker's programmed receptions as an emergency only (21 Nov 2026)

**Status: console-tested 21 Nov 2026 ([result](#result-21-nov-2026)):
Armada's gameplay is smooth but its music repeats; Worms plays music and
gameplay fine.** One behaviour change to the five CD music modules, the fix
for the gameplay lag the Armada timing found
([result](wince-cdda-play-timing-test.md#result-21-nov-2026)).

## The change (source `06341c5a`)

While the music is urgent (at most one second of fresh PCM, until 1.5), the
source worker received the music's card blocks by programmed transfers, one
512-byte block per pump: up to 32 per playing step and 92 per visit while
filling before playback, at priority 2, above the game's normal threads.
Those steps were the lag: 236 of 1,437 took 8 ms or more (239 ended urgent),
the longest 173 ms, and 8,530 of the music's 27,371 card blocks were
programmed, while the music's reserve never fell below 1,012 ms.

`KUI_CE_CDDA_EMERGENCY_PIO=1` (Makefile.armada-stream and
Makefile.armada-vmu-a2; controller only) keeps urgency and its longer card
grants (75 blocks instead of 8 while a game read waits), but the worker
receives by programmed transfers only while the music plays with at most
half a second of fresh PCM, until it has one second again
([transport](../src/loader/armada_ce_stream_transport.c)). Otherwise its
visit starts, or restarts, the reception by DMA and ends, and the interrupt
chain carries the job, as for the game's reads; filling before playback is
never an emergency. The refetch after an overrun and the retries keep their
programmed fallback.

The default (0) builds every accepted module unchanged: the five CD music
modules rebuild byte-identical to `7b216f2e1136`, the adaptive set to
`52a946122537`, and the gameplay timing run 1 module to its delivered bytes.

## Test build `06341c5a3681`

The five CD music modules with the adaptive window (`KUI_CE_SLOW_WINDOW=16
KUI_CE_SLOW_ADAPT=2`, as `52a946122537`) and this change; against
`52a946122537`, every card reader is byte-identical except its build ID (and
World Party's, also the linked address of the controller's drive status
word, which moved with the controller), and the controller grows by 96 bytes
(64 in the combined module).

| Module | Route | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-armada.kui` | Armada, CD music On, VMU Off | 135,348 | `91A2A1AE` | `a29ddda2cbf634a1...` |
| `ce-cdda-bam4.kui` | Bust-A-Move 4, CD music On | 135,332 | `69175F62` | `adc682ab1b643703...` |
| `ce-cdda-worms.kui` | Worms Armageddon, CD music On | 136,260 | `D841F413` | `7a09c4f849f8e3af...` |
| `ce-cdda-wwp.kui` | Worms World Party, CD music On | 135,940 | `D87C83F5` | `ea9ced36057325cd...` |
| `ce-armada-all-on.kui` | Armada, CD music On, VMU On | 206,332 | `EE540C64` | `cf675aac47173824...` |
| `MEASURE/ce-cdda-armada.kui` | the gameplay timing with this change (fixed 64-reception window, as run 1) | 138,508 | `C9D6F9C6` | `7a1e8c7348da3bca...` |

Delivered: `KUI-WinCE-CD-Music-Worker-Fix-21NOV2026-1345-CST-06341c5.zip`
(SHA256 `c80bb79a35fe104ae401b768b075ffd2900c6af26de6c864f2c82f85e04ec9f9`), with
the card's `52a946122537` modules in `IF-NEEDED/`. Full hashes are in its
`INVENTORY.json`.

Gates on the exact builds: the stream excerpt checker (with each title's
profile, and `--cdda-vmu` for the combined module), the layout checker and
the call checks pass; the card readers end at their limits as before;
stack worst cases unchanged (512 to 516 of 2,000; 504 of 976; the timing
build 656). Host: the transport and player tests also run with the switch
(`test-armada-ce-stream`): the floor's hysteresis, an urgent worker
finishing a 75-block job by DMA alone, programmed reception below the
floor, and the player publishing the floor with its priority, never while
filling.

## Questions for the console test

- Every CD music game with CD music On: does the gameplay lag go, and does
  the music play without cuts or skips (the risk of this change)?
- Armada, the measured run against the gameplay timing's run 1: OVER8MS,
  MAX-US, POLLED and WORK-MS should fall; URGENT may stay; LOW-MS shows how
  low the music's reserve now runs.

A game that plays as well with CD music On as Off is ready for CD music On
by default (a GUI change of its own).

## Result (21 Nov 2026)

The owner: Armada (US), CD music On, Virtual VMU Off on `ce-cdda-armada.kui`
`06341c5a3681`: smooth gameplay, but "a lot of audio repeats", possibly
during loading. Worms: music and gameplay fine. The owner recalled the same
trade-off from earlier builds: fixing the gameplay broke the CD music, and
the reverse. The measured Armada run was not made.

The report's first page (A+B+X+Y+Start), the fixed module:

| Row | Values |
|---|---|
| GD BIND SOURCE GAPS WORKER | 9 · 0 · `011F8F00` (18,845,440 bytes of music) · 0 · 1 |
| AUDIO WHY NODE VALUE EXPECT DETAIL | all 0 (no fault) |
| STREAM ATTEMPT PLAY REFUSED HR | 2 · 7 · 7 · 0 · 0 |
| BUFFER CAPS BYTES VOICES CRC | `0103DE9C` · `1E8` · `56200` (352,768) · 15 · `EE6CA949` |
| CMD FIRST LAST PLAYCUR WRITECUR | 20 (play) · track 4 · 4 · `1BD80` · `22800` |

1. **The model never saw the ring run out**: GAPS counts its underrun
   recoveries and is 0. Its underrun test compares the play cursor with the
   fresh PCM written.
2. **CE mixes ahead of the play cursor**: the write cursor runs `0x6A80`
   bytes (27,264, about 155 ms) past it. Music written later than that
   lookahead is mixed as last lap's ring contents, an audible repeat, while
   the play cursor still trails the fresh data. So the music ran short
   without the model's test firing.
3. **Why the music ran short without the worker's programmed receptions**:
   in gameplay timing run 1, 8,530 of the music's 27,371 card blocks (31%)
   came from them. The music's DMA chain stops at every restart made inside
   an interrupt or GD call: the card's token search after CMD18 (about 330
   bytes) exceeds the 256-byte slice those calls allow, and after an overrun
   the music refetches by programmed transfers and arms no DMA. Each stop
   waits for the worker's next visit (73 ms apart on average in run 1).
   Armada overran 1,017 times in 105 s of play; Worms, with less bus
   contention, keeps up.
