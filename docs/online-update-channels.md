# Publishing online K-UI updates

Open the Main or Nightly channel to choose a package before downloading it.
Each channel lists up to ten builds by GitHub publication time, newest first.
The picker identifies the exact version or source build, download size, scope,
and declared changes to the GUI, readers and other managed software. The
default nightly contains the GUI. A maintainer can also publish a reader-only
update, a mix of components, or a complete release. Each component retains
its own build ID.

The Main channel offers published, verified full stable KUPs. A release contains the exact GUI, game readers and other
managed software from its installation ZIP. The console compares their
hashes against the installed files. Save data and user settings live outside
the package. The separately distributed boot CD, physical GUI aliases and
any installation-only bootstrap steps are described in the release's SD
installation instructions.

## Select the contents of a nightly

With no `config/nightly-package.json`, the workflow builds the GUI from
`main` as usual. To include other software, place already tested files under
`resources/nightly-software/`, preserving their paths relative to `KUI/`.
Commit both the files and their reviewed SHA-256 values in
`config/nightly-package.json`. Obtain a file's value with `sha256sum`.
For example, a partial update of the GUI and the generic WinCE reader uses
the following recipe after filling in the reader's hash.

```json
{
  "gui": true,
  "full": false,
  "files": {
    "apps/games/ce-boot.kui": "replace with the 64-character SHA-256 of the tested reader"
  },
  "remove": []
}
```

Set `gui` to `false` for a partial containing only the selected readers or
assets. The job then skips the GUI compiler. With `gui` set to `true`,
the GUI comes from the verified native build of this commit. Reader binaries
come directly from the named Git blobs and retain their embedded identities.
List obsolete managed paths in `remove` to make them unavailable in the
selected installation. With `full` set to `false`, all unlisted components
carry forward.

For a complete nightly, use a recipe containing just
`{"full_release": "vMAJOR.MINOR.PATCH"}`. Commit it on the source revision
intended for that release. Publish the matching stable KUP, installation ZIP
and SHA-256 manifest, then run the nightly workflow for the same commit. The
publisher compares the stable KUP with the checked installation ZIP, verifies
the GUI build and tag, and places those exact software bytes into a testing
package. A missing or mismatched complete release stops publication. Use a
partial recipe while only individual components are qualified.

The recipe remains active until changed or removed on `main`. The packaging
and publishing jobs validate the inventory against that source revision and
check the final KUP with the console decoder. GitHub's publication receipt
ties each package, including a reader-only one, to its SHA-256 and commit.

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
