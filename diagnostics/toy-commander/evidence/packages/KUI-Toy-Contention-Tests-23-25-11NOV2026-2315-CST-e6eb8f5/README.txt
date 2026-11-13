K-UI Toy Commander tests 23-25
Reader e6eb8f5f2f0f / Card Tools afb564e9d11b

INSTALL
Copy the included KUI folder onto the SD card, merging folders and replacing
only these four software files. Cold-boot through your K-UI CD after copying.
Open Card Tools: it should offer tests 23, 24, 25. The ordinary Games entry does
not select these tests. Keep the K-UI boot CD inserted for Start to reboot.
Use the existing original raw2352 Toy image. No conversion or rerip.

RUN
Suggested order: 24, 23, 25. Each selection applies its test settings automatically
(CD Music Off, Virtual VMU Off). Let both movies play without manually skipping.
After the movies finish, skip themselves or stop, hold A+B+X+Y and both triggers
for about 3 seconds until the report appears. Release the buttons. After REPORT
SAVED TO SD, press Start to reboot to K-UI. Pages also loop every 5 seconds.
The gesture needs the game to keep polling its controller; a total freeze can
prevent saving. Existing numbered reports are preserved under unused suffixes.
Upload /reports/toy/23.txt,24.txt,25.txt (or the new suffixed versions), plus
whether each movie was smoother and whether the second reached its normal end.

WHAT CHANGES
23: Finish our current SD receive before normalizing/checking data, adding it
to the ring and copying into the game's request. Arm the next receive afterward.
24: Around Toy's original repeated movie-audio cache flush, drain the current
owned SD block, pause rearming, execute the original flush, then resume the same
CMD18 stream. This targets a verified full-cache operation whose causal role in
SCI reception loss is still a hypothesis.
25: Both 23 and 24.
All use the 192 KiB data arena and 0x48000-byte total lease used by 20. The oversized
1 MiB arena used by tests 19/22 is absent. Strict SD block CRC checks
and Toy's original movie clock/lag guard remain. No EDC reconstruction from 21.

MEANING OF RESULTS
Judge visible smoothness and full movie completion first. New BARRIER counters
show whether 24/25 actually ran the new boundary and drained any SD blocks. If
barrier/guard telemetry shows the hook was inactive or rejected, an unchanged
movie does not disprove the contention hypothesis. If it ran cleanly but losses
and reader time remain high, this exact cache-flush interference is insufficient
to explain the problem. Test 23 separates our own memory work; 25 tests interaction.
The existing report also retains request readiness, loss/restart counts, reader
duration and the final timeline. A higher first-block hit count is not a fix.

QUALIFICATION
All final packages passed production GUI admission against the original Toy
startup. All three passed linked memory, stack, relocation and report checks.
Hooked 24/25 passed 3456 original-versus-hook cases each, plus rejection of four
deliberately corrupted hooks. The 126 cold-installer cases and production SCI/card
host tests passed with address/undefined-behavior sanitizers. These checks do
not establish Dreamcast playback success; these are unaccepted hardware tests.

EXACT CHANGED INVENTORY
KUI/tools.kui
KUI/apps/games/retail-toy-test-23.kui
KUI/apps/games/retail-toy-test-24.kui
KUI/apps/games/retail-toy-test-25.kui
All other installed files are retained: boot.kui/runtime.kui, ordinary/native
and WinCE/CDDA/Smash readers, settings, game files, reports and VMU saves.
No KUP, game executable, disc content or save data is included.
