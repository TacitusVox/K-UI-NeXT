# Toy Commander evidence

This folder preserves the recovered data from numbered Tests 1–66. Start with [the per-test index](metadata/INDEX.md). The machine-readable [index](metadata/evidence-index.json) records report hashes, report sizes and package membership.

## Original console records

`console/` contains 65 original uploaded text files. Sixty-one numbered tests have populated reports. Tests 19 and 22 contain allocation-failure placeholders. Test 30 contains a failed-save placeholder. Test 26 has two separate report uploads. The duplicate Library copies of Test 28 have identical bytes and appear once.

The available measurements for Test 30 and Test 35 are preserved in [transcriptions](transcriptions/README.md). Test 43 reset before a populated report could be saved. Its reset observation is recorded in the index. The final Test 63 is the physical GD-ROM control.

Test 66 retains the final attached log and the owner’s [mild-improvement observation](analysis/test-66-console-outcome.md). No full-playback success is claimed.

## Package identity

`packages/` retains the original instructions and checksum list from 45 recovered delivery or local-candidate archives. Each `inventory.json` verifies every member's size and SHA-256. KUI module entries also contain the validated embedded build, payload CRC32 and memory size. Some later archives include retained controls, so package membership is broader than the test named on the archive.

The console report's embedded build identifies the sampled reader. A GUI build, archive name or successful compile can identify a different component. The Test 33 discrepancy and superseded local candidates remain explicit in the index.

[Compiled provenance](metadata/compiled-provenance.md) links available source objects and clean retained ELF matches. Missing early source objects remain identified.

## Host and linked records

`host/logs/` preserves build output, sanitizer runs, linked stack receipts and original-backed execution receipts. Exploratory and failed runs are retained with their original filenames. Final checks can be identified from the exact packaged build and compiled provenance.

`host/reproduction/` preserves the session's supplemental Python, C and shell probes. `host/fixtures/` preserves authored formatter and state-machine fixtures, with executable host binaries omitted. `host/gui/` preserves cold route, report reservation and filesystem checks. These records are host simulation or linked instruction evidence. Console playback conclusions require the original reports and the owner's observations.

Some supplemental scripts use the original session layout and paths. They require the corresponding source checkout, compiled ELF inputs and, where indicated, the owner's legally obtained original executable. The private executable's SHA-256 is recorded in the scripts. Game binaries, movies, disc images, saves, private disassembly dumps, transfer credentials and room photographs are excluded.

`analysis/` contains the earlier November 11 investigation and the owner's supplied AVI scan metadata. These are historical analysis with their own stated limits. Later numbered reports should be read alongside them.

## Integrity

`SHA256SUMS.txt` covers every staged artifact other than the checksum file itself and its machine-readable inventory. `metadata/files.json` records each covered file's size and SHA-256. The package inventories cover module bytes without including those binaries in this text evidence folder.
