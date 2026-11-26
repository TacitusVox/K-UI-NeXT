# Getting started with K-UI 3.0 RC1

[Project overview](../README.md) · [Hardware and compatibility](hardware-and-compatibility.md) · [Updating an existing installation](release-v3.0.0.md)

K-UI is software you run on the Dreamcast itself. The normal setup uses a
**boot CD to start K-UI** and a **separate SD adapter to store its software,
games and saves**. Once the menu has loaded, you can exchange the boot CD for
an original game disc.

This guide describes the **3.0 RC1 “Muramasa”** release and its current menus.
You do not need to build the source or install DreamShell.

## 1. Check what hardware you have

For this installation route you need:

- A Dreamcast with its original, working GD-ROM drive that can boot a burned
  Dreamcast CD-R (MIL-CD support).
- A controller and a supported SD adapter: an external **SCIF serial-port
  adapter**, or an internally wired **SCI microSD modification**.
- A FAT32 or exFAT SD card, a computer/card reader to copy the files, and a
  compatible K-UI boot disc. For a new disc you also need a CD writer and CD-R.

**If your only SD card is inside a GDEMU, start with the
[GDEMU explanation](hardware-and-compatibility.md#gdemu-and-other-optical-drive-emulators).**
That card is not the SCIF/SCI storage this guide uses. GDEMU is not an officially
supported K-UI setup.

The rear serial-port adapter is **SCIF**. **SCI** is a different, internally
wired connection; a setting cannot turn one into the other. The menu, disc
tools and save tools work with supported SCIF storage, but the current
Background, Windows CE and software virtual-VMU game routes require SCI.

Networking is optional. A physical VMU is useful for normal saves and for
importing existing progress; K-UI's software virtual cards are a separate,
optional feature for supported games.

## 2. Download the ready-to-use package

Open the [K-UI 3.0 RC1 release](https://github.com/TacitusVox/K-UI-NeXT/releases/tag/v3.0.0-rc.1)
and download **`kui-3.0.0-rc.1-sd.zip`** under **Assets**.

| Download | What it is for |
| --- | --- |
| **`kui-3.0.0-rc.1-sd.zip`** | First installation or a manual upgrade. Includes K-UI's SD files and the optional boot-CD image. |
| `kui-3.0.0-rc.1-full.kup` | An update package for an already working, current K-UI updater. |
| `SHA256SUMS.txt` | Checksums for verifying the downloads. |
| GitHub's **Source code** archives | Developer source files, rather than the prepared SD installation. |

Extract the SD ZIP on your computer. At its top level you will find **`KUI/`**,
**`boot-cd/kui-bootloader-v3.cdi`**, **`START-HERE.md`** and **`build.json`**, plus
license information.

## 3. Put K-UI on its SD card

Use **FAT32 or exFAT**. The normal runtime supports a whole-card filesystem or
one supported MBR primary partition with 512-byte logical sectors. GPT and
multi-partition layouts are not the ordinary supported installation. Keep an
already working card's format; an update does not require reformatting.

Copy the ZIP's **entire `KUI` folder into the root of the SD card**. The root is
the first directory you see when opening the card on your computer.

Check these paths before ejecting it:

| Path on the card | Purpose |
| --- | --- |
| **`KUI/boot.kui`** | Software used by the compatible boot disc. |
| **`KUI/runtime.kui`** | Matching K-UI runtime. |
| Other supplied files and folders inside `KUI/` | Matching game readers, theme files, music and supporting data. Copy them together. |
| `Games/` | Optional at first. Put each complete game image in its own folder here when you want to play from SD. |

The two software files should be directly inside **`KUI/`**, not an accidental
`KUI/KUI/` or `kui-3.0.0-rc.1-sd/KUI/` directory. When upgrading, merge the
supplied files and preserve your other games, saves and settings.

Safely eject the card from the computer.

## 4. Prepare the boot CD and start K-UI

If you already have a working **independent K-UI boot CD** compatible with your
storage adapter, keep using it. It does not need replacing for every K-UI
software update. The older DreamShell-based K-UI 1.0 disc is a different boot
environment.

For a new disc, use CD-writing software that supports CDI images and burn
**`boot-cd/kui-bootloader-v3.cdi` as a disc image to a CD-R**. A data CD merely
containing that `.cdi` file will not serve as the boot disc.

1. With the Dreamcast powered off, connect the supported adapter and insert
   its SD card. Connect the controller and insert the K-UI boot CD.
2. Power on. Let the bootloader's countdown finish, or press **A** to start
   K-UI immediately.
3. If you are at the boot menu, select **Start K-UI** and press **A**. Its
   storage choice can use **Auto**, or Left/Right can select your connection.
4. At Home, open **Diagnostics** and check for **K-UI 3.0 RC1**, build
   **`93f13e84d989`**. That is the published RC1 build; a later nightly will
   identify itself differently.

You should now see eight Home choices: **Games, Disc tools, VMU tools,
File Manager, Music Player, Network, Diagnostics and Settings**.

K-UI stays in RAM after loading. For the disc tasks below, wait until Home is
ready, open the lid, replace the boot CD with your game disc, and close the lid.
Keep the SD card connected while K-UI or a game using it is running.

## 5. Learn the basic controls

| Where | Controls |
| --- | --- |
| Most menus | **D-pad/stick** selects; **A** opens or confirms; **B** goes back or cancels. Read the bottom of the screen for that page's actions. |
| Games library | **A** selects a game; **X** favorites/unfavorites; **Y** changes tabs; **L/R** jumps letters; **START** opens library options. |
| Game launch screen | **A Launch** starts the selected image; **Y Options** opens its saved launch choices; **B Details** goes back to inspection details. |
| GD Play | **A K-UI boot**, **X Stock retail boot**, **Y Options**, **START Save cards**. |
| Menu screenshot | Hold **L + R**, then press **Y**. The BMP is saved under `KUI/screenshots/`. |

**X is not Refresh in the current Games library.** Use
**START → Scan for changes** after adding or removing game folders.

## 6. Try something useful

### Play an original disc

1. Insert the game after K-UI has loaded.
2. Open **Disc tools → Play inserted disc**. This opens **GD Play**.
3. Press **Y Options**. Leave **Virtual VMU Off** to use the game's ordinary
   physical-memory-card path. Enable **Region-free** if you want to try a
   supported import through K-UI's launcher.
4. Press **B** to return, then **A K-UI boot**.

These K-UI options apply to the supported native-disc launcher. **X Stock
retail boot** instead uses the console BIOS's region and boot behavior.
The project has confirmed PAL *Shenmue II* Disc 1 booting on an NTSC console;
that result is not a claim that every disc and option work.

### Back up a physical memory card

1. Open **VMU tools → Physical VMUs**.
2. Use **Left/Right** to select the connected card. **A1/A2** mean slots 1/2
   on the controller in console port A; B1/B2 are on port B, and so on.
3. Press **Y Back up all** and wait for completion.
4. Use **R Backups**, or **VMU tools → Backups & restore**, to inspect the
   saved backups on SD.

This task does not require enabling software virtual VMUs or launching a game.
For alternative memory-card devices, see the
[device support explanation](hardware-and-compatibility.md#physical-vmus-and-replacement-devices).

### Copy an original disc to SD

1. Insert the disc and open **Disc tools → Copy a disc**.
2. Press **A** for a new copy, review the confirmation, then press **A** again
   to begin. The default capture format is a GDI with its track files.
3. K-UI normally writes a title-named folder under **`/Games`**, such as
   `Games/MDK2/MDK2.gdi` and that disc's tracks.
4. **B** requests a controlled stop. Wait until the operation/report finishes.
   **X** resumes the matching saved job; **Y** verifies the saved files.

Keep the complete output folder together. For a damaged disc, the separate
**Disc tools → Recover a damaged disc** workflow preserves progress and retries
unreadable areas. Verification and recovery results describe the data actually
read; a partial recovery is not a complete good dump.

### Play a game from SD

1. Put each complete game image and all referenced files in its own folder
   under **`/Games`**. For a GDI, keep the `.gdi` and every track it names
   together. For BIN/CUE, keep the `.cue` and all referenced `.bin` files.
2. Open **Games → START → Scan for changes** if you added files on a computer.
3. Select the title with **A**. For a grouped entry, choose its disc or
   **Original / 2048-byte copy** as prompted.
4. On the launch screen, use **A Launch**. **Y Options** changes that image's
   saved launch settings. Start with its normal settings before experimenting
   with optional services.

Direct-loading paths exist for GDI, supported BIN/CUE and CDI layouts, and
qualifying Dreamcast ISO/BIN/IMG images. **CHD and CSO need the PC importer**;
placing them on the card does not enable direct compressed loading. ZSO has
separate experimental native SCI readers, so it is not the beginner default.
[Format/import reference](games-formats.md).

Windows CE and the current Background/virtual-VMU routes require **SCI**.
Supported native Standard loading remains available on SCIF. A successful
ordinary boot does not establish that virtual saves, CD music and all movies
will work with that title. See the [RC1 compatibility notes](release-v3.0.0-notes.md).

**Returning after play:** wait for the game to finish saving, power the console
off, put the K-UI boot CD back if you exchanged it for a retail disc, and power
on. This is the normal RC1 return workflow; the menu's **B** button is not an
in-game return shortcut.

### Use a virtual VMU for a supported disc game

1. Use the supported **SCI** setup and a qualifying native game.
2. Open **Disc tools → Play inserted disc → Y Options**. Set **Virtual VMU
   On** and choose **A2**, or **A1** if the game requires its first slot.
3. Leave the selected virtual slot physically empty. A physical memory card
   can occupy the other slot.
4. Return to GD Play and press **START Save cards**. Use **START New blank**
   to create a 200-block card, or **Y Link imported card** for existing saves,
   and review the confirmation.
5. To make that card the launch default, select it in the linked-card list
   with **A** and confirm **Use this card by default?** Creating or linking
   another card preserves any previous default until you change it.
6. Return and use **A K-UI boot**. Save and load from within the game normally.

You do **not** need a ripped copy of the game for this disc-card workflow.
To bring over existing progress, start at **VMU tools → Import saves**, review
the matches, and link the resulting card. Importing saves and enabling
**Use virtual VMU** are separate actions. Support is game-dependent; this is
K-UI's software card, not a setting for a VM2 or VMUPro.

### Add covers, manuals and a different look

- **Games → START → Change layout** selects List, Compact, Gallery or Flow.
- **Games → START → Search and filters** helps find titles in a larger library.
- With supported networking, **Games → START → Scan from internet** can fetch
  artwork, manuals and optional gameplay previews. Downloaded page-based
  manuals open on the Dreamcast; PDF-only manuals are saved for another device.
  [Media and network setup](game-media.md).
- **Settings → Appearance** changes the theme, splash, banner, background and
  colors. [Theme guide](themes.md).

## Common setup problems

| What you see | What to check |
| --- | --- |
| Boot CD does not start | The console must support this burned-disc boot method. Check the CDI was written as an image to CD-R, and that the drive can read it. |
| Bootloader appears but K-UI does not load | Check the adapter type, supported card format and the root-level `KUI/boot.kui` and `KUI/runtime.kui` paths. A GDEMU's SD card is not this storage device. |
| The old version starts after a manual update | For the manual SD install, power off and cold boot. **Restart K-UI** may select an earlier installed update version; see [the upgrade guide](release-v3.0.0.md). |
| Games is empty | Put complete image folders under `/Games`, then use **START → Scan for changes**. Check the chosen format is supported. |
| A virtual card is unavailable or a game refuses a launch option | Check SCI storage, the selected empty A1/A2 slot and the title's documented support. Recognizing a disc or save does not prove every loader feature works. |
| Cover/manual download fails | Check your supported network/storage combination and read the status message. Media availability varies by release. For certificate-date errors, check the Dreamcast clock. |

For a report, include the build shown in **Diagnostics**, the game and region,
storage adapter/connection, memory-card device if relevant, and the operation
that failed. **Diagnostics → START → Create problem report** gathers useful
files. [Reporting guide](support-reports.md) ·
[GitHub issues](https://github.com/TacitusVox/K-UI-NeXT/issues).
