# Broadband Adapter updater test

This review feed serves a single GUI package for existing standalone K-UI
installations. The manual test GUI and the installable GUI were both built
from commit `8cf00aa5f2f1d7e3b76fd49959c8030c1149af5c`. The manual GUI
downloads from this branch. The installed GUI uses the production channel.

1. To exercise the new installation checks from the first download, back up
   `KUI/boot.kui` and `KUI/runtime.kui`, copy the current manual test ZIP's
   `KUI` folder onto the card, then cold boot your usual disc. Confirm GUI
   **8cf00aa5f2f1**. Earlier review GUIs **bad8016cee9a** and **b8b1fc445da6**
   can fetch this package too. Their first installation runs the older
   installer. After restarting into the new GUI, you can select the cached
   KUP in **Settings → Updates** to exercise its installation checks.
2. Check the Dreamcast date and time. Connect the Broadband Adapter, open
   **Settings → Updates**, press **L**, choose **Broadband Adapter** on
   **Connection**, then press **A**. To test resume, press **B** during
   the transfer, wait for it to stop, then start it again.
3. Confirm GUI **8cf00aa5f2f1**, package **74d1da86d711**, channel
   **testing** and a download of **3,010,292 bytes**. Press **A** to
   install. The new installer finishes the bank writes, reopens the files
   on the card, and verifies the replacements against their recorded size
   and SHA-256 before selecting the bank. After writing the selection it
   checks the files again and confirms that the expected selection still
   wins. If this final file check fails, the installer restores the previous
   selection only when it can verify the prior files and selection. An
   uncertain result is reported as a failure.
4. Choose **START → Restart K-UI** to run the selected bank. The current
   manual and installed GUIs share a source ID, so check the package ID at
   review and the selected bank after restart. A power cycle loads the
   physical GUI from the card. **Restart K-UI** re-enters the selected bank.

The default download limit is five. Open **Settings → Updates → L →
Downloads to keep**, pick a limit with left/right, then press **A** on
that row to apply it offline.

KUP SHA-256 is `36dc2383a98d6ba2b6becae875539342ebf8854f93a50e66728e4b5313b8c4ea`.
The nightly packages GUI software. See the [updater guide](../../docs/updater.md)
for package and boot behavior and [GUI nightlies](../../docs/gui-nightlies.md)
for production setup. The [source bundle](source.bundle) retains the original
**b8b1fc445da6** review build.
