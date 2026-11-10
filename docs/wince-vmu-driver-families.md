# WinCE virtual VMU: controller-driver families (US library)

The owner's `--windows-ce` startup run covers the US WinCE library: 36
startups from 36 distinct kernels, plus two duplicate discs. Each kernel's
`maple.dll` and `mapledev.dll` (the Windows CE controller and device drivers
that the virtual card hooks) were compared word by word. A word counts as equal
when it matches raw, or as the same offset into its own module, or as the same
offset into a module of the same name. Within each family below, no word
differs: the driver code is identical apart from where the kernel placed it.
Record: [survey](evidence/wince-us-controller-driver-survey-2026-11-10.json).

| Family | `maple.dll` text | Games |
| --- | ---: | --- |
| 1 | 29,568 B | Bang! Gunship Elite |
| 2 | 29,552 B | Worms World Party, Railroad Tycoon II, Who Wants to Beat Up a Millionaire, Championship Surfer, Atari Anniversary Edition, Starlancer, Ducati World, Q\*bert, The Next Tetris, Hidden and Dangerous, 4x4 Evolution, Hoyle Casino, Sno-Cross, Tomb Raider Chronicles |
| 3 | 29,552 B | Urban Chaos, Spirit of Speed 1937 (same size, different code from family 2) |
| 4 | 27,744 B | Nightmare Creatures II, Rainbow Six, KISS Psycho Circus, Greatest Arcade Hits 1 and 2, NFL Quarterback Club 2001, Sierra Sports Maximum Pool |
| 5 | 27,664 B | Bust-A-Move 4, Resident Evil 2 (both discs), Super Runabout, Tomb Raider: The Last Revelation, Caesars Palace 2000 |
| 6 | 22,432 B | Armada (accepted helper), Wild Metal |
| 7 | 19,472 B | Sega Rally 2 |
| 8 | 19,456 B | Worms Armageddon, South Park Chef's Luv Shack, Quarterback Club 2000 |

Virtua Cop 2 (27,698 B) and Sega Swirl start from the native Smash Pack menu
and are not in this run; their helpers are separate.

## Memory layout

Most kernels end their RAM at `0x8cfd0000`, followed by 192 KiB of OEM driver
globals, as Armada does. The Armada reservation directly below that boundary
therefore has the same address in those kernels. These differ:

- Family 8 and Sega Rally 2 (the CE2 kernels): RAM ends at `0x8cef0000`,
  followed by 1.06 MiB of driver globals.
- Q\*bert (`0x8cad0000`), Rainbow Six (`0x8c500000`), KISS Psycho Circus
  (`0x8cff0000`), Tomb Raider Chronicles (`0x8cdd0000`) and Tomb Raider: The
  Last Revelation (`0x8ced0000`).

## Consequences

- The hook is audited once per family, not per game; a member applies it at
  its own module base. Reservation placement follows the kernel's RAM end.
- Wild Metal has Armada's exact drivers and Armada's memory layout.
- The owner's other WinCE games cover four more families: World Party (2),
  Nightmare Creatures II (4), Bust-A-Move 4 (5) and Worms Armageddon (8).
  With Armada's family that is 32 of the 36 US games, so roughly 33 discs.
- The installed GUI (`00d8ed67d9ad`) routes only Armada's VMU modules, and its
  source is not in this repository. Other titles need a GUI built from this
  repository that keeps the Armada routing.

Not established: hook points, reservation fit and console behavior for any
family except Armada's; JPN and PAL kernels.

## Hook windows (survey, 10 Nov 2026)

The reviewed `maple.dll` windows behind the Armada hook
(`tools/armada_ce_maple_hook_owner_audit.py`) and the VC2 helper
(`tools/vc2_ce_vmu_owner_audit.py`) were searched in one driver per family.

- **Wild Metal (family 6):** every Armada window is at Armada's RVA. The
  windows that differ hold absolute addresses, and they differ only by the
  module base (`0x01b40000` against `0x01dd0000`). The Armada helper applies
  after relocation, with Armada's memory layout.
- **Families 2 (World Party), 4 (NC2) and 5 (BAM4):** exactly two Armada
  windows match, the two initialization kernel-mode windows. They are at
  `0x20f8`/`0x234e` in family 2 and `0x2100`/`0x2356` in families 4 and 5.
  Family 5 also has Armada's 28-byte completion entry, at `0x6f64` (VC2's
  completion RVA). VC2's 13 windows match 1 of 13 in families 4 and 5, and
  none in family 2.
- **Family 8 (Worms Armageddon):** no Armada window matches.

Each of these families therefore needs its own audit, as Armada and VC2 had,
covering:

- the completion entry and walker, and the worker's completion call;
- worker thread creation;
- the `SetKMode` and `WaitForSingleObject` import stubs;
- the root global;
- the text-tail padding for the bridge.

## Next steps

1. Family 2 (14 games, World Party among them): audit `maple.dll` as above
   and derive its hook profile.
2. Parameterize the CE Maple helper by family profile and module base. Today
   it holds Armada's constants in its linker scripts and
   `include/kui/armada_ce_maple_observe.h`.
3. Build a GUI from this repository's source that routes Virtual VMU On for
   WinCE games by kernel family. It must keep the installed GUI's Armada
   routing (the `KUIAVM01` card profile slot and the all-on module) and the
   generic module's `KUICEM01` record.
4. Test package: the new GUI with World Party's virtual VMU, CD music Off and
   On. Check first that Armada VMU On behaves exactly as before.

## US and JPN together (10 Nov 2026)

The JPN run adds 45 Windows CE games. Across 81 games and 75 distinct
kernels there are 12 `maple.dll` families, each identical modulo
relocation. Most JPN games join US families.
Record: [US and JPN survey](evidence/wince-controller-driver-survey-us-jpn-2026-11-10.json).

| `maple.dll` text | Kernels | Regions | Includes |
| ---: | ---: | --- | --- |
| 29,568 B | 10 | J, U | Bang! Gunship Elite, Evangelion Type C and Type E, Princess Maker Collection, Happy Lesson |
| 29,552 B | 18 | J, U | Worms World Party's family |
| 29,552 B | 2 | U | Urban Chaos, Spirit of Speed 1937 |
| 27,744 B | 9 | J, U | Nightmare Creatures II's family, Super Robot Wars Alpha |
| 27,696 B | 1 | J | Virtua Cop 2 (standalone JPN release; the Smash Pack VC2 driver size) |
| 27,664 B | 12 | J, U | Bust-A-Move 4's family, Get Colony, Rune Jade |
| 22,432 B | 4 | J, U | Armada, Wild Metal, Biohazard 2 Value Plus, Hello Kitty Lovely Fruit Park |
| 19,504 B | 2 | J | World Neverland Plus, Kitahei Gold |
| 19,472 B | 1 | U | Sega Rally 2 (US) |
| 19,472 B | 6 | J | Morita no Saikyo Shogi and Reversi, Super Producers |
| 19,456 B | 9 | J, U | Worms Armageddon's family, Space Griffon VF-9, Densha de Go |
| 16,304 B | 1 | J | Sega Rally 2 (JPN) |

Biohazard 2 Value Plus and Hello Kitty Lovely Fruit Park have Armada's exact
drivers, as Wild Metal does. Their RAM layouts are in the record.
