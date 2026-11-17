# Scan from internet on Dreamcast

Open **Games**, highlight a game, press **START**, then choose **Scan from internet**. Press **A** on **Start scan** to download its box cover. K-UI contacts the public provider over HTTPS and saves the result to the card. A computer, account and media server are unnecessary.

Change **Scan for** to scan all of `/Games`. Enable **Manuals** or **Previews** to include them. Box covers are selected by default so the first scan is practical on a slow connection. Press **B** to cancel. A subsequent scan reuses completed files and retries the interrupted asset. Browsing the library and opening installed media work offline.

| Media | Source and result |
| --- | --- |
| Box covers | Libretro Dreamcast box art. K-UI matches a disc's product number and region, using the title when needed. The bundled index covers 1,104 release-specific images. Downloaded box art can replace an automatic disc icon. A manually assigned or loose custom cover keeps priority. |
| Manuals | Internet Archive page scans are assembled one page at a time into a book the console can open. Where only a Digital Press PDF is available, K-UI saves the original and reports it as PDF-only. |
| Gameplay previews | Internet Archive Dreamcast video snaps. K-UI decodes the source on the Dreamcast and stores up to ten seconds of silent 160 × 120 video at ten frames per second. Available previews and manuals depend on the matched release. |

The compiled media catalogue associates 351 different page sets and 279 distinct previews with matching releases. The providers are built into this version. ScreenScraper is not one of the on-console sources. [Cover catalogue and attribution](internet-covers.md) · [Manual and preview sources](internet-manuals-previews.md)

## Choose the connection

| Connection | Card and setup |
| --- | --- |
| Broadband Adapter | Connect Ethernet to a router that supplies an IPv4 address and DNS by DHCP. SCI and SCIF storage can both be used. |
| W5500 | Use SCIF storage. The W5500 uses the SCI port and obtains its configuration from DHCP. |
| Wi-Fi | Use SCIF storage and the existing configured K-UI ESP32-C5 wireless board on SCI. |
| Dial DreamPi | Connect the stock modem to an operating DreamPi and explicitly choose **Dial DreamPi**. Start scan waits for dial tone, dials the local DreamPi, negotiates PPP, and hangs up when the operation ends. SCI and SCIF cards can both be used. |

**Automatic** checks the broadband adapter first, followed by W5500 and Wi-Fi. Choose the specific adapter when multiple are attached. For example, selecting **Wi-Fi** bypasses an installed BBA with its Ethernet cable unplugged. DreamPi is always an explicit choice. If HTTPS reports a certificate date error, check the Dreamcast clock in Settings.

## Open downloaded media

The Games list uses the new box art. From the scan screen choose the highlighted game's manual or preview. In a manual, Left and Right change pages in fit mode. **A** toggles zoom, the D-pad pans, and **X/Y** turns pages while zoomed. **A** replays a preview and **B** returns.

Covers are saved under `KUI/covers/downloads/`, with the existing RGB565 thumbnails in the Games cache. Manuals and previews are saved under `KUI/media/`, keyed to the game image path. A PDF-only result is accessible on the card and identified as such by the console viewer.

## What a scan costs

The regional cover catalogue and resolver occupy about 77 KiB in the linked program. The HTTP and TLS buffers together use 34,716 bytes of heap per active transfer on SH-4, plus the selected adapter’s buffers. A scan processes one asset at a time and streams network data to storage. Initial PNG decoding needs more working memory. The original U.S. Crazy Taxi cover is 490,502 bytes and required 2.46 MiB for the compressed image plus decoder allocations in a host measurement. These figures exclude the rest of K-UI.

Manual pages download individually. A complete ten-second preview occupies 3,840,016 bytes on the card. The decoder also uses temporary storage for the compressed source. In the U.S. Crazy Taxi test, the first 1,754,567 bytes of a 5,562,116-byte source produced the same 100 frames as downloading the full video. Other titles depend on their frame rate, compression and Ogg page layout. Dial-up can take several minutes for a preview. The console reports transfer and conversion progress.

## First console test

1. Back up your existing `KUI/boot.kui` and `KUI/runtime.kui`, copy the ZIP's `KUI/` folder to the card root, and cold-boot from your usual boot disc.
2. Highlight the U.S. **Crazy Taxi**. Run a cover-only scan and return to Games to see the box. Repeat the scan to check that it recognizes the installed cover.
3. Enable the manual and preview and start the scan again. Open the manual, turn a page, then play the preview. Cancel a library scan with **B** and check that you can start another scan.
4. For a hardware failure, capture the adapter name and the status message. The status distinguishes connection, DNS, certificate, storage and provider failures. On BBA, **Network → Inspect adapter** and **Test network** are separate cable and DHCP checks.

The same branch includes [FTP](ftp.md), [browser sharing](browser-companion.md), and the [PC rip receiver](network-rip.md). These services are independent of scanning for game media.
