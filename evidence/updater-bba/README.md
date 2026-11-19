# K-UI WinCE loader and updater hardware test

GUI 2b9493e4df01   Package 75d4fb39c737   WinCE reader 88ff969d505c / F20C6F56

1. Back up KUI/boot.kui, KUI/runtime.kui and KUI/apps/games/ce-boot.kui from your existing standalone K-UI installation. Merge this ZIP's KUI folder onto the card. Keep any existing KUI/WCEDATA/WCEDATA.BIN in place. For a fresh card, if you want to record and the file is missing, copy only the blank KUI/WCEDATA/WCEDATA.BIN from Claude's original data-session ZIP once. It needs about 1.1 GB free.

2. Hold START at power-on to enter the physical test GUI. If the boot disc waits at its menu, choose Start K-UI with A while keeping START held until the GUI opens. Open Settings → Updates. Confirm GUI 2b9493e4df01 and Running ROOT.

3. From Games, launch Bang! Gunship Elite from your usual SCI card with Background reader and CD music Off. Let it reach playable gameplay. The newest KUI/apps/games/last-launch-a.txt or last-launch-b.txt should name apps/games/ce-boot.kui, source build 88ff969d505c and CRC F20C6F56. A fresh receipt confirms the GUI prepared the intended module. Reaching gameplay confirms the game booted. If you are collecting sessions, keep the existing data file and follow Claude's recording procedure. WCE DATA ON SLOT means recording started. During a recording save, keep power on until the blue screen finishes.

4. In Settings → Updates → test, select WinCE-BBA-2b9493e4df01.kup and review package 75d4fb39c737 (testing). Press A to install, wait for verification, then START → Restart K-UI. In Updates, Running and Selected should show the same non-ROOT bank. Launch Bust-A-Move 4 with the same settings and check its new receipt and gameplay. Both GUI variants show 2b9493e4df01. Package 75d4fb39c737 identifies the incoming KUP; Running and Selected identify the installed snapshot, whose ID can differ from the package ID. Write down the bank ID for the next step.

5. Power off and boot normally with the controller released. Confirm Running and Selected still show the same bank ID as in step 4. Run Bang! again from this bank if you need a direct comparison with step 3.

6. For the BBA test, plug in the adapter with the console powered off, connect Ethernet, then start the physical GUI by holding START as in step 2. Confirm the Dreamcast date and time. Open Settings → Updates → L, choose Broadband Adapter on Connection and press A. During the first transfer press B, wait for it to stop, then press A to resume. The downloaded package must show 75d4fb39c737, the same package as step 4. Confirm installation and verification, restart, then check Running and Selected again. If the transfer finishes before you interrupt it, the result still tests a complete download; a subsequent check can reuse its verified cache.

7. For recovery, power on holding START. Running should show ROOT while Selected still names the saved bank. In Settings → Updates, Y reviews Restore previous and R reviews Original baseline. Confirm with A only if you want to change the saved default, then cold boot normally to check that choice. To undo the manual-copy portion of this test, restore the three backups from step 1 after selecting the original baseline.

Package contents
The review KUP replaces runtime.kui and apps/games/ce-boot.kui. User data, other readers, disc images and themes are kept. The physical test GUI uses the review branch feed. The installed GUI uses the production feed. Both route Bang! and Bust-A-Move 4 with CD music Off to the WinCE reader in this package. This is Claude's data-session reader. Other CD music or title-specific paths can use different modules.

Package size: 3,111,664 bytes
KUP SHA-256: 16c5f5c56ee6f9b5c31fe3e8604cc7fbc07549b19bb188662f74c1dd504137d3
WinCE module SHA-256: 68b1a52d68e0b16008c60a9ff83eefae4016af3b71c178662181cf4ad7f37b72
GUI source: https://github.com/TacitusVox/K-UI-NeXT/commit/2b9493e4df012f9f3d1bff2bb1e06b443ef63b3c
WinCE data-session module source: https://github.com/TacitusVox/K-UI-NeXT/commit/88ff969d505c12af963db10876964978fd2ca72e
Known WinCE GUI route: https://github.com/TacitusVox/K-UI-NeXT/commit/33bd22e7fb81c0db129153c20e2b91ce3816d878
