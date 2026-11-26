# Hardware and compatibility

K-UI is developed for a Dreamcast that keeps its original GD-ROM drive and
uses a supported storage adapter. This page explains which hardware supplies
which features. For downloads and installation, start with
[Getting started](getting-started.md); see the [project overview](../README.md)
for what K-UI does.

## What do I need to get started?

- A Dreamcast capable of booting the supplied homebrew **MIL-CD** boot image,
  with a working original GD-ROM drive.
- A compatible independent **K-UI boot disc**. The RC1 SD ZIP includes the
  optional Bootloader v3 CDI image; an existing compatible K-UI disc can
  continue to be used.
- A supported **SD connection**, with a FAT32 or exFAT card containing the
  supplied `KUI` folder. Choose the connection according to the features below.
- A controller and display. Games normally use a physical memory card unless
  you enable a supported K-UI virtual-VMU route.

The normal workflow is to boot K-UI from its CD, load its software from SD,
then choose an app. Once K-UI is running, you can swap the boot CD for an
original game disc for disc tools or GD Play. Internet access is optional;
online downloads require supported networking hardware.

See [boot-disc details](boot-disc-v3.md) and the
[console boot requirement](hardware-test.md).

## Which SD device do I have?

**The place the card plugs in matters.** These connections provide different
services, even though several use an SD or microSD card.

| Hardware | What it provides | Current K-UI scope |
| --- | --- | --- |
| **External serial-port SD adapter — SCIF** | K-UI storage through the Dreamcast's rear serial port. | Supported shell/storage use and supported native Standard-loader games. It does not enable the SCI-only features below. |
| **Internal SCI microSD modification — SCI** | A separate SD connection requiring internal hardware work. | Supported storage, Background loading, Windows CE loading and qualifying software virtual-VMU routes. Game and option compatibility still varies. |
| **GDEMU or another optical-drive emulator (ODE)** | Replaces the optical drive and supplies emulated discs from its own storage. | Not officially supported. K-UI has no implemented GDEMU-card browser or image-switching integration. |
| **VM2, VMUPro or another memory-card device with storage** | Saves and virtual-card banks managed by a controller-slot peripheral. | Its storage is not a K-UI system/game drive. Named-device compatibility and automatic switching are not established by K-UI. |
| **IDE/CF hardware** | An experimental storage connection through the G1 bus. | Implementation exists, but physical-console validation remains outstanding. It is not a validated beginner setup. |

SCI is not another name for the ordinary external serial-port adapter. K-UI's
[transport implementation](../include/kui/storage.h) distinguishes SCIF, SCI
and IDE/CF; the [storage guide](storage-transports.md) explains their ownership
and selection.

## GDEMU and other optical-drive emulators

**GDEMU and other ODEs are not officially supported.** K-UI's Games menu
browses its own supported storage. It does not currently enumerate the
numbered folders on a GDEMU card or tell GDEMU which disc image to mount.

Booting a K-UI disc image through an ODE would be a separate, unvalidated
setup. It would not automatically make that device's card available for
K-UI's files, games or virtual saves. The project's support boundary does
not establish that every possible ODE arrangement is technically impossible.

For the device's own behavior, see the official
[GDEMU installation](https://gdemu.wordpress.com/installation/gdemu-installation/)
and [operation](https://gdemu.wordpress.com/operation/gdemu-operation/) guides.
Features involving original physical discs require the original drive.

## What is K-UI's virtual VMU?

It is **software save-storage emulation**. On a supported launch, K-UI presents
a memory card to the game and stores its contents on K-UI's SD card. You do
not need a VM2 or VMUPro for this feature. Each normal virtual card has
**200 user blocks**, and you can keep separate cards for games or save sets.
It does not provide a handheld VMU screen or run VMU mini-games.

The current integrated in-game virtual-card routes require **SCI storage** and an
eligible game/loader combination. Physical-disc virtual VMUs are for qualifying
**native Dreamcast games** (games that do not use Windows CE): the original disc supplies the game, while SCI
stores the saves. Supported Windows CE virtual-save combinations have their
own requirements; Windows CE loading alone does not imply virtual-save support.

Choose virtual slot **A1 or A2** where offered. **The selected virtual slot
must be physically empty.** A real device in that slot conflicts with the
software card. The other slot can remain physical on the supported native
route. A1 and A2 are the two accessory slots on the controller in console
port A. See [virtual-slot rules](native-virtual-a1.md),
[cards for original discs](disc-card-linking.md) and
[physical-disc VMU scope](physical-disc-vmu-plan.md).

## Physical VMUs and replacement devices

These are physical memory-card devices with their own firmware and storage.
K-UI's VMU Manager uses standard **Maple** memory-card operations—the protocol
used through the controller—but that does not certify every replacement card.
K-UI currently makes no validated VM2/VMUPro compatibility claim.

Ordinary game saving, VMU Manager backup/import and automatic per-game card
switching are separate capabilities. The manufacturers describe GameID or
other device-specific switching mechanisms in the
[VM2 documentation](https://www.dreammods.net/store/p6/VM2.html) and
[VMUPro documentation](https://www.8bitmods.wiki/vmupro). K-UI does not currently
provide documented integration with those mechanisms.

Using a physical card does not require enabling K-UI's **Virtual VMU** option.
That setting controls K-UI's software card; it does not configure an external
device or expose that device's microSD as K-UI storage.

## Filesystems, networking and game compatibility

Use **FAT32 or exFAT** for the current runtime, apps and Games library. The
bootstrap's limited read-only ext4 support does not provide runtime ext4
support. See [filesystem scope](storage-filesystems.md).

SCI storage occupies the same interface used by the supported W5500 and
ESP32 Wi-Fi setups; those network configurations use SCIF storage. The
Broadband Adapter leaves SCI available for storage. See
[network media setup](game-media.md).

Hardware support is only part of compatibility. Region, image format, reader,
virtual-card and CD-music options can affect an individual game. A successful
boot does not establish working saves, music or a complete playthrough. Use
the [RC1 coverage notes](release-v3.0.0-notes.md) for the released scope and
[accepted results](accepted-progress.md) for configuration-specific evidence.
