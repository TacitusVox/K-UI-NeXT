# Broadband Adapter updater test

This directory gives the pre-merge review build a live HTTPS target. Its
GUI package comes from the clean production-channel source `30155a97c524`.
The optional review GUI was built from `b8b1fc445da6` with
`KUI_UPDATE_REVIEW_FEED=1` and `KUI_GITHUB_NIGHTLY=0`. Normal builds use the
public release channel. The adjacent [source bundle](source.bundle) retains the original build
commits. In a clone with the baseline, run `git fetch /path/to/source.bundle HEAD`
to inspect those exact revisions.

1. Back up your existing `KUI/boot.kui` and `KUI/runtime.kui`. Copy the
   review manual ZIP’s `KUI` folder onto the existing card. Cold boot using
   the usual disc. The running GUI should show `b8b1fc445da6`.
2. Open **Settings → Updates**, press **L**, select **Broadband Adapter**,
   then press **A**. The Dreamcast clock is used for HTTPS validation.
3. At the installation review, check for **GUI 30155a97c524 / package
   e0af44ad15ec** on the testing channel. A completed download is
   3,001,056 bytes. Press **A** to install, then **START → Restart K-UI**.
   The running GUI should now show `30155a97c524`.
4. To test an interrupted transfer, press **B** partway through step 2,
   allow the operation to stop, and start it again before installation.
   The updater resumes when GitHub serves the requested byte range.
5. Cold boot returns to the physical review GUI. Restore your backed-up
   aliases and cold boot when you want the original physical installation.

The installed target uses the production nightly feed on its next online
check. That feed first becomes available after the workflow is on `main`
and a nightly has been published. Installed readers, themes and saves are
outside this GUI-only transfer.

SHA-256 of `K-UI.kup` is
`37ca854ba9e19cfde478106416701af9b51d1384d71b8c2b272e8d535bbdafa9`.
