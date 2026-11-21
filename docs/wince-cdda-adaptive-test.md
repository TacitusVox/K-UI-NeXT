# WinCE CD music readers: the adaptive slow window (21 Nov 2026)

**Status: console-tested 21 Nov 2026 ([result](#result-21-nov-2026)): CD
music On is still a tiny bit worse in gameplay than CD music Off.** One
behaviour change to the five Windows CE CD music modules, the one the
generic module already runs with CD music Off: after a DMA overrun at
12.5 MHz the next receptions run at 6.25 MHz for a window that starts at 16
and doubles, up to 64, while overruns recur within 8 receptions of its end;
a later overrun halves it again ([generic module's record](wince-adaptive-window-test.md)).
The installed CD music modules (`7b216f2e1136`) keep a fixed 64-reception
window. This is the reader half of CD music On matching CD music Off
([design](wince-unified-runtime.md)).

Source `52a94612`: Makefile.armada-stream (Armada, BAM4, Worms, WWP) and
Makefile.armada-vmu-a2 (the combined Armada CD music and virtual VMU module)
take the generic module's switches, `KUI_CE_SLOW_WINDOW` and
`KUI_CE_SLOW_ADAPT`. The defaults (64, 0) build all five accepted modules
byte-identical to `7b216f2e1136`.

## Test build `52a946122537` (`KUI_CE_SLOW_WINDOW=16 KUI_CE_SLOW_ADAPT=2`)

| Module | Route | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|---|
| `ce-cdda-armada.kui` | Armada, CD music On, VMU Off | 135,252 | `5A83CB23` | `844302f60ae543ba...` |
| `ce-cdda-bam4.kui` | Bust-A-Move 4, CD music On | 135,236 | `C005B85B` | `4d5576a87126b737...` |
| `ce-cdda-worms.kui` | Worms Armageddon, CD music On | 136,164 | `E1FA2F7D` | `75f4790557b67815...` |
| `ce-cdda-wwp.kui` | Worms World Party, CD music On | 135,844 | `CE8E47B7` | `2cbc169e7337b628...` |
| `ce-armada-all-on.kui` | Armada, CD music On, VMU On | 206,268 | `E7F5B8B9` | `8ae9da3f0b77e875...` |

The full hashes are in the ZIP's `INVENTORY.json`. Delivered:
`KUI-WinCE-CD-Music-Adaptive-Test-21NOV2026-0852-CST-52a9461.zip` (SHA256
`79b257caf083395542c66362a454736cde01c1c0f50c6eec8da1a94eba24b4ee`), with the
five installed `7b216f2e1136` modules in `IF-NEEDED/`. The README asks the
owner to copy `ce-armada-all-on.kui` only where the card already has one.

Gates, per module, from each recipe on the exact build:

- **Space.** The four CD music readers end exactly at their `0x8c00d800`
  limit (64 bytes were free for WWP, 96 for the others; the window costs 64);
  the combined reader ends at `0x8c00d7e0` (limit `0x8c00dc00`).
- **Region.** The reader region stays `0xf60` bytes (the combined one
  `0x760`): the stream state's extra word fits its padding, so the stream
  controller's pinned bus guard is unchanged. The four modules compile reader
  and controller with the same switches; the combined module's controller
  reads the moved fields only in its timing view, which gets them too.
- **Stack.** Call-graph worst cases 512 to 516 of 2,000 bytes (combined:
  504 of 976).
- The stream excerpt checker (`--stream --reader-fixes`, the title profiles;
  `--cdda-vmu`), the layout checker and the relaxed-call check pass.

Host tests: `test-ce-cdda-reader` runs the Armada stream bootstrap test with
the delivered reader changes and with this window (50,229 checks each); the
adaptive policy itself is `test-sci-stream-ce-adapt`.

## Questions for the console test

CD music On, each game the owner has, against the installed CD music modules
and against the same game with CD music Off (the candidate `91b0540103f0`):

- Armada (US), VMU Off: the intro movie's echo and smoothness, play, music.
- Armada (US), VMU On: the same, and a save.
- Bust-A-Move 4 (US), Worms Armageddon (US) (its intro audio lag), Worms
  World Party (US).

A game that plays as well with CD music On as with it Off is ready for CD
music On by default (a GUI change of its own).

## Result (21 Nov 2026)

The owner's report, CD music On, against the same games with CD music Off on
the candidate `91b0540103f0`:

| Game | Movies | Gameplay |
|---|---|---|
| Armada (US), VMU Off (`ce-cdda-armada.kui`) | the intro still has a tiny lag | the lag the candidate had removed with CD music Off is back |
| Armada (US), VMU On (`ce-armada-all-on.kui`) | the intro lag is gone | the same minor lag |
| Bust-A-Move 4 (US) | okay, maybe a tiny lag | okay, maybe a tiny lag |
| Worms Armageddon (US) and Worms World Party (US) | fine | a tiny lag |

Overall the owner found gameplay a tiny bit worse with CD music On than Off.
So CD music On has not reached parity, and it stays the player's choice (not
a default) for these games.

What the report does not settle:

- Whether this set is better, the same or worse than the installed CD music
  modules (`7b216f2e1136`) with CD music On: the comparison was with CD music
  Off. The owner described the VMU On module as the one from before the
  universal-loader work, so which `ce-armada-all-on.kui` ran is unconfirmed.
- Where CD music On loses time in gameplay. The music adds its own card
  reads (about 172 KiB/s) and the work of feeding them to the game's sound
  buffer; the slow window only changes how the reader recovers from an
  overrun, so it cannot remove that load. The existing Armada timing build
  measures only the intro movie, and the four CD music readers now have no
  space left for counters.

## Not answered

- Timings: no measurement build of these modules.
- CD music On for games without a CD music module (other sound groups and
  kernels, [roadmap](wince-roadmap.md) S3 and later).
