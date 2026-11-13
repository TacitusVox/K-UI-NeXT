# Compiled Toy Commander test archive

`compiled-tests-1-65.tar.xz` preserves 114 unique original KUI modules recovered from 44 test archives. Full SHA-256 filenames deduplicate repeated modules. It also contains each original archive README and checksum list, exact archive-to-module paths, validated embedded builds and payload CRCs.

The 142,518,680 bytes of original software compress to 2,989,788 bytes in the catalog. Every decompressed file was checked against its staged source. Every software module was checked against its original ZIP member.

Extract with `tar -xJf compiled-tests-1-65.tar.xz` into an empty directory. Read the included README and select the historical archive mapping required for a test. Run `sha256sum -c SHA256SUMS.txt` inside the extracted catalog to verify its 249 files.

The original ZIP hashes and sizes are retained. The catalog preserves original member bytes and paths. The ZIP container byte streams are represented by their original hashes.

See `../evidence/metadata/INDEX.md` for the console results and source provenance gaps.
