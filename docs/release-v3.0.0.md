# Installing K-UI 3.0 RC1

With a current K-UI updater, place the release candidate’s `.kup` in `KUI/updates/test/`. Open **Settings → Updates**, select the package, review and install it, then select **Restart K-UI**. The updater verifies and selects the complete software set in a bank on the card. Games, virtual cards, and preferences stay on the card.

For physical startup from the SD card, power down and merge the SD installation ZIP’s `KUI` directory into the card root. Replace the supplied files and keep your other files. The ZIP supplies matching `boot.kui` and `runtime.kui` files. Cold-boot with your compatible independent K-UI boot disc. An existing working disc can continue to load the updated runtime.

Open **Diagnostics** and check for **K-UI 3.0 RC1** and the build shown in the distribution’s `build.json`. A manual SD install updates the physical files. **Restart K-UI** can still select a previously installed bank.
