# Test 84 re-run of 22 November 2026: the host investigation

Four independent investigations and a cross-check (subagents of the CD music
thread's session, 22 Nov 2026), from the owner's CD-audio archive (each
game's `1ST_READ.BIN`, `IP.BIN`, `disc.gdi`, `startup-info.json` and sound
drivers; the executables of Plasma Sword, Tokyo Xtreme Racer, MDK2, Bangai-O
and Omikron match the console launch records' CRCs) and the K-UI and module
sources at `89a241cd` (identical to `64ae0743` and module `4abc63b0cc58` in
every file involved). Host results only: no console run has confirmed any of
them. Private game bytes stay out of the repository; addresses and
instruction-level descriptions only.

## Tokyo Xtreme Racer (slot 10): black screen after the bootstrap

**Cause (host, likely):** memory, not the drive model.

- The game's SDK pool is one call, `syMallocInit(0x8cdb0000, 0x70000)`
  (448 KiB, syMalloc 1.35), found by the production discovery and in the
  routing corpus (`dual-seeded` with VMU Off).
- Every Background module takes its 284 KiB lease (0x47000 + a 32-byte
  header) from the bottom of that pool when the initializer returns
  (`native_allocate_bottom`, `vmu_native_heap_probe_stage.c`). The placement
  check (`kui_vmu_native_stage_bottom_allocation_safe`) asks only that the
  lease lie outside the stage; nothing reserves room for the game.
- Executed in the repository's SH model with the game's own allocator code:
  without the lease the game's one large request succeeds (7 units left);
  with it, 5,247 units are free and the largest request that succeeds is
  167,872 bytes.
- The request: the graphics library's vertex-buffer init, called from the
  hardware init right after the pool is set up, asks the pool for 458,480
  bytes (executed in the same model from the game's call site). On NULL it
  stores `0x20000000` as its buffer without a check; no frame can be built.
- No CD play happens before the logos (the play entry is reached only from
  the music wrappers of later scenes and one status-3 helper), and the game
  never waits on a drive status, so DRIVE, KEEP and SURVEY never act.
- The owner's installed New reader `f8807cb16561` (module `09c1480c...`, its
  own build checked) and Test 83's module take the same lease the same way.
- The PAL Tokyo Highway Challenge and the Japanese Shutokou Battle have the
  same pool and request.

**Not settled:** what the game does with the bad buffer (only the console can
show it); whether the re-converted image is sound beyond its executable (the
first conversion was incomplete: "Invalid track length"). The library
compatibility row (7 Nov) refused TXR on the Background reader under the old
top placement; the bottom placement now admits it.

**Other games the same gap reaches (host):** four Buzz 2000 discs are routed
`dual-seeded` with pools smaller than the lease (the stage must stop them at
phase 4); two Proyakyu discs have 512 KiB pools, of which the lease is 55%.

## POD SpeedZone: zero bytes at the executable's first sector

**Cause (host, likely):** the card's copy of track 11 holds zeros there.

- LBA 548141 is byte 279,055,392 of track 11 (281,428,560 bytes, 1,009
  sectors before its end). `game_image.c` uses the GDI LBA as written and a
  64-bit offset; `read_file` reads it through FatFs (exFAT, `FSIZE_t` 64-bit),
  not through the Retail map. FatFs reads a file's clusters as stored and
  ignores exFAT's ValidDataLength.
- MDK2's executable sits 280,205,520 bytes into its 281,421,504-byte track
  (517 sectors from the end) and read correctly in this run; Toy Commander
  and KOF read theirs at about 384 MiB.
- Production FatFs and `game_image.c` over a host image of the card's
  geometry read POD's sector correctly wherever the file sits; with zeros
  there they print the console's three lines.
- The copy that verified on 20 Nov was the bundler's own extraction of the
  owner's CHD, not the GDI set on the card. The card's GDI files are chdman
  extractions with the same geometry (their CRCs match the launch records for
  five games).

## Plasma Sword (slot 8): the reader's stop screen

**Cause (host, proven):** the first Katana libraries.

- Maple library pd 1.09 (Dec 1998), Shinobi 1.36, syMalloc 1.05, gdFs
  1.06a. The reader's Maple families are pd 1.46 (EARLY_V1, EARLY_P2_V1) and
  pd 1.50 (UNIFORM_V1). Its submit scan finds one candidate site, which
  matches 3 of 32 witnesses: `kui_vmu_native_discover` returns NOT_FOUND,
  `discover_layout` fails, phase 2 detail 10. The photo's zero SDK fields fit.
- The pd 1.09 server is the same design with different registers, field
  offsets (8 bytes lower) and literal placement; the shape occurs once in
  each of the corpus's 31 pd 1.09/1.09a programs and in no other.
- Its SDK SCI family is -1 (no reviewed early callback-table initializer),
  and its SDK pool starts inside its 9.5 MB program, so even a pd 1.09 Maple
  family would not admit it to the Background reader.
- Why it launched: when the K-UI's complete-executable discovery fails, the
  preflight returns SKIPPED, the same as a skipped preflight, and the
  combined module is launched unseeded "so the original stage can follow its
  usual path" (`docs/vmu-native-launch-cache.md`; Main `34e0ddce`, `eddbb01f`,
  1 and 5 Nov). The stage runs the same predicates on the same bytes, so it
  cannot succeed where the K-UI failed. The 64-slot map of `1c71ba3afa17`
  used to refuse Plasma Sword (66 tracks) before that.
- On the host, none of the 61 corpus programs routed `dual-unseeded` with VMU
  Off passes the stage's first steps (58 at the Maple step, 3 at the
  extent). `docs/cd-audio-corpus.md` and the Routing paragraph of
  `cdda-background-test-84.md` ("61 with the stage's discovery") overstate
  that.
