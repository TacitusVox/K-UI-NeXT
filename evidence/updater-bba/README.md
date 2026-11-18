# Broadband Adapter updater test

The review branch serves a GUI-only KUP for a Dreamcast hardware test before
production nightlies go live. The package and the current manual test GUI
were built from commit `bad8016cee9aca559d52a1bef51c2c8e695d2418`.

1. Back up `KUI/boot.kui` and `KUI/runtime.kui` on the card. Copy the
   manual test ZIP's `KUI` folder over the existing installation, then cold
   boot from your usual disc. If you already installed the earlier review
   GUI `b8b1fc445da6`, you can run the download test from that GUI.
2. Check the Dreamcast clock. Open **Settings → Updates**, press **L**, select
   **Broadband Adapter** on **Connection**, then press **A**. To test resume,
   press **B** during the transfer, let it stop, then start it again.
3. The review should show GUI **bad8016cee9a**, package **ebff3330053f**,
   channel **testing**. The download is 3,010,292 bytes. Press **A** to
   install, then **START → Restart K-UI**. The old review GUI now shows the
   new build ID. The current manual ZIP already shows that source ID, so
   check the package ID at review and the selected bank after restart.
4. On the new GUI, open **Settings → Updates → L** and move down to
   **Downloads to keep**. Choose 1, 3, 5, 10, or Unlimited. Press **A** on
   that row to save and apply the setting offline. Five is the default. A
   single review package can exercise download, resume and installation.
   Trimming multiple versions is exercised in the FAT32/exFAT tests.

The physical test GUI uses the review feed. The installed GUI checks the
production feed, available after the first successful nightly on `main`.
Cold boot starts the physical GUI again. To restore your previous physical
GUI, copy back your two backups and cold boot.

KUP SHA-256 is `4125d0918146692475f1a1be7ac24d5d6baeef864324a0e0dbeb6829dc51ed96`. The [source bundle](source.bundle)
in this directory preserves the earlier review GUI sources. The new build
uses the public commit above. See [software updates](../../docs/updater.md)
for version compatibility and [GUI nightlies](../../docs/gui-nightlies.md)
for production setup.
