# WinCE CD music: Ducati World's repeat count (21 Nov 2026)

**Status: test build `d7fd810eb837` delivered 21 Nov 2026; console test
pending.** One behaviour change on round 1's Ducati World module
(`1c71ba3afa17`, [record](wince-cdda-wwp-group-test.md)), which played no
music ([result](wince-cdda-wwp-group-test.md#result-21-nov-2026)).

## Why Ducati World was silent

From the owner's files (static analysis; nothing was executed):

- **One request per race.** Ducati World's only CD audio request is IOCTL
  `0x25020` {t, t, 1000}, sent once at race setup, with t from 5 to 14
  (`DUCATI.EXE` `0x51520`; the 1000 is a literal at `0x5153e`). The CD driver
  `wsegacd.dll` forwards it unchanged as GD 20 PLAY; this disc's `IP.BIN`
  gives no track offset. There is no PLAY2. The front end sends no CD audio
  command, on any console.
- **No status polling.** The game never reads the drive status (its poll
  routine has no callers) and relies on the drive repeating the track.
- **The refusal.** The stream model refused a PLAY or PLAY2 with a repeat
  count above 15 (`src/loader/armada_ce_stream_model.c`). The player counted
  the refusal and played nothing.
- **A new process per race.** Each race sends GD 19 (table of contents),
  GD 20 PLAY {t, t, 1000}, PAUSE (22) and RELEASE (23) pairs, and GD 33 STOP
  about a second after the race ends. Then the process exits.
  `DUCATIDC.EXE` stays resident and starts `DUCATI.EXE` again (command line
  `F`) when its primary thread ends. Each race is therefore a new process
  with its own DirectSound.

## What a console does with 1000

The drive's CD_PLAY packet holds the repeat count in four bits: 0 to 14
repeats, and 15 repeats endlessly (GD-ROM SPI specification 1.30).

No public source shows what the BIOS does with a larger value. It could
keep the low four bits (1000 becomes 8, nine passes) or clamp it. It does
not refuse it: the retail game has its music.

The references disagree:
- Flycast's drive emulation and MAME mask the packet's nibble.
- Flycast's BIOS emulation copies the 32-bit value.
- DreamShell's `isoldr` and KallistiOS treat 15 and above as endless.

## The change (source `d7fd810e`)

`KUI_ARMADA_CE_STREAM_REPEAT_CLAMP` saturates a PLAY20 or PLAY21 repeat count
above 15 to 15, endless, instead of refusing it. Counts 0 to 15 keep their
meaning. Endless play covers any race, and the game stops the track itself.

`Makefile.armada-stream` `KUI_CE_CDDA_REPEAT_CLAMP` reaches the controller
only. It is on by default only for `KUI_WINCE_CDDA_WWP_KERNEL=DUCATI_WORLD`,
so every other module keeps its bytes.

## Known limit: the first race only

The player binds the first process that plays: its DirectSound object, and a
worker thread created in it. It keeps that binding for the console session
(`worker_started` is never cleared). Ducati's second race is a new
`DUCATI.EXE` process. Its commands are queued and counted but never applied,
so that race is silent. Rebinding after the owner process ends is a separate
behaviour change, to be designed after this test.

This test also reaches a path the earlier module never did: the race's
process exits after a stream has played. Its STOP reaches the worker
asynchronously; World Party's stop can take several 20 ms visits, so the
process may end before the stop completes. What the game's DirectSound then
does with K-UI's buffer is not established. The test listens for it.

## Test build `d7fd810eb837`

| Module | Bytes | Payload CRC | SHA-256 |
|---|---|---|---|
| `ce-cdda-41b0fe340c3a.kui` | 136,004 | `3FB89B1C` | `4979cff7a8e005e11a2cd8e6730ed43cc6c8cbc35afb2f828f653517937a7a2b` |

Ducati World's group record and the accepted recipe, as in round 1, with the
clamp.

Gates, from the committed tree:

- **Accepted bytes.** The five accepted CD music modules rebuild
  byte-identical to `1ba2fa7e7040`. The ten other round-1 modules rebuild
  byte-identical to `1c71ba3afa17`. Ducati's module with
  `KUI_CE_CDDA_REPEAT_CLAMP=0` rebuilds byte-identical to `1c71ba3afa17`
  (`702BE589`).
- **What the clamp changes.** Against that build, Ducati's module differs in:
  - `kui_armada_ce_stream_request` (480 to 468 bytes) and the controller's
    layout after it (41,632 to 41,600 bytes);
  - the stage's embedded copy of the controller;
  - one word of the low reader: the address of the controller's drive status,
    32 bytes lower;
  - the build ID.
- **Checks.** The stream checker (`--stream --wwp --wwp-kernel DUCATI_WORLD
  --reader-fixes`) and the call checks pass. The stack worst case is 516 of
  2,000 bytes.
- **Host.** The model test runs in both builds:
  - repeat 16, 1000, `0x7fffffff` and `0xffffffff` for both commands;
  - repeat 0 to 15 unchanged;
  - three ring passes of {t, t, 1000}, then PAUSE, RELEASE and STOP.

  The player test queues {1, 1, 1000} in the default, emergency-PIO and
  clamp builds.
- **GUI admission of the real bytes.** `test-wince-packaged-prepare --kernel`
  runs with Ducati's kernel, for raw and 2048-byte DATA. A kernel with one
  byte changed is refused.

Delivered: `KUI-WinCE-Ducati-Music-Test-21NOV2026-1929-CST-d7fd810.zip`
(222,421 bytes, SHA256
`d7f807e502a7bb48ce46f82377c2fa205b0b05b13e15f59431a10283eb901937`). It holds:
- the module;
- `IF-NEEDED/`: round 1's module;
- `KUI/apps/games/ce-boot.kui`: the everyday candidate `91b0540103f0` (exact
  delivered bytes, payload CRC `6829D405`). It replaces the read-ahead test
  build `1a5f46776c0c` still on the owner's card, which was not adopted
  ([why](wince-cdda-wwp-group-test.md#result-21-nov-2026)).

The GUI stays `1c71ba3afa17`.

## Questions for the console test

- **Ducati World, CD music On, first race:**
  - Does the music start with the race?
  - Does it pause and resume with the game?
  - Photograph the report's first page during the race. REFUSED should be 0.
- **The end of that race and the return to the menus:** any stuck sound,
  loop or hang?
- **A second race:** silence is expected. Is there any hang or other change?
- **4x4 Evolution, CD music Off, with the candidate `ce-boot.kui`:** does
  skipping the intro still crash?