- The Standard route (Background loading Off, Virtual VMU Off) loads
  `retail-physical.kui`, which has no Maple or SCI gate; its 160-slot map holds
  Plasma Sword. No pd 1.09 game has a recorded console result on it.

## The reports (slots 6, 7, 9)

- **Bangai-O (proven):** the game sets its MANATEE 2 split to 0 MIDI voices,
  64 one-shot ports and 0 stream ports (after the library's own default of
  48/8/8). With fewer than two stream ports the survey has no candidate pair
  (`toy_cdda_survey.c`, by design since `8894be0`). The unmodified survey run
  over the game's own driver with that split gives the console's row
  exactly. Under that split every channel is a one-shot port, which the
  driver switches off at a loop's end without checking use: the planned Test
  85 cannot sound there. A corpus scan finds the same kind of split in 11
  programs, 7 of them Background-routed (static).
- **MDK2 (proven):** AUDIO64 1.90b.1. The game's own code puts its stream on
  channel 63 and gives effects channels 0 to 62 round robin, so no fixed pair
  is free without a patch of the game (as the existing MDK2 adapter makes).
  MAPPED 0: the data-only map uses 70 of 160 slots, so its 28 audio files are
  in more than 90 pieces; a recopy, not more map room, would map them (if the
  card has contiguous free space).
- **Omikron:** no CD command (its program has no play reference); the
  executable is the European disc's file. Whether its song is drive audio or
  a streamed data file is open.
- **Test 85's candidates:** ChuChu Rocket (MANATEE 2, library split, both
  stream ports free in 2,640 surveys) and KOF Dream Match 1999 (MANATEE 1,
  pool entries 13 and 14 free in 10,084 surveys).

## Next steps the cross-check ranked

1. Owner, no build: Tokyo Xtreme Racer and Plasma Sword on the Standard
   reader; the card copies of POD and Tokyo Xtreme Racer checked on the PC.
2. If Tokyo Xtreme Racer runs on Standard: a K-UI route rule for pools that
   cannot hold the lease and the game's known startup demand (standalone New
   where the SDK SCI family exists, otherwise a refusal), at least the
   stage's own minimum (Buzz 2000).
3. A K-UI refusal with a message instead of an unseeded launch when the
   discovery fails definitely (Plasma Sword and the other 60); it reverses a
   designed path, so it needs the owner's agreement.
4. Test 85 with sound only where both candidates were proved free, and the
   survey count and MANATEE 2 split added to the report.
5. Later reader work: the early SDK SCI family (standalone New for about 40
   early programs, Plasma Sword included), then a pd 1.09 Maple family and a
   reviewed placement for pools that overlap the program.
