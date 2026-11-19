# Verified updater cold-boot and Broadband Adapter test

The manual test ZIP includes a physical GUI configured with this review feed and the same KUP served here for offline testing.

Back up KUI/boot.kui and KUI/runtime.kui. Copy this ZIP's KUI folder
onto your existing standalone K-UI card, merging directories. On this
first boot, enter the physical test GUI using your usual boot disc.
Hold START as you power on. If the disc waits at its menu, select Start K-UI with A and keep holding START until the physical GUI opens. This works even if an older update is already selected.

With a Broadband Adapter, check the console clock, open Settings → Updates,
press L, choose Broadband Adapter on Connection and press A. To test
resume, press B during the transfer, wait for it to stop and start it
again. For an offline test, open Settings → Updates → test, select
Cold-boot-67d1ac04091f.kup and press A. Both routes offer the identical package.

At review, confirm GUI 67d1ac04091f, package f1a5254e200a, channel testing.
Press A to install and wait for verification. Select START → Restart K-UI.
In Settings → Updates, check that Running and Selected show the same bank ID. Then power off, power on without a recovery button and
check the running bank again. Both routes should enter the installed
version. The physical GUI and installable GUI were built from the same
source, so their displayed GUI build IDs alone cannot distinguish them.

If the new GUI hangs, hold START as you power on. If the disc waits at its menu, select Start K-UI with A and keep holding START until the physical GUI opens.
Recovery leaves the saved selection intact. In Settings → Updates,
Y reviews the previous selection and R reviews the
original baseline. Press A to confirm the restore you want. This makes
that version the default for later boots. A one-time physical recovery
also lets you test a fresh download from the branch feed. The installed
GUI uses the production channel after the first main nightly is published.

KUP size in bytes: 3010232
KUP SHA-256: 2fcaf33d0869015516de52d095fb04e5f654277703af487372c4c75947e37976
Source revision: 67d1ac04091fb6a9ee82c6d34eccb7bbb9a223e9

The download limit defaults to five. In Settings → Updates → L, move to
Downloads to keep, choose the limit using left/right and press A. The
manually copied test KUP stays in the test folder.

See the [updater guide](../../docs/updater.md) for installation and recovery and the [nightly guide](../../docs/gui-nightlies.md) for publishing.
