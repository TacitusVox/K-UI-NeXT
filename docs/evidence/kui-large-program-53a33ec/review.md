# Review of the large-program check

One independent reviewer (a separate agent of this session) read
`git show 0c7a9db7` and the code around it on 27 November 2026 (UTC): the
launcher's preflight and `prepare_launch`, the Maple discovery, the layout
step, the program extent, the combined module's stage and its card
preparation, the game image reader and the pinned KallistiOS allocator. It
was given the change, the console report that caused it and a list of places
to attack. It was not given the test results, the corpus runs or the private
game files. It worked in its own checkout and wrote its own harnesses: random
and exhaustive piecewise scans against the whole-program loop, the real
`kui_vmu_native_discover` against the piecewise search on driver bodies it
accepts, the stage's `kui_vmu_persist_stage_admit` on the build's template
profile, and mutants of the guard test and of the scan.

Its verdict: no defect in the new pass. The refusal is sound, the search and
the CRC are correct. The K-UI of `0c7a9db7` had been built; it was not sent.
The corrections below are commit `53a33ec7`, from which the delivered K-UI
was built.

## Findings and what was done

| # | Finding | Disposition |
|---|---|---|
| F1 | Older than this change, and left open by it. With Virtual VMU On a launch without the launcher's answer still starts the combined module in its normal mode. The launcher prepares a card only for a game it has checked (`virtual_a2` needs a ready proof), so the module carries the build's template profile, and its stage stops at storage start with "NATIVE VMU PREPARED PROFILE / GAME MAP REFUSED", before the program is loaded. For a program too large to hold whose file has the 32 words, that is where a Virtual VMU On launch ends; the new log line said "the stage decides". By reading, and by running the stage's admission on the template profile with the card geometry of the owner's logs (refused). Not run on a console. | Real, and not changed in this build: refusing such a launch is a behaviour change of its own, and this build's one change has no console result yet. The log line now says what the launcher found, and the comment what follows (`53a33ec7`). Recorded as a known limit (`background-launch-checks.md`); the package README names the screen. The owner decides whether K-UI should refuse these launches. It also means the stop screen of 27 November (phase 2, detail 0xA) was a Virtual VMU Off launch. |
| F2 | The guard test passed when a new way to accept a driver was added in another form: a second loop after the site loop, an acceptance before the prefilter, a stage that returns before its check in one mode, a discovery over a sector-rounded length, an extent longer than the file. By execution (five mutants). | Real. Corrected in `53a33ec7`: the guard pins the Maple discovery, the program extent, the layout step, the stage's check and the stage's relay up to its check whole (SHA-256), with what to answer before a pin is updated. The five mutants, and one more, now fail it (`host-checks.txt`). Exporting the table from the core instead of copying it would change a reader input and was not done. |
| F3 | A change of `games_native_witness.inc` alone left the unit and launcher test binaries stale; `test-games-native-cache`, which holds the piecewise tests, was not part of `test-shared-game-launch`. | Corrected in `53a33ec7`. |
| F4 | The piece buffer's own allocation failure was untested, and a test comment said it was. A program that was held while the workspace was refused gets no scan. | The test is added and the comment corrected in `53a33ec7`. The second remark is as before the change and is not changed: that launch has no answer. |
| F5 | A program too large to hold cannot reach the reader-only module: its SCI family is not looked for, so New (X) is told "Set Background loading Off" where the same program held whole would start that module. | Known and recorded as a limit. Before the change it was the stop screen. Streaming the SCI family is a change of its own. |
| F6 | The search counts a site that is 2 mod 4 as a witness, although `sdk_at` can never accept one (both loads of the initializer decode to one slot). A module without a seed slot returns before the scan. | Both give "no answer", as before. Not changed. The unit test now states the first one. |

## Checked by the reviewer and found correct

- The refusal: all three combined builds reach the discovery unseeded; no
  mode skips it; a replay needs a seed only a ready proof fills; arena and
  fixed plans come after the driver discovery; Smash, collection, CD music
  and patch routes are never a combined module; the stage's program is the
  boot file's bytes and then at most that; the launch always carries the
  CRC flag when the pass runs, so the stage binds its bytes to the
  launcher's.
- Against the real discovery: of 5,387 synthetic images written to satisfy
  all of `sdk_at`, the discovery counted 2,690 (INCOMPLETE) and the search
  found a witness in every one, at every piece edge and both ends; all 5,387
  with one word broken were NOT_FOUND and refused.
- The search and its buffer: 6,000 random and 60,976 exhaustive edge launches
  agree with the whole-program loop; no sanitizer report; arithmetic safe to
  12 MiB; padding enters neither the CRC nor the search.
- The CRC: `kui_crc32` and `kui_retail_crc32` are one function in the console
  K-UI; `crc_known` is set only after the whole loop and the last cancel
  check; its one use is the launch's CRC; nothing writes the card between.
- That the path runs on the console: a GDI launch never carries encoding
  bits, both callers in `main.c` ask for the report CRC, and Plasma Sword's
  program cannot be held in any state of K-UI's memory.
- Console hazards: the allocator returns NULL without a panic; read size and
  call depth equal the old CRC pass; destinations keep the allocation's
  alignment class; the inner loop is 26 SH-4 instructions a site (about 124
  million for 9.07 MiB, not timed on a console); the new log lines fit 76
  columns.
- The messages: for a program too large to hold without a witness, both
  readers and both installed states say "Set Background loading Off and
  Virtual VMU Off; saves then need a real VMU" with Virtual VMU On and "Set
  Background loading Off" with it Off; either reaches the Standard reader in
  one step.

## Not reached by the reviewer

- Any console run, and the SH-4 build of the K-UI.
- The private game files: it did not confirm which original programs lack
  the 32 words. That is `corpus-large.txt`.
- The Standard reader's handling of a large program with Virtual VMU On.
- `make test-universal-loader` in full, and the Toy test modules beyond the
  shared stage file.

## After the review

The corrections are `53a33ec7`: one log line, one compile-time check, tests
and the Makefile. No launch decides differently; of the K-UI's 888 objects
the launcher's alone differs from the first build, by that log line
(`objects.txt`). The reviewer's comparison with the real discovery was taken
into the unit tests in a smaller form. The corrections were tested
(`host-checks.txt`) and were not reviewed a second time.
