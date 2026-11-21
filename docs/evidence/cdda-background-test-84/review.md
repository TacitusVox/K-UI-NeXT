# Test 84 review record

Three readers beside the author, each with one question and without the author's conclusions, on
20 November 2026 (America/Chicago), on the survey as committed. Each worked on scratch copies outside the tree; nothing
here is a console result. Findings are as the readers gave them, shortened to their claim and the fix
they asked for; the last line of each says what was done. Frame sizes the readers quote came from their
own builds (one with GCC 13); the delivered build's are in the document's Verification.

## The drivers

Does the survey read each original driver as the drivers' own code does? The reader ran the survey over the 60 driver images of the owner's sound-driver archive, header-stripped, with their start-up stores. Reviewed: `5f44706`.

1. **defect**, `src/loader/toy_cdda_survey.c:116`. MANATEE 1.01.1d (1998.08.31, sha256 4320e30aed03…, 5 uses: Tokyo Xtreme Racer, Tokyo Highway Challenge, Shutokou Battle x2 and Dreamcast Express Extra) is classified UNKNOWN instead of GEN1_OLD. family_of requires the state word 'SEGA' at 0xF8 for every 01 01 build, but this build never writes 0xF8. As a result its candidate pool entries 14/15 are never surveyed.

   Asked: For 01 01 builds, take the running proof from a word that every gen-1 reset writes, for example 0x13408 == 0x00018000 (4320e@0xbe0-0xbe8, 906e3@0x170-0x178). Alternatively, accept state 0 for build 0x1d. Then let the existing base-word proof at pool +8 decide, and correct the header comment and the test fixture.

   Done: Fixed in `8894be0`: a MANATEE 1 build is told by its version alone (`01 01`, the build byte against 0x37); the pool's register bases prove its layout.

2. **defect**, `src/loader/toy_cdda_survey.c:213`. Under AICADRV Ver1.70b-1.78h, the word at 0xF8 is a live peak-voice count. The survey treats version plus 0xF8 as the driver's identity, so it counts a driver change whenever the game's polyphony changes. DT's changes field then saturates at 15 and its state byte reports a voice count. For these titles identity is the only data the design keeps (design 7.3: 'AICADRV ... identity only').

   Asked: Use 0xF8 only as MANATEE's running word ('EXEC'/'SEGA'). For other drivers, compare the version and date only, and leave 0xF8 out of DT unless the family defines it.

   Done: Fixed in `8894be0`: identity is the version and the build date; the state word is reported for the MANATEE families only.

3. **risk**, `src/loader/toy_cdda_survey.c:213`. The date text is read only when the version or the state word changes. A swap between builds that share both is therefore not counted, and date_hash keeps the first driver's date. The AUDIO64 line always has state 0 and many of its builds share version bytes; for example AUDIO64 1.90h.2 and MIDIDA 1.90h.2 both read 01 5A 68 00.

   Asked: Read the three date words on every survey (6 more reads), and count a change when the version, the date hash or the AUDIO64/hybrid proof differs.

   Done: Fixed in `8894be0`: the date words are read at every survey; a change of the version or the date's hash counts.

4. **nit**, `src/loader/toy_cdda_survey.c:228`. A hybrid (MIDIDA) is reported as plain AUDIO64 whenever the last survey is cut short or its base word is not yet proved. HYBRID is decided only inside candidates(), but V.family is overwritten before the read < 0 test.

   Asked: Keep the previous family when candidates() could not finish (b->ok false), or record the hybrid proof separately from the version family.

   Done: Fixed in `8894be0`: a survey cut short leaves the family of the last survey that finished.

5. **nit**, `src/loader/toy_cdda_survey.c:133`. A valid MANATEE-2 split with fewer than two stream ports is counted under N as 'records proved no layout'. The driver accepts such a split, and the notes read it as 'no stream channel to borrow', not as an unproved layout.

   Asked: Return 0 (no candidates) for a split that sums to 64 with streams < 2. Keep -1 for sums other than 64 and for streams > 16.

   Done: Fixed in `8894be0`: such a split has no candidates (0), not an unproved layout.

