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

| Launch | Installed GUI `00d8ed67d9ad` | GUI built from this repository |
| --- | --- | --- |
| Windows CE, CD music Off | `ce-boot.kui`, with the map descriptor rule below | `ce-probe.kui`, 64 map slots |
| Armada, CD music On, VMU Off | `ce-cdda-armada.kui` | the same |
| Armada, CD music On, VMU On | `ce-armada-all-on.kui`, with the checks below | no route |
| Armada, legacy virtual VMU | `ce-probe.kui`, which it calls the "Armada legacy virtual VMU module" and inspects first (`0x8c15c040`) | `ce-probe.kui` is the generic module for every Windows CE game |
| BAM4, Worms Armageddon, Worms World Party, CD music On | their own modules | the same |

The exact condition that selects the legacy module was not decoded.

### Generic module: map descriptor at payload `0x140`

At `0x8c1593e2` the GUI compares the launch path with `ce-boot.kui`. For that
module it reads the 16 bytes at payload offset `0x140`:

- all of them equal to a blank record: the map has 64 slots;
- `KUICEM01`, version 1 (`0x148`) and 96 slots (`0x14c`): the map has 96 slots;
- anything else: the launch stops with "unsupported ordinary Windows CE map
  descriptor".

The repository's generic module writes exactly that record when built with
`KUI_RETAIL_CE_MAP_RECORD=1` (`src/loader/retail_entry.S`), as the delivered
`ce-boot.kui` of build `7b216f2e1136` is.

### Armada with CD music and virtual VMU: `ce-armada-all-on.kui`

- The disc header's CRC32 is compared with `0x684f8040` first (`0x8c15c6ec`):
  Armada's `IP.BIN`, the value `src/core/game_profile.c` records for it. The
  route admits Armada's exact disc only.
- The 16 bytes at payload `0x150` must be `KUIAVM01`, version 1, and `0x1000`
  (`0x8c15c5d0`); otherwise "Armada all-on module capability is missing or
  invalid".
- The module needs a card profile slot ("no card profile slot") and a complete
  CD music seed ("Armada all-on module has no complete CD music seed").
- The launch is logged as "Armada all-on: CD music and dedicated virtual A2
  test card".

Both GUIs share the card profile records (`KUIVMUI1`, `KUIVMUA1`, `KUIVMUC2`)
of the native virtual VMU. How the GUI composes the 4,096-byte profile it
writes into the slot was not decoded.

### Also in this GUI

- `launch-timing.txt` and `smash-launch-timing.txt`: the repository's launch
  timing, which a GUI build enables with `KUI_SMASH_LAUNCH_TIMING=1`.

## Consequence

A GUI built from this repository today would load `ce-probe.kui` for every
Windows CE game. In the owner's installation that file is Armada's legacy
virtual VMU module (build `33298cc0bdb8`; `accepted-progress.md` records the
path collision as a deployment gap). Armada with VMU On would have no route at
all. No repository GUI goes to the owner until it reproduces these routes and
Armada behaves on the console as it does under `00d8ed67d9ad`.

## What a repository GUI needs for virtual VMU in more Windows CE games

1. `ce-boot.kui` as the generic module, with the descriptor rule above.
   `ce-probe.kui` stays Armada's legacy module, or is retired by name.
2. The Armada routes as above, first, checked against `00d8ed67d9ad` on the
   console.
3. A virtual VMU route chosen by the kernel's `maple.dll` family
   ([driver families](wince-vmu-driver-families.md)), not by title. Each family
   module carries an admission record such as `KUIAVM01` (capability and
   profile slot), and the GUI fills its card profile and, with CD music, its
   seed.
4. `KUI_SMASH_LAUNCH_TIMING=1`, so the launch report stays.

Open: how the profile written into the slot is composed. It can be read from
the same disassembly, or settled by reproducing the GUI's checks against its
own accepted modules.
