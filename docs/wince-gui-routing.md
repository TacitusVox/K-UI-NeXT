# Windows CE routing in the installed GUI `00d8ed67d9ad`

The owner's GUI `00d8ed67d9ad` routes Armada's virtual VMU modules and reads the
generic module's map record. Its source commit,
`00d8ed67d9ad1a3de57690918912c83cb58706ad`, is in neither `TPMJB/K-UI-NeXT` nor
`TacitusVox/K-UI-NeXT`: a fetch by hash is refused and no branch contains its
routing code (16 Nov 2026). This record is read from the GUI itself: the
`runtime.kui` of `KUI-Armada-VMU-Off-Fix-07NOV2026-1325-CST-4d893b3.zip`
(`KUIRUN1` build `00d8ed67d9ad`, payload 3,563,352 bytes, CRC32 `4c5b0d7d`),
its strings, and a disassembly of its launch code at `0x8c157000`-`0x8c15e000`.
Addresses below are the GUI's own.

## Routes

| Launch | Installed GUI `00d8ed67d9ad` and the repository GUI since 17 Nov 2026 |
| --- | --- |
| Windows CE, CD music Off | `ce-boot.kui`, with the map record rule below |
| Armada, CD music Off, `ce-probe.kui` installed | `ce-probe.kui`, Armada's legacy virtual VMU module |
| Armada, CD music On, VMU Off | `ce-cdda-armada.kui` |
| Armada, CD music On, VMU On, `ce-armada-all-on.kui` installed | `ce-armada-all-on.kui`, with its own test card |
| BAM4, Worms Armageddon, Worms World Party, CD music On | their own modules |

### How the routes were recovered (17 Nov 2026)

`00d8ed67d9ad` is Main `e20eae43` plus one change to the launcher. Its other
3,397 functions are byte-identical (absolute addresses and branches out of a
function masked) to a build of `e20eae43` with:

- `BUILD_ID=00d8ed67d9ad` and `KUI_SMASH_LAUNCH_TIMING=1`;
- the embedded bundle of the four Smash Pack modules (`cc81e63ca4d2`,
  identical to the delivered files);
- the Standard binding of the owner's `retail-boot.kui` `9e3295bcfaec`
  (`resources/updater-bindings/installed-standard-9e3295bcfaec.json`).

The changed functions were decoded and rewritten in this repository. Built the
same way, 3,408 of the reconstruction's 3,412 functions match `00d8ed67d9ad`
exactly, including the map encoder and validator, the launch-timing writer and
the Standard recognizer. The other four:

- `main`, which embeds the image size;
- the installed-module check, written as two functions;
- `prepare_launch`, 21,936 bytes against 21,900;
- the WinCE timing record, 184 bytes against 176.

Every string of the Windows CE routes is in both images.

### Generic module: `ce-boot.kui`

The module is read after the IP check. If its path is `ce-boot.kui`, the 16
bytes at payload `0x140` decide its map capacity:

- all zero: 64 slots;
- `KUICEM01`, version 1, 96: 96 slots (`kui_retail_manifest_encode_capacity`);
- anything else: "unsupported ordinary Windows CE map descriptor".

A capacity not above the track count is refused ("Windows CE background map
has %u slots for %u tracks and their data extents"); a full map names the
fragmented track or the capacity.

### Armada's legacy module: `ce-probe.kui`

CD music Off, the background reader, Armada's IP.BIN (CRC32 `0x684f8040`),
Armada's disc layout and Armada's kernel SHA: if `ce-probe.kui` is installed
(present, or in the GUI's bundle), it is the launch module. Absent, the
generic module is used. Any other state ("cannot inspect installed Armada
legacy virtual VMU module") refuses: a directory, an unresolvable path, or a
file missing from a selected bank. The VMU setting is not consulted.

### Armada with CD music and virtual VMU: `ce-armada-all-on.kui`

- CD music On, Armada's runtime profile, IP.BIN and disc layout, and the
  module installed: the saved VMU setting is read. On selects the module; Off
  keeps `ce-cdda-armada.kui`. An unreadable setting refuses.
- The 16 bytes at payload `0x150` must be `KUIAVM01`, version 1, `0x1000`.
- The complete CE audio map fills its `CEX2` stream seed ("Armada all-on module
  has no complete CD music seed" without one).
- The virtual card is the per-game card of product `KUIARMCD`, a dedicated test
  card: the complete map's copy carries that product, the launch map keeps
  Armada's. Every image file is excluded from the card's placement.
- The report's CD music profile is "Armada CD music + virtual A2"; the log says
  "Armada all-on: CD music and dedicated virtual A2 test card".

### Launch timing

A Windows CE CD music launch writes `launch-timing.txt` as a native launch,
reader "Background SCI + CD music", labelled WinCE, with its kernel identity
read and hash as phase `kernel_read_hash`. The log's native line reads
"UI loader ms".

## What a repository GUI needs for virtual VMU in more Windows CE games

1. The routes above, checked against `00d8ed67d9ad` on the console: done in
   source; the console check is pending.
2. A virtual VMU route chosen by the kernel's `maple.dll` family
   ([driver families](wince-vmu-driver-families.md)), not by title. Each family
   module carries an admission record such as `KUIAVM01`, and the GUI fills its
   card profile and, with CD music, its seed.
3. `KUI_SMASH_LAUNCH_TIMING=1`, so the launch report stays.

## Delivery and open items (17 Nov 2026)

- **Console test pending:** `KUI-WinCE-GUI-Routes-16NOV2026-1928-CST-069d612.zip`,
  GUI build `069d612cadea` (payload CRC32 `178a6a7b`), identical `boot.kui` and
  `runtime.kui`, no module changes. It is built with launch timing, the Smash
  bundle and the owner's Standard binding. Record:
  [reconstruction evidence](evidence/gui-00d8ed67d9ad-reconstruction-2026-11-17.json).
  Until the owner confirms Armada (all-on and CD music with VMU Off), a CD
  music Off Windows CE game and one Standard virtual VMU game, `00d8ed67d9ad`
  stays the reference.
- **Packagers:** `tools/package.py` and `tools/global_loader_rollout.py` still
  stage the generic module as `ce-probe.kui`, this repository's layout before
  this change. A full package for this GUI must stage it as `ce-boot.kui`. The
  owner's installation already has `ce-boot.kui`.
- **Not covered on host:** the all-on card preparation is compiled only with
  `KUI_VMU_GAME_CARDS`, which no host test builds. It was compared with
  `00d8ed67d9ad`'s code; the console test checks it.
