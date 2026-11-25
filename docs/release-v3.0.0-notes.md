# K-UI 3.0 RC1 “Muramasa”

<p align="center"><img src="https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/93f13e84d9893d149d2202c90417c3ec6b5aa940/resources/themes/sources/moonlit-muramasa.png" width="800" alt="K-UI 3.0 Muramasa by TacitusVox. A samurai overlooks a moonlit city from a rain-soaked rooftop."></p>

Muramasa brings native Dreamcast games, Windows CE and Sega Smash Pack into one launcher for consoles using the original GD-ROM drive and SD storage. Since 2.0 Tyrfing, K-UI has gained per-game virtual VMUs, a software updater, selectable themes and a searchable Games library. **The full update is the same build tested on Dreamcast, `93f13e84d989`.**

[Download the SD ZIP](https://github.com/TacitusVox/K-UI-NeXT/releases/download/v3.0.0-rc.1/kui-3.0.0-rc.1-sd.zip) · [Download the full KUP](https://github.com/TacitusVox/K-UI-NeXT/releases/download/v3.0.0-rc.1/kui-3.0.0-rc.1-full.kup) · [Installation guide](https://github.com/TacitusVox/K-UI-NeXT/blob/v3.0.0-rc.1/docs/release-v3.0.0.md) · [Full changelog since 2.0](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/changelog-v2.0.0-to-v3.0.0-rc.1.md)

## Release previews

**Browse your games by their covers.** Gallery shows five titles here, with *Crazy Taxi* selected. In Flow, moving to *Jet Grind Radio* brings its case to the front.

[![Gallery view on Dreamcast with covers for Armada, Crazy Taxi, Jet Grind Radio, Sonic Adventure and Toy Commander](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/10-game-library-gallery.png)](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/10-game-library-gallery.png)

| Flow with *Jet Grind Radio* selected | GD Play options for original discs |
| --- | --- |
| [![Flow view showing the enlarged Jet Grind Radio cover beside the game list](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/11-game-library-flow.png)](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/11-game-library-flow.png) | [![GD Play options with Region-free On, Virtual VMU On and slot A2 selected for the virtual card](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/08-gd-play-options.png)](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/08-gd-play-options.png) |

**Pick up a scratched-disc recovery where you left off.** The recovery screen lets you continue a saved job and retry the sectors the drive missed.

[![Damaged-disc recovery screen with Continue recovery highlighted and options for starting a job or viewing diagnostics](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/09-recovery-dashboard.png)](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/f8a5491c3697e4811f5f9a0d185423b2f9c574c1/docs/releases/3.0.0-rc.1/images/09-recovery-dashboard.png)

All four are 640 × 480 captures from the published RC1 GUI running in Flycast. The two library views use a five-title demonstration catalogue. [Retail cover artwork](https://github.com/libretro-thumbnails/Sega_-_Dreamcast/tree/28b140d16f188301ad697e5935c6dbb412a4ceda/Named_Boxarts)

## Games, saves and CD music

- **Play an import or save from an original disc.** GD Play’s region-free option booted PAL *Shenmue II* Disc 1 on an NTSC Dreamcast. On supported native discs, a virtual VMU can hold saves on an SCI SD card. In a separate hardware test, *Power Stone 2* saved to virtual slot A2 and loaded the save after relaunching from disc.
- **Native games and Smash Pack.** Native games can use the Background reader and per-game virtual VMUs. The Standard VMU reader and all four Smash Pack readers ship with their matching GUI. Favorites, Recently played, search, Flow view and multi-disc grouping make larger libraries easier to browse.
- **Windows CE.** The generic reader gains video-memory transfers and adaptive SCI reads. Dedicated CD-music readers cover 18 additional kernels in the Worms World Party and Bust-A-Move 4 families. **4x4 Evolution plays CD music on Dreamcast.** RC1 also fixes Ducati World’s music repeat count and video-memory reads in the standalone Japanese Virtua Cop 2.
- **Around the console.** K-UI adds themes, menu screenshots and a gallery, virtual-card import and recovery, network media downloads, and damaged-disc recovery.

The hardware check covered the full update, cold boot, game launches, virtual saves and Smash Pack handoffs. Compatibility and performance vary by game. Native Background CD audio and FMV playback are still in development. Five of the newly routed Bust-A-Move 4 family kernels still await hardware reports. [Game compatibility evidence](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/accepted-progress.md)

## Installing RC1

**With a current K-UI updater**, copy the full KUP into `KUI/updates/test/`. Open **Settings → Updates**, install it and choose **Restart K-UI**. If you installed the full `93f13e84d989` test package, you already have this release.

**For cold boot, or to upgrade stock 1.7 or 2.0.0**, merge the SD ZIP’s `KUI/` folder into the card root and replace the supplied files. Boot with your independent K-UI disc. The KUP updates the bank used by **Restart K-UI**. The ZIP also updates the physical GUI your disc reads at power-on. Both downloads carry the same 104 managed files.

[Changelog PDF](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/releases/3.0.0-rc.1/K-UI-3.0-RC1-Changelog.pdf) · [Hardware test checklist](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/releases/3.0.0-rc.1/Hardware-testing.md) · [SHA-256 checksums](https://github.com/TacitusVox/K-UI-NeXT/releases/download/v3.0.0-rc.1/SHA256SUMS.txt)
