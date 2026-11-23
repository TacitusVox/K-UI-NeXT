# Installing or updating to K-UI 3.0 RC1

**New to K-UI? Start with [Getting started](getting-started.md).** It explains
the supported hardware, SD adapter, boot disc and first-use controls. GDEMU and
replacement-memory-card questions are covered in the
[hardware FAQ](hardware-and-compatibility.md).

The public **3.0 RC1 “Muramasa”** build is **`93f13e84d989`**.
[Release downloads](https://github.com/TacitusVox/K-UI-NeXT/releases/tag/v3.0.0-rc.1).

## Choose your download

| Your starting point | Download and route |
| --- | --- |
| New installation, stock 2.0 Tyrfing, or an older independent K-UI without the current updater | **`kui-3.0.0-rc.1-sd.zip`**; use the manual SD install below. |
| Working current K-UI updater | **`kui-3.0.0-rc.1-full.kup`**; use Settings → Updates. |
| Both ordinary power-on and Restart K-UI should open RC1 | Install the SD ZIP, cold boot it, then install the matching full KUP from that current GUI. |

The two downloads contain the same managed software. Their installation and
startup paths differ. Games, virtual cards, saves and preferences are user data
and stay on the card.

## Manual SD install: first use or ordinary cold boot

1. Download and extract **`kui-3.0.0-rc.1-sd.zip`**.
2. With the Dreamcast powered off, merge its **`KUI` folder into the root of
   your supported FAT32/exFAT card**. Replace the supplied software files and
   preserve your other files. Copy the complete software folder, including
   both **`KUI/boot.kui`** and **`KUI/runtime.kui`**.
3. Safely eject the card and return it to the supported adapter.
4. Cold boot using your compatible independent K-UI boot disc. An existing
   working disc can keep loading the updated software.

For a new disc, the SD ZIP includes **`boot-cd/kui-bootloader-v3.cdi`**. Burn it
as a CDI disc image to CD-R; see [first boot](getting-started.md#4-prepare-the-boot-cd-and-start-k-ui).
The older DreamShell-based K-UI 1.0 disc cannot serve as this independent
bootstrap.

## Install a KUP with a current updater

1. Copy **`kui-3.0.0-rc.1-full.kup`** to **`KUI/updates/test/`** on the card.
2. Open **Settings → Updates**, select the package, review it and confirm
   installation.
3. Choose **Restart K-UI** to run the verified software version selected by
   the updater.

This uses an installed software set in a separate directory on the card. It
does not replace the manual ZIP step for the physical files loaded at power-on.
Likewise, manually copying the ZIP does not clear an earlier updater selection:
**Restart K-UI may still select that earlier version** until you install/select
the matching update.

[Online update channels](online-update-channels.md) · [Updater reference](updater.md).

## Confirm what is running

Open **Diagnostics** and check for **K-UI 3.0 RC1**, build **`93f13e84d989`**.
The ZIP's **`build.json`** identifies that release's software. If checking both
startup paths, confirm after a cold power-on and again after **Restart K-UI**.

The GUI build identifies the menu. For a game-specific compatibility report,
include its selected reader and settings when available; one successful game
launch does not validate every optional service.
