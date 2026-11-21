# Test 84 pre-delivery review

Before the archives were sent, two readers checked them independently on 21 November 2026 (America/Chicago):
one read both READMEs against the source, the other audited the archives as packages. A third reader then
checked the revised archives, and a last one the second revision. Nothing here is a console result. Findings are
shortened to their claim and the fix asked for; the last line of each says what was done.

## The READMEs against the source

1. **wrong**, Main archive README.txt:83-87 (MDK2 note). "MDK2: on your card its CD music tracks have no audio in them (the disc's table lists 28 tracks; none of their audio is on the card). ... For Test 85 MDK2 would need an image that has its audio tracks."

   Asked: MDK2: in Test 83 its 28 audio tracks (27 of them CD music; 31 tracks in all) did not fit in the reader's map: the launch record says MAPPED 0. Their files are on the card, but in too many pieces for the map. This build does not need the audio, so run it anyway: it shows what MDK2 asks for. For Test 85, MDK2's files would need to be on the card in fewer pieces (a fresh copy of its folder may do it).

   Done: Rewritten: the audio tracks did not fit in the map (MAPPED 0); the files are on the card in more pieces than the map holds; 31 tracks, 28 audio, 27 CD music; a fresh copy left until Test 85.

2. **misleading**, Main README.txt:106-108; second README.txt:14-16. "If a game misbehaves with the first and not with the second, the sound-chip reading is the cause; if with both, the drive's answers are."

   Asked: If a game misbehaves with the first and not with the second, the sound-chip reading is the likely cause; if with both, the cause is in what both add to Test 83, most likely the drive's answers. For a game you did not run under Test 83, also try it once with your own module, so that I know whether the reader itself is the cause.

   Done: Reworded in both READMEs: "the likely cause"; "what both add to Test 83, most likely the drive's answers"; a try with the owner's own module for a game not run under Test 83.

3. **missing**, Main README.txt:104-106; second README.txt:10-13. "both modules save into it (the second one's reports have no row CD SV, so they tell themselves apart)"

   Asked: Add: "A launch that hangs or crashes saves no report, and its launch record is the same for both modules: keep a list, in order, of each launch's game and which archive's module it used (the slots fill in that order)."

   Done: Added: a list of each launch's game and module, sent with STREAM.TXT.

4. **misleading**, Main README.txt:91-93 (WHAT TO TELL ME). "For each game: did it start, play, pause and resume, and load as under Test 83?"

   Asked: For each game: did it start, play, pause and resume, and load as under Test 83 (or, for a game you did not run under Test 83, as with your own module)?

   Done: Reworded: "(or, for a game you did not run in Test 83, as with your own module)".

5. **misleading**, Main README.txt:71-72 (WHICH GAMES item 4). "Omikron / The Nomad Soul (both drivers; it asks for no CD music as far as I can read, so its CD rows should stay at 0)"

   Asked: 4. Omikron / The Nomad Soul (its European disc carries both drivers; the disc has one CD music track, and I do not know whether the game plays it: its CD rows will say)

   Done: Reworded: one CD music track on its disc; not known whether the game plays it.

6. **missing**, Main README.txt:95-96 (IF SOMETHING GOES WRONG). "The SURVEY OFF lines and their meaning are Test 83's (its README)."

   Asked: - The SURVEY OFF lines, the REPORT NOT SAVED pages and the stop screens at launch are Test 83's; its README says what each means. If the pages say REPORT NOT SAVED, photograph every page: this round needs each game's numbers.

   Done: The pointer now names the SURVEY OFF lines, the stop screens and the REPORT NOT SAVED pages. Not taken: the advice to photograph pages, by the owner's standing wish (docs/handoff-2026-11-10.md).

7. **missing**, Second README.txt:8-9. "The same steps as the first archive's README. This archive replaces only KUI/apps/games/retail-native-dual.kui."

   Asked: Add: "- If the first archive's module is on the card, do not copy it to your computer as your own: keep the copy of your own module you made before the first archive, and put that back at the end."

   Done: Added to the second README: do not copy the card's module as the owner's own.