6. **test-gap**, `tests/test_toy_cdda_survey.c:205`. The authored driver fixtures contradict the original images exactly where the code is wrong, and no check runs the survey against the driver images that are available locally. That is why the 1.01.1d and AICADRV defects pass the test and its mutations.

   Asked: Add a host check that loads each local driver image header-stripped (SDRV -0x20, ARMCOM -0xc, others raw) plus its start-up stores, and asserts the family and proof. It must never copy driver bytes into the repository. Also correct the fixtures.

   Done: Fixed in `8894be0`: `tests/test_toy_cdda_survey_drivers.c` and `.py` (`toy-cdda-survey-driver-check`) run the survey, unmodified, over every driver image of the corpus the owner's archive holds (60), read in place and never copied; the fixtures were corrected.

Checked and found sound (abridged to their first sentence):

- The working tree was committed as 5f44706 during the review.
- Version byte layout is correct: byte 0x20 is the major, 0x21 the minor and 0x22 the build, read as a little-endian word.
- State constants are correct.
- Gen 2 is classified correctly, including 2.50.01 and 2.50.04: major 2 with 'EXEC' gives GEN2.
- Gen-1 pool facts hold in every gen-1 build, including the two oldest: 16 entries of 12 bytes at 0x12080, owner at +4, register base at +8 = 0x00801800 + 0x80e.
- NAMIHEI facts hold in both builds.
- AUDIO64 line is consistent across all 24 images.
- AICADRV ('AM2/' = 0x2F324D41) is correctly classified UNKNOWN, which is identity only per design 7.3.
- ARMCOM 1.69 (01 60 09 00) is classified UNKNOWN, although it has the hybrid layout ([0xE0] = 0x3efc, base 0x00801800) and docs/cd-audio-corpus.md:29-30 counts it as a hybrid.
- The ARM reset bit at 0xa0702c00 bit 0 = held in reset matches the SDK's read-modify-write (sh4-sound-usage.md 2.1).
- The window spans 0x1F4000 + 48 KiB = 0x200000, the top 48 KiB that lies outside the SDK heap in the 0x1F4000-form games.
- The report row 'CD SV DT B N WIN SKIP' (toy_suite_report_save.inc:163-190) decodes as its comment says.

## The bus

Can the survey disturb the game's bus, its masked time or its stack? Reviewed: `5f44706`.

1. **risk**, `src/loader/toy_cdda_survey.c:244`. The survey's 2 ms bound (TICK_LIMIT 1563, line 53) and its one-per-1/60 s spacing (SPACING 13021, line 54) only hold if TMU0 runs at the Toy profile (PCLK/64, TCOR0=0xffffffff, TSTR bit 0 set, FRQCR 0x0e0a). The survey never checks that profile itself. It relies on kui_toy_cdda_drive.state==PLAYING, but the player re-checks the profile only at play start, at RELEASE and at non-interrupt delivery visits before the game's GD calls. The console-used pilot this file says it is modelled on checked the profile before every bus visit.

   Asked: Re-admit the profile in the heartbeat itself before using TCNT0: read FRQCR (0xffc00000, 16-bit), TCOR0 (0xffd80008), TCR0 (0xffd80010, 16-bit) and TSTR (0xffd80004, 8-bit), all read-only with no side effects, and call kui_toy_pilot_clock_profile (include/kui/toy_pilot_clock.h:8-12). When it fails, skip the survey and count it, for example as a fourth SKIP field. That is four peripheral reads per heartbeat while playing.

   Done: Fixed in `8894be0`: each surveying heartbeat reads FRQCR, TCOR0, TCR0 and TSTR and makes no survey unless `kui_toy_pilot_clock_profile` holds; such heartbeats are counted in SKIP's timer field.

Checked and found sound (abridged to their first sentence):

- Scope note: during the review the parent workflow committed the working tree as 5f44706 'Test 84 SURVEY...'.
- SR handling is sound.
- Heartbeat context: the only caller of kui_vmu_native_passive_complete in the linked worker is kui_vmu_native_passive_complete_hook.
- G2 DMA check matches the pilot's begin() (toy_pilot_bus.c:66-70).
- FIFO: drain() polls 0xa05f688c with mask 0x31 ('tst #49' in the object), the same mask as toy_pilot_bus.c:12-13 and wce_probe_reader.inc:59.
- Budget: in the object the poll test is polls>9999 and the time test is (~TCNT0 - ~start)>1562, both checked before every poll.
- Addresses and side effects: the survey reads only 0xffd8000c, 0xa05f688c, the eight DMA registers, AICA 0xa0702c00 (ARM reset) and sound RAM.
- No store reaches hardware.
- Stack and placement: the .su frames are heartbeat 84, stable 20 and drain 4 bytes, with budget/survey/candidates/window inlined.
- Call-site ordering does not break the completion.
- Cost per heartbeat.
- Tests run: toy-cdda-survey-host-test passes (11 scenarios, 366,467 checks).

