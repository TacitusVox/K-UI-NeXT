# Interrupted-chat recovery

Recovered on November 12, 2026 (America/Chicago).

The prior chat's workspace survived. Its last prepared artifact was the final
Test 66 ZIP, built from clean reader commit `3124522ea906c8bd9c9cdcfd2ac5f1e87a99f3ae`
and clean Card Tools GUI commit `09c9a4b686d4b469e9cd18c7a86d5a03bf874eae`.
The ZIP was complete before this recovery; it was not rebuilt or changed.

At recovery, the public handoff branch pointed to
`41b5ad3f083c139022b247f4ab10e9db881d2f86`, with source through Test 65 and
71 diagnostic files. Remaining host evidence, compiled modules, histories and
Test 66 had not been published. This recovery publishes those remaining artifacts
and Test 66 on the same branch, preserving
`41b5ad3f083c139022b247f4ab10e9db881d2f86` as an ancestor. The user explicitly
approved the complete handoff to `TacitusVox/K-UI-NeXT`, branch
`codex/toy-commander-handoff`, after the archive upload was initially blocked
by automatic approval review.
There is no recovered error log establishing why the chat itself stopped.
An interrupted publication is observable; its platform cause is unknown.

## Independent checks

- All 1,082 checksummed evidence files passed verification. The full evidence
  archive contains 1,084 files, including its inventory and checksum list.
- The compiled catalog preserves 114 unique original modules from 44 archives,
  including original member paths, instructions, inventories and checksums.
  Original ZIP container hashes are recorded; earlier ZIP containers are not
  all duplicated as standalone files.
- Reader, GUI and physical-control history bundles were independently imported
  against the published main checkpoint; exact original heads and trees, object
  completeness and Git fsck results are recorded in `source-history/`.
- Test 66 ZIP CRC and every packaged SHA-256 passed. Reader and GUI module bytes
  match their retained builds and clean commit provenance. The ZIP SHA-256 is
  `71a11087f6ab0b486f47f241a822d20dc5bb4f081b8e6dfdacf43d66b999e3b8`.
- Twelve final reader qualification checks passed. GUI launch routing and report
  reservation passed on FAT32 and exFAT. These are host and linked-instruction
  checks; the owner has now tested Test 66 and reported mild improvements.

## What Test 65 established

The report records 2,019 polling errors and six take errors, with zero raw-sector
rejections and zero producer abandonments. Playback submitted only 91 bumper
frames and 32 intro frames before lag exits. Correct byte recovery did not meet
the playback deadlines. Host regression success was insufficient evidence for
the console result.

Test 66 replaces the separate transport pump with the established native reader
and its existing retries and bounded scheduled refill. One controlled scheduled
request required 22 foreground calls versus 65 with Test 65. A repeated-overrun
replay required 135 versus 132 calls, so that replay showed no improvement.
No full-speed console playback claim follows from these host results.

## Preserved gaps

Unavailable early source objects remain identified in the provenance index.
Tests 19 and 22 have allocation-failure placeholders; Test 30 has a failed-save
placeholder and a sourced transcription; Test 35 is transcribed; Test 43 reset
before a populated report. Recovered user observations and original report bytes
take precedence over host tests. Missing evidence has not been invented.

The Test 66 ZIP is the single final console test authorized by the user.

## Final Test 66 evidence received

The attached `66(1).txt` identifies reader `3124522ea906`. Its original 1,024 bytes are preserved in `evidence/console/66.txt`. The owner reports mild improvements; no complete playback success is claimed. The complete `evidence-1-66.tar.xz` adds this report, its observation and updated indexes to every recovered artifact in the original 1–65 archive. The earlier archive is retained unchanged.
