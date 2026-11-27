# Native CD audio · Test 84 · 26 November 2026

Hardware: Dreamcast VA1, SCI microSD. GUI 64ae07435de5. Launch sequences 651 and 652 used Background New (X), retail-native-dual.kui 4abc63b0cc58, module CRC BAFAAF7C, CD music Off. This diagnostic models the drive and samples the sound driver. Audible output is future Test 85 work.

## POD SpeedZone US (T-17713N)
The owner found a mismatch on the card, recopied the game, reported a full match to the PC reference, and confirmed the game now works. The earlier GUI refusal on the executable sector of track 11 cleared. A subsequent bootstrap stop with virtual VMU enabled preceded this successful survey run.

Launch sequence 651 and survey slot 011 match. The repository decoder identifies a complete report. Executable CRC E41C2053. All eight audio tracks mapped, including seven in the high-density session. Three PLAY requests, zero refused plays and zero lost clocks. At capture, track 7 was in the playing state with repeating playback requested. The game made 596 data reads during play. The drive model kept the playing status on 1,567 read-progress updates. MANATEE 1.01.37 pool entries 13 and 14 (channels 61 and 62) had zero busy observations across 3,684 valid samples. No unproved or skipped surveys. This makes POD an additional MANATEE 1 candidate for the first audible implementation. Sound-RAM allocation and audible behavior remain unverified.

## Tokyo Xtreme Racer US (T40202N)
The owner reported all SD tracks matched the PC reference. On the recorded Background run they saw a black screen after bootstrap and no visible SD activity. Launch sequence 652 and survey slot 012 identify the same Background New (X) Test 84 route as the historical failure. Executable CRC F129E73B and IP CRC B601BEB1 match the earlier slot 010. Slot 012 has a launch record only. Sequence 653 records Standard on the same image, GUI, IP CRC and executable CRC as sequence 652. The owner confirmed it booted on Standard. This supports the host finding that the Background worker’s 284 KiB reservation can starve the game’s 448 KiB startup pool. The host model identifies a 458,480-byte graphics request. The console result establishes the difference in boot behavior; the failed allocation itself was not traced on hardware.

## Plasma Sword US (T1207N)
The earlier Test 84 Background run (slot 008) stopped at phase 2, detail 0xA during SDK discovery. The game uses an early pd 1.09 Maple library. Sequence 654 records Standard, with IP CRC BB964C80 and executable CRC 0B9BD71C. The owner confirmed it booted on Standard.

## Decision for the next session
Both requested Background-Off boot controls are complete. The console owner observed both games boot. The two Standard logs record SCI, GUI 64ae07435de5, retail-physical.kui build f8807cb16561, payload CRC 6A85A797, CD music Off and RAM patches Off. The logs record preparation through handoff; gameplay and audible CD music were outside the reported boot milestone.

For Tokyo, check that the game retains enough memory for startup before offering the combined Background worker. For Plasma Sword, surface the failed SDK check and offer the working Standard route. If a player requested virtual saves, make the effects of choosing Standard with virtual VMU Off visible. For audible native CD audio, retain ChuChu Rocket and KOF Dream Match 1999 as the two initial driver-family cases; POD is an additional MANATEE 1 case with a saved survey.

STREAM.TXT and the two root-level last-launch files contain the Test 84 survey and the earlier Background launches. The standard/ folder contains the two later Standard launch reports. All TXT files preserve the supplied bytes. The previous ranked investigation is at https://github.com/TacitusVox/K-UI-NeXT/blob/3f757bc2ac0a/docs/evidence/cdda-background-test-84/console-rerun-2026-11-22/investigation.md