8. **missing**, Main README.txt:30-39 (READ THIS FIRST). The README names the module path, build and CRC but no GUI.

   Asked: Add: "- It is for the K-UI you installed for Test 83 (GUI 4d4dcff77494): the list of games below is what that K-UI sends to the New reader with the virtual VMU off."

   Done: Added: the K-UI of Test 83, GUI 4d4dcff77494; the standalone New reader not touched.

9. **missing**, Main README.txt:25-28. "a game that waits for a piece of CD music to end ... now waits as long as that piece lasts, in silence"

   Asked: Append: "The wait can be as long as the whole track, several minutes for some: give it that long before you call it a hang."

   Done: Added: the wait can last the whole track, several minutes.

10. **misleading**, Main README.txt:11-13. "Until now the reader always answered "not playing", so MDK2 kept asking for its track again, Bangai-O kept its music volume at zero, and every resume restarted the track."

   Asked: Until now the reader always answered "not playing", so, by my reading of their programs, MDK2 kept asking for its track again, Bangai-O kept its music volume at zero, and a resume could restart the track.

   Done: Reworded: "by my reading of their programs"; "a resume could restart the track".

11. **nit**, Main README.txt:102-103; second README.txt:5-6. "(it never reads the sound chip)" / "it never reads the sound chip"

   Asked: it never reads the sound chip while the game runs

   Done: Reworded in both READMEs: "never reads the sound chip while the game runs".

12. **nit**, Main README.txt:3-5. "your build 1d80f771f081: fragmented discs, cached launch, standalone fallback, the L+R+ABXY save during mixed controller/VMU polls"

   Asked: your build 1d80f771f081: fragmented discs, cached launch, the L+R+ABXY save during mixed controller/VMU polls

   Done: Removed from the reader's description; the GUI line's standalone route is named as untouched.

13. **nit**, Main README.txt:68-69, 74-76 (driver labels). "ChuChu Rocket (Sega's sound driver, newer) / Bangai-O (the same driver)"

   Asked: 1. ChuChu Rocket (MANATEE, the newer family) / 2. Bangai-O (the same family; two builds of it on the disc); items 6-8: "(MANATEE, older)" / "(MANATEE, oldest)" / "(MANATEE, older)"

   Done: Relabelled by MANATEE and AUDIO64 families.

14. **nit**, Main README.txt:39. "Keep your Test 83 file elsewhere if you still want it."

   Asked: Keep your Test 83 file on your computer if you still want it, not on the card.

   Done: Reworded: "on your computer if you still want it, not on the card".

## The archives as packages

