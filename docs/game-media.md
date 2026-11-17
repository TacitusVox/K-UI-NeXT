# Dreamcast Broadband Adapter and game media

The Games screen can fetch missing covers, manuals and short video previews over the Broadband Adapter. A companion on your PC or NAS contacts the Internet over HTTPS and converts the media. The Dreamcast fetches the prepared files over HTTP on your home network and keeps them on the card for offline use.

## Start the companion

The ZIP contains the two Python files in `tools/`. Keep them together on the computer. On Debian or OpenMediaVault, install the dependencies for the media types you want.

```sh
sudo apt install python3-pil ffmpeg poppler-utils
```

Use the computer's LAN IPv4 address in place of `192.168.1.20`. Run this from the extracted ZIP or repository root.

```sh
python3 tools/media_server.py --bind 192.168.1.20
```

This serves TCP port 8732. The companion is intended for the home LAN. Python 3.9+ and Pillow with `Image.Resampling` support are required. FFmpeg converts previews. Poppler's `pdfinfo` and `pdftoppm` render manuals.

| Source | What it supplies | Setup |
| --- | --- | --- |
| Existing ROCKNIX or EmulationStation library | Box art, manuals and previews already listed for your Dreamcast games | Make the Dreamcast `gamelist.xml` and its referenced media folders available to the computer, keeping their layout. Add `--gamelist /path/to/dreamcast/gamelist.xml`. For a separately stored gamelist, set `--media-root` to its Dreamcast ROM folder. Add `--offline` to use only that library. |
| ScreenScraper | Region-matched game media when available from the provider | Set `KUI_SS_DEVID` and `KUI_SS_DEVPASSWORD` on the computer. Optional member credentials use `KUI_SS_USER` and `KUI_SS_PASSWORD`. The developer credential pair must be issued for the application. |
| Libretro | Dreamcast box art | Enabled without an account. The companion matches the Dreamcast release catalogue and retrieves its named box art. |

A ROCKNIX copy works without ScreenScraper credentials. Use the Dreamcast system's gamelist and the files it references. The console stores the companion address. Provider credentials stay on the computer.

Sources: [ROCKNIX scraping](https://rocknix.org/configure/scraper/), [ScreenScraper API](https://www.screenscraper.fr/webapi2.php), [Libretro Dreamcast thumbnails](https://github.com/libretro-thumbnails/Sega_-_Dreamcast), [Libretro Dreamcast release catalogue](https://github.com/libretro/libretro-database/blob/master/metadat/redump/Sega%20-%20Dreamcast.dat).

## On the Dreamcast

1. Connect the BBA to your DHCP network. Keep SCI or SCIF storage selected as usual. For this test ZIP, back up the two existing GUI files, copy the supplied `KUI/` folder to the card root, and cold-boot K-UI from CD.
2. Open **Games**, highlight a game, press **START**, and open **Download game media**. Enter the computer's LAN IPv4 address. Choose the highlighted game or the entire library.
3. Choose the media types. Artwork alone is selected initially. Enable manuals and previews if you want them, then start the download. **B** stops a batch. Completed files are available on the next run.
4. Return to the Games list to see the cover. For the highlighted game, use **Open selected game's manual** or **Play selected game's preview** on the media screen. Previously downloaded media works with the companion offline.

K-UI keeps downloaded PNGs in `KUI/covers/downloads/` and builds the thumbnails the Games list already uses. Manuals and previews live in `KUI/media/`, keyed to the game image path. Existing disc artwork and owner-assigned covers have priority. Games with a title-only cache record can receive a cover. Manuals are paged JPEGs with fit and zoom modes. In fit mode, Left/Right changes page. **A** toggles zoom, the D-pad pans, and **X/Y** changes page while zoomed. Previews play up to ten seconds at 160 × 120, 10 frames per second, without audio. **A** replays and **B** returns. A full-length preview uses about 3.7 MiB on the card.

## First hardware checks

- **Network → Inspect adapter → Test network.** Check the assigned IPv4 address. Cancel a second run, then retest without rebooting.
- Download all three media types for one game which has known source files, open both viewers, then repeat the download. The second run should report existing files.
- Try the library download and press **B** during a preview transfer. Restart the batch and check that previously completed media still opens.
- Start [browser sharing](browser-companion.md) or [FTP](ftp.md), connect to the displayed address, stop the service, and start it again. The [PC rip receiver](network-rip.md) can use the same adapter.

The Network diagnostic, media downloader and file services acquire the adapter when started. Physical link negotiation, sustained throughput and frame pacing on your card need a console run.
