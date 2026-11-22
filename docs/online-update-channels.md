# Publishing online K-UI updates

Open the Main or Nightly channel to choose a package before downloading it.
Each channel lists up to ten builds by GitHub publication time, newest first.
The picker identifies the exact version or source build, download size, scope,
and declared changes to the GUI, readers and other managed software. New nightlies include the complete software for their main commit.
Each component retains its build ID. The release page supplies three assets.

| Asset | Use |
| --- | --- |
| `K-UI.kup` | Choose the nightly in Settings → Updates on an installation with the current software updater. |
| `K-UI-SD.zip` | For a clean card or an older GUI, including stock K-UI 1.7, follow the included instructions and merge the supplied KUI folder onto the card. Use the compatible independent boot CD. Both boot paths start the supplied GUI. |
| `SHA256SUMS.txt` | Check the two downloads on a computer with `sha256sum -c SHA256SUMS.txt`. |

Nightlies are prereleases. The Main channel offers verified full stable KUPs.
Games, cards, settings and personal themes remain outside managed software.

## Build the complete nightly

Each push to `main` checks for changed software inputs and can publish a fresh
testing build immediately. The scheduled daily run retries an unpublished
revision. A documentation-only change keeps the existing nightly.

The workflow uses `config/distribution.json` for the reviewed file paths and
SHA-256 hashes and `tools/stage_distribution.py` to prepare them. It links
the main commit's GUI with the declared Smash bundle and matching Standard
VMU reader binding. Staging checks the reader inputs, and the publisher
verifies the same managed software in the KUP and SD ZIP, including their
GUI identity. A reader source or build recipe change needs a matching review
of the distribution inventory. A missing reader, checksum mismatch, stale
input record or absent distribution manifest stops that night's publication.

A verified run publishes the KUP and manual installer on the same nightly
prerelease, then advances the testing feed. Existing tagged nightlies keep
their original bytes and recorded scope. Clients that only understand the
old GUI-only feed keep their last compatible choice. Use the SD installer
to bring an older GUI directly to the current software updater.

## Publish a stable update

1. Stage the accepted GUI and matching installation's complete managed
   software inventory. The GUI's build ID must match the first 12 digits
   of the release's source commit. Each game reader retains its own build
   ID. The existing release packager produces a full stable KUP. To package
   a prepared software directory directly, run
   `python3 tools/make_update.py staged/KUI kui-VERSION-update.kup --release --channel stable`.
2. Use a `vMAJOR.MINOR.PATCH` tag on the verified source commit and publish
   the installation's `*-release.zip`, its matching full `.kup`, and
   `SHA256SUMS.txt` with both asset digests. Keep the release build record
   in the installation ZIP. Drafts can be prepared ahead of time.
3. Publish the release as a full release and mark it latest. The
   **Update stable release channel** workflow checks the package with the
   console parser, verifies the ZIP and embedded component hashes, and
   publishes the official feed on `release-channel`. The repository's
   release-promotion workflow performs the same check for bot-created
   releases. To retry, run **Update stable release channel** on `main`.

The feed stores the source commit, asset size, SHA-256, tag and filename.
Publishing also writes `update-info.txt` with the same source and digest.
The console can show up to four recent changes, drawn from release bullets
or source history. A running build is known to be older when its commit
appears among the target's recorded Git ancestors. Historical or private
builds outside that bounded history have unknown order.

The original 2.0.0 release contains installation ZIPs and predates
the network updater. Its files remain available for manual installation.
The first compatible stable KUP must come from an actual subsequent
release. A user running an older GUI without online updates must first
install a compatible release using its SD installation instructions.

## Choose an earlier update

Each choice has its own release asset, SHA-256 and change notes. The console
checks the downloaded bytes and package manifest against that selection.
Declared replacements, removals and keeps describe the package. The installed
files determine how many replacements are needed on a particular console.

Computer downloads use the same [nightly list](https://github.com/TacitusVox/K-UI-NeXT/tree/nightly-channel)
and [stable release list](https://github.com/TacitusVox/K-UI-NeXT/tree/release-channel).
Both lists use GitHub publication times in UTC. For releases with the same
timestamp, the stable list shows the higher version first.

The publisher verifies existing eligible packages and includes as many real
builds as exist. Older GUI-only updaters keep receiving their latest compatible
single-file GUI nightly through the original feed. A newer reader-only update
is available to consoles with the expanded picker. Older release assets
remain accessible individually on GitHub after they leave a ten-build list.