## The integration

Do the tests catch what can go wrong at the call sites, in the report and in the drivers' records? Reviewed: `5f44706, then 8894be0`.

1. **test-gap**, `src/loader/vmu_native_passive.c:2563`. Nothing tests the fast path's early-SDK rule for the survey, nor its place after the reader's cooperative service and after controller_complete(). The CDDA mixed-pad variant never makes that condition false and is compiled without KUI_NATIVE_COOP_PUMP, which the Test 84 worker has.

   Asked: In the CDDA variant add a KUI_VMU_NATIVE_FAMILY_EARLY_V1 pass, as tests/test_vmu_native_split_controller.c:200 does. An INFO/ALLINFO-only split batch (split_batch(0u,...)) must make no survey, and a GETCOND batch exactly one. Also compile the variant with -DKUI_NATIVE_COOP_PUMP=1 so the tested completion is the worker's.

   Done: Closed in `4abc63b`: the gesture test's survey variant is built with `KUI_NATIVE_COOP_PUMP=1` and `KUI_NATIVE_IRQ_REARM=1`; an early-SDK pass checks that a discovery batch makes no survey and a GETCOND batch one; `tests/test_toy_cdda_survey_sites.py` puts five defects into the two call sites, all caught.

2. **test-gap**, `src/loader/vmu_native_passive.c:2623`. The `if(pad_sample>=0)` guard on the mixed-path survey is not pinned: a heartbeat on a batch without the port-A controller condition goes unnoticed.

   Asked: Advance survey_clock by 13021 before other_port_poll() and CHECK that `surveys` is unchanged afterwards.

   Done: Closed in `4abc63b`: the clock is advanced before the other port's poll and the survey count must not move.

3. **test-gap**, `src/loader/toy_cdda_survey.c:63`. The 2 ms bus budget (TICK_LIMIT 1563), the survey's bound on masked time inside the game's Maple completion, is pinned only to under about 8 ms.

   Asked: Record tcnt before and after the heartbeat in budgets() and CHECK that the elapsed ticks are at most TICK_LIMIT plus one poll's ticks, or bound the polls at 1563/4+1.

   Done: Closed in `4abc63b`: the budget case measures the heartbeat's ticks against 2 ms; "twice the time" is a caught mutation.

4. **test-gap**, `src/loader/toy_suite_report_save.inc:188`. No test pins seven of the survey row's caps: busy[0], busy[1], unproved, window_passes and the 8-bit SR.BL, clock, budget and G2 DMA counts. An uncapped count spills into the neighbouring field or wraps.

   Asked: Widen the fixture: busy and unproved of at least 0x10000, every SKIP count of at least 256, passes of at least 0x8000, and an 8-digit version. Expect '... FFFFFFFF FFFFFFFF FFFF.... FFFFFFFF'.

   Done: Closed in `4abc63b`: the widest fixture takes every field past its cap; the row reads `FFFFFFFF 7FFFFFFF FFFFFFFF FFFFFFFF FFFF0030 FFFFFFFF`, and the report is 979 bytes with the first movie's player left out.

5. **test-gap**, `src/loader/toy_cdda_survey.c:256`. Nothing pins what a survey whose records did not prove their layout must not do. Counting busy before the verdict passes, and so does reading and hashing the AUDIO64 window for an unproved driver.

   Asked: In manatee_1's disproof make the first candidate's owner non-zero and CHECK that busy[0] is unchanged. In audio64()'s unproved heartbeats CHECK that window_reads and window_offset are unchanged.

   Done: Closed in `4abc63b`: "busy counted before the verdict" and "the window for a driver its records did not prove" are caught mutations.

6. **test-gap**, `src/loader/toy_cdda_survey.c:180`. NAMIHEI's busy as bit 0 of the stream port's record is not pinned. MANATEE 2's is, by records valued 2 and 3.

   Asked: Give one NAMIHEI candidate a record word with bit 0 clear and other bits set, as manatee_2 does for port 7, and expect it free.

   Done: Closed in `4abc63b`: a NAMIHEI record with bit 0 clear and other bits set is free; "NAMIHEI's record busy by any bit" is caught.

