# Publishing online K-UI updates

The nightly feed distributes the GUI built from the checked-out source. A nightly's
one `runtime.kui` entry is its full list of replacements. The updater keeps
installed game readers. An older installation can therefore require a full
software release to bring its readers up to date.

The stable feed tracks the latest published GitHub release with a verified
full, stable KUP. A release contains the exact GUI, game readers and other
managed software from its installation ZIP. The console compares their
hashes against the installed files. Save data and user settings live outside
the package. The separately distributed boot CD, physical GUI aliases and
any installation-only bootstrap steps are described in the release's SD
installation instructions.

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

## Choose a previous nightly

The Nightly channel offers up to ten published GUI builds, newest first, with
their original GitHub publication dates in UTC. Each selection has its own
source commit, package length, SHA-256 and change notes. A previous build
uses its original release asset. The console compares its downloaded bytes
with that build's checksum. The same list has links for computer downloads
on the [nightly channel page](https://github.com/TacitusVox/K-UI-NeXT/tree/nightly-channel).

When the picker is first deployed, the publisher checks previously promoted
builds against their original release assets and verifies their KUP files.
The list starts with as many verified releases as exist. The picker retains
ten choices; previous releases remain available individually on GitHub.