1. **missing**, Both READMEs: main README.txt:104-106, alt README.txt:10-13. Launch record: src/core/toy_survey.c:98 and src/loader/toy_survey_stage.inc:137. Both modules carry the same build ID. A slot that holds only a launch record cannot be traced to the main or the alternate module. That is what a hang or a crash before the report gesture leaves, and it is the case the second archive exists for. The READMEs only say that saved reports can be told apart.

   Asked: Tell the owner, in both READMEs, to note which archive's module was on the card for each launch, at least for launches that end without a saved report. A slot with no report does not say which module wrote it.

   Done: Added: the list of launches and modules (as the README reader's finding 3).

2. **missing**, main README.txt:95-108 and alt README.txt:8-17. Neither README says to put the first archive's module back after a misbehaving game has been tried with the second archive. The rest of the game list could then be run with the module that has no survey.

   Asked: Add to both READMEs: after running that game with the second archive, copy the first archive's KUI/apps/games/retail-native-dual.kui back before continuing with the list.

   Done: Added to both READMEs: put the first module back after the second archive's try.

3. **misleading**, main README.txt:106-108 and alt README.txt:14-16. 'if with both, the drive's answers are [the cause]' claims more than the pair can show. The two modules differ only in SURVEY. Both also contain ROOM and HOOK, as well as DRIVE and KEEP.

   Asked: Reword in both READMEs, for example: 'if with both, it is one of the changes both modules share (the drive's answers, or the room and hook they need)'.

   Done: Reworded (as the README reader's finding 2).

4. **misleading**, <scratch>/t84/final/summary.txt:17-22, against main README.txt:127 and alt README.txt:30. The verification log of the 'verified clean build' records 'FAIL asm-audit (exit 1)' and 'FAILED (failures=1)' with no note. Both READMEs say 'This build passed its host and linked checks'.

   Asked: Note the FAIL in the summary and evidence as a known Main failure in files Test 84 does not touch, and keep the raw unittest output out of the summary. Optionally say 'its own host and linked checks' in the READMEs.

   Done: The evidence notes the asm-audit failure as the known Main failure; the READMEs say "its own host and linked checks".

5. **missing**, Alternate verification logs (<scratch>/t84/final/alt-*.log), against alt README.txt:30. No host unit test was compiled with the alternate's switches (DRIVE on, SURVEY off). Its evidence is the build, the linked checks, the host package/admission test and the parts check.

   Asked: Run the host test targets once with the alternate's switches (SURVEY off) and record the result, or reword the alternate README's claim to match the checks that were run.

   Done: Run: the second module's host targets with its switches. The report test did not compile with the survey off, a test-only defect fixed in 88ec3d6; every target then passed (alt-host-tests.log).

6. **missing**, Both README.txt. Neither README names the GUI the modules were admitted against (4d4dcff77494), which the owner should be running.

   Asked: Add one line saying the modules are for the owner's GUI 4d4dcff77494 (the Test 83 GUI line), started by a cold boot from the CD.

   Done: Added: GUI 4d4dcff77494; the cold boot was already in the steps.

7. **nit**, main README.txt:3-6. 'unchanged, with three things added' leaves out ROOM and HOOK, and that three of Test 83's report rows are gone.

   Asked: Say that the report drops Test 83's rows BIG, K and P to make room.

   Done: Added: the report leaves out Test 83's rows BIG, K and P.

8. **nit**, main README.txt:84. 'the disc's table lists 28 tracks': 28 is the number of audio tracks. The table lists 31 tracks.

   Asked: Write 'the disc's table lists 31 tracks, 28 of them audio'.

   Done: Fixed with the MDK2 note.

9. **nit**, main README.txt:123 and alt README.txt:26. 'Nothing else is in this archive', yet README.txt and SHA256SUMS.txt are in the archive too. The retained side of the inventory is a list of categories, not files.

   Asked: Write 'Nothing else is in this archive's KUI folder.'

   Done: Reworded: "Nothing else is in this archive's KUI folder."

10. **nit**, ZIP entry timestamps, both archives. Every entry is dated 2026-11-21 00:00:00 (DOS time, no extended-time field). That is later than the filename's 20NOV2026-2356 CST and matches no real event: the modules were built at 23:48 and 23:54 CST and the ZIPs written at 23:56:45 CST.

   Asked: Optional: date the entries 2026-11-20 23:56 Chicago time, or leave them as they are.

   Done: The archives are now written with Python zipfile, entries dated at the Chicago-local packaging time of the filename's stamp.

11. **nit**, Main ZIP entry KUI/SURVEY/STREAM.TXT. STREAM.TXT is flagged as text in the ZIP (internal attribute bit 0; zipinfo 't-'), while Test 83's copy was binary ('b-'). An extractor set to convert line endings would add 606 bytes to the 154,112-byte file, and the stage would refuse it.

   Asked: Write the ZIP with internal attribute 0 (Python zipfile, as for Test 83), or tell the owner to extract without line-ending conversion.

   Done: Every entry binary (internal attribute 0).

12. **nit**, KUI-CD-Drive-Test-84-without-survey-20NOV2026-2356-CST-4abc63b.zip (filename). The alternate's name has '-without-survey' between 'Test-84' and the stamp, so it does not fit the stated pattern KUI-CD-Drive-Test-84-<DDMONYYYY-HHMM>-CST-<7hex>.zip. Its stamp, CST and 4abc63b are correct.

   Asked: No change needed if the variant label is intended (the task names this file). Keep the same convention in later deliveries.

   Done: Kept: the variant label is intended.

## The first revision

The third reader found 24 of the 26 findings above resolved and 2 deliberately kept (the photograph advice and the
variant filename), and four new points:

1. **misleading**, Main README.txt:50-53 ('A launch that hangs or crashes saves no report') and :118-122 ('A hang or a crash: power off'); also :31-33. The owner is told that a stuck game cannot save a report, and to power off a hang without trying the save.

   Asked: Main :52-53: 'A launch that ends without a saved report (a crash, or one you power off) leaves a slot that looks the same for both modules: your list is how I tell them apart.' Hang bullet: 'A hang: first hold the save as in step 5 for about five seconds. If the pages come up, the report is saved: note it on your list and press Start. If nothing comes, power off and note the game and roughly when (in music, at a pause, at a load).' At :32 add: 'then save the report as in step 5 before you power off.'

   Done: Reworded: a hang is first saved by the gesture, which the worker keeps for a game that stops answering while its controller is polled; only a crash, or a launch powered off, ends without a report.

2. **missing**, Main README.txt:120-122 (own-module try) and :50-51 (list). 'For a game you did not run in Test 83, also try it once with your own module' gives no step back to this archive's module, and the list offers only 'this one or the second'.

   Asked: Append to :122: 'Then put this archive's module back on the card before you go on with the list.' At :51 write '(this one, the second, or your own)'.

   Done: Added: put this archive's module back after the own-module try; the list offers "this one, the second, or your own".

3. **nit**, Second README.txt:1-8, with the archive name '-without-survey'. The second module's launch line still reads 'SURVEY ON / REPORT SAVES TO SLOT nnn', yet its archive is named '-without-survey' and its README does not mention the line.

   Asked: Add to the second README's differences: 'Its launch line still says SURVEY ON / REPORT SAVES TO SLOT nnn: that is the report file, as with the first module.'

   Done: Added to both READMEs: the second module's launch line still says SURVEY ON; that is the report file.

4. **nit**, Main README.txt:15-17 (point 3). 'reading only: which sound driver the game runs, and whether the game itself uses the two sound channels' reads as the full list of what the survey reads, and it leaves one part out.

   Asked: '...the two sound channels the music will need in Test 85, and, with the AUDIO64 driver (MDK2, Toy Commander), whether the game uses the top 48 KiB of sound memory.'

   Done: Point 3 now names the AUDIO64 window.

## The second revision

The last reader found all four resolved and the archives ready, with one nit: the save needs 180
consecutive completed controller polls with the hold, then waits up to 60 more for a disc command,
so "about five seconds" is enough at 50 and 60 Hz but not for a game that polls slower while it is
stuck (6 s held at 30 Hz, up to 8 s in all).

   Done: the hang bullet now says to hold until the pages come up, for up to ten seconds (24 Hz and
   faster), and to check that they say REPORT SAVED TO SD; the silent-wait paragraph says to hold
   until the pages come up. The same pass noted that a launch record of the owner's own module names
   its own build, so the READMEs now say a slot without a report looks the same for the two Test 84
   modules, not for every module.

The delivered archives are this last revision:
`KUI-CD-Drive-Test-84-21NOV2026-0113-CST-4abc63b.zip` and
`KUI-CD-Drive-Test-84-without-survey-21NOV2026-0113-CST-4abc63b.zip` (package.json). Only their
READMEs changed from the archives the readers saw; the modules and the report file are the same
bytes throughout.
