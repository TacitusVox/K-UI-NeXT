K-UI 3.0 RC1 brings the native Dreamcast, Windows CE and Sega Smash Pack loaders into one release for consoles using an original GD-ROM drive and SD storage. The GUI build tested on hardware is **`93f13e84d989`**.

- **Games and saves.** Native games can use the Background reader and per-game virtual VMUs. The Standard VMU reader and all four Smash Pack readers ship with the GUI. The library adds Favorites, Recently played, search, Flow view and multi-disc grouping.
- **Windows CE and CD music.** The generic WinCE reader gains video-memory transfers and adaptive SCI reads. Dedicated CD-music readers cover 18 additional kernels in the Worms World Party and Bust-A-Move 4 families. **CD music is confirmed on Dreamcast in 4x4 Evolution.** This release also includes Ducati World’s music repeat fix and the standalone Japanese Virtua Cop 2’s video-memory fix.
- **Around the console.** Themes, menu screenshots and a gallery, virtual-card import and recovery, network media downloads, and damaged-disc recovery are included.

The release passed a hardware check of the full update, cold boot, game launches, virtual saves and Smash Pack handoffs. Compatibility and performance still vary by game. Native Background CD audio and FMV playback remain areas of active work. Five of the newly routed Bust-A-Move 4 family kernels still await hardware reports.

### Install

**With a current K-UI updater**, copy `kui-3.0.0-rc.1-full.kup` into `KUI/updates/test/`. Open **Settings → Updates**, install it and choose **Restart K-UI**. If you already installed the full `93f13e84d989` test package, you have the release software.

**For cold boot, or to upgrade stock 1.7 or 2.0.0**, merge the SD ZIP’s `KUI/` folder into the card root and replace the supplied files. Boot with your independent K-UI disc. The KUP updates the bank used by **Restart K-UI**. The SD ZIP also updates the physical GUI your disc reads at power-on. Both downloads carry the same 104 managed files.

[Detailed changelog since 2.0](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/changelog-v2.0.0-to-v3.0.0-rc.1.md) · [Installation guide](https://github.com/TacitusVox/K-UI-NeXT/blob/v3.0.0-rc.1/docs/release-v3.0.0.md) · [Game compatibility evidence](https://github.com/TacitusVox/K-UI-NeXT/blob/main/docs/accepted-progress.md)
