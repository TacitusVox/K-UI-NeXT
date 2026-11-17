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

## Worldwide (11 Nov 2026)

The full CHD collection run adds PAL. 104 Windows CE games on 94 distinct
kernels still fall into the same 12 `maple.dll` families; PAL adds no new
family. The largest are:

- World Party's family: 22 kernels.
- BAM4's: 15.
- Worms Armageddon's: 13.
- Bang! Gunship Elite's: 12.
- Nightmare Creatures II's: 10.

The PAL Nightmare Creatures II and Taxi 2 kernels join the Urban Chaos family
rather than the US NC2 family. A hook built per family therefore covers PAL
releases too, but the family must be looked up per kernel, not per title.
Record: [worldwide survey](evidence/wince-controller-driver-survey-world-2026-11-11.json).

## Family 7 placements (17 Nov 2026)

Armada, Wild Metal, Biohazard 2 Value Plus and Hello Kitty Lovely Fruit Park
carry the same `nk.exe` build, not only the same drivers. They differ in
placement alone. `tools/wince_family7_profiles.py` derives it from each kernel;
the record is [family 7 profiles](evidence/wince-family7-profiles-2026-11-17.json).

| Kernel | Kernel data shift | `maple.dll` base / ROM entry | `mapledev.dll` base / ROM entry |
| --- | ---: | --- | --- |
| Armada | 0 | `0x01dd0000` / `0x8c141614` | `0x01d20000` / `0x8c1416f4` |
| Wild Metal | `+0x200000` | `0x01b40000` / `0x8c341efc` | `0x01a90000` / `0x8c341fdc` |
| Biohazard 2 Value Plus | `-0x10000` | `0x01df0000` / `0x8c130d94` | `0x01d60000` / `0x8c130e34` |
| Hello Kitty Lovely Fruit Park | 0 | `0x01dc0000` / `0x8c143628` | `0x01d10000` / `0x8c143708` |

- In `nk.exe`, 1,023 of the 1,028 words that differ from Armada's move by
  exactly the data shift. The other five are the ROM header pointer and four
  values that follow where RAM starts.
- The drivers' code equals Armada's apart from addresses that move with a
  module's base. Their compressed data sections have equal stored sizes.
- RAM ends at `0x8cfd0000` in all four.

`include/kui/ce_maple_family7.h` records these, with Armada as the default.
Only `include/kui/armada_ce_maple_observe.h` and the write layout's bridge
follow it: with `CE_FAMILY7_KERNEL=1` the bridge links at `0x01b467a4`.

That is not yet a Wild Metal helper (corrected 17 Nov 2026,
[WinCE scope](wince-universal-scope.md)):

- The helper's owner profile, `include/kui/ce_maple_owner_profile.h`, still
  holds Armada's current process and thread (`0x8c145890`, `0x8c145894`), its
  process table (`0x8c14e6e0`), its bridge return (`0x01dd67c6`) and its Wait
  thunk (`0x01dd6314`). A scratch helper built with `CE_FAMILY7_KERNEL=1`
  contains those words and none of Wild Metal's (`0x8c345890`, `0x8c345894`,
  `0x01b467c6`). Its checks would refuse Wild Metal and publish no card.
- The stage's ROM placements and kernel admission
  (`include/kui/armada_ce_maple_reservation.h`) are still Armada's.

With Armada selected, all six delivered WinCE modules of build `7b216f2e1136`
rebuild byte for byte. The stage's kernel admission and the GUI route are
still Armada's ([GUI routing](wince-gui-routing.md)), so no other title is
enabled yet.

## Family 2 audit: World Party (17 Nov 2026)

World Party's `maple.dll` was disassembled beside Armada's and matched against
each reviewed Armada window. The survey found family 2 identical apart from
relocation, so these RVAs hold in all 22 family 2 kernels, each at its own
base. Record: [family 2 audit](evidence/wince-maple-family2-audit-2026-11-17.json).

| Window | Armada (family 7) | World Party (family 2) |
| --- | --- | --- |
| `CreateThread` import stub (COREDLL 492) | `0x6344` | `0x7f1c` |
| `SetKMode` import stub (COREDLL 630) | `0x6374` | `0x7f4c` |
| `WaitForSingleObject` import stub (COREDLL 497) | `0x6314` | `0x7eec` |
| Worker thread creation (two `CreateThread` calls) | `0x1b80`-`0x1bb4` | `0x1d68`-`0x1d9c` |
| Thread start wrappers | `0x183c`, `0x1854` | `0x1a00`, `0x1a18` |
| Worker | `0x5d4c` | `0x7910` |
| Worker's completion call (status bit `0x1000`) | `0x5daa`-`0x5db4` | `0x797a`-`0x7986` |
| Completion entry / resume after two pushes | `0x5b0c` / `0x5b10` | `0x7664` / `0x7668` |
| Initialization kernel mode | `0x1ef4`, `0x214a` | `0x20f8`, `0x234e` |

Two differences matter for the hook:

- **The worker waits forever.** Family 7's worker waits 60 ms at a time;
  family 2's waits until signalled. It also keeps a flag of whether the last
  pass completed, which gates two calls on the next pass.
- **The extension's fields are moved.** The completion reads field `0x15c`
  (Armada `0x158`), and the worker's thread handles are at `+384`/`+388`
  (Armada `+376`/`+380`).

Otherwise the completion entry is Armada's: the same seven register saves,
and `T` unused until the test at `0x767c`.

The bridge's room differs by kernel:

- **16 of the 22 kernels** have 140 zero bytes after `maple.dll`'s code, at
  `0x8374`. That is room for Armada's 92-byte bridge, within short-branch reach
  of the completion entry and the `SetKMode` stub.
- **Six kernels have none:** World Party, 4x4 Evolution, both Hundred Swords,
  Net de Para and Starlancer. There, the bytes after the code hold other ROM
  records. `mapledev.dll`'s free tail is a candidate if it runs in the same
  process, which is not established.
- **RAM end:** Q\*bert, Rainbow Six and Tomb Raider Chronicles end RAM below
  `0x8cfd0000` and need the reservation moved.

### Next for family 2

1. A family 2 hook profile: Armada's hook at the RVAs above, adjusted for the
   indefinite wait and the moved fields.
2. Place the bridge for the six kernels without a tail.
3. The GUI route of [GUI routing](wince-gui-routing.md).