7. **test-gap**, `src/loader/toy_cdda_survey.c:172`. MANATEE 1's register-base proof of the first candidate entry is not pinned: only the second entry's proof is ever disproved.

   Asked: Add a heartbeat that corrupts entry `first`'s base (+8) and expects unproved+1 with proved unchanged.

   Done: Closed in `4abc63b`: the first entry's base is corrupted in its own heartbeat; "MANATEE 1's first entry unproved" is caught.

8. **risk**, `tests/test_toy_cdda_survey_drivers.c:85`. In the new original-driver check, the test itself decides AUDIO64 versus HYBRID: it writes the ports' base word from the corpus label, so for these images the family comparison with the static reading is circular.

   Asked: Label the authored start-up stores in the result and evidence. Report the AUDIO64/HYBRID and MANATEE 1 pool results as authored-model checks, not original-backed family identification.

   Done: Closed in `4abc63b`: each line of the original-driver check names what the family rests on (19 of 59 on the image's bytes alone; the rest also on an authored start-up store), and the AUDIO64/hybrid lines are called checks of the model.

9. **nit**, `src/loader/toy_cdda_survey.c:54`. FIFO_MASK 0x31: only bit 5 is pinned; dropping the AICA (0x01) or G2 (0x10) bit passes.

   Asked: Have fifo_busy return 0x01, 0x10 and 0x20 in turn.

   Done: Closed in `4abc63b`: the busy FIFO cycles 0x01, 0x10 and 0x20; dropping either bit is caught.

10. **nit**, `include/kui/toy_cdda_survey.h:5`. The header, the source and the Makefile cite docs/cdda-background-test-84.md, which does not exist.

   Asked: Land the document in its separate docs commit, or point to docs/cd-audio-corpus.md until then.

   Done: Closed by this documentation commit: `docs/cdda-background-test-84.md`.

11. **nit**, `src/loader/toy_cdda_survey.c:277`. The `V.started &&` term is not pinned; removing it passes. The effect is at most one refused first survey when TCNT0 is within 13,021 ticks of 0xFFFFFFFF.

   Asked: In one scenario start tcnt within SPACING of UINT32_MAX and CHECK that the first heartbeat surveys.

   Done: Closed in `4abc63b`: a case starts the clock within the spacing of its wrap; "the first survey refused below the clock's wrap" is caught.

Checked and found sound (abridged to their first sentence):

- What I reviewed: work started on 5f44706 with a clean tree.
- Switch isolation: `python3 tests/test_toy_test_switches.py` passes (30 shared sources; its fixture is unchanged by both commits).
- Byte level with SURVEY=0: 8894be0's Test 84 worker (native-worker.bin, 41,920 bytes) is identical to e1fcf30's KEEP worker, same BUILD_ID, built from a `git archive` export in scratch.
- Makefile: SURVEY=1 requires TOY_CDDA_KEEP=1 and TOY_SURVEY_MIXED_PAD=1 (:546-551) and adds the source to NATIVE_SRC only.
- Report row: toy_report_short's 104-byte buffer holds at most 96 bytes for this row (21-character label, 6 values), the row writes 6 values (the limit), and it is written whole or not at all.
- Host tests on 8894be0, into scratch: toy-cdda-survey-host-test passes (12 scenarios, 447,952 checks), and the official mutation check catches 42 of 42.
- Bus model: every read except TMU0 is checked for IMASK 15 and SR.BL clear.
- Window chunk commit: the cut-short scenario (bus stuck after the identity, the candidates and two bursts) leaves the offset and hash uncommitted, and later passes mark no change.
- Identity path in 8894be0: a version change, the same version with a different date (AUDIO64 vs MIDIDA), flips of the state word alone (not counted), and a cut survey keeping the family are each pinned by a caught mutation.
- Both heartbeat paths are exercised: removing either call fails the gesture test, and so does moving the mixed-path call before toy_suite_finish_gesture (its terminal handoff check).
- Stack, from the linked worker built in scratch: frames are heartbeat 88, stable 20 and drain 4 bytes (survey, candidates, window and budget are inlined), 112 bytes in all.
