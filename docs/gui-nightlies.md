# K-UI GUI nightlies

A nightly updates the K-UI GUI on an existing installation. It uses the current
testing-channel KUP producer to package one replacement, `runtime.kui`. The
installed game readers, optional card themes and shared user data carry forward
under the updater’s partial-package rules. The GUI uses the same build settings
as the recent themes GUI, including the default reader admission manifests and
`KUI_SMASH_LAUNCH_TIMING=1`. The nightly also sets `KUI_GITHUB_NIGHTLY=1` so the
console can recognize an installed nightly of the same revision. A standalone reader change needs its own qualified
package.

## Turn on the channel

Merge the updater and `.github/workflows/nightly.yml` into `main`, and enable
GitHub Actions for the repository. To publish the first nightly immediately,
open **Actions → K-UI GUI nightly → Run workflow**, choose **main**, and run it.
GitHub supplies the workflow token. The publish job needs permission to create a
prerelease and tag, and to write the `nightly-channel` branch. The first
successful run creates those automatically.

Actions checks at **08:23 UTC** each day (2:23 a.m. CST, 3:23 a.m. CDT).
It compares the tracked GUI source, build inputs and packaging recipe against
the last published nightly. Multiple GUI commits between checks produce one
nightly. Documentation-only changes and unmodified inputs skip the toolchain
and release. An interruption leaves the prior feed entry in place, so the next
scheduled or manual run can catch up.

## What gets published

Each nightly has its own prerelease tagged `nightly-<40-character source SHA>`
and a public asset named `K-UI.kup`. The prerelease notes include its SHA-256.
The publisher downloads the asset, validates it with the same C package parser
used by the GUI, then atomically advances the feed. The four lines of the
[console feed](https://raw.githubusercontent.com/TacitusVox/K-UI-NeXT/nightly-channel/nightly.txt)
are the format identifier, full commit SHA, byte count and SHA-256. The console
uses the commit to address the specific release asset.

The read-only build job uses Ubuntu 24.04, the repository’s pinned SH-4/KOS
toolchain and FatFs, BearSSL for HTTPS, and the host C compiler for KUP checks.
It runs the updater, loopback HTTPS, and nightly publication tests before
handing the KUP to the publish job. A failed run’s log shows the failing step.
If a published asset exists after an interrupted feed update, a retry verifies
the published bytes and finishes the feed update.
