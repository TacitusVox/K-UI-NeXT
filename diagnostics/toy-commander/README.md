# Toy Commander SCI investigation handoff

This branch preserves the Toy Commander investigation for independent review.

## Downloads

- [Final Test 66 ZIP](builds/KUI-Toy-SCI-Test-66-12NOV2026-2121-CST-3124522.zip) — ready to install; console tested; owner reports mild improvements, unresolved.
- [Complete Tests 1–66 evidence archive](evidence-1-66.tar.xz) — all recovered console records, package inventories, host logs, scripts and provenance metadata.
- [Compiled Tests 1–65 archive](builds/compiled-tests-1-65.tar.xz) — 114 distinct original modules from 44 recovered archives.
- [Test 66 qualification archive](test-66.tar.xz) — build logs, linked ELF artifacts, host checks and reproduction scripts.
- [Source histories and import instructions](source-history/README.md) — reader, Card Tools GUI and physical-disc control bundles.
- [Recovery and publication record](RECOVERY.md) — what survived, what had reached GitHub, evidence gaps and verification.

The original `evidence-1-65.tar.xz` is retained unchanged.
[Test 66 console log](evidence/console/66.txt) and the [owner’s result](evidence/analysis/test-66-console-outcome.md) are included in the complete archive.

The `evidence/` directory exposes the recovered reports and per-test index for browsing.
Extract `evidence-1-66.tar.xz` into an empty directory for the complete evidence tree.
Extract `test-66.tar.xz` separately for the complete final-test qualification tree.
The top-level `SHA256SUMS.txt` verifies all downloadable archives and history bundles.

The root source matches reader Test 66, commit `3124522ea906`.
The `source-history/` bundles retain the complete locally available reader,
Card Tools and physical-control histories. The published main checkpoint
`776c9259c00cf2aa8acd341f5ab8ac14017d5898` is a parent of this handoff branch.

## Playback target and hardware

The test console is an NTSC Dreamcast VA1 using the internal SCI SD connection.
The source is the user's original raw Toy Commander GDI. Current comparisons
disable virtual VMU and CD music to isolate the movie data path. The original
display configuration is retained. Both opening movies run in their normal
sequence. Success requires fluent playback through the end of both movies.

Physical-disc Test 63 supplies the measured reference. It submitted 331 bumper
frames and 1,968 intro frames, with zero skipped frames and normal movie exits.
Its longest in-play scene-submission gaps were approximately 47 and 59 ms.
The report timer counts 781,250 ticks per second. Report numeric values are
hexadecimal.

## Console results and reproducibility

Original console reports and the per-test index are browsable under `evidence/`.
Complete package metadata, source mappings, test scripts and build logs are
preserved in `evidence-1-65.tar.xz`. The deduplicated compiled module catalog is under
`builds/`. It preserves every distinct original KUI module from the recovered
delivery archives, with mappings back to each archive's original file paths.
The evidence index identifies reports that were unavailable or tests that
failed before a report could be generated. Transcriptions retain their source
provenance and are distinguishable from uploaded report bytes.

The recorded user playback results take precedence over host regression tests.
Host tests check specific service, cursor, transport, rendering-hook and cold
startup invariants. They have repeatedly failed to predict real playback.
Passing them does not establish a full-speed game result.

## Recent failures that require independent review

Test 57 was among the strongest user-observed SD builds. It still stalled near
the end of the bumper and lost smooth playback a few seconds into the intro.
Tests 52 through 56 often appeared identical on the console.

Test 62 introduced an 80-raw-sector rolling producer in the existing private
reader arena. Its counters were zero because the masked GDFS kickoff discarded
the newly opened window. The original host scenario omitted that immediate
masked EXEC call. Test 64 retained the window through that call.

Test 64 opened 504 windows, verified 418 raw sectors and abandoned 494 windows.
The per-operation fault distribution was absent from that report. Source review
and controlled media failures reproduced abandonment after a second receive
overrun, bad card CRC or malformed successor token. The hit counter also omitted
requests published immediately after pumping. A zero hit counter alone was
insufficient to establish that no request used the producer.

Test 65 retained the verified prefix and retried failed card blocks with bounded
foreground PIO. The console result regressed substantially. Its report records
2,023 successful card-block recoveries, 2,019 polling errors and six take errors.
The first failure was POLL/OVERRUN. Raw-sector rejection and producer abandonment
were both zero. It submitted only 91 bumper frames and 32 intro frames before
the player's lag exits. The first withheld bumper request took approximately
150 ms from acceptance to completion, and the intro request approximately
252 ms. This establishes a transport scheduling and deadline problem despite
successful byte recovery.

The private producer introduced its own pump and retry behavior. The established
reader already had a different bounded service cadence, retry policy and
foreground refill slice. An independent review should examine these differences
and the real SCI/PVR DMA contention before increasing buffer sizes.

## Accessing historical source

Clone this branch, then run `git fetch` on each local
`source-history/*-history.bundle` to import its historical refs. The bundle
manifest gives exact branch refs, original heads, checksums and the required
published main checkpoint. The branch's own history contains that checkpoint.
Original source commit IDs become addressable after importing the bundles.
The source-provenance table maps module build IDs and package CRCs to available
commits. Earlier missing source objects are explicitly listed.

## Final console experiment

Test 66 replaces the separate FIFO transport pump with the established native
low reader. The native reader retains its existing retry count, programmed
fallback and bounded scheduled refill. The private movie cursor and original
guest cursor are exchanged only during the reader callback and restored before
native completion publication. Complete raw sectors still require their
address and EDC checks.

A matched original scheduled EXEC replay completed the same 14-sector request
in 22 foreground calls with Test 66, compared with 65 foreground calls using
Test 65's custom pump. The first scheduled Test 66 visit processed three card
blocks. A separate repeated-overrun replay took 135 calls with Test 66 and
132 with Test 65. The latter comparison establishes no playback speedup.
The host model cannot establish real decode or SCI timing.

Test 66 passed cancellation, malformed-token retry exhaustion, raw EDC rejection,
ring wrap, full-buffer receive retirement, exact request bytes, original
GDFS/renderer replay and FAT32/exFAT routing checks. The owner tested it and reported mild improvements. The original report is
retained byte-for-byte; smooth completion of both movies was not reported.
This remains the final authorized console test.

Build instructions are retained in each original package README and source
Makefile. The toolchain version used locally is SH ELF GCC 15.2.0 with
binutils 2.45.1. Private replay scripts expect the user's own Toy Commander
executable at the supplied local input path.
