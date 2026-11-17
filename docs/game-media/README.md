# Games cover, manual and preview update

The adjacent patch contains the source, tests, setup notes and decoder license for the test build `d60672f986a7`. Its base is `a8fa1a892f254dcfa8b17e182ea6c4b15823d811`. Its verified Git source tree is `0422b54dc14e753c8f9a9d8ef69665250b9f99c0`.

To build from the exact source tree, start in a clone of this repository.

```sh
git fetch origin codex/bba-network-audit
git show origin/codex/bba-network-audit:docs/game-media/cover-preview-changes.patch > /tmp/kui-game-media.patch
git worktree add --detach ../kui-media-build a8fa1a892f254dcfa8b17e182ea6c4b15823d811
cd ../kui-media-build
git apply --index /tmp/kui-game-media.patch
git write-tree
```

The last command prints the source tree above. Follow the setup in `docs/game-media.md` from this worktree. The installed K-UI fetches art and manuals directly on the Dreamcast. Cached previews play once after six seconds on a title. The test package includes the updated GUI and both boot filenames.
