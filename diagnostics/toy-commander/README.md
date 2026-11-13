# Toy Commander SCI investigation handoff

This branch preserves the Toy Commander investigation for independent review.
The root source initially matches reader Test 65, commit
`3f786e1926e23f92043980000471d76cf4c92a99`. The branch's merge parents retain
the complete locally available reader, Card Tools and physical-control histories.
The current main branch is also a parent, allowing reviewers to compare the work
with the published project.

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

Original console reports, package metadata, source mappings, test scripts,
build logs and the per-test evidence index are under `evidence/`.
Compiled test archives are under `builds/`.
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

Use `git log --all --oneline` after cloning the branch. Reader, GUI and physical
histories are merge parents, so their original commit IDs remain addressable.
The source-provenance table maps module build IDs and package CRCs to available
commits. Earlier missing source objects are explicitly listed.

Build instructions are retained in each original package README and source
Makefile. The toolchain version used locally is SH ELF GCC 15.2.0 with
binutils 2.45.1. Private replay scripts expect the user's own Toy Commander
executable at the supplied local input path.
