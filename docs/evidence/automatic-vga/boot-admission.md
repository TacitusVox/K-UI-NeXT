# VGA boot checks and Windows CE

**Games from SD** and **GD Play → A / K-UI boot** load the original disc bootstrap and enter its second stage at `0xac00e000`. This is the uncached alias of `0x8c00e000`, or `IP.BIN` + `0x6000`. Neither K-UI route requires the disc header to advertise VGA. The SD path includes Windows CE builds. GD Play's direct-disc preflight accepts native games and declines Windows CE. **X / Stock boot** restarts the console through its BIOS and uses the original disc's boot behavior. [SD stage](https://github.com/TacitusVox/K-UI-NeXT/blob/main/src/loader/retail_stage.S) · [GD Play stage](https://github.com/TacitusVox/K-UI-NeXT/blob/main/src/loader/disc_stage.S) · [GD Play with virtual VMU](https://github.com/TacitusVox/K-UI-NeXT/blob/main/src/loader/disc_vmu_stage_asm.S) · [Disc preflight](https://github.com/TacitusVox/K-UI-NeXT/blob/main/src/core/disc_boot.c) · [Stock boot](https://github.com/TacitusVox/K-UI-NeXT/blob/main/src/apps/gd_play.c)

In the original header, the seven hexadecimal digits beginning at `0x38` encode the peripheral flags. Mask `0x10` advertises VGA and mask `0x01` identifies Windows CE. The loader already clears a different flag at `IP.BIN` + `0xfc` as part of its bootstrap handoff. [Peripheral layout](https://mc.pp.se/dc/ip0000.bin.html) · [Bootstrap layout](https://dreamcast.wiki/IP.BIN)

## Original bootstrap checks

| US original disc | Product and version | Peripheral field | VGA flag |
| --- | --- | --- | --- |
| Bust-A-Move 4 | `T-8117N` V1.001 | `019FA01` | Clear |
| Evolution: The World of Sacred Device | `T17706N` V1.290 | `0799800` | Clear |
| Nightmare Creatures II | `T9504M` V1.000 | `07BBA01` | Clear |
| Power Stone | `T1201N` V1.001 | `07BBA10` | Set |
| Skies of Arcadia, disc 1 | `MK-51052` V1.002 | `0799A10` | Set |

The first three startup samples have identical code from `IP.BIN` + `0x6000` through `0x615f` (SHA-256 `4e5516bc173756237b79a3b011e81cc90a855ebeab6cf806bcb2ddecccddf215`). The SH-4 disassembly shows CPU setup, cache control, a small RAM clear and the call to the loaded executable at `0xac010000`. None of those operations consult the VGA flag or cable pins. A header edit therefore adds no boot capability for these samples. The game or its Windows CE display driver still controls the signal during gameplay.

## Hardware reports by region

| Release | Reported behavior with original game data |
| --- | --- |
| Power Stone, PAL | Boots in VGA directly. The US sample above also advertises VGA. |
| Skies of Arcadia, US / PAL | US supports VGA. The original PAL edition gives sound only when forced. The PAL edition has a published program fix, with a later release also correcting timing and cutscene audio. |
| Bust-A-Move 4, North American NTSC / PAL | Both are reported playable with a switchable VGA box or boot disc. The US copy above is a Windows CE example to check in Games from SD. |
| Evolution: The World of Sacred Device, NTSC / PAL | A switch-box report lists the NTSC edition. A PAL owner reported successful VGA boot using DC-X. |
| Conflict Zone, Hidden & Dangerous, NFL Blitz 2000, Tom Clancy’s Rainbow Six, PAL | A PAL owner reported successful VGA boot using DC-X. Hidden & Dangerous is a Windows CE release. |
| Sega Rally 2, PAL | The Windows CE release is listed as working directly in VGA. |
| Caesars Palace 2000 | The community index lists this Windows CE game as unsupported, including forced boot. |

These are original-hardware reports with different loaders. K-UI signal checks are still pending. For a forceable release, use a fixed VGA cable, record the product ID, version and launch route, and check the title screen and one playable scene. For a native Windows CE comparison, use Sega Rally 2 from SD. A K-UI menu or BIOS logo alone establishes only the menu's video mode.

Hardware and patch sources include [Sega_Gamer’s region lists and switch-trick notation](https://gamefaqs.gamespot.com/dreamcast/916412-dreamcast/faqs/36035), [PAL DC-X results](https://dreamcast-talk.com/forum/viewtopic.php?t=494), [Adam Koralik’s switch-box list](https://www.youtube.com/watch?v=HuTYgAyUhTE), [Riggles’ PAL Skies release notes](https://www.dreamcast-talk.com/forum/viewtopic.php?t=18173), [the Windows CE game list](https://www.libretro.com/index.php/category/dreamcast/page/2/) and [the community patch index](https://consolemods.org/wiki/Dreamcast:List_of_VGA_Games_and_Patches).
