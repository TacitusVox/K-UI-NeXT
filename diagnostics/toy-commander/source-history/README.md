# Original source histories

These bundles restore the original reader, GUI and physical-disc source commits and their reachable histories. Each contains the exact set of source objects absent from the prerequisite K-UI-NeXT main commit. Shared main objects are omitted by object ID. Bundle object membership was checked against that set difference.

The prerequisite is `776c9259c00cf2aa8acd341f5ab8ac14017d5898`. Each bundle was verified and fetched into a fresh depth-1 clone of that remote main commit. Original branch HEADs and tree IDs matched. Every reachable object resolved and `git fsck --full --no-reflogs` passed.

| Source | Original HEAD | Packed objects | Bundle bytes |
| --- | --- | --- | --- |
| reader | `3124522ea906c8bd9c9cdcfd2ac5f1e87a99f3ae` | 666 | 509,110 |
| gui | `09c9a4b686d4b469e9cd18c7a86d5a03bf874eae` | 278 | 375,014 |
| disc | `5034eb26898d25b7a2fc8021fbb842962699c5c2` | 84 | 332,379 |

## Restore

Run these commands from a clone of K-UI-NeXT containing this directory. Fetching the prerequisite by exact ID also supports a later shallow clone.

```sh
git fetch --depth=1 origin 776c9259c00cf2aa8acd341f5ab8ac14017d5898
git bundle verify diagnostics/toy-commander/source-history/reader-history.bundle
git fetch diagnostics/toy-commander/source-history/reader-history.bundle refs/heads/toy-reader-history:refs/heads/toy-reader-history
git fetch diagnostics/toy-commander/source-history/gui-history.bundle refs/heads/toy-gui-history:refs/heads/toy-gui-history
git fetch diagnostics/toy-commander/source-history/disc-history.bundle refs/heads/toy-disc-history:refs/heads/toy-disc-history
git log toy-reader-history
git show toy-gui-history
git show toy-disc-history
```

The `manifest.json` records complete original refs, trees, SHA-256 checksums, sizes and verification results. Each bundle contains its source history independently, so importing another bundle first is optional.
