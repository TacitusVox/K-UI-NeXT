# Scan from internet on Dreamcast

Open **Games**, highlight a game, press **START**, then choose **Scan from internet**. Press **A** on **Start scan** to download its box cover and manual. K-UI contacts the public provider over HTTPS and saves the result to the card. A computer, account and media server are unnecessary.

Change **Scan for** to scan all of `/Games`. **Box art** and **Manuals** start on. Turn off **Box art** for a manual-only scan, or turn off **Manuals** for a smaller first transfer over dial-up. Enable **Previews** to include gameplay video. Press **B** to cancel. A subsequent scan reuses completed files and retries the interrupted asset. Browsing the library and opening installed media work offline.

| Media | Source and result |
| --- | --- |
| Box covers | K-UI matches the disc's product number and region. For 329 catalogued releases the console can request a small JPEG directly from Internet Archive. The Libretro catalogue supplies the full-size fallback and covers 1,104 releases. Downloaded box art can replace an automatic disc icon. A manually assigned or loose custom cover keeps priority. |
| Manuals | Internet Archive page scans are assembled one page at a time into a book the console can open. Where only a Digital Press PDF is available, K-UI saves the original and reports it as PDF-only. |
| Gameplay previews | Internet Archive Dreamcast video snaps. K-UI decodes the source on the Dreamcast and stores up to ten seconds of silent 160 × 120 video at ten frames per second. Available previews and manuals depend on the matched release. |

The compiled media catalogue associates 382 different page sets and 279 distinct previews with matching releases. The providers are built into this version. ScreenScraper is not one of the on-console sources. [Cover catalogue and attribution](internet-covers.md) · [Manual and preview sources](internet-manuals-previews.md)

## Choose the connection

| Connection | Card and setup |
| --- | --- |
| Broadband Adapter | Connect Ethernet to a router that supplies an IPv4 address and DNS by DHCP. SCI and SCIF storage can both be used. |
| W5500 | Use SCIF storage. The W5500 uses the SCI port and obtains its configuration from DHCP. |
| Wi-Fi | Use SCIF storage and the existing configured K-UI ESP32-C5 wireless board on SCI. |
| Dial DreamPi | Connect the stock modem to an operating DreamPi and explicitly choose **Dial DreamPi**. Start scan waits for dial tone, dials the local DreamPi, negotiates PPP, and hangs up when the operation ends. SCI and SCIF cards can both be used. |

**Automatic** checks the broadband adapter first, followed by W5500 and Wi-Fi. Choose the specific adapter when multiple are attached. For example, selecting **Wi-Fi** bypasses an installed BBA with its Ethernet cable unplugged. DreamPi is always an explicit choice. If HTTPS reports a certificate date error, check the Dreamcast clock in Settings.

## Open downloaded media

After a selected-game scan, press **X** on the results screen to read its downloaded manual. On later visits, choose **Open selected game's manual** on the scan screen. The booklet opens offline. Leave a title highlighted for three seconds in any Games layout to play its downloaded preview once in the artwork tile. Moving or leaving Games stops playback. If the title has no clip, its cover stays visible. You can also play the clip directly from the scan screen. In a manual, Left and Right change pages in fit mode. **A** toggles zoom, the D-pad pans, and **X/Y** turns pages while zoomed. **A** replays a preview and **B** returns.

Each downloaded box is stored once in `KUI/covers/downloads/` at the three sizes used by Games, 160, 104 and 56 pixels square. Together with its Games cache record it uses 79,632 bytes of file data. An internet scan can convert an earlier downloaded PNG already on the card using the local copy. Manuals and previews are saved under `KUI/media/`, keyed to the game image path. A PDF-only result is accessible on the card and identified as such by the console viewer.

## What a scan costs

For the U.S. Crazy Taxi cover, the source-hosted JPEG is 192 × 190 pixels and transfers 23,900 bytes. The Libretro fallback PNG is 512 × 510 pixels and transfers 490,502 bytes. K-UI runs at 640 × 480 and fits the cover to its Games tile. Even a 4K baseline JPEG or non-interlaced PNG can be converted on the console. The scan streams the original to the card and removes it after installing the thumbnails. On FAT32 and exFAT test images, an 11,467,965-byte PNG used 229,328 bytes of additional scan heap and a 6,117,673-byte JPEG used 484,304 bytes. Each installed cover occupied 79,632 bytes. Network and TLS buffers are additional.

Manual pages download individually. A complete ten-second preview occupies 3,840,016 bytes on the card. Browsing reads no movie frames before the three-second delay. During playback two frames take 76,800 bytes of RAM. The decoder also uses temporary storage for the compressed source. In the U.S. Crazy Taxi test, the first 1,754,567 bytes of a 5,562,116-byte source produced the same 100 frames as downloading the full video. Other titles depend on their frame rate, compression and Ogg page layout. Dial-up can take several minutes for a preview. The console reports transfer and conversion progress.

## First console test

1. Back up your existing `KUI/boot.kui` and `KUI/runtime.kui`, copy the ZIP's `KUI/` folder to the card root, and cold-boot from your usual boot disc.
2. Highlight the U.S. **Crazy Taxi**. Leave Box art and Manuals enabled and run the selected-game scan. Press **X** from the results to open the manual, turn a page, then return to Games to see the box.
3. Enable Previews and scan again. The completed cover and manual are reused. Return to Games and leave Crazy Taxi highlighted for three seconds. Move away during playback, then come back and check that the three-second wait starts over. Cancel a library scan with **B** and check that you can start another scan.
4. For a hardware failure, capture the adapter name and the status message. The status distinguishes connection, DNS, certificate, storage and provider failures. On BBA, **Network → Inspect adapter** and **Test network** are separate cable and DHCP checks.

The same branch includes [FTP](ftp.md), [browser sharing](browser-companion.md), and the [PC rip receiver](network-rip.md). These services are independent of scanning for game media.
